#pragma once

#define POS_LINE_MIN            12
#define POS_NUM_FONT_H          POS_LINE_MIN
#define COLOR_POS_NUM_TEXT      RGB(0xff, 0xff, 0xff)

// CPosInfoView view

class FileScanThread;
struct ComparerPane;

class CPosInfoView : public CScrollView
{
	DECLARE_DYNCREATE(CPosInfoView)

protected:
	CPosInfoView();           // protected constructor used by dynamic creation
	virtual ~CPosInfoView();

public:
#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif
	void ConfigureScrollSizes(CComparerDoc *pDoc);
	void DrawFrameRect(CDC* pDC, CRect *clipBox, int w);
	void DrawEachRect(CDC* pDC,
					ComparerPane *pane,
					CRect *frameRect,
					bool parseDone,
					int &curBrush,
					int &preBrush,
					int i);

private:
	CPen *mDiffPen;
	int mWClient, mHClient;
	int mPosLines;
	int mPosLinesPerFrame;
	CFont mPosNumFont;
	FileScanThread *mFileScanThread;
	size_t mDiffFlagSize;
	bool *mDiffFlags;

	CComparerDoc* GetDocument() const
	{ return reinterpret_cast<CComparerDoc*>(m_pDocument); }

protected:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
	virtual void OnInitialUpdate();     // first time after construct

	DECLARE_MESSAGE_MAP()
public:
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
};


