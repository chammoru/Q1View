
#include "stdafx.h"
#include "FrmProvideThread.h"
#include "ViewerCmn.h"
#include "QOcv.h"

bool FrmProvideThread::threadLoop()
{
	BYTE *RGB = mBufferPool->checkout();
	if (RGB == NULL)
		return false;

	long frameID = GetNextFrameID();
	bool ok = loadOrigBuf(frameID, mOrigBuf);
	if (!ok) {
		sendQuitMsg(frameID);
		mBufferPool->turn_back(RGB);
		return false;
	}

	BufferInfo bi = PostProcess(mColorSpace, mBgr888Processor, mW, mH,
		mOrigBuf, RGB, mBufOffset2, mBufOffset3, mRot, mCsc2Rgb888, frameID);

	mBufferQueue->ordered_push(bi, frameID);

	return true;
}
