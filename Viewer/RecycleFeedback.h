#pragma once

#include "QRecycleFilesWin.h"
#include <commctrl.h>
#include <algorithm>
#pragma comment(lib, "comctl32.lib")

namespace q1view {
struct RecycleDialogText {
    std::wstring instruction, content, details;
};

inline std::wstring RecycleFileName(const std::wstring& path) {
    const auto slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? path : path.substr(slash + 1);
}

inline RecycleDialogText RecycleConfirmationText(const std::vector<std::wstring>& paths) {
    RecycleDialogText text;
    text.instruction = paths.size() == 1 ? L"Move this item to the Recycle Bin?" :
        L"Move " + std::to_wstring(paths.size()) + L" items to the Recycle Bin?";
    const size_t shown = (std::min)(paths.size(), size_t(3));
    for (size_t i = 0; i < shown; ++i) {
        if (i) text.content += L"\n";
        text.content += RecycleFileName(paths[i]);
    }
    if (shown < paths.size()) text.content += L"\nAnd " + std::to_wstring(paths.size() - shown) +
        (paths.size() - shown == 1 ? L" more item." : L" more items.");
    const size_t detailed = (std::min)(paths.size(), size_t(10));
    for (size_t i = 0; i < detailed; ++i) {
        if (i) text.details += L"\n";
        text.details += paths[i];
    }
    if (detailed < paths.size()) text.details += L"\nShowing " + std::to_wstring(detailed) +
        L" of " + std::to_wstring(paths.size()) + L" selected paths.";
    return text;
}

inline std::wstring RecycleFailureReason(HRESULT error) {
    wchar_t* buffer = nullptr;
    std::wstring reason;
    if (FAILED(error) && HRESULT_FACILITY(error) == FACILITY_WIN32 &&
        FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, HRESULT_CODE(error), 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr)) {
        reason = buffer; LocalFree(buffer);
        const auto end = reason.find_last_not_of(L" \t\r\n");
        reason.erase(end == std::wstring::npos ? 0 : end + 1);
    }
    return reason.empty() ? L"Windows did not confirm that this item was moved." : reason;
}

inline RecycleDialogText RecycleResultText(const std::vector<RecycleResult>& results) {
    RecycleDialogText text;
    const size_t moved = std::count_if(results.begin(), results.end(), [](const RecycleResult& r) { return r.recycled; });
    const size_t failed = results.size() - moved;
    text.instruction = L"Moved " + std::to_wstring(moved) + L" of " + std::to_wstring(results.size()) +
        L" items to the Recycle Bin.";
    text.content = failed == 1 ? L"1 item could not be moved:" : std::to_wstring(failed) + L" items could not be moved:";
    size_t shown = 0;
    for (const auto& result : results) if (!result.recycled && shown++ < 3)
        text.content += L"\n" + RecycleFileName(result.path);
    if (failed > 3) text.content += L"\nAnd " + std::to_wstring(failed - 3) + (failed == 4 ? L" more item." : L" more items.");
    shown = 0;
    for (const auto& result : results) if (!result.recycled && shown++ < 10) {
        if (!text.details.empty()) text.details += L"\n\n";
        text.details += result.path + L"\n" + RecycleFailureReason(result.error);
    }
    if (failed > 10) text.details += L"\n\nShowing 10 of " + std::to_wstring(failed) + L" items that could not be moved.";
    return text;
}

inline TASKDIALOGCONFIG RecycleDialogConfig(HWND owner, const RecycleDialogText& text, bool confirmation) {
    static const TASKDIALOG_BUTTON move[] = {{IDOK, L"Move to Recycle Bin"}};
    TASKDIALOGCONFIG config = {sizeof(config)};
    config.hwndParent = owner ? GetAncestor(owner, GA_ROOT) : nullptr;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.dwCommonButtons = confirmation ? TDCBF_CANCEL_BUTTON : TDCBF_OK_BUTTON;
    config.pszWindowTitle = L"Move to Recycle Bin";
    config.pszMainInstruction = text.instruction.c_str();
    config.pszContent = text.content.c_str();
    config.pszExpandedInformation = text.details.empty() ? nullptr : text.details.c_str();
    config.pszExpandedControlText = L"Hide details";
    config.pszCollapsedControlText = L"Show details";
    config.cButtons = confirmation ? 1 : 0;
    config.pButtons = confirmation ? move : nullptr;
    config.nDefaultButton = confirmation ? IDCANCEL : IDOK;
    return config;
}

inline bool ShowRecycleDialog(HWND owner, const RecycleDialogText& text, bool confirmation) {
    auto config = RecycleDialogConfig(owner, text, confirmation);
    const HWND focus = GetFocus();
    struct RestoreFocus {
        HWND root, focus;
        ~RestoreFocus() {
            if (GetForegroundWindow() == root && IsWindow(focus) && IsWindowVisible(focus) && IsWindowEnabled(focus))
                SetFocus(focus);
        }
    } restore{config.hwndParent, focus};
    int button = IDCANCEL;
    if (SUCCEEDED(TaskDialogIndirect(&config, &button, nullptr, nullptr))) return confirmation && button == IDOK;
    // If the native dialog cannot be created, do not interpret that failure as consent.
    const std::wstring message = text.instruction + L"\n\n" + text.content + L"\n\n" + text.details;
    const UINT flags = confirmation ? MB_OKCANCEL | MB_DEFBUTTON2 : MB_OK;
    return MessageBoxW(config.hwndParent, message.c_str(), L"Move to Recycle Bin", flags) == IDOK && confirmation;
}
}
