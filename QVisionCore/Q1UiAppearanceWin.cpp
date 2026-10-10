// Keep WinRT headers separate from MFC/GDI+ renderer translation units.
#include "Q1UiAppearanceWin.h"
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.ui.viewmanagement.h>

#pragma comment(lib, "runtimeobject.lib")

namespace q1view {
WindowsUiAppearance ValidWindowsUiAppearance(int value)
{ return value >= 0 && value <= 2 ? static_cast<WindowsUiAppearance>(value) : WindowsUiAppearance::System; }
bool ResolveWindowsUiDark(WindowsUiAppearance choice, bool systemDark)
{ return choice == WindowsUiAppearance::Dark || (choice == WindowsUiAppearance::System && systemDark); }

WindowsUiPalette ResolveWindowsUiPalette(bool dark)
{
	WindowsUiPalette colors = dark ? WindowsUiPalette{
		RGB(24,24,24), RGB(32,32,32), RGB(38,38,38), RGB(48,48,48), RGB(56,56,56),
		RGB(69,69,69), RGB(119,129,142), RGB(242,242,242), RGB(183,190,199),
		RGB(138,180,255), RGB(24,24,24), RGB(38,54,80), RGB(234,196,106),
		RGB(255,154,164), RGB(125,217,170), RGB(24,24,24), RGB(242,242,242), 0,0,0
	} : WindowsUiPalette{
		RGB(243,244,246), RGB(255,255,255), RGB(247,248,250), RGB(238,240,243), RGB(228,231,236),
		RGB(215,218,223), RGB(123,132,144), RGB(32,33,36), RGB(88,97,110),
		RGB(37,102,217), RGB(255,255,255), RGB(234,241,255), RGB(135,90,0),
		RGB(180,35,50), RGB(19,109,69), RGB(247,248,250), RGB(32,33,36), 0,0,0
	};
	// One theme controls chrome and the exposed image surround together.
	colors[size_t(WindowsUiColor::Canvas)] = dark ? RGB(24,24,24) : RGB(236,236,236);
	colors[size_t(WindowsUiColor::CanvasText)] = colors[size_t(WindowsUiColor::Text)];
	colors[size_t(WindowsUiColor::CanvasMuted)] = colors[size_t(WindowsUiColor::Muted)];
	return colors;
}

int WindowsUiSystemColor(WindowsUiColor role)
{
	switch (role) {
	case WindowsUiColor::Text: case WindowsUiColor::CanvasText: case WindowsUiColor::OverlayText: return COLOR_WINDOWTEXT;
	case WindowsUiColor::Muted: case WindowsUiColor::CanvasMuted: return COLOR_WINDOWTEXT;
	case WindowsUiColor::Accent: case WindowsUiColor::Selection: case WindowsUiColor::Hover:
	case WindowsUiColor::Pressed: return COLOR_HIGHLIGHT;
	case WindowsUiColor::OnAccent: return COLOR_HIGHLIGHTTEXT;
	case WindowsUiColor::Border: case WindowsUiColor::Boundary: return COLOR_WINDOWTEXT;
	case WindowsUiColor::Warning: case WindowsUiColor::Danger: case WindowsUiColor::Success: return COLOR_WINDOWTEXT;
	default: return COLOR_WINDOW;
	}
}

namespace {
struct AppearanceState {
	WindowsUiAppearance appearance = WindowsUiAppearance::System;
	bool systemDark = false, highContrast = false, uninitialize = false, subscribed = false;
	Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IUISettings3> settings;
	EventRegistrationToken token = {};
	// Immutable role palettes for native menu backgrounds, bounded for the
	// process lifetime. Never delete a brush while a native popup references it.
	HBRUSH brushes[2][size_t(WindowsUiColor::Count)] = {};
	AppearanceState() {
		uninitialize = SUCCEEDED(RoInitialize(RO_INIT_SINGLETHREADED));
		Microsoft::WRL::ComPtr<IInspectable> instance;
		if (FAILED(RoActivateInstance(Microsoft::WRL::Wrappers::HStringReference(
			RuntimeClass_Windows_UI_ViewManagement_UISettings).Get(), &instance)) || FAILED(instance.As(&settings))) return;
		const DWORD uiThread = GetCurrentThreadId();
		using Handler = ABI::Windows::Foundation::ITypedEventHandler<ABI::Windows::UI::ViewManagement::UISettings*, IInspectable*>;
		auto handler = Microsoft::WRL::Callback<Handler>([uiThread](ABI::Windows::UI::ViewManagement::IUISettings*, IInspectable*) -> HRESULT {
			EnumThreadWindows(uiThread, [](HWND window, LPARAM) -> BOOL {
				PostMessageW(window, WM_UI_APPEARANCE_CHANGED, 0, 0); return TRUE;
			}, 0);
			return S_OK;
		});
		subscribed = SUCCEEDED(settings->add_ColorValuesChanged(handler.Get(), &token));
	}
	~AppearanceState() {
		if (subscribed) settings->remove_ColorValuesChanged(token);
		settings.Reset();
		for (auto& palette : brushes) for (auto brush : palette) if (brush) DeleteObject(brush);
		if (uninitialize) RoUninitialize();
	}
};
AppearanceState& State() { static AppearanceState state; return state; }
}

void WindowsUiAppearanceSettings::Initialize(int appearance)
{ State().appearance = ValidWindowsUiAppearance(appearance); RefreshSystem(); }
void WindowsUiAppearanceSettings::RefreshSystem()
{
	auto& state = State();
	HIGHCONTRASTW contrast = {sizeof(contrast)};
	state.highContrast = SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) && (contrast.dwFlags & HCF_HIGHCONTRASTON);
	ABI::Windows::UI::Color foreground = {};
	if (state.settings && SUCCEEDED(state.settings->GetColorValue(ABI::Windows::UI::ViewManagement::UIColorType_Foreground, &foreground)))
		state.systemDark = 5 * foreground.G + 2 * foreground.R + foreground.B > 8 * 128;
}
void WindowsUiAppearanceSettings::SetAppearance(WindowsUiAppearance choice) { State().appearance = ValidWindowsUiAppearance(int(choice)); }
WindowsUiAppearance WindowsUiAppearanceSettings::Appearance() const { return State().appearance; }
bool WindowsUiAppearanceSettings::Dark() const { return !HighContrast() && ResolveWindowsUiDark(Appearance(), State().systemDark); }
bool WindowsUiAppearanceSettings::HighContrast() const { return State().highContrast; }
COLORREF WindowsUiAppearanceSettings::Color(WindowsUiColor role) const
{ return HighContrast() ? GetSysColor(WindowsUiSystemColor(role)) : ResolveWindowsUiPalette(Dark())[size_t(role)]; }
HBRUSH WindowsUiAppearanceSettings::Brush(WindowsUiColor role) const
{
	if (HighContrast()) return GetSysColorBrush(WindowsUiSystemColor(role));
	auto& brush = State().brushes[Dark() ? 1 : 0][size_t(role)];
	if (!brush) brush = CreateSolidBrush(Color(role));
	return brush;
}
WindowsUiAppearanceSettings& WindowsUiAppearanceState() { static WindowsUiAppearanceSettings settings; return settings; }
} // namespace q1view
