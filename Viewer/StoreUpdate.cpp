// StoreUpdate.cpp : Microsoft Store self-update helper implementation.
//
// This translation unit is compiled as C++17 without the project's precompiled
// header (see Viewer.vcxproj) because C++/WinRT requires C++17 while the rest of
// the project builds as C++14. It deliberately pulls in no MFC headers.

#include "StoreUpdate.h"

#include <appmodel.h>        // GetCurrentPackageFullName
#include <shobjidl_core.h>   // IInitializeWithWindow
#include <cstdio>            // swprintf_s
#include <thread>

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Services.Store.h>

#pragma comment(lib, "windowsapp.lib")

using namespace winrt;
using namespace winrt::Windows::Services::Store;

namespace q1 {
namespace store {

// These failures are non-fatal by design (offline, not signed in, Store policy,
// etc.), so they are swallowed rather than surfaced to the user. Emit a debug
// trace so the cause is still recoverable from DebugView / the debugger output.
static void LogFailure(const wchar_t *what, const winrt::hresult_error &e)
{
	wchar_t buf[512];
	swprintf_s(buf, L"[Q1View][StoreUpdate] %s failed: hr=0x%08X %s\n",
		what, static_cast<unsigned>(e.code()), e.message().c_str());
	::OutputDebugStringW(buf);
}

static void LogFailure(const wchar_t *what)
{
	wchar_t buf[256];
	swprintf_s(buf, L"[Q1View][StoreUpdate] %s failed: unknown exception\n", what);
	::OutputDebugStringW(buf);
}

bool IsPackaged()
{
	// A packaged process has a package full name; an unpackaged one returns
	// APPMODEL_ERROR_NO_PACKAGE. Querying with a zero-length buffer yields
	// ERROR_INSUFFICIENT_BUFFER only when a name actually exists.
	UINT32 length = 0;
	return GetCurrentPackageFullName(&length, nullptr) == ERROR_INSUFFICIENT_BUFFER;
}

// Desktop (Win32) packaged apps have no CoreWindow, so the StoreContext must be
// associated with a top-level window before any call that can show Store UI.
static void AssociateWindow(const StoreContext &context, HWND ownerWnd)
{
	auto interop = context.as<::IInitializeWithWindow>();
	check_hresult(interop->Initialize(ownerWnd));
}

void CheckForUpdatesAsync(HWND notifyWnd, UINT availableMsg)
{
	if (notifyWnd == nullptr || !IsPackaged())
		return;

	std::thread([notifyWnd, availableMsg]() {
		init_apartment(apartment_type::multi_threaded);
		try {
			StoreContext context = StoreContext::GetDefault();
			auto updates = context.GetAppAndOptionalStorePackageUpdatesAsync().get();
			if (updates != nullptr && updates.Size() > 0)
				::PostMessage(notifyWnd, availableMsg,
					static_cast<WPARAM>(updates.Size()), 0);
		} catch (const winrt::hresult_error &e) {
			LogFailure(L"update check", e);
		} catch (...) {
			LogFailure(L"update check");
		}
		uninit_apartment();
	}).detach();
}

void DownloadAndInstallAsync(HWND ownerWnd, UINT doneMsg)
{
	if (ownerWnd == nullptr || !IsPackaged())
		return;

	std::thread([ownerWnd, doneMsg]() {
		init_apartment(apartment_type::multi_threaded);
		bool ok = false;
		try {
			StoreContext context = StoreContext::GetDefault();
			AssociateWindow(context, ownerWnd);
			auto updates = context.GetAppAndOptionalStorePackageUpdatesAsync().get();
			if (updates != nullptr && updates.Size() > 0) {
				auto result =
					context.RequestDownloadAndInstallStorePackageUpdatesAsync(updates).get();
				ok = result.OverallState() == StorePackageUpdateState::Completed;
			}
		} catch (const winrt::hresult_error &e) {
			ok = false;
			LogFailure(L"update install", e);
		} catch (...) {
			ok = false;
		}
		uninit_apartment();
		::PostMessage(ownerWnd, doneMsg, ok ? 1 : 0, 0);
	}).detach();
}

} // namespace store
} // namespace q1
