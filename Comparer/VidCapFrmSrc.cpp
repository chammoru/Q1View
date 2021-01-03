#include "stdafx.h"
#include "VidCapFrmSrc.h"
#include "QImageStr.h"
#include "QDebug.h"

using namespace cv;

VidCapFrmSrc::VidCapFrmSrc(SQPane* pane)
: FrmSrc(pane, true)
, mW(-1)
, mH(-1)
{}

VidCapFrmSrc::~VidCapFrmSrc()
{
}

bool VidCapFrmSrc::Open(const CString& filePath)
{
	String str = CT2A(filePath.GetString());

	if (!mVidCap.open(str) || !mVidCap.isOpened())
		return false;

	mW = static_cast<int>(mVidCap.get(cv::CAP_PROP_FRAME_WIDTH));
	mH = static_cast<int>(mVidCap.get(cv::CAP_PROP_FRAME_HEIGHT));

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

	mW = -1;
	mH = -1;
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

const struct qcsc_info* VidCapFrmSrc::GetColorSpace(CString& pathName,
	struct qcsc_info* sortedCscInfo)
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
	cv::Mat matTemp(mH, mW, CV_8UC3, origBuf, mW * (size_t)QIMG_DST_RGB_BYTES);
	mVidCap >> matTemp;
	matTemp.release();

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
