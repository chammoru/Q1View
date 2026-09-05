#include "stdafx.h"
#include "VidCapThread.h"
#include "HdrToneMap.h"

VidCapThread::VidCapThread(SBufferPool * pBufferPool,
							SSafeCQ<BufferInfo> *pBufferQueue,
							cv::VideoCapture &vidCap,
							bool &toneMapHlg,
							long *pPlayFrameID,
							q1::ImageProcessor *pBgr888Processor)
: FrmProvideThread(pBufferPool, pBufferQueue, pPlayFrameID, pBgr888Processor)
, mVidCap(vidCap)
, mToneMapHlg(toneMapHlg)
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
	if (!ok || matTemp.empty() || matTemp.cols != mW || matTemp.rows != mH ||
		matTemp.type() != CV_8UC3)
		return false;
	const size_t stride = size_t(mW) * QIMG_DST_RGB_BYTES;
	if (matTemp.data != buf || matTemp.step != stride) {
		for (int y = 0; y < mH; ++y)
			memcpy(buf + size_t(y) * stride, matTemp.ptr(y), stride);
	}
	if (mToneMapHlg) {
		cv::parallel_for_(cv::Range(0, mH), [&](const cv::Range& range) {
			q1::ToneMapOpenCvHlgBgrRows(buf, mW, buf, mW, mW,
				range.start, range.end);
		});
	}

	return true;
}

bool VidCapThread::loadRgbBuf(long frameID, BYTE *buf)
{
	if (frameID >= mFrames)
		return false;

	const bool fuseRotation = mToneMapHlg && mRot != QROT_000;
	BYTE* decoded = fuseRotation ? mOrigBuf : buf;
	const int decodedStridePixels = fuseRotation ? mW : ROUNDUP_DWORD(mW);
	const size_t decodedStride = size_t(decodedStridePixels) * QIMG_DST_RGB_BYTES;
	cv::Mat matTemp(mH, mW, CV_8UC3, decoded, decodedStride);
	bool ok = mVidCap.read(matTemp);
	if (!ok || matTemp.empty() || matTemp.cols != mW || matTemp.rows != mH ||
		matTemp.type() != CV_8UC3) {
		return false;
	}

	if (matTemp.data != decoded || matTemp.step != decodedStride) {
		for (int y = 0; y < mH; ++y)
			memcpy(decoded + y * decodedStride, matTemp.ptr(y), mW * QIMG_DST_RGB_BYTES);
	}
	if (fuseRotation) {
		const int destinationWidth = (mRot == QROT_090 || mRot == QROT_270) ? mH : mW;
		constexpr int tileSize = 8;
		const int tileColumns = (mW + tileSize - 1) / tileSize;
		cv::parallel_for_(cv::Range(0, tileColumns), [&](const cv::Range& range) {
			q1::ToneMapOpenCvHlgBgrRotateTiles(decoded, mW, mH, buf,
				ROUNDUP_DWORD(destinationWidth), int(mRot) * 90,
				range.start, range.end);
		});
	} else if (mToneMapHlg) {
		cv::parallel_for_(cv::Range(0, mH), [&](const cv::Range& range) {
			q1::ToneMapOpenCvHlgBgrRows(decoded, ROUNDUP_DWORD(mW), buf,
				ROUNDUP_DWORD(mW), mW, range.start, range.end);
		});
	}

	return true;
}

void VidCapThread::cancelFrameReservation(long frameID)
{
	// VidCapFrmSrc owns a single sequential worker. GetNextFrameID() reserves
	// the ID before decoding, so undo that reservation when no frame was read.
	if (*mPlayFrameIdPtr == frameID)
		qcmn_atomic_dec(mPlayFrameIdPtr);
}
