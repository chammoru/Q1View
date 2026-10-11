#pragma once
#include "Q1UiAppearanceWin.h"
#include "Q1UiFontWin.h"
#include <algorithm>

// Keep the real SCROLLBAR window, its range messages, keyboard interface and
// Windows accessibility provider. Only normal-theme painting/mouse tracking
// are customized; contrast themes use the native implementation unchanged.
class CThemeScrollBar : public CScrollBar
{
public:
	struct Geometry { CRect client; int arrow = 0, top = 0, bottom = 0; };
	Geometry GetGeometry() const {
		Geometry g; GetClientRect(g.client);
		SCROLLBARINFO info = {sizeof(info)};
		if (::GetScrollBarInfo(m_hWnd, OBJID_CLIENT, &info)) {
			g.arrow = info.dxyLineButton; g.top = info.xyThumbTop; g.bottom = info.xyThumbBottom;
		}
		return g;
	}
	bool IsTrackingThumb() const { return mDragging; }
	int TrackPosition() const { return mTrackPosition; }
	void CancelTracking() {
		mDragging = false; mPressed = -1; KillTimer(1);
		if (::GetCapture()==m_hWnd) ::ReleaseCapture();
	}

protected:
	LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override {
		if (message==q1view::WM_UI_APPEARANCE_CHANGED) {
			if (q1view::WindowsUiAppearanceState().HighContrast()) CancelTracking();
			Invalidate(FALSE); return 0;
		}
		if (message==WM_DESTROY) CancelTracking();
		if (q1view::WindowsUiAppearanceState().HighContrast())
			return CScrollBar::WindowProc(message,wp,lp);
		switch (message) {
		case WM_PAINT: { CPaintDC dc(this); Paint(dc); return 0; }
		case WM_ERASEBKGND: return TRUE;
		case WM_LBUTTONDBLCLK:
		case WM_LBUTTONDOWN: {
			if (!IsWindowEnabled()) return 0;
			mMouse = Point(lp); const auto g = GetGeometry();
			if (g.bottom<=g.top) return 0;
			SetFocus(); SetCapture();
			if (mMouse.y>=g.top && mMouse.y<g.bottom) {
				mDragging = true; mGrab = mMouse.y-g.top; mTrackPosition = GetScrollPos();
			} else { mPressed = Hit(g,mMouse.y); Notify(mPressed); SetTimer(1,400,nullptr); }
			Invalidate(FALSE); return 0;
		}
		case WM_MOUSEMOVE: {
			mMouse = Point(lp); mHovered = true;
			TRACKMOUSEEVENT tracking = {sizeof(tracking),TME_LEAVE,m_hWnd,0}; ::TrackMouseEvent(&tracking);
			if (mDragging && ::GetCapture()==m_hWnd) Drag(mMouse.y);
			Invalidate(FALSE); return 0;
		}
		case WM_MOUSELEAVE: mHovered = false; Invalidate(FALSE); return 0;
		case WM_LBUTTONUP:
			if (::GetCapture()==m_hWnd) {
				if (mDragging) { Drag(Point(lp).y); Notify(SB_THUMBPOSITION,mTrackPosition); }
				CancelTracking(); Notify(SB_ENDSCROLL); Invalidate(FALSE);
			}
			return 0;
		case WM_CAPTURECHANGED: CancelTracking(); Invalidate(FALSE); return 0;
		case WM_CANCELMODE: CancelTracking(); Invalidate(FALSE); return 0;
		case WM_TIMER:
			if (wp==1 && mPressed!=-1 && ::GetCapture()==m_hWnd) {
				const auto g = GetGeometry();
				if (g.client.PtInRect(mMouse) && Hit(g,mMouse.y)==mPressed) Notify(mPressed);
				SetTimer(1,70,nullptr); return 0;
			}
			break;
		case WM_MOUSEWHEEL: return GetParent()->SendMessage(message,wp,lp);
		case SBM_SETSCROLLINFO: {
			const auto result = CScrollBar::WindowProc(message,FALSE,lp); Invalidate(FALSE); return result;
		}
		case SBM_SETPOS: {
			const auto result = CScrollBar::WindowProc(message,wp,FALSE); Invalidate(FALSE); return result;
		}
		case SBM_SETRANGEREDRAW: {
			const auto result = CScrollBar::WindowProc(SBM_SETRANGE,wp,lp); Invalidate(FALSE); return result;
		}
		}
		const auto result = CScrollBar::WindowProc(message,wp,lp);
		if (message==WM_SETFOCUS || message==WM_KILLFOCUS || message==WM_ENABLE || message==WM_KEYDOWN)
			Invalidate(FALSE);
		return result;
	}

private:
	bool mDragging = false, mHovered = false;
	int mGrab = 0, mTrackPosition = 0, mPressed = -1;
	CPoint mMouse;
	static CPoint Point(LPARAM value) { return CPoint(short(LOWORD(value)),short(HIWORD(value))); }
	static int Hit(const Geometry& g, int y) {
		if (y<0 || y>=g.client.bottom) return -1;
		if (y<g.arrow) return SB_LINEUP;
		if (y>=g.client.bottom-g.arrow) return SB_LINEDOWN;
		if (y<g.top) return SB_PAGEUP;
		if (y>=g.bottom) return SB_PAGEDOWN;
		return -1;
	}
	void Notify(int code, int position = 0) {
		if (code>=0) GetParent()->SendMessage(WM_VSCROLL,MAKEWPARAM(code,position),reinterpret_cast<LPARAM>(m_hWnd));
	}
	void Drag(int y) {
		const auto g = GetGeometry();
		SCROLLINFO info = {sizeof(info),SIF_RANGE|SIF_PAGE}; ::GetScrollInfo(m_hWnd,SB_CTL,&info);
		const int travel = g.client.bottom-2*g.arrow-(g.bottom-g.top);
		const int limit = (std::max)(info.nMin,info.nMax-int(info.nPage ? info.nPage-1 : 0));
		const int offset = (std::max)(0,(std::min)(travel,y-mGrab-g.arrow));
		mTrackPosition = info.nMin + (travel>0 ? int((static_cast<long long>(offset)*(limit-info.nMin)+travel/2)/travel) : 0);
		Notify(SB_THUMBTRACK,mTrackPosition);
	}
	void Paint(CDC& dc) {
		using namespace q1view;
		const auto g = GetGeometry();
		dc.FillSolidRect(g.client,WindowsUiColorValue(WindowsUiColor::SurfaceAlt));
		if (g.bottom<=g.top || !IsWindowEnabled()) return;
		const int inset = (std::max)(2,WindowsUiPixels(3,WindowsUiDpi(m_hWnd)));
		CRect thumb(inset,g.top+1,g.client.right-inset,g.bottom-1);
		const auto color = WindowsUiColorValue(mDragging ? WindowsUiColor::Text :
			mHovered ? WindowsUiColor::Muted : WindowsUiColor::Boundary);
		CBrush brush(color); CPen pen(PS_SOLID,1,color);
		auto* oldBrush = dc.SelectObject(&brush); auto* oldPen = dc.SelectObject(&pen);
		if (thumb.Width()>0 && thumb.Height()>0) dc.RoundRect(thumb,CPoint(inset*2,inset*2));
		dc.SelectObject(oldBrush); dc.SelectObject(oldPen);
		const COLORREF arrow = WindowsUiColorValue(WindowsUiColor::Muted);
		CPen arrowPen(PS_SOLID,(std::max)(1,WindowsUiPixels(1,WindowsUiDpi(m_hWnd))),arrow);
		oldPen = dc.SelectObject(&arrowPen);
		const int mid = g.client.right/2, size = (std::max)(2,int(g.client.right/5));
		for (int i=0;i<2;++i) {
			const int y = i ? g.client.bottom-g.arrow/2 : g.arrow/2;
			const int direction = i ? -1 : 1;
			dc.MoveTo(mid-size,y+direction*size/2); dc.LineTo(mid,y-direction*size/2); dc.LineTo(mid+size,y+direction*size/2);
		}
		dc.SelectObject(oldPen);
		if (::GetFocus()==m_hWnd) { CRect focus = g.client; focus.DeflateRect(1,1); dc.DrawFocusRect(focus); }
	}
};
