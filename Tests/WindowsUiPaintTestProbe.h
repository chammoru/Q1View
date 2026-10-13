#pragma once

#include "../QVisionCore/Q1UiAppearanceWin.h"
#include <stdexcept>

// In-process probes of the application's real HWNDs; no external UI controller,
// desktop capture, OS setting changes, or writes to production preferences.
class WindowsUiPaintTestSurface {
	HDC mDc = nullptr;
	HBITMAP mBitmap = nullptr;
	HGDIOBJ mPrevious = nullptr;
public:
	RECT rect = {};
	explicit WindowsUiPaintTestSurface(HWND window) {
		GetClientRect(window, &rect);
		if (rect.right < 5 || rect.bottom < 5) throw std::runtime_error("paint probe requires visible-sized bounds");
		HDC source = GetDC(window);
		mDc = CreateCompatibleDC(source);
		mBitmap = mDc ? CreateCompatibleBitmap(source, rect.right, rect.bottom) : nullptr;
		ReleaseDC(window, source);
		if (!mBitmap) { if (mDc) DeleteDC(mDc); throw std::runtime_error("paint probe allocation failed"); }
		mPrevious = SelectObject(mDc, mBitmap);
	}
	~WindowsUiPaintTestSurface() { SelectObject(mDc,mPrevious); DeleteObject(mBitmap); DeleteDC(mDc); }
	WindowsUiPaintTestSurface(const WindowsUiPaintTestSurface&) = delete;
	WindowsUiPaintTestSurface& operator=(const WindowsUiPaintTestSurface&) = delete;
	HDC Dc() const { return mDc; }
	void Fill(COLORREF color) {
		HBRUSH brush = CreateSolidBrush(color); FillRect(mDc,&rect,brush); DeleteObject(brush);
	}
	COLORREF Corner() const { GdiFlush(); return GetPixel(mDc,2,rect.bottom-3); }
};

class ScopedWindowsUiPaintTestTheme {
	q1view::WindowsUiAppearance mSaved = q1view::WindowsUiAppearanceState().Appearance();
public:
	~ScopedWindowsUiPaintTestTheme() { q1view::WindowsUiAppearanceState().SetAppearance(mSaved); }
};

template<class Check>
void CheckWindowsUiErase(HWND window, q1view::WindowsUiColor role, bool fillsBackground, Check check)
{
	WindowsUiPaintTestSurface surface(window);
	const COLORREF sentinel = RGB(17,34,51);
	surface.Fill(sentinel);
	const auto handled = SendMessageW(window,WM_ERASEBKGND,reinterpret_cast<WPARAM>(surface.Dc()),0);
	check(handled != 0,"custom window owns its background erase");
	check(surface.Corner() == (fillsBackground ? q1view::WindowsUiColorValue(role) : sentinel),
		"erase phase uses its theme role or retains pixels, never a default system brush");
}

template<class Check>
void CheckWindowsUiOwnerDraw(HWND window, Check check)
{
	using namespace q1view;
	WindowsUiPaintTestSurface surface(window);
	HWND parent = GetParent(window);
	wchar_t cls[32] = {}; GetClassNameW(window,cls,_countof(cls));
	const bool label = _wcsicmp(cls,L"Static") == 0;
	const UINT message = label ? WM_CTLCOLORSTATIC : WM_CTLCOLORBTN;
	const auto brush = reinterpret_cast<HBRUSH>(SendMessageW(parent,message,
		reinterpret_cast<WPARAM>(surface.Dc()),reinterpret_cast<LPARAM>(window)));
	LOGBRUSH logical = {};
	check(brush && GetObjectW(brush,sizeof(logical),&logical) && logical.lbColor == WindowsUiColorValue(WindowsUiColor::SurfaceAlt),
		"native owner-drawn control receives a themed parent background brush");
	CheckWindowsUiErase(window,WindowsUiColor::SurfaceAlt,false,check);
	DRAWITEMSTRUCT draw = {}; draw.CtlType = label ? ODT_STATIC : ODT_BUTTON;
	draw.CtlID = GetDlgCtrlID(window); draw.hwndItem = window; draw.hDC = surface.Dc();
	draw.rcItem = surface.rect; draw.itemAction = ODA_DRAWENTIRE;
	for (UINT state : {0u,UINT(ODS_FOCUS),UINT(ODS_SELECTED),UINT(ODS_DISABLED)}) {
		draw.itemState = state;
		SendMessageW(parent,WM_DRAWITEM,draw.CtlID,reinterpret_cast<LPARAM>(&draw));
		check(surface.Corner() == WindowsUiColorValue(WindowsUiColor::SurfaceAlt),
			"normal, focused, pressed and disabled owner drawing fills the control with its theme background");
	}
	const DWORD before = GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
	for (int i=0;i<200;++i) SendMessageW(parent,WM_DRAWITEM,draw.CtlID,reinterpret_cast<LPARAM>(&draw));
	check(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS) == before,
		"repeated owner-drawn control painting releases its temporary GDI resources");
}
