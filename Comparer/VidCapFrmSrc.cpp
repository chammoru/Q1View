#include "stdafx.h"
#include "VidCapFrmSrc.h"
#include "QImageStr.h"
#include "QDebug.h"

#include <opencv2/imgproc/imgproc.hpp>

using namespace cv;

VidCapFrmSrc::VidCapFrmSrc(SQPane* pane)
: FrmSrc(pane, true)
, mSrcW(-1)
, mSrcH(-1)
{}

VidCapFrmSrc::~VidCapFrmSrc()
{
}

bool VidCapFrmSrc::Open(const CString& filePath, const struct qcsc_info* sortedCscInfo,
	int srcW, int srcH, int dstW, int dstH)
{
	String str = CT2A(filePath.GetString());

	if (!mVidCap.open(str) || !mVidCap.isOpened())
		return false;

	mSrcW = static_cast<int>(mVidCap.get(cv::CAP_PROP_FRAME_WIDTH));
	mSrcH = static_cast<int>(mVidCap.get(cv::CAP_PROP_FRAME_HEIGHT));
	mDstW = dstW;
	mDstH = dstH;

	return true;
}

int VidCapFrmSrc::GetFrameNum()
{
	return static_cast<long>(mVidCap.get(cv::CAP_PROP_FRAME_COUNT));
}

void VidCapFrmSrc::Release()
{
	if (IsAvailable())
		mVidCap.release();

	mSrcW = -1;
	mSrcH = -1;
}

bool VidCapFrmSrc::GetResolution(CString& pathName, int* w, int* h)
{
	String str = CT2A(pathName.GetString());
	VideoCapture vidCap;
	if (!vidCap.open(str) || !vidCap.isOpened())
		return false;

	*w = static_cast<int>(vidCap.get(cv::CAP_PROP_FRAME_WIDTH));
	*h = static_cast<int>(vidCap.get(cv::CAP_PROP_FRAME_HEIGHT));
	vidCap.release();

	return true;
}

const struct qcsc_info* VidCapFrmSrc::GetColorSpace(const CString& pathName,
	const struct qcsc_info* sortedCscInfo, bool doReisze)
{
	String str = CT2A(pathName.GetString());
	VideoCapture vidCap;
	if (!vidCap.open(str) || !vidCap.isOpened())
		return NULL;

	vidCap.release();
	return q1::image_find_cs(sortedCscInfo, "bgr888");
}

bool VidCapFrmSrc::IsAvailable()
{
	return  mVidCap.isOpened();
}

bool VidCapFrmSrc::FillSceneBuf(BYTE* origBuf)
{
	if (mSrcW != mDstW || mSrcH != mDstH) {
		Mat matSrcTemp(mSrcH, mSrcW, CV_8UC3);
		mVidCap >> matSrcTemp;
		Mat matDstTemp(mDstH, mDstW, CV_8UC3, origBuf, mDstW * (size_t)QIMG_DST_RGB_BYTES);
		resize(matSrcTemp, matDstTemp, Size(mDstW, mDstH));
		matSrcTemp.release();
		memcpy(origBuf, matDstTemp.ptr(), matDstTemp.total() * matDstTemp.elemSize());
		matDstTemp.release();
	} else {
		Mat matSrcTemp(mSrcH, mSrcW, CV_8UC3, origBuf, mSrcW * (size_t)QIMG_DST_RGB_BYTES);
		mVidCap >> matSrcTemp;
		matSrcTemp.release();
	}

	return true;
}

long VidCapFrmSrc::GetNextFrameID()
{
	return static_cast<long>(mVidCap.get(cv::CAP_PROP_POS_FRAMES));
}

bool VidCapFrmSrc::SetNextFrameID(long frameID)
{
	bool ok = mVidCap.set(cv::CAP_PROP_POS_FRAMES, frameID);
	if (!ok) {
		LOGWRN("failed to mVidCap.set with frameID[%d]", frameID);
		return false;
	}

	return true;
}

double VidCapFrmSrc::GetFps()
{
	return mVidCap.get(cv::CAP_PROP_FPS);
}
