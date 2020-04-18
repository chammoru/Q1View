#pragma once
#include "FrmSrc.h"

class CComparerDoc;

class RawFrmSrc : public FrmSrc
{
public:
	RawFrmSrc(SQPane *pane);

	virtual ~RawFrmSrc();
	virtual bool Open(const CString& filePath);
	virtual int GetFrameNum();
	virtual void Release();
	virtual bool GetResolution(CString &pathName, int* w, int* h);
	virtual const struct qcsc_info* GetColorSpace(CString &pathName,
		struct qcsc_info* sortedCscInfo);
	virtual bool IsAvailable();
	virtual void FillSceneBuf(BYTE* origBuf, long curFrameID);

private:
	CFile mFile;
	ULONGLONG mFileSize;
};

