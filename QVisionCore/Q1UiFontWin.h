#pragma once

// Windows-only UI typography shared by the MFC Viewer and Comparator.  The
// packaged font travels beside each executable, so load it privately instead
// of installing it globally on the customer's system.
#include <windows.h>
#include <tchar.h>
#include <string>

namespace q1view {

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

} // namespace q1view
