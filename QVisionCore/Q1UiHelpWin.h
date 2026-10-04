#pragma once

#include "Q1UiFontWin.h"
#include <vector>

namespace q1view {

struct WindowsUiHelpRow { std::wstring key, description; };
struct WindowsUiHelpRowBounds { RECT key, description; };
struct WindowsUiHelpLayout {
	std::vector<WindowsUiHelpRowBounds> rows;
	int height = 0;
};

inline bool WindowsUiHelpNavigation(MSG* message, int page, int maximum, int& offset)
{
	if (!message) return false;
	if (message->message == WM_MOUSEWHEEL) {
		offset -= MulDiv(GET_WHEEL_DELTA_WPARAM(message->wParam), (std::max)(20, page / 6), WHEEL_DELTA);
	} else if (message->message == WM_KEYDOWN) {
		switch (message->wParam) {
		case VK_PRIOR: offset -= page; break;
		case VK_NEXT: offset += page; break;
		case VK_HOME: offset = 0; break;
		case VK_END: offset = maximum; break;
		case VK_UP: offset -= (std::max)(20, page / 12); break;
		case VK_DOWN: offset += (std::max)(20, page / 12); break;
		default: return false;
		}
	} else return false;
	offset = (std::max)(0, (std::min)(offset, maximum));
	return true;
}

// Proportional fonts cannot align columns with spaces. Measure each key and
// wrap descriptions in an independent column, preserving literal '&' glyphs.
inline WindowsUiHelpLayout MeasureWindowsUiHelp(HDC dc, const RECT& bounds,
	const std::vector<WindowsUiHelpRow>& rows, WindowsUiFontCache& fonts, UINT dpi, double scale)
{
	WindowsUiHelpLayout layout;
	const int saved = SaveDC(dc);
	SelectObject(dc, fonts.Get(WindowsUiFontRole::Body, dpi, scale));
	int keyWidth = 0;
	for (const auto& row : rows) {
		SIZE size = {};
		GetTextExtentPoint32W(dc, row.key.c_str(), int(row.key.size()), &size);
		keyWidth = (std::max)(keyWidth, int(size.cx));
	}
	const int width = (std::max)(2, int(bounds.right - bounds.left));
	const int gap = (std::min)(width / 8, WindowsUiPixels(12, dpi));
	keyWidth = (std::min)(keyWidth, (width - gap) * 2 / 5);
	const int minimumLine = WindowsUiPixels(20, dpi, scale);
	int top = bounds.top;
	for (const auto& row : rows) {
		RECT key = {bounds.left, top, bounds.left + keyWidth, top};
		RECT description = {key.right + gap, top, bounds.right, top};
		DrawTextW(dc, row.key.c_str(), -1, &key, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
		DrawTextW(dc, row.description.c_str(), -1, &description, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
		const int height = (std::max)(minimumLine,
			(std::max)(int(key.bottom - top), int(description.bottom - top)));
		// CALCRECT may widen an unbreakable word. Keep the actual draw clip in
		// its column rather than overwriting the neighbouring description.
		key.right = bounds.left + keyWidth; key.bottom = top + height;
		description.left = key.right + gap; description.right = bounds.right;
		description.bottom = top + height;
		layout.rows.push_back({key, description});
		top += height + WindowsUiPixels(4, dpi);
	}
	layout.height = top - bounds.top;
	RestoreDC(dc, saved);
	return layout;
}

// Returns the maximum scroll offset, used by the owning nonactivating overlay.
inline int DrawWindowsUiHelp(HDC dc, RECT bounds, const std::wstring& title,
	const std::wstring& version, const std::vector<WindowsUiHelpRow>& rows,
	WindowsUiFontCache& fonts, UINT dpi, double scale, int& scrollOffset)
{
	const int saved = SaveDC(dc);
	const int titleHeight = WindowsUiPixels(22, dpi, scale);
	const int supportingHeight = WindowsUiPixels(16, dpi, scale);
	SetBkMode(dc, TRANSPARENT);
	SelectObject(dc, fonts.Get(WindowsUiFontRole::Status, dpi, scale));
	RECT titleRect = bounds; titleRect.bottom = titleRect.top + titleHeight;
	DrawTextW(dc, title.c_str(), -1, &titleRect, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
	SelectObject(dc, fonts.Get(WindowsUiFontRole::Supporting, dpi, scale));
	RECT versionRect = bounds; versionRect.top = titleRect.bottom;
	versionRect.bottom = versionRect.top + supportingHeight;
	DrawTextW(dc, version.c_str(), -1, &versionRect, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
	RECT content = bounds;
	content.top = versionRect.bottom + WindowsUiPixels(12, dpi);
	content.bottom -= supportingHeight + WindowsUiPixels(8, dpi);
	if (content.bottom <= content.top) { scrollOffset = 0; RestoreDC(dc, saved); return 0; }
	const auto layout = MeasureWindowsUiHelp(dc, content, rows, fonts, dpi, scale);
	const int maxScroll = (std::max)(0, layout.height - int(content.bottom - content.top));
	scrollOffset = (std::max)(0, (std::min)(scrollOffset, maxScroll));
	const int bodySaved = SaveDC(dc);
	IntersectClipRect(dc, content.left, content.top, content.right, content.bottom);
	SelectObject(dc, fonts.Get(WindowsUiFontRole::Body, dpi, scale));
	for (size_t i = 0; i < rows.size(); ++i) {
		RECT key = layout.rows[i].key, description = layout.rows[i].description;
		OffsetRect(&key, 0, -scrollOffset); OffsetRect(&description, 0, -scrollOffset);
		if (key.bottom <= content.top || key.top >= content.bottom) continue;
		DrawTextW(dc, rows[i].key.c_str(), -1, &key, DT_WORDBREAK | DT_NOPREFIX);
		DrawTextW(dc, rows[i].description.c_str(), -1, &description, DT_WORDBREAK | DT_NOPREFIX);
	}
	RestoreDC(dc, bodySaved);
	SelectObject(dc, fonts.Get(WindowsUiFontRole::Supporting, dpi, scale));
	RECT footer = bounds; footer.top = bounds.bottom - supportingHeight;
	const wchar_t* hint = maxScroll ? L"Scroll / Page Down for more. Click or Esc to close." : L"Click or Esc to close.";
	DrawTextW(dc, hint, -1, &footer, DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
	RestoreDC(dc, saved);
	return maxScroll;
}

} // namespace q1view
