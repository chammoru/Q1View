#pragma once

// Windows-only UI typography shared by the MFC Viewer and Comparator.  The
// packaged font travels beside each executable, so load it privately instead
// of installing it globally on the customer's system.
#include <windows.h>
#include <tchar.h>
#include <string>
#include <array>
#include <algorithm>
#include <cmath>

namespace q1view {

// Only UI text/its measured bounds change. Image-space pixel labels keep their
// zoom-derived size and OS-owned controls keep their native typography.
constexpr UINT WM_UI_TYPOGRAPHY_CHANGED = WM_APP + 117;

class WindowsUiTextSettings {
public:
	void Refresh();
	double Scale() const;
};

WindowsUiTextSettings& WindowsUiSettings();

// Persistent back-buffer DCs must not retain a font that a later settings
// change will replace. Local fonts must still be deselected before destruction.
class WindowsUiDcState {
	HDC mDc;
	int mSaved;
public:
	explicit WindowsUiDcState(HDC dc) : mDc(dc), mSaved(SaveDC(dc)) {}
	~WindowsUiDcState() { if (mSaved) RestoreDC(mDc, mSaved); }
	WindowsUiDcState(const WindowsUiDcState&) = delete;
	WindowsUiDcState& operator=(const WindowsUiDcState&) = delete;
};

enum class WindowsUiFontRole {
	Body, Command, Caption, Folder, Supporting, Status, EmptyTitle, Metric, Numeric, Count
};

struct WindowsUiFontSpec { int size, lineHeight, weight; bool numeric; };

inline WindowsUiFontSpec WindowsUiFontSpecification(WindowsUiFontRole role)
{
	switch (role) {
	case WindowsUiFontRole::Command: return {14, 20, FW_MEDIUM, false};
	case WindowsUiFontRole::Caption: return {13, 18, FW_NORMAL, false};
	case WindowsUiFontRole::Folder: return {13, 18, FW_MEDIUM, false};
	case WindowsUiFontRole::Supporting: return {12, 16, FW_NORMAL, false};
	case WindowsUiFontRole::Status: return {16, 22, FW_MEDIUM, false};
	case WindowsUiFontRole::EmptyTitle: return {20, 28, FW_SEMIBOLD, false};
	case WindowsUiFontRole::Metric: return {16, 22, FW_NORMAL, true};
	case WindowsUiFontRole::Numeric: return {13, 18, FW_NORMAL, true};
	default: return {14, 20, FW_NORMAL, false};
	}
}

inline int WindowsUiPixels(double dip, UINT dpi, double textScale = 1.0)
{
	return (std::max)(1, int(std::lround(dip * (dpi ? dpi : 96) / 96.0 * textScale)));
}

inline UINT WindowsUiDpi(HWND window)
{
	if (!window || !IsWindow(window)) return 96;
	// Comparator's historical targetver does not declare the Windows 10 API.
	using GetWindowDpi = UINT(WINAPI*)(HWND);
	static const auto getDpi = reinterpret_cast<GetWindowDpi>(
		GetProcAddress(GetModuleHandle(_T("user32.dll")), "GetDpiForWindow"));
	if (getDpi) { const UINT dpi = getDpi(window); if (dpi) return dpi; }
	HDC dc = GetDC(window);
	const UINT dpi = dc ? GetDeviceCaps(dc, LOGPIXELSX) : 96;
	if (dc) ReleaseDC(window, dc);
	return dpi ? dpi : 96;
}

inline LPCTSTR WindowsUiTextFontFamily()
{
	static const bool loaded = [] {
		wchar_t module[MAX_PATH] = {};
		const DWORD length = GetModuleFileNameW(nullptr, module, _countof(module));
		if (length == 0 || length >= _countof(module))
			return false;
		std::wstring path(module, length);
		const std::wstring::size_type separator = path.find_last_of(L"\\\\/");
		if (separator == std::wstring::npos)
			return false;
		path.resize(separator + 1);
		path += L"Fonts\\PretendardVariable.ttf";
		return AddFontResourceExW(path.c_str(), FR_PRIVATE, nullptr) > 0;
	}();
	return loaded ? _T("Pretendard Variable") : _T("Segoe UI");
}

inline LPCTSTR WindowsUiNumericFontFamily()
{
	static const bool available = [] {
		LOGFONT query = {};
		query.lfCharSet = DEFAULT_CHARSET;
		lstrcpyn(query.lfFaceName, _T("Cascadia Mono"), LF_FACESIZE);
		bool found = false;
		HDC dc = GetDC(nullptr);
		if (dc) {
			EnumFontFamiliesEx(dc, &query, [](const LOGFONT*, const TEXTMETRIC*, DWORD, LPARAM data) -> int {
				*reinterpret_cast<bool*>(data) = true; return 0;
			}, reinterpret_cast<LPARAM>(&found), 0);
			ReleaseDC(nullptr, dc);
		}
		return found;
	}();
	return available ? _T("Cascadia Mono") : _T("Consolas");
}

inline LOGFONT WindowsUiLogFont(WindowsUiFontRole role, UINT dpi, double textScale)
{
	const auto spec = WindowsUiFontSpecification(role);
	LOGFONT font = {};
	font.lfHeight = -WindowsUiPixels(spec.size, dpi, textScale); // character height, not cell/point size
	font.lfWeight = spec.weight;
	font.lfCharSet = DEFAULT_CHARSET;
	font.lfQuality = CLEARTYPE_NATURAL_QUALITY;
	lstrcpyn(font.lfFaceName, spec.numeric ? WindowsUiNumericFontFamily() : WindowsUiTextFontFamily(), LF_FACESIZE);
	return font;
}

// One fixed-size cache per control. No font creation per frame, no unbounded
// process-global cache keyed by every window/DPI combination. Call Get before
// selecting the font, and restore the previous DC object before a later Get.
class WindowsUiFontCache {
	struct Entry { HFONT font = nullptr; LONG height = 0; };
	std::array<Entry, static_cast<size_t>(WindowsUiFontRole::Count)> mEntries = {};
public:
	WindowsUiFontCache() = default;
	WindowsUiFontCache(const WindowsUiFontCache&) = delete;
	WindowsUiFontCache& operator=(const WindowsUiFontCache&) = delete;
	~WindowsUiFontCache() { for (auto& entry : mEntries) if (entry.font) DeleteObject(entry.font); }
	HFONT Get(WindowsUiFontRole role, UINT dpi, double scale)
	{
		auto& entry = mEntries[static_cast<size_t>(role)];
		const auto spec = WindowsUiFontSpecification(role);
		const LONG height = -WindowsUiPixels(spec.size, dpi, scale);
		if (!entry.font || entry.height != height) {
			auto logFont = WindowsUiLogFont(role, dpi, scale);
			HFONT replacement = CreateFontIndirect(&logFont);
			if (replacement) {
				if (entry.font) DeleteObject(entry.font);
				entry.font = replacement; entry.height = height;
			}
		}
		return entry.font ? entry.font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
	}
	HFONT Get(WindowsUiFontRole role, HWND window)
	{ return Get(role, WindowsUiDpi(window), WindowsUiSettings().Scale()); }
};

// Existing persistent MFC CFont members can share the same role definitions.
template<class Font> inline void EnsureWindowsUiFont(Font& font, WindowsUiFontRole role, HWND window)
{
	const auto wanted = WindowsUiLogFont(role, WindowsUiDpi(window), WindowsUiSettings().Scale());
	LOGFONT current = {};
	if (font.GetSafeHandle() && font.GetLogFont(&current) &&
		current.lfHeight == wanted.lfHeight && current.lfWeight == wanted.lfWeight &&
		lstrcmp(current.lfFaceName, wanted.lfFaceName) == 0) return;
	font.DeleteObject();
	font.CreateFontIndirect(&wanted);
}

} // namespace q1view
