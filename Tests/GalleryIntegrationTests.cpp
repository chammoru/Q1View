// Built only with /p:Q1ViewGalleryTests=true. Exercises the real MFC pane,
// worker queue and D3D11/Direct2D renderer, using isolated app preferences.
#include "../Viewer/stdafx.h"
#include "../Viewer/Viewer.h"
#include "../Viewer/MainFrm.h"
#include "../Viewer/ViewerDoc.h"
#include "../Viewer/ViewerView.h"
#include "../Viewer/ThumbnailPane.h"
#include "../Viewer/GalleryGridCanvas.h"
#include "QViewerCmn.h"
#include <cstdio>
#include <stdexcept>
#include <functional>
#include <opencv2/videoio.hpp>

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
    void Run() {
        auto frame = static_cast<CMainFrame*>(AfxGetMainWnd());
        auto view = static_cast<CViewerView*>(frame->GetActiveView());
        auto doc = static_cast<CViewerDoc*>(frame->GetActiveDocument());
        Require(frame && view && doc, "real Viewer document/view created");
        frame->ShowWindow(SW_SHOWNOACTIVATE);
        frame->MoveWindow(20, 20, 1200, 850);
        if (!frame->mDrawerVisible) frame->OnToggleDrawer();
        Pump(.4);
        auto& pane = *frame->mpDrawer;
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
        Require(pane.mEntries.size() == 3000 && pane.GetItemCount() == 0, "large grid does not create list-control rows or image-list copies");
        Require(grid.mDevice && grid.mContext && grid.mTarget, "actual Direct2D/D3D11 render target created");
        Require(!grid.mCache.empty(), "worker results populate CPU thumbnail cache");
        bool uploaded = false;
        for (auto& p : grid.mCache) if (p.second.gpu) uploaded = true;
        Require(uploaded, "visible thumbnails uploaded to GPU");
        unsigned generation = pane.mGen.load();
        auto entries = pane.mEntries.data();
        grid.Select(10, true);
        for (int step : {2,4,3,5,2,1,4,1}) { pane.ApplyViewStep(step, false); Pump(.03); }
        Require(pane.mGen == generation && pane.mEntries.data() == entries, "repeated zoom preserves folder generation and entries");
        Require(grid.Selection() == 10, "selection survives interrupted zoom transitions");
        Require(doc->GetPathName() == original, "drawer browsing/zoom does not replace active document");
        Pump(.5);
        Require(!grid.mLayout.Animating(grid.Now()), "transition settles at latest layout");
        grid.Scroll(grid.mLayout.MaxScroll()); Pump(1);
        auto visible = grid.mLayout.Visible(grid.Now());
        Require(!visible.empty() && visible.back() == 2999, "last item visible after scroll to end");
        Await([&] { return grid.mCache.find(2999) != grid.mCache.end(); }, "end-of-folder thumbnail decoded");
        pane.ApplyViewStep(0, false); Pump(.2);
        Require(pane.GetItemCount() >= 3000, "compact list retained");
        pane.ApplyViewStep(1, false); Pump(.2);
        Require(grid.IsWindowVisible(), "list-to-grid switch restores canvas");
        MSG key = {}; key.hwnd = grid.GetSafeHwnd(); key.message = WM_KEYDOWN; key.wParam = VK_END;
        Require(grid.PreTranslateMessage(&key) && grid.Selection() == 2999, "End selects final grid item");
        key.wParam = VK_HOME; grid.PreTranslateMessage(&key);
        key.wParam = VK_RIGHT; grid.PreTranslateMessage(&key);
        Require(grid.Selection() == 1, "Home and arrow selection work in grid");

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
        pane.NavigateTo(missing); Pump(.1);
        Require(pane.mEntries.empty() && grid.mCache.empty(), "folder switch discards stale entries and cached textures");
        pane.NavigateTo(folder); pane.NavigateTo(missing); pane.NavigateTo(folder); Pump(1);
        Require(pane.mOutstanding <= 4, "decode plus posted-result backlog bounded to four");
        CRect beforeOpen; frame->GetWindowRect(&beforeOpen);
        CString expected = pane.mEntries[3].path;
        pane.ActivateIndex(3,false); Pump(.3);
        CRect afterOpen; frame->GetWindowRect(&afterOpen);
        Require(doc->GetPathName() == expected && beforeOpen == afterOpen, "deferred grid activation loads the selected file without resizing the outer window");

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
            long start = doc->mCurFrameID; int paints = view->mPlayFrameCount;
            auto t = grid.Now(); Pump(5); double elapsed = grid.Now()-t;
            double baseline = (view->mPlayFrameCount-paints)/elapsed;
            Require(baseline > 1, "presentation timing counters are active");
            fprintf(report,"baseline: %.3f presented fps, frame %ld -> %ld\n", baseline,start,doc->mCurFrameID);
            Require(doc->mCurFrameID > start, "baseline playback advances");
            summarizeGaps("baseline");
            paints = view->mPlayFrameCount; start = doc->mCurFrameID; t = grid.Now();
            double next = t; int step = 0; double longest = 0, previous = t;
            Pump(5,[&] {
                double now = grid.Now(); longest = std::max(longest,now-previous); previous = now;
                if (now >= next) { pane.ApplyViewStep(step++%5+1,false); next = now+.08; }
            });
            elapsed = grid.Now()-t; double zoomFps=(view->mPlayFrameCount-paints)/elapsed;
            fprintf(report,"zoom: %.3f presented fps, frame %ld -> %ld, max pump gap %.3f ms\n",zoomFps,start,doc->mCurFrameID,longest*1000);
            Require(view->mIsPlaying && doc->mCurFrameID > start, "playback continues through rapid grid zoom");
            Require(zoomFps >= baseline*.90, "grid zoom presentation rate within 10 percent of baseline");
            summarizeGaps("zoom");
            afterMessage = {};
            CRect bounds; frame->GetWindowRect(&bounds);
            for (int i=0;i<6;++i) { frame->OnToggleDrawer(); Pump(.2); }
            CRect after; frame->GetWindowRect(&after);
            Require(bounds == after && view->mIsPlaying, "drawer toggles preserve outer window and playback");
            pane.SetResizing(true);
            pane.MoveWindow(0,0,350,750); pane.MoveWindow(0,0,600,750); pane.SetResizing(false); Pump(.3);
            Require(grid.mLayout.width > 500 && view->mIsPlaying, "grid refits after divider resizing without stopping playback");
            frame->RecalcLayout();
        }
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
