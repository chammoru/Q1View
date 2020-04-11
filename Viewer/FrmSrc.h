#pragma once

#include "stdafx.h"

class CViewerDoc;

class FrmSrc
{
public:
	FrmSrc(bool fixedResolution)
	: mFixedResolution(fixedResolution) {}
	virtual ~FrmSrc() {};

	virtual bool Open(CString &filePath) = 0;
	virtual void ConfigureDoc(CViewerDoc *pDoc) = 0;
	virtual bool LoadOrigBuf(CViewerDoc *pDoc, BYTE *buf) = 0;
	virtual long CalNumFrame(CViewerDoc *pDoc) = 0;
	virtual bool SetFramePos(CViewerDoc *pDoc, long id) = 0;
	virtual bool Play(CViewerDoc *pDoc) = 0;
	virtual void Stop() = 0;
	virtual void Release() = 0;
	inline bool isFixed() { return mFixedResolution; }

private:
	bool mFixedResolution;
};
