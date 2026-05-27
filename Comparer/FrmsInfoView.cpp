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
	::lstrcpy(lf.lfFaceName, Q1UI_FONT_TEXT);

	mLabelFont.CreateFontIndirect(&lf);

	lf.lfHeight = RESULT_FONT_H;
	mResultFont.CreateFontIndirect(&lf);

	// MetricCal owns GDI+ objects, so create it after GdiplusStartup().
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
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()


static void DrawCenteredMessage(CDC *pDC, CRect rect, CFont *titleFont, CFont *bodyFont,
								const CString &title, const CString &body)
{
	CRect titleRect = rect;
	titleRect.bottom = rect.CenterPoint().y - 2;
	CRect bodyRect = rect;
	bodyRect.top = rect.CenterPoint().y + 5;

	pDC->SetBkMode(TRANSPARENT);
	CFont *prevFont = pDC->SelectObject(titleFont);
	pDC->SetTextColor(Q1UI_COLOR_TEXT);
	pDC->DrawText(title, &titleRect, DT_SINGLELINE | DT_CENTER | DT_BOTTOM | DT_END_ELLIPSIS);
	pDC->SelectObject(bodyFont);
	pDC->SetTextColor(Q1UI_COLOR_TEXT_MUTED);
	pDC->DrawText(body, &bodyRect, DT_SINGLELINE | DT_CENTER | DT_TOP | DT_END_ELLIPSIS);
	pDC->SelectObject(prevFont);
}

void CFrmsInfoView::OnDraw(CDC* pDC)
{
	CComparerDoc* pDoc = GetDocument();

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);

	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(pDC, mWClient, mHClient);

	memDC.SetStretchBltMode(COLORONCOLOR);
	memDC.SelectObject(bitmap);

	memDC.FillSolidRect(CRect(0, 0, mWClient, mHClient), Q1UI_COLOR_SURFACE);
	memDC.Draw3dRect(CRect(0, 0, mWClient, mHClient), Q1UI_COLOR_BORDER_SOFT, Q1UI_COLOR_BORDER_SOFT);

	size_t stepCount = pDoc->mMinFrames;
	if (stepCount <= 0) {
		DrawCenteredMessage(&memDC, CRect(0, 0, mWClient, mHClient),
			&mResultFont, &mLabelFont, _T("Metrics"),
			_T("Open comparable sources to calculate PSNR or SSIM"));
		pDC->BitBlt(0, 0, mWClient, mHClient, &memDC, 0, 0, SRCCOPY);

		return;
	}

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	size_t itemCount = mPsnrCal->CalculateCoords(&mGraphRect, pMainFrm->mMetricIdx);
	if (itemCount <= 0) {
		DrawCenteredMessage(&memDC, CRect(0, 0, mWClient, mHClient),
			&mResultFont, &mLabelFont, _T("Scanning"),
			_T("Metric points will appear as frames are parsed"));
		pDC->BitBlt(0, 0, mWClient, mHClient, &memDC, 0, 0, SRCCOPY);
		return;
	}

	memDC.SetBkMode(TRANSPARENT);

	mPsnrCal->DrawCmpResult(&memDC, &mResultFont);
	mPsnrCal->DrawYLabel(&memDC, &mYLabelRect, &mLabelFont);
	mPsnrCal->DrawXLabel(&memDC, mHClient, &mLabelFont);

	// GDI+ gives the metric graph anti-aliased lines and labels.
	Graphics graphics(memDC.m_hDC);
	graphics.SetSmoothingMode(SmoothingModeAntiAlias);
	graphics.SetTextRenderingHint(TextRenderingHintAntiAlias);

	IFrmCmpStrategy *frmCmpStrategy = pDoc->mFrmCmpStrategy;
	const char **planeLabels = frmCmpStrategy->GetPlaneLabels();

	mPsnrCal->DrawLines(&graphics);
	mPsnrCal->DrawAverages(&graphics, &mAverageRect, planeLabels);

	pDC->BitBlt(0, 0, mWClient, mHClient, &memDC, 0, 0, SRCCOPY);
}


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


void CFrmsInfoView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

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

	CComparerDoc* pDoc = GetDocument();

	pDoc->mFrmsInfoView = this;
	mFileScanThread = pDoc->mFileScanThread;

	return 0;
}

BOOL CFrmsInfoView::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

BOOL CFrmsInfoView::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint pt)
{
	// pt is in screen coordinates; convert to client coordinates so we can
	// check the cursor against the graph area.
	CPoint client = pt;
	ScreenToClient(&client);
	if (!mGraphRect.PtInRect(client))
		return FALSE;

	int graphX = client.x - mGraphRect.left - GRAPH_IN_MARGIN_L;
	mPsnrCal->ZoomAtX(zDelta, graphX);
	Invalidate(FALSE);
	return TRUE;
}

void CFrmsInfoView::OnLButtonDblClk(UINT /*nFlags*/, CPoint point)
{
	// Double-click anywhere on the graph restores the full-range view.
	if (mGraphRect.PtInRect(point)) {
		mPsnrCal->ResetView();
		Invalidate(FALSE);
	}
}
