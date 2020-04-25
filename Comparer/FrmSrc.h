#pragma once
struct qcsc_info;
struct SQPane;

class FrmSrc
{
public:
	FrmSrc(SQPane* pane);
	virtual ~FrmSrc() {}
	virtual bool Open(const CString& filePath) = 0;
	virtual int GetFrameNum() = 0;
	virtual void Release() = 0;
	virtual bool GetResolution(CString &pathName, int* w, int* h) = 0;
	virtual const struct qcsc_info* GetColorSpace(CString &pathName,
		struct qcsc_info* sortedCscInfo) = 0;
	virtual bool IsAvailable() = 0;
	virtual bool FillSceneBuf(BYTE *origBuf) = 0;
	virtual long GetNextFrameID() = 0;
	virtual bool SetNextFrameID(long frameID) = 0;

protected:
	SQPane *mPane;

private:
};
