// /p:Q1ViewComparerTests=true: actual MFC document/views, isolated preferences.
#include "../Comparator/stdafx.h"
#include "../Comparator/Comparator.h"
#include "../Comparator/MainFrm.h"
#include "../Comparator/ComparatorDoc.h"
#include "../Comparator/ComparatorView.h"
#include "../Comparator/FrmCmpStrategy.h"
#include "../Comparator/PosInfoView.h"
#include <oleacc.h>
#pragma comment(lib, "oleacc.lib")
#include "Q1UiFontWin.h"
#include "Q1UiAppearanceMenuWin.h"
#include "SharedThemeTestWriter.h"
#include <cstdio>
#include <stdexcept>

namespace {
FILE* report;
constexpr int METRIC_SSIM_IDX = 1;
void Check(bool value, const char* message) {
	fprintf(report, "%s: %s\n", value ? "PASS" : "FAIL", message); fflush(report);
	if (!value) throw std::runtime_error(message);
}
void Pump(DWORD milliseconds) {
	const ULONGLONG end = GetTickCount64() + milliseconds;
	do {
		MSG message;
		while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
			if (message.message == WM_QUIT) throw std::runtime_error("unexpected quit");
			if (!AfxGetApp()->PreTranslateMessage(&message)) { TranslateMessage(&message); DispatchMessage(&message); }
		}
		Sleep(1);
	} while (GetTickCount64() < end);
}
std::vector<DWORD> Pixels(HWND window) {
	RECT bounds; GetClientRect(window, &bounds);
	Check(bounds.right > 0 && bounds.bottom > 0, "pixel sample has nonzero bounds");
	HDC source = GetDC(window), target = CreateCompatibleDC(source);
	BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = bounds.right; info.bmiHeader.biHeight = -bounds.bottom;
	info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr; HBITMAP bitmap = CreateDIBSection(target, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	Check(bitmap != nullptr, "pixel sample DIB created");
	HGDIOBJ previous = SelectObject(target, bitmap);
	const BOOL copied = BitBlt(target, 0, 0, bounds.right, bounds.bottom, source, 0, 0, SRCCOPY);
	GdiFlush();
	std::vector<DWORD> result(static_cast<DWORD*>(bits), static_cast<DWORD*>(bits) + size_t(bounds.right) * bounds.bottom);
	for (auto& pixel : result) pixel &= 0xffffff;
	SelectObject(target, previous); DeleteObject(bitmap); DeleteDC(target); ReleaseDC(window, source);
	Check(copied != FALSE, "presented window pixels read back");
	return result;
}

struct RawTimelineFixture {
	CString path;
	explicit RawTimelineFixture(int count) {
		wchar_t folder[MAX_PATH], name[MAX_PATH];
		Check(GetTempPathW(MAX_PATH,folder) && GetTempFileNameW(folder,L"Q1T",0,name), "temporary timeline fixture allocated");
		path = CString(name)+L"_16x16_yuv420.yuv";
		DeleteFileW(name);
		FILE* file = nullptr; Check(_wfopen_s(&file,path,L"wb")==0 && file, "RAW timeline fixture opened");
		BYTE pixels[16*16*3/2]; memset(pixels,128,sizeof(pixels));
		bool written = true;
		for (int i=0;i<count;++i) { pixels[0]=BYTE(i); written = written && fwrite(pixels,1,sizeof(pixels),file)==sizeof(pixels); }
		written = fclose(file)==0 && written; Check(written,"RAW timeline frames generated");
	}
	~RawTimelineFixture() { DeleteFileW(path); }
};

void CheckScrollbarAccessibility(CScrollBar* bar) {
	wchar_t name[32] = {}; GetClassNameW(bar->m_hWnd,name,_countof(name));
	Check(_wcsicmp(name,L"ScrollBar")==0, "themed scrollbar retains native SCROLLBAR window class");
	IAccessible* accessible = nullptr;
	Check(SUCCEEDED(AccessibleObjectFromWindow(bar->m_hWnd,OBJID_CLIENT,IID_IAccessible,reinterpret_cast<void**>(&accessible))) && accessible,
		"native scrollbar accessibility provider remains available");
	VARIANT self = {}, role = {}; self.vt=VT_I4; self.lVal=CHILDID_SELF;
	const HRESULT result = accessible->get_accRole(self,&role);
	const bool correct = SUCCEEDED(result) && role.vt==VT_I4 && role.lVal==ROLE_SYSTEM_SCROLLBAR;
	VariantClear(&role); accessible->Release(); Check(correct,"native accessible role remains ScrollBar");
}
}

int RunComparerTypographyTests() {
	wchar_t output[MAX_PATH] = {};
	GetEnvironmentVariableW(L"Q1VIEW_COMPARER_TEST_REPORT", output, _countof(output));
	if (!output[0]) wcscpy_s(output, L"Tests\\bin\\x64\\Release\\comparer-typography-report.txt");
	if (_wfopen_s(&report, output, L"w") || !report) return 2;
	try {
		auto* frame = static_cast<CMainFrame*>(AfxGetMainWnd());
		Check(frame != nullptr, "real Comparer frame created");
		auto* doc = static_cast<CComparatorDoc*>(frame->GetActiveDocument());
		Check(doc != nullptr, "real Comparer document created");
		// An isolated test profile may still restore a maximized startup state.
		// Normalize it before taking equal-size movement pixel baselines.
		frame->ShowWindow(SW_RESTORE);
		frame->MoveWindow(50, 50, 1100, 800); Pump(250);
		auto* timeline = doc->mPosInfoView;
		Check(timeline && timeline->GetScrollBarCtrl(SB_VERT) && !timeline->GetScrollBarCtrl(SB_VERT)->IsWindowVisible() &&
			timeline->GetScrollPosition().y==0, "empty Timeline has no scrollbar or phantom scroll offset");
		Check(true, "test frame initial layout completed");
		wchar_t first[MAX_PATH], second[MAX_PATH];
		GetFullPathNameW(L"docs\\images\\sample-colored-pencils-reference.png", MAX_PATH, first, nullptr);
		GetFullPathNameW(L"docs\\images\\sample-colored-pencils-changed-region.png", MAX_PATH, second, nullptr);
		std::vector<CString> files = {first, second};
		doc->OpenMultiFiles(files); Pump(500);
		// Opening a large source intentionally auto-maximizes the production
		// frame. Movement checks need a restored, fixed-size window instead.
		frame->ShowWindow(SW_RESTORE);
		frame->MoveWindow(50, 50, 1100, 800); Pump(250);
		Check(doc->mFrmCmpStrategy && doc->mPane[0].isAvail() && doc->mPane[1].isAvail(), "two real image sources and comparison strategy loaded");
		Check(!timeline->GetScrollBarCtrl(SB_VERT)->IsWindowVisible(), "still-image Timeline does not show an unnecessary scrollbar");
		doc->mHasSelection = true; doc->mSelStart = CPoint(2, 3); doc->mSelCur = CPoint(10, 12);
		doc->mN = 3; doc->mD = 1; doc->mXOff = 7; doc->mYOff = 9;
		int l, t, r, b; Check(doc->GetSelectionRect(l,t,r,b) && l==2 && t==3 && r==10 && b==12, "ROI uses committed source-pixel corners");
		CString psnr = doc->mFrmCmpStrategy->CropScore(doc->mPane, doc->mPane + 1, METRIC_PSNR_IDX, l,t,r,b);
		CString ssim = doc->mFrmCmpStrategy->CropScore(doc->mPane, doc->mPane + 1, METRIC_SSIM_IDX, l,t,r,b);
		Check(!psnr.IsEmpty() && !ssim.IsEmpty(), "real PSNR and SSIM ROI scores calculated");
		std::vector<BYTE> original(doc->mPane[0].rgbBuf, doc->mPane[0].rgbBuf + doc->mPane[0].rgbBufSize);
		const CString path = doc->mPane[0].pathName;
		const QIMAGE_CS color = doc->mPane[0].colorSpace;
		const float n = doc->mN, d = doc->mD, x = doc->mXOff, y = doc->mYOff;
		frame->SendMessage(q1view::WM_UI_TYPOGRAPHY_CHANGED); Pump(250);
		Check(doc->mN==n && doc->mD==d && doc->mXOff==x && doc->mYOff==y, "typography relayout preserves zoom and pan inputs");
		Check(doc->GetSelectionRect(l,t,r,b) && l==2 && t==3 && r==10 && b==12, "typography relayout preserves ROI");
		Check(doc->mPane[0].pathName==path && doc->mPane[0].colorSpace==color && original.size()==doc->mPane[0].rgbBufSize &&
			std::equal(original.begin(), original.end(), doc->mPane[0].rgbBuf), "typography relayout preserves actual image bytes, file and format");
		Check(psnr == doc->mFrmCmpStrategy->CropScore(doc->mPane,doc->mPane+1,METRIC_PSNR_IDX,l,t,r,b) &&
			ssim == doc->mFrmCmpStrategy->CropScore(doc->mPane,doc->mPane+1,METRIC_SSIM_IDX,l,t,r,b), "PSNR and SSIM ROI scores unchanged after typography relayout");
		const auto* strategy = doc->mFrmCmpStrategy;
		const auto* scan = doc->mFileScanThread;
		std::vector<BYTE> secondOriginal(doc->mPane[1].rgbBuf,doc->mPane[1].rgbBuf+doc->mPane[1].rgbBufSize);
		const auto theme = q1view::WindowsUiAppearanceState().Appearance();
		CRect themeBounds; frame->GetWindowRect(themeBounds);
		HMENU options = frame->mOptionsMenu.GetSafeHmenu();
		Check(frame->mUiMenus.Text(frame->GetMenu()->GetSafeHmenu(),5,true)==L"&Options" && GetMenuItemCount(options)==1 &&
			GetMenuItemID(options,0)==ID_OPTIONS_DIFF_RESOLUTION, "Comparer has no theme menu and retains its comparison-specific option");
		frame->SendMessage(WM_COMMAND,q1view::ID_UI_APPEARANCE_SYSTEM+(UINT(theme)+1)%3); Pump(30);
		Check(q1view::WindowsUiAppearanceState().Appearance()==theme, "retired Comparer theme command cannot change shared preference");
		auto SharedTheme = [&](unsigned choice) {
			Check(WriteSharedThemeFromChild(AfxGetApp()->m_pszRegistryKey,choice), "separate process writes isolated shared theme and posts notification");
			Pump(100);
			Check(unsigned(q1view::WindowsUiAppearanceState().Appearance())==choice, "Comparer receives cross-process theme change without activation or its own theme command");
		};
		for (UINT command : {q1view::ID_UI_APPEARANCE_LIGHT,q1view::ID_UI_APPEARANCE_DARK,
			q1view::ID_UI_APPEARANCE_LIGHT,q1view::ID_UI_APPEARANCE_SYSTEM}) {
			SharedTheme(command-q1view::ID_UI_APPEARANCE_SYSTEM);
			Check(q1view::WindowsUiAppearanceCommandChecked(command), "actual appearance command updates checked preference");
			Check(q1view::WindowsUiAppearanceState().HighContrast() || q1view::WindowsUiColorValue(q1view::WindowsUiColor::Canvas)==
				(q1view::WindowsUiAppearanceState().Dark() ? RGB(24,24,24) : RGB(236,236,236)), "Comparer theme changes UI and canvas brightness together");
			CRect current; frame->GetWindowRect(current);
			Check(current==themeBounds && doc->mN==n && doc->mD==d && doc->mXOff==x && doc->mYOff==y,
				"appearance switch preserves Comparer geometry, zoom and pan");
			Check(doc->GetSelectionRect(l,t,r,b) && l==2 && t==3 && r==10 && b==12, "appearance switch preserves Comparer ROI");
			Check(doc->mFrmCmpStrategy==strategy && doc->mFileScanThread==scan && doc->mPane[0].pathName==path &&
				doc->mPane[0].colorSpace==color && std::equal(original.begin(),original.end(),doc->mPane[0].rgbBuf) &&
				std::equal(secondOriginal.begin(),secondOriginal.end(),doc->mPane[1].rgbBuf), "appearance switch preserves both source buffers and does not restart comparison scanning");
			Check(psnr==doc->mFrmCmpStrategy->CropScore(doc->mPane,doc->mPane+1,METRIC_PSNR_IDX,l,t,r,b) &&
				ssim==doc->mFrmCmpStrategy->CropScore(doc->mPane,doc->mPane+1,METRIC_SSIM_IDX,l,t,r,b), "appearance switch leaves actual PSNR and SSIM unchanged");
		}
		SharedTheme(UINT(theme));
		Pump(100);
		HWND menuButton = GetDlgItem(frame->mUiFrame.MenuHost(), 2);
		const auto before = Pixels(menuButton);
		const auto imageBefore = Pixels(doc->mPane[0].pView->m_hWnd);
		CRect bounds; frame->GetWindowRect(bounds);
		for (int i=0;i<12;++i) {
			frame->SetWindowPos(nullptr, bounds.left+(i%2)*65, bounds.top+(i%2)*39, 0,0,SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE);
			Pump(70);
			Check(Pixels(menuButton)==before, "presented menu text pixels survive window movement without mouse/click repaint");
			Check(Pixels(doc->mPane[0].pView->m_hWnd)==imageBefore, "presented image pane pixels survive movement without forced repaint");
		}
		Check(doc->GetSelectionRect(l,t,r,b) && l==2 && t==3 && r==10 && b==12 && doc->mN==n && doc->mD==d, "window movement preserves selection and scale");
		wchar_t live[8] = {};
		if (GetEnvironmentVariableW(L"Q1VIEW_COMPARER_TEST_LIVE_MOVE", live, _countof(live))) {
			// Passive in-app readback while external native input moves this test
			// window. No synthesized mouse/key input or forced repaint here.
			CRect observed; frame->GetWindowRect(observed);
			const ULONGLONG end = GetTickCount64()+60000;
			int moves = 0;
			while (GetTickCount64()<end && moves<1) {
				Pump(100); CRect current; frame->GetWindowRect(current);
				if (current.TopLeft()!=observed.TopLeft()) {
					// Do not sample an intermediate native bit-copy while the
					// drag button is still down. Observe the settled presentation.
					if (GetAsyncKeyState(VK_LBUTTON)&0x8000) continue;
					Pump(350);
					Check(Pixels(menuButton)==before, "live native mouse-move menu pixels match pre-move readback");
					Check(Pixels(doc->mPane[0].pView->m_hWnd)==imageBefore, "live native mouse-move image pixels match pre-move readback");
					observed=current; ++moves;
				}
			}
			Check(moves>=1, "settled live native window move observed without forcing repaint");
		}
		frame->MoveWindow(50, 50, 800, 500); Pump(250);
		Check(doc->mPane[0].pView->mHCanvas >= q1view::WindowsUiPixels(60, q1view::WindowsUiDpi(frame->m_hWnd)), "small window retains useful image canvas at current OS text size");
		// All four panes, including C/D, must repaint without changing their inputs.
		Check(doc->OpenMultiFiles({first,second,first,second})!=FALSE, "four same-folder still-image comparison sources open");
		frame->ShowWindow(SW_RESTORE); frame->MoveWindow(50,50,1200,800); Pump(300);
		doc->mN=.05f; doc->mD=ZOOM_DELTA(doc->mN); doc->mXOff=doc->mYOff=0;
		doc->setDstSize(); doc->MarkImgViewProcessing(); doc->UpdateAllViews(nullptr); Pump(100);
		std::vector<BYTE> fourSources[4];
		for (int i=0;i<4;++i) {
			Check(doc->mPane[i].isAvail(), "A/B/C/D source pane remains available");
			fourSources[i].assign(doc->mPane[i].rgbBuf,doc->mPane[i].rgbBuf+doc->mPane[i].rgbBufSize);
		}
		for (UINT command : {q1view::ID_UI_APPEARANCE_DARK,q1view::ID_UI_APPEARANCE_LIGHT,q1view::ID_UI_APPEARANCE_SYSTEM}) {
			SharedTheme(command-q1view::ID_UI_APPEARANCE_SYSTEM);
			const COLORREF canvasColor = q1view::WindowsUiColorValue(q1view::WindowsUiColor::Canvas);
			const DWORD expected = (DWORD(GetRValue(canvasColor))<<16) | (DWORD(GetGValue(canvasColor))<<8) | GetBValue(canvasColor);
			for (int i=0;i<4;++i) {
				auto* paneView = doc->mPane[i].pView;
				const auto pixels = Pixels(paneView->m_hWnd);
				RECT client; GetClientRect(paneView->m_hWnd,&client);
				const int y = paneView->mRcControls.bottom+5;
				Check(y<client.bottom && pixels[size_t(y)*client.right+5]==expected,
					"A/B/C/D presented exposed canvas matches active theme background");
				Check(fourSources[i].size()==doc->mPane[i].rgbBufSize && std::equal(fourSources[i].begin(),fourSources[i].end(),doc->mPane[i].rgbBuf),
					"theme/background switching preserves all four source buffers");
			}
		}
		SharedTheme(UINT(theme));
		Pump(100);
		{
			RawTimelineFixture raw(120);
			Check(doc->OpenMultiFiles({raw.path,raw.path})!=FALSE, "multi-frame RAW sources open for real Timeline scrolling");
			frame->ShowWindow(SW_RESTORE); frame->MoveWindow(50,50,1100,800); Pump(250);
			doc->KillPlayTimer();
			Check(doc->mPane[0].frames==120 && doc->mPane[1].frames==120, "Timeline sources expose 120 real frames");
			auto* bar = static_cast<CThemeScrollBar*>(timeline->GetScrollBarCtrl(SB_VERT));
			Check(bar->IsWindowVisible() && timeline->GetScrollLimit(SB_VERT)>0, "overflowing Timeline shows its scrollbar");
			CheckScrollbarAccessibility(bar);
			CRect anchored; bar->GetWindowRect(anchored);
			const int frameId = doc->mPane[0].curFrameID;
			timeline->SendMessage(WM_MOUSEWHEEL,MAKEWPARAM(0,short(-WHEEL_DELTA)),0); Pump(40);
			Check(timeline->GetScrollPosition().y>0 && doc->mPane[0].curFrameID==frameId, "wheel scrolls Timeline without advancing image frames");
			bar->SendMessage(WM_KEYDOWN,VK_NEXT,0); Pump(40);
			Check(timeline->GetScrollPosition().y>100, "native scrollbar Page Down keyboard interface remains functional");
			timeline->SendMessage(WM_KEYDOWN,VK_HOME,0); Pump(40);
			Check(timeline->GetScrollPosition().y==0, "Timeline Home returns to top");
			for (unsigned choice : {2u,1u,0u}) {
				timeline->SendMessage(WM_KEYDOWN,VK_NEXT,0);
				const int retainedOffset = timeline->GetScrollPosition().y;
				SharedTheme(choice);
				Check(retainedOffset>0 && timeline->GetScrollPosition().y==retainedOffset,
					"theme change preserves nonzero Timeline scroll offset");
				bar->SendMessage(WM_MOUSELEAVE); bar->UpdateWindow();
				const auto g = bar->GetGeometry();
				const int x = g.client.right/2, y = (g.top+g.bottom)/2;
				const auto before = Pixels(bar->m_hWnd);
				const COLORREF track = q1view::WindowsUiColorValue(q1view::WindowsUiColor::SurfaceAlt);
				const DWORD expected = (DWORD(GetRValue(track))<<16)|(DWORD(GetGValue(track))<<8)|GetBValue(track);
				if (!q1view::WindowsUiAppearanceState().HighContrast())
					Check(before[size_t(g.client.bottom/2)*g.client.right]==expected, "presented scrollbar track follows active Light/Dark/System palette");
				// A synthetic in-app move does not move the physical pointer;
				// sample synchronously before Windows posts its real leave event.
				bar->SendMessage(WM_MOUSEMOVE,0,MAKELPARAM(x,y)); bar->UpdateWindow();
				if (!q1view::WindowsUiAppearanceState().HighContrast())
					Check(Pixels(bar->m_hWnd)!=before, "scrollbar hover has visible feedback");
				// The native contrast-theme handler runs its own modal mouse loop;
				// only our normal-theme handler accepts synchronous test messages.
				if (!q1view::WindowsUiAppearanceState().HighContrast()) {
					bar->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,y));
					bar->SendMessage(WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x,g.client.bottom-g.arrow-2));
					bar->SendMessage(WM_LBUTTONUP,0,MAKELPARAM(x,g.client.bottom-g.arrow-2)); Pump(40);
					Check(timeline->GetScrollPosition().y==timeline->GetScrollLimit(SB_VERT), "thumb drag reaches exact Timeline end");
					Check(!bar->IsTrackingThumb() && ::GetCapture()!=bar->m_hWnd,"thumb release clears dragging and capture");
				}
				CRect current; bar->GetWindowRect(current);
				Check(current==anchored && doc->mPane[0].curFrameID==frameId, "scrollbar stays anchored and scrolling preserves current source frame");
				timeline->SendMessage(WM_KEYDOWN,VK_HOME,0); Pump(30);
				bar->SendMessage(WM_MOUSELEAVE);
			}
			if (!q1view::WindowsUiAppearanceState().HighContrast()) {
				const auto g = bar->GetGeometry();
				const int x = g.client.right/2, down = g.client.bottom-g.arrow/2;
				bar->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,down));
				const int firstStep = timeline->GetScrollPosition().y;
				bar->SendMessage(WM_TIMER,1,0);
				Check(firstStep>0 && timeline->GetScrollPosition().y>firstStep,"arrow press and repeat scroll by lines");
				bar->SendMessage(WM_LBUTTONUP,0,MAKELPARAM(x,down));
				const int released = timeline->GetScrollPosition().y;
				bar->SendMessage(WM_TIMER,1,0);
				Check(::GetCapture()!=bar->m_hWnd && timeline->GetScrollPosition().y==released,"arrow release stops repeat and capture");
				timeline->SendMessage(WM_KEYDOWN,VK_HOME,0);
				bar->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,g.client.bottom-g.arrow-2));
				bar->SendMessage(WM_LBUTTONUP,0,MAKELPARAM(x,g.client.bottom-g.arrow-2));
				Check(timeline->GetScrollPosition().y>firstStep,"track click scrolls a page rather than a line");
				// Exercise the full 32-bit range plumbing without generating a
				// huge video or changing any source frame buffers.
				SCROLLINFO large = {sizeof(large),SIF_RANGE|SIF_PAGE|SIF_POS,0,200000,UINT(g.client.bottom),0};
				bar->SetScrollInfo(&large);
				const auto big = bar->GetGeometry();
				bar->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(x,(big.top+big.bottom)/2));
				bar->SendMessage(WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(x,big.client.bottom-big.arrow-1));
				Check(timeline->GetScrollLimit(SB_VERT)>65535 && timeline->GetScrollPosition().y==timeline->GetScrollLimit(SB_VERT),
					"thumb tracking preserves positions above the 16-bit message limit");
				bar->SendMessage(WM_CANCELMODE);
				Check(!bar->IsTrackingThumb() && ::GetCapture()!=bar->m_hWnd,"cancel mode safely releases active thumb drag");
				timeline->ConfigureScrollSizes(doc);
				timeline->SendMessage(WM_KEYDOWN,VK_HOME,0);
			}
			SharedTheme(UINT(theme));
			Check(doc->OpenMultiFiles({first,second})!=FALSE,"still images reopen after Timeline regression checks"); Pump(100);
			Check(!bar->IsWindowVisible() && timeline->GetScrollPosition().y==0,"returning to still images hides scrollbar and clears offset");
		}
		{
			RawTimelineFixture raw(2);
			Check(doc->OpenMultiFiles({raw.path,raw.path})!=FALSE,"short video timeline fixture opens"); Pump(100);
			doc->KillPlayTimer();
			Check(!timeline->GetScrollBarCtrl(SB_VERT)->IsWindowVisible() && timeline->GetScrollPosition().y==0,
				"all-fitting video Timeline has no phantom two-pixel scrollbar");
			doc->OpenMultiFiles({first,second}); Pump(100);
		}
		fprintf(report, "Actual window DPI: %u; OS text factor: %.3f\n", q1view::WindowsUiDpi(frame->m_hWnd), q1view::WindowsUiSettings().Scale());
		fprintf(report, "ALL COMPARER TYPOGRAPHY CHECKS PASSED\n"); fclose(report); return 0;
	} catch(const std::exception& error) {
		fprintf(report, "ERROR: %s\n", error.what()); fclose(report); return 1;
	}
}
