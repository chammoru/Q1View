#pragma once

#include "QImageDiff.h"
#include "ComparerPane.h"
#include "qimage_metrics.h"
#include "QMath.h"

struct IFrmCmpStrategy
{
	int mW, mH;
	const char **mPlaneLabels;
	IFrmCmpStrategy(const char **planeLabels) : mPlaneLabels(planeLabels) {}
	virtual ~IFrmCmpStrategy() {}

	inline void Setup(int w, int h) { mW = w; mH = h; }
	inline const char **GetPlaneLabels() const { return mPlaneLabels; }
	inline bool CheckIdentityWithPsnr(double psnr[QPLANES]) const
	{ return qisinf(psnr[0]) && qisinf(psnr[1]) && qisinf(psnr[2]); }

	void CalMetrics(ComparerPane *paneA, ComparerPane *paneB, CString &frmState) const;
	virtual void DiffNMetrics(SQPane *paneA, SQPane *paneB,
		double metrics[METRIC_COUNT][QPLANES], list<RLC> rlc[QPLANES]) const = 0;
	virtual void AllocBuffer(SQPane *paneL, SQPane *paneR) const = 0;
	virtual void FlagTotalDiffLine(const list<RLC> rlc[QPLANES], bool *flags, int n) const = 0;
	virtual void RecordMetrics(BYTE *a, BYTE *b, double metrics[METRIC_COUNT][QPLANES]) const = 0;
	virtual void RecordMetrics(BYTE* a, BYTE* b, CString scores[METRIC_COUNT]) const = 0;

protected:
	virtual void CalMetricsImpl(ComparerPane *paneA, ComparerPane *paneB,
		CString scores[METRIC_COUNT]) const = 0;
	void FlagDiffLine(const list<RLC> *rlc, bool *flags, int n, short rate) const;
};
