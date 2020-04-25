#pragma once
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "FrmSrc.h"

class VidCapFrmSrc : public FrmSrc
{
public:
	VidCapFrmSrc(SQPane* pane);

	virtual ~VidCapFrmSrc();
	virtual bool Open(const CString& filePath);
	virtual int GetFrameNum();
	virtual void Release();
	virtual bool GetResolution(CString& pathName, int* w, int* h);
	virtual const struct qcsc_info* GetColorSpace(CString& pathName,
		struct qcsc_info* sortedCscInfo);
	virtual bool IsAvailable();
	virtual bool FillSceneBuf(BYTE* origBuf);
	virtual long GetNextFrameID();
	virtual bool SetNextFrameID(long frameID);

private:
	cv::VideoCapture mVidCap;
	int mW, mH;
};
