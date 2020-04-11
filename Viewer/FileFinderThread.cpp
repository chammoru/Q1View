#include "stdafx.h"
#include "FileFinderThread.h"

FileFinderThread::FileFinderThread(SBufferPool * pBufferPool,
								 SSafeCQ<BufferInfo> *pBufferQueue,
								 long *pPlayFrameID,
								 QImageProcessor *pBgr888Processor)
: FrmProvideThread(pBufferPool, pBufferQueue, pPlayFrameID, pBgr888Processor)
{
}

FileFinderThread::~FileFinderThread()
{
}

bool FileFinderThread::setupDetail(CViewerDoc *pDoc)
{
	CString &rPathName = pDoc->mPathName;

	mFinder.Close();
	mWorking = mFinder.FindFile(pDoc->mPurePathRegex);
	if (!mWorking)
		return false;

	bool found = false;
	while (mWorking) {
		mWorking = mFinder.FindNextFile();

		if (mFinder.IsDirectory())
			continue;

		if (rPathName == mFinder.GetFilePath()) {
			found = true;
			break;
		}
	}

	if (!found)
		return false;

	return true;
}

void FileFinderThread::sendQuitMsg(long frameID)
{
	BufferInfo bi;
	bi.ID = MSG_QUIT;
	bi.addr = 0;

	mBufferQueue->push(bi);
}

bool FileFinderThread::loadOrigBuf(long frameID, BYTE *buf)
{
	CString candidate = _T("");
	if (!mWorking)
		return false;

	while (mWorking) {
		mWorking = mFinder.FindNextFile();
		if (mFinder.IsDirectory())
			continue;

		candidate = mFinder.GetFilePath();
		break;
	}

	if (candidate == _T(""))
		return false;

	std::string str = CT2A(candidate);
	// We cannot know the cols and rows unless imreading.
	// I want to remove the 'copyTo' by just parsing the file
	mOcvMat = cv::imread(str);
	if (mOcvMat.data == NULL || mOcvMat.cols != mW || mOcvMat.rows != mH)
		return false;

	cv::Mat matTemp(mH, mW, CV_8UC3, buf, mW * QIMG_DST_RGB_BYTES);
	mOcvMat.copyTo(matTemp);
	mOcvMat.release();

	return true;
}
