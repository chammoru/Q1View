#pragma once

// CComparatorView view
#include "QMenuItem.h"
#include "QViewerCmn.h"

namespace Gdiplus {
	class Pen;
	class SolidBrush;
}

#define QMENUITEM_IN_MARGIN_H     8

class CComparatorView : public CScrollView
{
protected:
	CComparatorView();           // protected constructor used by dynamic creation
	virtual ~CComparatorView();
	CComparatorDoc* GetDocument() const;

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
	std::vector<CComparatorView *> GetOhterViews(CComparatorDoc *pDoc);
	ComparatorPane *GetPane(CComparatorDoc* pDoc) const;

	DECLARE_MESSAGE_MAP()

public:
	ComparatorPane *mPane;
	int mXDst, mYDst;
	int mWClient, mHClient;
	int mWCanvas, mHCanvas;
	CPoint mPointS;
	bool mIsClicked;
	CRect mRcControls;
	CRect mRcCsQMenu;
	CRect mRcNameQMenu;
	CRect mRcClose;
	bool mCloseHover;
	CMenu mCsMenu;
	CMenu mMouseMenu;   // right-click popup: Sel Mode / Clear Selection
	CQMenuItem mNameQMenu;
	CQMenuItem mCsQMenu;
	bool mProcessing;
	bool mShowHelp;
	int mRgbBufSize;
	BYTE *mRgbBuf;
	CFont mDefPixelTextFont;
	CFont mPixelTextFont;
	LONG  mPixelTextFontHeight;
	CDC mMemDC;
	CBitmap mMemBitmap;
	Gdiplus::Pen*        mDiffCellPen;
	Gdiplus::SolidBrush* mDiffDotBrush;

	void AdjustWindowSize(int numPrevViews, int splitBarChange = 0) const;
	void Initialize(CComparatorDoc *pDoc);
	void DeterminDestOriginCoord(CComparatorDoc *pDoc);
	void CheckCsRadio(int cs);
	void UpdateCsLabel(const TCHAR *csLabel);
	void UpdateFileName(const TCHAR* filename);
	void ScaleRgbBuf(CComparatorDoc *pDoc, BYTE *rgbBuffer, q1::GridInfo &gi);
	void ScaleNearestNeighbor(CComparatorDoc *pDoc, BYTE *src, BYTE *dst, int sDst,
		q1::GridInfo &gi);
	void EnsureNnOffsetBuf(CComparatorDoc *pDoc);
	void EnsurePixelTextFont(float n);
	void DrawHighZoomCells(CComparatorDoc *pDoc, ComparatorPane *pane);
	void DrawEmptyPane(CDC *pDC, CComparatorDoc *pDoc);
	void DrawDiffOverlay(CDC *pDC, CComparatorDoc *pDoc, ComparatorPane *pane);
	void DrawCursorCoord(CDC *pDC, CComparatorDoc *pDoc, ComparatorPane *pane);
	void UpdateCursorCoord(const CPoint &clientPoint);
	void InvalidateCursorCoord(CComparatorDoc *pDoc);
	// Synchronized selection rectangle (issue #74).
	void ClientToSource(const CPoint &clientPoint, int &srcX, int &srcY) const;
	void InvalidateAllPanes(CComparatorDoc *pDoc);
	void ClearSelection(CComparatorDoc *pDoc);
	void DrawSelection(CDC *pDC, CComparatorDoc *pDoc);
	void DrawCloseButton(CDC *pDC, bool paneAvailable);
	void ClosePane();
	void UpdateCloseHover(const CPoint &point);
	void RepaintCloseButton();
	void DrawHelpMenu(CDC *pDC);
	void ToggleHelp();

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnMouseLeave();
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg BOOL OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message);
	afx_msg void OnCsChange(UINT nID);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMenu(UINT nID);
};

#ifndef _DEBUG  // debug version in ComparatorView.cpp
inline CComparatorDoc* CComparatorView::GetDocument() const
	{ return reinterpret_cast<CComparatorDoc*>(m_pDocument); }
#endif
