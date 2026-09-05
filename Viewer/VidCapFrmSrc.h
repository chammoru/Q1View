#pragma once

#include "FrmSrc.h"

#include <opencv2/core/core.hpp>

#include "QCvUtil.h"
#include "QDebug.h"
#include "HdrToneMap.h"
#include "VidCapThread.h"
#include "VideoColorInfo.h"
#include "VideoOrientation.h"

#include <cmath>
#include <string>

class VidCapFrmSrc : public FrmSrc {
public:
	VidCapFrmSrc(CViewerDoc *pDoc)
	: FrmSrc(true), mNextFrame(-1), mMetadataRotation(0), mToneMapHlg(false)
	{
		mVidCapThread = new VidCapThread(pDoc->mBufferPool, pDoc->mBufferQueue,
		mVidCap, mToneMapHlg, &pDoc->mPlayFrameID, pDoc->mBgr888Processor);
	}

	virtual ~VidCapFrmSrc()
	{
		Release();
		delete mVidCapThread;
	}

	virtual inline bool Open(CString &filePath)
	{
		mToneMapHlg = ::GetEnvironmentVariableW(
			L"Q1VIEW_DISABLE_HLG_TONE_MAP", nullptr, 0) == 0 &&
			q1::IsBt2020HlgVideo(filePath.GetString());
		if (!q1::openVideoCaptureW(mVidCap, filePath.GetString())) {
			mToneMapHlg = false;
			return false;
		}
		if (mToneMapHlg)
			q1::PrepareOpenCvHlgToSdrLut();
		if (mToneMapHlg)
			LOGINF("%s", "BT.2020 HLG to SDR tone mapping active");
		// Newer OpenCV versions can apply container orientation themselves.
		// Keep decoded pixels in coded orientation because Q1View applies the
		// same metadata through its own rotation pipeline below.
#if CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR >= 5)
		mVidCap.set(cv::CAP_PROP_ORIENTATION_AUTO, 0);
#endif
		
		mNextFrame = 0;
		mMetadataRotation = 0;
		q1::ReadVideoDisplayRotation(filePath.GetString(), mMetadataRotation);

		mFrames = static_cast<long>(mVidCap.get(cv::CAP_PROP_FRAME_COUNT));

		return true;
	}

	virtual inline void ConfigureDoc(CViewerDoc *pDoc)
	{
		const int sourceW = static_cast<int>(mVidCap.get(cv::CAP_PROP_FRAME_WIDTH));
		const int sourceH = static_cast<int>(mVidCap.get(cv::CAP_PROP_FRAME_HEIGHT));
		pDoc->mOrigW = sourceW;
		pDoc->mOrigH = sourceH;
		pDoc->mW = sourceW;
		pDoc->mH = sourceH;
		pDoc->mRot = static_cast<QROTATION>((mMetadataRotation / 90) % QROT_MAX);
		if (pDoc->mRot == QROT_090 || pDoc->mRot == QROT_270)
			QSWAP(pDoc->mW, pDoc->mH);

		pDoc->mColorSpace = QIMAGE_CS_BGR888;
		pDoc->mCsc2Rgb888 = qimage_bgr888_to_bgr888;
		pDoc->mCsLoadInfo = qimage_rgb888_load_info;
		pDoc->mSampleNativePixel = nullptr;

		pDoc->mFps = mVidCap.get(cv::CAP_PROP_FPS);
		pDoc->mHasTimingFps = pDoc->mFps > 0.0 && std::isfinite(pDoc->mFps);
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
		bool ok = mVidCap.read(matTemp) && !matTemp.empty() &&
			matTemp.cols == w && matTemp.rows == h && matTemp.type() == CV_8UC3;
		if (!ok) {
			mNextFrame = -1;
			return false;
		}
		const size_t stride = size_t(w) * QIMG_DST_RGB_BYTES;
		if (matTemp.data != buf || matTemp.step != stride) {
			for (int y = 0; y < h; ++y)
				memcpy(buf + size_t(y) * stride, matTemp.ptr(y), stride);
		}
		if (mToneMapHlg) {
			cv::parallel_for_(cv::Range(0, h), [&](const cv::Range& range) {
				q1::ToneMapOpenCvHlgBgrRows(buf, w, buf, w, w,
					range.start, range.end);
			});
		}

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

		if (!mVidCap.set(cv::CAP_PROP_POS_FRAMES, id))
			return false;

		mNextFrame = id;
		return true;
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

	virtual bool isVideo() const override { return true; }
	virtual bool usesHlgToneMapping() const override { return mToneMapHlg; }

	virtual inline void Stop()
	{
		mVidCapThread->requestExitAndWait();
		mVidCapThread->LogPlaybackTrace();
	}

	virtual inline void Release()
	{
		mVidCap.release();
		mNextFrame = -1;
		mMetadataRotation = 0;
		mToneMapHlg = false;
	}

private:
	cv::VideoCapture mVidCap;
	long mNextFrame;
	long mFrames;
	int mMetadataRotation;
	bool mToneMapHlg;
	VidCapThread *mVidCapThread;
};
