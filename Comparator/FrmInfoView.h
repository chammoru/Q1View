#pragma once
#include "afxwin.h"
#include "AnalysisScrollView.h"

#define FRM_STATE_FONT_H      16
// CFrmInfoView view

class CFrmInfoView : public CAnalysisScrollView
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
	CComparatorDoc* GetDocument() const
	{ return reinterpret_cast<CComparatorDoc*>(m_pDocument); }

protected:
	DECLARE_MESSAGE_MAP()

public:
	CFont mStateFont;
	int mWClient, mHClient;
	CRect mRcClient;
	CReferenceLabel mReferenceLabel;
	CComparisonButton mPairButtons[3];
	CRect SourceColumnBounds(int slot) const;
	BOOL OnCommand(WPARAM wParam, LPARAM lParam) override;
	BOOL PreTranslateMessage(MSG* message) override;

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg void OnSize(UINT nType, int cx, int cy);
};


