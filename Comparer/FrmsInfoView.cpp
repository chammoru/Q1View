// FrmsInfoView.cpp : implementation file
//

#include "stdafx.h"

#include "Comparer.h"
#include "ComparerDoc.h"
#include "MainFrm.h"

#include "FrmCmpInfo.h"
#include "FrmsInfoView.h"
#include "FileScanThread.h"
#include "FrmCmpStrategy.h"
#include "MetricCal.h"

#include "QDebug.h"

// CFrmsInfoView

IMPLEMENT_DYNCREATE(CFrmsInfoView, CView)

CFrmsInfoView::CFrmsInfoView()
: mWClient(0)
, mHClient(0)
, mFileScanThread(NULL)
{
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&mGdiplusToken, &gdiplusStartupInput, NULL);

	LOGFONT lf;

	::ZeroMemory(&lf, sizeof(lf));
	lf.lfHeight = LABEL_FONT_H;
	::lstrcpy(lf.lfFaceName, _T("Arial"));

	mLabelFont.CreateFontIndirect(&lf);

	lf.lfHeight = RESULT_FONT_H;
	mResultFont.CreateFontIndirect(&lf);

	// mPsnrCal should be 'new'ed after 'GdiplusStartup()'
	mPsnrCal = new MetricCal();
}

CFrmsInfoView::~CFrmsInfoView()
{
	delete mPsnrCal;

	GdiplusShutdown(mGdiplusToken);
}

BEGIN_MESSAGE_MAP(CFrmsInfoView, CView)
	ON_WM_SIZE()
	ON_WM_CREATE()
	ON_WM_ERASEBKGND()
END_MESSAGE_MAP()


// CFrmsInfoView drawing

void CFrmsInfoView::OnDraw(CDC* pDC)
{
	CComparerDoc* pDoc = GetDocument();
	// TODO: add draw code here

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);

	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(pDC, mWClient, mHClient);

	memDC.SetStretchBltMode(COLORONCOLOR);
	memDC.SelectObject(bitmap);

	memDC.BitBlt(0, 0, mWClient, mHClient, NULL, 0, 0, WHITENESS);

	size_t stepCount = pDoc->mMinFrames;
	if (stepCount <= 0) {
		pDC->BitBlt(0, 0, mWClient, mHClient, &memDC, 0, 0, SRCCOPY);

		return;
	}

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	size_t itemCount = mPsnrCal->CalculateCoords(&mGraphRect, pMainFrm->mMetricIdx);
	if (itemCount <= 0) {
		pDC->BitBlt(0, 0, mWClient, mHClient, &memDC, 0, 0, SRCCOPY);
		return;
	}

	memDC.SetBkMode(TRANSPARENT);

	mPsnrCal->DrawCmpResult(&memDC, &mResultFont);
	mPsnrCal->DrawYLabel(&memDC, &mYLabelRect, &mLabelFont);
	mPsnrCal->DrawXLabel(&memDC, mHClient, &mLabelFont);

	// Use Gdiplus for the sophisticated graph
	Graphics graphics(memDC.m_hDC);
	graphics.SetSmoothingMode(SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

	IFrmCmpStrategy *frmCmpStrategy = pDoc->mFrmCmpStrategy;
	const char **planeLabels = frmCmpStrategy->GetPlaneLabels();

	mPsnrCal->DrawLines(&graphics);
	mPsnrCal->DrawAverages(&graphics, &mAverageRect, planeLabels);

	pDC->BitBlt(0, 0, mWClient, mHClient, &memDC, 0, 0, SRCCOPY);
}


// CFrmsInfoView diagnostics

#ifdef _DEBUG
void CFrmsInfoView::AssertValid() const
{
	CView::AssertValid();
}

#ifndef _WIN32_WCE
void CFrmsInfoView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}
#endif
#endif //_DEBUG


// CFrmsInfoView message handlers

void CFrmsInfoView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here
	GetClientRect(&mGraphRect);
	mGraphRect.DeflateRect(GRAPH_OUT_MARGIN_L,
		GRAPH_OUT_MARGIN_T,
		GRAPH_OUT_MARGIN_R,
		GRAPH_OUT_MARGIN_B);
	mYLabelRect.SetRect(0,
		Y_LABEL_MARGIN_T,
		GRAPH_OUT_MARGIN_L,
		cy - Y_LABEL_MARGIN_B);
	mAverageRect.SetRect(mGraphRect.right + AVG_MARGIN_L,
		AVG_MARGIN_T,
		cx - AVG_MARGIN_R,
		cy - AVG_MARGIN_B);

	mWClient = cx;
	mHClient = cy;
}

int CFrmsInfoView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  Add your specialized creation code here
	CComparerDoc* pDoc = GetDocument();

	pDoc->mFrmsInfoView = this;
	mFileScanThread = pDoc->mFileScanThread;

	return 0;
}

BOOL CFrmsInfoView::OnEraseBkgnd(CDC* pDC)
{
	// TODO: Add your message handler code here and/or call default

	return TRUE;
}
