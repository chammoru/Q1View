// Built only with /p:Q1ViewGalleryTests=true. Exercises the real MFC pane,
// worker queue and D3D11/Direct2D renderer, using isolated app preferences.
#include "../Viewer/stdafx.h"
#include "../Viewer/Viewer.h"
#include "../Viewer/MainFrm.h"
#include "../Viewer/ViewerDoc.h"
#include "../Viewer/ViewerView.h"
#include "../Viewer/FrmSrc.h"
#include "../Viewer/ThumbnailPane.h"
#include "../Viewer/GalleryGridCanvas.h"
#include "QViewerCmn.h"
#include "QCvUtil.h"
#include "QFileActionsWin.h"
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
        Require(pane.mEntries.size() == 3001 && pane.mEntries.front().kind == CThumbnailPane::ENTRY_PARENT &&
            pane.GetItemCount() == 0, "large grid keeps one parent tile without list-control rows or image-list copies");
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
        Require(!visible.empty() && visible.back() == 3000, "last item visible after scroll to end");
        Await([&] { return grid.mCache.find(3000) != grid.mCache.end(); }, "end-of-folder thumbnail decoded");
        pane.ApplyViewStep(0, false); Pump(.2);
        Require(pane.GetItemCount() >= 3000, "compact list retained");
        pane.ApplyViewStep(1, false); Pump(.2);
        Require(grid.IsWindowVisible(), "list-to-grid switch restores canvas");
        MSG key = {}; key.hwnd = grid.GetSafeHwnd(); key.message = WM_KEYDOWN; key.wParam = VK_END;
        Require(grid.PreTranslateMessage(&key) && grid.Selection() == 3000, "End selects final grid item");
        key.wParam = VK_HOME; grid.PreTranslateMessage(&key);
        Require(grid.Selection() == 0 && grid.Label(0) == L"..", "Home selects the visible parent-folder tile");
        key.wParam = VK_RIGHT; grid.PreTranslateMessage(&key);
        Require(grid.Selection() == 1, "arrow navigation moves from parent tile to first file");

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
        pane.NavigateTo(missing); Pump(.1);
        Require(pane.mGen == beforeMissing && pane.mEntries.size() == 3001, "missing folder leaves navigation and cache intact");
        pane.NavigateTo(folder); pane.NavigateTo(missing); pane.NavigateTo(folder); Pump(1);
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
            CRect playbackBounds; frame->GetWindowRect(&playbackBounds);
            const float zoom = view->mN, xOffset = view->mXOff, yOffset = view->mYOff;
            const CString media = doc->mPathName;
            const UINT openGeneration = doc->mOpenGeneration;
            for (int mode = 0; mode < pane.ViewStepCount(); ++mode) {
                pane.NavigateTo(child); pane.ApplyViewStep(mode, false);
                Require(!pane.mEntries.empty() && pane.mEntries.front().kind == CThumbnailPane::ENTRY_PARENT &&
                    (!pane.IsGrid() || grid.Label(0) == L".."), "visible parent tile is first at every thumbnail size");
                CMenu parentMenu; pane.BuildContextMenu(parentMenu, 0);
                Require(parentMenu.GetMenuItemCount() == 0, "parent navigation is represented by the tile, not a context command");
                pane.ActivateIndex(0, true); Pump(.05);
                const int selected = pane.IsGrid() ? grid.Selection() : pane.GetNextItem(-1, LVNI_SELECTED);
                Require(pane.mFolder == folder && pane.mViewStep == mode && selected >= 0 &&
                    pane.mEntries[selected].path == child, "parent tile reveals child folder at every thumbnail size");
                CMenu folderMenu; pane.BuildContextMenu(folderMenu, selected);
                Require(folderMenu.GetMenuState(CThumbnailPane::CMD_COPY, MF_BYCOMMAND) != UINT(-1), "folder menu provides real file copy");
                pane.ActivateIndex(selected, true); Pump(.05);
                Require(pane.mFolder == child, "folder activation navigates without opening media");
                MSG up = {}; up.hwnd = pane.GetSafeHwnd(); up.message = WM_KEYDOWN; up.wParam = VK_BACK;
                Require(pane.PreTranslateMessage(&up) && pane.mFolder == folder, "Backspace remains a parent-navigation shortcut");
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
