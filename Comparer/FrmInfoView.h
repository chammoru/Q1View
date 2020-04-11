#pragma once
#include "afxwin.h"

#define FRM_STATE_FONT_H      16
// CFrmInfoView view

class CFrmInfoView : public CView
{
	DECLARE_DYNCREATE(CFrmInfoView)

protected:
	CFrmInfoView();           // protected constructor used by dynamic creation
	virtual ~CFrmInfoView();

public:
	virtual void OnDraw(CDC* pDC);      // overridden to draw this view
#ifdef _DEBUG
	virtual void AssertValid() const;
#ifndef _WIN32_WCE
	virtual void Dump(CDumpContext& dc) const;
#endif
#endif

private:
	CComparerDoc* GetDocument() const
	{ return reinterpret_cast<CComparerDoc*>(m_pDocument); }

protected:
	DECLARE_MESSAGE_MAP()

public:
	CFont mStateFont;
	int mWClient, mHClient;
	CRect mRcClient;

	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
};


