#pragma once

#include "SThread.h"
#include "qimage_cs.h"
#include "QImageDiff.h"
#include "ComparerPane.h"
#include "ComparerDoc.h"

using namespace std;

class FrmCmpInfo;

class FileScanThread : public SThread
{
	const CComparerDoc *mDoc;
	long mCurFrameID;
	int mFrmCmpInfoSize;
	FrmCmpInfo *mFrmCmpInfo;
	int mCurFrames;
	SQPane mScanInfo[COMPARING_VIEWS_NUM];
	IFrmCmpStrategy *mFrmCmpStrategy;

public:

	FileScanThread(const CComparerDoc *pDoc);
	virtual ~FileScanThread(void);
	virtual SmpError readyToRun();
	virtual bool threadLoop();
	void setup();
	void deinit();
	inline const FrmCmpInfo *getFrmCmpInfo() const { return mFrmCmpInfo; }
};
