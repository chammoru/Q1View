#pragma once

#include "FrmCmpStrategy.h"
#include "QImageDiff.h"
#include "ComparerPane.h"

class RgbFrmCmpStrategy : public IFrmCmpStrategy
{
	static const char *sRgbPlaneLabels[QPLANES];
	void Rgb888AllocBuffer(SQPane *pane) const;

	inline BYTE *GetRgb888Addr(SQPane *pane) const
	{
		// Even if the current cs is already BGR888,
		// it should be changed because of different alignments
		return ConvertToRgb888(pane, mW, mH);
	}

	void RecordMetrics(BYTE *a, BYTE *b, double metrics[METRIC_COUNT][QPLANES]) const;
	void RecordMetrics(BYTE* a, BYTE* b, std::vector<CString>& scores) const;

public:
	RgbFrmCmpStrategy() : IFrmCmpStrategy(sRgbPlaneLabels) {}
	virtual ~RgbFrmCmpStrategy() {}

	static BYTE *ConvertToRgb888(SQPane *pane, int w, int h);
	virtual void CalMetricsImpl(ComparerPane *paneA, ComparerPane *paneB,
		std::vector<CString>& scores) const;
	virtual void DiffNMetrics(SQPane *paneA, SQPane *paneB,
		double metrics[METRIC_COUNT][QPLANES], list<RLC> rlc[QPLANES]) const;
	virtual void AllocBuffer(SQPane *paneL, SQPane *paneR) const;
	virtual void FlagTotalDiffLine(const list<RLC> rlc[QPLANES], bool *flags, int n) const;
};
