// PosInfoView.cpp : implementation file
//

#include "stdafx.h"
#include "Comparer.h"
#include "ComparerDoc.h"
#include "PosInfoView.h"
#include "FrmCmpInfo.h"
#include "FrmCmpStrategy.h"
#include "FileScanThread.h"
#include "MainFrm.h"
#include "QViewerCmn.h"

// CPosInfoView

IMPLEMENT_DYNCREATE(CPosInfoView, CScrollView)

CPosInfoView::CPosInfoView()
: mDiffPen(new CPen(PS_SOLID, 1, Q1UI_COLOR_DANGER))
, mWClient(0)
, mHClient(0)
, mPosLines(0)
, mPosLinesPerFrame(0)
, mFileScanThread(NULL)
, mDiffFlagSize(1)
, mDiffFlags(new bool[mDiffFlagSize])
{
	LOGFONT lf;

	::ZeroMemory(&lf, sizeof(lf));
	lf.lfHeight = POS_NUM_FONT_H;
	lf.lfWeight = FW_SEMIBOLD;
	::lstrcpy(lf.lfFaceName, Q1UI_FONT_TEXT);

	mPosNumFont.CreateFontIndirect(&lf);
}

CPosInfoView::~CPosInfoView()
{
	delete mDiffPen;
	delete [] mDiffFlags;
}


BEGIN_MESSAGE_MAP(CPosInfoView, CScrollView)
	ON_WM_ERASEBKGND()
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_MOUSEWHEEL()
	ON_WM_VSCROLL()
END_MESSAGE_MAP()


void CPosInfoView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();
}

static void DrawDiffPosLines(CDC *pDC, CRect *frameRect, bool *flags, int n)
{
	int top = frameRect->top + 1;
	int left = frameRect->left + 1;
	int right = frameRect->right - 1;

	for (int i = 0; i < n; i++) {
		if (flags[i] == false)
			continue;

		pDC->MoveTo(left, top + i);
		pDC->LineTo(right, top + i);
	}
}

void CPosInfoView::DrawEachRect(CDC* pDC,
								ComparerPane *pane,
								CRect *frameRect,
								bool parseDone,
								int &curBrush,
								int &preBrush,
								int i)
{
	CComparerDoc* pDoc = GetDocument();

	COLORREF frameColor;
	if (!pane->isAvail()) {
		frameColor = Q1UI_COLOR_SURFACE;
	} else if (parseDone) {
		frameColor = Q1UI_COLOR_ACCENT_SOFT;
	} else if (i < pane->frames) {
		frameColor = Q1UI_COLOR_BORDER_SOFT;
	} else {
		frameColor = Q1UI_COLOR_APP_BG;
	}

	pDC->FillSolidRect(frameRect, frameColor);
	pDC->Draw3dRect(frameRect, Q1UI_COLOR_SURFACE, Q1UI_COLOR_SURFACE);

	if (parseDone) {
		CPen *prev = pDC->SelectObject(mDiffPen);
		IFrmCmpStrategy *frmCmpStrategy = pDoc->mFrmCmpStrategy;
		list<RLC> diffRLC[QPLANES];

		if (mFileScanThread->copyDiffRLC(i, diffRLC)) {
			frmCmpStrategy->FlagTotalDiffLine(diffRLC, mDiffFlags, mPosLinesPerFrame);
			DrawDiffPosLines(pDC, frameRect, mDiffFlags, mPosLinesPerFrame);
		}
		pDC->SelectObject(prev);
	}

	const COLORREF curIdColor = Q1UI_COLOR_ACCENT;
	COLORREF preColor;
	bool isCurFrame = i == pane->curFrameID;

	if (isCurFrame)
		preColor = pDC->SetTextColor(curIdColor);

	CString numStr;
	numStr.Format(_T("%d"), i);
	pDC->DrawText(numStr, frameRect, DT_SINGLELINE | DT_CENTER |  DT_VCENTER);

	if (isCurFrame)
		pDC->SetTextColor(preColor);

	frameRect->OffsetRect(0, mPosLinesPerFrame + 1);
}

void CPosInfoView::DrawFrameRect(CDC* pDC, CRect *clipBox, int w)
{
	CComparerDoc* pDoc = GetDocument();

	ComparerPane *paneL = &pDoc->mPane[CComparerDoc::IMG_VIEW_1];
	ComparerPane *paneR = &pDoc->mPane[CComparerDoc::IMG_VIEW_2];

	int outerRcH = mPosLinesPerFrame + 1;
	int startFrameID = clipBox->top / outerRcH;
	int endFrameID = MIN(clipBox->bottom / outerRcH, pDoc->mMaxFrames - 1);

	int y = outerRcH * startFrameID - clipBox->top;
	CRect frameRectL(0, y, w / 2, y + outerRcH + 1);
	CRect frameRectR(w / 2, y, w, y + outerRcH + 1);

	pDC->SelectStockObject(WHITE_PEN);
	pDC->SetBkMode(TRANSPARENT);
	pDC->SelectObject(&mPosNumFont);
	pDC->SetTextColor(COLOR_POS_NUM_TEXT);

	int curBrush;
	int preBrush = -1;

	int minFrames = pDoc->mMinFrames;

	for (int i = startFrameID; i <= endFrameID; i++) {
		bool parseDone = i < minFrames && mFileScanThread->isFrameParsed(i);

		DrawEachRect(pDC, paneL, &frameRectL, parseDone, curBrush, preBrush, i);
		DrawEachRect(pDC, paneR, &frameRectR, parseDone, curBrush, preBrush, i);
	}
}

void CPosInfoView::OnDraw(CDC* pDC)
{
	CComparerDoc* pDoc = GetDocument();

	CRect clipBox;
	pDC->GetClipBox(&clipBox);

	int h = clipBox.Height();

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);

	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(pDC, mWClient, h);

	memDC.SetStretchBltMode(COLORONCOLOR);
	memDC.SelectObject(bitmap);

	ComparerPane *paneL = &pDoc->mPane[CComparerDoc::IMG_VIEW_1];
	ComparerPane *paneR = &pDoc->mPane[CComparerDoc::IMG_VIEW_2];

	memDC.FillSolidRect(CRect(0, 0, mWClient, h), Q1UI_COLOR_APP_BG);

	if (!paneL->isAvail() && !paneR->isAvail()) {
		LOGFONT lf;
		mPosNumFont.GetLogFont(&lf);
		::lstrcpy(lf.lfFaceName, Q1UI_FONT_TEXT);
		lf.lfHeight = 14;
		lf.lfWeight = FW_SEMIBOLD;
		CFont labelFont;
		labelFont.CreateFontIndirect(&lf);
		CFont *prevFont = memDC.SelectObject(&labelFont);
		memDC.SetBkMode(TRANSPARENT);
		memDC.SetTextColor(Q1UI_COLOR_TEXT_MUTED);
		CRect msgRect(0, 0, mWClient, h);
		memDC.DrawText(_T("Timeline"), &msgRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
		memDC.SelectObject(prevFont);
		goto OnDrawExit;
	}

	DrawFrameRect(&memDC, &clipBox, mWClient);

OnDrawExit:
	pDC->BitBlt(0, clipBox.top, mWClient, h, &memDC, 0, 0, SRCCOPY);
}


#ifdef _DEBUG
void CPosInfoView::AssertValid() const
{
	CScrollView::AssertValid();
}

#ifndef _WIN32_WCE
void CPosInfoView::Dump(CDumpContext& dc) const
{
	CScrollView::Dump(dc);
}
#endif
#endif //_DEBUG


BOOL CPosInfoView::OnEraseBkgnd(CDC* pDC)
{
	return TRUE;
}

int CPosInfoView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CScrollView::OnCreate(lpCreateStruct) == -1)
		return -1;

	CComparerDoc *pDoc = GetDocument();
	pDoc->mPosInfoView = this;
	mFileScanThread = pDoc->mFileScanThread;

	CSize sizeTotal;
	sizeTotal.cx = sizeTotal.cy = MIN_SIDE;
	SetScrollSizes(MM_TEXT, sizeTotal);

	return 0;
}

void CPosInfoView::ConfigureScrollSizes(CComparerDoc *pDoc)
{
	CRect rcClient;
	GetClientRect(&rcClient);

	mWClient = rcClient.Width();
	mHClient = rcClient.Height();

	int maxFrames = max(pDoc->mMaxFrames, 1);

	mPosLinesPerFrame = max(mHClient / maxFrames, POS_LINE_MIN);
	if (mPosLinesPerFrame > mDiffFlagSize) {
		delete [] mDiffFlags;

		mDiffFlagSize = mPosLinesPerFrame;
		mDiffFlags = new bool[mDiffFlagSize];
	}

	mPosLines = mPosLinesPerFrame * maxFrames
	          + maxFrames + 1;
	CSize sizeTotal(MIN_SIDE, mPosLines);

	SetScrollSizes(MM_TEXT, sizeTotal);
}

void CPosInfoView::OnSize(UINT nType, int cx, int cy)
{
	CScrollView::OnSize(nType, cx, cy);

	CComparerDoc *pDoc = GetDocument();

	ConfigureScrollSizes(pDoc);
}

void CPosInfoView::OnLButtonDown(UINT nFlags, CPoint point)
{
	CComparerDoc* pDoc = GetDocument();

	ComparerPane *paneL = &pDoc->mPane[CComparerDoc::IMG_VIEW_1];
	ComparerPane *paneR = &pDoc->mPane[CComparerDoc::IMG_VIEW_2];

	if (!paneL->isAvail() && !paneR->isAvail())
		return;

	pDoc->KillPlayTimer();

	CPoint ulCorner = GetScrollPosition();

	int frameID = (ulCorner.y + point.y) / (mPosLinesPerFrame + 1);
	frameID = min(frameID, pDoc->mMaxFrames - 1);

	CComparerDoc::PANE_ID idx;
	if (point.x < (mWClient >> 1))
		idx = CComparerDoc::IMG_VIEW_1;
	else
		idx = CComparerDoc::IMG_VIEW_2;

	ComparerPane *pane = &pDoc->mPane[idx];
	pane->SetNextFrameID(frameID);
	pDoc->ReadSource4View(pane);

	IFrmCmpStrategy *compareStrategy = pDoc->mFrmCmpStrategy;
	if (compareStrategy) {
		CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
		compareStrategy->CalMetrics(paneL, paneR, pMainFrm->mMetricIdx, pDoc->mFrmState);
	}

	pDoc->UpdateAllViews(NULL);

	CScrollView::OnLButtonDown(nFlags, point);
}

BOOL CPosInfoView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	int nPos = GetScrollPos(SB_VERT);
	SetScrollPos(SB_VERT, nPos - zDelta);
	Invalidate(FALSE);

	return CScrollView::OnMouseWheel(nFlags, zDelta, pt);
}

void CPosInfoView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	SCROLLINFO si;
	ZeroMemory(&si, sizeof(SCROLLINFO));

	GetScrollInfo(SB_VERT, &si, SIF_TRACKPOS);
	nPos = si.nTrackPos;

	CScrollView::OnVScroll(nSBCode, nPos, pScrollBar);
}
