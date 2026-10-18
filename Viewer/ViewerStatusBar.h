#pragma once

#include "Q1UiBufferedPaintWin.h"
#include "Q1UiFontWin.h"
#include <commctrl.h>

// A native read-only Static (MSAA/UIA name is the full text), not a disabled
// command. A tooltip preserves full information if the single line ellipsizes.
class ViewerStatusBar {
	HWND mWindow = nullptr, mTip = nullptr;
	q1view::WindowsUiFontCache mFonts;
	std::wstring mText;
	std::wstring mSampling, mZoom;
	static LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
		auto* self = reinterpret_cast<ViewerStatusBar*>(data);
		if (message == WM_ERASEBKGND) return TRUE;
		if (message == WM_PAINT) {
			PAINTSTRUCT paint; HDC dc = BeginPaint(window, &paint);
			RECT rect; GetClientRect(window, &rect);
			{
			q1view::WindowsUiBufferedPaint buffer(dc, rect, q1view::WindowsUiColor::Surface);
			{
				q1view::WindowsUiDcState saved(buffer.Dc());
				SelectObject(buffer.Dc(), self->mFonts.Get(q1view::WindowsUiFontRole::Supporting, window));
				SetBkMode(buffer.Dc(), TRANSPARENT);
				SetTextColor(buffer.Dc(), q1view::WindowsUiColorValue(q1view::WindowsUiColor::Muted));
				RECT edge = rect; edge.bottom = 1;
				FillRect(buffer.Dc(), &edge, q1view::WindowsUiColorBrush(q1view::WindowsUiColor::Border));
				RECT sampling, zoom;
				self->TextRects(buffer.Dc(), rect, sampling, zoom);
				DrawTextW(buffer.Dc(), self->mSampling.c_str(), -1, &sampling, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
				DrawTextW(buffer.Dc(), self->mZoom.c_str(), -1, &zoom, DT_SINGLELINE | DT_VCENTER | DT_RIGHT | DT_END_ELLIPSIS | DT_NOPREFIX);
			}
			}
			EndPaint(window, &paint); return 0;
		}
		if (message == WM_NCDESTROY) { self->mWindow = nullptr; self->mTip = nullptr; RemoveWindowSubclass(window, Proc, 1); }
		return DefSubclassProc(window, message, wp, lp);
	}
public:
	bool Initialize(HWND parent) {
		mWindow = CreateWindowExW(0, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT | SS_NOPREFIX,
			0, 0, 0, 0, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
		if (!mWindow) return false;
		SetWindowSubclass(mWindow, Proc, 1, reinterpret_cast<DWORD_PTR>(this));
		mTip = CreateWindowExW(WS_EX_TOPMOST, TOOLTIPS_CLASSW, nullptr, WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, mWindow, nullptr, GetModuleHandleW(nullptr), nullptr);
		if (mTip) {
			TOOLINFOW tool = {sizeof(tool)}; tool.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
			tool.hwnd = mWindow; tool.uId = reinterpret_cast<UINT_PTR>(mWindow); tool.lpszText = const_cast<wchar_t*>(L"");
			SendMessageW(mTip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&tool));
			SendMessageW(mTip, TTM_SETMAXTIPWIDTH, 0, 900);
		}
		return true;
	}
	HWND Window() const { return mWindow; }
	const std::wstring& Text() const { return mText; }
	const std::wstring& SamplingText() const { return mSampling; }
	const std::wstring& ZoomText() const { return mZoom; }
	// The caller selects the same status font used by painting. Keep zoom at
	// the right edge and let the left field ellipsize before the fields overlap.
	void TextRects(HDC dc, RECT rect, RECT& sampling, RECT& zoom) const {
		const int padding = q1view::WindowsUiPixels(8, q1view::WindowsUiDpi(mWindow));
		rect.left = (std::min)(rect.right, rect.left + padding);
		rect.right = (std::max)(rect.left, rect.right - padding);
		SIZE size = {}; GetTextExtentPoint32W(dc, mZoom.c_str(), int(mZoom.size()), &size);
		zoom = rect; zoom.left = (std::max)(rect.left, rect.right - size.cx);
		sampling = rect; sampling.right = (std::max)(rect.left, zoom.left - padding);
	}
	int Height() const {
		if (!mWindow) return 0;
		const auto dpi = q1view::WindowsUiDpi(mWindow);
		return (std::max)(q1view::WindowsUiPixels(24, dpi),
			q1view::WindowsUiPixels(16, dpi, q1view::WindowsUiSettings().Scale()) + q1view::WindowsUiPixels(8, dpi));
	}
	void Layout(HWND parent, bool visible) {
		if (!mWindow) return;
		RECT rect; GetClientRect(parent, &rect);
		SetWindowPos(mWindow, HWND_TOP, 0, (std::max)(0L, rect.bottom - Height()), rect.right, Height(),
			SWP_NOACTIVATE | (visible ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
	}
	void SetText(const std::wstring& sampling, const std::wstring& zoom) {
		if ((mSampling == sampling && mZoom == zoom) || !mWindow) return;
		mSampling = sampling; mZoom = zoom;
		mText = sampling.empty() ? zoom : sampling + L"  |  " + zoom;
		SetWindowTextW(mWindow, mText.c_str());
		if (mTip) {
			TOOLINFOW tool = {sizeof(tool)}; tool.hwnd = mWindow;
			tool.uId = reinterpret_cast<UINT_PTR>(mWindow); tool.lpszText = const_cast<wchar_t*>(mText.c_str());
			SendMessageW(mTip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&tool));
		}
		InvalidateRect(mWindow, nullptr, FALSE);
	}
};
