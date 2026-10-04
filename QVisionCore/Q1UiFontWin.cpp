// Keep WinRT headers out of the MFC/GDI+ renderers (which expose Color globally).
#include "Q1UiFontWin.h"
#include <wrl.h>
#include <wrl/wrappers/corewrappers.h>
#include <windows.ui.viewmanagement.h>

#pragma comment(lib, "runtimeobject.lib")

namespace q1view {
namespace {
class TextScaleState {
	Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IUISettings2> mSettings;
	EventRegistrationToken mToken = {};
	bool mSubscribed = false, mUninitialize = false;
	double mScale = 1.0;
public:
	TextScaleState()
	{
		mUninitialize = SUCCEEDED(RoInitialize(RO_INIT_SINGLETHREADED));
		Microsoft::WRL::ComPtr<IInspectable> instance;
		if (FAILED(RoActivateInstance(Microsoft::WRL::Wrappers::HStringReference(
			RuntimeClass_Windows_UI_ViewManagement_UISettings).Get(), &instance)) ||
			FAILED(instance.As(&mSettings))) return;
		Refresh();
		const DWORD uiThread = GetCurrentThreadId();
		using Handler = ABI::Windows::Foundation::ITypedEventHandler<
			ABI::Windows::UI::ViewManagement::UISettings*, IInspectable*>;
		auto handler = Microsoft::WRL::Callback<Handler>(
			[uiThread](ABI::Windows::UI::ViewManagement::IUISettings*, IInspectable*) -> HRESULT {
				// This callback may run off-thread. Do not touch MFC or GDI.
				EnumThreadWindows(uiThread, [](HWND window, LPARAM) -> BOOL {
					PostMessage(window, WM_UI_TYPOGRAPHY_CHANGED, 0, 0); return TRUE;
				}, 0);
				return S_OK;
			});
		mSubscribed = SUCCEEDED(mSettings->add_TextScaleFactorChanged(handler.Get(), &mToken));
	}
	~TextScaleState()
	{
		if (mSubscribed) mSettings->remove_TextScaleFactorChanged(mToken);
		mSettings.Reset();
		if (mUninitialize) RoUninitialize();
	}
	void Refresh()
	{
		double scale = 1.0;
		if (mSettings && SUCCEEDED(mSettings->get_TextScaleFactor(&scale)) &&
			std::isfinite(scale) && scale >= 1.0 && scale <= 2.25) mScale = scale;
	}
	double Scale() const { return mScale; }
};
TextScaleState& State() { static TextScaleState state; return state; }
} // namespace

void WindowsUiTextSettings::Refresh() { State().Refresh(); }
double WindowsUiTextSettings::Scale() const { return State().Scale(); }
WindowsUiTextSettings& WindowsUiSettings() { static WindowsUiTextSettings settings; return settings; }
} // namespace q1view
