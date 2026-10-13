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
	, m_frames(0)
{
	CWnd* pMainWnd = AfxGetMainWnd();
	m_hMainWnd = pMainWnd ? pMainWnd->GetSafeHwnd() : NULL;

	for (int i = 0; i < 4; i++) {
		const ComparatorPane *pane = &m_doc->mPane[i];
		m_pane[i].pathName = pane->pathName;
		m_pane[i].srcW = pane->srcW;
		m_pane[i].srcH = pane->srcH;
		m_pane[i].origSceneSize = pane->origSceneSize;
		m_pane[i].colorSpace = pane->colorSpace;
		m_pane[i].csLoadInfo = pane->csLoadInfo;
		m_pane[i].csc2yuv420 = pane->csc2yuv420;
		m_pane[i].csc2rgb888 = pane->csc2rgb888;
		m_pane[i].disableImageSequence = pane->disableImageSequence;
		m_pane[i].available = pane->isAvail();
	}
	for (int t = 1; t < 4; ++t) {
		m_pairFrames[t-1] = doc->PairFrames(t);
		m_generation[t-1] = baseScanner->pairGeneration(t);
		m_frames = max(m_frames, m_pairFrames[t-1]);
	}
}

LpipsScanThread::~LpipsScanThread()
{
	requestExitAndWait();
}

bool LpipsScanThread::threadLoop()
{
	try {
		unsigned gen = m_baseScanner->getScanGeneration();

		// The backbone id ("alex"/"vgg") for the selected LPIPS metric drives which
		// model is provisioned and loaded; non-ML metrics never start this worker.
		const char* mlId = qmetric_info_table[m_metricIdx].ml_id;
		if (!mlId)
			return false;

		std::wstring modelPath = MlModelProvisioner::provisionLpipsModel(m_hMainWnd, mlId);
		if (modelPath.empty()) {
			for (int t = 1; t < 4; ++t) if (m_pairFrames[t-1])
				m_baseScanner->setPairError(t, _T("Unavailable: model preparation cancelled or failed"), m_metricIdx);
			return false;
		}

		if (!LpipsEngine::getInstance().init(modelPath, mlId)) {
			for (int t = 1; t < 4; ++t) if (m_pairFrames[t-1])
				m_baseScanner->setPairError(t, _T("Error: model initialization failed"), m_metricIdx);
			return false;
		}

		SQPane scanInfo[4];
		for (int i = 0; i < 4; i++) {
			const PaneSnapshot *pane = &m_pane[i];
			if (!pane->available) continue;
			scanInfo[i].origSceneSize = pane->origSceneSize;
			scanInfo[i].origBuf = new BYTE[pane->origSceneSize];
			scanInfo[i].origBufSize = pane->origSceneSize;
			scanInfo[i].colorSpace = pane->colorSpace;
			scanInfo[i].csc2yuv420 = pane->csc2yuv420;
			scanInfo[i].csc2rgb888 = pane->csc2rgb888;
			scanInfo[i].csLoadInfo = pane->csLoadInfo;
			scanInfo[i].disableImageSequence = pane->disableImageSequence;
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

			bool needed[4] = {};
			for (int t = 1; t < 4; ++t) {
				double cached[QPLANES];
				if (frameID >= m_pairFrames[t-1] || m_baseScanner->copyMetrics(frameID, m_metricIdx, cached, t)) continue;
				while (!m_baseScanner->isFrameParsed(frameID, t))
			{
				if (exitPending() || m_baseScanner->getScanGeneration() != gen)
					goto cleanup;

				// If the base scan is no longer running, it either finished (this
				// frame may have just been parsed in the same instant the running
				// flag dropped) or aborted. Re-check once; if the frame is still
				// unparsed, the scan errored out, so stop instead of waiting forever.
				if (!m_baseScanner->isScanRunning()) {
					if (m_baseScanner->isFrameParsed(frameID, t))
						break;
					m_baseScanner->setPairError(t, _T("Error: base frame unavailable"), m_metricIdx);
					break;
				}
				Sleep(20);
			}
				if (m_baseScanner->isFrameParsed(frameID, t)) needed[0] = needed[t] = true;
			}

			bool decoded[4] = {};
			for (int i = 0; i < 4; i++) if (needed[i]) {
				decoded[i] = scanInfo[i].isAvail() && scanInfo[i].frmSrc->SetNextFrameID(frameID)
					&& scanInfo[i].FillSceneBuf(scanInfo[i].origBuf);
				if (!decoded[i]) continue;
				int bufOff2 = 0, bufOff3 = 0;
				scanInfo[i].csLoadInfo(m_w, m_h, &bufOff2, &bufOff3);
				scanInfo[i].csc2rgb888(scanInfo[i].rgbBuf, scanInfo[i].origBuf, scanInfo[i].origBuf + bufOff2, scanInfo[i].origBuf + bufOff3, ROUNDUP_DWORD(m_w), m_w, m_h);
			}

			for (int t = 1; t < 4 && !exitPending(); ++t) if (needed[t]) {
				if (!decoded[0] || !decoded[t]) {
					m_baseScanner->setPairError(t, _T("Error: frame decode failed"), m_metricIdx); continue;
				}
				double dist = LpipsEngine::getInstance().distance(scanInfo[0].rgbBuf, scanInfo[t].rgbBuf, m_w, m_h, ROUNDUP_DWORD(m_w) * QIMG_DST_RGB_BYTES);
				if (!m_baseScanner->setLazyMetric(frameID, m_metricIdx, dist, m_generation[t-1], t))
					m_baseScanner->setPairError(t, _T("Error: inference failed"), m_metricIdx);
			}
		}

	cleanup:
		for (int i = 0; i < 4; i++) {
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
	} catch (const std::exception&) {
		for (int t=1;t<4;++t) if (m_pairFrames[t-1])
			m_baseScanner->setPairError(t,_T("Error: perceptual metric worker stopped"),m_metricIdx);
		return false;
	}
}
