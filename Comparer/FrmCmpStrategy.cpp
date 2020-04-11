#include "StdAfx.h"
#include "ComparerDoc.h"
#include "FrmCmpStrategy.h"
#include "qimage_metrics.h"
#include "QMath.h"

void IFrmCmpStrategy::CalMetrics(ComparerPane *paneA, ComparerPane *paneB, CString &frmState) const
{
	if (paneA->curFrameID >= paneA->frames || paneB->curFrameID >= paneB->frames) {
		return;
	}
	frmState.Empty();

	double metrics[METRIC_COUNT][QPLANES];
	CalMetricsImpl(paneA, paneB, metrics);
	for (int i = METRIC_PSNR_IDX; i < METRIC_COUNT; i++) {
		double *metric = metrics[i];
		const qmetric_info *qminfo = &qmetric_info_table[i];
		CString metricName = CA2W(qminfo->name);

		frmState.AppendFormat(_T(" %s(%.4f %.4f %.4f)"),
			metricName, metric[0], metric[1], metric[2]);
	}
}

void IFrmCmpStrategy::FlagDiffLine(const list<RLC> *rlc, bool *flags, int n, short rate) const
{
	list<RLC>::const_iterator it;
	double ratio = double(n) / mH;
	int cur = 0;
	for (it = rlc->begin(); it != rlc->end(); it++) {
		long count = (*it).mCount;
		bool isDiff = !(*it).mSame;

		if (isDiff) {
			int from = ROUND2I(cur * ratio * rate);
			int to = ROUND2I((cur + count) * ratio * rate);
			to = min(to, n);

			for (int i = from; i < to; i++)
				flags[i] = true;
		}

		cur += count;
	}
}
