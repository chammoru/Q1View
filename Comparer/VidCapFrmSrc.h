#pragma once
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "FrmSrc.h"

class VidCapFrmSrc : public FrmSrc
{
public:
	VidCapFrmSrc(SQPane* pane);

	virtual ~VidCapFrmSrc();
	virtual bool Open(const CString& filePath, const struct qcsc_info* sortedCscInfo,
		int srcW, int srcH, int dstW, int dstH);
	virtual int GetFrameNum();
	virtual void Release();
	virtual bool GetResolution(CString& pathName, int* w, int* h);
	virtual const struct qcsc_info* GetColorSpace(const CString& pathName,
		const struct qcsc_info* sortedCscInfo, bool doReisze);
	virtual bool IsAvailable();
	virtual bool FillSceneBuf(BYTE* origBuf);
	virtual long GetNextFrameID();
	virtual bool SetNextFrameID(long frameID);
	virtual double GetFps();

private:
	cv::VideoCapture mVidCap;
	int mSrcW, mSrcH, mDstW, mDstH;
};
