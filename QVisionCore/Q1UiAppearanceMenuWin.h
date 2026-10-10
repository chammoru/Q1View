#pragma once
#include "Q1UiAppearanceWin.h"

namespace q1view {
constexpr UINT ID_UI_APPEARANCE_SYSTEM = 0x7800;
constexpr UINT ID_UI_APPEARANCE_LIGHT = 0x7801;
constexpr UINT ID_UI_APPEARANCE_DARK = 0x7802;
constexpr UINT ID_UI_CANVAS_NEUTRAL = 0x7803;
constexpr UINT ID_UI_CANVAS_DARK = 0x7804;
constexpr UINT ID_UI_CANVAS_LIGHT = 0x7805;

inline void AppendWindowsUiAppearanceMenus(HMENU parent)
{
	HMENU appearance = CreatePopupMenu(), canvas = CreatePopupMenu();
	if (!parent || !appearance || !canvas) {
		if (appearance) DestroyMenu(appearance);
		if (canvas) DestroyMenu(canvas);
		return;
	}
	AppendMenuW(appearance, MF_STRING, ID_UI_APPEARANCE_SYSTEM, L"&System");
	AppendMenuW(appearance, MF_STRING, ID_UI_APPEARANCE_LIGHT, L"&Light");
	AppendMenuW(appearance, MF_STRING, ID_UI_APPEARANCE_DARK, L"&Dark");
	AppendMenuW(canvas, MF_STRING, ID_UI_CANVAS_NEUTRAL, L"&Neutral gray");
	AppendMenuW(canvas, MF_STRING, ID_UI_CANVAS_DARK, L"&Dark gray");
	AppendMenuW(canvas, MF_STRING, ID_UI_CANVAS_LIGHT, L"&Light gray");
	AppendMenuW(parent, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(parent, MF_POPUP, reinterpret_cast<UINT_PTR>(appearance), L"&Appearance");
	AppendMenuW(parent, MF_POPUP, reinterpret_cast<UINT_PTR>(canvas), L"Image &background");
}
template<class App> void LoadWindowsUiAppearancePreferences(App& app)
{
	WindowsUiAppearanceState().Initialize(app.GetProfileInt(L"Appearance", L"Theme", 0),
		app.GetProfileInt(L"Appearance", L"Canvas", 0));
}
template<class App> void SelectWindowsUiAppearanceCommand(App& app, UINT command)
{
	if (command < ID_UI_APPEARANCE_SYSTEM || command > ID_UI_CANVAS_LIGHT) return;
	if (command <= ID_UI_APPEARANCE_DARK) {
		const int choice = int(command - ID_UI_APPEARANCE_SYSTEM);
		WindowsUiAppearanceState().SetAppearance(ValidWindowsUiAppearance(choice));
		app.WriteProfileInt(L"Appearance", L"Theme", choice);
	} else {
		const int choice = int(command - ID_UI_CANVAS_NEUTRAL);
		WindowsUiAppearanceState().SetCanvas(ValidWindowsUiCanvas(choice));
		app.WriteProfileInt(L"Appearance", L"Canvas", choice);
	}
}
inline bool WindowsUiAppearanceCommandChecked(UINT command)
{
	if (command < ID_UI_APPEARANCE_SYSTEM || command > ID_UI_CANVAS_LIGHT) return false;
	return command <= ID_UI_APPEARANCE_DARK ? int(command - ID_UI_APPEARANCE_SYSTEM) == int(WindowsUiAppearanceState().Appearance()) :
		int(command - ID_UI_CANVAS_NEUTRAL) == int(WindowsUiAppearanceState().Canvas());
}
} // namespace q1view
