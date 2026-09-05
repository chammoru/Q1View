#include "stdafx.h"
#include "GalleryGridCanvas.h"
#include "ThumbnailPane.h"
#include "QViewerCmn.h"
#include "QDebug.h"
#include <shlwapi.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

namespace {
constexpr UINT_PTR AnimationTimer = 1;
constexpr UINT_PTR DecodeTimer = 2;
D2D1_COLOR_F Color(COLORREF c) {
    return D2D1::ColorF(GetRValue(c) / 255.0f, GetGValue(c) / 255.0f, GetBValue(c) / 255.0f);
}
D2D1_RECT_F Bounds(const q1view::GalleryRect& r, float inset = 0) {
    return D2D1::RectF(r.x + inset, r.y + inset, r.x + r.size - inset, r.y + r.size - inset);
}
}

BEGIN_MESSAGE_MAP(CGalleryGridCanvas, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_TIMER()
    ON_WM_MOUSEWHEEL()
    ON_WM_VSCROLL()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_RBUTTONDOWN()
    ON_WM_MOUSEMOVE()
    ON_WM_DESTROY()
    ON_MESSAGE(WM_MOUSELEAVE, OnMouseLeave)
    ON_MESSAGE(WM_DPICHANGED_AFTERPARENT, OnDpiChanged)
END_MESSAGE_MAP()

CGalleryGridCanvas::CGalleryGridCanvas(CThumbnailPane& owner) : mOwner(owner) {}
CGalleryGridCanvas::~CGalleryGridCanvas() { ClearCache(); }
double CGalleryGridCanvas::Now() {
    LARGE_INTEGER counter, frequency;
    QueryPerformanceCounter(&counter); QueryPerformanceFrequency(&frequency);
    return double(counter.QuadPart) / frequency.QuadPart;
}
BOOL CGalleryGridCanvas::CreateCanvas() {
    auto cls = AfxRegisterWndClass(CS_DBLCLKS, LoadCursor(nullptr, IDC_ARROW));
    if (!CreateEx(0, cls, _T("Thumbnail gallery"), WS_CHILD | WS_VSCROLL | WS_TABSTOP,
        CRect(0, 0, 1, 1), &mOwner, 0x7180)) return FALSE;
    mTooltip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX);
    mTooltip.AddTool(this, _T(""), CRect(0, 0, 1, 1), 1);
    mTooltip.SetMaxTipWidth(600);
    return TRUE;
}
void CGalleryGridCanvas::ClearCache() {
    for (auto& p : mCache) if (p.second.cpu) DeleteObject(p.second.cpu);
    mCache.clear(); mCacheBytes = 0;
}
void CGalleryGridCanvas::Reset() {
    KillTimer(AnimationTimer); KillTimer(DecodeTimer);
    ClearCache(); mSelected = mHover = -1; mTracking = false;
    CRect rc; GetClientRect(&rc);
    mLayout.Reset(int(mOwner.mEntries.size()), mOwner.GridColsForStep(mOwner.mViewStep),
        float(std::max(1, rc.Width())), float(std::max(1, rc.Height())), GetDpiForWindow(m_hWnd) / 96.0f);
    Relayout(false);
}
void CGalleryGridCanvas::Relayout(bool animate) {
    if (!GetSafeHwnd()) return;
    CRect rc; GetClientRect(&rc);
    float dpi = GetDpiForWindow(m_hWnd) / 96.0f;
    float width = float(rc.Width());
    if (mOwner.mSlideWidth > 0)
        width = float(mOwner.mSlideWidth - GetSystemMetricsForDpi(SM_CXVSCROLL, GetDpiForWindow(m_hWnd)));
    mLayout.Retarget(mOwner.GridColsForStep(mOwner.mViewStep), width,
        float(rc.Height()), dpi, mSelected, Now(), animate);
    // Decode sizes are bounded even on very wide/high-DPI drawers.
    mOwner.mThumb = std::min(1024, std::max(16, int(std::ceil(mLayout.Pitch()))));
    UpdateScrollBar();
    Invalidate(FALSE);
    SetTimer(AnimationTimer, 16, nullptr);
    SetTimer(DecodeTimer, 220, nullptr); // settle first, then replace low-resolution tiles
}
void CGalleryGridCanvas::Select(int index, bool reveal) {
    mSelected = index >= 0 && index < mLayout.count ? index : -1;
    if (reveal) { mLayout.Reveal(mSelected); UpdateScrollBar(); }
    Invalidate(FALSE);
    SetTimer(DecodeTimer, 90, nullptr);
}
void CGalleryGridCanvas::PauseAnimation() {
    // Called when the drawer is hidden: do not spend UI/GPU time behind it.
    KillTimer(AnimationTimer); KillTimer(DecodeTimer);
}
void CGalleryGridCanvas::UpdateScrollBar() {
    SCROLLINFO si = { sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS | SIF_DISABLENOSCROLL };
    si.nMin = 0; si.nMax = int(std::ceil(mLayout.MaxScroll() + mLayout.height)) - 1;
    si.nPage = UINT(std::max(1.0f, mLayout.height)); si.nPos = int(mLayout.scroll);
    SetScrollInfo(SB_VERT, &si, TRUE);
}
void CGalleryGridCanvas::Scroll(float position) {
    mLayout.ScrollTo(position); UpdateScrollBar(); Invalidate(FALSE);
    SetTimer(DecodeTimer, 90, nullptr);
}
void CGalleryGridCanvas::TrimCache() {
    while ((mCacheBytes > CacheBudget || mCache.size() > CacheEntryLimit) && !mCache.empty()) {
        auto oldest = std::min_element(mCache.begin(), mCache.end(),
            [](const auto& a, const auto& b) { return a.second.used < b.second.used; });
        mCacheBytes -= oldest->second.bytes;
        DeleteObject(oldest->second.cpu); mCache.erase(oldest);
    }
}
void CGalleryGridCanvas::Accept(int index, HBITMAP bitmap, int size) {
    auto existing = mCache.find(index);
    if (existing != mCache.end() && existing->second.size >= size) { DeleteObject(bitmap); return; }
    auto& cached = mCache[index];
    if (cached.cpu) DeleteObject(cached.cpu);
    mCacheBytes -= cached.bytes;
    cached.cpu = bitmap; cached.size = size; cached.bytes = size_t(size) * size * 4;
    cached.used = ++mUse; cached.gpu.Reset(); mCacheBytes += cached.bytes;
    TrimCache(); Invalidate(FALSE);
    SetTimer(DecodeTimer, 90, nullptr);
}
void CGalleryGridCanvas::QueueVisible() {
    if (!IsWindowVisible() || !mOwner.IsGrid()) return;
    // Remove obsolete queued work, but keep the <=4 tasks already decoding.
    {
        std::lock_guard<std::mutex> lock(mOwner.mMutex);
        for (const auto& task : mOwner.mTasks)
            if (task.gen == mOwner.mGen && task.index < int(mOwner.mEntries.size()))
                mOwner.mEntries[task.index].queued = false;
        mOwner.mTasks.clear();
    }
    auto visible = mLayout.Visible(Now());
    auto nearby = mLayout.Visible(Now(), 2);
    visible.insert(visible.end(), nearby.begin(), nearby.end());
    for (int i : visible) {
        auto& entry = mOwner.mEntries[i];
        auto cached = mCache.find(i);
        if (cached != mCache.end()) {
            cached->second.used = ++mUse;
            if (cached->second.size >= mOwner.mThumb) continue;
        }
        if (!entry.queued && !entry.badge) {
            entry.queued = true;
            mOwner.QueueThumb(i, entry.path);
        }
    }
}
CString CGalleryGridCanvas::Label(int index) const {
    CString ext = PathFindExtension(mOwner.mEntries[index].path);
    if (!ext.IsEmpty()) ext = ext.Mid(1);
    ext.MakeUpper(); return ext.Left(12);
}
void CGalleryGridCanvas::DropDevice() {
    for (auto& p : mCache) p.second.gpu.Reset();
    if (mContext) mContext->SetTarget(nullptr);
    mTarget.Reset(); mBrush.Reset(); mContext.Reset(); mD2Device.Reset();
    mSwapChain.Reset(); mImmediate.Reset(); mDevice.Reset(); mFactory.Reset();
    mText.Reset(); mWriteFactory.Reset(); mWidth = mHeight = 0;
}
bool CGalleryGridCanvas::EnsureDevice(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (GetEnvironmentVariableW(L"Q1VIEW_DISABLE_GALLERY_GPU", nullptr, 0)) return false;
    if (!mDevice) {
        D3D_FEATURE_LEVEL level;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &mDevice, &level, &mImmediate);
        if (FAILED(hr)) hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &mDevice, &level, &mImmediate);
        if (FAILED(hr)) return false;
        D2D1_FACTORY_OPTIONS options = {};
        if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory1),
            &options, reinterpret_cast<void**>(mFactory.GetAddressOf())))) return false;
        Ptr<IDXGIDevice> dxgi;
        if (FAILED(mDevice.As(&dxgi)) || FAILED(mFactory->CreateDevice(dxgi.Get(), &mD2Device)) ||
            FAILED(mD2Device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &mContext))) return false;
        Ptr<IDXGIAdapter> adapter; Ptr<IDXGIFactory2> factory;
        if (FAILED(dxgi->GetAdapter(&adapter)) || FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;
        DXGI_SWAP_CHAIN_DESC1 desc = {};
        desc.Width = width; desc.Height = height; desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = 2; desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        if (FAILED(factory->CreateSwapChainForHwnd(mDevice.Get(), m_hWnd, &desc, nullptr, nullptr, &mSwapChain))) return false;
        factory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(mContext->CreateSolidColorBrush(Color(Q1UI_COLOR_ACCENT), &mBrush))) return false;
        if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(mWriteFactory.GetAddressOf())))) return false;
        if (FAILED(mWriteFactory->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 13 * GetDpiForWindow(m_hWnd) / 96.0f, L"en-us", &mText))) return false;
        mText->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
        mText->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
        mContext->SetDpi(96, 96); // layout and pointer coordinates are physical pixels
        LOGINF("%s", "Gallery Direct2D/D3D11 renderer active");
    }
    if (width != mWidth || height != mHeight) {
        mContext->SetTarget(nullptr); mTarget.Reset();
        if (FAILED(mSwapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0))) return false;
        Ptr<IDXGISurface> surface;
        if (FAILED(mSwapChain->GetBuffer(0, IID_PPV_ARGS(&surface)))) return false;
        auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
        if (FAILED(mContext->CreateBitmapFromDxgiSurface(surface.Get(), &props, &mTarget))) return false;
        mContext->SetTarget(mTarget.Get()); mWidth = width; mHeight = height;
    }
    return true;
}
bool CGalleryGridCanvas::PaintGpu(const std::vector<int>& visible, double now, bool& pending) {
    CRect client; GetClientRect(&client);
    if (!EnsureDevice(client.Width(), client.Height())) return false;
    int uploads = 0;
    mContext->BeginDraw(); mContext->Clear(Color(Q1UI_COLOR_SURFACE_ALT));
    float dpi = GetDpiForWindow(m_hWnd) / 96.0f;
    for (int i : visible) {
        auto r = mLayout.Rect(i, now);
        if (r.y + r.size < 0 || r.y > mLayout.height) continue;
        auto cached = mCache.find(i);
        ID2D1Bitmap1* bitmap = nullptr;
        if (cached != mCache.end()) {
            auto& c = cached->second; c.used = ++mUse;
            if (!c.gpu && uploads < 2) {
                DIBSECTION dib = {};
                if (GetObject(c.cpu, sizeof(dib), &dib) && dib.dsBm.bmBits) {
                    auto props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_NONE,
                        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE));
                    HRESULT hr = mContext->CreateBitmap(D2D1::SizeU(c.size, c.size), dib.dsBm.bmBits,
                        dib.dsBm.bmWidthBytes, &props, &c.gpu);
                    if (FAILED(hr)) { mContext->EndDraw(); return false; }
                    ++uploads;
                }
            }
            bitmap = c.gpu.Get();
            if (!bitmap) pending = true;
        }
        if (bitmap) mContext->DrawBitmap(bitmap, Bounds(r), 1, D2D1_INTERPOLATION_MODE_HIGH_QUALITY_CUBIC);
        else {
            mBrush->SetColor(Color(Q1UI_COLOR_SURFACE)); mContext->FillRectangle(Bounds(r), mBrush.Get());
            CString label = Label(i); mBrush->SetColor(Color(Q1UI_COLOR_TEXT));
            auto rect = Bounds(r);
            mContext->DrawText(label, label.GetLength(), mText.Get(), rect, mBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_CLIP);
        }
        if (i == mSelected || i == mHover) {
            mBrush->SetColor(Color(i == mSelected ? Q1UI_COLOR_ACCENT : Q1UI_COLOR_TEXT));
            float stroke = (i == mSelected ? 3.0f : 1.0f) * dpi;
            mContext->DrawRectangle(Bounds(r, stroke / 2), mBrush.Get(), stroke);
        }
    }
    HRESULT hr = mContext->EndDraw();
    if (FAILED(hr)) return false;
    // Never wait for a second vertical sync on the video's UI thread.
    hr = mSwapChain->Present(0, DXGI_PRESENT_DO_NOT_WAIT);
    if (hr == DXGI_ERROR_WAS_STILL_DRAWING) { pending = true; return true; }
    return SUCCEEDED(hr);
}
void CGalleryGridCanvas::PaintFallback(CDC& dc, const std::vector<int>& visible, double now) {
    CRect client; GetClientRect(&client);
    CDC memory; memory.CreateCompatibleDC(&dc);
    CBitmap buffer; buffer.CreateCompatibleBitmap(&dc, std::max(1, client.Width()), std::max(1, client.Height()));
    auto old = memory.SelectObject(&buffer);
    memory.FillSolidRect(client, Q1UI_COLOR_SURFACE_ALT);
    memory.SetBkMode(TRANSPARENT); memory.SetTextColor(Q1UI_COLOR_TEXT);
    memory.SetStretchBltMode(HALFTONE); memory.SetBrushOrg(0, 0);
    for (int i : visible) {
        auto r = mLayout.Rect(i, now);
        CRect rect(int(r.x), int(r.y), int(r.x + r.size), int(r.y + r.size));
        auto it = mCache.find(i);
        if (it != mCache.end()) {
            DIBSECTION dib = {}; GetObject(it->second.cpu, sizeof(dib), &dib);
            BITMAPINFO info = {}; info.bmiHeader = dib.dsBmih;
            StretchDIBits(memory, rect.left, rect.top, rect.Width(), rect.Height(), 0, 0,
                it->second.size, it->second.size, dib.dsBm.bmBits, &info, DIB_RGB_COLORS, SRCCOPY);
        } else { memory.FillSolidRect(rect, Q1UI_COLOR_SURFACE); memory.DrawText(Label(i), rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE); }
        if (i == mSelected || i == mHover) {
            COLORREF color = i == mSelected ? Q1UI_COLOR_ACCENT : Q1UI_COLOR_TEXT;
            int stroke = std::max(1, MulDiv(i == mSelected ? 3 : 1, GetDpiForWindow(m_hWnd), 96));
            for (int s = 0; s < stroke; ++s) { memory.Draw3dRect(rect, color, color); rect.DeflateRect(1, 1); }
        }
    }
    dc.BitBlt(0, 0, client.Width(), client.Height(), &memory, 0, 0, SRCCOPY);
    memory.SelectObject(old);
}
void CGalleryGridCanvas::OnPaint() {
    CPaintDC dc(this);
    if (!IsWindowVisible()) return;
    auto now = Now(); auto visible = mLayout.Visible(now); bool pending = false;
    if (mTracking) mHover = mLayout.Hit(float(mPointer.x), float(mPointer.y), now);
    if (mLayout.Animating(now)) mTooltip.Pop();
    if (now < mRetryAt || !PaintGpu(visible, now, pending)) {
        if (now >= mRetryAt) { DropDevice(); mRetryAt = now + 5; }
        PaintFallback(dc, visible, now);
    }
    if (pending || mLayout.Animating(now)) SetTimer(AnimationTimer, 16, nullptr);
    else KillTimer(AnimationTimer);
}
BOOL CGalleryGridCanvas::OnEraseBkgnd(CDC*) { return TRUE; }
void CGalleryGridCanvas::OnSize(UINT type, int cx, int cy) {
    CWnd::OnSize(type, cx, cy);
    if (mTooltip.GetSafeHwnd()) mTooltip.SetToolRect(this, 1, CRect(0, 0, cx, cy));
    if (mOwner.IsGrid() && !mOwner.mResizing) Relayout(false);
}
void CGalleryGridCanvas::OnTimer(UINT_PTR id) {
    if (!IsWindowVisible()) { PauseAnimation(); return; }
    if (id == AnimationTimer) Invalidate(FALSE);
    else if (id == DecodeTimer) { KillTimer(id); QueueVisible(); }
    else CWnd::OnTimer(id);
}
BOOL CGalleryGridCanvas::OnMouseWheel(UINT flags, short delta, CPoint point) {
    if (flags & MK_CONTROL) {
        mWheel += delta;
        int steps = mWheel / WHEEL_DELTA; mWheel %= WHEEL_DELTA;
        if (steps) mOwner.ApplyViewStep(mOwner.mViewStep + steps, true);
    } else Scroll(mLayout.scroll - delta / float(WHEEL_DELTA) * mLayout.Pitch());
    return TRUE;
}
void CGalleryGridCanvas::OnVScroll(UINT code, UINT, CScrollBar*) {
    SCROLLINFO si = { sizeof(si), SIF_TRACKPOS }; GetScrollInfo(SB_VERT, &si);
    float target = mLayout.scroll;
    switch (code) {
    case SB_LINEUP: target -= mLayout.Pitch(); break;
    case SB_LINEDOWN: target += mLayout.Pitch(); break;
    case SB_PAGEUP: target -= mLayout.height; break;
    case SB_PAGEDOWN: target += mLayout.height; break;
    case SB_TOP: target = 0; break;
    case SB_BOTTOM: target = mLayout.MaxScroll(); break;
    case SB_THUMBTRACK: case SB_THUMBPOSITION: target = float(si.nTrackPos); break;
    default: return;
    }
    Scroll(target);
}
void CGalleryGridCanvas::OnLButtonDown(UINT, CPoint point) {
    SetFocus(); Select(mLayout.Hit(float(point.x), float(point.y), Now()), false);
}
void CGalleryGridCanvas::OnRButtonDown(UINT flags, CPoint point) { OnLButtonDown(flags, point); }
void CGalleryGridCanvas::OnLButtonDblClk(UINT flags, CPoint point) {
    OnLButtonDown(flags, point); mOwner.ActivateIndex(mSelected, false);
}
void CGalleryGridCanvas::OnMouseMove(UINT, CPoint point) {
    mPointer = point; mTracking = true;
    int hit = mLayout.Hit(float(point.x), float(point.y), Now());
    if (hit != mHover) {
        mHover = hit; Invalidate(FALSE);
        mTooltip.UpdateTipText(hit >= 0 ? mOwner.mEntries[hit].path.GetString() : _T(""), this, 1);
    }
    TRACKMOUSEEVENT track = { sizeof(track), TME_LEAVE, m_hWnd, 0 }; TrackMouseEvent(&track);
}
LRESULT CGalleryGridCanvas::OnMouseLeave(WPARAM, LPARAM) { mTracking = false; mHover = -1; Invalidate(FALSE); return 0; }
LRESULT CGalleryGridCanvas::OnDpiChanged(WPARAM, LPARAM) { DropDevice(); Relayout(false); return 0; }
void CGalleryGridCanvas::OnDestroy() { PauseAnimation(); ClearCache(); DropDevice(); CWnd::OnDestroy(); }
BOOL CGalleryGridCanvas::PreTranslateMessage(MSG* message) {
    if (mTooltip.GetSafeHwnd()) mTooltip.RelayEvent(message);
    if (message->message == WM_KEYDOWN) {
        int target = mSelected < 0 ? 0 : mSelected;
        switch (message->wParam) {
        case VK_LEFT: case VK_PRIOR: --target; break;
        case VK_RIGHT: case VK_NEXT: ++target; break;
        case VK_UP: target -= mLayout.columns; break;
        case VK_DOWN: target += mLayout.columns; break;
        case VK_HOME: target = 0; break;
        case VK_END: target = mLayout.count - 1; break;
        case VK_RETURN: mOwner.ActivateIndex(mSelected, false); return TRUE;
        default: return CWnd::PreTranslateMessage(message);
        }
        Select(std::max(0, std::min(mLayout.count - 1, target)), true); return TRUE;
    }
    return CWnd::PreTranslateMessage(message);
}
