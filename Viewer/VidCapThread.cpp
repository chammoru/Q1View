#include "stdafx.h"
#include "VidCapThread.h"

VidCapThread::VidCapThread(SBufferPool * pBufferPool,
							SSafeCQ<BufferInfo> *pBufferQueue,
							cv::VideoCapture &vidCap,
							long *pPlayFrameID,
							q1::ImageProcessor *pBgr888Processor)
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
	bool ok = mVidCap.read(matTemp);
	if (!ok || matTemp.empty())
		return false;

	return true;
}

void VidCapThread::cancelFrameReservation(long frameID)
{
	// VidCapFrmSrc owns a single sequential worker. GetNextFrameID() reserves
	// the ID before decoding, so undo that reservation when no frame was read.
	if (*mPlayFrameIdPtr == frameID)
		qcmn_atomic_dec(mPlayFrameIdPtr);
}
