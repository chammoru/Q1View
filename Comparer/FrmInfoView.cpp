// FrmStateView.cpp : implementation file
//

#include "stdafx.h"
#include "Comparer.h"
#include "ComparerDoc.h"
#include "FrmInfoView.h"
#include "QViewerCmn.h"

// CFrmInfoView

IMPLEMENT_DYNCREATE(CFrmInfoView, CView)

CFrmInfoView::CFrmInfoView()
: mWClient(0)
, mHClient(0)
{
	LOGFONT lf;

	::ZeroMemory(&lf, sizeof(lf));
	lf.lfHeight = FRM_STATE_FONT_H;
	lf.lfWeight = FW_SEMIBOLD;
	::lstrcpy(lf.lfFaceName, Q1UI_FONT_TEXT);

	mStateFont.CreateFontIndirect(&lf);
}

CFrmInfoView::~CFrmInfoView()
{
}

BEGIN_MESSAGE_MAP(CFrmInfoView, CView)
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
END_MESSAGE_MAP()


void CFrmInfoView::OnDraw(CDC* pDC)
{
	CComparerDoc *pDoc = GetDocument();

	CString frameState = pDoc->mFrmState;
	bool hasState = frameState.GetLength() > 0;
	if (!hasState)
		frameState = _T("Ready");

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);

	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(pDC, mWClient, mHClient);

	memDC.SelectObject(bitmap);
	memDC.SetStretchBltMode(COLORONCOLOR);

	memDC.FillSolidRect(CRect(0, 0, mWClient, mHClient), Q1UI_COLOR_SURFACE_ALT);
	memDC.Draw3dRect(CRect(0, 0, mWClient, mHClient), Q1UI_COLOR_BORDER_SOFT, Q1UI_COLOR_BORDER_SOFT);

	memDC.SelectObject(&mStateFont);
	memDC.SetBkMode(TRANSPARENT);
	memDC.SetTextColor(hasState ? Q1UI_COLOR_TEXT : Q1UI_COLOR_TEXT_MUTED);
	memDC.DrawText(frameState, &mRcClient, DT_SINGLELINE | DT_CENTER |  DT_VCENTER);

	pDC->BitBlt(0, 0, mWClient, mHClient, &memDC, 0, 0, SRCCOPY);
}


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


BOOL CFrmInfoView::OnEraseBkgnd(CDC* pDC)
{
	CComparerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return TRUE;

	BOOL ret;

	CString &frameState = pDoc->mFrmState;
	if (frameState.GetLength()) {
		// The state text is painted from an off-screen buffer.
		ret = TRUE;
	} else {
		ret = CView::OnEraseBkgnd(pDC);
	}

	return ret;
}

void CFrmInfoView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	GetClientRect(&mRcClient);

	mWClient = cx;
	mHClient = cy;
}
