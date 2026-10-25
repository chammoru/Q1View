#pragma once

#include "GalleryLayout.h"
#include <UIAutomation.h>
#include <wrl/client.h>
#include <memory>
#include <functional>
#include <string>

// The provider reads committed metadata, never the pane, filesystem or decoder.
// Only this UI-thread bridge can execute requests against the real canvas.
namespace q1view {
constexpr UINT WM_GALLERY_UIA_ACTION = WM_APP + 218;
struct GalleryAccessibleEntry {
    std::wstring path, name;
    int kind = 0;
    bool media = false;
};
enum class GalleryAccessibleAction { Focus, Select, Add, Remove, Invoke, Reveal, ScrollPercent, ScrollAmount };
struct GalleryUiaState;
class GalleryUiaBridge {
public:
    using Action = std::function<HRESULT(GalleryAccessibleAction, int, double)>;
    GalleryUiaBridge(HWND window, Action action);
    ~GalleryUiaBridge();
    LRESULT GetObject(WPARAM wp, LPARAM lp);
    void Entries(const std::wstring& folder, std::vector<GalleryAccessibleEntry> entries);
    void Layout(const GalleryLayout& layout);
    void View(const GalleryLayout& layout, const std::set<int>& selected, int focus, bool enabled);
    void FocusChanged();
    void Dispatch(UINT_PTR request);
    void Close();
#ifdef Q1VIEW_GALLERY_TESTS
    std::wstring Diagnostics();
    int LiveProviders() const;
#endif
private:
    std::shared_ptr<GalleryUiaState> mState;
    Microsoft::WRL::ComPtr<IRawElementProviderSimple> mRoot;
    Action mAction;
    HRESULT mComInit = E_FAIL;
};
}
