#pragma once

#include "SThread.h"
#include "SMutex.h"
#include "qimage_cs.h"
#include "QImageDiff.h"
#include "ComparatorPane.h"
#include "ComparatorDoc.h"
#include "FrmCmpInfo.h"

using namespace std;

class FileScanThread : public SThread
{
	const CComparatorDoc *mDoc;
	long mCurFrameID;
	int mFrmCmpInfoSize;
	FrmCmpInfo *mFrmCmpInfo;
	mutable SMutex mFrmCmpInfoLock;
	int mCurFrames;
	SQPane mScanInfo[COMPARING_VIEWS_NUM];
	IFrmCmpStrategy *mFrmCmpStrategy;

public:

	FileScanThread(const CComparatorDoc *pDoc);
	virtual ~FileScanThread(void);
	virtual SmpError readyToRun();
	virtual bool threadLoop();
	void setup();
	void deinit();
	bool isFrameParsed(int frameID) const;
	bool isScanComplete() const;
	bool copyDiffRLC(int frameID, list<RLC> diffRLC[QPLANES]) const;
	bool copyMetrics(int frameID, int metricIdx, double metrics[QPLANES]) const;
};
