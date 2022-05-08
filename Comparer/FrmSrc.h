#pragma once
struct qcsc_info;
struct SQPane;

class FrmSrc
{
public:
	FrmSrc(SQPane* pane, bool fixedResolution);
	virtual ~FrmSrc() {}
	virtual bool Open(const CString& filePath, const struct qcsc_info* sortedCscInfo,
		int srcW, int srcH, int dstW, int dstH) = 0;
	virtual int GetFrameNum() = 0;
	virtual void Release() = 0;
	virtual bool GetResolution(CString &pathName, int* w, int* h) = 0;
	virtual const struct qcsc_info* GetColorSpace(const CString &pathName,
		const struct qcsc_info* sortedCscInfo, bool doReisze) = 0;
	virtual bool IsAvailable() = 0;
	virtual bool FillSceneBuf(BYTE *origBuf) = 0;
	virtual long GetNextFrameID() = 0;
	virtual bool SetNextFrameID(long frameID) = 0;
	virtual double GetFps() = 0;
	inline bool isFixedResolution() { return mFixedResolution; }

protected:
	SQPane *mPane;

private:
	bool mFixedResolution;
};
