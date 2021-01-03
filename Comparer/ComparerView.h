#pragma once

// CComparerView view
#include "QMenuItem.h"
#include "QViewerCmn.h"

#define QMENUITEM_IN_MARGIN_H     6

class CComparerView : public CScrollView
{
protected:
	CComparerView();           // protected constructor used by dynamic creation
	virtual ~CComparerView();
	CComparerDoc* GetDocument() const;

public:
#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif

protected:
	virtual void OnDraw(CDC *pDC);      // overridden to draw this view
	virtual void OnInitialUpdate();     // first time after construct
	std::vector<CComparerView *> GetOhterViews(CComparerDoc *pDoc);
	ComparerPane *GetPane(CComparerDoc* pDoc) const;

	DECLARE_MESSAGE_MAP()

public:
	ComparerPane *mPane;
	int mXDst, mYDst;
	int mWClient, mHClient;
	int mWCanvas, mHCanvas;
	CPoint mPointS;
	bool mIsClicked;
	CRect mRcControls;
	CRect mRcCsQMenu;
	CRect mRcNameQMenu;
	CMenu mCsMenu;
	CQMenuItem mNameQMenu;
	CQMenuItem mCsQMenu;
	bool mProcessing;
	int mRgbBufSize;
	BYTE *mRgbBuf;
	CFont mDefPixelTextFont;

	void AdjustWindowSize(int numPrevViews, int splitBarChange = 0) const;
	void Initialize(CComparerDoc *pDoc);
	void DeterminDestOriginCoord(CComparerDoc *pDoc);
	void CheckCsRadio(int cs);
	void UpdateCsLabel(const TCHAR *csLabel);
	void UpdateFileName(const TCHAR* filename);
	void ScaleRgbBuf(CComparerDoc *pDoc, BYTE *rgbBuffer, q1::GridInfo &gi);
	void ScaleNearestNeighbor(CComparerDoc *pDoc, BYTE *src, BYTE *dst, int sDst,
		q1::GridInfo &gi);

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnCsChange(UINT nID);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
};

#ifndef _DEBUG  // debug version in ComparerView.cpp
inline CComparerDoc* CComparerView::GetDocument() const
	{ return reinterpret_cast<CComparerDoc*>(m_pDocument); }
#endif
