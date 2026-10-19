#pragma once

// Directory metadata only: never decode a source image while navigating.
#include <windows.h>
#include <string>
#include <vector>
#include <algorithm>
#include "ViewerFileOrder.h"

namespace q1view {
struct BrowserDirectory {
    std::wstring folder;
    std::vector<std::wstring> directories, files;
    DWORD error = ERROR_SUCCESS;
};

inline BrowserDirectory ReadBrowserDirectory(std::wstring folder)
{
    BrowserDirectory result;
    if (!folder.empty() && folder.back() != L'\\') folder += L'\\';
    result.folder = folder;
    // Do not let removable media generate a second, system-owned modal dialog.
    DWORD oldMode = 0;
    const BOOL changedMode = SetThreadErrorMode(SEM_FAILCRITICALERRORS, &oldMode);
    struct RestoreMode { BOOL changed; DWORD mode; ~RestoreMode() { if (changed) SetThreadErrorMode(mode, nullptr); } } mode{changedMode, oldMode};
    if (folder.empty()) {
        DWORD size = GetLogicalDriveStringsW(0, nullptr);
        if (!size) { result.error = GetLastError(); return result; }
        std::vector<wchar_t> drives(size + 1);
        DWORD read = GetLogicalDriveStringsW(DWORD(drives.size()), drives.data());
        if (!read || read >= drives.size()) { result.error = read ? ERROR_MORE_DATA : GetLastError(); return result; }
        for (const wchar_t* drive = drives.data(); *drive; drive += wcslen(drive) + 1)
            result.directories.emplace_back(drive);
        return result;
    }
    // An accessible empty directory is success; a failed wildcard search alone
    // cannot distinguish that from an absent/inaccessible directory.
    HANDLE directory = CreateFileW(folder.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (directory == INVALID_HANDLE_VALUE) { result.error = GetLastError(); return result; }
    struct Close { HANDLE handle; ~Close() { CloseHandle(handle); } } close{directory};
    BY_HANDLE_FILE_INFORMATION info{};
    if (!GetFileInformationByHandle(directory, &info)) { result.error = GetLastError(); return result; }
    if (!(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) { result.error = ERROR_DIRECTORY; return result; }
    WIN32_FIND_DATAW data{};
    HANDLE find = FindFirstFileExW((folder + L"*").c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, nullptr, 0);
    if (find == INVALID_HANDLE_VALUE) {
        result.error = GetLastError();
        if (result.error == ERROR_FILE_NOT_FOUND) {
            const DWORD attributes = GetFileAttributesW(folder.c_str());
            result.error = attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() :
                ((attributes & FILE_ATTRIBUTE_DIRECTORY) ? ERROR_SUCCESS : ERROR_DIRECTORY);
        }
        return result;
    }
    do {
        if (!wcscmp(data.cFileName, L".") || !wcscmp(data.cFileName, L"..")) continue;
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (!(data.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
                result.directories.push_back(folder + data.cFileName);
        } else result.files.push_back(folder + data.cFileName);
    } while (FindNextFileW(find, &data));
    const DWORD error = GetLastError();
    FindClose(find);
    if (error != ERROR_NO_MORE_FILES) result.error = error;
    FILE_STANDARD_INFO standard{};
    if (!result.error && GetFileInformationByHandleEx(directory, FileStandardInfo, &standard, sizeof(standard)) && standard.DeletePending)
        result.error = ERROR_PATH_NOT_FOUND;
    const auto less = [](const std::wstring& a, const std::wstring& b) {
        return LessFileNameOrdinal(CString(a.c_str()), CString(b.c_str()));
    };
    std::sort(result.directories.begin(), result.directories.end(), less);
    std::sort(result.files.begin(), result.files.end(), less);
    return result;
}

inline std::wstring BrowserNavigationError(const std::wstring& path, DWORD error)
{
    std::wstring text = L"Can't open folder:\n" + path + L"\n\n";
    switch (error) {
    case ERROR_ACCESS_DENIED: return text + L"Access was denied. Check your permissions, then try again.";
    case ERROR_FILE_NOT_FOUND: case ERROR_PATH_NOT_FOUND: case ERROR_DIRECTORY:
        return text + L"The folder could not be found. Check whether it was moved or deleted, then try again.";
    case ERROR_NOT_READY: case ERROR_INVALID_DRIVE: case ERROR_DEVICE_NOT_CONNECTED:
    case ERROR_BAD_NETPATH: case ERROR_BAD_NET_NAME: case ERROR_NETWORK_UNREACHABLE:
        return text + L"The drive or network location is unavailable. Check its connection, then try again.";
    default: return text + L"The folder could not be read. Check the location and try again.\nWindows error: " + std::to_wstring(error);
    }
}
}
