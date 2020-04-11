#pragma once

#define GRAPH_OUT_MARGIN_L  50
#define GRAPH_OUT_MARGIN_T  5
#define GRAPH_OUT_MARGIN_R  60
#define GRAPH_OUT_MARGIN_B  13

#define GRAPH_IN_MARGIN_L   8
#define GRAPH_IN_MARGIN_R   1

#define Y_LABEL_MARGIN_T    (GRAPH_OUT_MARGIN_T - 5)
#define Y_LABEL_MARGIN_B    (GRAPH_OUT_MARGIN_B - 5)

#define MIN_X_AXIS_STEP_W   5
#define LABEL_FONT_H        14
#define RESULT_FONT_H       32
#define COLOR_RESULT_TEXT   RGB(0x33, 0x99, 0xff)

#define X_LABEL_INTERVAL    (GRAPH_OUT_MARGIN_L + GRAPH_IN_MARGIN_L)
#define X_LABEL_INTERVAL_HF (X_LABEL_INTERVAL / 2)

#define DOT_RAD_MINUS_ONE   2
#define DOT_DIAMETER        (DOT_RAD_MINUS_ONE * 2 + 1)

#define AVG_MARGIN_L        (DOT_RAD_MINUS_ONE + 4)
#define AVG_MARGIN_R        (DOT_RAD_MINUS_ONE + 2)
#define AVG_MARGIN_T        2
#define AVG_MARGIN_B        (DOT_RAD_MINUS_ONE + 1)

class CComparerDoc;
class FileScanThread;
class MetricCal;
// CFrmsInfoView view

class CFrmsInfoView : public CView
{
	DECLARE_DYNCREATE(CFrmsInfoView)

	CComparerDoc* GetDocument() const
	{ return reinterpret_cast<CComparerDoc*>(m_pDocument); }

	int mWClient, mHClient;
	CRect mGraphRect, mYLabelRect, mAverageRect;
	FileScanThread *mFileScanThread;
	CFont mLabelFont, mResultFont;
	ULONG_PTR mGdiplusToken;
	MetricCal *mPsnrCal;

protected:
	CFrmsInfoView();           // protected constructor used by dynamic creation
	virtual ~CFrmsInfoView();

public:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif

protected:
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
};


