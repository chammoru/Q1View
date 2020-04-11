#pragma once

#include <list>
using namespace std;

#include <gdiplus.h>
using namespace Gdiplus;

#include <QCommon.h>

#include "FrmsInfoView.h"

class FrmCmpInfo;

class MetricCal
{
	static const Color QMetricColors[QPLANES];

	double mMinVal, mMaxVal;
	double mMinValReal, mMaxValReal;
	double mMinValUser, mMaxValUser;
	double mAccum[QPLANES];
	Pen *mLinePens[QPLANES];
	SolidBrush *mDotBrushes[QPLANES];

	const FrmCmpInfo *mFrmCmpInfo;
	size_t mStepCount;
	size_t mFrameIdx;

	list<size_t> mShowID;
	list<double> mShowVal[QPLANES];

	CRect mGraphRect;
	int mStepW;
	Font *mAvgFont;
	int mMetricIdx;

	void DrawFrameID(CDC* pDC, size_t frameID, LONG xPt, int hClient) const;
	inline void DrawDot(Graphics *graphics, Brush *brush, int x, int y) const
	{
		graphics->FillEllipse(brush,
			x - DOT_RAD_MINUS_ONE,
			y - DOT_RAD_MINUS_ONE,
			DOT_DIAMETER, DOT_DIAMETER);
	}
	void DrawOneLine(Graphics *graphics, double ratio, int idx) const;
	void Setup(CRect *graphRect, int metricIdx);
	void CalMinMaxAccum();
	void Init();

public:
	MetricCal();
	~MetricCal();

	void Update(const FrmCmpInfo *frmCmpInfo, size_t stepCount);
	size_t CalculateCoords(CRect *graphRect, int metricIdx);

	void DrawCmpResult(CDC* pDC, CFont *font) const;
	void DrawYLabel(CDC* pDC, CRect *yLabelRect, CFont *font) const;
	void DrawXLabel(CDC* pDC, int hClient, CFont *font) const;
	void DrawLines(Graphics *graphics) const;
	void DrawAverages(Graphics *graphics, CRect *rect, const char **labels) const;

	inline double getMaxVal() const { return mMaxValUser; }
	inline double getMinVal() const { return mMinValUser; }
	inline double getMidVal() const { return (mMaxValUser + mMinValUser) / 2; }
	inline double getVRange() const { return mMaxValUser - mMinValUser; }
	inline int getRealGraphRight() const
	{
		int xLeft = GRAPH_IN_MARGIN_L + mGraphRect.left;
		int wGraph = int(mShowID.size() - 1) * mStepW;
		return xLeft + wGraph;
	}
	inline bool isAllParsed() const { return mFrameIdx == mStepCount; }
	inline int getGraphW() const { return mGraphRect.Width() - GRAPH_IN_MARGIN_L; }
};
