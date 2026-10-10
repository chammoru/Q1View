#pragma once
#include "Q1UiAppearanceWin.h"

namespace q1view {
constexpr UINT ID_UI_APPEARANCE_SYSTEM = 0x7800;
constexpr UINT ID_UI_APPEARANCE_LIGHT = 0x7801;
constexpr UINT ID_UI_APPEARANCE_DARK = 0x7802;

inline void AppendWindowsUiThemeMenu(HMENU parent)
{
	HMENU appearance = CreatePopupMenu();
	if (!parent || !appearance) {
		if (appearance) DestroyMenu(appearance);
		return;
	}
	AppendMenuW(appearance, MF_STRING, ID_UI_APPEARANCE_SYSTEM, L"&System");
	AppendMenuW(appearance, MF_STRING, ID_UI_APPEARANCE_LIGHT, L"&Light");
	AppendMenuW(appearance, MF_STRING, ID_UI_APPEARANCE_DARK, L"&Dark");
	if (GetMenuItemCount(parent)>0) AppendMenuW(parent, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(parent, MF_POPUP, reinterpret_cast<UINT_PTR>(appearance), L"&Theme");
}
template<class App> void LoadWindowsUiAppearancePreferences(App& app)
{
	// Retain the existing theme key; obsolete independent Canvas values are
	// intentionally ignored, not deleted or used to override the chosen theme.
	WindowsUiAppearanceState().Initialize(app.GetProfileInt(L"Appearance", L"Theme", 0));
}
template<class App> void SelectWindowsUiAppearanceCommand(App& app, UINT command)
{
	if (command < ID_UI_APPEARANCE_SYSTEM || command > ID_UI_APPEARANCE_DARK) return;
	const int choice = int(command - ID_UI_APPEARANCE_SYSTEM);
	WindowsUiAppearanceState().SetAppearance(ValidWindowsUiAppearance(choice));
	app.WriteProfileInt(L"Appearance", L"Theme", choice);
}
inline bool WindowsUiAppearanceCommandChecked(UINT command)
{
	return command >= ID_UI_APPEARANCE_SYSTEM && command <= ID_UI_APPEARANCE_DARK &&
		int(command - ID_UI_APPEARANCE_SYSTEM) == int(WindowsUiAppearanceState().Appearance());
}
} // namespace q1view
