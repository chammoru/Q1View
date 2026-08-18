
#include "stdafx.h"
#include "FrmProvideThread.h"
#include "ViewerCmn.h"
#include "QOcv.h"

void FrmProvideThread::LogPlaybackTrace() const
{
	if (!mTraceEnabled || mTraceFrameCount == 0) {
		return;
	}

	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	double tickToMs = 1000.0 / static_cast<double>(frequency.QuadPart);
	LOGWRN("worker summary: frames=%ld read_avg=%.3fms post_avg=%.3fms",
		mTraceFrameCount, mTraceLoadTicks * tickToMs / mTraceFrameCount,
		mTracePostProcessTicks * tickToMs / mTraceFrameCount);
}

bool FrmProvideThread::threadLoop()
{
	BYTE *RGB = mBufferPool->checkout();
	if (RGB == NULL)
		return false;

	long frameID = GetNextFrameID();
	const bool directRgb = mColorSpace == QIMAGE_CS_BGR888 &&
		mBgr888Processor == NULL && mRot == QROT_000 &&
		supportsDirectRgbLoad();
	LARGE_INTEGER loadStart = {};
	LARGE_INTEGER loadEnd = {};
	if (mTraceEnabled)
		QueryPerformanceCounter(&loadStart);
	bool ok = directRgb ? loadRgbBuf(frameID, RGB) :
		loadOrigBuf(frameID, mOrigBuf);
	if (mTraceEnabled)
		QueryPerformanceCounter(&loadEnd);
	if (!ok) {
		cancelFrameReservation(frameID);
		sendQuitMsg(frameID);
		mBufferPool->turn_back(RGB);
		return false;
	}

	LARGE_INTEGER postEnd = {};
	BufferInfo bi;
	if (directRgb) {
		bi.ID = frameID;
		bi.addr = RGB;
	} else {
		bi = PostProcess(mColorSpace, mBgr888Processor, mW, mH,
			mOrigBuf, RGB, mBufOffset2, mBufOffset3, mRot, mCsc2Rgb888, frameID);
	}
	if (mTraceEnabled) {
		QueryPerformanceCounter(&postEnd);
		mTraceFrameCount++;
		mTraceLoadTicks += loadEnd.QuadPart - loadStart.QuadPart;
		mTracePostProcessTicks += postEnd.QuadPart - loadEnd.QuadPart;
	}

	mBufferQueue->ordered_push(bi, frameID);

	return true;
}
