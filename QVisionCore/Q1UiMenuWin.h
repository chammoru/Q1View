#pragma once

#include "Q1UiFontWin.h"
#include <oleacc.h>
#include <memory>
#include <vector>
#include <cwctype>

namespace q1view {

// Keep real HMENUs and Windows menu tracking. Only measurement/painting change.
// MSAA metadata must be first so native accessibility exposes each item name.
class WindowsUiMenus {
	struct Item {
		MSAAMENUINFO accessible = {MSAA_MENU_SIG, 0, nullptr};
		HMENU menu = nullptr;
		UINT position = 0, type = 0;
		bool bar = false;
		std::wstring text;
	};
	std::vector<std::unique_ptr<Item>> mItems;
	WindowsUiFontCache mFonts;
	WindowsUiFontCache mBarFonts;
	HWND mWindow = nullptr;
	bool mHostedBar = false;
	Item* Find(ULONG_PTR data) const {
		for (const auto& item : mItems) if (reinterpret_cast<ULONG_PTR>(item.get()) == data) return item.get();
		return nullptr;
	}
	int Px(int value) const { return WindowsUiPixels(value, WindowsUiDpi(mWindow), WindowsUiSettings().Scale()); }
	HFONT Font(const Item* item) {
		const UINT dpi = WindowsUiDpi(mWindow);
		double scale = WindowsUiSettings().Scale();
		if (!item->bar || mHostedBar) return mFonts.Get(WindowsUiFontRole::Body, dpi, scale);
		// Windows, not WM_MEASUREITEM, ultimately owns the native bar's row
		// height. Fit within that hit-tested row instead of clipping enlarged
		// glyphs or changing global NONCLIENTMETRICS for other applications.
		// Do not query menu-item rectangles while Windows is measuring/drawing
		// that menu: the query can re-enter native menu layout during creation.
		using MetricForDpi = int(WINAPI*)(int, UINT);
		static const auto metric = reinterpret_cast<MetricForDpi>(
			GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetSystemMetricsForDpi"));
		const int height = metric ? metric(SM_CYMENU, dpi) : GetSystemMetrics(SM_CYMENU);
		scale = (std::min)(scale, (std::max)(1.0, double(height - WindowsUiPixels(4, dpi)) / WindowsUiPixels(18, dpi)));
		return mBarFonts.Get(WindowsUiFontRole::Body, dpi, scale);
	}
	static std::wstring ReadText(HMENU menu, UINT position) {
		MENUITEMINFOW info = {sizeof(info)}; info.fMask = MIIM_STRING;
		if (!GetMenuItemInfoW(menu, position, TRUE, &info)) return {};
		std::vector<wchar_t> buffer(info.cch + 1);
		info.dwTypeData = buffer.data(); info.cch = UINT(buffer.size());
		if (!GetMenuItemInfoW(menu, position, TRUE, &info)) return {};
		return buffer.data();
	}
	static std::wstring VisibleText(const std::wstring& text) {
		std::wstring result;
		for (size_t i = 0; i < text.size(); ++i) {
			if (text[i] != L'&') result += text[i];
			else if (i + 1 < text.size() && text[i + 1] == L'&') { result += L'&'; ++i; }
		}
		return result;
	}
	static bool Attached(HMENU menu, const Item* item) {
		for (int i = 0; i < GetMenuItemCount(menu); ++i) {
			MENUITEMINFOW info = {sizeof(info)}; info.fMask = MIIM_DATA;
			if (GetMenuItemInfoW(menu, i, TRUE, &info) && info.dwItemData == reinterpret_cast<ULONG_PTR>(item)) return true;
		}
		return false;
	}
	void SyncMenu(HMENU menu, bool bar) {
		for (int position = 0; position < GetMenuItemCount(menu); ++position) {
			MENUITEMINFOW info = {sizeof(info)};
			info.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_SUBMENU;
			if (!GetMenuItemInfoW(menu, position, TRUE, &info)) continue;
			Item* item = Find(info.dwItemData);
			// Do not take ownership of unrelated custom menu drawing/data.
			if ((info.fType & MFT_OWNERDRAW) && !item) continue;
			const auto text = ReadText(menu, UINT(position));
			if (!item) {
				for (auto& candidate : mItems)
					if (candidate->menu == menu && !Attached(menu, candidate.get())) { item = candidate.get(); break; }
			}
			if (!item) { mItems.emplace_back(new Item); item = mItems.back().get(); }
			item->menu = menu; item->position = UINT(position); item->type = info.fType;
			item->bar = bar; item->text = text;
			item->accessible.cchWText = DWORD(item->text.size());
			item->accessible.pszWText = const_cast<wchar_t*>(item->text.c_str());
			MENUITEMINFOW style = {sizeof(style)}; style.fMask = MIIM_FTYPE | MIIM_DATA;
			style.fType = info.fType | MFT_OWNERDRAW;
			style.dwItemData = reinterpret_cast<ULONG_PTR>(item);
			SetMenuItemInfoW(menu, position, TRUE, &style);
			if (info.hSubMenu) SyncMenu(info.hSubMenu, false);
		}
	}
public:
	void Sync(HWND window, HMENU menu, bool hostedBar = false) { mWindow = window; mHostedBar = hostedBar; if (menu) SyncMenu(menu, true); }
	std::wstring Text(HMENU menu, UINT id, bool byPosition = false) const {
		MENUITEMINFOW info = {sizeof(info)}; info.fMask = MIIM_DATA;
		if (GetMenuItemInfoW(menu, id, byPosition, &info))
			if (auto* item = Find(info.dwItemData)) return item->text;
		if (byPosition) return ReadText(menu, id);
		for (int i = 0; i < GetMenuItemCount(menu); ++i) {
			MENUITEMINFOW entry = {sizeof(entry)}; entry.fMask = MIIM_ID;
			if (GetMenuItemInfoW(menu, i, TRUE, &entry) && entry.wID == id) return ReadText(menu, UINT(i));
		}
		return {};
	}
	bool Measure(MEASUREITEMSTRUCT* measure) {
		if (!measure || measure->CtlType != ODT_MENU) return false;
		auto* item = Find(measure->itemData); if (!item) return false;
		HDC dc = GetDC(mWindow); if (!dc) return false;
		{ WindowsUiDcState state(dc); SelectObject(dc, Font(item));
			const auto text = VisibleText(item->text); SIZE size = {};
			GetTextExtentPoint32W(dc, text.c_str(), int(text.size()), &size);
			measure->itemHeight = (item->type & MFT_SEPARATOR) ? Px(9) : (std::max)(Px(30), int(size.cy) + Px(10));
			measure->itemWidth = size.cx + Px(item->bar ? 16 : 64);
		}
		ReleaseDC(mWindow, dc); return true;
	}
	bool Draw(DRAWITEMSTRUCT* draw) {
		if (!draw || draw->CtlType != ODT_MENU) return false;
		auto* item = Find(draw->itemData); if (!item) return false;
		WindowsUiDcState state(draw->hDC);
		SelectObject(draw->hDC, Font(item)); SetBkMode(draw->hDC, TRANSPARENT);
		const bool disabled = (draw->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
		const bool selected = (draw->itemState & (ODS_SELECTED | ODS_HOTLIGHT)) != 0;
		const int background = selected ? COLOR_HIGHLIGHT : (item->bar ? COLOR_MENUBAR : COLOR_MENU);
		FillRect(draw->hDC, &draw->rcItem, GetSysColorBrush(background));
		SetTextColor(draw->hDC, GetSysColor(disabled ? COLOR_GRAYTEXT : (selected ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT)));
		RECT rect = draw->rcItem;
		if (item->type & MFT_SEPARATOR) {
			rect.left += Px(28); rect.right -= Px(8);
			rect.top = (rect.top + rect.bottom) / 2; rect.bottom = rect.top + 1;
			FillRect(draw->hDC, &rect, GetSysColorBrush(COLOR_3DSHADOW)); return true;
		}
		if (!item->bar) {
			rect.left += Px(28); rect.right -= Px(24);
			MENUITEMINFOW current = {sizeof(current)};
			current.fMask = MIIM_FTYPE | MIIM_STATE;
			GetMenuItemInfoW(item->menu, item->position, TRUE, &current);
			const int middle = (rect.top + rect.bottom) / 2;
			HPEN pen = CreatePen(PS_SOLID, Px(2), GetTextColor(draw->hDC));
			HBRUSH brush = CreateSolidBrush(GetTextColor(draw->hDC));
			const auto oldPen = SelectObject(draw->hDC, pen), oldBrush = SelectObject(draw->hDC, brush);
			if ((draw->itemState & ODS_CHECKED) || (current.fState & MFS_CHECKED)) {
				const int x = draw->rcItem.left + Px(14);
				if (current.fType & MFT_RADIOCHECK)
					Ellipse(draw->hDC, x - Px(2), middle - Px(2), x + Px(3), middle + Px(3));
				else {
					POINT tick[] = {{x - Px(5), middle}, {x - Px(1), middle + Px(4)}, {x + Px(6), middle - Px(4)}};
					Polyline(draw->hDC, tick, 3);
				}
			}
			// Windows paints the submenu chevron after WM_DRAWITEM. Reserve
			// its gutter, but do not overpaint it with a duplicate indicator.
			SelectObject(draw->hDC, oldPen); SelectObject(draw->hDC, oldBrush);
			DeleteObject(pen); DeleteObject(brush);
		} else { rect.left += Px(8); rect.right -= Px(8); }
		UINT flags = DT_SINGLELINE | DT_VCENTER;
		if (draw->itemState & ODS_NOACCEL) flags |= DT_HIDEPREFIX;
		const auto tab = item->text.find(L'\t');
		const auto label = item->text.substr(0, tab);
		DrawTextW(draw->hDC, label.c_str(), -1, &rect, flags | DT_LEFT);
		if (tab != std::wstring::npos)
			DrawTextW(draw->hDC, item->text.c_str() + tab + 1, -1, &rect, flags | DT_RIGHT | DT_NOPREFIX);
		return true;
	}
	bool MenuChar(UINT character, HMENU menu, LRESULT& result) const {
		std::vector<UINT> matches; int highlighted = -1;
		for (int i = 0; i < GetMenuItemCount(menu); ++i) {
			MENUITEMINFOW info = {sizeof(info)}; info.fMask = MIIM_DATA | MIIM_STATE;
			if (!GetMenuItemInfoW(menu, i, TRUE, &info)) continue;
			if (info.fState & MFS_HILITE) highlighted = i;
			auto* item = Find(info.dwItemData);
			if (!item || info.fState & (MFS_DISABLED | MFS_GRAYED)) continue;
			for (size_t j = 0; j + 1 < item->text.size(); ++j) {
				if (item->text[j] != L'&') continue;
				if (item->text[j + 1] == L'&') { ++j; continue; }
				if (towupper(item->text[j + 1]) == towupper(wchar_t(character))) matches.push_back(UINT(i));
				break;
			}
		}
		if (matches.empty()) return false;
		UINT next = matches.front();
		for (UINT position : matches) if (int(position) > highlighted) { next = position; break; }
		result = MAKELRESULT(next, matches.size() == 1 ? MNC_EXECUTE : MNC_SELECT); return true;
	}
};
} // namespace q1view
