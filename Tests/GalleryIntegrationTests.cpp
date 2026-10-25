// Built only with /p:Q1ViewGalleryTests=true. Exercises the real MFC pane,
// worker queue and D3D11/Direct2D renderer, using isolated app preferences.
#include "../Viewer/stdafx.h"
#include "../Viewer/Viewer.h"
#include "../Viewer/MainFrm.h"
#include "../Viewer/ViewerDoc.h"
#include "../Viewer/ViewerView.h"
#include "../Viewer/FrmSrc.h"
#include "../Viewer/Y4mReaderWin.h"
#include "../Viewer/ThumbnailPane.h"
#include "../Viewer/GalleryGridCanvas.h"
#include "QViewerCmn.h"
#include "QImageStr.h"
#include "QCvUtil.h"
#include "QFileActionsWin.h"
#include "QRecycleFilesWin.h"
#include "../Viewer/ViewerWindowGeometry.h"
#include "Q1UiAppearanceMenuWin.h"
#include "SharedThemeTestWriter.h"
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <functional>
#include <opencv2/videoio.hpp>
#include "WindowsUiPaintTestProbe.h"

struct GalleryIntegrationTests {
    FILE* report = nullptr;
    std::function<void()> afterMessage;
    void Require(bool okay, const char* text) {
        fprintf(report, "%s: %s\n", okay ? "PASS" : "FAIL", text); fflush(report);
        if (!okay) throw std::runtime_error(text);
    }
    void Pump(double seconds, std::function<void()> action = {}) {
        double end = CGalleryGridCanvas::Now() + seconds;
        while (CGalleryGridCanvas::Now() < end) {
            const double batchEnd = CGalleryGridCanvas::Now() + .008;
            MSG msg;
            while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
                if (msg.message == WM_QUIT) throw std::runtime_error("unexpected quit");
                if (!AfxGetApp()->PreTranslateMessage(&msg)) { TranslateMessage(&msg); DispatchMessage(&msg); }
                if (afterMessage) afterMessage();
                // Keep synthetic zoom input timely even under a continuous
                // stream of playback messages; this is not a UI-stall metric.
                if (CGalleryGridCanvas::Now() >= std::min(end, batchEnd)) break;
            }
            if (action) action();
            Sleep(1);
        }
    }
    void Await(std::function<bool()> predicate, const char* text) {
        double end = CGalleryGridCanvas::Now() + 10;
        while (!predicate() && CGalleryGridCanvas::Now() < end) Pump(.05);
        Require(predicate(), text);
    }
    void FolderCaptionTests(CMainFrame* frame) {
        auto& pane = *frame->mpDrawer;
        auto& grid = *pane.mGrid;
        const int oldStep = pane.mViewStep;
        pane.ApplyViewStep(3, false); Pump(.1);
        CRect viewport; grid.GetClientRect(viewport);
        Require(grid.EnsureDevice(viewport.Width(), viewport.Height()), "folder caption checks use the real DirectWrite factory and bundled font");
        const auto originalFormat = grid.mText;
        struct Restore { std::function<void()> action; ~Restore() { action(); } } restore{
            [&] { grid.mText = originalFormat; pane.ApplyViewStep(oldStep, false); }
        };
        const CString names[] = {
            L"[Short]",
            L"[\uAE34 \uD55C\uAE00 \uD3F4\uB354\uBA85 \uC5EC\uD589 \uC0AC\uC9C4 \uC6D0\uBCF8 \uBCF4\uC815 \uBAA8\uC74C]",
            L"[This is a very long English folder name with spaces that should wrap before the final visible ellipsis]",
            L"[ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz012345678901234567890123456789]",
            L"[2026 \uCE90\uB098\uB2E4 Canada \uC5EC\uD589 Original photos 012345678901234567890123456789]",
            L"[\U0001f4f7 cafe\u0301 \uC0AC\uC9C4 \U0001f1e8\U0001f1e6 \uBCF4\uC815 \U0001f4f7 cafe\u0301 original photos]",
            L"[[Literal brackets] A&B && \uD3F4\uB354]"
        };
        CClientDC source(&grid);
        CDC dc; dc.CreateCompatibleDC(&source);
        BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=512; info.bmiHeader.biHeight=-512; info.bmiHeader.biPlanes=1;
        info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
        void* raw=nullptr;
        CBitmap bitmap; bitmap.Attach(CreateDIBSection(dc,&info,DIB_RGB_COLORS,&raw,nullptr,0));
        Require(bitmap.GetSafeHandle() && raw, "fallback caption checks allocate an isolated bitmap, not desktop pixels");
        CBitmap* oldBitmap=dc.SelectObject(&bitmap);
        struct RestoreBitmap { CDC& dc; CBitmap* old; ~RestoreBitmap(){dc.SelectObject(old);} } restoreBitmap{dc,oldBitmap};
        const auto pixels=static_cast<DWORD*>(raw);
        for(UINT dpi : {96u,120u,144u,192u}) for(double scale : {1.0,1.5,2.0}) {
            const float fontSize=float(q1view::WindowsUiPixels(13,dpi,scale));
            Require(SUCCEEDED(grid.mWriteFactory->CreateTextFormat(pane.mFontFamily,grid.mFontCollection.Get(),DWRITE_FONT_WEIGHT_MEDIUM,
                DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,fontSize,L"ko-kr",&grid.mText)), "caption matrix creates the unchanged medium caption font");
            grid.mText->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            grid.mText->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            for(int drawer : {210,360}) for(int columns=1;columns<=5;++columns) {
                const float edge=float(drawer)*dpi/96/columns-float(dpi)/96;
                const float pad=std::max(4.f,edge*.08f), available=edge-2*pad;
                for(const CString& name : names) {
                    Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
                    const bool created=grid.CreateFolderTextLayout(name,available,available,layout);
                    if(!created) {
                        Microsoft::WRL::ComPtr<IDWriteTextLayout> probe;
                        grid.mWriteFactory->CreateTextLayout(name,name.GetLength(),grid.mText.Get(),available,10000,&probe);
                        UINT32 count=0; probe->GetLineMetrics(nullptr,0,&count);
                        std::vector<DWRITE_LINE_METRICS> lines(count); probe->GetLineMetrics(lines.data(),count,&count);
                        Require(count && lines.front().height>available,"too-small cards suppress partial glyph rows without reducing font size");
                        continue;
                    }
                    DWRITE_TEXT_METRICS metrics{}; layout->GetMetrics(&metrics);
                    FLOAT actualFontSize=0; layout->GetFontSize(0,&actualFontSize);
                    UINT32 count=0; layout->GetLineMetrics(nullptr,0,&count);
                    std::vector<DWRITE_LINE_METRICS> lines(count); layout->GetLineMetrics(lines.data(),count,&count);
                    // DirectWrite may retain zero-height clipped logical lines.
                    // The last *visible* line, not vector.back(), must trim.
                    float height=0; UINT32 visible=0;
                    for(const auto& line : lines) {
                        if(line.height<=0 || height+line.height>layout->GetMaxHeight()+.02f) break;
                        height+=line.height; ++visible;
                    }
                    for(UINT32 i=0;i<visible;++i) {
                        if(lines[i].isTrimmed && i!=visible-1) fprintf(report,"CAPTION: dpi=%u scale=%.1f drawer=%d columns=%d name=%d row=%u visible=%u logical=%u maxHeight=%.3f lineHeight=%.3f\n",
                            dpi,scale,drawer,columns,int(&name-names),i,visible,count,layout->GetMaxHeight(),lines[i].height);
                        Require(!lines[i].isTrimmed || i==visible-1,"only the final visible caption line can have an ellipsis");
                    }
                    if(visible<count) Require(visible && lines[visible-1].isTrimmed,"overflow text trims at the final visible line, not a clipped logical line");
                    Require(layout->GetTextAlignment()==DWRITE_TEXT_ALIGNMENT_LEADING && layout->GetParagraphAlignment()==DWRITE_PARAGRAPH_ALIGNMENT_NEAR &&
                        std::abs(metrics.left)<.01f && std::abs(metrics.top)<.01f && height<=available+.02f &&
                        layout->GetMaxHeight()<=available+.02f && actualFontSize==fontSize,
                        "ordinary Korean/English/unbroken/mixed captions are top-left aligned inside complete rows at every size and text scale");
                    Require(visible>=1,"trimmed layouts retain a complete visible line");
                }
            }
            Microsoft::WRL::ComPtr<IDWriteTextLayout> parent;
            Require(grid.CreateFolderTextLayout(L"[..]",200,180,parent,true) && parent->GetTextAlignment()==DWRITE_TEXT_ALIGNMENT_CENTER &&
                parent->GetParagraphAlignment()==DWRITE_PARAGRAPH_ALIGNMENT_CENTER && grid.mText->GetTextAlignment()==DWRITE_TEXT_ALIGNMENT_CENTER &&
                grid.mText->GetParagraphAlignment()==DWRITE_PARAGRAPH_ALIGNMENT_CENTER,
                "only parent captions stay centered; per-layout alignment never mutates the shared format");
            CFont font; const LOGFONT lf=q1view::WindowsUiLogFont(q1view::WindowsUiFontRole::Folder,dpi,scale);
            font.CreateFontIndirect(&lf); q1view::WindowsUiDcState fontState(dc.GetSafeHdc()); dc.SelectObject(&font);
            TEXTMETRIC tm{}; dc.GetTextMetrics(&tm);
            for(const CString& name : names) for(bool centered : {false,true}) {
                dc.FillSolidRect(0,0,512,512,RGB(255,255,255)); dc.SetBkMode(TRANSPARENT); dc.SetTextColor(RGB(0,0,0));
                const CRect bounds(12,14,232,14+tm.tmHeight*3+tm.tmHeight/2);
                const CString text=centered ? CString(L"[..]") : name, saved=text;
                grid.DrawFolderCaptionGdi(dc,text,bounds,centered); GdiFlush();
                int left=512,top=512,right=-1,bottom=-1;
                bool inside=true;
                for(int y=0;y<bounds.bottom+4;++y) for(int x=0;x<bounds.right+4;++x) if((pixels[y*512+x]&0xffffff)!=0xffffff) {
                    left=std::min(left,x);top=std::min(top,y);right=std::max(right,x);bottom=std::max(bottom,y);
                    inside=inside && bounds.PtInRect(CPoint(x,y));
                }
                Require(inside && right>=left && bottom>=top && text==saved,"GDI captions stay inside their text bounds and preserve literal Unicode names");
                if(centered) Require(std::abs((left+right)/2-bounds.CenterPoint().x)<fontSize/2 &&
                    std::abs((top+bottom)/2-bounds.CenterPoint().y)<fontSize/2,"GDI parent caption stays horizontally and vertically centered");
                else Require(left<bounds.left+fontSize/2 && top<bounds.top+tm.tmHeight/2 && bottom<bounds.top+tm.tmHeight*3,
                    "GDI ordinary captions start at top left and leave the partial final row unpainted");
            }
        }
    }
    void BrowserNavigationTests(CMainFrame* frame, const CString& fixture) {
        auto& pane = *frame->mpDrawer;
        auto& grid = *pane.mGrid;
        auto* doc = static_cast<CViewerDoc*>(frame->GetActiveDocument());
        auto* view = static_cast<CViewerView*>(frame->GetActiveView());
        const CString oldFolder = pane.mFolder;
        const auto reader = pane.mReadDirectory;
        const auto reporter = pane.mReportNavigation;
        const int oldStep = pane.mViewStep;
        wchar_t temp[MAX_PATH]; GetTempPathW(MAX_PATH, temp);
        CString root; root.Format(L"%sQ1View-navigation-%lu-%llu\\",temp,GetCurrentProcessId(),GetTickCount64());
        CString child = root + L"\xC5EC\xD589 \xC0AC\xC9C4 long folder name\\";
        Require(CreateDirectoryW(root,nullptr) && CreateDirectoryW(child,nullptr), "navigation fixtures use isolated Unicode directories");
        const CString image = root + L"sample.png";
        Require(CopyFileW(fixture,image,TRUE)!=FALSE,"navigation fixture image copied");
        pane.NavigateTo(root);
        Await([&] { return !pane.mNavigationBusy && pane.mFolder==root; },"real directory metadata commits asynchronously");
        const CString document = doc->mPathName;
        const auto openGeneration = doc->mOpenGeneration;
        const float zoom = view->mN;
        const int x = view->mXDst, y = view->mYDst;
        const auto pixels = view->mStableRgbBufferInfo.addr;
        for (int step : {0,1,5}) {
            pane.ApplyViewStep(step,false); Pump(.05);
            CString path; pane.mPathBar.GetWindowText(path);
            Require(path==root && pane.mPathBar.IsWindowVisible() && (pane.mPathBar.GetStyle()&ES_READONLY),
                "list and grid retain the exact read-only current Browse path");
            CRect header; pane.mPathHeader.GetWindowRect(header); pane.ScreenToClient(header);
            CRect pathBounds; pane.mPathBar.GetWindowRect(pathBounds); pane.ScreenToClient(pathBounds);
            const int separator=CBrowserPathHeader::SeparatorThickness(GetDpiForWindow(pane.mPathHeader.m_hWnd));
            Require(pathBounds.top>=header.top && pathBounds.bottom<header.bottom-separator,
                "read-only path leaves clear space above the header separator in list and grid");
            if(step) { CRect canvas; grid.GetWindowRect(canvas); pane.ScreenToClient(canvas);
                Require(canvas.top>=header.bottom && canvas.Height()>0,"grid is laid out below the native path header without overlap"); }
            else { CRect row; pane.GetItemRect(0,row,LVIR_BOUNDS);
                Require(row.top>=header.bottom,"compact rows start below the path header"); }
            pane.FocusPath();
            Require(::GetFocus()==pane.mPathBar.m_hWnd,"path is keyboard focusable in both presentations");
            pane.mPathBar.SendMessage(WM_CHAR,'z'); pane.mPathBar.GetWindowText(path);
            Require(path==root,"typing cannot edit the path or navigate");
            MSG key{};key.hwnd=pane.mPathBar.m_hWnd;key.message=WM_KEYDOWN;key.wParam=VK_ESCAPE;
            Require(pane.mPathBar.PreTranslateMessage(&key) && ::GetFocus()!=pane.mPathBar.m_hWnd,"Escape returns to the list/grid without closing the app");
        }
        IAccessible* accessible=nullptr;
        Require(SUCCEEDED(AccessibleObjectFromWindow(pane.mPathBar.m_hWnd,OBJID_CLIENT,IID_PPV_ARGS(&accessible))),"native path edit exposes MSAA");
        VARIANT self{};self.vt=VT_I4;self.lVal=CHILDID_SELF;
        BSTR name=nullptr,value=nullptr;
        accessible->get_accName(self,&name);accessible->get_accValue(self,&value);
        Require(name && !wcscmp(name,L"Current folder") && value && root==value,"accessible path has a meaningful name and full unelided value");
        SysFreeString(name);SysFreeString(value);accessible->Release();
        {
            CClientDC dc(&pane.mPathBar);CFont* old=dc.SelectObject(pane.mPathBar.GetFont());
            const CString longPath=L"D:\\very long ancestor directory\\another ancestor\\\xC5EC\xD589 \xC0AC\xC9C4\\";
            const CString expected=L"D:\\\x2026\\\xC5EC\xD589 \xC0AC\xC9C4";
            SIZE size{};GetTextExtentPoint32W(dc,expected,expected.GetLength(),&size);
            Require(CBrowserPathBar::Compact(dc,longPath,size.cx+1)==expected,"compact path preserves the drive and current Unicode folder, eliding ancestors rather than the tail");
            const CString unc=L"\\\\server\\share\\long ancestor\\another ancestor\\current";
            const CString compactUnc=L"\\\\server\\share\\\x2026\\current";
            GetTextExtentPoint32W(dc,compactUnc,compactUnc.GetLength(),&size);
            Require(CBrowserPathBar::Compact(dc,unc,size.cx+1)==compactUnc,"UNC compaction preserves share identity and the current leaf");
            for(int width : {10,40,80,120,240}) {
                const CString compact=CBrowserPathBar::Compact(dc,longPath,width);
                GetTextExtentPoint32W(dc,compact,compact.GetLength(),&size);
                Require(size.cx<=width,"even extremely narrow path text remains inside its measured bounds");
            }
            dc.SelectObject(old);
            for(UINT dpi : {96u,144u,192u}) {
                pane.RebuildFonts(dpi);
                CClientDC measured(&pane.mPathBar);CFont* previous=measured.SelectObject(pane.mPathBar.GetFont());
                TEXTMETRIC metrics{};measured.GetTextMetrics(&metrics);measured.SelectObject(previous);
                LOGFONT pathFont{}, rowFont{};
                pane.mPathBar.GetFont()->GetLogFont(&pathFont); pane.mLabelFont.GetLogFont(&rowFont);
                Require(pathFont.lfWeight==FW_MEDIUM && rowFont.lfWeight==FW_NORMAL &&
                    pathFont.lfHeight==rowFont.lfHeight && !lstrcmp(pathFont.lfFaceName,rowFont.lfFaceName) &&
                    pane.mPathBar.GetFont()->GetSafeHandle()==pane.mFolderFont.GetSafeHandle(),
                    "location header reuses medium Pretendard at the same caption size without changing regular item text");
                Require(CBrowserPathHeader::SeparatorThickness(dpi)==q1view::WindowsUiPixels(1,dpi),
                    "location boundary remains one DIP at 100/150/200-percent DPI");
                CRect bounds;pane.mPathBar.GetClientRect(bounds);
                Require(bounds.Height()>=metrics.tmHeight,"native path row expands to contain enlarged 100/150/200-percent caption metrics");
            }
            pane.RebuildFonts(GetDpiForWindow(pane.m_hWnd));
        }
        pane.SetFocus();
        const HWND browseFocus=::GetFocus();
        Require(PostMessageW(browseFocus,WM_KEYDOWN,VK_TAB,1) && PostMessageW(browseFocus,WM_KEYUP,VK_TAB,1|(1L<<30)|(1L<<31)),"physical Tab messages queued from the browser");
        Pump(.05);
        Require(pane.IsPathFocused(),"the real MFC message-pump Tab path reaches the path text before splitter traversal");
        {
            Microsoft::WRL::ComPtr<IDataObject> clipboard; OleGetClipboard(&clipboard);
            struct Restore { IDataObject* data; ~Restore(){OleSetClipboard(data);OleFlushClipboard();} } restore{clipboard.Get()};
            pane.FocusPath(); BYTE keyboard[256], saved[256]; GetKeyboardState(saved); memcpy(keyboard,saved,sizeof(keyboard)); keyboard[VK_CONTROL]=0x80;
            struct RestoreKeys { BYTE* keys; ~RestoreKeys(){SetKeyboardState(keys);} } keys{saved};
            SetKeyboardState(keyboard);
            MSG key{};key.hwnd=pane.mPathBar.m_hWnd;key.message=WM_KEYDOWN;key.wParam='A';
            Require(AfxGetApp()->PreTranslateMessage(&key),"Ctrl+A is handled by path text, not file selection");
            key.wParam='C';Require(AfxGetApp()->PreTranslateMessage(&key),"Ctrl+C uses the focused read-only path");
            Require(OpenClipboard(frame->m_hWnd)!=FALSE,"path copy clipboard opened for verification");
            const HANDLE data=GetClipboardData(CF_UNICODETEXT);const wchar_t* copied=data?static_cast<const wchar_t*>(GlobalLock(data)):nullptr;
            const bool exact=copied && root==copied;if(copied) GlobalUnlock(data);CloseClipboard();
            Require(exact,"path clipboard contains the exact full value, not the visually elided string");
        }
        pane.SetFocus();
        {
            ScopedWindowsUiPaintTestTheme theme;
            for(auto appearance : {q1view::WindowsUiAppearance::Light,q1view::WindowsUiAppearance::Dark}) {
                q1view::WindowsUiAppearanceState().SetAppearance(appearance);
                pane.SendMessage(q1view::WM_UI_APPEARANCE_CHANGED);
                for(int step : {0,1,5}) {
                    pane.ApplyViewStep(step,false); Pump(.05);
                    auto check=[&](bool okay,const char* text){Require(okay,text);};
                    CheckWindowsUiErase(pane.mPathBar.m_hWnd,q1view::WindowsUiColor::LocationHeader,true,check);
                    WindowsUiPaintTestSurface surface(pane.mPathBar.m_hWnd);
                    for(UINT message : {UINT(WM_CTLCOLORSTATIC),UINT(WM_CTLCOLOREDIT)}) {
                        const auto brush=HBRUSH(pane.mPathHeader.SendMessage(message,WPARAM(surface.Dc()),LPARAM(pane.mPathBar.m_hWnd)));
                        LOGBRUSH logical{};
                        Require(brush && GetObjectW(brush,sizeof(logical),&logical) &&
                            logical.lbColor==q1view::WindowsUiColorValue(q1view::WindowsUiColor::LocationHeader) && GetTextColor(surface.Dc())==Q1UI_COLOR_TEXT,
                            "focused and unfocused native path keep the neutral header surface and readable normal text color");
                    }
                    WindowsUiPaintTestSurface header(pane.mPathHeader.m_hWnd);
                    const int thickness=CBrowserPathHeader::SeparatorThickness(GetDpiForWindow(pane.mPathHeader.m_hWnd));
                    for(UINT message : {UINT(WM_ERASEBKGND),UINT(WM_PRINTCLIENT)}) {
                        header.Fill(RGB(17,34,51));
                        pane.mPathHeader.SendMessage(message,WPARAM(header.Dc()),PRF_CLIENT);
                        GdiFlush();
                        Require(GetPixel(header.Dc(),2,0)==q1view::WindowsUiColorValue(q1view::WindowsUiColor::LocationHeader) &&
                            GetPixel(header.Dc(),2,header.rect.bottom-thickness-1)==q1view::WindowsUiColorValue(q1view::WindowsUiColor::LocationHeader) &&
                            GetPixel(header.Dc(),2,header.rect.bottom-thickness)==Q1UI_COLOR_BORDER_SOFT &&
                            GetPixel(header.Dc(),header.rect.right-2,header.rect.bottom-1)==Q1UI_COLOR_BORDER_SOFT,
                            "header paint and erase draw exactly one full-width bottom separator, without per-row or focus outlines");
                    }
                    const DWORD gdi=GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS);
                    for(int i=0;i<100;++i) pane.mPathHeader.SendMessage(WM_PRINTCLIENT,WPARAM(header.Dc()),PRF_CLIENT);
                    Require(GetGuiResources(GetCurrentProcess(),GR_GDIOBJECTS)==gdi,
                        "repeated location header painting allocates no persistent GDI objects");
                }
            }
        }
        pane.SendMessage(q1view::WM_UI_APPEARANCE_CHANGED);
        pane.NavigateTo(child); Await([&] { return !pane.mNavigationBusy && pane.mFolder==child; },"accessible empty Unicode folder opens normally");
        Require(pane.mEntries.size()==1 && pane.mEntries.front().kind==CThumbnailPane::ENTRY_PARENT,"empty folder retains its parent rather than being treated as an error");
        pane.GoToParent();Await([&] { return !pane.mNavigationBusy && pane.mFolder==root; },"parent navigation commits with the same worker");
        Require(pane.mSelection.focus>=0 && pane.mEntries[pane.mSelection.focus].path==child,"going up reveals the child directory");
        const CString late=child+L"late.png";
        auto once=std::make_shared<std::atomic<bool>>(false);
        pane.mReadDirectory=[reader,fixture,late,once](const std::wstring& folder){auto snapshot=reader(folder);if(!once->exchange(true)) CopyFileW(fixture,late,TRUE);return snapshot;};
        pane.NavigateTo(child);
        Await([&]{return !pane.mNavigationBusy && pane.mFolder==child && pane.mEntries.size()==2;},"post-registration validation catches a file created after enumeration but before its watch exists");
        pane.mReadDirectory=reader;
        pane.GoToParent();Await([&]{return !pane.mNavigationBusy && pane.mFolder==root;},"parent restored after watch-registration race check");
        Require(DeleteFileW(late)!=FALSE,"late-arrival fixture cleaned up");
        int errors=0;CString lastError;
        pane.mReportNavigation=[&](const CString& message){++errors;lastError=message;};
        for(DWORD code : {DWORD(ERROR_PATH_NOT_FOUND),DWORD(ERROR_ACCESS_DENIED),DWORD(ERROR_NOT_READY),DWORD(ERROR_DEVICE_NOT_CONNECTED),DWORD(ERROR_READ_FAULT)}) {
            pane.mReadDirectory=[code](const std::wstring& folder){q1view::BrowserDirectory result;result.folder=folder;result.files.push_back(L"partial.png");result.error=code;return result;};
            const auto generation=pane.mGen.load();const auto size=pane.mEntries.size();const auto selected=pane.mSelection.focus;const float scroll=grid.mLayout.scroll;
            const int before=errors;
            pane.NavigateTo(root+L"unavailable\\");
            Await([&] { return !pane.mNavigationBusy; },"injected directory error is delivered through the production request path");
            CString path;pane.mPathBar.GetWindowText(path);
            Require(pane.mFolder==root && path==root && pane.mGen==generation && pane.mEntries.size()==size && pane.mSelection.focus==selected && grid.mLayout.scroll==scroll,
                "all directory failure types retain committed path, entries, selection, scroll and thumbnail generation");
            Require(errors==before+1 && lastError.Find(L"unavailable")>=0,"explicit failure supplies one actionable message including its target");
            pane.mDirectoryRefreshPending=true;pane.RefreshDirectoryContents();
            Await([&] { return !pane.mNavigationBusy; },"background refresh error completes");
            Require(errors==before+1 && pane.mGen==generation,"background failure neither opens a modal nor invalidates thumbnails");
        }
        // Hold an old request while accepting a newer one: no sleeps or device
        // disconnects are needed to reproduce a late network completion.
        auto entered=std::make_shared<std::atomic<bool>>(false);
        auto release=std::make_shared<std::atomic<bool>>(false);
        pane.mReadDirectory=[entered,release,reader](const std::wstring& folder){entered->store(true);while(!release->load()) Sleep(1);auto result=reader(folder);result.error=ERROR_ACCESS_DENIED;return result;};
        pane.NavigateTo(child);Await([&]{return entered->load();},"slow old navigation starts off the UI thread");
        const int previousErrors=errors;
        pane.mReadDirectory=reader;pane.NavigateTo(root);release->store(true);
        Await([&]{return !pane.mNavigationBusy;},"newest queued request wins after a slow old failure");
        Require(pane.mFolder==root && errors==previousErrors,"stale failures do not replace a newer folder or display an error");
        pane.mReadDirectory=reader;
        pane.NavigateTo(root.Left(3));Await([&]{return !pane.mNavigationBusy;},"drive root metadata loads");
        Require(pane.CanGoToParent() && pane.mEntries.front().kind==CThumbnailPane::ENTRY_PARENT,"drive root keeps a visible, applicable parent action");
        pane.GoToParent();Await([&]{return !pane.mNavigationBusy;},"root parent opens drive chooser");
        CString path;pane.mPathBar.GetWindowText(path);
        Require(pane.mHasDirectory && pane.mFolder.IsEmpty() && path==L"Drives","drive chooser is explicit rather than an uninitialized empty path");
        Require(!pane.mEntries.empty() && grid.Label(0)==L"["+pane.mEntries[0].path+L"]","drive labels retain their root backslash in bracket notation");
        Require(doc->mPathName==document && doc->mOpenGeneration==openGeneration && view->mStableRgbBufferInfo.addr==pixels && view->mN==zoom && view->mXDst==x && view->mYDst==y,
            "browsing success and failure never reopen or transform the displayed image");
        pane.mReportNavigation=reporter;pane.NavigateTo(oldFolder);Await([&]{return !pane.mNavigationBusy;},"original Browse location restored");
        pane.ApplyViewStep(oldStep,false);
        {
            CThumbnailPane closing;
            Require(closing.CreatePane(frame,0x7c11)!=FALSE,"isolated drawer created for blocked-worker shutdown");
            auto started=std::make_shared<std::atomic<bool>>(false), unblock=std::make_shared<std::atomic<bool>>(false), finished=std::make_shared<std::atomic<bool>>(false);
            closing.mReadDirectory=[started,unblock,finished](const std::wstring& folder){started->store(true);while(!unblock->load()) Sleep(1);q1view::BrowserDirectory result;result.folder=folder;finished->store(true);return result;};
            closing.NavigateTo(root);Await([&]{return started->load();},"isolated metadata worker is blocked without blocking the UI");
            const auto before=GetTickCount64();closing.DestroyWindow();
            Require(GetTickCount64()-before<1500,"closing a drawer does not join an indefinitely blocked metadata read");
            unblock->store(true);Await([&]{return finished->load();},"detached stopped worker releases its owned request safely");
        }
        Require(DeleteFileW(image) && RemoveDirectoryW(child) && RemoveDirectoryW(root),"temporary navigation fixtures removed");
    }
    void RecycleTests(CMainFrame* frame, const CString& fixture) {
        auto& pane = *frame->mpDrawer;
        auto& grid = *pane.mGrid;
        auto* doc = static_cast<CViewerDoc*>(frame->GetActiveDocument());
        auto* view = static_cast<CViewerView*>(frame->GetActiveView());
        const auto confirm = pane.mConfirmRecycle;
        const auto reportFailure = pane.mReportRecycle;
        wchar_t temp[MAX_PATH]; GetTempPathW(MAX_PATH, temp);
        CString root; root.Format(L"%sQ1View-recycle-%lu-%llu\\", temp, GetCurrentProcessId(), GetTickCount64());
        Require(CreateDirectoryW(root, nullptr) != FALSE, "isolated recycle fixture created");
        for (int mode : {0, 1}) {
            CString folder; folder.Format(L"%s%d\\", root.GetString(), mode);
            Require(CreateDirectoryW(folder, nullptr) != FALSE, "selection mode fixture created");
            const CString child = folder + L"folder\\";
            Require(CreateDirectoryW(child, nullptr) != FALSE, "directory exclusion fixture created");
            std::vector<CString> files;
            for (const wchar_t* name : {L"a.png", L"b.png", L"c.png", L"\xC0AC\xC9C4.png"}) {
                files.push_back(folder + name);
                Require(CopyFileW(fixture, files.back(), TRUE) != FALSE, "recycle photo fixture created");
            }
            const CString text = folder + L"document.txt";
            Require(CopyFileW(fixture, text, TRUE) != FALSE, "non-media exclusion fixture created");
            pane.NavigateTo(folder); pane.ApplyViewStep(mode, false); Pump(.1);
            auto indexOf = [&](const CString& path) {
                for (int i = 0; i < int(pane.mEntries.size()); ++i) if (pane.mEntries[i].path == path) return i;
                return -1;
            };
            auto click = [&](int index, UINT flags, bool right = false) {
                CPoint point;
                CWnd* target = &pane;
                if (mode) {
                    const auto rect = grid.mLayout.Rect(index, grid.Now());
                    point = CPoint(int(rect.x + rect.size / 2), int(rect.y + rect.size / 2));
                    target = &grid;
                } else {
                    pane.EnsureVisible(index, FALSE);
                    CRect rect; pane.GetItemRect(index, rect, LVIR_BOUNDS); point = rect.CenterPoint();
                }
                target->SendMessage(right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, flags, MAKELPARAM(point.x, point.y));
            };
            int first = indexOf(files[0]), last = indexOf(files[3]);
            click(first, 0);
            CMenu singleMenu; pane.BuildContextMenu(singleMenu, first);
            CString singleRecycleLabel; singleMenu.GetMenuString(CThumbnailPane::CMD_RECYCLE, singleRecycleLabel, MF_BYCOMMAND);
            Require(singleMenu.GetMenuState(CThumbnailPane::CMD_COMPARE, MF_BYCOMMAND) == UINT(-1) &&
                singleRecycleLabel == L"Move to Recycle Bin", "single-file menu uses singular Recycle Bin wording without Compare");
            {
                q1view::WindowsUiMenus contextMenus; pane.mContextMenus=&contextMenus;
                pane.SendMessage(WM_INITMENUPOPUP,reinterpret_cast<WPARAM>(singleMenu.GetSafeHmenu()));
                MENUITEMINFOW info={sizeof(info)}; info.fMask=MIIM_FTYPE|MIIM_DATA;
                Require(GetMenuItemInfoW(singleMenu.GetSafeHmenu(),0,TRUE,&info) && (info.fType&MFT_OWNERDRAW),
                    "thumbnail context popup uses the same shared painting as Comparator menus");
                MEASUREITEMSTRUCT measure={}; measure.CtlType=ODT_MENU; measure.itemData=info.dwItemData;
                Require(pane.SendMessage(WM_MEASUREITEM,0,reinterpret_cast<LPARAM>(&measure)) &&
                    measure.itemHeight>=UINT(q1view::WindowsUiPixels(32,q1view::WindowsUiDpi(pane.m_hWnd),q1view::WindowsUiSettings().Scale())),
                    "thumbnail context popup routes measurement through shared DPI-aware body-font sizing");
                HMENU names=GetSubMenu(singleMenu.GetSafeHmenu(),4);
                pane.SendMessage(WM_INITMENUPOPUP,reinterpret_cast<WPARAM>(names));
                Require(names && GetMenuItemInfoW(names,0,TRUE,&info) && (info.fType&MFT_OWNERDRAW),
                    "nested Copy path/name popup receives the same styling as its parent");
                pane.mContextMenus=nullptr;
            }
            click(last, MK_CONTROL);
            Require(pane.SelectedMediaPaths().size() == 2, "MFC Ctrl-click selects disjoint media in both views");
            CMenu compareTwo; pane.BuildContextMenu(compareTwo, first);
            Require(compareTwo.GetMenuState(CThumbnailPane::CMD_COMPARE, MF_BYCOMMAND) != UINT(-1) &&
                compareTwo.GetMenuState(CThumbnailPane::CMD_OPEN, MF_BYCOMMAND) == UINT(-1) &&
                compareTwo.GetMenuState(CThumbnailPane::CMD_PROPERTIES, MF_BYCOMMAND) == UINT(-1),
                "two-file menu offers Compare and hides single-file commands");
            click(last, MK_CONTROL);
            Require(pane.SelectedMediaPaths().size() == 1, "MFC Ctrl-click toggles selected media off");
            click(first, 0); click(indexOf(files[2]), MK_SHIFT);
            Require(pane.SelectedMediaPaths().size() == 3, "MFC Shift-click selects a contiguous media range");
            click(last, MK_SHIFT | MK_CONTROL);
            Require(pane.SelectedMediaPaths().size() == 4, "MFC Ctrl-Shift extends the range");
            CMenu compareFour; pane.BuildContextMenu(compareFour, first);
            Require(compareFour.GetMenuState(CThumbnailPane::CMD_COMPARE, MF_BYCOMMAND) != UINT(-1),
                "four-file menu keeps Compare available at Comparator's limit");
            click(first, 0); click(last, MK_CONTROL); click(first, 0, true);
            Require(pane.SelectedMediaPaths().size() == 2, "MFC right-click preserves multi-selection");
            CMenu menu; pane.BuildContextMenu(menu, first);
            CString label; menu.GetMenuString(CThumbnailPane::CMD_RECYCLE, label, MF_BYCOMMAND);
            Require(label == L"Move 2 items to Recycle Bin", "MFC recycle menu displays selected count");

            pane.SelectByPath(files[0]); pane.SelectIndex(indexOf(files[1]), true);
            Require(doc->mPathName.CompareNoCase(files[0]) != 0,
                "drawer watches its browsed folder independently of the active file");
            const CString watchedAddition = folder + L"watch-added.png";
            Require(CopyFileW(fixture, watchedAddition, TRUE) != FALSE,
                "watched-directory addition fixture created");
            Await([&] { return indexOf(watchedAddition) >= 0; },
                "filesystem add notification refreshes the visible drawer automatically");
            Require(pane.SelectedMediaPaths() == std::vector<CString>{files[0], files[1]},
                "automatic directory refresh preserves surviving multi-selection");
            const CString watchedRename = folder + L"watch-renamed.png";
            Require(MoveFileW(watchedAddition, watchedRename) != FALSE,
                "watched-directory rename fixture moved");
            Await([&] { return indexOf(watchedAddition) < 0 && indexOf(watchedRename) >= 0; },
                "filesystem rename notifications refresh the visible drawer automatically");
            Require(DeleteFileW(watchedRename), "watched-directory rename fixture removed");
            Await([&] { return indexOf(watchedRename) < 0; },
                "filesystem remove notification refreshes the visible drawer automatically");
            const CString watchedFolder = folder + L"watch-folder\\";
            Require(CreateDirectoryW(watchedFolder, nullptr) != FALSE,
                "watched-directory subfolder fixture created");
            Await([&] { return indexOf(watchedFolder) >= 0; },
                "filesystem subfolder notification refreshes the visible drawer automatically");
            Require(RemoveDirectoryW(watchedFolder) != FALSE,
                "watched-directory subfolder fixture removed");
            Await([&] { return indexOf(watchedFolder) < 0; },
                "filesystem subfolder removal refreshes the visible drawer automatically");
            pane.mDirectoryRefreshPending = true;
            pane.mRecycleBusy = true;
            const unsigned beforeRecycleRefresh = pane.mGen.load();
            pane.RefreshDirectoryContents();
            Require(pane.mGen.load() == beforeRecycleRefresh && pane.mDirectoryRefreshPending,
                "directory refresh waits until an in-progress recycle operation finishes");
            pane.mRecycleBusy = false;
            pane.RefreshDirectoryContents();
            Await([&] { return !pane.mNavigationBusy; }, "deferred directory enumeration completes before selection checks");
            Require(!pane.mDirectoryRefreshPending && pane.SelectedMediaPaths() ==
                std::vector<CString>{files[0], files[1]},
                "deferred directory refresh retains selection after recycling finishes");
            Require(AfxGetApp()->OpenDocumentFile(files[0]) != nullptr,
                "active recycle fixture opened after independent directory-watch checks");
            pane.SetCurrentFile(files[0]);

            const CString extra = folder + L"z-extra.png";
            Require(CopyFileW(fixture, extra, TRUE) != FALSE, "fifth media fixture created for menu selection boundary");
            pane.Populate(folder, files[0]); Pump(.1);
            first = indexOf(files[0]);
            pane.SelectIndex(first, false, false, false);
            for (const CString& path : {files[1], files[2], files[3], extra})
                pane.SelectIndex(indexOf(path), true, false, false);
            Require(pane.SelectedMediaPaths().size() == 5, "five media files can be selected for menu boundary coverage");
            CMenu compareFive; pane.BuildContextMenu(compareFive, first);
            CString fiveRecycleLabel; compareFive.GetMenuString(CThumbnailPane::CMD_RECYCLE, fiveRecycleLabel, MF_BYCOMMAND);
            Require(compareFive.GetMenuState(CThumbnailPane::CMD_COMPARE, MF_BYCOMMAND) == UINT(-1) &&
                compareFive.GetMenuState(CThumbnailPane::CMD_OPEN, MF_BYCOMMAND) == UINT(-1) &&
                fiveRecycleLabel == L"Move 5 items to Recycle Bin",
                "more-than-four selection hides Compare and single-file actions but keeps batch recycling");
            Require(DeleteFileW(extra), "fifth media menu fixture removed before recycle behavior tests");
            pane.Populate(folder, files[0]); Pump(.1);
            first = indexOf(files[0]); last = indexOf(files[3]);
            pane.SelectIndex(first, false, false, false);
            pane.SelectIndex(last, true, false, false);
            int confirmations = 0;
            pane.mConfirmRecycle = [&](size_t count) { ++confirmations; Require(count == 2, "MFC confirmation captures batch count"); return false; };
            MSG key = {}; key.message = WM_KEYDOWN; key.wParam = VK_DELETE;
            key.hwnd = mode ? grid.m_hWnd : pane.m_hWnd;
            Require(mode ? grid.PreTranslateMessage(&key) : pane.PreTranslateMessage(&key), "Delete reaches drawer recycle handler");
            Require(confirmations == 1 && GetFileAttributes(files[0]) != INVALID_FILE_ATTRIBUTES && pane.SelectedMediaPaths().size() == 2,
                "MFC cancellation preserves files and selection");
            pane.HandleSelectionKey(VK_DELETE, false, true);
            Require(confirmations == 1, "MFC Shift-Delete does not bypass recycling");
            pane.HandleSelectionKey('A', true, false);
            Require(pane.SelectedMediaPaths().size() == 4 && pane.mSelection.selected.size() == 4,
                "MFC Ctrl-A excludes parent, directory, and non-media files");
            if (!mode) {
                pane.SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
                pane.SetItemState(first, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                Require(pane.SelectedMediaPaths() == std::vector<CString>{files[0]},
                    "native list selection changes keep the shared selection model in sync");
            }
            pane.mConfirmRecycle = [&](size_t) { pane.NavigateTo(child); return true; };
            pane.RecycleSelected();
            Await([&] { return !pane.mNavigationBusy && pane.mFolder == child; }, "confirmation navigation completes without recycling old items");
            Require(pane.mFolder == child && GetFileAttributes(files[0]) != INVALID_FILE_ATTRIBUTES,
                "MFC folder change during confirmation invalidates batch");
            pane.NavigateTo(folder); Await([&] { return !pane.mNavigationBusy && pane.mFolder == folder; }, "recycle fixture folder reopens before choosing files");
            pane.SelectByPath(files[0]); pane.SelectIndex(indexOf(files[3]), true);
            const unsigned oldGeneration = pane.mGen.load();
            pane.ActivateIndex(indexOf(files[0]), true);
            pane.mConfirmRecycle = [](size_t) { return true; };
            CString report;
            pane.mReportRecycle = [&](const CString& message) { report = message; };
            HANDLE locked = CreateFileW(files[0], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, 0, nullptr);
            Require(locked != INVALID_HANDLE_VALUE, "locked file fixture opened");
            pane.RecycleSelected(); CloseHandle(locked); Pump(.1);
            Require(GetFileAttributes(files[0]) != INVALID_FILE_ATTRIBUTES && GetFileAttributes(files[3]) == INVALID_FILE_ATTRIBUTES &&
                pane.SelectedMediaPaths() == std::vector<CString>{files[0]} && report.Find(L"1 of 2") >= 0,
                "MFC partial failure preserves locked item and successful Unicode recycle");
            Require(GetFileAttributes(child) != INVALID_FILE_ATTRIBUTES && GetFileAttributes(text) != INVALID_FILE_ATTRIBUTES,
                "directory and non-media files never enter deletion payload");
            Require(pane.mPendingGeneration == 0, "deletion invalidates deferred activation");
            auto* stale = new CThumbnailPane::Result{oldGeneration, 0, pane.mThumb, nullptr};
            { std::lock_guard<std::mutex> lock(pane.mMutex); ++pane.mOutstanding; }
            const CString rowZero = pane.mEntries[0].path;
            pane.OnThumbReady(reinterpret_cast<WPARAM>(stale), 0);
            Require(pane.mEntries[0].path == rowZero, "stale decode result cannot mutate recycled indices");
            const auto rejectedDirectory = q1view::RecycleFiles(pane.m_hWnd, {std::wstring(child.GetString())});
            Require(!rejectedDirectory[0].recycled && GetFileAttributes(child) != INVALID_FILE_ATTRIBUTES,
                "native backend also rejects directory payloads");
            Microsoft::WRL::ComPtr<IShellItem> shellItem;
            Require(SUCCEEDED(SHCreateItemFromParsingName(files[0], nullptr, IID_PPV_ARGS(&shellItem))), "recycle guard fixture binds");
            q1view::RecycleResult guarded;
            auto sink = Microsoft::WRL::Make<q1view::RecycleProgress>(&guarded);
            Require(FAILED(sink->PreDeleteItem(0, shellItem.Get())) && GetFileAttributes(files[0]) != INVALID_FILE_ATTRIBUTES,
                "permanent-delete fallback is rejected before touching the file");

            Require(AfxGetApp()->OpenDocumentFile(files[1]) != nullptr, "active recycle photo opened"); Pump(.05);
            pane.SetCurrentFile(files[1]);
#ifdef _WIN32
            HANDLE activeLock = CreateFileW(files[1], GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                nullptr, OPEN_EXISTING, 0, nullptr);
            Require(activeLock != INVALID_HANDLE_VALUE, "active recycle failure fixture opened");
            pane.RecycleSelected(); CloseHandle(activeLock); Pump(.05);
            Require(doc->mPathName == files[1] && doc->mFrmSrc &&
                pane.SelectedMediaPaths() == std::vector<CString>{files[1]},
                "failed active recycle reopens the image and preserves its selection");
#endif
            CRect geometry; frame->GetWindowRect(&geometry);
            const unsigned oldWatch = doc->mFileChangeNotiThread->Generation();
            pane.mReportRecycle = [&](const CString&) { Require(false, "active file recycling should succeed"); };
            pane.RecycleSelected(); Pump(.1);
            CRect after; frame->GetWindowRect(&after);
            Require(doc->mPathName == files[2] && GetFileAttributes(files[1]) == INVALID_FILE_ATTRIBUTES && geometry == after,
                "active photo deletion selects next survivor without resizing");
            const unsigned opened = doc->mOpenGeneration;
            frame->SendMessage(WM_RELOAD, oldWatch, 1);
            Require(doc->mOpenGeneration == opened, "stale watcher notification cannot reload the next document");
            pane.RecycleSelected(); Pump(.05);
            Require(doc->mPathName == files[0], "last active photo deletion selects previous survivor");
            pane.RecycleSelected(); Pump(.05);
            Require(doc->mPathName.IsEmpty() && doc->mFrmSrc == nullptr && !view->mStableRgbBufferInfo.addr,
                "deleting final photo leaves a safe empty view");
        }
        const CString video = root + L"a-playing.avi";
        cv::VideoWriter writer(std::string(CW2A(video, CP_UTF8)), cv::VideoWriter::fourcc('M','J','P','G'), 30, cv::Size(64,48));
        Require(writer.isOpened(), "dedicated recycle video fixture created");
        for (int i = 0; i < 300; ++i) writer.write(cv::Mat(48,64,CV_8UC3,cv::Scalar(i%255,80,30)));
        writer.release();
        const CString next = root + L"b-next.png";
        Require(CopyFileW(fixture, next, TRUE) != FALSE, "post-video survivor created");
        Require(AfxGetApp()->OpenDocumentFile(video) != nullptr, "recycle video opens in actual Viewer");
        Pump(.1);
        if (!view->mIsPlaying) view->SetPlayTimer(doc);
        Await([&] { return view->mIsPlaying && view->mStableRgbBufferInfo.ID > 0; }, "recycle video is actively playing");
        pane.ApplyViewStep(0, false); pane.SetCurrentFile(video);
        pane.RecycleSelected(); Pump(.1);
        Require(GetFileAttributes(video) == INVALID_FILE_ATTRIBUTES && doc->mPathName == next && !view->mIsPlaying,
            "playing video releases decoder/audio handles before recycling and activates survivor");
        pane.mConfirmRecycle = confirm;
        pane.mReportRecycle = reportFailure;
    }
    void DefaultWindowTests(CMainFrame* frame, CViewerView* view, CViewerDoc* doc) {
        Require(doc->mPathName.IsEmpty() && doc->mW == 800 && doc->mH == 600,
            "empty Windows launch uses 800x600 image and RAW defaults, independent of UI DPI");
        CString resolutionLabel;
        frame->GetMenu()->GetMenuString(1, resolutionLabel, MF_BYPOSITION);
        Require(resolutionLabel == L"800&x600", "startup resolution menu matches the RAW default");
        const int preset = q1::image_resolution_idx(800, 600);
        Require(preset >= 0 && (frame->mResolutionMenu.GetMenuState(ID_RESOLUTION_START + preset, MF_BYCOMMAND) & MF_CHECKED),
            "800x600 preset is checked on startup");
        Require(!frame->mDrawerVisible, "default launch keeps the drawer closed");
        const UINT dpi = q1view::WindowsUiDpi(frame->m_hWnd);
        CRect initial, content; frame->GetWindowRect(initial); frame->GetContentRect(content);
        MONITORINFO monitor = {sizeof(monitor)};
        Require(GetMonitorInfo(MonitorFromWindow(frame->m_hWnd, MONITOR_DEFAULTTONEAREST), &monitor) != FALSE,
            "startup monitor work area available");
        Require(initial.left >= monitor.rcWork.left && initial.top >= monitor.rcWork.top &&
            initial.right <= monitor.rcWork.right && initial.bottom <= monitor.rcWork.bottom,
            "default outer window fits the monitor work area");
        const int menuWidth = frame->mUiFrame.SingleRowMenuWidth(dpi, 1.0);
        CRect client; frame->GetClientRect(client);
        const SIZE available = {
            monitor.rcWork.right - monitor.rcWork.left - (initial.Width() - client.Width()),
            monitor.rcWork.bottom - monitor.rcWork.top - (initial.Height() - client.Height()) - frame->mUiFrame.Height() - frame->StatusHeight()
        };
        const auto expected = q1view::ViewerDefaultContentSize(dpi, menuWidth, available);
        Require(content.Width() == expected.cx && content.Height() == expected.cy,
            "real default viewing area matches measured DPI-scaled startup policy");
        fprintf(report, "Startup DPI %u, text %.3f, content %dx%d, measured normal menu %d px\n",
            dpi, q1view::WindowsUiSettings().Scale(), content.Width(), content.Height(), menuWidth);
        if (q1view::WindowsUiSettings().Scale() == 1.0 && content.Width() >= menuWidth)
            for (const auto& rect : frame->mUiFrame.MenuRects())
                Require(rect.top == 0, "normal default launch menu stays on one row");
        frame->OnToggleDrawer(); Pump(.25);
        CRect withDrawer; frame->GetWindowRect(withDrawer);
        Require(frame->mDrawerVisible && withDrawer == initial, "opening the startup drawer consumes existing window space");
        frame->OnToggleDrawer(); Pump(.25);
        CRect withoutDrawer; frame->GetWindowRect(withoutDrawer);
        Require(!frame->mDrawerVisible && withoutDrawer == initial, "closing the startup drawer preserves default geometry");
        frame->MoveWindow(initial.left, initial.top, q1view::WindowsUiPixels(320, dpi), initial.Height()); Pump(.1);
        bool wrapped = false;
        for (const auto& rect : frame->mUiFrame.MenuRects()) wrapped |= rect.top > 0;
        Require(wrapped && frame->mUiFrame.OverflowPositions().empty(), "manual narrowing wraps the original menus without hiding commands in More");
        frame->SizeDefaultWindow(); Pump(.1);
        CRect resizedContent; frame->GetContentRect(resizedContent);
        Require(resizedContent.Width() == expected.cx && resizedContent.Height() == expected.cy,
            "startup sizing recomputes chrome after a wrapped menu becomes wider");
        frame->MoveWindow(initial); Pump(.1);
        view->ToggleFullScreen(); Pump(.1); view->ToggleFullScreen(); Pump(.1);
        CRect restored; frame->GetWindowRect(restored);
        Require(restored == initial && doc->mW == 800 && doc->mH == 600 && doc->mPathName.IsEmpty(),
            "full-screen exit restores default geometry without changing empty image state");
        RawDefaultTests(frame, view, doc);
        frame->MoveWindow(initial); Pump(.1);
    }
    void RawDefaultTests(CMainFrame* frame, CViewerView* view, CViewerDoc* doc) {
        wchar_t temp[MAX_PATH]; GetTempPathW(MAX_PATH, temp);
        CString root; root.Format(L"%sQ1View-raw-default-%lu-%llu\\", temp, GetCurrentProcessId(), GetTickCount64());
        Require(CreateDirectoryW(root, nullptr) != FALSE, "isolated RAW-default fixture directory created");
        auto writeRaw = [&](const CString& path, int width, int height) {
            // One complete YUV420 frame; no dimensions in the default filename.
            const std::vector<BYTE> pixels(size_t(width) * height * 3 / 2, 128);
            CFile file;
            Require(file.Open(path, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary) != FALSE,
                "RAW-default fixture opened for writing");
            file.Write(pixels.data(), UINT(pixels.size())); file.Close();
        };
        const CString unnamed = root + L"default.yuv";
        const CString named = root + L"known_16x16_yuv420.yuv";
        writeRaw(unnamed, 800, 600); writeRaw(named, 16, 16);
        Require(AfxGetApp()->OpenDocumentFile(unnamed) != nullptr, "RAW without filename dimensions opens");
        frame->RefreshCommandStates();
        Require(frame->IsCommandEnabled(ID_RESOLUTION_START) && frame->IsCommandEnabled(ID_CS_START),
            "actual RAW source retains enabled resolution and color-space interpretation commands");
        Await([&] { return view->mStableRgbBufferInfo.addr != nullptr; }, "default RAW frame is decoded and presented");
        Require(doc->mFrmSrc && !doc->mFrmSrc->isFixed() && doc->mW == 800 && doc->mH == 600 &&
            doc->mOrigSceneSize == 720000 && doc->mFrames == 1,
            "unnamed RAW uses 800x600 pixels and correct YUV420 frame bytes/count");
        Require(AfxGetApp()->OpenDocumentFile(L"Tests\\fixtures\\sample_16x16.png") != nullptr,
            "decoded photo opens before testing the return to editable RAW menus");
        frame->RefreshCommandStates(true);
        Require(AfxGetApp()->OpenDocumentFile(named) != nullptr,"explicit RAW opens after a decoded photo");
        frame->RefreshCommandStates(); Pump(.05);
        for(UINT position : {1u,2u}) {
            Require(!(frame->GetMenu()->GetMenuState(position,MF_BYPOSITION)&(MF_DISABLED|MF_GRAYED)) &&
                IsWindowEnabled(::GetDlgItem(frame->mUiFrame.MenuHost(),position+1)),
                "returning from a decoded photo enables the actual RAW root menu button");
            auto* popup=frame->GetMenu()->GetSubMenu(position);
            frame->SendMessage(WM_INITMENUPOPUP,reinterpret_cast<WPARAM>(popup->GetSafeHmenu()),position);
            Require(!(popup->GetMenuState(0,MF_BYPOSITION)&(MF_DISABLED|MF_GRAYED)),
                "opening a RAW popup retains enabled interpretation commands after native MFC updates");
        }
        auto requireHostedLabel=[&](UINT position,const CString& expected) {
            wchar_t label[256]={}; GetWindowTextW(::GetDlgItem(frame->mUiFrame.MenuHost(),position+1),label,_countof(label));
            Require(expected==label,"RAW menu command updates its actual hosted label as well as document state");
        };
        for(int i=0;i<ARRAY_SIZE(qcsc_info_table);++i) if(qcsc_info_table[i].cs==QIMAGE_CS_NV12) {
            frame->SendMessage(WM_COMMAND,ID_CS_START+i); Pump(.05);
            Require(doc->mColorSpace==QIMAGE_CS_NV12,"RAW color-space menu command actually changes interpretation");
            requireHostedLabel(2,L"NV12");
        }
        for(int i=0;i<ARRAY_SIZE(qcsc_info_table);++i) if(qcsc_info_table[i].cs==QIMAGE_CS_YUV420) {
            frame->SendMessage(WM_COMMAND,ID_CS_START+i); Pump(.05);
            Require(doc->mColorSpace==QIMAGE_CS_YUV420,"RAW color-space menu can restore YUV420");
        }
        Require(AfxGetApp()->OpenDocumentFile(named) != nullptr && doc->mW == 16 && doc->mH == 16 && doc->mFrames == 1,
            "explicit RAW filename dimensions override the new default");
        frame->ApplyResolution(800,600);
        Require(AfxGetApp()->OpenDocumentFile(unnamed)!=nullptr,"full-sized RAW fixture reopens before resolution command");
        for(int i=0;i<ARRAY_SIZE(q1::resolution_info_table)-1;++i) {
            int width=0,height=0;
            if(!q1::image_parse_w_h(q1::resolution_info_table[i],&width,&height) && width==320 && height==240)
                frame->SendMessage(WM_COMMAND,ID_RESOLUTION_START+i);
        }
        Require(doc->mW==320 && doc->mH==240,"RAW resolution menu command actually changes interpreted dimensions");
        Await([&]{return view->mStableRgbBufferInfo.addr && doc->mDocState!=DOC_JUSTLOAD;},"reinterpreted RAW frame is presented");
        Require(view->mW==320 && view->mH==240 && doc->mFrames==6,
            "resolution command updates the displayed RAW dimensions and recalculates its frame count");
        requireHostedLabel(1,L"320&x240");
        Require(AfxGetApp()->OpenDocumentFile(unnamed) != nullptr && doc->mW == 320 && doc->mH == 240,
            "unnamed RAW continues to honor a manually selected resolution");
        frame->ApplyResolution(800, 600);
        doc->CloseMediaForRecycle();
        Require(DeleteFileW(unnamed) && DeleteFileW(named) && RemoveDirectoryW(root), "RAW-default fixtures cleaned up after releasing media");
    }
    void ChromeTests(CMainFrame* frame, CViewerView* view) {
        Require(frame->mUiFrame.Initialized() && frame->mUiFrame.Custom(), "real MFC Viewer uses the custom client-painted title");
        CRect content, splitter;
        frame->GetContentRect(content); frame->mwndSplitter.GetWindowRect(splitter); frame->ScreenToClient(splitter);
        Require(content == splitter && content.top >= 62, "MFC reserves chrome once without overlapping or double-resizing the image splitter");
        const HMENU menu = frame->GetMenu()->GetSafeHmenu();
        const int menuCount = GetMenuItemCount(menu);
        const CString path = static_cast<CViewerDoc*>(frame->GetActiveDocument())->GetPathName();
        CRect before; frame->GetWindowRect(before);
        frame->SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE); Pump(.4);
        MONITORINFO monitor = {sizeof(monitor)};
        GetMonitorInfo(MonitorFromWindow(frame->m_hWnd, MONITOR_DEFAULTTONEAREST), &monitor);
        CRect client; frame->GetClientRect(client); frame->ClientToScreen(client);
        Require(frame->IsZoomed() && client.left >= monitor.rcWork.left && client.top >= monitor.rcWork.top &&
            client.right <= monitor.rcWork.right && client.bottom <= monitor.rcWork.bottom, "maximized client stays within the monitor work area and taskbar");
        frame->SendMessage(WM_SYSCOMMAND, SC_RESTORE); Pump(.3);
        CRect restored; frame->GetWindowRect(restored);
        Require(!frame->IsZoomed() && restored == before, "native restore retains the original window rectangle");
        for (int i = 0; i < 3; ++i) {
            view->ToggleFullScreen(); Pump(.1);
            Require(view->IsFullScreen() && frame->mUiFrame.Height() == 0 && frame->GetMenu() == nullptr,
                "full screen hides both custom title and menu without leftover reservation");
            if (i == 0) {
                frame->mHelpOverlay.Toggle();
                frame->mCommandStateSignature = ~0u;
                frame->RefreshCommandStates();
                const auto context = frame->CommandContext();
                const unsigned signature = unsigned(context.rawInput) | (unsigned(context.pixels) << 1) |
                    (unsigned(context.clipboardImage) << 2) | (unsigned(context.file) << 3);
                Require(frame->mHelpOverlay.IsShown() && frame->mCommandStateSignature == signature,
                    "visible help refreshes command availability even when full screen has no menu");
                frame->mHelpOverlay.Hide();
            }
            view->ToggleFullScreen(); Pump(.1);
            Require(!view->IsFullScreen() && frame->GetMenu()->GetSafeHmenu() == menu && GetMenuItemCount(menu) == menuCount,
                "full-screen exit preserves the same menu and does not duplicate dynamic entries");
        }
        Require(static_cast<CViewerDoc*>(frame->GetActiveDocument())->GetPathName() == path, "window chrome operations never change the active file");
        frame->GetContentRect(content); frame->mwndSplitter.GetWindowRect(splitter); frame->ScreenToClient(splitter);
        Require(content == splitter, "splitter returns to the chrome-reserved client rectangle after full screen");
    }

    void PaintBackgroundTests(CViewerView* view, CViewerDoc* doc) {
        ScopedWindowsUiPaintTestTheme restoreTheme;
        struct RestoreState {
            CViewerDoc* doc; DocState saved;
            ~RestoreState() { doc->mDocState=saved; }
        } restoreState{doc,doc->mDocState};
        auto check=[&](bool okay,const char* text) { Require(okay,text); };
        for (auto theme : {q1view::WindowsUiAppearance::Dark,q1view::WindowsUiAppearance::Light}) {
            q1view::WindowsUiAppearanceState().SetAppearance(theme);
            doc->mDocState=DOC_JUSTLOAD;
            CheckWindowsUiErase(view->m_hWnd,q1view::WindowsUiColor::Canvas,true,check);
            doc->mDocState=DOC_ADJUSTED;
            CheckWindowsUiErase(view->m_hWnd,q1view::WindowsUiColor::Canvas,false,check);
        }
    }
    void AutoplayNavigationTests(CMainFrame* frame) {
        auto* view = static_cast<CViewerView*>(frame->GetActiveView());
        auto* doc = view->GetDocument();
        const CString previous = doc->mPathName;
        CRect originalWindow; frame->GetWindowRect(originalWindow);
        struct RestoreAutoplay {
            bool enabled = theApp.IsVideoAutoplayEnabled();
            ~RestoreAutoplay() { theApp.SetVideoAutoplayEnabled(enabled); }
        } restoreAutoplay;
        wchar_t temp[MAX_PATH]; GetTempPathW(MAX_PATH, temp);
        CString root; root.Format(L"%sQ1View-autoplay-nav-%lu-%llu\\", temp, GetCurrentProcessId(), GetTickCount64());
        Require(CreateDirectoryW(root, nullptr) != FALSE, "isolated autoplay navigation folder created");
        const CString photo = root + L"a.png", first = root + L"b.avi", second = root + L"c.avi";
        Require(q1::imwriteW(photo.GetString(), cv::Mat(64,96,CV_8UC3,cv::Scalar(40,80,120))), "autoplay navigation photo generated");
        {
            cv::VideoWriter writer(std::string(CW2A(first,CP_UTF8)), cv::VideoWriter::fourcc('M','J','P','G'),30.0,cv::Size(96,64));
            Require(writer.isOpened(), "autoplay navigation video writer opened");
            for (int i=0;i<150;++i) writer.write(cv::Mat(64,96,CV_8UC3,cv::Scalar(i%256,(i*3)%256,(i*7)%256)));
        }
        Require(CopyFileW(first, second, TRUE) != FALSE, "second autoplay navigation video generated");
        const CString raw=root+L"d_96x64_yuv420_30fps.yuv", single=root+L"e_96x64_yuv420.yuv",
            lastRaw=root+L"f_96x64_yuv420_30fps.yuv";
        auto writeRaw=[&](const CString& path,int frames) {
            CFile file; Require(file.Open(path,CFile::modeCreate|CFile::modeWrite|CFile::typeBinary)!=FALSE,
                "RAW autoplay navigation fixture opened for writing");
            std::vector<BYTE> pixels(96*64*3/2,128);
            for(int i=0;i<frames;++i) { pixels[0]=BYTE(i); file.Write(pixels.data(),UINT(pixels.size())); }
        };
        writeRaw(raw,150); writeRaw(single,1); writeRaw(lastRaw,150);
        theApp.SetVideoAutoplayEnabled(true);
        Require(AfxGetApp()->OpenDocumentFile(photo) != nullptr, "navigation starts on a still image");
        Await([&] { return view->mStableRgbBufferInfo.addr && doc->mDocState != DOC_JUSTLOAD; }, "navigation photo presented");
        auto sendKey = [&](UINT key) {
            Await([&] { return !view->mKeyProcessing && doc->mDocState != DOC_JUSTLOAD; }, "view is ready for the next user navigation key");
            view->SendMessage(WM_KEYDOWN,key,1);
            view->SendMessage(WM_KEYUP,key,1 | (1L<<30) | (1L<<31));
        };
        auto navigate = [&](UINT key, const CString& expected, bool playing) {
            CRect before; frame->GetWindowRect(before);
            sendKey(key);
            Require(doc->mPathName.CompareNoCase(expected)==0, "file navigation selects the expected adjacent source");
            Await([&] { return view->mStableRgbBufferInfo.addr && doc->mDocState != DOC_JUSTLOAD; }, "adjacent source first frame presented");
            if (playing) Await([&] { return view->mIsPlaying && view->mStableRgbBufferInfo.ID>0; },
                "adjacent multi-frame source autoplays and advances when the setting is enabled");
            else {
                Pump(.15);
                Require(!view->mIsPlaying && view->mStableRgbBufferInfo.ID==0 &&
                    !doc->mAutoplayAfterPresent && !doc->mAutoplayMessagePending,
                    "disabled autoplay or a single-frame source remains paused without a stale autoplay request");
            }
            CRect after; frame->GetWindowRect(after);
            Require(before==after, "user file navigation keeps the existing outer window size");
            Require(doc->mCurFrameID==view->mStableRgbBufferInfo.ID && (view->mHProgress>0)==(doc->mFrames>1) &&
                frame->mStatusBar.Text().find(L"Frame")==std::wstring::npos,
                "multi-frame sources retain their progress bar without duplicating frame information in the status line");
        };
        navigate(VK_NEXT,first,true); // still -> video
        navigate(VK_NEXT,second,true); // playing video -> next video
        view->ToggleFullScreen(); Pump(.1);
        Require(view->IsFullScreen() && !frame->GetMenu(), "full-screen navigation starts without menu chrome");
        navigate(VK_PRIOR,first,true);
        Require(view->IsFullScreen() && !frame->GetMenu(), "previous-file autoplay retains full-screen mode and hidden menu");
        navigate(VK_NEXT,second,true);
        Require(view->IsFullScreen() && !frame->GetMenu(), "next-file autoplay retains full-screen mode and hidden menu");
        view->ToggleFullScreen(); Pump(.1);
        navigate(VK_PRIOR,first,true); // playing video -> previous video
        view->KillPlayTimerSafe();
        navigate(VK_NEXT,second,true); // manual pause does not disable the preference
        theApp.SetVideoAutoplayEnabled(false);
        navigate(VK_PRIOR,first,false);
        navigate(VK_NEXT,second,false);
        theApp.SetVideoAutoplayEnabled(true);
        Require(doc->ReloadDocument()!=FALSE, "internal reload succeeds with autoplay preference enabled");
        Pump(.2);
        Require(!view->mIsPlaying && !doc->mAutoplayAfterPresent && !doc->mAutoplayMessagePending,
            "internal reload does not turn a paused video into an explicit autoplay open");
        doc->Rotate90(); Pump(.2);
        Require(!view->mIsPlaying && !doc->mAutoplayAfterPresent && !doc->mAutoplayMessagePending,
            "rotating a paused video does not start autoplay");
        sendKey(VK_PRIOR);
        Await([&] { return view->mIsPlaying; }, "previous video plays before moving back to a photo");
        sendKey(VK_PRIOR);
        Require(doc->mPathName.CompareNoCase(photo)==0, "navigation returns to the still image");
        Pump(.2);
        Require(!view->mIsPlaying && !doc->mAutoplayAfterPresent && !doc->mAutoplayMessagePending,
            "moving from video to photo cannot consume a stale autoplay request");
        navigate(VK_NEXT,first,true); navigate(VK_NEXT,second,true);
        navigate(VK_NEXT,raw,true);
        Require(!doc->mFrmSrc->isFixed() && doc->mFrames==150,"navigation opens a multi-frame RAW source, not a fixed video decoder");
        navigate(VK_NEXT,single,false); navigate(VK_NEXT,lastRaw,true);
        view->KillPlayTimerSafe(); navigate(VK_PRIOR,single,false); navigate(VK_PRIOR,raw,true);
        view->ToggleFullScreen(); Pump(.05);
        navigate(VK_PRIOR,second,true); navigate(VK_NEXT,raw,true);
        view->ToggleFullScreen(); Pump(.05);
        theApp.SetVideoAutoplayEnabled(false);
        navigate(VK_PRIOR,second,false); navigate(VK_NEXT,raw,false);
        theApp.SetVideoAutoplayEnabled(true);
        Require(doc->ReloadDocument()!=FALSE,"RAW internal reload succeeds"); Pump(.1);
        Require(!view->mIsPlaying && !doc->mAutoplayAfterPresent && !doc->mAutoplayMessagePending,
            "RAW internal reload remains paused despite enabled autoplay");
        Require(AfxGetApp()->OpenDocumentFile(photo)!=nullptr,"a different source opens before the explicit RAW-open check");
        Require(AfxGetApp()->OpenDocumentFile(raw)!=nullptr,"explicit multi-frame RAW open succeeds");
        Await([&]{return view->mIsPlaying && view->mStableRgbBufferInfo.ID>0;},"explicit RAW open also autoplays after presentation");
        const CString reopen = previous.IsEmpty() ? CString(L"Tests\\fixtures\\sample_16x16.png") : previous;
        Require(AfxGetApp()->OpenDocumentFile(reopen)!=nullptr, "original source restored after autoplay regression checks");
        frame->MoveWindow(originalWindow);
        Pump(.1);
        Require(DeleteFileW(photo) && DeleteFileW(first) && DeleteFileW(second) &&
            DeleteFileW(raw) && DeleteFileW(single) && DeleteFileW(lastRaw) && RemoveDirectoryW(root),
            "autoplay fixtures cleaned up after releasing all decoder handles");
    }

    void Y4mInterpretationTests(CMainFrame* frame,CViewerView* view,CViewerDoc* doc) {
        const CString previous=doc->mPathName;
        CRect bounds; frame->GetWindowRect(bounds);
        struct Restore { bool autoplay=theApp.IsVideoAutoplayEnabled(); ~Restore(){theApp.SetVideoAutoplayEnabled(autoplay);} } restore;
        theApp.SetVideoAutoplayEnabled(false);
        wchar_t temp[MAX_PATH]; GetTempPathW(MAX_PATH,temp);
        CString root; root.Format(L"%sQ1View-y4m-%lu-%llu\\",temp,GetCurrentProcessId(),GetTickCount64());
        Require(CreateDirectoryW(root,nullptr)!=FALSE,"isolated Y4M test directory created");
        const CString path=root+L"framed_96x64.yuv", malformed=root+L"malformed.y4m";
        {
            CFile file; Require(file.Open(path,CFile::modeCreate|CFile::modeWrite|CFile::typeBinary)!=FALSE,"Y4M fixture opened");
            const std::string header="YUV4MPEG2 W96 H64 F30000:1001 Ip A1:1\n";
            file.Write(header.data(),UINT(header.size()));
            std::vector<BYTE> pixels(96*64*3/2,128);
            for(int i=0;i<90;++i) {
                const std::string marker=i%2 ? "FRAME Xtest="+std::to_string(i)+"\n" : "FRAME\n";
                file.Write(marker.data(),UINT(marker.size()));
                std::fill(pixels.begin(),pixels.begin()+96*64,BYTE(40+i));
                file.Write(pixels.data(),UINT(pixels.size()));
            }
        }
        auto verify=[&](const CString& input,bool generated) {
            Require(AfxGetApp()->OpenDocumentFile(input)!=nullptr,"framed YUV opens through the ordinary document route");
            Await([&]{return view->mStableRgbBufferInfo.addr && doc->mDocState!=DOC_JUSTLOAD;},"framed YUV first payload is presented");
            Require(doc->mFrmSrc && !doc->mFrmSrc->isFixed() && doc->mFrmSrc->RawLayout() && doc->mFrames>1,
                "playable Y4M is indexed RAW input rather than a locked decoded-video source");
            frame->RefreshCommandStates();
            Require(frame->IsCommandEnabled(ID_RESOLUTION_START) && frame->IsCommandEnabled(ID_CS_START) &&
                IsWindowEnabled(::GetDlgItem(frame->mUiFrame.MenuHost(),2)) && IsWindowEnabled(::GetDlgItem(frame->mUiFrame.MenuHost(),3)),
                "playable framed RAW exposes enabled resolution and color-space buttons");
            const int width=doc->mW,height=doc->mH; const long count=doc->mFrames;
            Require(doc->mColorSpace==QIMAGE_CS_YUV420 && std::abs(doc->mFps-30000.0/1001)<.00001,
                "Y4M header supplies native YUV420 and rational timing instead of decoded BGR");
            if(generated) Require(count==90 && width==96 && height==64 && doc->mOrigBuf[0]==40,
                "variable FRAME markers are excluded from native pixel data and frame count");
            Require(doc->SeekScene(17)>=0,"indexed framed RAW seeks to frame 17"); Pump(.05);
            if(generated) Require(doc->mOrigBuf[0]==57,"seeking skips variable-length markers and reads the correct raw payload");
            auto format=[&](QIMAGE_CS cs) {
                for(int i=0;i<ARRAY_SIZE(qcsc_info_table);++i) if(qcsc_info_table[i].cs==cs) frame->SendMessage(WM_COMMAND,ID_CS_START+i);
                Pump(.05); Require(doc->mColorSpace==cs,"framed RAW color-space menu changes actual interpretation");
            };
            format(QIMAGE_CS_NV12);
            view->SendMessage(WM_KEYDOWN,VK_SPACE,1);
            Await([&]{return view->mIsPlaying && view->mStableRgbBufferInfo.ID>0;},"reinterpreted framed RAW plays through indexed worker reads");
            frame->SendMessage(WM_COMMAND,ID_RESOLUTION_START+1); Pump(.05); // 320x240
            Require(doc->mW==320 && doc->mH==240 && doc->mFrames==count && !view->mIsPlaying,
                "resolution can change during RAW playback safely, retaining container frame boundaries");
            const auto bytes=doc->mFrmSrc->RawLayout()->payloadBytes;
            Require(doc->mOrigSceneSize>bytes && doc->mOrigBuf[bytes]==0 && doc->mOrigBuf[doc->mOrigSceneSize-1]==0,
                "oversized reinterpretation pads missing pixels instead of reading frame headers or the next image");
            frame->ApplyResolution(width,height); format(QIMAGE_CS_YUV420);
            theApp.SetVideoAutoplayEnabled(true);
            Require(AfxGetApp()->OpenDocumentFile(L"Tests\\fixtures\\sample_16x16.png")!=nullptr,"still source opens before framed RAW autoplay");
            Require(AfxGetApp()->OpenDocumentFile(input)!=nullptr,"framed RAW is opened again as a new source");
            Await([&]{return view->mIsPlaying && view->mStableRgbBufferInfo.ID>0;},"framed RAW autoplays with editable native interpretation");
            view->KillPlayTimerSafe(); theApp.SetVideoAutoplayEnabled(false);
        };
        verify(path,true);
        wchar_t real[32768]={};
        if(GetEnvironmentVariableW(L"Q1VIEW_Y4M_TEST_FILE",real,_countof(real))) verify(real,false);
        for(const std::string& data : {std::string("YUV4MPEG2 W0 H64\nFRAME\n"),std::string("YUV4MPEG2 W2 H2 C444\nFRAME\n"),
            std::string("YUV4MPEG2 W2 H2\nFRAME\n12"),std::string("YUV4MPEG2 W2147483647 H2147483647 C420p10\nFRAME\n")}) {
            CFile file; Require(file.Open(malformed,CFile::modeCreate|CFile::modeReadWrite|CFile::typeBinary)!=FALSE,"parser boundary fixture opened");
            file.Write(data.data(),UINT(data.size())); file.Seek(0,CFile::begin);
            q1view::Y4mInfo info;
            Require(!q1view::ParseY4m(file,info),"invalid dimensions, unsupported layouts and truncated payloads are rejected safely");
        }
        Require(AfxGetApp()->OpenDocumentFile(previous)!=nullptr,"original source restored after framed RAW checks");
        frame->MoveWindow(bounds); Pump(.05);
        Require(DeleteFileW(path) && DeleteFileW(malformed) && RemoveDirectoryW(root),"generated framed RAW fixtures released and cleaned up");
    }

    void InteractionMenuTests(CMainFrame* frame,CViewerView* view) {
        auto* options=frame->GetMenu()->GetSubMenu(5);
        const bool selected=view->mSelMode, synced=frame->IsSyncInputEnabled();
        const auto path=view->GetDocument()->mPathName;
        const auto generation=view->GetDocument()->mOpenGeneration;
        const auto pixels=view->mStableRgbBufferInfo.addr;
        Require(options->GetMenuState(ID_SELECTION_MODE,MF_BYCOMMAND)!=UINT(-1) &&
            options->GetMenuState(ID_SYNC_INPUT,MF_BYCOMMAND)!=UINT(-1) && view->mMouseMenu.GetMenuItemCount()==2,
            "Viewer exposes selection/sync in Options while retaining both context shortcuts");
        auto check=[&] {
            frame->SendMessage(WM_INITMENUPOPUP,reinterpret_cast<WPARAM>(options->GetSafeHmenu()),5);
            view->SendMessage(WM_INITMENUPOPUP,reinterpret_cast<WPARAM>(view->mMouseMenu.GetSafeHmenu()));
            for(auto* menu : {options,&view->mMouseMenu}) {
                Require(bool(menu->GetMenuState(ID_SELECTION_MODE,MF_BYCOMMAND)&MF_CHECKED)==view->mSelMode,
                    "Options and context selection checks follow the one actual Viewer mode");
                Require(bool(menu->GetMenuState(ID_SYNC_INPUT,MF_BYCOMMAND)&MF_CHECKED)==frame->IsSyncInputEnabled(),
                    "Options and context sync checks follow the same per-window setting");
            }
        };
        check();
        if(frame->mpDrawer) frame->mpDrawer->SetFocus();
        frame->SendMessage(WM_COMMAND,ID_SELECTION_MODE);
        Require(view->mSelMode!=selected,"Viewer Options selection command toggles exactly once while browser has focus"); check();
        view->SendMessage(WM_COMMAND,ID_SELECTION_MODE);
        Require(view->mSelMode==selected,"Viewer context selection command uses the same toggle"); check();
        view->SendMessage(WM_KEYDOWN,'S',1); view->SendMessage(WM_KEYUP,'S',1);
        Require(view->mSelMode!=selected,"existing S shortcut shares the Options selection state"); check();
        view->SendMessage(WM_KEYDOWN,'S',1); view->SendMessage(WM_KEYUP,'S',1);
        frame->SendMessage(WM_COMMAND,ID_SYNC_INPUT);
        Require(frame->IsSyncInputEnabled()!=synced,"Options toggles Viewer window synchronization exactly once"); check();
        view->SendMessage(WM_COMMAND,ID_SYNC_INPUT);
        Require(frame->IsSyncInputEnabled()==synced && view->mSelMode==selected,"context shortcut restores the shared interaction state"); check();
        Require(view->GetDocument()->mPathName==path && view->GetDocument()->mOpenGeneration==generation &&
            view->mStableRgbBufferInfo.addr==pixels,"interaction menu changes do not reopen or replace the current source");
        view->SetFocus();
    }

    void CommandRefreshTests(CMainFrame* frame) {
        frame->RefreshCommandStates(true);
        const auto applied = frame->mCommandMenuRefreshCount;
        const double begin = CGalleryGridCanvas::Now();
        for (int i=0;i<2000;++i) frame->RefreshCommandStates();
        const double cached = CGalleryGridCanvas::Now()-begin;
        Require(frame->mCommandMenuRefreshCount==applied, "unchanged idle state performs no full menu enumeration or hosted-button synchronization");
        const double forceBegin = CGalleryGridCanvas::Now();
        for (int i=0;i<2000;++i) frame->RefreshCommandStates(true);
        fprintf(report,"Command refresh / 2000 calls: cached %.3f ms; forced full update %.3f ms\n", cached*1000,(CGalleryGridCanvas::Now()-forceBegin)*1000);
        Require(frame->mCommandMenuRefreshCount==applied+2000, "explicit menu initialization can still force a complete availability refresh");
        auto* view = static_cast<CViewerView*>(frame->GetActiveView());
        auto* doc = view->GetDocument();
        frame->UpdateCsLabel(CString(q1::image_find_cs_name(doc->mSortedCscInfo,doc->mColorSpace)).MakeUpper());
        frame->RefreshCommandStates();
        Require(frame->mCommandMenuRefreshCount==applied+2001, "rebuilding a source-format menu label invalidates cached menu availability");
        const auto root = frame->GetMenu()->GetSafeHmenu();
        const bool enabled = !(GetMenuState(root,2,MF_BYPOSITION)&(MF_GRAYED|MF_DISABLED));
        Require(enabled==frame->CommandContext().rawInput, "cached refresh does not leave a rebuilt fixed-source RAW menu enabled");
    }
    void ViewerShellTests(CMainFrame* frame, CViewerView* view, CViewerDoc* doc) {
        // A source change must refresh hosted labels independently of zoom.
        frame->UpdateResolutionLabel(800,600);
        frame->UpdateCsLabel(L"YUV420");
        frame->UpdateFpsLabel(1.0);
        frame->DrawMenuBar();
        doc->UpdateMenu();
        CString expectedLabels[3];
        expectedLabels[0].Format(L"%d&x%d",doc->mW,doc->mH);
        expectedLabels[1]=CString(q1::image_find_cs_name(doc->mSortedCscInfo,doc->mColorSpace)).MakeUpper();
        expectedLabels[2].Format(L"%.2ff&ps",doc->mFps);
        for(int i=0;i<3;++i) {
            wchar_t label[256]={};
            GetWindowTextW(::GetDlgItem(frame->mUiFrame.MenuHost(),i+2),label,_countof(label));
            Require(expectedLabels[i]==label,
                "opening a source refreshes the actual resolution, format and FPS buttons without a top zoom item");
        }
        CRect original; frame->GetWindowRect(original);
        const CString path = doc->mPathName;
        const auto generation = doc->mOpenGeneration;
        const auto pixels = view->mStableRgbBufferInfo.addr;
        const auto presented = view->mStableRgbBufferInfo.ID;
        const auto selection = view->mSelRegions.size();
        const UINT dpi = q1view::WindowsUiDpi(frame->m_hWnd);
        for (auto size : {CSize(640,480), CSize(960,600), CSize(1440,900)}) {
            // The specified client dimensions, not an outer-window estimate.
            CRect outer, client; frame->GetWindowRect(outer); frame->GetClientRect(client);
            frame->SetWindowPos(nullptr, 0, 0, q1view::WindowsUiPixels(size.cx,dpi) + outer.Width()-client.Width(),
                q1view::WindowsUiPixels(size.cy,dpi) + outer.Height()-client.Height(), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
            Pump(.08);
            frame->GetClientRect(client);
            const auto& rects = frame->mUiFrame.MenuRects();
            Require(frame->mUiFrame.OverflowPositions().empty(), "Viewer keeps all original input menus directly accessible without More");
            for (size_t i=0; i<rects.size(); ++i) {
                const auto& r=rects[i];
                Require(r.left>=0 && r.top>=0 && r.right<=client.right && r.right>r.left, "all Viewer command targets remain visible within the measured wrapped rows");
                for(size_t j=i+1; j<rects.size(); ++j) { CRect overlap; Require(!overlap.IntersectRect(&r,&rects[j]), "wrapped command targets never overlap"); }
            }
            Require(!rects.empty() && rects.back().right==client.right,
                "Help remains at the far right without a duplicate top-level magnification field");
            CRect status, content, splitter;
            ::GetWindowRect(frame->mStatusBar.Window(),status); frame->ScreenToClient(status);
            frame->GetContentRect(content); frame->mwndSplitter.GetWindowRect(splitter); frame->ScreenToClient(splitter);
            Require(status.bottom==client.bottom && status.Height()==frame->StatusHeight() && content.bottom==status.top && content==splitter,
                "read-only status reserves exactly its measured height without covering the image");
        }
        frame->MoveWindow(original); Pump(.1);
        frame->UpdateViewerStatus();
        wchar_t accessible[1024] = {}; GetWindowTextW(frame->mStatusBar.Window(),accessible,_countof(accessible));
        Require(frame->mStatusBar.Text()==accessible && frame->mStatusBar.Text().find(L"%")!=std::wstring::npos,
            "native read-only status exposes the full actual zoom text, independent of visual ellipsis");
        const auto rects = frame->mUiFrame.MenuRects();
        const int height = frame->mUiFrame.Height();
        auto verifyZoom=[&] {
            CString expected; expected.Format(L"%.1f%%",double(view->mN)*100.0);
            Require(frame->mStatusBar.ZoomText()==expected.GetString(),
                "bottom-right magnification reports the real zoom without a duplicate menu item");
            const std::wstring sampling=view->GetScalingStatusLabel().GetString();
            Require(frame->mStatusBar.SamplingText()==sampling && frame->mStatusBar.Text()==sampling+L"  |  "+expected.GetString(),
                "status retains the actual sampling mode and zoom only, excluding dimensions, color space, Fit/Manual and frame text");
            q1view::WindowsUiFontCache fonts; HDC dc=CreateCompatibleDC(nullptr);
            const auto old=SelectObject(dc,fonts.Get(q1view::WindowsUiFontRole::Supporting,frame->mStatusBar.Window()));
            RECT bounds,left,right; ::GetClientRect(frame->mStatusBar.Window(),&bounds);
            frame->mStatusBar.TextRects(dc,bounds,left,right);
            const int padding=q1view::WindowsUiPixels(8,q1view::WindowsUiDpi(frame->mStatusBar.Window()));
            SIZE size={}; GetTextExtentPoint32W(dc,expected,expected.GetLength(),&size);
            Require(left.left==padding && right.right==bounds.right-padding && left.right<=right.left && right.right-right.left>=size.cx,
                "sampling stays left and the entire measured zoom stays right without overlap or clipping");
            SelectObject(dc,old); DeleteDC(dc);
            Require(frame->mUiFrame.Height()==height,"changing magnification never changes menu height");
            const auto& current=frame->mUiFrame.MenuRects();
            for(size_t i=0;i<rects.size();++i) Require(EqualRect(&rects[i],&current[i])!=FALSE,
                "all magnification changes preserve Help and every menu rectangle");
            CRect client; frame->GetClientRect(client);
            Require(current.back().right==client.right,"status changes do not move the right-aligned Help menu");
            for(size_t i=0;i<current.size();++i) for(size_t j=i+1;j<current.size();++j) {
                CRect overlap; Require(!overlap.IntersectRect(&current[i],&current[j]),
                    "zoom values beyond the compact reservation expand without overlapping other labels");
            }
        };
        auto setZoom=[&](float zoom) {
            ViewerSyncInputState input={}; input.command=VIEWER_SYNC_VIEW_STATE; input.scalar=zoom;
            view->ApplySyncInput(input);
        };
        for(float zoom : {.67f,1.f,1.13f,2.f,9.99f,10.f,99.99f,128.f}) {
            setZoom(zoom); Pump(.02); verifyZoom();
        }
        setZoom(.99f);
        CPoint wheel(view->mWCanvas/2,view->mHCanvas/2); view->ClientToScreen(&wheel);
        view->SendMessage(WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),MAKELPARAM(wheel.x,wheel.y)); Pump(.03);
        Require(view->mN==1.f,"the existing mouse-wheel zoom can still reach exact 100 percent without a reset button");
        verifyZoom();
        view->SendMessage(WM_MOUSEWHEEL,MAKEWPARAM(0,WHEEL_DELTA),MAKELPARAM(wheel.x,wheel.y)); Pump(.03);
        Require(view->mN>1.f,"mouse wheel actually zooms beyond 100 percent"); verifyZoom();
        view->FitToWindow(); view->Invalidate(FALSE); Pump(.03); verifyZoom();
        const auto originalMode=view->GetScalingMode();
        for(auto mode : {q1::ImageScalingMode::Smooth,q1::ImageScalingMode::PixelExact,q1::ImageScalingMode::Auto}) {
            view->SetScalingMode(mode,false,false); Pump(.03); verifyZoom();
        }
        view->SetScalingMode(originalMode,false,false); Pump(.03);
        Require(frame->mUiFrame.Height()==height && frame->mUiFrame.MenuRects().size()==rects.size(), "zoom status never changes command row height or control count");
        for(size_t i=0;i<rects.size();++i) Require(EqualRect(&rects[i],&frame->mUiFrame.MenuRects()[i])!=FALSE,"zoom leaves Help and every command hit target fixed");
        Require(doc->mPathName==path && doc->mOpenGeneration==generation && view->mStableRgbBufferInfo.addr==pixels &&
            view->mStableRgbBufferInfo.ID==presented && view->mSelRegions.size()==selection,
            "Viewer shell changes preserve source, presented frame, decoder generation and ROI selection");
        // The full suite below separately exercises manual pan anchors, E,
        // playback and GPU recreation. These checks exercise live zoom labels.
    }

    void Run() {
        auto frame = static_cast<CMainFrame*>(AfxGetMainWnd());
        auto view = static_cast<CViewerView*>(frame->GetActiveView());
        auto doc = static_cast<CViewerDoc*>(frame->GetActiveDocument());
        Require(frame && view && doc, "real Viewer document/view created");
        {
            const UINT selectionState=view->mMouseMenu.GetMenuState(ID_MOUSEMENU_START,MF_BYCOMMAND);
            view->SendMessage(WM_INITMENUPOPUP,reinterpret_cast<WPARAM>(view->mMouseMenu.GetSafeHmenu()));
            MENUITEMINFOW info={sizeof(info)}; info.fMask=MIIM_FTYPE|MIIM_DATA|MIIM_STATE;
            Require(GetMenuItemInfoW(view->mMouseMenu.GetSafeHmenu(),0,TRUE,&info) && (info.fType&MFT_OWNERDRAW),
                "Viewer canvas right-click menu uses Comparator's shared menu renderer");
            MEASUREITEMSTRUCT measure={}; measure.CtlType=ODT_MENU; measure.itemData=info.dwItemData;
            Require(view->SendMessage(WM_MEASUREITEM,0,reinterpret_cast<LPARAM>(&measure)) &&
                measure.itemHeight>=UINT(q1view::WindowsUiPixels(32,q1view::WindowsUiDpi(view->m_hWnd),q1view::WindowsUiSettings().Scale())),
                "Viewer context menu uses the same DPI-aware body font and row spacing as Comparator");
            Require((view->mMouseMenu.GetMenuState(ID_MOUSEMENU_START,MF_BYCOMMAND)&MF_CHECKED)==(selectionState&MF_CHECKED),
                "context-menu styling preserves selection mode's native checked state");
        }
        for(int i=0;i<frame->GetMenu()->GetMenuItemCount();++i)
        {
            Require(frame->GetMenu()->GetMenuItemID(i)!=ID_FILE_OPEN, "Viewer has no duplicate top-level Open command");
            Require(frame->GetMenu()->GetMenuItemID(i)!=ID_MAGNIFY,"magnification appears only in the bottom status, never in the menu bar");
            const auto label=frame->mUiMenus.Text(frame->GetMenu()->GetSafeHmenu(),i,true);
            Require(label!=L"&View" && label!=L"&More", "Viewer has no separate View or More menu");
        }
        Require(frame->GetMenu()->GetSubMenu(1)->GetSafeHmenu()==frame->mResolutionMenu.GetSafeHmenu() &&
            frame->GetMenu()->GetSubMenu(2)->GetSafeHmenu()==frame->mCsMenu.GetSafeHmenu() &&
            frame->GetMenu()->GetSubMenu(3)->GetSafeHmenu()==frame->mFpsMenu.GetSafeHmenu(),
            "resolution, pixel format and FPS retain their original top-level positions");
        const int rootCount=frame->GetMenu()->GetMenuItemCount();
        Require(frame->GetMenu()->GetMenuItemID(4)==ID_TOGGLE_DRAWER &&
            frame->mUiMenus.Text(frame->GetMenu()->GetSafeHmenu(),5,true)==L"&Options",
            "Browse appears immediately left of Options in root and native button order");
        Require(rootCount>=8 && frame->GetMenu()->GetMenuItemID(rootCount-1)==ID_VIEWER_HELP,
            "Help is the last root item, with no zoom, Fit or 100 percent menu controls");
        Require(frame->GetMenu()->GetSubMenu(0)->GetMenuState(ID_FILE_OPEN,MF_BYCOMMAND)!=UINT(-1),
            "File menu retains its original Open command");
        CMenu* fileMenu=frame->GetMenu()->GetSubMenu(0);
        CMenu* settingsMenu=frame->GetMenu()->GetSubMenu(5);
        Require(fileMenu->GetMenuState(ID_TOGGLE_DRAWER,MF_BYCOMMAND)==UINT(-1) &&
            fileMenu->GetMenuState(ID_AUTOPLAY_VIDEOS,MF_BYCOMMAND)==UINT(-1) &&
            settingsMenu->GetMenuState(ID_AUTOPLAY_VIDEOS,MF_BYCOMMAND)!=UINT(-1),
            "File has no duplicate browser or autoplay setting; autoplay is available under Options");
        const bool autoplayBefore=theApp.IsVideoAutoplayEnabled();
        auto verifyAutoplayCheck=[&] {
            frame->SendMessage(WM_INITMENUPOPUP,reinterpret_cast<WPARAM>(settingsMenu->GetSafeHmenu()),5);
            Require(bool(settingsMenu->GetMenuState(ID_AUTOPLAY_VIDEOS,MF_BYCOMMAND)&MF_CHECKED)==theApp.IsVideoAutoplayEnabled(),
                "Options autoplay check reflects the unchanged saved preference and existing command handler");
        };
        verifyAutoplayCheck(); frame->SendMessage(WM_COMMAND,ID_AUTOPLAY_VIDEOS);
        Require(theApp.IsVideoAutoplayEnabled()!=autoplayBefore,"moved autoplay command still toggles its original preference");
        verifyAutoplayCheck(); frame->SendMessage(WM_COMMAND,ID_AUTOPLAY_VIDEOS); verifyAutoplayCheck();
        Require(theApp.IsVideoAutoplayEnabled()==autoplayBefore,"autoplay preference is restored after isolated menu checks");
        InteractionMenuTests(frame,view);
        std::vector<ACCEL> accelerators(CopyAcceleratorTable(frame->m_hAccelTable,nullptr,0));
        CopyAcceleratorTable(frame->m_hAccelTable,accelerators.data(),int(accelerators.size()));
        Require(std::any_of(accelerators.begin(),accelerators.end(),[](const ACCEL& key) {
            return key.cmd==ID_FILE_OPEN && key.key=='O' && (key.fVirt&(FVIRTKEY|FCONTROL))==(FVIRTKEY|FCONTROL);
        }), "Ctrl+O retains the original file-open command binding");
        Require(std::any_of(accelerators.begin(),accelerators.end(),[](const ACCEL& key) {
            return key.cmd==ID_TOGGLE_DRAWER && key.key=='E' && (key.fVirt&FVIRTKEY) && !(key.fVirt&(FCONTROL|FALT|FSHIFT));
        }),"E still toggles Browse after removing its duplicate File entry");
        frame->RefreshCommandStates();
        const auto commandContext = frame->CommandContext();
        Require(frame->IsCommandEnabled(ID_EDIT_PASTE) == bool(IsClipboardFormatAvailable(CF_DIB) || IsClipboardFormatAvailable(CF_DIBV5)),
            "paste menu policy reflects supported clipboard formats without changing the clipboard");
        Require(frame->IsCommandEnabled(ID_COMPARE), "main Compare remains enabled with or without a current file");
        for (int i=0;i<frame->GetMenu()->GetMenuItemCount();++i) if(frame->GetMenu()->GetMenuItemID(i)==ID_COMPARE) {
            Require((frame->GetMenu()->GetMenuState(i,MF_BYPOSITION)&(MF_GRAYED|MF_DISABLED))==0 &&
                IsWindowEnabled(::GetDlgItem(frame->mUiFrame.MenuHost(),i+1)),
                "the actual Compare menu and native button are enabled, including the empty startup window");
        }
        Require(!frame->OpenComparatorFiles({}), "selection-based comparison still rejects an empty batch instead of launching a blank workspace");
        Require(!frame->OpenComparatorFiles(std::vector<CString>(5,L"unused.png")), "selection-based comparison still rejects more than four files");
        if(!commandContext.pixels) Require(frame->mStatusBar.ZoomText()==L"--" && frame->mStatusBar.SamplingText().empty(),
            "empty input does not pretend to have a loaded 100 percent zoom");
        const auto clipboardSequence = GetClipboardSequenceNumber();
        if (!commandContext.pixels) {
            frame->SendMessage(WM_COMMAND, ID_EDIT_COPY); frame->OnEditCopy();
            Require(GetClipboardSequenceNumber() == clipboardSequence, "disabled copy is inert through WM_COMMAND and direct handler without touching the clipboard");
        }
        Require(!frame->IsCommandEnabled(ID_CS_END) && !frame->IsCommandEnabled(ID_RESOLUTION_END), "unassigned dynamic IDs cannot invoke a parser or custom-input dialog");
        PaintBackgroundTests(view,doc);
        wchar_t paintOnly[8] = {};
        if (GetEnvironmentVariableW(L"Q1VIEW_GALLERY_PAINT_ONLY",paintOnly,_countof(paintOnly))) {
            wchar_t image[MAX_PATH] = {};
            GetFullPathNameW(L"Tests\\fixtures\\sample_16x16.png",MAX_PATH,image,nullptr);
            Require(AfxGetApp()->OpenDocumentFile(image)!=nullptr,"repository image opened for loaded-state paint regression");
            Pump(.2);
            view=static_cast<CViewerView*>(frame->GetActiveView());
            doc=static_cast<CViewerDoc*>(frame->GetActiveDocument());
            Require(doc->mDocState!=DOC_JUSTLOAD,"real source has left the loading state");
            PaintBackgroundTests(view,doc);
            fprintf(report,"Paint-only run; full gallery/GPU suite is separate\n"); return;
        }
        if (doc->mPathName.IsEmpty()) DefaultWindowTests(frame, view, doc);
        else {
            // The CI source-open path uses an image fixture. mOrigW/H are
            // rotation bookkeeping (-1 before rotation), not source dimensions.
            const auto source = q1::imreadW(doc->mPathName.GetString());
            Require(!source.empty() && doc->mFrmSrc != nullptr &&
                doc->mW == source.cols && doc->mH == source.rows,
                "command-line image fixture launch retains its decoded source resolution");
            const int w=doc->mW, h=doc->mH; const auto cs=doc->mColorSpace; const auto generation=doc->mOpenGeneration;
            frame->RefreshCommandStates();
            Require(!frame->IsCommandEnabled(ID_CS_START) && !frame->IsCommandEnabled(ID_RESOLUTION_START),
                "fixed decoded source disables both RAW menu groups");
            frame->SendMessage(WM_COMMAND, ID_CS_START); frame->OnCsChange(ID_CS_START);
            doc->NextCs(); frame->NextResolution(); frame->ApplyResolution(176,144);
            Require(doc->mW==w && doc->mH==h && doc->mColorSpace==cs && doc->mOpenGeneration==generation,
                "disabled RAW interpretation is inert through menu, shortcut helper and direct handler paths");
        }
        wchar_t startupOnly[8] = {};
        if (GetEnvironmentVariableW(L"Q1VIEW_GALLERY_STARTUP_ONLY", startupOnly, _countof(startupOnly))) {
            fprintf(report, "Startup-only run; full gallery/GPU suite is separate\n");
            return;
        }
        frame->ShowWindow(SW_SHOWNOACTIVATE);
        frame->MoveWindow(20, 20, 1200, 850);
        if (!frame->mDrawerVisible) frame->OnToggleDrawer();
        Pump(.4);
        wchar_t captionOnly[8] = {};
        if (GetEnvironmentVariableW(L"Q1VIEW_FOLDER_CAPTION_ONLY",captionOnly,_countof(captionOnly))) {
            FolderCaptionTests(frame);
            fprintf(report,"Focused folder-caption alignment checks completed\n"); return;
        }
        wchar_t browserOnly[8] = {};
        if (GetEnvironmentVariableW(L"Q1VIEW_BROWSER_NAVIGATION_ONLY",browserOnly,_countof(browserOnly))) {
            wchar_t fixture[MAX_PATH]; GetFullPathNameW(L"Tests\\fixtures\\sample_16x16.png",MAX_PATH,fixture,nullptr);
            Require(AfxGetApp()->OpenDocumentFile(fixture)!=nullptr,"repository sample opened for focused navigation checks");
            Pump(.2);
            BrowserNavigationTests(frame,fixture);
            RecycleTests(frame,fixture);
            fprintf(report,"Focused Browse navigation and interaction checks completed\n"); return;
        }
        // The shell/zoom checks below require a displayed image. A no-argument
        // startup run has deliberately just verified the empty -- status.
        if (doc->mPathName.IsEmpty()) {
            wchar_t fixture[MAX_PATH]; GetFullPathNameW(L"Tests\\fixtures\\sample_16x16.png",MAX_PATH,fixture,nullptr);
            Require(AfxGetApp()->OpenDocumentFile(fixture)!=nullptr,"loaded-source shell checks establish their own repository fixture");
            Pump(.2);
        }
        ChromeTests(frame, view);
        ViewerShellTests(frame, view, doc);
        CommandRefreshTests(frame);
        AutoplayNavigationTests(frame);
        Y4mInterpretationTests(frame,view,doc);
        wchar_t navigationOnly[8] = {};
        if (GetEnvironmentVariableW(L"Q1VIEW_AUTOPLAY_NAVIGATION_ONLY", navigationOnly, _countof(navigationOnly))) {
            fprintf(report,"Autoplay navigation-only run; full gallery/GPU suite is separate\n");
            return;
        }
        auto& pane = *frame->mpDrawer;
        Require(pane.mPrivateFontLoaded && pane.mFontFamily == L"Pretendard Variable",
            "bundled Pretendard typography loaded for the MFC drawer");
        for (UINT dpi : { 96u, 144u, 192u }) {
            pane.RebuildFonts(dpi);
            LOGFONT regular = {}, folderFont = {}, badge = {};
            pane.mLabelFont.GetLogFont(&regular);
            pane.mFolderFont.GetLogFont(&folderFont);
            pane.mExtFont.GetLogFont(&badge);
            const double textScale = q1view::WindowsUiSettings().Scale();
            Require(regular.lfHeight == -q1view::WindowsUiPixels(13, dpi, textScale) && regular.lfWeight == FW_NORMAL &&
                folderFont.lfHeight == -q1view::WindowsUiPixels(13, dpi, textScale) && folderFont.lfWeight == FW_MEDIUM &&
                badge.lfHeight == -q1view::WindowsUiPixels(12, dpi, textScale) && badge.lfWeight == FW_SEMIBOLD,
                "MFC drawer typography scales at 100, 150, and 200 percent DPI");
        }
        pane.RebuildFonts(GetDpiForWindow(pane.m_hWnd));
        pane.ApplyViewStep(1, false);
        auto& grid = *pane.mGrid;

        wchar_t temp[MAX_PATH]; GetTempPathW(MAX_PATH, temp);
        CString folder; folder.Format(L"%sQ1View-gallery-%lu-\xAC80\xC99D\\", temp, GetCurrentProcessId());
        Require(CreateDirectoryW(folder, nullptr) != FALSE, "Unicode fixture directory created");
        CString original = doc->GetPathName();
        // Copy the small repository test image, not any user media.
        wchar_t cwd[32768]; GetCurrentDirectoryW(_countof(cwd), cwd);
        CString fixture = CString(cwd) + L"\\Tests\\fixtures\\sample_16x16.png";
        for (int i = 0; i < 3000; ++i) {
            CString path; path.Format(L"%stile-%04d.png", folder.GetString(), i);
            if (!CopyFileW(fixture, path, TRUE)) throw std::runtime_error("fixture copy failed");
        }
        pane.NavigateTo(folder);
        Pump(2);
        Require(pane.mEntries.size() == 3001 && pane.mEntries.front().kind == CThumbnailPane::ENTRY_PARENT &&
            pane.GetItemCount() == 0, "large grid keeps one parent tile without list-control rows or image-list copies");
        Require(grid.mDevice && grid.mContext && grid.mTarget && grid.mFontCollection && grid.mBadgeText,
            "actual Direct2D/D3D11 target uses the bundled font collection");
        Microsoft::WRL::ComPtr<IDWriteTextLayout> longFolderLayout;
        Require(grid.CreateFolderTextLayout(
            L"[This is a deliberately very long folder name that must wrap before it is trimmed]",
            110.0f, 52.0f, longFolderLayout), "long-folder DirectWrite layout created");
        UINT32 lineCount = 0;
        longFolderLayout->GetLineMetrics(nullptr, 0, &lineCount);
        std::vector<DWRITE_LINE_METRICS> lineMetrics(lineCount);
        Require(SUCCEEDED(longFolderLayout->GetLineMetrics(lineMetrics.data(),lineCount,&lineCount)),"long-folder line metrics available");
        UINT32 visibleLines=0; float visibleHeight=0;
        for(const auto& line : lineMetrics) {
            if(line.height<=0 || visibleHeight+line.height>longFolderLayout->GetMaxHeight()+.02f) break;
            visibleHeight+=line.height; ++visibleLines;
        }
        Require(visibleLines > 1 && lineMetrics[visibleLines-1].isTrimmed,
            "long folder name wraps before the final visible line is ellipsized");
        FolderCaptionTests(frame);
        Require(!grid.mCache.empty(), "worker results populate CPU thumbnail cache");
        bool uploaded = false;
        for (auto& p : grid.mCache) if (p.second.gpu) uploaded = true;
        Require(uploaded, "visible thumbnails uploaded to GPU");
        unsigned generation = pane.mGen.load();
        auto entries = pane.mEntries.data();
        grid.Select(10, true);
        // Real theme commands must not invalidate the folder or its decoded/GPU
        // thumbnails. Wait for the existing visible queue, then snapshot handles.
        Await([&] { std::lock_guard<std::mutex> lock(pane.mMutex); return pane.mTasks.empty() && pane.mOutstanding==0; },
            "visible thumbnail work settles before appearance checks");
        const auto theme = q1view::WindowsUiAppearanceState().Appearance();
        const auto selectedMedia = pane.SelectedMediaPaths();
        const auto scroll = grid.mLayout.scroll;
        std::map<int,std::pair<HBITMAP,ID2D1Bitmap1*>> cachedImages;
        for (const auto& image : grid.mCache) cachedImages[image.first] = {image.second.cpu,image.second.gpu.Get()};
        const auto source = doc->mFrmSrc;
        const auto opened = doc->mOpenGeneration;
        const auto imageInfo = view->mStableRgbBufferInfo;
        std::vector<BYTE> imageBytes;
        if (imageInfo.addr && view->mW>0 && view->mH>0)
            imageBytes.assign(imageInfo.addr,imageInfo.addr+size_t(ROUNDUP_DWORD(view->mW))*view->mH*QIMG_DST_RGB_BYTES);
        const auto zoomBefore = view->mN, xBefore = view->mXOff, yBefore = view->mYOff;
        const auto regionsBefore = view->mSelRegions;
        QSelRegion appearanceRoi; appearanceRoi.SetRect(1,1,4,4); view->mSelRegions.push_back(appearanceRoi);
        cv::Mat captured;
        if (imageInfo.addr) captured = view->CreateRoiMat(0,0,4,4).clone();
        const auto captureBuffer = view->mCaptRgbBuf;
        const int captureSize = view->mCaptRgbBufSize;
        std::vector<BYTE> captureBytes;
        if (!captured.empty()) captureBytes.assign(captureBuffer,captureBuffer+captureSize);
        CRect appearanceBounds; frame->GetWindowRect(appearanceBounds);
        HMENU options = GetSubMenu(frame->GetMenu()->GetSafeHmenu(),5);
        Require(frame->mUiMenus.Text(frame->GetMenu()->GetSafeHmenu(),5,true)==L"&Options" && options && GetMenuItemCount(options)==7 &&
            frame->mUiMenus.Text(options,0,true)==L"&Image Scaling" && GetMenuItemCount(GetSubMenu(options,0))==3 &&
            GetMenuItemID(options,1)==ID_AUTOPLAY_VIDEOS && GetMenuItemID(options,3)==ID_SELECTION_MODE &&
            GetMenuItemID(options,4)==ID_SYNC_INPUT && frame->mUiMenus.Text(options,6,true)==L"&Theme",
            "Options contains scaling, autoplay, interaction modes and Theme without unwanted zoom commands");
        for (UINT command : {q1view::ID_UI_APPEARANCE_LIGHT,q1view::ID_UI_APPEARANCE_DARK,
            q1view::ID_UI_APPEARANCE_LIGHT,q1view::ID_UI_APPEARANCE_SYSTEM}) {
            if (command==q1view::ID_UI_APPEARANCE_DARK || command==q1view::ID_UI_APPEARANCE_SYSTEM)
                Require(WriteSharedThemeFromChild(AfxGetApp()->m_pszRegistryKey,command-q1view::ID_UI_APPEARANCE_SYSTEM),
                    "separate process writes shared theme for existing Viewer without activation");
            else frame->SendMessage(WM_COMMAND,command);
            Pump(.08);
            Require(q1view::WindowsUiAppearanceCommandChecked(command), "real Viewer appearance command updates checked preference");
            Require(Q1UI_COLOR_CANVAS_BG==q1view::WindowsUiColorValue(q1view::WindowsUiColor::Canvas) &&
                (q1view::WindowsUiAppearanceState().HighContrast() || Q1UI_COLOR_CANVAS_BG==
                    (q1view::WindowsUiAppearanceState().Dark() ? RGB(24,24,24) : RGB(236,236,236))),
                "Viewer theme changes UI and canvas brightness together");
            CRect current; frame->GetWindowRect(current);
            Require(current==appearanceBounds && view->mN==zoomBefore && view->mXOff==xBefore && view->mYOff==yBefore &&
                doc->mFrmSrc==source && doc->mOpenGeneration==opened && doc->GetPathName()==original,
                "Viewer appearance switch preserves window, zoom, pan, source and open generation");
            Require(imageBytes.empty() || (view->mStableRgbBufferInfo.addr==imageInfo.addr &&
                std::equal(imageBytes.begin(),imageBytes.end(),view->mStableRgbBufferInfo.addr)), "Viewer appearance switch leaves source image bytes unchanged");
            Require(view->mSelRegions.size()==regionsBefore.size()+1 && view->mSelRegions.back().mSelRect==appearanceRoi.mSelRect,
                "Viewer appearance switch preserves source-space selection rectangle");
            Require(captureBytes.empty() || (view->mCaptRgbBuf==captureBuffer && view->mCaptRgbBufSize==captureSize &&
                std::equal(captureBytes.begin(),captureBytes.end(),captureBuffer)), "Viewer appearance switch leaves capture buffer and pixels unchanged");
            if (view->mRgbBuf && view->mWDst<view->mWCanvas && view->mHDst<view->mHCanvas) {
                const COLORREF background = Q1UI_COLOR_CANVAS_BG;
                auto repainted=[&] { return view->mRgbBuf && view->mRgbBuf[0]==GetBValue(background) &&
                    view->mRgbBuf[1]==GetGValue(background) && view->mRgbBuf[2]==GetRValue(background); };
                if(!repainted()) fprintf(report,"THEME pending canvas paint=%d actual=%u/%u/%u expected=%u/%u/%u\n",
                    GetUpdateRect(view->m_hWnd,nullptr,FALSE),view->mRgbBuf[0],view->mRgbBuf[1],view->mRgbBuf[2],
                    GetBValue(background),GetGValue(background),GetRValue(background));
                // Theme changes invalidate asynchronously; wait for the actual
                // paint, without forcing one or weakening its pixel check.
                Await(repainted,"actual scaled Viewer canvas repaints exposed pixels with matching theme background");
            }
            Require(pane.mGen==generation && pane.mEntries.data()==entries && pane.SelectedMediaPaths()==selectedMedia && grid.mLayout.scroll==scroll,
                "appearance switch preserves folder entries, multi-selection and gallery scroll");
            for (const auto& image : cachedImages) {
                const auto found = grid.mCache.find(image.first);
                Require(found!=grid.mCache.end() && found->second.cpu==image.second.first && found->second.gpu.Get()==image.second.second,
                    "appearance switch retains existing CPU and GPU thumbnail handles");
            }
        }
        frame->SendMessage(WM_COMMAND,q1view::ID_UI_APPEARANCE_SYSTEM+UINT(theme));
        Pump(.08);
        view->mSelRegions = regionsBefore;
        for (int step : {2,4,3,5,2,1,4,1}) { pane.ApplyViewStep(step, false); Pump(.03); }
        Require(pane.mGen == generation && pane.mEntries.data() == entries, "repeated zoom preserves folder generation and entries");
        Require(grid.Selection() == 10, "selection survives interrupted zoom transitions");
        Require(doc->GetPathName() == original, "drawer browsing/zoom does not replace active document");
        Pump(.5);
        Require(!grid.mLayout.Animating(grid.Now()), "transition settles at latest layout");
        grid.Scroll(grid.mLayout.MaxScroll()); Pump(1);
        auto visible = grid.mLayout.Visible(grid.Now());
        Require(!visible.empty() && visible.back() == 3000, "last item visible after scroll to end");
        Await([&] { return grid.mCache.find(3000) != grid.mCache.end(); }, "end-of-folder thumbnail decoded");
        pane.ApplyViewStep(0, false); Pump(.2);
        Require(pane.GetItemCount() >= 3000, "compact list retained");
        pane.ApplyViewStep(1, false); Pump(.2);
        Require(grid.IsWindowVisible(), "list-to-grid switch restores canvas");
        MSG key = {}; key.hwnd = grid.GetSafeHwnd(); key.message = WM_KEYDOWN; key.wParam = VK_END;
        Require(grid.PreTranslateMessage(&key) && grid.Selection() == 3000, "End selects final grid item");
        key.wParam = VK_HOME; grid.PreTranslateMessage(&key);
        Require(grid.Selection() == 0 && grid.Label(0) == L"[..]", "Home selects the typographic parent-folder tile");
        key.wParam = VK_RIGHT; grid.PreTranslateMessage(&key);
        Require(grid.Selection() == 1, "arrow navigation moves from parent tile to first file");

		// A docked drawer must still change the final image viewport. Fit mode
		// recentres the whole image in that viewport and recomputes its scale once;
		// only the former sequence of intermediate resizes has been removed.
		view->FitToWindow();
		Require(view->IsFitToWindow(), "drawer geometry check starts in fit-to-window mode");
		const int openCanvasWidth = view->mWCanvas;
		const float openFitZoom = view->mN;
		const float expectedOpenFit = q1::GetFitRatio(1.0f, view->mW, view->mH,
			view->mWCanvas, view->mHCanvas);
		CRect openViewRect;
		view->GetWindowRect(&openViewRect);
		const int openImageCenterX = openViewRect.left + view->mXDst + view->mWDst / 2;
		Require(std::fabs(openFitZoom - expectedOpenFit) < 0.0001f &&
			view->mWDst <= view->mWCanvas && view->mHDst <= view->mHCanvas,
			"fit mode keeps small images at 100 percent and fits larger images with the drawer open");

        key.wParam = 'E';
        Require(grid.PreTranslateMessage(&key), "grid forwards Viewer accelerators to the frame");
		Require(!frame->mDrawerVisible, "E closes the drawer in one stable layout pass while the grid has focus");
		CRect closedViewRect;
		view->GetWindowRect(&closedViewRect);
		const int closedImageCenterX = closedViewRect.left + view->mXDst + view->mWDst / 2;
		const float expectedClosedFit = q1::GetFitRatio(1.0f, view->mW, view->mH,
			view->mWCanvas, view->mHCanvas);
		Require(view->IsFitToWindow() && view->mWCanvas > openCanvasWidth &&
			std::fabs(view->mN - expectedClosedFit) < 0.0001f &&
			view->mN + 0.0001f >= openFitZoom && view->mWDst <= view->mWCanvas &&
			view->mHDst <= view->mHCanvas && closedImageCenterX < openImageCenterX,
			"closing the docked drawer expands, refits, and recentres the image viewport");

		key.lParam = 1L << 30;
		Require(frame->TranslateGlobalAccelerator(&key) && !frame->mDrawerVisible,
			"keyboard auto-repeat does not retrigger the drawer toggle");
		key.lParam = 0;
        Require(frame->TranslateGlobalAccelerator(&key), "frame accelerator can restore drawer focus path");
		CRect reopenedViewRect;
		view->GetWindowRect(&reopenedViewRect);
		const int reopenedImageCenterX = reopenedViewRect.left + view->mXDst + view->mWDst / 2;
		Require(frame->mDrawerVisible && view->mWCanvas == openCanvasWidth &&
			std::fabs(view->mN - openFitZoom) < 0.0001f &&
			reopenedImageCenterX == openImageCenterX,
			"reopening restores the fitted drawer viewport without intermediate layouts");

		// Exercise the same application message-pump path as a physical E press.
		// Direct PreTranslateMessage calls do not cover focus moving from the drawer
		// to the image (or back) while the key's remaining messages are still queued.
		const CString documentBeforePhysicalE = doc->GetPathName();
		auto PostPhysicalE = [&]() {
			const bool wasVisible = frame->mDrawerVisible;
			HWND target = ::GetFocus();
			Require(target != NULL, "physical E regression has a focused target window");
			Require(::PostMessage(target, WM_KEYDOWN, 'E', 1) != FALSE,
				"physical E keydown posted through the Windows message queue");
			Require(::PostMessage(target, WM_KEYUP, 'E',
				1 | (1L << 30) | (1L << 31)) != FALSE,
				"physical E keyup posted through the Windows message queue");
			// A pending metadata commit/thumbnail presentation can consume a
			// fixed 200 ms pump before the posted keyboard message is dequeued.
			// Wait for that message's actual effect, not for an arbitrary delay.
			Await([&] { return frame->mDrawerVisible != wasVisible; }, "physical E reaches the frame through the real message queue");
			Pump(.05);
		};
		PostPhysicalE();
		Require(!frame->mDrawerVisible && doc->GetPathName() == documentBeforePhysicalE,
			"physical E closes the focused drawer without opening another image");
		PostPhysicalE();
		Require(frame->mDrawerVisible && doc->GetPathName() == documentBeforePhysicalE,
			"physical E opens the drawer without opening another image");

		// Manual zoom is inspection state rather than automatic layout. Preserve the
		// exact on-screen image origin and zoom as the docked viewport changes.
		CPoint zoomPoint = reopenedViewRect.CenterPoint();
		view->ChangeZoom(WHEEL_DELTA, zoomPoint);
		const float manualZoom = view->mN;
		view->GetWindowRect(&reopenedViewRect);
		const CPoint manualOrigin(reopenedViewRect.left + view->mXDst,
			reopenedViewRect.top + view->mYDst);
		frame->OnToggleDrawer();
		CRect manualClosedRect;
		view->GetWindowRect(&manualClosedRect);
		Require(!view->IsFitToWindow() && std::fabs(view->mN - manualZoom) < 0.0001f &&
			CPoint(manualClosedRect.left + view->mXDst,
				manualClosedRect.top + view->mYDst) == manualOrigin,
			"closing the drawer preserves manual zoom and the inspected screen position");
		frame->OnToggleDrawer();
		CRect manualReopenedRect;
		view->GetWindowRect(&manualReopenedRect);
		Require(!view->IsFitToWindow() && std::fabs(view->mN - manualZoom) < 0.0001f &&
			CPoint(manualReopenedRect.left + view->mXDst,
				manualReopenedRect.top + view->mYDst) == manualOrigin,
			"reopening the drawer preserves manual zoom and the inspected screen position");
		view->FitToWindow();
		Pump(.2);

        CString driveRoot = folder.Left(3);
        pane.NavigateTo(driveRoot); Pump(.2);
        Require(!pane.mEntries.empty() && pane.mEntries.front().kind == CThumbnailPane::ENTRY_PARENT &&
            grid.Label(0) == L"[..]", "drive roots show a visible parent tile");
        pane.ActivateIndex(0, true); Pump(.2);
        Require(pane.mFolder.IsEmpty() && !pane.mEntries.empty() &&
            pane.mEntries.front().kind == CThumbnailPane::ENTRY_DIR,
            "activating the drive-root parent tile opens the drive chooser");
        pane.NavigateTo(driveRoot); Pump(.2);
        Require(pane.GoToParent(), "drive-root parent request accepted");
        Await([&] { return !pane.mNavigationBusy; }, "drive-root parent request completes");
        Require(pane.mFolder.IsEmpty() && !pane.mEntries.empty() &&
            pane.mEntries.front().kind == CThumbnailPane::ENTRY_DIR,
            "drive chooser opens after navigating above a drive root");
        pane.NavigateTo(folder); Pump(.5);

        // Fill with large thumbnails to force eviction and verify ownership.
        pane.mThumb = 16;
        for (int i=0;i<600;++i) grid.Accept(i,pane.MakePlaceholder(L""),16);
        Require(grid.mCache.size() <= grid.CacheEntryLimit,"tiny-thumbnail cache also bounds GDI handle count");
        pane.mThumb = 1024;
        for (int i = 0; i < 30; ++i) {
            HBITMAP bitmap = pane.MakePlaceholder(L"TEST");
            Require(bitmap != nullptr, "cache fixture allocated");
            grid.Accept(i, bitmap, 1024);
        }
        Require(grid.mCacheBytes <= grid.CacheBudget && grid.mCache.size() <= 16, "CPU cache evicts to 64 MiB; GPU entries share lifetime");
        grid.Select(0, true); grid.Relayout(false); Pump(.3);
        grid.DropDevice(); grid.Invalidate(FALSE); Pump(.3);
        Require(grid.mDevice && grid.mTarget && !grid.mCache.empty(), "GPU resource recreation retains CPU thumbnails");
        SetEnvironmentVariableW(L"Q1VIEW_DISABLE_GALLERY_GPU",L"1");
        grid.DropDevice(); grid.Invalidate(FALSE); Pump(.1);
        Require(!grid.mDevice && !grid.mCache.empty(), "GDI fallback retains grid thumbnails");
        SetEnvironmentVariableW(L"Q1VIEW_DISABLE_GALLERY_GPU",nullptr);
        grid.mRetryAt = 0; grid.Invalidate(FALSE); Pump(.2);
        Require(grid.mDevice != nullptr, "GPU rendering recovers after fallback");
        CString missing = folder + L"missing\\";
        const auto beforeMissing = pane.mGen.load();
        const auto navigationReporter = pane.mReportNavigation;
        int missingErrors = 0;
        pane.mReportNavigation = [&](const CString&) { ++missingErrors; };
        pane.NavigateTo(missing); Pump(.1);
        Await([&] { return !pane.mNavigationBusy; }, "missing-folder result delivered");
        Require(pane.mGen == beforeMissing && pane.mEntries.size() == 3001 && missingErrors == 1, "missing folder leaves navigation and cache intact with one error");
        pane.NavigateTo(folder); pane.NavigateTo(missing); pane.NavigateTo(folder); Pump(1);
        pane.mReportNavigation = navigationReporter;
        Require(pane.mOutstanding <= 4, "decode plus posted-result backlog bounded to four");
        CRect beforeOpen; frame->GetWindowRect(&beforeOpen);
        CString expected = pane.mEntries[3].path;
        pane.ActivateIndex(3,false); Pump(.3);
        CRect afterOpen; frame->GetWindowRect(&afterOpen);
        Require(doc->GetPathName() == expected && beforeOpen == afterOpen, "deferred grid activation loads the selected file without resizing the outer window");

        Require(q1view::ParentDirectory(L"C:\\").empty() &&
            q1view::ParentDirectory(L"\\\\server\\share\\").empty(), "drive and UNC share roots have no parent");
        Require(q1view::ParentDirectory(L"\\\\server\\share\\child\\") == L"\\\\server\\share\\",
            "UNC parent stops at share boundary");
        Require(q1view::ParentDirectory(L"\\\\?\\UNC\\server\\share\\").empty() &&
            q1view::ParentDirectory(L"\\\\?\\C:\\child\\") == L"\\\\?\\C:\\", "extended-length paths respect roots");
        const CString unicodeFile = folder + L"\xC0AC\xC9C4 name.png";
        Require(CopyFileW(fixture, unicodeFile, FALSE) != FALSE, "Unicode clipboard fixture created");
        {
            Microsoft::WRL::ComPtr<IDataObject> previousClipboard;
            OleGetClipboard(&previousClipboard);
            struct RestoreClipboard {
                IDataObject* previous;
                ~RestoreClipboard() { OleSetClipboard(previous); OleFlushClipboard(); }
            } restore{previousClipboard.Get()};
            Require(q1view::ClipboardFile(frame->m_hWnd, unicodeFile.GetString()), "file copy writes native CF_HDROP");
            Require(OpenClipboard(frame->m_hWnd) != FALSE, "read file clipboard");
            HDROP drop = static_cast<HDROP>(GetClipboardData(CF_HDROP));
            wchar_t copied[32768] = {};
            const UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
            DragQueryFileW(drop, 0, copied, _countof(copied));
            CloseClipboard();
            Require(count == 1 && unicodeFile == copied, "Explorer-compatible clipboard retains exact Unicode file path");
            // Consume the actual clipboard IDataObject with the Windows Shell
            // copy engine, as an Explorer paste does. Only temporary fixtures
            // are written, and the user's original clipboard is restored.
            const CString pasteFolder = folder + L"paste-target\\";
            Require(CreateDirectoryW(pasteFolder, nullptr) != FALSE, "temporary paste destination created");
            Microsoft::WRL::ComPtr<IDataObject> fileClipboard;
            Microsoft::WRL::ComPtr<IShellItem> destination;
            Microsoft::WRL::ComPtr<IFileOperation> operation;
            Require(SUCCEEDED(OleGetClipboard(&fileClipboard)) &&
                SUCCEEDED(SHCreateItemFromParsingName(pasteFolder, nullptr, IID_PPV_ARGS(&destination))) &&
                SUCCEEDED(CoCreateInstance(__uuidof(FileOperation), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&operation))),
                "Windows Shell accepts file clipboard and paste target");
            operation->SetOperationFlags(FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMATION | FOF_NOCONFIRMMKDIR);
            Require(SUCCEEDED(operation->CopyItems(fileClipboard.Get(), destination.Get())) &&
                SUCCEEDED(operation->PerformOperations()), "native Shell paste completes");
            BOOL aborted = FALSE;
            Require(SUCCEEDED(operation->GetAnyOperationsAborted(&aborted)) && !aborted,
                "native Shell paste is not aborted");
            Require(GetFileAttributesW(pasteFolder + L"\xC0AC\xC9C4 name.png") != INVALID_FILE_ATTRIBUTES &&
                GetFileAttributesW(unicodeFile) != INVALID_FILE_ATTRIBUTES, "paste copies Unicode file and preserves original");
            Require(q1view::ClipboardText(frame->m_hWnd, unicodeFile.GetString()), "copy path writes Unicode text");
            OpenClipboard(frame->m_hWnd);
            HANDLE text = GetClipboardData(CF_UNICODETEXT);
            const wchar_t* contents = static_cast<const wchar_t*>(GlobalLock(text));
            const bool matches = contents && unicodeFile == contents;
            if (contents) GlobalUnlock(text);
            CloseClipboard();
            Require(matches, "copied path text is exact");
            Require(!q1view::ClipboardFile(frame->m_hWnd, missing.GetString()) &&
                !q1view::ShowInExplorer(missing.GetString()) &&
                !q1view::ShowFileProperties(frame->m_hWnd, missing.GetString()), "missing shell items fail gracefully");
        }

        // Actual application playback, not an isolated decoder benchmark.
        wchar_t video[32768];
        bool suppliedVideo = GetEnvironmentVariableW(L"Q1VIEW_GALLERY_TEST_VIDEO", video, _countof(video)) != 0;
        if (!suppliedVideo) {
            swprintf_s(video,L"%sQ1View-gallery-playback-%lu.avi",temp,GetCurrentProcessId());
            cv::VideoWriter writer(std::string(CW2A(video,CP_UTF8)), cv::VideoWriter::fourcc('M','J','P','G'),60000.0/1001,cv::Size(640,360));
            Require(writer.isOpened(),"synthetic 59.94fps video writer opened");
            for (int i=0;i<1800;++i) {
                cv::Mat rgb(360,640,CV_8UC3,cv::Scalar(i%256,(i*3)%256,(i*7)%256)); writer.write(rgb);
            }
            writer.release();
            fprintf(report,"Using synthetic 640x360 MJPEG video; this does not replace 4K long-GOP validation.\n");
        }
        {
            Require(AfxGetApp()->OpenDocumentFile(video) != nullptr, "representative video opened");
            view = static_cast<CViewerView*>(frame->GetActiveView());
            doc = static_cast<CViewerDoc*>(frame->GetActiveDocument());
			if (GetEnvironmentVariableW(L"Q1VIEW_GALLERY_TEST_EXPECT_HLG_TONE_MAP",
				nullptr, 0))
				Require(doc->mFrmSrc->usesHlgToneMapping(), "BT.2020 HLG metadata enables SDR tone mapping");
			wchar_t dumpFrame[32768] = {};
			if (GetEnvironmentVariableW(L"Q1VIEW_GALLERY_TEST_DUMP_FRAME", dumpFrame,
				_countof(dumpFrame))) {
				doc->mAutoplayAfterPresent = false;
				Pump(.2);
				Await([&] { return view->mStableRgbBufferInfo.ID == 0; },
					"initial video frame presented before deterministic dump");
				wchar_t dumpFrameIdText[32] = {};
				const long dumpFrameId = GetEnvironmentVariableW(
					L"Q1VIEW_GALLERY_TEST_DUMP_FRAME_ID", dumpFrameIdText,
					_countof(dumpFrameIdText)) ? _wtol(dumpFrameIdText) : 0;
				if (dumpFrameId != 0) {
					Require(doc->SeekScene(dumpFrameId) == dumpFrameId,
						"deterministic dump frame queued");
					view->Invalidate(FALSE);
					Pump(.2);
					Await([&] { return view->mStableRgbBufferInfo.ID == dumpFrameId; },
						"deterministic dump frame presented");
				}
				cv::Mat displayed(doc->mH, doc->mW, CV_8UC3,
					view->mStableRgbBufferInfo.addr,
					ROUNDUP_DWORD(doc->mW) * QIMG_DST_RGB_BYTES);
				Require(q1::imwriteW(dumpFrame, displayed), "displayed video frame dumped");
				if (dumpFrameId != 0) {
					Require(doc->SeekScene(0) == 0, "playback restart frame queued after dump");
					view->Invalidate(FALSE);
					Pump(.2);
					Await([&] { return view->mStableRgbBufferInfo.ID == 0; },
						"playback restart frame presented after dump");
				}
				view->SetPlayTimer(doc);
			}
			wchar_t expectedRotationText[16] = {};
			if (GetEnvironmentVariableW(L"Q1VIEW_GALLERY_TEST_EXPECT_ROTATION", expectedRotationText,
				_countof(expectedRotationText))) {
				const int expectedRotation = _wtoi(expectedRotationText);
				Require(int(doc->mRot) * 90 == expectedRotation,
					"video display matrix initializes document rotation");
				const bool swapsAxes = expectedRotation == 90 || expectedRotation == 270;
				Require((!swapsAxes && doc->mW == doc->mOrigW && doc->mH == doc->mOrigH) ||
					(swapsAxes && doc->mW == doc->mOrigH && doc->mH == doc->mOrigW),
					"display dimensions follow video orientation");
			}
            pane.NavigateTo(folder); Pump(1);
            Require(view->mIsPlaying, "video is playing");
            std::vector<double> gaps;
            double lastFrame = grid.Now();
            int lastCount = view->mPlayFrameCount;
            afterMessage = [&] {
                if (view->mPlayFrameCount != lastCount) {
                    double now = grid.Now(); gaps.push_back((now-lastFrame)*1000);
                    lastFrame = now; lastCount = view->mPlayFrameCount;
                }
            };
            auto summarizeGaps = [&](const char* label) {
                if (!gaps.empty()) {
                    std::sort(gaps.begin(),gaps.end());
                    fprintf(report,"%s frame intervals: p95 %.3f ms, p99 %.3f ms, max %.3f ms\n",label,
                        gaps[size_t((gaps.size()-1)*.95)],gaps[size_t((gaps.size()-1)*.99)],gaps.back());
                }
                gaps.clear(); lastFrame = grid.Now();
            };
            // Leave enough media after the performance samples for the drawer
            // and divider checks when a short representative clip is supplied.
            const double duration = doc->mFps > 0 ? double(doc->mFrames) / doc->mFps : 0;
            const double sampleSeconds = suppliedVideo && duration > 0 && duration < 15 ? 3.0 : 5.0;
            long start = doc->mCurFrameID; int paints = view->mPlayFrameCount;
            auto t = grid.Now(); Pump(sampleSeconds); double elapsed = grid.Now()-t;
            double baseline = (view->mPlayFrameCount-paints)/elapsed;
            Require(baseline > 1, "presentation timing counters are active");
            fprintf(report,"baseline: %.3f presented fps, frame %ld -> %ld\n", baseline,start,doc->mCurFrameID);
            Require(doc->mCurFrameID > start, "baseline playback advances");
            summarizeGaps("baseline");
            paints = view->mPlayFrameCount; start = doc->mCurFrameID; t = grid.Now();
            double next = t; int step = 0; double longest = 0, previous = t;
            Pump(sampleSeconds,[&] {
                double now = grid.Now(); longest = std::max(longest,now-previous); previous = now;
                if (now >= next) { pane.ApplyViewStep(step++%5+1,false); next = now+.08; }
            });
            elapsed = grid.Now()-t; double zoomFps=(view->mPlayFrameCount-paints)/elapsed;
            fprintf(report,"zoom: %.3f presented fps, frame %ld -> %ld, max pump gap %.3f ms\n",zoomFps,start,doc->mCurFrameID,longest*1000);
            Require(view->mIsPlaying && doc->mCurFrameID > start, "playback continues through rapid grid zoom");
            Require(zoomFps >= baseline*.90, "grid zoom presentation rate within 10 percent of baseline");
            summarizeGaps("zoom");
            afterMessage = {};
			const long pausedFrame = view->mStableRgbBufferInfo.ID;
			BYTE* pausedBuffer = view->mStableRgbBufferInfo.addr;
			view->KillPlayTimerSafe();
			Pump(.2);
			Require(!view->mIsPlaying && view->mStableRgbBufferInfo.ID == pausedFrame &&
				view->mStableRgbBufferInfo.addr == pausedBuffer,
				"pause preserves the exact last-presented RGB buffer");
			const long steppedFrame = pausedFrame + 1;
			Require(doc->NextScene() == steppedFrame, "paused frame step queues the following frame");
			view->Invalidate(FALSE);
			Await([&] { return view->mStableRgbBufferInfo.ID == steppedFrame; },
				"paused frame step presents the requested frame");
			Require(!view->mIsPlaying, "frame stepping does not resume playback");
			view->SetPlayTimer(doc);
			Pump(.4);
			Require(view->mIsPlaying && view->mStableRgbBufferInfo.ID > steppedFrame,
				"playback resumes from the decoder's following frame");
            CRect bounds; frame->GetWindowRect(&bounds);
            for (int i=0;i<6;++i) { frame->OnToggleDrawer(); Pump(.2); }
            CRect after; frame->GetWindowRect(&after);
            Require(bounds == after && view->mIsPlaying, "drawer toggles preserve outer window and playback");
            pane.SetResizing(true);
            pane.MoveWindow(0,0,350,750); pane.MoveWindow(0,0,600,750); pane.SetResizing(false); Pump(.3);
            Require(grid.mLayout.width > 500 && view->mIsPlaying, "grid refits after divider resizing without stopping playback");
            frame->RecalcLayout();
			wchar_t replacementVideo[MAX_PATH] = {};
			if (GetEnvironmentVariableW(L"Q1VIEW_GALLERY_TEST_REPLACEMENT_VIDEO",
				replacementVideo, _countof(replacementVideo))) {
				// A single-file shell drop reaches CViewerApp::OpenDocumentFile and
				// reuses this SDI document. Alternate active videos repeatedly so the
				// test exercises decoder teardown while VideoCapture::read is busy.
				for (int replacement = 0; replacement < 12; ++replacement) {
					const wchar_t* target = (replacement & 1) ? video : replacementVideo;
					Require(view->mIsPlaying, "video is active before document replacement");
					Require(AfxGetApp()->OpenDocumentFile(target) != nullptr,
						"playing document replaced through the shell-open path");
					view = static_cast<CViewerView*>(frame->GetActiveView());
					doc = static_cast<CViewerDoc*>(frame->GetActiveDocument());
					Await([&] { return view->mIsPlaying && doc->mCurFrameID > 0; },
						"replacement video presents and resumes playback");
				}
			}
			view->KillPlayTimerSafe();
			const long eofStart = std::max(0L, doc->mFrames - 2);
			Require(doc->SeekScene(eofStart) == eofStart, "near-EOF frame queued");
			view->Invalidate(FALSE);
			Await([&] { return view->mStableRgbBufferInfo.ID == eofStart; },
				"near-EOF frame presented");
			view->SetPlayTimer(doc);
			Await([&] { return !view->mIsPlaying; }, "playback stops cleanly at EOF");
			Require(view->mStableRgbBufferInfo.ID == doc->mFrames - 1,
				"last valid frame remains presented at EOF");

            Require(doc->SeekScene(0) == 0, "restart frame queued for drawer navigation validation");
            view->Invalidate(FALSE);
            Await([&] { return view->mStableRgbBufferInfo.ID == 0; }, "restart frame presented");
            view->SetPlayTimer(doc);
            Await([&] { return view->mIsPlaying; }, "video active for folder-only navigation");
            const CString child = folder + L"\xD558\xC704 folder\\";
            Require(CreateDirectoryW(child, nullptr) != FALSE, "Unicode child folder created");
            const CString nonMedia = folder + L"notes.txt";
            HANDLE note = CreateFileW(nonMedia, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            Require(note != INVALID_HANDLE_VALUE, "non-media filtering fixture created");
            CloseHandle(note);
            const CString rawInput = folder + L"frame.yuv";
            HANDLE raw = CreateFileW(rawInput, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            Require(raw != INVALID_HANDLE_VALUE, "raw-input filtering fixture created");
            CloseHandle(raw);
            const CString archive = folder + L"archive.zip";
            HANDLE zip = CreateFileW(archive, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            Require(zip != INVALID_HANDLE_VALUE, "archive filtering fixture created");
            CloseHandle(zip);
            const CString brokenImage = folder + L"broken.png";
            HANDLE badImage = CreateFileW(brokenImage, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            Require(badImage != INVALID_HANDLE_VALUE, "broken-image fixture created");
            CloseHandle(badImage);
            const CString brokenVideo = folder + L"broken.mp4";
            HANDLE badVideo = CreateFileW(brokenVideo, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            Require(badVideo != INVALID_HANDLE_VALUE, "failed-video-preview fixture created");
            CloseHandle(badVideo);
            const CString previewVideo = folder + L"preview.avi";
            cv::VideoWriter previewWriter(std::string(CW2A(previewVideo, CP_UTF8)),
                cv::VideoWriter::fourcc('M','J','P','G'), 24, cv::Size(64, 64));
            Require(previewWriter.isOpened(), "video-thumbnail fixture created");
            for (int i = 0; i < 3; ++i)
                previewWriter.write(cv::Mat(64, 64, CV_8UC3, cv::Scalar(30 + i * 20, 90, 180)));
            previewWriter.release();
            CRect playbackBounds; frame->GetWindowRect(&playbackBounds);
            const float zoom = view->mN, xOffset = view->mXOff, yOffset = view->mYOff;
            const CString media = doc->mPathName;
            const UINT openGeneration = doc->mOpenGeneration;
            const auto playbackTimer = view->mTimerID;
            for (UINT command : {q1view::ID_UI_APPEARANCE_DARK,q1view::ID_UI_APPEARANCE_LIGHT,q1view::ID_UI_APPEARANCE_SYSTEM}) {
                frame->SendMessage(WM_COMMAND,command);
                Require(view->mIsPlaying && view->mTimerID==playbackTimer && doc->mOpenGeneration==openGeneration && doc->mPathName==media,
                    "theme switch during playback retains timer, decoder and media without reopening");
            }
            for (int mode = 0; mode < pane.ViewStepCount(); ++mode) {
                pane.NavigateTo(folder); pane.ApplyViewStep(mode, false);
                Await([&] { return !pane.mNavigationBusy && pane.mFolder == folder; }, "filtering checks wait for the requested directory snapshot");
                const auto containsPath = [&](const CString& path) {
                    return std::any_of(pane.mEntries.begin(), pane.mEntries.end(),
                        [&](const CThumbnailPane::Entry& entry) { return entry.path.CompareNoCase(path) == 0; });
                };
                Require(containsPath(nonMedia) == (mode == 0) && containsPath(rawInput) == (mode == 0) &&
                    containsPath(archive) == (mode == 0),
                    "documents, raw inputs, and archives remain in compact navigation but not thumbnail grids");
                Require(containsPath(brokenImage), "supported broken image remains discoverable");
                if (mode == 0) {
                    Require(containsPath(brokenVideo) && containsPath(previewVideo),
                        "compact navigation retains videos regardless of preview availability");
                } else {
                    Await([&] { return !containsPath(brokenVideo); },
                        "video without a meaningful preview leaves the thumbnail grid");
                    Await([&] {
                        for (int i = 0; i < (int)pane.mEntries.size(); ++i) {
                            const CThumbnailPane::Entry& entry = pane.mEntries[i];
                            if (entry.path.CompareNoCase(brokenImage) != 0) continue;
                            if (entry.badge) return true;
                            grid.Select(i, true);
                            grid.QueueVisible();
                            return false;
                        }
                        return false;
                    }, "broken image remains with preview-unavailable treatment");
                    Await([&] {
                        for (int i = 0; i < (int)pane.mEntries.size(); ++i) {
                            if (pane.mEntries[i].path.CompareNoCase(previewVideo) != 0) continue;
                            if (grid.mCache.find(i) != grid.mCache.end()) return true;
                            grid.Select(i, true);
                            grid.QueueVisible();
                            return false;
                        }
                        return false;
                    }, "video with a meaningful thumbnail remains in the grid");
                }
                pane.NavigateTo(child); pane.ApplyViewStep(mode, false);
                Await([&] { return !pane.mNavigationBusy && pane.mFolder == child; }, "child folder opens before testing its parent tile");
                Require(!pane.mEntries.empty() && pane.mEntries.front().kind == CThumbnailPane::ENTRY_PARENT &&
                    (pane.IsGrid() ? grid.Label(0) == L"[..]" : pane.GetItemText(0, 0) == L"[..]"),
                    "typographic parent entry is first at every thumbnail size");
                CMenu parentMenu; pane.BuildContextMenu(parentMenu, 0);
                Require(parentMenu.GetMenuItemCount() == 0, "parent navigation is represented by the tile, not a context command");
                pane.ActivateIndex(0, true);
                Await([&] { return !pane.mNavigationBusy && pane.mFolder == folder; }, "parent tile completes its asynchronous navigation");
                const int selected = pane.IsGrid() ? grid.Selection() : pane.GetNextItem(-1, LVNI_SELECTED);
                Require(pane.mFolder == folder && pane.mViewStep == mode && selected >= 0 &&
                    pane.mEntries[selected].path == child, "parent tile reveals child folder at every thumbnail size");
                Require((pane.IsGrid() ? grid.Label(selected) : pane.GetItemText(selected, 0)) == L"[\xD558\xC704 folder]",
                    "Unicode child directory uses bracket notation without decorative artwork");
                if (pane.IsGrid())
                    Require(grid.Tooltip(selected) == L"\xD558\xC704 folder",
                        "grid folder tooltip preserves the original unadorned name");
                CMenu folderMenu; pane.BuildContextMenu(folderMenu, selected);
                Require(folderMenu.GetMenuState(CThumbnailPane::CMD_COPY, MF_BYCOMMAND) != UINT(-1), "folder menu provides real file copy");
                pane.ActivateIndex(selected, true);
                // Model the same-folder rebuild caused when an asynchronous
                // video preview is rejected between the click and its posted
                // activation message. The captured folder action must survive.
                pane.Populate(pane.mFolder, child);
                // Activation is posted, not synchronous. Playback/decode work
                // may consume a fixed 50 ms pump before that message is reached.
                // Await the actual outcome, retaining the bounded failure timeout.
                Await([&] { return pane.mFolder == child; }, "folder activation survives a same-folder asynchronous refresh");
                MSG up = {}; up.hwnd = pane.GetSafeHwnd(); up.message = WM_KEYDOWN; up.wParam = VK_BACK;
                Require(pane.PreTranslateMessage(&up), "Backspace remains a parent-navigation shortcut");
                Await([&] { return !pane.mNavigationBusy && pane.mFolder == folder; }, "Backspace navigation completes in the parent folder");
                CMenu background; pane.BuildContextMenu(background, -1);
                Require(background.GetMenuItemCount() == 0, "empty background has no redundant parent context command");
                Require(view->mIsPlaying && doc->mPathName == media && doc->mOpenGeneration == openGeneration,
                    "folder navigation never opens, seeks, or restarts active media");
            }
            CRect finalBounds; frame->GetWindowRect(&finalBounds);
            Require(finalBounds == playbackBounds && view->mN == zoom && view->mXOff == xOffset && view->mYOff == yOffset,
                "folder navigation preserves window geometry, zoom and focal point");
            pane.ApplyViewStep(0, false); pane.SelectByPath(unicodeFile);
            const int fileIndex = pane.GetNextItem(-1, LVNI_SELECTED);
            CMenu fileMenu; pane.BuildContextMenu(fileMenu, fileIndex);
            Require(fileMenu.GetMenuState(CThumbnailPane::CMD_PROPERTIES, MF_BYCOMMAND) != UINT(-1), "file menu includes native properties");
            pane.ActivateIndex(fileIndex, true); pane.NavigateTo(child); Pump(.05);
            Require(doc->mPathName == media, "stale deferred activation cannot open an item after folder change");
            view->KillPlayTimerSafe();
        }
        BrowserNavigationTests(frame, fixture);
        RecycleTests(frame, fixture);
        wchar_t hold[16];
        if (GetEnvironmentVariableW(L"Q1VIEW_GALLERY_TEST_HOLD_SECONDS",hold,_countof(hold))) {
            fprintf(report,"Holding test window for visual inspection\n"); fflush(report);
            Pump(std::min(60, _wtoi(hold)));
        }
        fflush(report);
    }
};

int RunGalleryIntegrationTests() {
    wchar_t path[32768];
    if (!GetEnvironmentVariableW(L"Q1VIEW_GALLERY_TEST_REPORT",path,_countof(path))) return 2;
    GalleryIntegrationTests tests;
    if (_wfopen_s(&tests.report,path,L"w") != 0) return 2;
    int result = 0;
    try { tests.Run(); fprintf(tests.report,"ALL INTEGRATION CHECKS PASSED\n"); }
    catch (const std::exception& error) { fprintf(tests.report,"FAILED: %s\n",error.what()); result = 1; }
    fclose(tests.report); return result;
}
