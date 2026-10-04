#pragma once

#include "Q1UiMenuWin.h"
#include <dwmapi.h>
#include <commctrl.h>
#include <windowsx.h>
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "comctl32.lib")

#ifndef WM_DPICHANGED
#define WM_DPICHANGED 0x02E0
#endif

namespace q1view {

// Client-painted title, native DWM caption buttons, and accessible Win32 menu
// buttons. Popup HMENUs, command IDs, and Windows popup tracking are retained.
// No global NONCLIENTMETRICS changes and no separate overlay windows/timers.
class WindowsUiFrame {
	HWND mWindow = nullptr, mBar = nullptr, mPreviousFocus = nullptr;
	HMENU mMenu = nullptr, mOwnedMenu = nullptr, mSelectedMenu = nullptr;
	WindowsUiMenus* mMenus = nullptr;
	WindowsUiFontCache mFonts;
	std::vector<HWND> mButtons;
	std::vector<RECT> mButtonRects;
	bool mCustom = false, mMenuVisible = true, mTracking = false, mKeyboard = false;
	bool mDeferredClose = false;
	int mTitleHeight = 0, mMenuHeight = 0, mOpen = -1, mNext = -1;
	UINT mSelected = 0, mSelectedFlags = 0;
	static WindowsUiFrame*& Tracking() { static thread_local WindowsUiFrame* value = nullptr; return value; }
	static constexpr UINT WM_OPEN_MENU = WM_APP + 119;
	static constexpr UINT WM_REPAINT_FRAME = WM_APP + 120;

	static int Metric(int id, UINT dpi) {
		using Function = int(WINAPI*)(int, UINT);
		static const auto fn = reinterpret_cast<Function>(GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi"));
		return fn ? fn(id, dpi) : GetSystemMetrics(id);
	}
	int Px(int dip) const { return WindowsUiPixels(dip, WindowsUiDpi(mWindow)); }
	bool Caption() const { return (GetWindowLongPtrW(mWindow, GWL_STYLE) & WS_CAPTION) == WS_CAPTION; }
	RECT CaptionButtons() const {
		RECT client; GetClientRect(mWindow, &client);
		RECT rect = {(std::max)(0, int(client.right) - Px(138)), 0, client.right, (std::min)(mTitleHeight, Px(32))};
		RECT native = {};
		if (SUCCEEDED(DwmGetWindowAttribute(mWindow, DWMWA_CAPTION_BUTTON_BOUNDS, &native, sizeof(native))) && native.right > native.left) {
			RECT window; GetWindowRect(mWindow, &window); POINT origin = {}; ClientToScreen(mWindow, &origin);
			const LONG left = native.left - (origin.x - window.left);
			if (left < client.right) rect.left = (std::max)(0L, left);
			rect.top = (std::max)(0L, native.top - (origin.y - window.top));
			rect.bottom = (std::min)(LONG(mTitleHeight), native.bottom - (origin.y - window.top));
			if (rect.bottom <= rect.top) { rect.top = 0; rect.bottom = (std::min)(mTitleHeight, Px(32)); }
		}
		return rect;
	}
	static bool CanCustomize() {
		HIGHCONTRASTW contrast = {sizeof(contrast)};
		SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0);
		BOOL composition = FALSE;
		return !(contrast.dwFlags & HCF_HIGHCONTRASTON) && SUCCEEDED(DwmIsCompositionEnabled(&composition)) && composition;
	}
	MENUITEMINFOW Info(int position) const {
		MENUITEMINFOW item = {sizeof(item)};
		item.fMask = MIIM_ID | MIIM_SUBMENU | MIIM_STATE | MIIM_FTYPE | MIIM_DATA;
		GetMenuItemInfoW(mMenu, position, TRUE, &item); return item;
	}
	int ButtonAt(POINT screen) const {
		if (!mBar || !IsWindowVisible(mBar)) return -1;
		ScreenToClient(mBar, &screen);
		for (size_t i = 0; i < mButtonRects.size(); ++i)
			if (PtInRect(&mButtonRects[i], screen) && IsWindowEnabled(mButtons[i])) return int(i);
		return -1;
	}
	bool IsButton(HWND window) const {
		return std::find(mButtons.begin(), mButtons.end(), window) != mButtons.end();
	}
	void RememberFocus(HWND window) {
		if (window && window != mBar && !IsButton(window) && IsChild(mWindow, window)) mPreviousFocus = window;
	}
	void RestoreFocus() {
		if (IsWindow(mPreviousFocus) && IsWindowVisible(mPreviousFocus)) SetFocus(mPreviousFocus);
		mKeyboard = false;
	}
	int NextEnabled(int position, int direction) const {
		const int count = int(mButtons.size());
		for (int i = 0; i < count; ++i) {
			position = (position + direction + count) % count;
			if (IsWindowEnabled(mButtons[position])) return position;
		}
		return -1;
	}
	static LRESULT CALLBACK ButtonProc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
		auto* self = reinterpret_cast<WindowsUiFrame*>(data);
		if (message == WM_SETFOCUS) self->RememberFocus(reinterpret_cast<HWND>(wp));
		if (message == WM_NCDESTROY) RemoveWindowSubclass(window, ButtonProc, 1);
		return DefSubclassProc(window, message, wp, lp);
	}
	static LRESULT CALLBACK BarProc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
		auto* self = reinterpret_cast<WindowsUiFrame*>(GetWindowLongPtrW(window, GWLP_USERDATA));
		if (message == WM_NCCREATE) {
			self = static_cast<WindowsUiFrame*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
			SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
		}
		if (!self) return DefWindowProcW(window, message, wp, lp);
		if (message == WM_ERASEBKGND) {
			RECT rect; GetClientRect(window, &rect); FillRect(reinterpret_cast<HDC>(wp), &rect, GetSysColorBrush(COLOR_MENUBAR)); return TRUE;
		}
		if (message == WM_DRAWITEM) {
			auto draw = *reinterpret_cast<DRAWITEMSTRUCT*>(lp);
			const int index = int(draw.CtlID) - 1;
			if (index >= 0 && index < int(self->mButtons.size())) {
				draw.CtlType = ODT_MENU; draw.itemData = self->Info(index).dwItemData;
				if (self->mTracking && self->mOpen == index) draw.itemState |= ODS_SELECTED;
				if (!self->mKeyboard) draw.itemState |= ODS_NOACCEL;
				self->mMenus->Draw(&draw);
				if (draw.itemState & ODS_FOCUS) { InflateRect(&draw.rcItem, -2, -2); DrawFocusRect(draw.hDC, &draw.rcItem); }
				return TRUE;
			}
		}
		if (message == WM_COMMAND && HIWORD(wp) == BN_CLICKED) {
			PostMessageW(self->mWindow, WM_OPEN_MENU, LOWORD(wp) - 1, 0); return 0;
		}
		return DefWindowProcW(window, message, wp, lp);
	}
	static LRESULT CALLBACK MenuFilter(int code, WPARAM wp, LPARAM lp) {
		auto* self = Tracking();
		if (code == MSGF_MENU && self) {
			const auto* message = reinterpret_cast<MSG*>(lp);
			int next = -1;
			if (message->message == WM_MOUSEMOVE) next = self->ButtonAt(message->pt);
			if (message->message == WM_KEYDOWN && self->mSelectedMenu == self->Info(self->mOpen).hSubMenu) {
				if (message->wParam == VK_LEFT) next = self->NextEnabled(self->mOpen, -1);
				if (message->wParam == VK_RIGHT && !(self->mSelectedFlags & MF_POPUP)) next = self->NextEnabled(self->mOpen, 1);
			}
			if (next >= 0 && next != self->mOpen) { self->mNext = next; EndMenu(); return 1; }
		}
		return CallNextHookEx(nullptr, code, wp, lp);
	}
	void Open(int position, bool keyboard) {
		if (mTracking || position < 0 || position >= int(mButtons.size()) || !IsWindowEnabled(mButtons[position])) return;
		mKeyboard = keyboard;
		mTracking = true;
		WindowsUiFrame* previous = Tracking(); Tracking() = this;
		HHOOK hook = SetWindowsHookExW(WH_MSGFILTER, MenuFilter, nullptr, GetCurrentThreadId());
		UINT command = 0;
		do {
			mOpen = position; mNext = -1; mSelectedMenu = Info(position).hSubMenu; mSelectedFlags = 0;
			if (keyboard) SetFocus(mButtons[position]);
			const auto item = Info(position);
			if (!item.hSubMenu) { command = item.wID; break; }
			RECT rect = mButtonRects[position]; MapWindowPoints(mBar, nullptr, reinterpret_cast<POINT*>(&rect), 2);
			InvalidateRect(mButtons[position], nullptr, FALSE); UpdateWindow(mButtons[position]);
			TPMPARAMS parameters = {sizeof(parameters)}; parameters.rcExclude = rect;
			command = TrackPopupMenuEx(item.hSubMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD,
				rect.left, rect.bottom, mWindow, &parameters);
			InvalidateRect(mButtons[position], nullptr, FALSE);
			position = mNext;
		} while (!command && position >= 0 && !mDeferredClose && IsWindow(mWindow));
		if (hook) UnhookWindowsHookEx(hook);
		Tracking() = previous; mTracking = false; mOpen = -1;
		if (command || !keyboard) RestoreFocus();
		if (IsWindow(mWindow)) {
			for (HWND button : mButtons) InvalidateRect(button, nullptr, FALSE);
			if (mDeferredClose) { mDeferredClose = false; PostMessageW(mWindow, WM_CLOSE, 0, 0); }
			else if (command) PostMessageW(mWindow, WM_COMMAND, command, 0);
		}
	}
public:
	~WindowsUiFrame() { Destroy(); }
	void Destroy() {
		// Detaching the root menu transfers its former HWND-owned lifetime to
		// this helper. Native fallback menus remain HWND-owned on destruction.
		if (mOwnedMenu && IsMenu(mOwnedMenu) && ::GetMenu(mWindow) != mOwnedMenu) DestroyMenu(mOwnedMenu);
		mOwnedMenu = nullptr; mMenu = nullptr; mWindow = nullptr; mCustom = false;
	}
	bool Initialized() const { return mWindow != nullptr; }
	bool Custom() const { return mCustom; }
	HMENU Menu() const { return mMenuVisible ? mMenu : nullptr; }
	HMENU RetainedMenu() const { return mMenu; }
	int Height() const { return mCustom && Caption() ? mTitleHeight + mMenuHeight : 0; }
	int TitleHeight() const { return mTitleHeight; }
	HWND MenuHost() const { return mBar; }
	const std::vector<RECT>& MenuRects() const { return mButtonRects; }
	void Initialize(HWND window, WindowsUiMenus& menus) {
		mWindow = window; mMenus = &menus; mMenu = mOwnedMenu = ::GetMenu(window);
		WNDCLASSW cls = {}; cls.hInstance = GetModuleHandleW(nullptr); cls.lpfnWndProc = BarProc;
		cls.lpszClassName = L"Q1View.UiMenuHost"; cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
		RegisterClassW(&cls);
		mBar = CreateWindowExW(0, cls.lpszClassName, L"Application menu", WS_CHILD | WS_CLIPCHILDREN,
			0, 0, 0, 0, window, nullptr, cls.hInstance, this);
		RefreshSettings(); Sync();
	}
	void SetMenu(HMENU menu) {
		if (menu) mMenu = menu;
		mMenuVisible = menu != nullptr;
		::SetMenu(mWindow, mCustom ? nullptr : Menu()); Sync();
	}
	void RefreshSettings() {
		const bool custom = mBar && CanCustomize();
		if (custom != mCustom) {
			mCustom = custom;
			::SetMenu(mWindow, mCustom ? nullptr : Menu());
			SetWindowPos(mWindow, nullptr, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
		Layout(); Extend();
	}
	void Sync() {
		if (!Initialized()) return;
		mMenus->Sync(mWindow, mMenu, mCustom);
		const int count = (std::max)(0, GetMenuItemCount(mMenu));
		while (int(mButtons.size()) > count) { DestroyWindow(mButtons.back()); mButtons.pop_back(); }
		while (int(mButtons.size()) < count) {
			const UINT id = UINT(mButtons.size()) + 1;
			HWND button = CreateWindowExW(0, L"BUTTON", L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | BS_OWNERDRAW,
				0, 0, 0, 0, mBar, reinterpret_cast<HMENU>(UINT_PTR(id)), GetModuleHandleW(nullptr), nullptr);
			if (!button) break;
			SetWindowSubclass(button, ButtonProc, 1, reinterpret_cast<DWORD_PTR>(this)); mButtons.push_back(button);
		}
		for (size_t i = 0; i < mButtons.size(); ++i) {
			const auto text = mMenus->Text(mMenu, UINT(i), true);
			wchar_t current[256] = {}; GetWindowTextW(mButtons[i], current, 256);
			if (text != current) SetWindowTextW(mButtons[i], text.c_str());
			EnableWindow(mButtons[i], !(Info(int(i)).fState & (MFS_DISABLED | MFS_GRAYED)));
			InvalidateRect(mButtons[i], nullptr, FALSE);
		}
		Layout();
	}
	void Layout() {
		if (!Initialized()) return;
		RECT client; GetClientRect(mWindow, &client);
		const int width = client.right;
		const UINT dpi = WindowsUiDpi(mWindow); const double scale = WindowsUiSettings().Scale();
		mTitleHeight = (std::max)(Px(32), WindowsUiPixels(20, dpi, scale) + Px(12));
		const int rowHeight = (std::max)(Px(30), WindowsUiPixels(20, dpi, scale) + Px(8));
		mButtonRects.resize(mButtons.size());
		int left = 0, top = 0;
		HDC dc = GetDC(mWindow);
		if (dc) {
			WindowsUiDcState saved(dc); SelectObject(dc, mFonts.Get(WindowsUiFontRole::Body, mWindow));
			for (size_t i = 0; i < mButtons.size(); ++i) {
				auto text = mMenus->Text(mMenu, UINT(i), true);
				RECT measured = {}; DrawTextW(dc, text.c_str(), -1, &measured, DT_SINGLELINE | DT_CALCRECT);
				const int buttonWidth = (std::min)((std::max)(1, width), int(measured.right) + WindowsUiPixels(16, dpi, scale));
				if (left && left + buttonWidth > width) { left = 0; top += rowHeight; }
				mButtonRects[i] = {left, top, left + buttonWidth, top + rowHeight}; left += buttonWidth;
			}
		}
		if (dc) ReleaseDC(mWindow, dc);
		// Align the trailing group (Help, Compare, Update, zoom label) only when
		// it fits on the same row; narrow windows wrap instead of overlapping.
		for (size_t i = 0; i < mButtons.size(); ++i) if (Info(int(i)).fType & MFT_RIGHTJUSTIFY) {
			const int shift = width - left;
			bool sameRow = true;
			for (size_t j = i; j < mButtons.size(); ++j) if (mButtonRects[j].top != mButtonRects[i].top) sameRow = false;
			if (sameRow && shift > 0) for (size_t j = i; j < mButtons.size(); ++j) OffsetRect(&mButtonRects[j], shift, 0);
			break;
		}
		mMenuHeight = mMenuVisible && !mButtons.empty() ? top + rowHeight : 0;
		const bool visible = mCustom && Caption() && mMenuVisible;
		SetWindowPos(mBar, HWND_TOP, 0, mTitleHeight, width, mMenuHeight,
			SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
		for (size_t i = 0; i < mButtons.size(); ++i) {
			const auto& rect = mButtonRects[i];
			MoveWindow(mButtons[i], rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
		}
		RECT title = {0, 0, width, mTitleHeight}; InvalidateRect(mWindow, &title, FALSE);
	}
	void Extend() {
		if (!Initialized()) return;
		MARGINS margins = {}; margins.cyTopHeight = mCustom && Caption() ? mTitleHeight : 0;
		DwmExtendFrameIntoClientArea(mWindow, &margins);
		const COLORREF background = mCustom ? GetSysColor(COLOR_MENUBAR) : 0xffffffff;
		const COLORREF foreground = mCustom ? GetSysColor(COLOR_MENUTEXT) : 0xffffffff;
		DwmSetWindowAttribute(mWindow, DWMWA_CAPTION_COLOR, &background, sizeof(background));
		DwmSetWindowAttribute(mWindow, DWMWA_TEXT_COLOR, &foreground, sizeof(foreground));
	}
	void Paint(HDC target) {
		if (!mCustom || !Caption()) return;
		RECT client; GetClientRect(mWindow, &client);
		if (client.right <= 0 || mTitleHeight <= 0) return;
		// GDI text leaves alpha at zero. In an extended DWM frame that makes
		// glyphs transparent; compose one opaque, buffered title surface.
		BITMAPINFO bitmap = {}; bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmap.bmiHeader.biWidth = client.right; bitmap.bmiHeader.biHeight = -mTitleHeight;
		bitmap.bmiHeader.biPlanes = 1; bitmap.bmiHeader.biBitCount = 32; bitmap.bmiHeader.biCompression = BI_RGB;
		void* pixels = nullptr; HDC dc = CreateCompatibleDC(target);
		HBITMAP surface = CreateDIBSection(target, &bitmap, DIB_RGB_COLORS, &pixels, nullptr, 0);
		if (!dc || !surface) { if (dc) DeleteDC(dc); if (surface) DeleteObject(surface); return; }
		const auto previous = SelectObject(dc, surface);
		const RECT captionButtons = CaptionButtons();
		{
		RECT title = {0, 0, client.right, mTitleHeight};
		WindowsUiDcState saved(dc);
		FillRect(dc, &title, GetSysColorBrush(COLOR_MENUBAR));
		const int right = captionButtons.left;
		HICON icon = reinterpret_cast<HICON>(SendMessage(mWindow, WM_GETICON, ICON_SMALL2, 0));
		if (!icon) icon = reinterpret_cast<HICON>(GetClassLongPtr(mWindow, GCLP_HICONSM));
		if (!icon) icon = reinterpret_cast<HICON>(GetClassLongPtr(mWindow, GCLP_HICON));
		if (icon) DrawIconEx(dc, Px(10), (mTitleHeight - Px(20)) / 2, icon, Px(20), Px(20), 0, nullptr, DI_NORMAL);
		title.left = Px(38); title.right = (std::max)(title.left, LONG(right - Px(8)));
		SelectObject(dc, mFonts.Get(WindowsUiFontRole::Body, mWindow));
		SetBkMode(dc, TRANSPARENT); SetTextColor(dc, GetSysColor(GetForegroundWindow() == mWindow ? COLOR_MENUTEXT : COLOR_GRAYTEXT));
		std::vector<wchar_t> text(GetWindowTextLengthW(mWindow) + 1);
		GetWindowTextW(mWindow, text.data(), int(text.size()));
		DrawTextW(dc, text.data(), -1, &title, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
		}
		for (int y = 0; y < mTitleHeight; ++y) for (int x = 0; x < client.right; ++x) {
			auto& pixel = static_cast<DWORD*>(pixels)[size_t(y) * client.right + x];
			pixel = x >= captionButtons.left && y >= captionButtons.top && y < captionButtons.bottom ? 0 : pixel | 0xff000000;
		}
		BitBlt(target, 0, 0, client.right, mTitleHeight, dc, 0, 0, SRCCOPY);
		SelectObject(dc, previous); DeleteObject(surface); DeleteDC(dc);
	}
	bool Before(UINT message, WPARAM wp, LPARAM lp, LRESULT& result) {
		if (!Initialized()) return false;
		if (message == WM_EXITSIZEMOVE && mCustom) PostMessageW(mWindow, WM_REPAINT_FRAME, 0, 0);
		if (message == WM_WINDOWPOSCHANGING && mCustom) {
			// Native bit-copy preservation assumes the standard client origin.
			// Our client title changes that origin: invalidate instead of copying
			// stale child pixels during moves as well as size changes.
			auto* position = reinterpret_cast<WINDOWPOS*>(lp);
			if (!(position->flags & SWP_NOMOVE) || !(position->flags & SWP_NOSIZE)) position->flags |= SWP_NOCOPYBITS;
		}
		if (mTracking && (message == WM_CLOSE || (message == WM_SYSCOMMAND && (wp & 0xfff0) == SC_CLOSE))) {
			// Finish popup tracking/unhooking before MFC deletes the owner frame.
			mDeferredClose = true; mNext = -1; EndMenu(); result = 0; return true;
		}
		if (message == WM_OPEN_MENU) { Open(int(wp), mKeyboard); result = 0; return true; }
		if (message == WM_MENUSELECT && mTracking) { mSelected = LOWORD(wp); mSelectedFlags = HIWORD(wp); mSelectedMenu = reinterpret_cast<HMENU>(lp); }
		if (!mCustom || !Caption()) return false;
		if (message == WM_NCCALCSIZE) {
			RECT* rect = wp ? &reinterpret_cast<NCCALCSIZE_PARAMS*>(lp)->rgrc[0] : reinterpret_cast<RECT*>(lp);
			const UINT dpi = WindowsUiDpi(mWindow);
			const int x = Metric(SM_CXSIZEFRAME, dpi) + Metric(SM_CXPADDEDBORDER, dpi);
			const int y = Metric(SM_CYSIZEFRAME, dpi) + Metric(SM_CXPADDEDBORDER, dpi);
			rect->left += x; rect->right -= x; rect->bottom -= y; if (IsZoomed(mWindow)) rect->top += y;
			result = wp ? WVR_REDRAW : 0; return true;
		}
		LRESULT dwm = 0;
		if (DwmDefWindowProc(mWindow, message, wp, lp, &dwm) &&
			(message != WM_NCHITTEST || dwm == HTMINBUTTON || dwm == HTMAXBUTTON || dwm == HTCLOSE)) { result = dwm; return true; }
		if (message == WM_NCHITTEST) {
			POINT screen = {GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
			RECT bounds; GetWindowRect(mWindow, &bounds);
			const UINT dpi = WindowsUiDpi(mWindow);
			const int x = Metric(SM_CXSIZEFRAME, dpi) + Metric(SM_CXPADDEDBORDER, dpi);
			const int y = Metric(SM_CYSIZEFRAME, dpi) + Metric(SM_CXPADDEDBORDER, dpi);
			const bool left = screen.x < bounds.left + x, right = screen.x >= bounds.right - x;
			const bool top = screen.y < bounds.top + y, bottom = screen.y >= bounds.bottom - y;
			if (!IsZoomed(mWindow) && (left || right || top || bottom)) {
				result = top ? (left ? HTTOPLEFT : right ? HTTOPRIGHT : HTTOP) :
					bottom ? (left ? HTBOTTOMLEFT : right ? HTBOTTOMRIGHT : HTBOTTOM) : left ? HTLEFT : HTRIGHT;
				return true;
			}
			POINT point = screen; ScreenToClient(mWindow, &point);
			const RECT buttons = CaptionButtons();
			if (PtInRect(&buttons, point)) {
				// DWM may not have initialized its own hit-test state yet (e.g.
				// immediately after a frame/DPI change). Retain the native hit
				// codes rather than turning a visible caption button into a drag.
				const int buttonWidth = (std::max)(1L, (buttons.right - buttons.left) / 3);
				result = point.x >= buttons.right - buttonWidth ? HTCLOSE :
					point.x >= buttons.right - 2 * buttonWidth ? HTMAXBUTTON : HTMINBUTTON;
				return true;
			}
			result = point.y < mTitleHeight ? (point.x < Px(34) ? HTSYSMENU : HTCAPTION) : HTCLIENT;
			return true;
		}
		if (message == WM_PAINT) { PAINTSTRUCT paint; HDC dc = BeginPaint(mWindow, &paint); Paint(dc); EndPaint(mWindow, &paint); result = 0; return true; }
		if (message == WM_ERASEBKGND) { result = TRUE; return true; }
		if (message == WM_SYSCOMMAND && (wp & 0xfff0) == SC_KEYMENU && lp != L' ') {
			mKeyboard = true; RememberFocus(GetFocus()); const int index = NextEnabled(-1, 1);
			if (index >= 0) SetFocus(mButtons[index]); result = 0; return true;
		}
		return false;
	}
	void After(UINT message) {
		if (!Initialized()) return;
		if (message == WM_REPAINT_FRAME && mCustom) {
			RedrawWindow(mWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW | RDW_FRAME);
			for (HWND button : mButtons)
				RedrawWindow(button, nullptr, nullptr, RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
		}
		if (message == WM_SIZE || message == WM_STYLECHANGED) { Layout(); Extend(); }
		if (message == WM_ACTIVATE || message == WM_SETTEXT) { Extend(); RECT rect; GetClientRect(mWindow, &rect); rect.bottom = mTitleHeight; InvalidateRect(mWindow, &rect, FALSE); }
		if (message == WM_SETTINGCHANGE || message == WM_THEMECHANGED || message == WM_DWMCOMPOSITIONCHANGED) { RefreshSettings(); Sync(); }
	}
	bool Translate(MSG* message) {
		if (!mCustom || !Caption() || !message || mTracking) return false;
		if ((message->message == WM_SYSCHAR && message->wParam != L' ') ||
			(message->message == WM_CHAR && mKeyboard && IsButton(GetFocus()))) {
			LRESULT match;
			if (mMenus->MenuChar(UINT(message->wParam), mMenu, match)) {
				RememberFocus(GetFocus()); mKeyboard = true; const int index = LOWORD(match);
				SetFocus(mButtons[index]); Open(index, true); return true;
			}
		}
		if (message->message != WM_KEYDOWN) return false;
		if (message->wParam == VK_F10 && !(GetKeyState(VK_SHIFT) & 0x8000)) {
			if (mKeyboard) RestoreFocus(); else { RememberFocus(GetFocus()); mKeyboard = true; const int index = NextEnabled(-1, 1); if (index >= 0) SetFocus(mButtons[index]); }
			return true;
		}
		if (!IsButton(GetFocus())) return false;
		const int current = int(std::find(mButtons.begin(), mButtons.end(), GetFocus()) - mButtons.begin());
		switch (message->wParam) {
		case VK_ESCAPE: RestoreFocus(); return true;
		case VK_LEFT: case VK_RIGHT: { const int index = NextEnabled(current, message->wParam == VK_LEFT ? -1 : 1); if (index >= 0) SetFocus(mButtons[index]); return true; }
		case VK_RETURN: case VK_DOWN: case VK_SPACE: Open(current, true); return true;
		default: break;
		}
		return false;
	}
};
} // namespace q1view
