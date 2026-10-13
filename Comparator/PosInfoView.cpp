// PosInfoView.cpp : implementation file
//

#include "stdafx.h"
#include "Comparator.h"
#include "ComparatorDoc.h"
#include "PosInfoView.h"
#include "FrmCmpInfo.h"
#include "FrmCmpStrategy.h"
#include "FileScanThread.h"
#include "MainFrm.h"
#include "QViewerCmn.h"
#include "Q1UiFontWin.h"

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
	::lstrcpy(lf.lfFaceName, q1view::WindowsUiTextFontFamily());

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
	ON_WM_KEYDOWN()
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
								ComparatorPane *pane,
								CRect *frameRect,
								bool parseDone,
								int &curBrush,
								int &preBrush,
								int i)
{
	CComparatorDoc* pDoc = GetDocument();

	bool isCurFrame = pane->isAvail() && (i == pane->curFrameID);
	bool isLeftPane = (pane == &pDoc->mPane[CComparatorDoc::IMG_VIEW_1]);

	COLORREF frameColor;
	if (isCurFrame) {
		// Pane-specific highlight so left vs right is obvious at a glance.
		frameColor = isLeftPane || q1view::WindowsUiAppearanceState().HighContrast() ? Q1UI_COLOR_ACCENT : Q1UI_COLOR_WARNING;
	} else if (!pane->isAvail()) {
		frameColor = Q1UI_COLOR_SURFACE;
	} else if (parseDone) {
		frameColor = Q1UI_COLOR_ACCENT_SOFT;
	} else if (i < pane->frames) {
		frameColor = Q1UI_COLOR_SURFACE_ALT;
	} else {
		frameColor = Q1UI_COLOR_APP_BG;
	}

	pDC->FillSolidRect(frameRect, frameColor);
	pDC->Draw3dRect(frameRect, Q1UI_COLOR_SURFACE, Q1UI_COLOR_SURFACE);

	// Keep the timeline overview for video sources; the pink overlay only
	// describes differences in the currently displayed frame.
	if (parseDone) {
		CPen *prev = pDC->SelectObject(mDiffPen);
		const int target = pane == pDoc->mPane ? pDoc->mActiveTarget : int(pane-pDoc->mPane);
		IFrmCmpStrategy *frmCmpStrategy = pDoc->PairStrategy(target);
		list<RLC> diffRLC[QPLANES];

		if (frmCmpStrategy && mFileScanThread->copyDiffRLC(i, diffRLC, target)) {
			frmCmpStrategy->FlagTotalDiffLine(diffRLC, mDiffFlags, mPosLinesPerFrame);
			DrawDiffPosLines(pDC, frameRect, mDiffFlags, mPosLinesPerFrame);
		}
		pDC->SelectObject(prev);
	}

	COLORREF preColor = 0;
	const bool highlighted = isCurFrame || (parseDone && q1view::WindowsUiAppearanceState().HighContrast());
	if (highlighted) {
		// Highlight labels must use the corresponding foreground in both
		// Light/Dark and Windows contrast themes.
		preColor = pDC->SetTextColor(Q1UI_COLOR_ACCENT_TEXT);
	}

	CString numStr;
	numStr = i >= pane->frames ? _T("\x2014") : isCurFrame ? _T("\x25cf") : _T("");
	pDC->DrawText(numStr, frameRect, DT_SINGLELINE | DT_CENTER |  DT_VCENTER);

	if (highlighted)
		pDC->SetTextColor(preColor);

	frameRect->OffsetRect(0, mPosLinesPerFrame + 1);
}

void CPosInfoView::DrawFrameRect(CDC* dc, CRect* clip, int width)
{
	auto* doc=GetDocument(); auto slots=doc->TimelineSources();
	if (slots.empty()) return;
	int pitch=mPosLinesPerFrame+1;
	int start=max(0,int((clip->top-mHeaderHeight)/pitch));
	int end=min(int((clip->bottom-mHeaderHeight)/pitch),doc->mMaxFrames-1);
	dc->SelectObject(&mPosNumFont); dc->SetBkMode(TRANSPARENT); dc->SetTextColor(COLOR_POS_NUM_TEXT);
	for (int i=start;i<=end;++i) {
		int y=mHeaderHeight+pitch*i-clip->top;
		CRect gutter(0,y,mGutterWidth,y+pitch); CString number; number.Format(_T("%d"),i);
		dc->SetTextColor(COLOR_POS_NUM_TEXT); dc->DrawText(number,gutter,DT_SINGLELINE|DT_CENTER|DT_VCENTER);
		for (size_t col=0;col<slots.size();++col) {
			int slot=slots[col]; int target=slot==0 ? doc->mActiveTarget : slot;
			CRect cell(mGutterWidth+(width-mGutterWidth)*(int)col/(int)slots.size(),y,
				mGutterWidth+(width-mGutterWidth)*(int)(col+1)/(int)slots.size(),y+pitch);
			int current=0,previous=-1;
			bool parsed=i<doc->mPane[slot].frames && mFileScanThread->isFrameParsed(i,target);
			DrawEachRect(dc,doc->mPane+slot,&cell,parsed,current,previous,i);
		}
	}
	// Sticky source identifiers and shared frame-index gutter.
	dc->FillSolidRect(CRect(0,0,width,mHeaderHeight),Q1UI_COLOR_SURFACE);
	CRect gutter(0,0,mGutterWidth,mHeaderHeight); dc->DrawText(_T("#"),gutter,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
	for (size_t col=0;col<slots.size();++col) {
		CRect cell(mGutterWidth+(width-mGutterWidth)*(int)col/(int)slots.size(),0,
			mGutterWidth+(width-mGutterWidth)*(int)(col+1)/(int)slots.size(),mHeaderHeight);
		CString letter; letter.Format(_T("%c"),_T('A')+slots[col]);
		dc->SetTextColor(slots[col]==0 ? Q1UI_COLOR_ACCENT : Q1UI_COLOR_TEXT);
		dc->DrawText(letter,cell,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
	}
}

int CPosInfoView::SuggestedWidth()
{
	const auto dpi=q1view::WindowsUiDpi(m_hWnd);
	return mGutterWidth+q1view::WindowsUiPixels(32*(int)GetDocument()->TimelineSources().size()+17,dpi,q1view::WindowsUiSettings().Scale());
}

void CPosInfoView::OnDraw(CDC* pDC)
{
	LOGPEN pen = {};
	if (!mDiffPen->GetLogPen(&pen) || pen.lopnColor != Q1UI_COLOR_DANGER) {
		mDiffPen->DeleteObject(); mDiffPen->CreatePen(PS_SOLID, 1, Q1UI_COLOR_DANGER);
	}
	q1view::EnsureWindowsUiFont(mPosNumFont, q1view::WindowsUiFontRole::Numeric, m_hWnd);
	CComparatorDoc* pDoc = GetDocument();

	CRect clipBox;
	pDC->GetClipBox(&clipBox);

	int h = clipBox.Height();
	if (mWClient<=0 || h<=0) return;

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);

	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(pDC, mWClient, h);

	memDC.SetStretchBltMode(COLORONCOLOR);
	memDC.SelectObject(bitmap);

	ComparatorPane *paneL = &pDoc->mPane[CComparatorDoc::IMG_VIEW_1];
	ComparatorPane *paneR = &pDoc->mPane[CComparatorDoc::IMG_VIEW_2];

	memDC.FillSolidRect(CRect(0, 0, mWClient, h), Q1UI_COLOR_APP_BG);

	// Pilot: hide the timeline entirely when neither pane is a video. The pink
	// grid overlay in ComparatorView already conveys diff information; the
	// timeline is only useful for picking a frame in a video source.
	bool hasVideo = pDoc->mMaxFrames>1;

	if (pDoc->TimelineSources().empty() || !hasVideo) {
		CFont *prevFont = memDC.SelectObject(CFont::FromHandle(
			mUiFonts.Get(q1view::WindowsUiFontRole::Command, m_hWnd)));
		memDC.SetBkMode(TRANSPARENT);
		memDC.SetTextColor(Q1UI_COLOR_TEXT_MUTED);
		CRect msgRect(0, 0, mWClient, h);
		LPCTSTR msg = !pDoc->TimelineSources().empty()
			? _T("Timeline\n(video only)") : _T("Timeline");
		memDC.DrawText(msg, &msgRect,
			DT_CENTER | DT_VCENTER | DT_WORDBREAK | DT_NOCLIP);
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

	CComparatorDoc *pDoc = GetDocument();
	pDoc->mPosInfoView = this;
	mFileScanThread = pDoc->mFileScanThread;

	ModifyStyle(WS_HSCROLL | WS_VSCROLL, WS_CLIPCHILDREN);
	if (!mTimelineScroll.Create(WS_CHILD | WS_TABSTOP | SBS_VERT, CRect(0,0,0,0), this, 0x7810)) return -1;
	mTimelineScroll.SetWindowText(_T("Timeline scroll bar"));
	SetScrollSizes(MM_TEXT, CSize(0,0));

	return 0;
}

void CPosInfoView::ConfigureScrollSizes(CComparatorDoc *pDoc)
{
	CRect rcClient;
	GetClientRect(&rcClient);

	mWClient = rcClient.Width();
	mHClient = rcClient.Height();

	const bool hasVideo = pDoc->mMaxFrames>1;
	const int maxFrames = hasVideo ? max(pDoc->mMaxFrames,0) : 0;
	const int minimum = q1view::WindowsUiPixels(18, q1view::WindowsUiDpi(m_hWnd), q1view::WindowsUiSettings().Scale());
	mHeaderHeight = minimum+1;
	q1view::EnsureWindowsUiFont(mPosNumFont,q1view::WindowsUiFontRole::Numeric,m_hWnd);
	CClientDC dc(this); auto* old=dc.SelectObject(&mPosNumFont);
	CString number; number.Format(_T("%d"), max(0,pDoc->mMaxFrames-1));
	mGutterWidth=dc.GetTextExtent(number).cx+q1view::WindowsUiPixels(12,q1view::WindowsUiDpi(m_hWnd));
	dc.SelectObject(old);
	// Include each row's border in its pitch. The old height+2 calculation
	// created a scroll range even for an empty or fully fitting timeline.
	const int pitch = maxFrames ? max((mHClient-mHeaderHeight)/maxFrames,minimum+1) : minimum+1;
	mPosLinesPerFrame = pitch-1;
	if (mPosLinesPerFrame > mDiffFlagSize) {
		delete [] mDiffFlags;

		mDiffFlagSize = mPosLinesPerFrame;
		mDiffFlags = new bool[mDiffFlagSize];
	}

	mPosLines = maxFrames ? int(min(mHeaderHeight+static_cast<long long>(pitch)*maxFrames,static_cast<long long>(INT_MAX))) : 0;
	SetScrollSizes(MM_TEXT, CSize(0,mPosLines), CSize(0,max(1,mHClient*9/10)), CSize(0,pitch));
	const bool visible = mPosLines>mHClient && mHClient>0;
	if (mTimelineScroll.GetSafeHwnd()) {
		if (!visible) mTimelineScroll.CancelTracking();
		const int width = min(mWClient,q1view::WindowsUiPixels(17,q1view::WindowsUiDpi(m_hWnd)));
		mTimelineScroll.SetWindowPos(nullptr,mWClient-width,0,width,mHClient,SWP_NOZORDER | SWP_NOACTIVATE);
		mTimelineScroll.ShowWindow(visible ? SW_SHOWNA : SW_HIDE);
		if (visible) mWClient-=width;
	}
}

CScrollBar* CPosInfoView::GetScrollBarCtrl(int bar) const
{
	if (bar==SB_VERT && mTimelineScroll.GetSafeHwnd()) return &mTimelineScroll;
	return CScrollView::GetScrollBarCtrl(bar);
}

BOOL CPosInfoView::OnScrollBy(CSize scroll, BOOL doScroll)
{
	if (!mTimelineScroll.GetSafeHwnd() || !mTimelineScroll.IsWindowEnabled()) return FALSE;
	const int old = GetScrollPos(SB_VERT);
	const int position = int(max(0LL,min(static_cast<long long>(GetScrollLimit(SB_VERT)),static_cast<long long>(old)+scroll.cy)));
	if (position==old) return FALSE;
	if (doScroll) {
		SetScrollPos(SB_VERT,position);
		// Repaint the timeline, rather than ScrollWindow which also moves its
		// child scrollbar. The scrollbar must remain anchored to the viewport.
		Invalidate(FALSE);
	}
	return TRUE;
}

void CPosInfoView::OnSize(UINT nType, int cx, int cy)
{
	CScrollView::OnSize(nType, cx, cy);

	CComparatorDoc *pDoc = GetDocument();

	ConfigureScrollSizes(pDoc);
}

void CPosInfoView::OnLButtonDown(UINT flags,CPoint point)
{
	auto* doc=GetDocument(); auto slots=doc->TimelineSources();
	if (slots.empty() || point.x>=mWClient || point.y<mHeaderHeight || doc->mMaxFrames<=1) return;
	int frame=(GetScrollPosition().y+point.y-mHeaderHeight)/(mPosLinesPerFrame+1);
	if (frame<0 || frame>=doc->mMaxFrames) return;
	if (point.x<mGutterWidth) { doc->KillPlayTimer(); doc->SetScenes(frame); }
	else {
		doc->mRequestedFrame=-1;
		int col=(point.x-mGutterWidth)*(int)slots.size()/max(1,mWClient-mGutterWidth);
		if (col<0 || col>=(int)slots.size()) return;
		auto* pane=doc->mPane+slots[col];
		if (frame>=pane->frames) return; // A missing frame is not the last frame.
		doc->KillPlayTimer(); bool updated=false; doc->SetScene(frame,pane,updated);
		doc->RefreshSelectionMetric();
	}
	doc->UpdateAllViews(nullptr); SetFocus();
	CScrollView::OnLButtonDown(flags,point);
}

BOOL CPosInfoView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	return DoMouseWheel(nFlags,zDelta,pt);
}

void CPosInfoView::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)
{
	if (nSBCode==SB_THUMBTRACK || nSBCode==SB_THUMBPOSITION) {
		SCROLLINFO si = {};
		GetScrollInfo(SB_VERT,&si,SIF_TRACKPOS);
		const int position = mTimelineScroll.IsTrackingThumb() ? mTimelineScroll.TrackPosition() : si.nTrackPos;
		OnScrollBy(CSize(0,position-GetScrollPos(SB_VERT)));
	} else CScrollView::OnVScroll(nSBCode,nPos,pScrollBar);
}

void CPosInfoView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	int scroll = -1;
	switch (nChar) {
	case VK_UP: scroll = SB_LINEUP; break;
	case VK_DOWN: scroll = SB_LINEDOWN; break;
	case VK_PRIOR: scroll = SB_PAGEUP; break;
	case VK_NEXT: scroll = SB_PAGEDOWN; break;
	case VK_HOME: scroll = SB_TOP; break;
	case VK_END: scroll = SB_BOTTOM; break;
	}
	if (scroll!=-1) { OnScroll(MAKEWORD(0xff,scroll),0); return; }
	// The timeline owns focus after a seek click, so route playback controls
	// here too: Space toggles play/pause, Left/Right step one frame. Mirrors
	// CComparatorView::OnKeyDown so the same shortcuts behave identically
	// whether the image view or the timeline currently has focus.
	CComparatorDoc* pDoc = GetDocument();

	bool isProcessing = pDoc->CheckImgViewProcessing();
	if (nChar != VK_SPACE && isProcessing) {
		CScrollView::OnKeyDown(nChar, nRepCnt, nFlags);
		return;
	}

	switch (nChar) {
	case VK_SPACE:
		pDoc->TogglePlay();
		break;
	case VK_LEFT:
		pDoc->OffsetScenes(-1);
		break;
	case VK_RIGHT:
		pDoc->OffsetScenes(1);
		break;
	default:
		CScrollView::OnKeyDown(nChar, nRepCnt, nFlags);
		return;
	}

	pDoc->MarkImgViewProcessing();
	pDoc->UpdateAllViews(NULL);
}
