#include "StdAfx.h"
#include "ComparerDoc.h"
#include "RgbFrmCmpStrategy.h"
#include "qimage_metrics.h"

void RgbFrmCmpStrategy::Rgb888AllocBuffer(SQPane *pane) const
{
	int stride = ROUNDUP_DWORD(mW);
	size_t rgbBufSize = stride * mH * QIMG_DST_RGB_BYTES;
	if (rgbBufSize > pane->rgbBufSize) {
		pane->rgbBufSize = rgbBufSize;

		if (pane->rgbBuf)
			delete [] pane->rgbBuf;

		pane->rgbBuf = new BYTE[rgbBufSize];
	}
}

BYTE *RgbFrmCmpStrategy::ConvertToRgb888(SQPane *pane, int w, int h)
{
	int bufOffset2 = 0, bufOffset3 = 0;

	pane->csLoadInfo(w, h, &bufOffset2, &bufOffset3);

	pane->csc2rgb888(pane->rgbBuf,
		pane->origBuf,
		pane->origBuf + bufOffset2,
		pane->origBuf + bufOffset3, ROUNDUP_DWORD(w), w, h);

	return pane->rgbBuf;
}

void RgbFrmCmpStrategy::RecordMetrics(BYTE *a, BYTE *b,
									  double metrics[METRIC_COUNT][QPLANES]) const
{
	int stride = ROUNDUP_DWORD(mW);

	for (int i = METRIC_PSNR_IDX; i < METRIC_COUNT; i++) {
		double *metric = metrics[i];
		const qmetric_info *qminfo = &qmetric_info_table[i];

		metric[0] = qminfo->measure(a, b, mW, mH, stride, 3);
		metric[1] = qminfo->measure(a + 1, b + 1, mW, mH, stride, 3);
		metric[2] = qminfo->measure(a + 2, b + 2, mW, mH, stride, 3);
	}
}

void RgbFrmCmpStrategy::CalMetricsImpl(ComparerPane *paneA, ComparerPane *paneB,
									   double metrics[METRIC_COUNT][QPLANES]) const
{
	RecordMetrics(paneA->rgbBuf, paneB->rgbBuf, metrics);
}

void RgbFrmCmpStrategy::DiffNMetrics(SQPane *paneA, SQPane *paneB,
									 double metrics[METRIC_COUNT][QPLANES],
									 list<RLC> rlc[QPLANES]) const
{
	int stride = ROUNDUP_DWORD(mW);

	BYTE *a = GetRgb888Addr(paneA);
	BYTE *b = GetRgb888Addr(paneB);

	QCalRLC(a, b, mW, mH, stride, 3, &rlc[0]);
	QCalRLC(a + 1, b + 1, mW, mH, stride, 3, &rlc[1]);
	QCalRLC(a + 2, b + 2, mW, mH, stride, 3, &rlc[2]);

	RecordMetrics(a, b, metrics);
}

void RgbFrmCmpStrategy::AllocBuffer(SQPane *paneL, SQPane *paneR) const
{
	Rgb888AllocBuffer(paneL);
	Rgb888AllocBuffer(paneR);
}

void RgbFrmCmpStrategy::FlagTotalDiffLine(const list<RLC> rlc[QPLANES], bool *flags, int n) const
{
	for (int i = 0; i < n; i++)
		flags[i] = false;

	FlagDiffLine(&rlc[0], flags, n, 1);
	FlagDiffLine(&rlc[1], flags, n, 1);
	FlagDiffLine(&rlc[2], flags, n, 1);
}

const char *RgbFrmCmpStrategy::sRgbPlaneLabels[QPLANES] =
{
	"R",
	"G",
	"B",
};
