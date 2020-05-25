#pragma once

// CComparerViewC view
#include "QMenuItem.h"
#include "QViewerCmn.h"

#define QMENUITEM_IN_MARGIN_H     6
#define QMENUITEM_IN_MARGIN_W     8

class CComparerViewC : public CScrollView
{
protected:
	CComparerViewC();           // protected constructor used by dynamic creation
	virtual ~CComparerViewC();
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
	virtual CComparerViewC *GetOppoView(CComparerDoc *pDoc) = 0;
	virtual ComparerPane *GetPane(CComparerDoc* pDoc) const = 0;

	DECLARE_MESSAGE_MAP()

public:
	int mXDst, mYDst;
	int mWClient, mHClient;
	int mWCanvas, mHCanvas;
	CPoint mPointS;
	bool mIsClicked;
	CRect mRcControls;
	CRect mRcCsQMenu;
	CRect mRcBlankStatic;
	CMenu mCsMenu;
	CStatic mBlankStatic;
	CQMenuItem mCsQMenu;
	bool mProcessing;
	int mRgbBufSize;
	BYTE *mRgbBuf;
	CFont mDefPixelTextFont;

	void AdjustWindowSize() const;
	void Initialize(CComparerDoc *pDoc);
	void DeterminDestOriginCoord(CComparerDoc *pDoc);
	void CheckCsRadio(int cs);
	void UpdateCsLabel(const TCHAR *csLabel);
	virtual void ProcessDocument(CComparerDoc *pDoc) = 0;
	void ScaleRgbBuf(CComparerDoc *pDoc, BYTE *rgbBuffer, QGridInfo &gi);
	void ScaleNearestNeighbor(CComparerDoc *pDoc, BYTE *src, BYTE *dst, int sDst,
		QGridInfo &gi);

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
inline CComparerDoc* CComparerViewC::GetDocument() const
	{ return reinterpret_cast<CComparerDoc*>(m_pDocument); }
#endif
