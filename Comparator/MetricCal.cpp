
#include "stdafx.h"

#include "FileScanThread.h"
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
, mFileScanThread(NULL)
, mStepCount(0)
, mFrameIdx(0)
, mViewStartFrame(0)
, mViewEndFrame(0)
, mAvgFont(new Font(&FontFamily(Q1UI_FONT_TEXT), 9))
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
	Color(0xff, 0x25, 0x66, 0xd9),
	Color(0xff, 0xdc, 0x26, 0x26),
	Color(0xff, 0x16, 0x9b, 0x62),
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

	// Clamp the view range to the current parsed run. Zoom may have left the
	// range outside the new bounds after a fresh load or scan progress.
	if (mViewEndFrame == 0 || mViewEndFrame > mStepCount)
		mViewEndFrame = mStepCount;
	if (mViewStartFrame >= mViewEndFrame)
		mViewStartFrame = 0;

	size_t viewSpan = mViewEndFrame > mViewStartFrame
		? mViewEndFrame - mViewStartFrame : 1;
	mStepW = getGraphW() / int(viewSpan + 1);
	mStepW = max(mStepW, MIN_X_AXIS_STEP_W);

	mShowID.clear();
	// Clear every plane, not just the current metric's plane_count: switching
	// from a 3-plane metric to a 1-plane one (or back) must not leave stale lines.
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

	for (int i = 0; i < qmi->plane_count; i++)
		mAccum[i] = 0.0f;
}

void MetricCal::Update(const FileScanThread *fileScanThread, size_t stepCount)
{
	mFileScanThread = fileScanThread;
	mStepCount = stepCount;

	ResetView();
	Init();
}

void MetricCal::ResetView()
{
	mViewStartFrame = 0;
	mViewEndFrame = mStepCount;
}

void MetricCal::ZoomAtX(short zDelta, int graphX)
{
	if (mStepCount == 0)
		return;

	int graphW = getGraphW();
	if (graphW <= 0)
		return;

	if (graphX < 0) graphX = 0;
	if (graphX > graphW) graphX = graphW;

	// Anchor: the frame currently sitting under the cursor pixel.
	double span = double(mViewEndFrame - mViewStartFrame);
	if (span <= 0.0)
		span = double(mStepCount);
	double cursorFrame = double(mViewStartFrame) + (double(graphX) / graphW) * span;

	// One wheel notch (WHEEL_DELTA == 120) zooms by ~25%.
	const double kStep = 1.25;
	double notches = double(zDelta) / 120.0;
	double factor = pow(kStep, -notches); // zDelta > 0 -> zoom in -> smaller span
	double newSpan = span * factor;

	// Show at least 10 frames; don't zoom past the full parsed range.
	const double kMinSpan = 10.0;
	if (newSpan < kMinSpan)
		newSpan = kMinSpan;
	if (newSpan > double(mStepCount))
		newSpan = double(mStepCount);

	double newStart = cursorFrame - (double(graphX) / graphW) * newSpan;
	double newEnd = newStart + newSpan;
	if (newStart < 0.0) { newEnd -= newStart; newStart = 0.0; }
	if (newEnd > double(mStepCount)) {
		newStart -= newEnd - double(mStepCount);
		newEnd = double(mStepCount);
		if (newStart < 0.0) newStart = 0.0;
	}

	mViewStartFrame = static_cast<size_t>(newStart);
	mViewEndFrame   = static_cast<size_t>(newEnd);
	if (mViewEndFrame <= mViewStartFrame)
		mViewEndFrame = mViewStartFrame + 1;
}

long MetricCal::FrameAtX(int graphX) const
{
	if (mStepCount == 0 || mFrameIdx == 0)
		return -1;
	int graphW = getGraphW();
	if (graphW <= 0)
		return -1;

	if (graphX < 0)       graphX = 0;
	if (graphX > graphW)  graphX = graphW;

	size_t viewSpan = mViewEndFrame > mViewStartFrame
		? mViewEndFrame - mViewStartFrame : 1;
	long frameID = long(mViewStartFrame)
		+ long((size_t(graphX) * viewSpan) / size_t(graphW));

	// Clamp to frames that actually have parsed metrics so far.
	long maxFrame = long(mFrameIdx) - 1;
	if (frameID > maxFrame) frameID = maxFrame;
	if (frameID < 0)        frameID = 0;
	return frameID;
}

void MetricCal::CalMinMaxAccum()
{
	size_t i;
	double minVal = mMinValReal;
	double maxVal = mMaxValReal;

	for (i = mFrameIdx; i < mStepCount; i++) {
		double metrics[QPLANES];

		if (!mFileScanThread->copyMetrics(int(i), mMetricIdx, metrics))
			break;

		const qmetric_info *qmi = qmetric_info_table + mMetricIdx;
		double metric;
		for (int j = 0; j < qmi->plane_count; j++) {
			metric = qmi->clamp_to_max ? min(metrics[j], mMaxVal) : metrics[j];

			mAccum[j] += metric;
			minVal = min(minVal, metric);
			maxVal = max(maxVal, metric);
		}
	}

	mMaxValReal = maxVal;
	mMinValReal = minVal;

	const qmetric_info *qmi = qmetric_info_table + mMetricIdx;
	if (mMaxValReal - mMinValReal < qmi->same_epsilon) {
		// to prevent divide by zero
		mMaxValUser = mMaxValReal + qmi->same_epsilon;
		mMinValUser = mMinValReal - qmi->same_epsilon;
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
	size_t viewSpan = mViewEndFrame > mViewStartFrame
		? mViewEndFrame - mViewStartFrame : 1;

	for (int i = 0; i < graphW; i += mStepW) {
		size_t frameID = mViewStartFrame + (size_t(i) * viewSpan) / graphW;

		if (frameID >= mFrameIdx)
			break;

		if (frameID == prevID)
			continue;

		prevID = frameID;

		// record frame ID too
		mShowID.push_back(frameID);

		double metrics[QPLANES];
		if (!mFileScanThread->copyMetrics(int(frameID), mMetricIdx, metrics))
			break;

		const qmetric_info *qmi = qmetric_info_table + mMetricIdx;
		double metric;
		for (int j = 0; j < qmi->plane_count; j++) {
			metric = qmi->clamp_to_max ? min(metrics[j], mMaxVal) : metrics[j];
			mShowVal[j].push_back(metric);
		}
	}

	return mShowID.size();
}

void MetricCal::DrawCmpResult(CDC* pDC, CFont *font) const
{
	CString str;

	pDC->SelectObject(font);

	// "All same" means the worst-case frame is still at the identity value:
	// for higher-is-better metrics that is the minimum, for lower-is-better
	// (LPIPS) the maximum. same_epsilon absorbs tiny non-zero distances.
	const qmetric_info *qmi = qmetric_info_table + mMetricIdx;
	double sameProbe = qmi->higher_is_better ? mMinValReal : mMaxValReal;
	bool allSame = qmi->higher_is_better
		? (sameProbe >= qmi->same_value - qmi->same_epsilon)
		: (sameProbe <= qmi->same_value + qmi->same_epsilon);
	if (allSame) {
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
	pDC->SetTextColor(Q1UI_COLOR_TEXT_MUTED);

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
	pDC->SetTextColor(Q1UI_COLOR_TEXT_MUTED);

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

	const qmetric_info *qmi = qmetric_info_table + mMetricIdx;
	for (int i = 0; i < qmi->plane_count; i++)
		DrawOneLine(graphics, ratio, i);
}

void MetricCal::DrawAverages(Graphics *graphics, CRect *rect, const char **labels) const
{
	const qmetric_info *qmi = qmetric_info_table + mMetricIdx;
	int hUnit = rect->Height() / qmi->plane_count;
	int wText = rect->Width();
	int hLine = DOT_DIAMETER;
	int hText = hUnit - hLine;

	StringFormat SF;
	SF.SetAlignment(StringAlignmentCenter);
	SF.SetLineAlignment(StringAlignmentCenter);
	SF.SetFormatFlags(StringFormatFlagsNoWrap);
	SF.SetTrimming(StringTrimmingEllipsisCharacter);

	int top = rect->top;
	int left = rect->left;
	int lineL = left;
	int lineR = rect->right;
	RectF R(REAL(left), 0, REAL(wText), REAL(hText));

	CString average, label;

	for (int i = 0; i < qmi->plane_count; i++) {
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
