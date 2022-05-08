#pragma once
#include "FrmSrc.h"

class RawFrmSrc : public FrmSrc
{
public:
	RawFrmSrc(SQPane *pane);

	virtual ~RawFrmSrc();
	virtual bool Open(const CString& filePath, const struct qcsc_info* sortedCscInfo,
		int srcW, int srcH, int dstW, int dstH);
	virtual int GetFrameNum();
	virtual void Release();
	virtual bool GetResolution(CString &pathName, int* w, int* h);
	virtual const struct qcsc_info* GetColorSpace(const CString &pathName,
		const struct qcsc_info* sortedCscInfo, bool doReisze);
	virtual bool IsAvailable();
	virtual bool FillSceneBuf(BYTE* origBuf);
	virtual long GetNextFrameID();
	virtual bool SetNextFrameID(long frameID);
	virtual double GetFps();

private:
	CFile mFile;
	ULONGLONG mFileSize;
	size_t mOrigSceneSize, mResizeSceneSize;
	int mSrcW, mSrcH, mDstW, mDstH;
	const struct qcsc_info* mSrcCscInfo;
	BYTE* mResizeBuf;
};
