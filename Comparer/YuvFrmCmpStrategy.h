#pragma once

#include "FrmCmpStrategy.h"
#include "QImageDiff.h"
#include "ComparerPane.h"

class YuvFrmCmpStrategy : public IFrmCmpStrategy
{
	static const char *sYuvPlaneLabels[QPLANES];
	void Yuv420AllocBuffer(SQPane *pane) const;

	inline BYTE *GetYuv420Addr(SQPane *pane) const
	{
		BYTE *buf;

		if (pane->colorSpace == QIMAGE_CS_YUV420)
			buf = pane->origBuf;
		else
			buf = ConvertToYuv420(pane, mW, mH);

		return buf;
	}

	void RecordMetrics(BYTE *a, BYTE *b, double metrics[METRIC_COUNT][QPLANES]) const;

public:
	YuvFrmCmpStrategy() : IFrmCmpStrategy(sYuvPlaneLabels) {}
	virtual ~YuvFrmCmpStrategy() {}

	static BYTE *ConvertToYuv420(SQPane *pane, int w, int h);
	virtual void CalMetricsImpl(ComparerPane *paneA, ComparerPane *paneB,
		double metrics[METRIC_COUNT][QPLANES]) const;
	virtual void DiffNMetrics(SQPane *paneA, SQPane *paneB,
		double metrics[METRIC_COUNT][QPLANES], list<RLC> rlc[QPLANES]) const;
	virtual void AllocBuffer(SQPane *paneL, SQPane *paneR) const;
	virtual void FlagTotalDiffLine(const list<RLC> rlc[QPLANES], bool *flags, int n) const;
};
