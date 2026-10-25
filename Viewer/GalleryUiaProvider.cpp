#include "stdafx.h"
#include "GalleryUiaProvider.h"
#include <wrl/implements.h>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <iterator>

#pragma comment(lib, "uiautomationcore.lib")

namespace q1view {
namespace {
double Clock() { LARGE_INTEGER n, f; QueryPerformanceCounter(&n); QueryPerformanceFrequency(&f); return double(n.QuadPart)/f.QuadPart; }
struct Item : GalleryAccessibleEntry { int id = 0; };
struct Items {
    std::wstring folder;
    std::vector<Item> values;
    std::unordered_map<int, int> indices;
};
struct Request {
    int item;
    GalleryAccessibleAction action;
    double value;
    HANDLE done = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    HRESULT result = UIA_E_TIMEOUT;
    ~Request() { if (done) CloseHandle(done); }
};
std::atomic<int> nextCanvas{0};
}
struct GalleryUiaState {
    std::mutex mutex;
    HWND window;
    // Non-owning canonical root. The UI-thread bridge owns it while alive;
    // Close invalidates the state under the same lock before releasing it.
    IRawElementProviderSimple* root = nullptr;
    DWORD thread;
    const int canvas = ++nextCanvas;
    bool alive = true, enabled = true;
#ifdef Q1VIEW_GALLERY_TESTS
    std::atomic<int> options{0}, properties{0}, types{0}, hosts{0}, liveProviders{0};
#endif
    int nextItem = 0, focus = -1, announcedFocus = -1;
    GalleryLayout layout;
    std::set<int> selected;
    std::shared_ptr<const Items> items = std::make_shared<Items>();
    UINT_PTR nextRequest = 0;
    std::unordered_map<UINT_PTR, std::shared_ptr<Request>> requests;
    bool Valid(int id) const { return alive && (!id || items->indices.count(id)); }
    int Index(int id) const { auto it=items->indices.find(id); return it==items->indices.end() ? -1 : it->second; }
    bool Focused() const {
        GUITHREADINFO info{sizeof(info)};
        return GetForegroundWindow()==GetAncestor(window,GA_ROOT) && GetGUIThreadInfo(thread,&info) && info.hwndFocus==window;
    }
    int FocusId() const { return focus>=0 && focus<int(items->values.size()) ? items->values[focus].id : 0; }
    HRESULT Execute(int id, GalleryAccessibleAction action, double value=0) {
        auto request=std::make_shared<Request>();
        if (!request->done) return E_OUTOFMEMORY;
        request->item=id; request->action=action; request->value=value;
        UINT_PTR serial;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (!Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
            if (!enabled || !IsWindowVisible(window) || !IsWindowEnabled(window) || !IsWindowEnabled(GetAncestor(window,GA_ROOT))) return UIA_E_ELEMENTNOTENABLED;
            if ((action==GalleryAccessibleAction::Add || action==GalleryAccessibleAction::Remove) &&
                (!id || !items->values[Index(id)].media)) return UIA_E_INVALIDOPERATION;
            serial=++nextRequest;
            requests.emplace(serial,request);
            if (!PostMessage(window,WM_GALLERY_UIA_ACTION,serial,0)) { requests.erase(serial); return UIA_E_ELEMENTNOTAVAILABLE; }
        }
        // No CWnd access or lock across a client call/wait. A timed-out queued
        // request is removed, so it cannot unexpectedly execute later.
        if (GetCurrentThreadId()==thread) SendMessage(window,WM_GALLERY_UIA_ACTION,serial,0);
        if (WaitForSingleObject(request->done,2000)==WAIT_OBJECT_0) return request->result;
        std::lock_guard<std::mutex> lock(mutex);
        requests.erase(serial);
        return UIA_E_TIMEOUT;
    }
};

namespace {
using Microsoft::WRL::ComPtr;
// Free-threaded COM objects: queries use a synchronized, immutable metadata
// snapshot. Nodes are allocated lazily, not once per directory entry.
class Node final : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
    IRawElementProviderSimple, IRawElementProviderFragment, IRawElementProviderFragmentRoot,
    ISelectionProvider, ISelectionItemProvider, IInvokeProvider, IScrollItemProvider, IScrollProvider, Microsoft::WRL::FtmBase> {
    std::shared_ptr<GalleryUiaState> s;
    int id;
    ComPtr<Node> Make(int item) { return Microsoft::WRL::Make<Node>(s,item); }
    HRESULT Array(const std::vector<int>& ids, SAFEARRAY** result) {
        *result=SafeArrayCreateVector(VT_UNKNOWN,0,ULONG(ids.size()));
        if (!*result) return E_OUTOFMEMORY;
        for (LONG i=0; i<LONG(ids.size()); ++i) {
            auto node=Make(ids[i]);
            if (!node) { SafeArrayDestroy(*result); *result=nullptr; return E_OUTOFMEMORY; }
            IRawElementProviderSimple* simple=node.Get();
            HRESULT hr=SafeArrayPutElement(*result,&i,simple);
            if (FAILED(hr)) { SafeArrayDestroy(*result); *result=nullptr; return hr; }
        }
        return S_OK;
    }
    UiaRect Bounds() const { // caller holds the snapshot lock
        RECT client{}; POINT origin{};
        if (!IsWindowVisible(s->window) || IsIconic(GetAncestor(s->window,GA_ROOT)) ||
            !GetClientRect(s->window,&client) || !ClientToScreen(s->window,&origin)) return {};
        if (!id) return {double(origin.x),double(origin.y),double(client.right),double(client.bottom)};
        int index=s->Index(id); if (index<0) return {};
        const auto r=s->layout.Rect(index,Clock());
        const double left=std::max(0.,double(r.x)),top=std::max(0.,double(r.y));
        const double right=std::min(double(client.right),double(r.x+r.size)),bottom=std::min(double(client.bottom),double(r.y+r.size));
        if (right<=left || bottom<=top) return {};
        return {origin.x+left,origin.y+top,right-left,bottom-top};
    }
public:
    Node(std::shared_ptr<GalleryUiaState> state,int item) : s(std::move(state)),id(item) {
#ifdef Q1VIEW_GALLERY_TESTS
        ++s->liveProviders;
#endif
    }
    ~Node() {
#ifdef Q1VIEW_GALLERY_TESTS
        --s->liveProviders;
#endif
    }
    IFACEMETHODIMP QueryInterface(REFIID iid,void** result) override {
        if (!result) return E_POINTER;
        // A list item is not another fragment root. Keep the COM interface
        // contract consistent with the patterns advertised by GetPatternProvider.
        if ((id && (iid==__uuidof(IRawElementProviderFragmentRoot) || iid==__uuidof(ISelectionProvider) || iid==__uuidof(IScrollProvider))) ||
            (!id && (iid==__uuidof(ISelectionItemProvider) || iid==__uuidof(IInvokeProvider) || iid==__uuidof(IScrollItemProvider)))) {
            *result=nullptr; return E_NOINTERFACE;
        }
        return __super::QueryInterface(iid,result);
    }
    IFACEMETHODIMP get_ProviderOptions(ProviderOptions* result) override {
#ifdef Q1VIEW_GALLERY_TESTS
        if(!id)++s->options;
#endif
        if (!result) return E_POINTER;
        *result=ProviderOptions(ProviderOptions_ServerSideProvider|ProviderOptions_UseComThreading|ProviderOptions_ProviderOwnsSetFocus); return S_OK;
    }
    IFACEMETHODIMP GetPatternProvider(PATTERNID pattern,IUnknown** result) override {
        if (!result) return E_POINTER; *result=nullptr;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!id && pattern==UIA_SelectionPatternId) return QueryInterface(IID_PPV_ARGS(reinterpret_cast<ISelectionProvider**>(result)));
        if (!id && pattern==UIA_ScrollPatternId) return QueryInterface(IID_PPV_ARGS(reinterpret_cast<IScrollProvider**>(result)));
        if (id && pattern==UIA_SelectionItemPatternId) return QueryInterface(IID_PPV_ARGS(reinterpret_cast<ISelectionItemProvider**>(result)));
        if (id && pattern==UIA_InvokePatternId) return QueryInterface(IID_PPV_ARGS(reinterpret_cast<IInvokeProvider**>(result)));
        if (id && pattern==UIA_ScrollItemPatternId) return QueryInterface(IID_PPV_ARGS(reinterpret_cast<IScrollItemProvider**>(result)));
        return S_OK;
    }
    IFACEMETHODIMP GetPropertyValue(PROPERTYID property,VARIANT* result) override {
        if (!result) return E_POINTER; VariantInit(result);
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        const int index=s->Index(id);
#ifdef Q1VIEW_GALLERY_TESTS
        if(!id) { ++s->properties; if(property==UIA_ControlTypePropertyId)++s->types; }
#endif
        auto boolean=[&](bool value) { result->vt=VT_BOOL; result->boolVal=value?VARIANT_TRUE:VARIANT_FALSE; };
        auto string=[&](const wchar_t* value) { result->vt=VT_BSTR; result->bstrVal=SysAllocString(value); return result->bstrVal?S_OK:E_OUTOFMEMORY; };
        switch(property) {
        case UIA_ControlTypePropertyId: result->vt=VT_I4; result->lVal=id?UIA_ListItemControlTypeId:UIA_ListControlTypeId; break;
        case UIA_NamePropertyId: return string(id?s->items->values[index].name.c_str():L"Thumbnail gallery");
        case UIA_AutomationIdPropertyId: return string(id?(L"GalleryItem_"+std::to_wstring(id)).c_str():L"ThumbnailGallery");
        case UIA_FrameworkIdPropertyId: return string(L"Q1View");
        case UIA_IsControlElementPropertyId: case UIA_IsContentElementPropertyId: case UIA_IsKeyboardFocusablePropertyId: boolean(true); break;
        case UIA_HasKeyboardFocusPropertyId: boolean(s->Focused() && (id?s->FocusId()==id:!s->FocusId())); break;
        case UIA_IsEnabledPropertyId: boolean(s->enabled && IsWindowEnabled(s->window) && IsWindowEnabled(GetAncestor(s->window,GA_ROOT))); break;
        case UIA_IsOffscreenPropertyId: { const auto r=Bounds(); boolean(r.width<=0 || r.height<=0); break; }
        }
        return S_OK;
    }
    IFACEMETHODIMP get_HostRawElementProvider(IRawElementProviderSimple** result) override {
#ifdef Q1VIEW_GALLERY_TESTS
        if(!id)++s->hosts;
#endif
        if (!result) return E_POINTER; *result=nullptr;
        { std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE; }
        return id?S_OK:UiaHostProviderFromHwnd(s->window,result);
    }
    IFACEMETHODIMP Navigate(NavigateDirection direction,IRawElementProviderFragment** result) override {
        if (!result) return E_POINTER; *result=nullptr;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        int target=-1,index=s->Index(id),count=int(s->items->values.size());
        if (!id && direction==NavigateDirection_FirstChild && count) target=s->items->values.front().id;
        if (!id && direction==NavigateDirection_LastChild && count) target=s->items->values.back().id;
        if (id && direction==NavigateDirection_Parent) target=0;
        if (id && direction==NavigateDirection_NextSibling && index+1<count) target=s->items->values[index+1].id;
        if (id && direction==NavigateDirection_PreviousSibling && index>0) target=s->items->values[index-1].id;
        if (!target) return s->root->QueryInterface(IID_PPV_ARGS(result));
        if (target>0) { auto node=Make(target); return node?node.CopyTo(result):E_OUTOFMEMORY; }
        return S_OK;
    }
    IFACEMETHODIMP GetRuntimeId(SAFEARRAY** result) override {
        if (!result) return E_POINTER; *result=nullptr;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!id) return S_OK; // HWND host supplies the root's runtime ID.
        *result=SafeArrayCreateVector(VT_I4,0,3); if (!*result) return E_OUTOFMEMORY;
        int values[]={UiaAppendRuntimeId,s->canvas,id};
        for(LONG i=0;i<3;++i) SafeArrayPutElement(*result,&i,&values[i]); return S_OK;
    }
    IFACEMETHODIMP get_BoundingRectangle(UiaRect* result) override {
        if (!result) return E_POINTER; *result={};
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        *result=Bounds(); return S_OK;
    }
    IFACEMETHODIMP GetEmbeddedFragmentRoots(SAFEARRAY** result) override {
        if (!result) return E_POINTER; *result=nullptr;
        std::lock_guard<std::mutex> lock(s->mutex); return s->Valid(id)?S_OK:UIA_E_ELEMENTNOTAVAILABLE;
    }
    IFACEMETHODIMP SetFocus() override { return s->Execute(id,GalleryAccessibleAction::Focus); }
    IFACEMETHODIMP get_FragmentRoot(IRawElementProviderFragmentRoot** result) override {
        if (!result) return E_POINTER; *result=nullptr;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        return s->root->QueryInterface(IID_PPV_ARGS(result));
    }
    IFACEMETHODIMP ElementProviderFromPoint(double x,double y,IRawElementProviderFragment** result) override {
        if (!result) return E_POINTER; *result=nullptr;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        const auto bounds=Bounds();
        if (bounds.width<=0 || x<bounds.left || y<bounds.top || x>=bounds.left+bounds.width || y>=bounds.top+bounds.height) return S_OK;
        POINT origin{}; ClientToScreen(s->window,&origin);
        int index=s->layout.Hit(float(x-origin.x),float(y-origin.y),Clock());
        int target=index>=0 && index<int(s->items->values.size()) ? s->items->values[index].id : 0;
        if (!target) return s->root->QueryInterface(IID_PPV_ARGS(result));
        auto node=Make(target); return node?node.CopyTo(result):E_OUTOFMEMORY;
    }
    IFACEMETHODIMP GetFocus(IRawElementProviderFragment** result) override {
        if (!result) return E_POINTER; *result=nullptr;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        if (!s->Focused()) return S_OK;
        const int target=s->FocusId();
        if (!target) return s->root->QueryInterface(IID_PPV_ARGS(result));
        auto node=Make(target); return node?node.CopyTo(result):E_OUTOFMEMORY;
    }
    IFACEMETHODIMP GetSelection(SAFEARRAY** result) override {
        if (!result) return E_POINTER; *result=nullptr;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        std::vector<int> ids; for (int i:s->selected) if(i>=0 && i<int(s->items->values.size())) ids.push_back(s->items->values[i].id);
        return Array(ids,result);
    }
    IFACEMETHODIMP get_CanSelectMultiple(BOOL* result) override {
        if(!result)return E_POINTER; *result=TRUE;
        std::lock_guard<std::mutex> lock(s->mutex); return s->Valid(id)?S_OK:UIA_E_ELEMENTNOTAVAILABLE;
    }
    IFACEMETHODIMP get_IsSelectionRequired(BOOL* result) override {
        if(!result)return E_POINTER; *result=FALSE;
        std::lock_guard<std::mutex> lock(s->mutex); return s->Valid(id)?S_OK:UIA_E_ELEMENTNOTAVAILABLE;
    }
    IFACEMETHODIMP Select() override { return s->Execute(id,GalleryAccessibleAction::Select); }
    IFACEMETHODIMP AddToSelection() override { return s->Execute(id,GalleryAccessibleAction::Add); }
    IFACEMETHODIMP RemoveFromSelection() override { return s->Execute(id,GalleryAccessibleAction::Remove); }
    IFACEMETHODIMP get_IsSelected(BOOL* result) override {
        if(!result)return E_POINTER; *result=FALSE;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        *result=s->selected.count(s->Index(id))?TRUE:FALSE; return S_OK;
    }
    IFACEMETHODIMP get_SelectionContainer(IRawElementProviderSimple** result) override {
        if (!result) return E_POINTER; *result=nullptr;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        return s->root->QueryInterface(IID_PPV_ARGS(result));
    }
    IFACEMETHODIMP Invoke() override { return s->Execute(id,GalleryAccessibleAction::Invoke); }
    IFACEMETHODIMP ScrollIntoView() override { return s->Execute(id,GalleryAccessibleAction::Reveal); }
    IFACEMETHODIMP Scroll(ScrollAmount horizontal,ScrollAmount vertical) override {
        if(horizontal!=ScrollAmount_NoAmount)return UIA_E_INVALIDOPERATION;
        if(vertical<ScrollAmount_LargeDecrement || vertical>ScrollAmount_SmallIncrement)return E_INVALIDARG;
        return s->Execute(0,GalleryAccessibleAction::ScrollAmount,double(vertical));
    }
    IFACEMETHODIMP SetScrollPercent(double horizontal,double vertical) override {
        if(horizontal!=UIA_ScrollPatternNoScroll)return UIA_E_INVALIDOPERATION;
        if(vertical==UIA_ScrollPatternNoScroll) { std::lock_guard<std::mutex> lock(s->mutex); return s->Valid(id)?S_OK:UIA_E_ELEMENTNOTAVAILABLE; }
        if(!std::isfinite(vertical) || vertical<0 || vertical>100)return E_INVALIDARG;
        return s->Execute(0,GalleryAccessibleAction::ScrollPercent,vertical);
    }
    IFACEMETHODIMP get_HorizontalScrollPercent(double* result) override {
        if(!result)return E_POINTER; *result=UIA_ScrollPatternNoScroll;
        std::lock_guard<std::mutex> lock(s->mutex); return s->Valid(id)?S_OK:UIA_E_ELEMENTNOTAVAILABLE;
    }
    IFACEMETHODIMP get_HorizontalViewSize(double* result) override {
        if(!result)return E_POINTER; *result=100;
        std::lock_guard<std::mutex> lock(s->mutex); return s->Valid(id)?S_OK:UIA_E_ELEMENTNOTAVAILABLE;
    }
    IFACEMETHODIMP get_HorizontallyScrollable(BOOL* result) override {
        if(!result)return E_POINTER; *result=FALSE;
        std::lock_guard<std::mutex> lock(s->mutex); return s->Valid(id)?S_OK:UIA_E_ELEMENTNOTAVAILABLE;
    }
    IFACEMETHODIMP get_VerticalScrollPercent(double* result) override {
        if(!result)return E_POINTER;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        *result=s->layout.MaxScroll()>0?100.*s->layout.scroll/s->layout.MaxScroll():UIA_ScrollPatternNoScroll; return S_OK;
    }
    IFACEMETHODIMP get_VerticalViewSize(double* result) override {
        if(!result)return E_POINTER;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        *result=100.*s->layout.height/(s->layout.height+s->layout.MaxScroll()); return S_OK;
    }
    IFACEMETHODIMP get_VerticallyScrollable(BOOL* result) override {
        if(!result)return E_POINTER;
        std::lock_guard<std::mutex> lock(s->mutex); if (!s->Valid(id)) return UIA_E_ELEMENTNOTAVAILABLE;
        *result=s->layout.MaxScroll()>0; return S_OK;
    }
};
ComPtr<Node> Make(const std::shared_ptr<GalleryUiaState>& state,int id=0) { return Microsoft::WRL::Make<Node>(state,id); }
}

GalleryUiaBridge::GalleryUiaBridge(HWND window,Action action) : mState(std::make_shared<GalleryUiaState>()),mAction(std::move(action)) {
    // Empty-document startup does not necessarily initialize COM. The FTM
    // must be created in an initialized apartment, independently of decoders.
    mComInit=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    mState->window=window; mState->thread=GetWindowThreadProcessId(window,nullptr);
    Make(mState).As(&mRoot);
    mState->root=mRoot.Get();
}
GalleryUiaBridge::~GalleryUiaBridge() { Close(); if(SUCCEEDED(mComInit))CoUninitialize(); }
#ifdef Q1VIEW_GALLERY_TESTS
std::wstring GalleryUiaBridge::Diagnostics() {
    return L"options="+std::to_wstring(mState->options.load())+L" properties="+std::to_wstring(mState->properties.load())+
        L" types="+std::to_wstring(mState->types.load())+L" hosts="+std::to_wstring(mState->hosts.load())+L" COM="+std::to_wstring(mComInit);
}
int GalleryUiaBridge::LiveProviders() const { return mState->liveProviders.load(); }
#endif
LRESULT GalleryUiaBridge::GetObject(WPARAM wp,LPARAM lp) {
    // Disconnect can re-enter WM_GETOBJECT. Never publish the provider that
    // Close has already invalidated, even before its final COM release.
    { std::lock_guard<std::mutex> lock(mState->mutex); if(!mState->alive)return 0; }
    return UiaReturnRawElementProvider(mState->window,wp,lp,mRoot.Get());
}
void GalleryUiaBridge::Entries(const std::wstring& folder,std::vector<GalleryAccessibleEntry> entries) {
    auto next=std::make_shared<Items>(); next->folder=folder;
    bool changed=false;
    {
        std::lock_guard<std::mutex> lock(mState->mutex);
        auto old=mState->items;
        std::map<std::pair<int,std::wstring>,int> previous;
        if (old->folder==folder) for (const auto& item:old->values) previous[{item.kind,item.path}]=item.id;
        for (auto& entry:entries) {
            Item item; static_cast<GalleryAccessibleEntry&>(item)=std::move(entry);
            auto it=previous.find({item.kind,item.path});
            item.id=it==previous.end()?++mState->nextItem:it->second;
            next->indices[item.id]=int(next->values.size()); next->values.push_back(std::move(item));
        }
        changed=next->values.size()!=old->values.size();
        if(!changed) for(size_t i=0;i<next->values.size();++i) if(next->values[i].id!=old->values[i].id) { changed=true; break; }
        mState->items=next; mState->selected.clear(); mState->focus=-1;
    }
    if(changed && UiaClientsAreListening()) UiaRaiseStructureChangedEvent(mRoot.Get(),StructureChangeType_ChildrenInvalidated,nullptr,0);
}
void GalleryUiaBridge::View(const GalleryLayout& layout,const std::set<int>& selected,int focus,bool enabled) {
    std::set<int> old,added,removed;
    int oldFocus,newFocus;
    {
        std::lock_guard<std::mutex> lock(mState->mutex);
        old=mState->selected; oldFocus=mState->FocusId();
        mState->layout=layout; mState->selected=selected; mState->focus=focus; mState->enabled=enabled;
        newFocus=mState->FocusId();
    }
    if (!UiaClientsAreListening()) return;
    std::set_difference(selected.begin(),selected.end(),old.begin(),old.end(),std::inserter(added,added.end()));
    std::set_difference(old.begin(),old.end(),selected.begin(),selected.end(),std::inserter(removed,removed.end()));
    // Bulk selection sends one container invalidation, not thousands of events.
    if(added.size()+removed.size()>20) UiaRaiseAutomationEvent(mRoot.Get(),UIA_Selection_InvalidatedEventId);
    else {
        auto emit=[&](int index,bool value) {
            int id=0;
            { std::lock_guard<std::mutex> lock(mState->mutex); if(index>=0 && index<int(mState->items->values.size())) id=mState->items->values[index].id; }
            if(!id)return;
            auto node=Make(mState,id);
            VARIANT before{},after{}; before.vt=after.vt=VT_BOOL; before.boolVal=value?VARIANT_FALSE:VARIANT_TRUE; after.boolVal=value?VARIANT_TRUE:VARIANT_FALSE;
            UiaRaiseAutomationPropertyChangedEvent(node.Get(),UIA_SelectionItemIsSelectedPropertyId,before,after);
            UiaRaiseAutomationEvent(node.Get(),value?(selected.size()==1?UIA_SelectionItem_ElementSelectedEventId:UIA_SelectionItem_ElementAddedToSelectionEventId):UIA_SelectionItem_ElementRemovedFromSelectionEventId);
        };
        for(int index:removed)emit(index,false); for(int index:added)emit(index,true);
    }
    if(oldFocus!=newFocus) FocusChanged();
}
void GalleryUiaBridge::Layout(const GalleryLayout& layout) {
    std::lock_guard<std::mutex> lock(mState->mutex);
    mState->layout=layout;
}
void GalleryUiaBridge::FocusChanged() {
    int id,previous;
    {
        std::lock_guard<std::mutex> lock(mState->mutex);
        if(!mState->alive)return;
        id=mState->Focused()?mState->FocusId():-1;
        previous=mState->announcedFocus;
        mState->announcedFocus=id;
        if(previous>=0 && !mState->Valid(previous))previous=-1;
    }
    if(id==previous || !UiaClientsAreListening())return;
    auto emit=[&](int item,bool focused) {
        if(item<0)return;
        ComPtr<IRawElementProviderSimple> node;
        if(!item)node=mRoot;
        else Make(mState,item).As(&node);
        VARIANT before{},after{}; before.vt=after.vt=VT_BOOL;
        before.boolVal=focused?VARIANT_FALSE:VARIANT_TRUE; after.boolVal=focused?VARIANT_TRUE:VARIANT_FALSE;
        UiaRaiseAutomationPropertyChangedEvent(node.Get(),UIA_HasKeyboardFocusPropertyId,before,after);
        if(focused)UiaRaiseAutomationEvent(node.Get(),UIA_AutomationFocusChangedEventId);
    };
    emit(previous,false); emit(id,true);
}
void GalleryUiaBridge::Dispatch(UINT_PTR serial) {
    std::shared_ptr<Request> request;
    int index;
    {
        std::lock_guard<std::mutex> lock(mState->mutex);
        auto it=mState->requests.find(serial); if(it==mState->requests.end())return;
        request=it->second; mState->requests.erase(it);
        if(!mState->Valid(request->item)) { request->result=UIA_E_ELEMENTNOTAVAILABLE; SetEvent(request->done); return; }
        index=mState->Index(request->item);
    }
    request->result=mAction(request->action,index,request->value);
    SetEvent(request->done);
}
void GalleryUiaBridge::Close() {
    if(!mState)return;
    {
        std::lock_guard<std::mutex> lock(mState->mutex);
        if(!mState->alive)return;
        mState->alive=false; mState->items=std::make_shared<Items>();
        mState->root=nullptr;
        for(auto& pending:mState->requests) { pending.second->result=UIA_E_ELEMENTNOTAVAILABLE; SetEvent(pending.second->done); }
        mState->requests.clear();
    }
    UiaReturnRawElementProvider(mState->window,0,0,nullptr);
    // Never make an outbound COM disconnect during a sent-message callback.
    // Stale nodes already return ELEMENTNOTAVAILABLE through the shared token.
    if(!InSendMessageEx(nullptr)) UiaDisconnectProvider(mRoot.Get());
    mRoot.Reset(); mAction={};
}
}
