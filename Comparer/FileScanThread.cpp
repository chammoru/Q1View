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
		scanInfo->closeFrmSrcs();
		scanInfo->openFrmSrc(pane->pathName);
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
		scanInfo->FillSceneBuf(scanInfo->origBuf, -1);
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
