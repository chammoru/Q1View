// FrmStateView.cpp : implementation file
//

#include "stdafx.h"
#include "Comparer.h"
#include "ComparerDoc.h"
#include "FrmInfoView.h"

// CFrmInfoView

IMPLEMENT_DYNCREATE(CFrmInfoView, CView)

CFrmInfoView::CFrmInfoView()
: mWClient(0)
, mHClient(0)
{
	LOGFONT lf;

	::ZeroMemory(&lf, sizeof(lf));
	lf.lfHeight = FRM_STATE_FONT_H;
	::lstrcpy(lf.lfFaceName, _T("Arial"));

	mStateFont.CreateFontIndirect(&lf);
}

CFrmInfoView::~CFrmInfoView()
{
}

BEGIN_MESSAGE_MAP(CFrmInfoView, CView)
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
END_MESSAGE_MAP()


// CFrmInfoView drawing

void CFrmInfoView::OnDraw(CDC* pDC)
{
	CComparerDoc *pDoc = GetDocument();
	// TODO: add draw code here

	CString &frameState = pDoc->mFrmState;
	if (frameState.GetLength() <= 0)
		return;

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);

	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(pDC, mWClient, mHClient);

	memDC.SelectObject(bitmap);
	memDC.SetStretchBltMode(COLORONCOLOR);

	memDC.BitBlt(0, 0, mWClient, mHClient, NULL, 0, 0, WHITENESS);

	memDC.SelectObject(&mStateFont);
	memDC.SetBkMode(TRANSPARENT);
	memDC.DrawText(frameState, &mRcClient, DT_SINGLELINE | DT_CENTER |  DT_VCENTER);

	pDC->BitBlt(0, 0, mWClient, mHClient, &memDC, 0, 0, SRCCOPY);
}


// CFrmInfoView diagnostics

#ifdef _DEBUG
void CFrmInfoView::AssertValid() const
{
	CView::AssertValid();
}

#ifndef _WIN32_WCE
void CFrmInfoView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif
#endif //_DEBUG


// CFrmInfoView message handlers

BOOL CFrmInfoView::OnEraseBkgnd(CDC* pDC)
{
	// TODO: Add your message handler code here and/or call default
	CComparerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return TRUE;

	BOOL ret;

	CString &frameState = pDoc->mFrmState;
	if (frameState.GetLength()) {
		// prevent flickering
		ret = TRUE;
	} else {
		ret = CView::OnEraseBkgnd(pDC);
	}

	return ret;
}

void CFrmInfoView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here
	GetClientRect(&mRcClient);

	mWClient = cx;
	mHClient = cy;
}
