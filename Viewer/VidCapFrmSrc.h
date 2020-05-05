#pragma once

#include "FrmSrc.h"

#include <opencv2/core/core.hpp>

#include "VidCapThread.h"

#include <string>

class VidCapFrmSrc : public FrmSrc {
public:
	VidCapFrmSrc(CViewerDoc *pDoc)
	: FrmSrc(true), mNextFrame(-1)
	{
		mVidCapThread = new VidCapThread(pDoc->mBufferPool, pDoc->mBufferQueue,
		mVidCap, &pDoc->mPlayFrameID, pDoc->mBgr888Processor);
	}

	virtual ~VidCapFrmSrc()
	{
		Release();
		delete mVidCapThread;
	}

	virtual inline bool Open(CString &filePath)
	{
		std::string str = CT2A(filePath.GetString());

		if (!mVidCap.open(str) || !mVidCap.isOpened())
			return false;
		
		mNextFrame = 0;

		mFrames = static_cast<long>(mVidCap.get(cv::CAP_PROP_FRAME_COUNT));

		return true;
	}

	virtual inline void ConfigureDoc(CViewerDoc *pDoc)
	{
		pDoc->mW = static_cast<int>(mVidCap.get(cv::CAP_PROP_FRAME_WIDTH));
		pDoc->mH = static_cast<int>(mVidCap.get(cv::CAP_PROP_FRAME_HEIGHT));

		pDoc->mColorSpace = QIMAGE_CS_BGR888;
		pDoc->mCsc2Rgb888 = qimage_bgr888_to_bgr888;
		pDoc->mCsLoadInfo = qimage_rgb888_load_info;

		pDoc->mFps = mVidCap.get(cv::CAP_PROP_FPS);
	}

	virtual inline bool LoadOrigBuf(CViewerDoc *pDoc, BYTE *buf)
	{
		if (pDoc->mCurFrameID != mNextFrame) {
			bool ok = mVidCap.set(cv::CAP_PROP_POS_FRAMES, pDoc->mCurFrameID);
			if (!ok)
				return false;
		}

		int w = pDoc->mW, h = pDoc->mH;
		if (pDoc->mRot == QROT_090 || pDoc->mRot == QROT_270)
			QSWAP(w, h);

		cv::Mat matTemp(h, w, CV_8UC3, buf, w * (size_t)QIMG_DST_RGB_BYTES);
		mVidCap >> matTemp;
		matTemp.release();

		mNextFrame = pDoc->mCurFrameID + 1;

		return true;
	}

	virtual inline long CalNumFrame(CViewerDoc *pDoc)
	{
		return mFrames;
	}

	virtual inline bool SetFramePos(CViewerDoc *pDoc, long id)
	{
		if (id >= mFrames)
			return false;

		mNextFrame = id;

		return mVidCap.set(cv::CAP_PROP_POS_FRAMES, id);
	}

	virtual inline bool Play(CViewerDoc *pDoc)
	{
		bool ok = mVidCapThread->setup(pDoc);
		ASSERT(ok);
		if (!ok)
			return false;
		mVidCapThread->run();

		return true;
	}

	virtual inline void Stop()
	{
		mVidCapThread->requestExitAndWait();
	}

	virtual inline void Release()
	{
		mVidCap.release();
		mNextFrame = -1;
	}

private:
	cv::VideoCapture mVidCap;
	long mNextFrame;
	long mFrames;
	VidCapThread *mVidCapThread;
};
