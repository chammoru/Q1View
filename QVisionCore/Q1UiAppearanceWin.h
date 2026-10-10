#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <array>
#include <cstring>

namespace q1view {

constexpr UINT WM_UI_APPEARANCE_CHANGED = WM_APP + 118;
enum class WindowsUiAppearance { System, Light, Dark };
enum class WindowsUiCanvas { Neutral, Dark, Light };
enum class WindowsUiColor {
	Window, Surface, SurfaceAlt, Hover, Pressed, Border, Boundary, Text, Muted,
	Accent, OnAccent, Selection, Warning, Danger, Success, Overlay, OverlayText,
	Canvas, CanvasText, CanvasMuted, Count
};

using WindowsUiPalette = std::array<COLORREF, static_cast<size_t>(WindowsUiColor::Count)>;
WindowsUiAppearance ValidWindowsUiAppearance(int value);
WindowsUiCanvas ValidWindowsUiCanvas(int value);
bool ResolveWindowsUiDark(WindowsUiAppearance choice, bool systemDark);
WindowsUiPalette ResolveWindowsUiPalette(bool dark, WindowsUiCanvas canvas);
int WindowsUiSystemColor(WindowsUiColor role);

// Owned by the UI thread. No file/decoder/render/metric dependencies. WinRT
// callbacks only post a message; system state is read on the receiving thread.
class WindowsUiAppearanceSettings {
public:
	void Initialize(int appearance, int canvas);
	void RefreshSystem();
	void SetAppearance(WindowsUiAppearance choice);
	void SetCanvas(WindowsUiCanvas choice);
	WindowsUiAppearance Appearance() const;
	WindowsUiCanvas Canvas() const;
	bool Dark() const;
	bool HighContrast() const;
	COLORREF Color(WindowsUiColor role) const;
	HBRUSH Brush(WindowsUiColor role) const;
};
WindowsUiAppearanceSettings& WindowsUiAppearanceState();
inline COLORREF WindowsUiColorValue(WindowsUiColor role) { return WindowsUiAppearanceState().Color(role); }
inline HBRUSH WindowsUiColorBrush(WindowsUiColor role) { return WindowsUiAppearanceState().Brush(role); }

inline void FillWindowsUiCanvasBgr(BYTE* pixels, size_t bytes)
{
	const COLORREF color = WindowsUiColorValue(WindowsUiColor::Canvas);
	const BYTE r = GetRValue(color), g = GetGValue(color), b = GetBValue(color);
	if (r == g && g == b) { std::memset(pixels, r, bytes); return; }
	for (size_t i = 0; i + 2 < bytes; i += 3) { pixels[i] = b; pixels[i+1] = g; pixels[i+2] = r; }
}

} // namespace q1view
