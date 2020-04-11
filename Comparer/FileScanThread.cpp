#include "StdAfx.h"
#include "ComparerDoc.h"
#include "ComparerViewC.h"
#include "FrmCmpInfo.h"
#include "FileScanThread.h"
#include "FrmCmpStrategy.h"
#include "QDebug.h"

FileScanThread::FileScanThread(const CComparerDoc *pDoc)
: mDoc(pDoc)
, mCurFrameID(0)
, mFrmCmpInfoSize(1)
, mFrmCmpInfo(new FrmCmpInfo[mFrmCmpInfoSize])
, mCurFrames(0)
, mFrmCmpStrategy(NULL)
{
}

FileScanThread::~FileScanThread(void)
{
	delete [] mFrmCmpInfo;
}

static inline void closeSQPane(SQPane *scanInfo) {
	CFile *file = &scanInfo->file;
	if (file->m_hFile != CFile::hFileNull)
		file->Close();

	cv::Mat &ocvMat = scanInfo->mOcvMat;
	ocvMat.release();
}

void FileScanThread::setup()
{
	// NOTE: this delete should be here in the same thread of a caller
	mCurFrames = mDoc->mMinFrames;
	if (mCurFrames > mFrmCmpInfoSize) {
		delete [] mFrmCmpInfo;

		mFrmCmpInfoSize = mCurFrames;
		mFrmCmpInfo = new FrmCmpInfo[mCurFrames];
	} else {
		deinit();
	}
}

SmpError FileScanThread::readyToRun()
{
	if (mCurFrames <= 0) {
		LOGWRN("no frame to compare: minFrames(%d)", mCurFrames);
		return SMP_FAIL;
	}

	mCurFrameID = 0;

	for (int i = 0; i < CComparerDoc::IMG_VIEW_MAX; i++) {
		const ComparerPane *pane = &mDoc->mPane[i];
		SQPane *scanInfo = &mScanInfo[i];
		closeSQPane(scanInfo);

		if (pane->isOcvMat()) {
			cv::Mat &ocvMat = scanInfo->mOcvMat;
			const CString &pathName = pane->pathName;
			std::string str = CT2A(pathName);
			ocvMat = cv::imread(str, 1);
		} else {
			CFile *file = &scanInfo->file;
			CFileException e;
			BOOL ok = file->Open(pane->pathName,
				CFile::modeRead | CFile::shareDenyNone | CFile::typeBinary, &e);
			if (!ok) {
				e.ReportError();
				return SMP_FAIL;
			}
		}

		scanInfo->origSceneSize = pane->origSceneSize;
		if (scanInfo->origSceneSize > scanInfo->origBufSize) {
			if (scanInfo->origBuf)
				delete [] scanInfo->origBuf;

			scanInfo->origBuf = new BYTE[scanInfo->origSceneSize];
			scanInfo->origBufSize = scanInfo->origSceneSize;
		}

		scanInfo->colorSpace = pane->colorSpace;
		scanInfo->csc2yuv420 = pane->csc2yuv420;
		scanInfo->csc2rgb888 = pane->csc2rgb888;
		scanInfo->csLoadInfo = pane->csLoadInfo;
	}

	mFrmCmpStrategy = mDoc->mFrmCmpStrategy;
	mFrmCmpStrategy->AllocBuffer(&mScanInfo[CComparerDoc::IMG_VIEW_L],
		&mScanInfo[CComparerDoc::IMG_VIEW_R]);

	return SMP_OK;
}

bool FileScanThread::threadLoop()
{
	for (int i = 0; i < CComparerDoc::IMG_VIEW_MAX; i++) {
		SQPane *scanInfo = &mScanInfo[i];
		size_t origSceneSize = scanInfo->origSceneSize;

		if (scanInfo->isOcvMat()) {
			cv::Mat &ocvMat = scanInfo->mOcvMat;
			memcpy(scanInfo->origBuf, ocvMat.data, origSceneSize);
		} else {
			CFile *file = &scanInfo->file;
			UINT nRead = file->Read(scanInfo->origBuf, UINT(origSceneSize));
			if (nRead < origSceneSize) {
				LOGWRN("The remaining data is too short to be one frame");

				// initialize the remaining buffer
				memset(scanInfo->origBuf + nRead, 0, origSceneSize - nRead);
			}
		}
	}

	SQPane *scanInfoL = &mScanInfo[CComparerDoc::IMG_VIEW_L];
	SQPane *scanInfoR = &mScanInfo[CComparerDoc::IMG_VIEW_R];

	double (*metrics)[QPLANES] = mFrmCmpInfo[mCurFrameID].mMetrics;
	mFrmCmpStrategy->DiffNMetrics(scanInfoL, scanInfoR, metrics, mFrmCmpInfo[mCurFrameID].diffRLC);

	mFrmCmpInfo[mCurFrameID].mParseDone = true;
	++mCurFrameID;
	if (mCurFrameID >= mDoc->mMinFrames)
		return false;

	return true;
}

void FileScanThread::deinit()
{
	for (int i = 0; i < mCurFrames; i++) {
		mFrmCmpInfo[i].mParseDone = false;
		mFrmCmpInfo[i].deinit();
	}
}
