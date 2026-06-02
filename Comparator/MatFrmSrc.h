#pragma once
#include <vector>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include "FrmSrc.h"

class MatFrmSrc : public FrmSrc
{
public:
	MatFrmSrc(SQPane *pane);

	virtual ~MatFrmSrc();
	static bool AreInSameImageSequence(const CString& filePathA, int srcWA, int srcHA,
		const CString& filePathB, int srcWB, int srcHB);
	virtual bool Open(const CString& filePath, const struct qcsc_info* sortedCscInfo,
		int srcW, int srcH, int dstW, int dstH);
	virtual int GetFrameNum();
	virtual void Release();
	virtual bool GetResolution(CString &pathName, int* w, int* h);
	virtual const struct qcsc_info* GetColorSpace(const CString &pathName,
		const struct qcsc_info* sortedCscInfo, bool doResize);
	virtual bool IsAvailable();
	virtual bool FillSceneBuf(BYTE* origBuf);
	virtual long GetNextFrameID();
	virtual bool SetNextFrameID(long frameID);
	virtual double GetFps();

private:
	void BuildFileList(const CString& filePath, int srcW, int srcH);
	bool LoadFrame(long id);
	bool AdvanceTo(long startID);

	cv::Mat mOcvMat;
	std::vector<CString> mFileList;
	long mCurFrameID;
	int mDstW, mDstH;
};

