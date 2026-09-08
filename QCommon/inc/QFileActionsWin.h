#pragma once
#include <windows.h>
#include <shlobj.h>
#include <string>
#include <cstring>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

// Shared Windows file actions; paths are data, never shell command text.
namespace q1view {
inline std::wstring TrimDirectorySeparator(std::wstring path) {
    while (path.size() > 3 && path.back() == L'\\') path.pop_back();
    return path;
}
inline std::wstring ParentDirectory(std::wstring path) {
    for (auto& c : path) if (c == L'/') c = L'\\';
    path = TrimDirectorySeparator(path);
    size_t root = 0;
    if (path.compare(0, 8, L"\\\\?\\UNC\\") == 0) {
        const std::wstring parent = ParentDirectory(L"\\\\" + path.substr(8));
        return parent.empty() ? parent : L"\\\\?\\UNC\\" + parent.substr(2);
    }
    if (path.compare(0, 4, L"\\\\?\\") == 0) {
        const std::wstring parent = ParentDirectory(path.substr(4));
        return parent.empty() ? parent : L"\\\\?\\" + parent;
    }
    if (path.size() >= 3 && path[1] == L':' && path[2] == L'\\') root = 3;
    else if (path.compare(0, 2, L"\\\\") == 0) {
        const size_t server = path.find(L'\\', 2);
        if (server == std::wstring::npos) return {};
        const size_t share = path.find(L'\\', server + 1);
        if (share == std::wstring::npos) return {};
        root = share + 1;
    }
    if (!root || path.size() <= root) return {};
    const size_t slash = path.find_last_of(L'\\');
    if (slash == std::wstring::npos || slash + 1 < root) return {};
    return path.substr(0, slash + 1);
}
inline bool ClipboardText(HWND owner, const std::wstring& text) {
    const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!data) return false;
    void* memory = GlobalLock(data);
    if (!memory) { GlobalFree(data); return false; }
    std::memcpy(memory, text.c_str(), bytes); GlobalUnlock(data);
    if (!OpenClipboard(owner)) { GlobalFree(data); return false; }
    const bool ok = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, data);
    CloseClipboard(); if (!ok) GlobalFree(data);
    return ok;
}
inline bool ClipboardFile(HWND owner, const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    const size_t bytes = sizeof(DROPFILES) + (path.size() + 2) * sizeof(wchar_t);
    HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    if (!data) return false;
    auto* drop = static_cast<DROPFILES*>(GlobalLock(data));
    if (!drop) { GlobalFree(data); return false; }
    drop->pFiles = sizeof(DROPFILES); drop->fWide = TRUE;
    std::memcpy(reinterpret_cast<BYTE*>(drop) + sizeof(DROPFILES), path.c_str(),
        (path.size() + 1) * sizeof(wchar_t));
    GlobalUnlock(data);
    if (!OpenClipboard(owner)) { GlobalFree(data); return false; }
    const bool ok = EmptyClipboard() && SetClipboardData(CF_HDROP, data);
    if (ok) {
        HGLOBAL effect = GlobalAlloc(GMEM_MOVEABLE, sizeof(DWORD));
        if (effect) {
            auto* value = static_cast<DWORD*>(GlobalLock(effect));
            if (value) {
                *value = DROPEFFECT_COPY; GlobalUnlock(effect);
                if (!SetClipboardData(RegisterClipboardFormatW(L"Preferred DropEffect"), effect)) GlobalFree(effect);
            } else GlobalFree(effect);
        }
    }
    CloseClipboard(); if (!ok) GlobalFree(data);
    return ok;
}
inline bool ShowInExplorer(const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    PIDLIST_ABSOLUTE item = nullptr;
    HRESULT hr = SHParseDisplayName(path.c_str(), nullptr, &item, 0, nullptr);
    if (FAILED(hr)) return false;
    hr = SHOpenFolderAndSelectItems(item, 0, nullptr, 0);
    CoTaskMemFree(item); return SUCCEEDED(hr);
}
inline bool ShowFileProperties(HWND owner, const std::wstring& path) {
    if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    SHELLEXECUTEINFOW info = { sizeof(info) };
    info.fMask = SEE_MASK_INVOKEIDLIST | SEE_MASK_FLAG_NO_UI;
    info.hwnd = owner; info.lpVerb = L"properties";
    info.lpFile = path.c_str(); info.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&info) != FALSE;
}
}
