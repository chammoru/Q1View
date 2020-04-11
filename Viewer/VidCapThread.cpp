#include "stdafx.h"
#include "VidCapThread.h"

VidCapThread::VidCapThread(SBufferPool * pBufferPool,
							SSafeCQ<BufferInfo> *pBufferQueue,
							cv::VideoCapture &vidCap,
							long *pPlayFrameID,
							QImageProcessor *pBgr888Processor)
: FrmProvideThread(pBufferPool, pBufferQueue, pPlayFrameID, pBgr888Processor)
, mVidCap(vidCap)
{
}

VidCapThread::~VidCapThread(void)
{
}

bool VidCapThread::setupDetail(CViewerDoc *pDoc)
{
	mFrames = pDoc->mFrames;

	return true;
}

void VidCapThread::sendQuitMsg(long frameID)
{
	BufferInfo bi;
	bi.ID = MSG_QUIT;
	bi.addr = 0;

	mBufferQueue->push(bi);
}

bool VidCapThread::loadOrigBuf(long frameID, BYTE *buf)
{
	if (frameID >= mFrames)
		return false;

	cv::Mat matTemp(mH, mW, CV_8UC3, buf, mW * QIMG_DST_RGB_BYTES);
	mVidCap >> matTemp;

	return true;
}
