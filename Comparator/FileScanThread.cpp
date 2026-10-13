#include "StdAfx.h"
#include "FileScanThread.h"
#include "FrmCmpStrategy.h"
#include "LpipsEngine.h"
#include <cmath>

FileScanThread::FileScanThread(const CComparatorDoc* doc) : mDoc(doc) {}
FileScanThread::~FileScanThread() { requestExitAndWait(); }

void FileScanThread::setup(unsigned changedSlots)
{
	// Called on the UI thread only after both workers have joined.
	SMutex::Autolock lock(mFrmCmpInfoLock);
	++mScanGeneration;
	for (int i=0;i<3;++i) { mPending[i].reset(); mLatestRequest[i]=0; }
	mLiveRunning = false; mNextCurrentTarget=0;
	mCurFrameID = mCurFrames = 0; mReadersReady = false;
	mDirtyReaders |= changedSlots;
	for (int i=0;i<4;++i) if (!mDoc->mPane[i].isAvail()) mScanInfo[i].closeFrmSrcs();
	memset(mDecodeCounts, 0, sizeof(mDecodeCounts));
	for (int t = 1; t < 4; ++t) {
		int n = mDoc->PairFrames(t);
		if ((changedSlots & (1u | (1u << t))) || n != mSummary[t-1].eligible) {
			++mPairGeneration[t-1];
			mFrames[t-1].clear(); mFrames[t-1].resize(n);
			mSummary[t-1] = PairSequenceSummary();
			mSummary[t-1].eligible = n;
		}
		mStrategies[t-1] = mDoc->PairStrategy(t);
		mCurFrames = max(mCurFrames, n);
	}
}

SmpError FileScanThread::readyToRun()
{
	try {
		if (mReadersReady) return SMP_OK;
		// UI source metadata is immutable between setup and requestExitAndWait.
		for (int i = 0; i < 4; ++i) {
			auto& scan = mScanInfo[i];
			const auto& pane = mDoc->mPane[i];
			if (scan.isAvail() && !(mDirtyReaders&(1u<<i))) continue;
			scan.closeFrmSrcs();
			if (!pane.isAvail()) continue;
			scan.origSceneSize = pane.origSceneSize;
			if (scan.origBufSize < pane.origSceneSize) {
				delete[] scan.origBuf;
				scan.origBuf = new BYTE[pane.origSceneSize];
				scan.origBufSize = pane.origSceneSize;
			}
			scan.colorSpace = pane.colorSpace; scan.csLoadInfo = pane.csLoadInfo;
			scan.csc2yuv420 = pane.csc2yuv420; scan.csc2rgb888 = pane.csc2rgb888;
			scan.disableImageSequence = pane.disableImageSequence;
			scan.OpenFrmSrc(pane.pathName, mDoc->mSortedCscInfo,
				pane.srcW, pane.srcH, mDoc->mW, mDoc->mH);
		}
		for (int t = 1; t < 4; ++t)
			if (mStrategies[t-1]) mStrategies[t-1]->AllocBuffer(&mScanInfo[0], &mScanInfo[t]);
		mReadersReady = true; mDirtyReaders=0;
		return SMP_OK;
	} catch (const std::exception&) {
		FailAnalysis(_T("Error: source preparation failed")); return SMP_FAIL;
	}
}

void FileScanThread::Accumulate(int target, int frame, int metric)
{
	// Caller holds the cache lock. Averages count actual finite samples only;
	// positive infinity is separately represented, never padded with zeros.
	auto& s = mSummary[target-1];
	const auto& f = mFrames[target-1][frame];
	++s.measured[metric];
	for (int p = 0; p < qmetric_info_table[metric].plane_count; ++p) {
		double v = f.mMetrics[metric][p];
		if (std::isinf(v)) { ++s.infinite[metric][p]; continue; }
		if (!std::isfinite(v)) continue;
		if (!s.finite[metric][p]) s.minimum[metric][p] = s.maximum[metric][p] = v;
		else {
			s.minimum[metric][p] = min(s.minimum[metric][p], v);
			s.maximum[metric][p] = max(s.maximum[metric][p], v);
		}
		++s.finite[metric][p]; s.sum[metric][p] += v;
	}
}

bool FileScanThread::threadLoop()
{
	try {
		std::shared_ptr<const CurrentComparisonRequest> live;
		int target=0;
		{
			SMutex::Autolock lock(mFrmCmpInfoLock);
			// Round-robin keeps another pair responsive during a rapid C-only seek.
			for (int offset=0;offset<3;++offset) {
				const int index=(mNextCurrentTarget+offset)%3;
				if (!mPending[index]) continue;
				live.swap(mPending[index]); target=index+1;
				mNextCurrentTarget=(index+1)%3; break;
			}
			mLiveRunning = !!live;
		}
		if (live) {
			MeasureCurrent(*live,target);
			SMutex::Autolock lock(mFrmCmpInfoLock);
			mLiveRunning = false;
		}
		if (exitPending()) return false;
		if (mCurFrameID < mCurFrames) {
			bool needed[4] = {};
			for (int t = 1; t < 4; ++t)
				if (mCurFrameID < mSummary[t-1].eligible && !isFrameParsed(mCurFrameID, t)
					&& mSummary[t-1].error.IsEmpty()) needed[0] = needed[t] = true;
			bool decoded[4] = {};
			for (int i = 0; i < 4; ++i) if (needed[i] && !exitPending()) {
				auto& scan = mScanInfo[i];
				decoded[i] = scan.isAvail() && scan.frmSrc->SetNextFrameID(mCurFrameID)
					&& scan.FillSceneBuf(scan.origBuf);
				SMutex::Autolock lock(mFrmCmpInfoLock);
				if (decoded[i]) ++mDecodeCounts[i];
			}
			for (int t = 1; t < 4 && !exitPending(); ++t) if (needed[t]) {
				if (!decoded[0] || !decoded[t]) {
					setPairError(t, _T("Error: frame decode failed")); continue;
				}
				FrmCmpInfo result;
				mStrategies[t-1]->DiffNMetrics(&mScanInfo[0], &mScanInfo[t], result.mMetrics, result.diffRLC);
				if (exitPending()) return false;
				result.mParseDone = true;
				SMutex::Autolock lock(mFrmCmpInfoLock);
				mFrames[t-1][mCurFrameID] = result;
				++mSummary[t-1].parsed;
				for (int m = 0; m < METRIC_COUNT; ++m)
					if (!qmetric_info_table[m].lazy && qmetric_info_table[m].measure) {
						bool valid=true;
						for (int p=0;p<qmetric_info_table[m].plane_count;++p)
							valid=valid && !std::isnan(result.mMetrics[m][p]);
						mFrames[t-1][mCurFrameID].mMetricDone[m] = valid;
						if (valid) Accumulate(t, mCurFrameID, m);
						else mSummary[t-1].metricError[m]=_T("Error: metric unavailable for decoded frame");
					}
			}
			SMutex::Autolock lock(mFrmCmpInfoLock);
			++mCurFrameID;
		}
		return hasWork();
	} catch (const std::exception&) {
		FailAnalysis(_T("Error: comparison worker stopped")); return false;
	}
}

void FileScanThread::FailAnalysis(const CString& error)
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	for (int t=1;t<4;++t) if (mSummary[t-1].eligible) {
		mSummary[t-1].error=error;
		mLive[t-1]=PairCurrentResult(); mLive[t-1].request=mLatestRequest[t-1]; mLive[t-1].state=error;
	}
	for (auto& pending:mPending) pending.reset();
	mLiveRunning=false; mCurFrameID=mCurFrames;
}

void FileScanThread::MeasureCurrent(const CurrentComparisonRequest& req, int t)
{
	const auto& metric = qmetric_info_table[req.metric];
	if (!exitPending()) {
		{
			SMutex::Autolock lock(mFrmCmpInfoLock);
			if (req.id!=mLatestRequest[t-1]) return;
			++mCurrentMeasureCounts[t-1];
		}
		if (!req.sources[0] || !req.sources[t] || !req.strategies[t-1]) return;
		PairCurrentResult result; result.request = req.id;
		auto* a = req.sources[0].get(); auto* b = req.sources[t].get();
		if (metric.lazy) {
			if (!metric.ml_id || !LpipsEngine::getInstance().availableFor(metric.ml_id)) {
				result.state = _T("Preparing");
			} else {
				double cached[QPLANES];
				const bool aligned=a->curFrameID==b->curFrameID;
				double value = aligned && copyMetrics(a->curFrameID,req.metric,cached,t) ? cached[0] :
					LpipsEngine::getInstance().distance(a->rgbBuf, b->rgbBuf,
					req.width, req.height, ROUNDUP_DWORD(req.width) * 3);
				if (std::isfinite(value)) {
					result.full.Format(_T("%s %.4f"), CString(CA2W(metric.name)).GetString(), value);
					result.state = _T("Ready");
				} else result.state = _T("Error: inference failed");
				if (req.hasRoi && !exitPending()) {
					size_t off = (size_t)req.t * ROUNDUP_DWORD(req.width) * 3 + req.l * 3;
					double crop = LpipsEngine::getInstance().distance(a->rgbBuf + off, b->rgbBuf + off,
						req.r - req.l + 1, req.b - req.t + 1, ROUNDUP_DWORD(req.width) * 3);
					if (std::isfinite(crop)) result.roi.Format(_T("crop %dx%d %s(%.4f)"),
						req.r - req.l + 1, req.b - req.t + 1, CString(CA2W(metric.name)).GetString(), crop);
					else result.roi = _T("ROI: inference error");
				}
			}
		} else {
			auto* strategy = req.strategies[t-1];
			strategy->AllocBuffer(a, b);
			strategy->CalMetrics(a, b, req.metric, result.full);
			result.full.Replace(_T("inf"), _T("\x221e"));
			FrmCmpInfo channels;
			strategy->DiffNMetrics(a, b, channels.mMetrics, channels.diffRLC);
			const auto labels = strategy->GetPlaneLabels();
			for (int p = 0; p < 3; ++p) {
				CString value;
				if (std::isinf(channels.mMetrics[req.metric][p])) value = _T("\x221e");
				else if (!std::isfinite(channels.mMetrics[req.metric][p])) value=_T("N/A");
				else value.Format(_T("%.4f"), channels.mMetrics[req.metric][p]);
				result.channels.AppendFormat(_T("%s%s %s"), p ? _T("  ") : _T(""),
					CString(CA2W(labels[p])).GetString(), value.GetString());
			}
			result.state = _T("Ready");
			if (result.channels.Find(_T("N/A"))>=0) result.state=_T("Error: channel measurement unavailable");
			if (req.hasRoi) result.roi.Format(_T("crop %dx%d %s"), req.r-req.l+1, req.b-req.t+1,
				strategy->CropScore(a, b, req.metric, req.l, req.t, req.r, req.b).GetString());
			result.roi.Replace(_T("inf"),_T("\x221e"));
		}
		if (exitPending()) return;
		SMutex::Autolock lock(mFrmCmpInfoLock);
		if (req.id == mLatestRequest[t-1]) mLive[t-1] = result;
	}
}

void FileScanThread::deinit() { setup(); }
bool FileScanThread::isFrameParsed(int frame, int t) const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	return t >= 1 && t <= 3 && frame >= 0 && frame < (int)mFrames[t-1].size()
		&& mFrames[t-1][frame].mParseDone;
}
bool FileScanThread::isScanComplete() const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	for (const auto& s : mSummary) if (s.error.IsEmpty() && s.parsed < s.eligible) return false;
	return true;
}
bool FileScanThread::copyDiffRLC(int frame, list<RLC> rlc[QPLANES], int t) const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	if (t < 1 || t > 3 || frame < 0 || frame >= (int)mFrames[t-1].size() || !mFrames[t-1][frame].mParseDone) return false;
	for (int p = 0; p < QPLANES; ++p) rlc[p] = mFrames[t-1][frame].diffRLC[p];
	return true;
}
bool FileScanThread::copyMetrics(int frame, int metric, double values[QPLANES], int t) const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	if (t < 1 || t > 3 || metric < 0 || metric >= METRIC_COUNT || frame < 0
		|| frame >= (int)mFrames[t-1].size() || !mFrames[t-1][frame].mMetricDone[metric]) return false;
	for (int p = 0; p < QPLANES; ++p) values[p] = mFrames[t-1][frame].mMetrics[metric][p];
	return true;
}
unsigned FileScanThread::getScanGeneration() const { SMutex::Autolock lock(mFrmCmpInfoLock); return mScanGeneration; }
unsigned FileScanThread::pairGeneration(int t) const { SMutex::Autolock lock(mFrmCmpInfoLock); return mPairGeneration[t-1]; }
bool FileScanThread::setLazyMetric(int frame, int metric, double value, unsigned gen, int t)
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	if (t < 1 || t > 3 || metric < 0 || metric >= METRIC_COUNT || gen != mPairGeneration[t-1]
		|| frame < 0 || frame >= (int)mFrames[t-1].size() || !mFrames[t-1][frame].mParseDone
		|| !std::isfinite(value)) return false;
	auto& f = mFrames[t-1][frame];
	if (f.mMetricDone[metric]) return true;
	f.mMetrics[metric][0] = value; f.mMetricDone[metric] = true;
	Accumulate(t, frame, metric); return true;
}
PairSequenceSummary FileScanThread::summary(int t) const { SMutex::Autolock lock(mFrmCmpInfoLock); return mSummary[t-1]; }
void FileScanThread::setPairError(int t, const CString& error, int metric)
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	if (metric < 0) mSummary[t-1].error = error;
	else mSummary[t-1].metricError[metric] = error;
}
unsigned FileScanThread::decodeCount(int slot) const { SMutex::Autolock lock(mFrmCmpInfoLock); return mDecodeCounts[slot]; }
void FileScanThread::queueCurrent(std::shared_ptr<const CurrentComparisonRequest> request)
{
	if (!request || !request->id) return;
	SMutex::Autolock lock(mFrmCmpInfoLock);
	for (int t=1;t<4;++t) if (request->targets&(1u<<t)) {
		mLatestRequest[t-1] = request->id; mPending[t-1] = request;
	}
}
bool FileScanThread::copyCurrent(int t, unsigned long long id, PairCurrentResult& result) const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	if (t<1 || t>3 || !id || id!=mLatestRequest[t-1] || mLive[t-1].request != id) return false;
	result = mLive[t-1]; return true;
}
unsigned long long FileScanThread::currentMeasureCount(int t) const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	return t>=1 && t<=3 ? mCurrentMeasureCounts[t-1] : 0;
}
unsigned FileScanThread::pendingCurrentMask() const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	unsigned mask=0;
	for (int t=1;t<4;++t) if (mPending[t-1]) mask|=1u<<t;
	return mask;
}
void FileScanThread::cancelCurrent()
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	for (int i=0;i<3;++i) { mPending[i].reset(); mLatestRequest[i]=0; }
}
bool FileScanThread::hasWork() const
{
	SMutex::Autolock lock(mFrmCmpInfoLock);
	return mPending[0] || mPending[1] || mPending[2] || mLiveRunning || mCurFrameID < mCurFrames;
}
void FileScanThread::ensureRunning() { if (hasWork() && !isRunning()) run(); }
