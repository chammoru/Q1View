#include "stdafx.h"
#include "QMenuItem.h"
#include "Q1UiFontWin.h"
#include "Q1UiCommandWin.h"
#include "Q1UiBufferedPaintWin.h"

IMPLEMENT_DYNAMIC(CQMenuItem, CWnd)
CQMenuItem::CQMenuItem() = default;
CQMenuItem::~CQMenuItem() = default;

BEGIN_MESSAGE_MAP(CQMenuItem, CWnd)
	ON_WM_PAINT()
	ON_WM_DRAWITEM_REFLECT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_MOUSEMOVE()
	ON_WM_MOUSELEAVE()
	ON_WM_SETFOCUS()
	ON_WM_KILLFOCUS()
	ON_WM_ENABLE()
	ON_WM_KEYDOWN()
	ON_CONTROL_REFLECT(BN_CLICKED, OnClicked)
END_MESSAGE_MAP()

BOOL CQMenuItem::Create(LPCTSTR text, CRect& rect, CWnd* parent, CMenu* menu, DWORD align)
{
	mMenu = menu; mTextHorizAlign = align;
	return CWnd::Create(menu ? _T("BUTTON") : _T("STATIC"), text,
		WS_VISIBLE | WS_CHILD | (menu ? WS_TABSTOP | BS_OWNERDRAW : SS_LEFT | SS_NOPREFIX), rect, parent, 0xffff);
}

void CQMenuItem::DefaultSetting(CDC* dc, CString& text)
{
	q1view::EnsureWindowsUiFont(mFont, mMenu ? q1view::WindowsUiFontRole::Command : q1view::WindowsUiFontRole::Caption, m_hWnd);
	dc->SelectObject(mFont); dc->SetBkMode(TRANSPARENT);
	GetWindowText(text);
}

void CQMenuItem::CalcRect(CRect* rect)
{
	CClientDC dc(this); q1view::WindowsUiDcState saved(dc.GetSafeHdc());
	CString text; DefaultSetting(&dc, text);
	dc.DrawText(text, rect, DT_SINGLELINE | DT_CALCRECT | DT_NOPREFIX);
	const UINT dpi = q1view::WindowsUiDpi(m_hWnd);
	const int padding = q1view::WindowsUiPixels(8, dpi);
	rect->right += 2 * padding + (mMenu ? q1view::WindowsUiPixels(10, dpi) + padding : 0);
	rect->bottom = rect->top + (std::max)(q1view::WindowsUiPixels(28, dpi), rect->Height() + padding);
}

void CQMenuItem::OnPaint()
{
	CPaintDC dc(this); Paint(dc.GetSafeHdc());
}

void CQMenuItem::DrawItem(LPDRAWITEMSTRUCT item) { Paint(item->hDC); }

void CQMenuItem::Paint(HDC dc)
{
	const bool enabled = IsWindowEnabled() != FALSE;
	const bool pressed = enabled && mMenu && (mPopupOpen || mPopupQueued || (::SendMessage(m_hWnd, BM_GETSTATE, 0, 0) & BST_PUSHED));
	const bool hot = enabled && mMenu && mMouseIn;
	const auto background = q1view::WindowsUiCommandBackground(enabled, hot, pressed);
	q1view::WindowsUiBufferedPaint paint(dc, mRcClient, background);
	CDC* target = CDC::FromHandle(paint.Dc()); q1view::WindowsUiDcState saved(paint.Dc());
	CString text; DefaultSetting(target, text);
	target->SetTextColor(q1view::WindowsUiColorValue(!enabled ? q1view::WindowsUiColor::Muted :
		(hot || pressed) && q1view::WindowsUiAppearanceState().HighContrast() ? q1view::WindowsUiColor::OnAccent : q1view::WindowsUiColor::Text));
	CRect rect = mRcClient;
	const int padding = q1view::WindowsUiPixels(8, q1view::WindowsUiDpi(m_hWnd));
	rect.DeflateRect(padding, 0);
	if (mMenu) {
		const int arrow = q1view::WindowsUiPixels(10, q1view::WindowsUiDpi(m_hWnd));
		const int x = rect.right - arrow / 2, y = rect.CenterPoint().y;
		CPen pen(PS_SOLID, 1, q1view::WindowsUiColorValue(q1view::WindowsUiColor::Boundary));
		CPen* oldPen = target->SelectObject(&pen);
		auto* oldBrush = target->SelectStockObject(NULL_BRUSH);
		target->Rectangle(mRcClient);
		POINT chevron[] = {{x - arrow / 3, y - arrow / 5}, {x, y + arrow / 5}, {x + arrow / 3, y - arrow / 5}};
		target->Polyline(chevron, 3); rect.right -= arrow + padding;
		target->SelectObject(oldPen); target->SelectObject(oldBrush);
	}
	if (pressed) rect.OffsetRect(1, 1);
	target->DrawText(text, &rect, DT_SINGLELINE | DT_VCENTER | mTextHorizAlign | DT_END_ELLIPSIS | DT_NOPREFIX);
	if (mMenu && enabled && GetFocus() == this && !(SendMessage(WM_QUERYUISTATE) & UISF_HIDEFOCUS)) {
		CRect focus = mRcClient; focus.DeflateRect(3, 3); target->DrawFocusRect(focus);
	}
}

void CQMenuItem::OnClicked()
{
	if (!mMenu || !IsWindowEnabled() || mPopupOpen || mPopupQueued) return;
	// Finish native button release/capture handling before entering a menu loop,
	// just as the shared frame menu does. Coalesce duplicate notifications.
	mPopupQueued = PostMessage(OpenPopupMessage) != FALSE;
}

void CQMenuItem::OpenPopup()
{
	mPopupQueued = false;
	if (!mMenu || !IsWindowEnabled() || mPopupOpen) return;
	mPopupOpen = true; SetFocus(); Invalidate(FALSE); UpdateWindow();
	const HWND self = m_hWnd, parent = GetParent()->GetSafeHwnd();
	const HMENU menu = mMenu->GetSafeHmenu();
	CRect rect; GetWindowRect(rect);
	// RETURNCMD avoids duplicate WM_COMMAND, while retaining initialization
	// notifications for theme/fonts and command state. NONOTIFY suppresses those.
	const UINT command = ::TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
		rect.left, rect.bottom, 0, parent, nullptr);
#ifdef Q1VIEW_COMPARER_TESTS
	if (::IsWindow(parent)) {
		::SetPropW(parent, L"Q1ViewTestReturnedCommand", reinterpret_cast<HANDLE>(UINT_PTR(command)));
		::SetPropW(parent, L"Q1ViewTestRetainedControl", reinterpret_cast<HANDLE>(UINT_PTR(::IsWindow(self) && CWnd::FromHandlePermanent(self) == this)));
	}
#endif
	if (!::IsWindow(self) || CWnd::FromHandlePermanent(self) != this) return;
	mPopupOpen = false; Invalidate(FALSE);
	if (command && ::IsWindow(parent)) ::PostMessage(parent, WM_COMMAND, command, 0);
}

BOOL CQMenuItem::OnEraseBkgnd(CDC*) { return TRUE; }
LRESULT CQMenuItem::WindowProc(UINT message, WPARAM wp, LPARAM lp)
{
	if (message == OpenPopupMessage) { if (mPopupQueued) OpenPopup(); return 0; }
	if (mMenu && message == WM_LBUTTONDOWN) SendMessage(WM_CHANGEUISTATE, MAKEWPARAM(UIS_SET, UISF_HIDEFOCUS));
	const HWND self = m_hWnd;
	const LRESULT result = CWnd::WindowProc(message, wp, lp);
	// Owner-drawn native buttons can request a parent draw synchronously on a
	// state change. Our local buffered WM_PAINT path must be invalidated too.
	if (::IsWindow(self) && CWnd::FromHandlePermanent(self) == this &&
		(message == BM_SETSTATE || message == WM_CAPTURECHANGED || message == WM_LBUTTONDOWN ||
		message == WM_LBUTTONUP || message == WM_KEYDOWN || message == WM_KEYUP || message == WM_UPDATEUISTATE)) Invalidate(FALSE);
	return result;
}
void CQMenuItem::OnSize(UINT type, int cx, int cy) { CWnd::OnSize(type, cx, cy); mRcClient.SetRect(0, 0, cx, cy); }
void CQMenuItem::OnMouseMove(UINT flags, CPoint point)
{
	if (mMenu && IsWindowEnabled() && !mMouseIn) {
		mMouseIn = true; TRACKMOUSEEVENT track = {sizeof(track), TME_LEAVE, m_hWnd, 0};
		::TrackMouseEvent(&track); Invalidate(FALSE);
	}
	CWnd::OnMouseMove(flags, point);
}
void CQMenuItem::OnMouseLeave() { mMouseIn = false; Invalidate(FALSE); }
void CQMenuItem::OnSetFocus(CWnd* old) { CWnd::OnSetFocus(old); Invalidate(FALSE); }
void CQMenuItem::OnKillFocus(CWnd* next) { CWnd::OnKillFocus(next); Invalidate(FALSE); }
void CQMenuItem::OnEnable(BOOL enable) { CWnd::OnEnable(enable); mMouseIn = false; Invalidate(FALSE); }
void CQMenuItem::OnKeyDown(UINT key, UINT repeat, UINT flags)
{
	if (mMenu && IsWindowEnabled()) SendMessage(WM_CHANGEUISTATE, MAKEWPARAM(UIS_CLEAR, UISF_HIDEFOCUS));
	if (mMenu && key == VK_TAB) { GetParent()->SetFocus(); return; }
	if (mMenu && IsWindowEnabled() && (key == VK_RETURN || key == VK_DOWN || key == VK_F4)) {
		if (!(flags & KF_REPEAT)) OnClicked();
		return;
	}
	CWnd::OnKeyDown(key, repeat, flags); // Native Space activates once on key-up.
}
void CQMenuItem::SetWindowText(LPCTSTR text) { CWnd::SetWindowText(text); }
