#include "stdafx.h"
#include "LpipsScanThread.h"
#include "ComparatorDoc.h"
#include "FileScanThread.h"
#include "LpipsEngine.h"
#include "MlModelProvisioner.h"
#include "qimage_metrics.h"
#include "MainFrm.h"

LpipsScanThread::LpipsScanThread(CComparatorDoc* doc, FileScanThread* baseScanner, int metricIdx)
	: m_doc(doc)
	, m_baseScanner(baseScanner)
	, m_metricIdx(metricIdx)
	, m_w(doc->mW)
	, m_h(doc->mH)
	, m_frames(doc->mMinFrames)
{
	CWnd* pMainWnd = AfxGetMainWnd();
	m_hMainWnd = pMainWnd ? pMainWnd->GetSafeHwnd() : NULL;

	for (int i = 0; i < 2; i++) {
		const ComparatorPane *pane = &m_doc->mPane[i];
		m_pane[i].pathName = pane->pathName;
		m_pane[i].srcW = pane->srcW;
		m_pane[i].srcH = pane->srcH;
		m_pane[i].origSceneSize = pane->origSceneSize;
		m_pane[i].colorSpace = pane->colorSpace;
		m_pane[i].csLoadInfo = pane->csLoadInfo;
		m_pane[i].csc2yuv420 = pane->csc2yuv420;
		m_pane[i].csc2rgb888 = pane->csc2rgb888;
	}
}

LpipsScanThread::~LpipsScanThread()
{
	requestExitAndWait();
}

bool LpipsScanThread::threadLoop()
{
	unsigned gen = m_baseScanner->getScanGeneration();

	// The backbone id ("alex"/"vgg") for the selected LPIPS metric drives which
	// model is provisioned and loaded; non-ML metrics never start this worker.
	const char* mlId = qmetric_info_table[m_metricIdx].ml_id;
	if (!mlId)
		return false;

	std::wstring modelPath = MlModelProvisioner::provisionLpipsModel(m_hMainWnd, mlId);
	if (modelPath.empty())
		return false;

	if (!LpipsEngine::getInstance().init(modelPath, mlId))
		return false;

	SQPane scanInfo[2];
	for (int i = 0; i < 2; i++) {
		const PaneSnapshot *pane = &m_pane[i];
		scanInfo[i].origSceneSize = pane->origSceneSize;
		scanInfo[i].origBuf = new BYTE[pane->origSceneSize];
		scanInfo[i].origBufSize = pane->origSceneSize;
		scanInfo[i].colorSpace = pane->colorSpace;
		scanInfo[i].csc2yuv420 = pane->csc2yuv420;
		scanInfo[i].csc2rgb888 = pane->csc2rgb888;
		scanInfo[i].csLoadInfo = pane->csLoadInfo;
		scanInfo[i].OpenFrmSrc(pane->pathName, m_doc->mSortedCscInfo,
			pane->srcW, pane->srcH, m_w, m_h);

		int stride = ROUNDUP_DWORD(m_w);
		scanInfo[i].rgbBufSize = stride * m_h * QIMG_DST_RGB_BYTES;
		scanInfo[i].rgbBuf = new BYTE[scanInfo[i].rgbBufSize];
	}

	for (int frameID = 0; frameID < m_frames; frameID++)
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
			scanInfo[i].csLoadInfo(m_w, m_h, &bufOff2, &bufOff3);
			scanInfo[i].csc2rgb888(scanInfo[i].rgbBuf, scanInfo[i].origBuf, scanInfo[i].origBuf + bufOff2, scanInfo[i].origBuf + bufOff3, ROUNDUP_DWORD(m_w), m_w, m_h);
		}

		double dist = LpipsEngine::getInstance().distance(scanInfo[0].rgbBuf, scanInfo[1].rgbBuf, m_w, m_h, ROUNDUP_DWORD(m_w) * QIMG_DST_RGB_BYTES);

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
