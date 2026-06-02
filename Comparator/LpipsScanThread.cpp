#include "stdafx.h"
#include "LpipsScanThread.h"
#include "ComparatorDoc.h"
#include "FileScanThread.h"
#include "LpipsEngine.h"
#include "MlModelProvisioner.h"
#include "qimage_metrics.h"
#include "MainFrm.h"

LpipsScanThread::LpipsScanThread(CComparatorDoc* doc, FileScanThread* baseScanner, int metricIdx)
	: m_doc(doc), m_baseScanner(baseScanner), m_metricIdx(metricIdx)
{
	CWnd* pMainWnd = AfxGetMainWnd();
	m_hMainWnd = pMainWnd ? pMainWnd->GetSafeHwnd() : NULL;
}

LpipsScanThread::~LpipsScanThread()
{
	requestExitAndWait();
}

bool LpipsScanThread::threadLoop()
{
	unsigned gen = m_baseScanner->getScanGeneration();

	std::wstring modelPath = MlModelProvisioner::provisionLpipsModel(m_hMainWnd);
	if (modelPath.empty())
		return false;

	if (!LpipsEngine::getInstance().init(modelPath))
		return false;

	SQPane scanInfo[2];
	for (int i = 0; i < 2; i++) {
		const ComparatorPane *pane = &m_doc->mPane[i];
		scanInfo[i].origSceneSize = pane->origSceneSize;
		scanInfo[i].origBuf = new BYTE[pane->origSceneSize];
		scanInfo[i].origBufSize = pane->origSceneSize;
		scanInfo[i].colorSpace = pane->colorSpace;
		scanInfo[i].csc2yuv420 = pane->csc2yuv420;
		scanInfo[i].csc2rgb888 = pane->csc2rgb888;
		scanInfo[i].csLoadInfo = pane->csLoadInfo;
		scanInfo[i].OpenFrmSrc(pane->pathName, m_doc->mSortedCscInfo,
			pane->srcW, pane->srcH, m_doc->mW, m_doc->mH);
		
		int stride = ROUNDUP_DWORD(m_doc->mW);
		scanInfo[i].rgbBufSize = stride * m_doc->mH * QIMG_DST_RGB_BYTES;
		scanInfo[i].rgbBuf = new BYTE[scanInfo[i].rgbBufSize];
	}

	int frames = m_doc->mMinFrames;
	for (int frameID = 0; frameID < frames; frameID++)
	{
		if (exitPending() || m_baseScanner->getScanGeneration() != gen)
			break;

		while (!m_baseScanner->isFrameParsed(frameID))
		{
			if (exitPending() || m_baseScanner->getScanGeneration() != gen)
				goto cleanup;

			// If the base scan is no longer running, it either finished (this
			// frame may have just been parsed in the same instant the running
			// flag dropped) or aborted. Re-check once; if the frame is still
			// unparsed, the scan errored out, so stop instead of waiting forever.
			if (!m_baseScanner->isScanRunning()) {
				if (m_baseScanner->isFrameParsed(frameID))
					break;
				goto cleanup;
			}
			Sleep(20);
		}

		for (int i = 0; i < 2; i++) {
			scanInfo[i].FillSceneBuf(scanInfo[i].origBuf);
			int bufOff2 = 0, bufOff3 = 0;
			scanInfo[i].csLoadInfo(m_doc->mW, m_doc->mH, &bufOff2, &bufOff3);
			scanInfo[i].csc2rgb888(scanInfo[i].rgbBuf, scanInfo[i].origBuf, scanInfo[i].origBuf + bufOff2, scanInfo[i].origBuf + bufOff3, ROUNDUP_DWORD(m_doc->mW), m_doc->mW, m_doc->mH);
		}

		double dist = LpipsEngine::getInstance().distance(scanInfo[0].rgbBuf, scanInfo[1].rgbBuf, m_doc->mW, m_doc->mH, ROUNDUP_DWORD(m_doc->mW) * QIMG_DST_RGB_BYTES);

		m_baseScanner->setLazyMetric(frameID, m_metricIdx, dist, gen);
	}

cleanup:
	for (int i = 0; i < 2; i++) {
		if (scanInfo[i].origBuf) {
			delete[] scanInfo[i].origBuf;
			scanInfo[i].origBuf = NULL;
		}
		if (scanInfo[i].rgbBuf) {
			delete[] scanInfo[i].rgbBuf;
			scanInfo[i].rgbBuf = NULL;
		}
	}

	return false;
}
