#pragma once

#include "GalleryLayout.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d2d1_1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <map>

class CThumbnailPane;

// Grid-only child canvas. The parent retains folder identity, activation and
// decode workers; this window owns layout, input and disposable GPU resources.
class CGalleryGridCanvas : public CWnd {
public:
    explicit CGalleryGridCanvas(CThumbnailPane& owner);
    ~CGalleryGridCanvas();
    BOOL CreateCanvas();
    void Reset();
    void Relayout(bool animate);
    void Select(int index, bool reveal);
    int Selection() const { return mSelected; }
    void Accept(int index, HBITMAP bitmap, int size); // takes ownership
    void QueueVisible();
    void PauseAnimation();
    virtual BOOL PreTranslateMessage(MSG* message);

protected:
    DECLARE_MESSAGE_MAP()
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC*);
    afx_msg void OnSize(UINT, int, int);
    afx_msg void OnTimer(UINT_PTR);
    afx_msg BOOL OnMouseWheel(UINT, short, CPoint);
    afx_msg void OnVScroll(UINT, UINT, CScrollBar*);
    afx_msg void OnLButtonDown(UINT, CPoint);
    afx_msg void OnLButtonDblClk(UINT, CPoint);
    afx_msg void OnRButtonDown(UINT, CPoint);
    afx_msg void OnMouseMove(UINT, CPoint);
    afx_msg LRESULT OnMouseLeave(WPARAM, LPARAM);
    afx_msg LRESULT OnDpiChanged(WPARAM, LPARAM);
    afx_msg void OnDestroy();
private:
	friend struct GalleryIntegrationTests;
    template<class T> using Ptr = Microsoft::WRL::ComPtr<T>;
    struct Cached {
        HBITMAP cpu = nullptr;
        int size = 0;
        size_t bytes = 0;
        unsigned long long used = 0;
        Ptr<ID2D1Bitmap1> gpu;
    };
    static double Now();
    void Scroll(float position);
    void UpdateScrollBar();
    void ClearCache();
    void TrimCache();
    bool EnsureDevice(int width, int height);
    void DropDevice();
    bool PaintGpu(const std::vector<int>& visible, double now, bool& pending);
    void PaintFallback(CDC& dc, const std::vector<int>& visible, double now);
    CString Label(int index) const;
    CThumbnailPane& mOwner;
    q1view::GalleryLayout mLayout;
    int mSelected = -1, mHover = -1, mWheel = 0;
    CPoint mPointer;
    bool mTracking = false;
    CToolTipCtrl mTooltip;
    std::map<int, Cached> mCache;
    size_t mCacheBytes = 0;
    unsigned long long mUse = 0;
    static const size_t CacheBudget = 64 * 1024 * 1024; // CPU and GPU each <= 64 MiB
    static const size_t CacheEntryLimit = 512; // also bound GDI handles for tiny thumbnails
    Ptr<ID3D11Device> mDevice;
    Ptr<ID3D11DeviceContext> mImmediate;
    Ptr<ID2D1Factory1> mFactory;
    Ptr<ID2D1Device> mD2Device;
    Ptr<ID2D1DeviceContext> mContext;
    Ptr<IDXGISwapChain1> mSwapChain;
    Ptr<ID2D1Bitmap1> mTarget;
    Ptr<ID2D1SolidColorBrush> mBrush;
    Ptr<IDWriteFactory> mWriteFactory;
    Ptr<IDWriteTextFormat> mText;
    int mWidth = 0, mHeight = 0;
    double mRetryAt = 0;
};
