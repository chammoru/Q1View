#pragma once
#include <afxcmn.h>
#include <oleacc.h>
#include <functional>
#include <shlwapi.h>
#include "Q1UiFontWin.h"
#include "QViewerCmn.h"

#pragma comment(lib, "oleacc.lib")
#pragma comment(lib, "uuid.lib")

// A real read-only edit retains its complete accessible value even when its
// unfocused presentation is elided. Focus restores native selection/scrolling.
class CBrowserPathBar : public CEdit {
    IAccPropServices* properties = nullptr;
public:
    ~CBrowserPathBar() { if (properties) properties->Release(); }
    static CString Compact(HDC dc, const CString& path, int width) {
        auto fits = [&](const CString& text) {
            SIZE size{}; return GetTextExtentPoint32W(dc, text, text.GetLength(), &size) && size.cx <= width;
        };
        if (fits(path)) return path;
        const wchar_t* rootEnd = PathSkipRootW(path);
        const int rootLength = rootEnd ? int(rootEnd - path.GetString()) : 0;
        CString normalized = path;
        while (normalized.GetLength() > rootLength && normalized.Right(1) == L"\\") normalized.Truncate(normalized.GetLength()-1);
        const int slash = normalized.ReverseFind(L'\\');
        CString leaf = normalized.Mid(slash+1);
        CString prefix;
        if (slash >= rootLength && !leaf.IsEmpty()) prefix = path.Left(rootLength) + L"\x2026\\";
        else if (rootLength > 0 && slash == rootLength-1 && !leaf.IsEmpty()) prefix = path.Left(rootLength);
        else leaf = normalized;
        if (fits(prefix+leaf)) return prefix+leaf;
        // A very long UNC root must not consume the space for the current leaf.
        if (!prefix.IsEmpty() && !fits(prefix+L"\x2026")) prefix = rootLength>3 ? CString(L"\\\\\x2026\\") : CString();
        int low=0, high=leaf.GetLength();
        while(low<high) {
            const int middle=low+(high-low+1)/2;
            if(fits(prefix+leaf.Left(middle)+L"\x2026")) low=middle; else high=middle-1;
        }
        // Do not split a UTF-16 surrogate pair at the final visible boundary.
        if(low>0 && low<leaf.GetLength() && IS_HIGH_SURROGATE(leaf[low-1]) && IS_LOW_SURROGATE(leaf[low])) --low;
        const CString result=prefix+leaf.Left(low)+L"\x2026";
        return fits(result) ? result : CString();
    }
    std::function<bool(MSG*)> keyHandler;
    CToolTipCtrl tooltip;
    void SetPath(const CString& value) {
        if (!GetSafeHwnd()) return;
        SetWindowText(value);
        if (tooltip.GetSafeHwnd()) tooltip.UpdateTipText(value, this, 1);
        Invalidate(FALSE);
    }
    BOOL PreTranslateMessage(MSG* message) override {
        if (tooltip.GetSafeHwnd()) tooltip.RelayEvent(message);
        if (keyHandler && keyHandler(message)) return TRUE;
        return CEdit::PreTranslateMessage(message);
    }
protected:
    LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override {
        if (message == WM_ERASEBKGND) {
            CRect rect; GetClientRect(rect);
            CDC* dc = CDC::FromHandle(HDC(wp));
            dc->FillSolidRect(rect, q1view::WindowsUiColorValue(q1view::WindowsUiColor::LocationHeader)); return 1;
        }
        if (message == WM_DESTROY && properties) {
            const MSAAPROPID name = PROPID_ACC_NAME;
            properties->ClearHwndProps(m_hWnd, OBJID_CLIENT, CHILDID_SELF, &name, 1);
            properties->Release(); properties = nullptr;
        }
        if (message == WM_PAINT && ::GetFocus() != m_hWnd) {
            CPaintDC dc(this);
            CRect rect; GetClientRect(rect);
            dc.FillSolidRect(rect, q1view::WindowsUiColorValue(q1view::WindowsUiColor::LocationHeader));
            CFont* old = dc.SelectObject(GetFont());
            dc.SetBkMode(TRANSPARENT); dc.SetTextColor(Q1UI_COLOR_TEXT);
            rect.DeflateRect(MulDiv(6, GetDpiForWindow(m_hWnd), 96), 0);
            CString text; GetWindowText(text);
            text = Compact(dc.GetSafeHdc(), text, rect.Width());
            dc.DrawText(text, rect, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            dc.SelectObject(old);
            return 0;
        }
        LRESULT result = CEdit::WindowProc(message, wp, lp);
        if (message == WM_SETFOCUS || message == WM_KILLFOCUS) Invalidate(FALSE);
        if (message == WM_CREATE) {
            tooltip.Create(this, TTS_ALWAYSTIP | TTS_NOPREFIX);
            tooltip.SetMaxTipWidth(700);
            tooltip.AddTool(this, L"", CRect(0, 0, 1, 1), 1);
            // Dynamic annotation leaves the native edit's value/selection intact.
            if (SUCCEEDED(CoCreateInstance(CLSID_AccPropServices, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&properties)))) {
                properties->SetHwndPropStr(m_hWnd, OBJID_CLIENT, CHILDID_SELF, PROPID_ACC_NAME, L"Current folder");
            }
        }
        if (message == WM_SIZE && tooltip.GetSafeHwnd()) {
            CRect rect; GetClientRect(rect); tooltip.SetToolRect(this, 1, rect);
        }
        return result;
    }
};

// Reuse the report list's native header allocation, also above the grid canvas.
// This avoids replacing the splitter, list, scroll coordinate system or focus
// routing just to add one line. The single native column is not interactive.
class CBrowserPathHeader : public CHeaderCtrl {
public:
    static int SeparatorThickness(UINT dpi) { return q1view::WindowsUiPixels(1, dpi); }
protected:
    void Paint(HDC dc) {
        CRect rect; GetClientRect(rect);
        CDC* target = CDC::FromHandle(dc);
        target->FillSolidRect(rect, q1view::WindowsUiColorValue(q1view::WindowsUiColor::LocationHeader));
        // A structural boundary, not a selection/focus outline. Leave the
        // native edit inset above it so text selection cannot cover the line.
        rect.top = (std::max)(rect.top, rect.bottom - SeparatorThickness(GetDpiForWindow(m_hWnd)));
        target->FillSolidRect(rect, Q1UI_COLOR_BORDER_SOFT);
    }
    LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override {
        if (message == WM_PAINT) {
            CPaintDC dc(this); Paint(dc.GetSafeHdc()); return 0;
        }
        if (message == WM_PRINTCLIENT || message == WM_ERASEBKGND) {
            Paint(HDC(wp)); return message == WM_ERASEBKGND ? 1 : 0;
        }
        if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLOREDIT) {
            ::SetBkColor(HDC(wp), q1view::WindowsUiColorValue(q1view::WindowsUiColor::LocationHeader));
            ::SetTextColor(HDC(wp), Q1UI_COLOR_TEXT);
            return LRESULT(q1view::WindowsUiColorBrush(q1view::WindowsUiColor::LocationHeader));
        }
        LRESULT result = CHeaderCtrl::WindowProc(message, wp, lp);
        if (message == HDM_LAYOUT && result) {
            HDLAYOUT* layout = reinterpret_cast<HDLAYOUT*>(lp);
            CClientDC dc(this); CFont* old = dc.SelectObject(GetFont());
            TEXTMETRIC metrics{}; dc.GetTextMetrics(&metrics); dc.SelectObject(old);
            const int height = metrics.tmHeight + MulDiv(10, GetDpiForWindow(m_hWnd), 96);
            layout->prc->top += height - layout->pwpos->cy;
            layout->pwpos->cy = height;
        }
        return result;
    }
};
