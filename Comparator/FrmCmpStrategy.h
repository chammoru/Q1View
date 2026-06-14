#pragma once

#include "QImageDiff.h"
#include "ComparatorPane.h"
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

	void CalMetrics(ComparatorPane *paneA, ComparatorPane *paneB, int metricIdx,
		CString &frmState) const;
	// Per-plane (PSNR/SSIM) metric over a source-pixel crop rectangle [l..r]x[t..b]
	// of the current frame, computed on the same native buffers (YUV planes / RGB)
	// as the whole-image score so the two are directly comparable (issue #74
	// region metrics). Returns "name(values)" or empty for a lazy/ML metric, whose
	// crop is handled separately via the LPIPS engine.
	virtual CString CropScore(ComparatorPane *paneA, ComparatorPane *paneB, int metricIdx,
		int l, int t, int r, int b) const = 0;
	virtual void DiffNMetrics(SQPane *paneA, SQPane *paneB,
		double metrics[METRIC_COUNT][QPLANES], list<RLC> rlc[QPLANES]) const = 0;
	virtual void AllocBuffer(SQPane *paneL, SQPane *paneR) const = 0;
	virtual void FlagTotalDiffLine(const list<RLC> rlc[QPLANES], bool *flags, int n) const = 0;
	virtual void RecordMetrics(BYTE *a, BYTE *b, double metrics[METRIC_COUNT][QPLANES]) const = 0;
	virtual void RecordMetrics(BYTE* a, BYTE* b, int metricIdx,
		std::vector<CString>& scores) const = 0;

protected:
	virtual void CalMetricsImpl(ComparatorPane *paneA, ComparatorPane *paneB, int metricIdx,
		std::vector<CString>& scores) const = 0;
	void FlagDiffLine(const list<RLC> *rlc, bool *flags, int n, short rate) const;
};
