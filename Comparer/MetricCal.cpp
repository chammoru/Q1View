
#include "stdafx.h"

#include "FrmCmpInfo.h"
#include "MetricCal.h"

#include "QMath.h"

#include "qimage_metrics.h"

MetricCal::MetricCal()
: mMinVal(qmetric_info_table[METRIC_PSNR_IDX].min_val)
, mMaxVal(qmetric_info_table[METRIC_PSNR_IDX].max_val)
, mMinValReal(mMinVal)
, mMaxValReal(mMaxVal)
, mMinValUser(mMinValReal)
, mMaxValUser(mMaxValReal)
, mFrmCmpInfo(NULL)
, mStepCount(0)
, mFrameIdx(0)
, mAvgFont(new Font(&FontFamily(_T("Arial")), 9))
, mMetricIdx(METRIC_PSNR_IDX)
{
	for (int i = 0; i < QPLANES; i++) {
		mLinePens[i] = new Pen(QMetricColors[i]);
		mLinePens[i]->SetDashStyle(DashStyleDash);

		mDotBrushes[i] = new SolidBrush(QMetricColors[i]);
		mAccum[i] = 0.0f;
	}
}

MetricCal::~MetricCal()
{
	for (int i = 0; i < QPLANES; i++) {
		delete mLinePens[i];
		delete mDotBrushes[i];
	}

	delete mAvgFont;
}

const Color MetricCal::QMetricColors[QPLANES] =
{
	Color(0xff, 0xff, 0x66, 0x66),
	Color(0xff, 0x80, 0x80, 0x80),
	Color(0xff, 0x99, 0xcc, 0x66),
};

void MetricCal::DrawFrameID(CDC* pDC, size_t frameID, LONG xPt, int hClient) const
{
	CString xLabel;

	xLabel.Format(_T("%llu"), frameID);
	CRect rect(xPt - X_LABEL_INTERVAL_HF, 0, xPt + X_LABEL_INTERVAL_HF, hClient);
	pDC->DrawText(xLabel, &rect, DT_SINGLELINE | DT_CENTER |  DT_BOTTOM);
}

void MetricCal::DrawOneLine(Graphics *graphics, double ratio, int idx) const
{
	list<double>::const_iterator it = mShowVal[idx].begin();
	double normMetric = *it - getMinVal();

	Point pt1, pt2;
	pt1.X = GRAPH_IN_MARGIN_L + mGraphRect.left;
	pt1.Y = ROUND2I(mGraphRect.bottom - ratio * normMetric);

	graphics->FillEllipse(mDotBrushes[idx],
		pt1.X - DOT_RAD_MINUS_ONE, pt1.Y - DOT_RAD_MINUS_ONE,
		DOT_DIAMETER, DOT_DIAMETER);

	it++;

	for (; it != mShowVal[idx].end(); it++) {
		normMetric = *it - getMinVal();

		pt2.X = mStepW + pt1.X;
		pt2.Y = ROUND2I(mGraphRect.bottom - ratio * normMetric);

		graphics->DrawLine(mLinePens[idx], pt1, pt2);
		graphics->FillEllipse(mDotBrushes[idx],
			pt2.X - DOT_RAD_MINUS_ONE, pt2.Y - DOT_RAD_MINUS_ONE,
			DOT_DIAMETER, DOT_DIAMETER);

		pt1 = pt2;
	}
}

void MetricCal::Setup(CRect *graphRect, int metricIdx)
{
	mGraphRect = *graphRect;

	mStepW = getGraphW() / int(mStepCount + 1);
	mStepW = max(mStepW, MIN_X_AXIS_STEP_W);

	mShowID.clear();
	for (int i = 0; i < QPLANES; i++)
		mShowVal[i].clear();

	if (metricIdx != mMetricIdx) {
		const qmetric_info *qmi = qmetric_info_table + metricIdx;

		mMinVal = qmi->min_val;
		mMaxVal = qmi->max_val;
		mMetricIdx = metricIdx;

		Init();
	}
}

void MetricCal::Init()
{
	const qmetric_info *qmi = qmetric_info_table + mMetricIdx;

	mMaxValReal = qmi->min_val;
	mMinValReal = qmi->max_val;

	mFrameIdx = 0;

	for (int i = 0; i < QPLANES; i++)
		mAccum[i] = 0.0f;
}

void MetricCal::Update(const FrmCmpInfo *frmCmpInfo, size_t stepCount)
{
	mFrmCmpInfo = frmCmpInfo;
	mStepCount = stepCount;

	Init();
}

void MetricCal::CalMinMaxAccum()
{
	size_t i;
	double minVal = mMinValReal;
	double maxVal = mMaxValReal;

	for (i = mFrameIdx; i < mStepCount; i++) {
		const FrmCmpInfo *frmCmpInfo = mFrmCmpInfo + i;

		if (!frmCmpInfo->mParseDone)
			break;

		const double *metrics = frmCmpInfo->mMetrics[mMetricIdx];
		double metric;
		for (int j = 0; j < QPLANES; j++) {
			metric = min(metrics[j], mMaxVal);

			mAccum[j] += metric;
			minVal = min(minVal, metric);
			maxVal = max(maxVal, metric);
		}
	}

	mMaxValReal = maxVal;
	mMinValReal = minVal;

	const double cSameThreadhold = 0.01f;
	if (mMaxValReal - mMinValReal < cSameThreadhold) {
		// to prevent divide by zero
		mMaxValUser = mMaxValReal + cSameThreadhold;
		mMinValUser = mMinValReal - cSameThreadhold;
	} else {
		mMaxValUser = mMaxValReal;
		mMinValUser = mMinValReal;
	}

	mFrameIdx = i;
}

size_t MetricCal::CalculateCoords(CRect *graphRect, int metricIdx)
{
	Setup(graphRect, metricIdx);

	CalMinMaxAccum();

	size_t prevID = -1;
	int graphW = getGraphW();

	for (int i = 0; i < graphW; i += mStepW) {
		size_t frameID = (i * mStepCount) / graphW;

		if (frameID >= mFrameIdx)
			break;

		if (frameID == prevID)
			continue;

		prevID = frameID;

		// record frame ID too
		mShowID.push_back(frameID);

		const FrmCmpInfo *frmCmpInfo = mFrmCmpInfo + frameID;
		const double *metrics = frmCmpInfo->mMetrics[mMetricIdx];
		double metric;
		for (int j = 0; j < QPLANES; j++) {
			metric = min(metrics[j], mMaxVal);
			mShowVal[j].push_back(metric);
		}
	}

	return mShowID.size();
}

void MetricCal::DrawCmpResult(CDC* pDC, CFont *font) const
{
	CString str;

	pDC->SelectObject(font);

	if (mMinValReal == mMaxVal) {
		if (mFrameIdx == mStepCount)
			str.Format(_T("All frames are same"));
		else
			str.Format(_T("Same (until frame %llu)"), mFrameIdx - 1);
	} else {
		str.Format(_T("Different"));
	}

	COLORREF prevColor = pDC->SetTextColor(COLOR_RESULT_TEXT);
	pDC->DrawText(str, CRect(mGraphRect), DT_SINGLELINE | DT_CENTER |  DT_VCENTER);
	pDC->SetTextColor(prevColor);
}

void MetricCal::DrawYLabel(CDC* pDC, CRect *yLabelRect, CFont *font) const
{
	CString str;

	pDC->SelectObject(font);

	str.Format(_T("%.2f"), getMinVal());
	pDC->DrawText(str, yLabelRect, DT_SINGLELINE | DT_CENTER |  DT_BOTTOM);
	str.Format(_T("%.2f"), getMaxVal());
	pDC->DrawText(str, yLabelRect, DT_SINGLELINE | DT_CENTER |  DT_TOP);
	str.Format(_T("%.2f"), getMidVal());
	pDC->DrawText(str, yLabelRect, DT_SINGLELINE | DT_CENTER |  DT_VCENTER);
}

void MetricCal::DrawXLabel(CDC* pDC, int hClient, CFont *font) const
{
	pDC->SelectObject(font);

	list<size_t>::const_iterator it = mShowID.begin();
	LONG x = GRAPH_IN_MARGIN_L + mGraphRect.left;
	int xSum = x;
	if (xSum >= X_LABEL_INTERVAL) {
		DrawFrameID(pDC, *it, x, hClient);
		xSum = 0;
	}
	it++;

	for (; it != mShowID.end(); it++) {
		x += mStepW;
		xSum += mStepW;
		if (xSum >= X_LABEL_INTERVAL) {
			DrawFrameID(pDC, *it, x, hClient);
			xSum = 0;
		}
	}
}

void MetricCal::DrawLines(Graphics *graphics) const
{
	double ratio = mGraphRect.Height() / getVRange();

	for (int i = 0; i < QPLANES; i++)
		DrawOneLine(graphics, ratio, i);
}

void MetricCal::DrawAverages(Graphics *graphics, CRect *rect, const char **labels) const
{
	int hUnit = rect->Height() / QPLANES;
	int wText = rect->Width();
	int hLine = DOT_DIAMETER;
	int hText = hUnit - hLine;

	StringFormat SF;
	SF.SetAlignment(StringAlignmentCenter);
	SF.SetLineAlignment(StringAlignmentCenter);

	int top = rect->top;
	int left = rect->left;
	int lineL = left;
	int lineR = rect->right;
	RectF R(REAL(left), 0, REAL(wText), REAL(hText));

	CString average, label;

	for (int i = 0; i < QPLANES; i++) {
		R.Y = REAL(top);
		label = labels[i];

		average.Format(_T("%s: %.2f"), label, mAccum[i] / mFrameIdx);
		graphics->DrawString(average, -1, mAvgFont, R, &SF, mDotBrushes[i]);

		top += hText;

		int yLine = top + (hLine >> 1);
		graphics->DrawLine(mLinePens[i], lineL, yLine, lineR, yLine);
		DrawDot(graphics, mDotBrushes[i], lineL, yLine);
		DrawDot(graphics, mDotBrushes[i], lineR, yLine);

		top += hLine;
	}
}
