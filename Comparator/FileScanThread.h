#pragma once

#include "SThread.h"
#include "SMutex.h"
#include "qimage_cs.h"
#include "QImageDiff.h"
#include "ComparatorPane.h"
#include "ComparatorDoc.h"
#include "FrmCmpInfo.h"
#include "ComparisonResults.h"

using namespace std;

class FileScanThread : public SThread
{
	const CComparatorDoc *mDoc;
	mutable SMutex mFrmCmpInfoLock;
	std::vector<FrmCmpInfo> mFrames[3];
	PairSequenceSummary mSummary[3];
	unsigned mPairGeneration[3] = {}, mScanGeneration = 0;
	int mCurFrameID = 0, mCurFrames = 0;
	bool mReadersReady = false;
	unsigned mDirtyReaders = 0xf;
	SQPane mScanInfo[4];
	IFrmCmpStrategy* mStrategies[3] = {};
	unsigned mDecodeCounts[4] = {};
	// Independently replaceable pair slots keep unrelated in-flight work valid.
	std::shared_ptr<const CurrentComparisonRequest> mPending[3];
	unsigned long long mLatestRequest[3] = {}, mCurrentMeasureCounts[3] = {};
	int mNextCurrentTarget = 0;
	bool mLiveRunning = false;
	PairCurrentResult mLive[3];
	void MeasureCurrent(const CurrentComparisonRequest& request, int target);
	void Accumulate(int target, int frame, int metric);
	void FailAnalysis(const CString& error);

public:

	FileScanThread(const CComparatorDoc *pDoc);
	virtual ~FileScanThread(void);
	virtual SmpError readyToRun();
	virtual bool threadLoop();
	void setup(unsigned changedSlots = 0xf);
	void deinit();
	bool isFrameParsed(int frameID, int target = 1) const;
	bool isScanComplete() const;
	bool isScanRunning() const { return isRunning(); }
	bool copyDiffRLC(int frameID, list<RLC> diffRLC[QPLANES], int target = 1) const;
	bool copyMetrics(int frameID, int metricIdx, double metrics[QPLANES], int target = 1) const;

	unsigned getScanGeneration() const;
	unsigned pairGeneration(int target) const;
	bool setLazyMetric(int frameID, int metricIdx, double v, unsigned gen, int target = 1);
	PairSequenceSummary summary(int target) const;
	void setPairError(int target, const CString& error, int metric = -1);
	unsigned decodeCount(int slot) const;
	void queueCurrent(std::shared_ptr<const CurrentComparisonRequest> request);
	bool copyCurrent(int target, unsigned long long request, PairCurrentResult& result) const;
	unsigned long long currentMeasureCount(int target) const;
	unsigned pendingCurrentMask() const;
	void cancelCurrent();
	bool hasWork() const;
	void ensureRunning();
};
