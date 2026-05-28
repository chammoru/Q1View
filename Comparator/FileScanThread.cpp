#include "StdAfx.h"
#include "ComparatorDoc.h"
#include "ComparatorView.h"
#include "FrmCmpInfo.h"
#include "FileScanThread.h"
#include "FrmCmpStrategy.h"
#include "QDebug.h"

FileScanThread::FileScanThread(const CComparatorDoc *pDoc)
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
	SMutex::Autolock lock(mFrmCmpInfoLock);

	mCurFrames = mDoc->mMinFrames;
	if (mCurFrames > mFrmCmpInfoSize) {
		delete [] mFrmCmpInfo;

		mFrmCmpInfoSize = mCurFrames;
		mFrmCmpInfo = new FrmCmpInfo[mCurFrames];
	} else {
		for (int i = 0; i < mCurFrames; i++) {
			mFrmCmpInfo[i].mParseDone = false;
			mFrmCmpInfo[i].deinit();
		}
	}
}

SmpError FileScanThread::readyToRun()
{
	if (mCurFrames <= 0) {
		LOGWRN("no frame to compare: minFrames(%d)", mCurFrames);
		return SMP_FAIL;
	}

	mCurFrameID = 0;

	for (int i = 0; i < COMPARING_VIEWS_NUM; i++) {
		const ComparatorPane *pane = &mDoc->mPane[i];
		SQPane *scanInfo = &mScanInfo[i];
		scanInfo->closeFrmSrcs();
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
		scanInfo->OpenFrmSrc(pane->pathName, mDoc->mSortedCscInfo,
			pane->srcW, pane->srcH, mDoc->mW, mDoc->mH);
	}

	mFrmCmpStrategy = mDoc->mFrmCmpStrategy;
	mFrmCmpStrategy->AllocBuffer(&mScanInfo[CComparatorDoc::IMG_VIEW_1],
		&mScanInfo[CComparatorDoc::IMG_VIEW_2]);

	return SMP_OK;
}

bool FileScanThread::threadLoop()
{
	for (int i = 0; i < COMPARING_VIEWS_NUM; i++) {
		SQPane *scanInfo = &mScanInfo[i];
		scanInfo->FillSceneBuf(scanInfo->origBuf);
	}

	SQPane *scanInfoL = &mScanInfo[CComparatorDoc::IMG_VIEW_1];
	SQPane *scanInfoR = &mScanInfo[CComparatorDoc::IMG_VIEW_2];

	FrmCmpInfo frmCmpInfo;
	double (*metrics)[QPLANES] = frmCmpInfo.mMetrics;
	mFrmCmpStrategy->DiffNMetrics(scanInfoL, scanInfoR, metrics, frmCmpInfo.diffRLC);
	frmCmpInfo.mParseDone = true;

	{
		SMutex::Autolock lock(mFrmCmpInfoLock);
		mFrmCmpInfo[mCurFrameID] = frmCmpInfo;
	}
	++mCurFrameID;
	if (mCurFrameID >= mDoc->mMinFrames)
		return false;

	return true;
}

void FileScanThread::deinit()
{
	SMutex::Autolock lock(mFrmCmpInfoLock);

	for (int i = 0; i < mCurFrames; i++) {
		mFrmCmpInfo[i].mParseDone = false;
		mFrmCmpInfo[i].deinit();
	}
}

bool FileScanThread::isFrameParsed(int frameID) const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);

	if (frameID < 0 || frameID >= mCurFrames)
		return false;

	return mFrmCmpInfo[frameID].mParseDone;
}

bool FileScanThread::isScanComplete() const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);

	if (mCurFrames <= 0)
		return true;

	return mFrmCmpInfo[mCurFrames - 1].mParseDone;
}

bool FileScanThread::copyDiffRLC(int frameID, list<RLC> diffRLC[QPLANES]) const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);

	if (frameID < 0 || frameID >= mCurFrames || !mFrmCmpInfo[frameID].mParseDone)
		return false;

	for (int i = 0; i < QPLANES; i++)
		diffRLC[i] = mFrmCmpInfo[frameID].diffRLC[i];

	return true;
}

bool FileScanThread::copyMetrics(int frameID, int metricIdx, double metrics[QPLANES]) const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);

	if (frameID < 0 || frameID >= mCurFrames || !mFrmCmpInfo[frameID].mParseDone)
		return false;

	for (int i = 0; i < QPLANES; i++)
		metrics[i] = mFrmCmpInfo[frameID].mMetrics[metricIdx][i];

	return true;
}
