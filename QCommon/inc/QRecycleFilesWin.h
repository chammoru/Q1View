#pragma once

#include <windows.h>
#include <shlobj.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <string>
#include <vector>
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace q1view {
struct RecycleResult {
    std::wstring path;
    HRESULT error = E_ABORT;
    bool recycled = false;
};

class RecycleProgress final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>, IFileOperationProgressSink> {
public:
    explicit RecycleProgress(RecycleResult* result) : mResult(result) {}
    HRESULT STDMETHODCALLTYPE PreDeleteItem(DWORD flags, IShellItem* item) override {
        // Reject the Shell's permanent-delete fallback, including non-trash volumes.
        SFGAOF attributes = 0;
        if (!(flags & TSF_DELETE_RECYCLE_IF_POSSIBLE) ||
            FAILED(item->GetAttributes(SFGAO_FOLDER, &attributes)) || (attributes & SFGAO_FOLDER))
            return mResult->error = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE PostDeleteItem(DWORD, IShellItem*, HRESULT result, IShellItem* recycled) override {
        mResult->error = result;
        mResult->recycled = SUCCEEDED(result) && recycled != nullptr;
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE StartOperations() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE FinishOperations(HRESULT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PreRenameItem(DWORD, IShellItem*, LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE PostRenameItem(DWORD, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PreMoveItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE PostMoveItem(DWORD, IShellItem*, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PreCopyItem(DWORD, IShellItem*, IShellItem*, LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE PostCopyItem(DWORD, IShellItem*, IShellItem*, LPCWSTR, HRESULT, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PreNewItem(DWORD, IShellItem*, LPCWSTR) override { return E_NOTIMPL; }
    HRESULT STDMETHODCALLTYPE PostNewItem(DWORD, IShellItem*, LPCWSTR, LPCWSTR, DWORD, HRESULT, IShellItem*) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE UpdateProgress(UINT, UINT) override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ResetTimer() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE PauseTimer() override { return S_OK; }
    HRESULT STDMETHODCALLTYPE ResumeTimer() override { return S_OK; }
private:
    RecycleResult* mResult;
};

inline std::vector<RecycleResult> RecycleFiles(HWND owner, const std::vector<std::wstring>& paths) {
    std::vector<RecycleResult> results(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) results[i].path = paths[i];
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    struct Apartment {
        HRESULT result;
        ~Apartment() { if (SUCCEEDED(result)) CoUninitialize(); }
    } apartment{initialized};
    Microsoft::WRL::ComPtr<IFileOperation> operation;
    HRESULT setup = CoCreateInstance(__uuidof(FileOperation), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&operation));
    if (SUCCEEDED(setup)) setup = operation->SetOwnerWindow(owner);
    if (SUCCEEDED(setup)) setup = operation->SetOperationFlags(FOFX_RECYCLEONDELETE | FOFX_ADDUNDORECORD |
        FOF_NO_CONNECTED_ELEMENTS | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT);
    if (FAILED(setup)) {
        for (auto& result : results) result.error = setup;
        return results;
    }
    bool queued = false;
    for (auto& result : results) {
        const DWORD attributes = GetFileAttributesW(result.path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
            result.error = HRESULT_FROM_WIN32(attributes == INVALID_FILE_ATTRIBUTES ? GetLastError() : ERROR_DIRECTORY);
            continue;
        }
        Microsoft::WRL::ComPtr<IShellItem> item;
        HRESULT hr = SHCreateItemFromParsingName(result.path.c_str(), nullptr, IID_PPV_ARGS(&item));
        if (SUCCEEDED(hr)) {
            auto sink = Microsoft::WRL::Make<RecycleProgress>(&result);
            hr = sink ? operation->DeleteItem(item.Get(), sink.Get()) : E_OUTOFMEMORY;
        }
        if (FAILED(hr)) result.error = hr;
        else queued = true;
    }
    // One Shell batch: per-item callbacks retain successes even if later items fail.
    if (queued) operation->PerformOperations();
    return results;
}
}
