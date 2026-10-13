#pragma once

#include "ThemeScrollBar.h"
#include "QViewerCmn.h"
#include "Q1UiFontWin.h"
#include "Q1UiBufferedPaintWin.h"
#include <windowsx.h>

// The same themed, accessible native scrollbar used by Timeline. The child
// remains viewport anchored when content scrolls (CScrollView::ScrollWindow
// would move it, as well as any native pair-selection buttons).
class CAnalysisScrollView : public CScrollView
{
	DECLARE_DYNAMIC(CAnalysisScrollView)
protected:
	mutable CThemeScrollBar mAnalysisScroll;
	int mContentHeight = 0;
	CSize mExtentViewport = CSize(-1,-1);
	void SetAnalysisExtent(int height)
	{
		CRect rc; GetClientRect(rc);
		if (height==mContentHeight && rc.Size()==mExtentViewport) return;
		mExtentViewport=rc.Size();
		mContentHeight = max(0,height);
		SetScrollSizes(MM_TEXT,CSize(0,mContentHeight),CSize(0,max(1,rc.Height()*9/10)),CSize(0,24));
		int width=q1view::WindowsUiPixels(17,q1view::WindowsUiDpi(m_hWnd));
		mAnalysisScroll.SetWindowPos(nullptr,max(0,rc.Width()-width),0,width,rc.Height(),SWP_NOZORDER|SWP_NOACTIVATE);
		mAnalysisScroll.ShowWindow(height>rc.Height() ? SW_SHOWNA : SW_HIDE);
	}
	int ContentWidth() const
	{
		CRect rc; GetClientRect(rc);
		return max(0,rc.Width()-(mAnalysisScroll.IsWindowVisible() ? q1view::WindowsUiPixels(17,q1view::WindowsUiDpi(m_hWnd)) : 0));
	}
public:
	CScrollBar* GetScrollBarCtrl(int bar) const override
	{
		return bar==SB_VERT && mAnalysisScroll.GetSafeHwnd() ? &mAnalysisScroll : CScrollView::GetScrollBarCtrl(bar);
	}
protected:
	BOOL OnScrollBy(CSize scroll, BOOL apply=TRUE) override
	{
		int old=GetScrollPos(SB_VERT);
		int pos=int(max(0LL,min((long long)GetScrollLimit(SB_VERT),(long long)old+scroll.cy)));
		if (old==pos) return FALSE;
		if (apply) { SetScrollPos(SB_VERT,pos); Invalidate(FALSE); }
		return TRUE;
	}
	virtual BOOL AnalysisMouseWheel(UINT flags,short delta,CPoint point)
	{
		return DoMouseWheel(flags,delta,point);
	}
	LRESULT WindowProc(UINT message, WPARAM w, LPARAM l) override
	{
		if (message==WM_CTLCOLORBTN || message==WM_CTLCOLORSTATIC) {
			const HWND control=reinterpret_cast<HWND>(l);
			const int id=::GetDlgCtrlID(control);
			if (::GetParent(control)==m_hWnd &&
				((message==WM_CTLCOLORBTN && ((id>=0x7900 && id<0x7903) || id==0x7950)) ||
				 (message==WM_CTLCOLORSTATIC && id==0x7903))) {
				SetBkColor(reinterpret_cast<HDC>(w),Q1UI_COLOR_SURFACE_ALT);
				return reinterpret_cast<LRESULT>(q1view::WindowsUiColorBrush(q1view::WindowsUiColor::SurfaceAlt));
			}
		}
		if (message==WM_MOUSEWHEEL) {
			CPoint pt(GET_X_LPARAM(l),GET_Y_LPARAM(l));
			return AnalysisMouseWheel(LOWORD(w),GET_WHEEL_DELTA_WPARAM(w),pt);
		}
		if (message==WM_VSCROLL && (LOWORD(w)==SB_THUMBTRACK || LOWORD(w)==SB_THUMBPOSITION)) {
			SCROLLINFO si={}; GetScrollInfo(SB_VERT,&si,SIF_TRACKPOS);
			int pos=mAnalysisScroll.IsTrackingThumb() ? mAnalysisScroll.TrackPosition() : si.nTrackPos;
			return OnScrollBy(CSize(0,pos-GetScrollPos(SB_VERT)));
		}
		LRESULT result=CScrollView::WindowProc(message,w,l);
		if (message==WM_CREATE && result!=-1) {
			ModifyStyle(WS_VSCROLL|WS_HSCROLL,WS_CLIPCHILDREN);
			if (!mAnalysisScroll.Create(WS_CHILD|WS_TABSTOP|SBS_VERT,CRect(),this,0x7811)) return -1;
			mAnalysisScroll.SetWindowText(_T("Analysis scroll bar"));
			SetAnalysisExtent(0);
		}
		if (message==WM_SIZE && mAnalysisScroll.GetSafeHwnd()) SetAnalysisExtent(mContentHeight);
		return result;
	}
};

// A non-interactive native label for reference A, using the same footer style.
class CReferenceLabel : public CStatic
{
protected:
	LRESULT WindowProc(UINT message, WPARAM w, LPARAM l) override
	{
		if (message==WM_ERASEBKGND && (GetStyle()&SS_TYPEMASK)==SS_OWNERDRAW) return TRUE;
		return CStatic::WindowProc(message,w,l);
	}
public:
	CFont mFont;
	void DrawItem(LPDRAWITEMSTRUCT item) override
	{
		q1view::WindowsUiBufferedPaint paint(item->hDC,item->rcItem,q1view::WindowsUiColor::SurfaceAlt);
		CDC dc; dc.Attach(paint.Dc());
		CRect rc(item->rcItem);
		dc.Draw3dRect(rc,Q1UI_COLOR_BORDER_SOFT,Q1UI_COLOR_BORDER_SOFT);
		q1view::EnsureWindowsUiFont(mFont,q1view::WindowsUiFontRole::Body,m_hWnd);
		auto* old=dc.SelectObject(&mFont);
		dc.SetBkMode(TRANSPARENT); dc.SetTextColor(Q1UI_COLOR_TEXT_MUTED);
		CString text; GetWindowText(text); rc.DeflateRect(8,4);
		dc.DrawText(text,rc,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
		dc.SelectObject(old); dc.Detach();
	}
};

// A real Win32 button provides focus, keyboard activation, and accessible
// names; owner drawing only changes its themed presentation.
class CComparisonButton : public CButton
{
protected:
	LRESULT WindowProc(UINT message, WPARAM w, LPARAM l) override
	{
		// Keep native focus, keyboard, accessible names and button messages.
		// Only owner-drawn controls repaint the entire background themselves.
		if (message==WM_ERASEBKGND && (GetStyle()&BS_TYPEMASK)==BS_OWNERDRAW) return TRUE;
		return CButton::WindowProc(message,w,l);
	}
public:
	bool active=false;
	CFont mFont;
	CToolTipCtrl mInfoTip;
	CString mInfoText;
	void SetInfoTip(const CString& text)
	{
		if (mInfoText==text) return;
		mInfoText=text;
		if (!mInfoTip.GetSafeHwnd()) {
			mInfoTip.Create(this); mInfoTip.AddTool(this,text);
			mInfoTip.SetMaxTipWidth(q1view::WindowsUiPixels(560,q1view::WindowsUiDpi(m_hWnd)));
		} else mInfoTip.UpdateTipText(text,this);
	}
	BOOL PreTranslateMessage(MSG* message) override
	{
		if (mInfoTip.GetSafeHwnd()) mInfoTip.RelayEvent(message);
		return CButton::PreTranslateMessage(message);
	}
	void DrawItem(LPDRAWITEMSTRUCT item) override
	{
		q1view::WindowsUiBufferedPaint paint(item->hDC,item->rcItem,q1view::WindowsUiColor::SurfaceAlt);
		CDC dc; dc.Attach(paint.Dc());
		CRect rc(item->rcItem);
		dc.Draw3dRect(rc,active ? Q1UI_COLOR_ACCENT : Q1UI_COLOR_BORDER_SOFT,active ? Q1UI_COLOR_ACCENT : Q1UI_COLOR_BORDER_SOFT);
		q1view::EnsureWindowsUiFont(mFont,q1view::WindowsUiFontRole::Body,m_hWnd);
		auto* old=dc.SelectObject(&mFont);
		dc.SetBkMode(TRANSPARENT); dc.SetTextColor(Q1UI_COLOR_TEXT);
		CString text; GetWindowText(text); rc.DeflateRect(8,4);
		dc.DrawText(text,rc,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
		if (item->itemState&ODS_FOCUS) dc.DrawFocusRect(rc);
		dc.SelectObject(old); dc.Detach();
	}
};
