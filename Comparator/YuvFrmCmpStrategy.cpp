#include "StdAfx.h"
#include "ComparatorDoc.h"
#include "YuvFrmCmpStrategy.h"
#include "qimage_metrics.h"

void YuvFrmCmpStrategy::Yuv420AllocBuffer(SQPane *pane) const
{
	if (pane->csc2yuv420 == NULL)
		return; // no need to alloc YUV420 buffer

	int bufOffset2 = 0, bufOffset3 = 0;

	size_t yuv420SceneSize =
		qimage_yuv420_load_info(mW, mH, &bufOffset2, &bufOffset3);

	if (pane->yuv420BufSize < yuv420SceneSize) {
		if (pane->yuv420Buf)
			delete [] pane->yuv420Buf;

		pane->yuv420Buf = new BYTE[yuv420SceneSize];
		pane->yuv420BufSize = yuv420SceneSize;
	}
}

BYTE *YuvFrmCmpStrategy::ConvertToYuv420(SQPane *pane, int w, int h)
{
	int bufOffset2 = 0, bufOffset3 = 0;

	pane->csLoadInfo(w, h, &bufOffset2, &bufOffset3);

	pane->csc2yuv420(pane->yuv420Buf,
		pane->origBuf,
		pane->origBuf + bufOffset2,
		pane->origBuf + bufOffset3, 0, w, h);

	return pane->yuv420Buf;
}

void YuvFrmCmpStrategy::RecordMetrics(BYTE *a, BYTE *b,
									  double metrics[METRIC_COUNT][QPLANES]) const
{
	int bufOffset2 = 0, bufOffset3 = 0;
	qimage_yuv420_load_info(mW, mH, &bufOffset2, &bufOffset3);

	int chroma_w = (mW + 1) >> 1;
	int chroma_h = (mH + 1) >> 1;

	for (int i = METRIC_START_IDX; i < METRIC_COUNT; i++) {
		double *metric = metrics[i];
		const qmetric_info *qminfo = &qmetric_info_table[i];

		if (qminfo->lazy || qminfo->measure == NULL)
			continue;

		metric[0] = qminfo->measure(a, b, mW, mH, mW, 1);
		metric[1] = qminfo->measure(a + bufOffset2, b + bufOffset2, chroma_w, chroma_h, chroma_w, 1);
		metric[2] = qminfo->measure(a + bufOffset3, b + bufOffset3, chroma_w, chroma_h, chroma_w, 1);
	}
}

void YuvFrmCmpStrategy::RecordMetrics(BYTE* a, BYTE* b, int metricIdx,
	std::vector<CString>& scores) const
{
	int bufOffset2 = 0, bufOffset3 = 0;
	qimage_yuv420_load_info(mW, mH, &bufOffset2, &bufOffset3);

	int chroma_w = (mW + 1) >> 1;
	int chroma_h = (mH + 1) >> 1;

	CString score;
	const qmetric_info* qminfo = &qmetric_info_table[metricIdx];
	const CString metricName = CA2W(qminfo->name);

	if (qminfo->lazy || qminfo->measure == NULL)
		return;

	double metric0 = qminfo->measure(a, b, mW, mH, mW, 1);
	double metric1 = qminfo->measure(a + bufOffset2, b + bufOffset2, chroma_w, chroma_h, chroma_w, 1);
	double metric2 = qminfo->measure(a + bufOffset3, b + bufOffset3, chroma_w, chroma_h, chroma_w, 1);
	score.Format(_T("%s(%.4f %.4f %.4f)"), metricName, metric0, metric1, metric2);
	scores.push_back(score);
}

void YuvFrmCmpStrategy::CalMetricsImpl(ComparatorPane *paneA, ComparatorPane *paneB, int metricIdx,
	std::vector<CString> &scores) const
{
	RecordMetrics(GetYuv420Addr(paneA), GetYuv420Addr(paneB), metricIdx, scores);
}

CString YuvFrmCmpStrategy::CropScore(ComparatorPane *paneA, ComparatorPane *paneB, int metricIdx,
									 int l, int t, int r, int b) const
{
	const qmetric_info *qminfo = &qmetric_info_table[metricIdx];
	if (qminfo->lazy || qminfo->measure == NULL)
		return CString();

	int bufOffset2 = 0, bufOffset3 = 0;
	qimage_yuv420_load_info(mW, mH, &bufOffset2, &bufOffset3);
	const int chroma_w = (mW + 1) >> 1;

	BYTE *a = GetYuv420Addr(paneA);
	BYTE *b2 = GetYuv420Addr(paneB);

	// Luma crop [l..r] x [t..b]; the 4:2:0 chroma crop is the overlapping
	// half-resolution region, so the three planes cover the same image area.
	const int yw = r - l + 1;
	const int yh = b - t + 1;
	const int cl = l >> 1, ct = t >> 1, cr = r >> 1, cb = b >> 1;
	const int cw = cr - cl + 1;
	const int chh = cb - ct + 1;

	double mY = qminfo->measure(a + (size_t)t * mW + l,
		b2 + (size_t)t * mW + l, yw, yh, mW, 1);
	double mU = qminfo->measure(a + bufOffset2 + (size_t)ct * chroma_w + cl,
		b2 + bufOffset2 + (size_t)ct * chroma_w + cl, cw, chh, chroma_w, 1);
	double mV = qminfo->measure(a + bufOffset3 + (size_t)ct * chroma_w + cl,
		b2 + bufOffset3 + (size_t)ct * chroma_w + cl, cw, chh, chroma_w, 1);

	const CString metricName = CA2W(qminfo->name);
	CString score;
	score.Format(_T("%s(%.4f %.4f %.4f)"), (const TCHAR*)metricName, mY, mU, mV);
	return score;
}

void YuvFrmCmpStrategy::DiffNMetrics(SQPane *paneA, SQPane *paneB,
									 double metrics[METRIC_COUNT][QPLANES],
									 list<RLC> rlc[QPLANES]) const
{
	BYTE *a, *b;

	a = GetYuv420Addr(paneA);
	b = GetYuv420Addr(paneB);

	int bufOffset2 = 0, bufOffset3 = 0;
	qimage_yuv420_load_info(mW, mH, &bufOffset2, &bufOffset3);

	int chroma_w = (mW + 1) >> 1;
	int chroma_h = (mH + 1) >> 1;

	QCalRLC(a, b, mW, mH, mW, 1, &rlc[0]);
	QCalRLC(a + bufOffset2, b + bufOffset2, chroma_w, chroma_h, chroma_w, 1, &rlc[1]);
	QCalRLC(a + bufOffset3, b + bufOffset3, chroma_w, chroma_h, chroma_w, 1, &rlc[2]);

	RecordMetrics(a, b, metrics);
}

void YuvFrmCmpStrategy::AllocBuffer(SQPane *paneL, SQPane *paneR) const
{
	Yuv420AllocBuffer(paneL);
	Yuv420AllocBuffer(paneR);
}

void YuvFrmCmpStrategy::FlagTotalDiffLine(const list<RLC> rlc[QPLANES], bool *flags, int n) const
{
	for (int i = 0; i < n; i++)
		flags[i] = false;

	FlagDiffLine(&rlc[0], flags, n, 1);
	FlagDiffLine(&rlc[1], flags, n, 2);
	FlagDiffLine(&rlc[2], flags, n, 2);
}

const char *YuvFrmCmpStrategy::sYuvPlaneLabels[QPLANES] =
{
	"Y",
	"U",
	"V",
};
