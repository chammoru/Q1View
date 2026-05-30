// StoreUpdate.h : Microsoft Store self-update helper.
//
// Wraps Windows.Services.Store.StoreContext so the running app can detect and
// apply Store package updates. Every Store API call runs on a background MTA
// thread and results are posted back to a window, keeping the UI thread (and
// app cold start) unblocked.
//
// On non-packaged builds (sideload / local dev) all operations are no-ops, so
// the feature silently disappears outside the Store channel.

#pragma once

#include <windows.h>

namespace q1 {
namespace store {

// True when the running process is a packaged (Store/MSIX) app, i.e. updates
// can actually be applied. False for plain Win32 / sideload / dev builds.
bool IsPackaged();

// Asynchronously asks the Store whether updates are available. When (and only
// when) one or more updates are found, posts `availableMsg` to `notifyWnd` with
// wParam = update count. Nothing is posted on "up to date", error, offline, or
// non-packaged builds, so callers never have to special-case those.
void CheckForUpdatesAsync(HWND notifyWnd, UINT availableMsg);

// Downloads and installs the pending package updates, showing Store-provided
// progress UI parented to `ownerWnd`. Runs on a background thread and posts
// `doneMsg` to `ownerWnd` when finished (wParam = 1 on success, 0 on failure).
void DownloadAndInstallAsync(HWND ownerWnd, UINT doneMsg);

} // namespace store
} // namespace q1
