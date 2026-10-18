// /p:Q1ViewComparerTests=true: actual MFC document/views, isolated preferences.
#include "../Comparator/stdafx.h"
#include "../Comparator/Comparator.h"
#include "../Comparator/MainFrm.h"
#include "../Comparator/ComparatorDoc.h"
#include "../Comparator/ComparatorView.h"
#include "../Comparator/FrmCmpStrategy.h"
#include "../Comparator/YuvFrmCmpStrategy.h"
#include "../Comparator/RgbFrmCmpStrategy.h"
#include "../Comparator/PosInfoView.h"
#include "../Comparator/FileScanThread.h"
#include "../Comparator/FrmInfoView.h"
#include "../Comparator/FrmsInfoView.h"
#include "../Comparator/LpipsEngine.h"
#include <oleacc.h>
#pragma comment(lib, "oleacc.lib")
#include "Q1UiFontWin.h"
#include "Q1UiAppearanceMenuWin.h"
#include "SharedThemeTestWriter.h"
#include <cstdio>
#include <stdexcept>
#include <share.h>
#include <cmath>
#include <functional>
#include "WindowsUiPaintTestProbe.h"

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
		for (int dispatched=0; dispatched<200 && PeekMessage(&message, nullptr, 0, 0, PM_REMOVE); ++dispatched) {
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
	explicit RawTimelineFixture(int count, int delta=0, bool rgb=false, int width=16, int height=16) {
		wchar_t folder[MAX_PATH], name[MAX_PATH];
		Check(GetTempPathW(MAX_PATH,folder) && GetTempFileNameW(folder,L"Q1T",0,name), "temporary timeline fixture allocated");
		CString suffix; suffix.Format(L"_%dx%d_%s.%s",width,height,rgb ? L"rgb888" : L"yuv420",rgb ? L"rgb" : L"yuv");
		path = CString(name)+suffix;
		DeleteFileW(name);
		FILE* file = nullptr; Check(_wfopen_s(&file,path,L"wb")==0 && file, "RAW timeline fixture opened");
		std::vector<BYTE> pixels(rgb ? size_t(width)*height*3 : size_t(width)*height*3/2,128);
		bool written = true;
		for (int i=0;i<count;++i) { pixels[0]=BYTE(i+delta); written = written && fwrite(pixels.data(),1,pixels.size(),file)==pixels.size(); }
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

void WaitForAnalysis(CComparatorDoc* doc, DWORD limit=15000) {
	ULONGLONG end=GetTickCount64()+limit;
	for (;;) {
		Pump(20);
		doc->RefreshSelectionMetric();
		bool ready=doc->mFileScanThread->isScanComplete() && !doc->IsLpipsScanRunning();
		for (int target:doc->ComparisonTargets()) ready=ready && doc->mPairResults[target-1].state==_T("Ready");
		if (ready) return;
		if (GetTickCount64()>=end) throw std::runtime_error("comparison analysis timed out");
	}
}

bool SameCurrentResult(const PairCurrentResult& a,const PairCurrentResult& b) {
	return a.request==b.request && a.full==b.full && a.channels==b.channels && a.roi==b.roi && a.state==b.state;
}

std::shared_ptr<CurrentComparisonRequest> TestCurrentRequest(CComparatorDoc* doc,unsigned long long id,unsigned targets) {
	auto request=std::make_shared<CurrentComparisonRequest>();
	request->id=id; request->targets=targets; request->width=doc->mW; request->height=doc->mH;
	for (int slot=0;slot<4;++slot) if (!slot || (targets&(1u<<slot))) {
		const auto& source=doc->mPane[slot];
		auto snapshot=std::make_shared<ComparatorPane>();
		snapshot->curFrameID=source.curFrameID; snapshot->frames=source.frames;
		snapshot->colorSpace=source.colorSpace; snapshot->csLoadInfo=source.csLoadInfo;
		snapshot->csc2rgb888=source.csc2rgb888; snapshot->csc2yuv420=source.csc2yuv420;
		snapshot->origBufSize=snapshot->origSceneSize=source.origSceneSize;
		snapshot->origBuf=new BYTE[source.origSceneSize]; memcpy(snapshot->origBuf,source.origBuf,source.origSceneSize);
		snapshot->rgbBufSize=source.rgbBufSize; snapshot->rgbBuf=new BYTE[source.rgbBufSize];
		memcpy(snapshot->rgbBuf,source.rgbBuf,source.rgbBufSize); snapshot->rgbValid=true;
		request->sources[slot]=snapshot;
		if (slot) request->strategies[slot-1]=doc->PairStrategy(slot);
	}
	return request;
}

// Insert a newer request during a real metric calculation, deterministically
// testing the publication race without test hooks/sleeps in production code.
class QueueDuringMetricStrategy : public RgbFrmCmpStrategy {
public:
	mutable std::function<void()> duringMeasure;
	void CalMetricsImpl(ComparatorPane* a,ComparatorPane* b,int metric,std::vector<CString>& scores) const override {
		auto callback=std::move(duringMeasure);
		if (callback) callback();
		RgbFrmCmpStrategy::CalMetricsImpl(a,b,metric,scores);
	}
};

void CheckCurrentRequestIsolation(CComparatorDoc* doc) {
	doc->StopAnalysis();
	FileScanThread worker(doc); // No sequence domain: manually step the real current worker.
	QueueDuringMetricStrategy strategyB,strategyC;
	strategyB.Setup(doc->mW,doc->mH); strategyC.Setup(doc->mW,doc->mH);
	auto b=TestCurrentRequest(doc,1,2),c=TestCurrentRequest(doc,2,4);
	auto latestC=TestCurrentRequest(doc,3,4),d=TestCurrentRequest(doc,4,8);
	b->strategies[0]=&strategyB; c->strategies[1]=&strategyC;
	strategyB.duringMeasure=[&] { worker.queueCurrent(c); };
	strategyC.duringMeasure=[&] { worker.queueCurrent(latestC); worker.queueCurrent(d); };
	worker.queueCurrent(b); worker.threadLoop();
	PairCurrentResult resultB,resultC,resultD;
	Check(worker.copyCurrent(1,1,resultB) && resultB.state==_T("Ready") && worker.pendingCurrentMask()==4,
		"C queued during a real A/B calculation does not cancel A/B publication");
	worker.threadLoop();
	Check(!worker.copyCurrent(2,2,resultC) && worker.pendingCurrentMask()==12 && worker.copyCurrent(1,1,resultB),
		"C superseded during its own calculation cannot publish and preserves unrelated B/D work");
	worker.threadLoop(); worker.threadLoop();
	Check(worker.copyCurrent(2,3,resultC) && resultC.state==_T("Ready") && worker.copyCurrent(3,4,resultD) &&
		resultD.state==_T("Ready") && worker.copyCurrent(1,1,resultB) && !worker.hasWork(),
		"independent latest C and D requests both finish without dropping completed B");
	const auto measuredC=worker.currentMeasureCount(2);
	for (unsigned long long id=5;id<=104;++id) worker.queueCurrent(TestCurrentRequest(doc,id,4));
	Check(worker.pendingCurrentMask()==4,"100 rapid C requests occupy exactly one replaceable pending pair slot");
	worker.threadLoop();
	Check(worker.copyCurrent(2,104,resultC) && !worker.copyCurrent(2,103,resultC) &&
		worker.currentMeasureCount(2)==measuredC+1 && worker.currentMeasureCount(1)==1 && worker.currentMeasureCount(3)==1,
		"rapid C requests measure only the latest snapshot and never recompute B/D");
	worker.queueCurrent(TestCurrentRequest(doc,105,14)); worker.cancelCurrent();
	Check(!worker.pendingCurrentMask() && !worker.hasWork() && !worker.copyCurrent(1,1,resultB) && !worker.copyCurrent(1,0,resultB),
		"cancelling clears all pending slots and rejects old or zero request IDs");
}

void CheckFooterAlignment(CComparatorDoc* doc) {
	auto* frame=static_cast<CMainFrame*>(AfxGetMainWnd());
	auto* info=doc->mFrmInfoView;
	info->UpdateWindow();
	CRect area; info->GetClientRect(area); info->ClientToScreen(area);
	LONG right=area.right;
	if (info->GetScrollBarCtrl(SB_VERT)->IsWindowVisible())
		right-=q1view::WindowsUiPixels(17,q1view::WindowsUiDpi(info->m_hWnd));
	for (int slot=0;slot<frame->mNumOfViews;++slot) {
		if (slot && !doc->mPane[slot].isAvail()) continue;
		auto* view=doc->mPane[slot].pView;
		CRect sourceButton; view->mCsQMenu.GetWindowRect(sourceButton); view->ScreenToClient(sourceButton);
		Check(sourceButton.left >= 0 && sourceButton.right <= view->mRcClose.left &&
			view->mRcNameQMenu.Width() >= 0 && view->mRcNameQMenu.right <= view->mRcClose.left,
			"measured source controls never cover the pane close target, even in narrow or unequal columns");
		CRect image; view->GetClientRect(image); view->ClientToScreen(image);
		CWnd& footer=slot ? static_cast<CWnd&>(info->mPairButtons[slot-1]) : static_cast<CWnd&>(info->mReferenceLabel);
		CRect result; footer.GetWindowRect(result);
		Check(footer.IsWindowVisible() && result.left==max(image.left,area.left) && result.right==min(image.right,right),
			"each result footer matches the actual horizontal bounds of its own image pane");
	}
}

struct MenuUpdateProbe {
	HWND window;
	int paints = 0, positions = 0;
	explicit MenuUpdateProbe(HWND value) : window(value) {
		Check(SetWindowSubclass(window, Proc, 139, reinterpret_cast<DWORD_PTR>(this)) != FALSE, "zoom menu message probe installed");
	}
	~MenuUpdateProbe() { if (IsWindow(window)) RemoveWindowSubclass(window, Proc, 139); }
	static LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR data) {
		auto* probe = reinterpret_cast<MenuUpdateProbe*>(data);
		if (message == WM_PAINT) ++probe->paints;
		if (message == WM_WINDOWPOSCHANGED) ++probe->positions;
		return DefSubclassProc(window, message, wp, lp);
	}
};
}

namespace {
class SourceCommandTestHost : public CWnd {
public:
	int opened = 0, executed = 0; bool timeout = false, keysQueued = false;
	static constexpr UINT Command = 0x6111;
	BOOL OnCommand(WPARAM wp, LPARAM lp) override {
		if (!lp && LOWORD(wp) == Command) { ++executed; return TRUE; }
		return CWnd::OnCommand(wp, lp);
	}
	LRESULT WindowProc(UINT message, WPARAM wp, LPARAM lp) override {
		if (message == WM_INITMENUPOPUP) {
			++opened; SetTimer(119, 1500, nullptr);
			keysQueued = false;
			std::fprintf(report, "Source popup init flags=%u\n", ::GetMenuState(reinterpret_cast<HMENU>(wp), Command, MF_BYCOMMAND));
			return 0; // This standalone host owns its command, not the app frame.
		}
		if (message == WM_ENTERIDLE && wp == MSGF_MENU && !keysQueued) {
			keysQueued = true;
			::PostMessage(reinterpret_cast<HWND>(lp), WM_CHAR, 'e', 1);
		}
		if (message == WM_TIMER && wp == 119) { timeout = true; EndMenu(); KillTimer(119); return 0; }
		if (message == WM_MENUSELECT) std::fprintf(report, "Source menu select: id=%u flags=%u\n", LOWORD(wp), HIWORD(wp));
		return CWnd::WindowProc(message, wp, lp);
	}
};

void CheckSourceCommandButton()
{
	SourceCommandTestHost host;
	Check(host.CreateEx(0, AfxRegisterWndClass(0), _T("Source command regression"), WS_OVERLAPPEDWINDOW,
		30, 30, 400, 160, nullptr, nullptr), "native source-command test host created");
	CMenu menu; menu.CreatePopupMenu(); menu.AppendMenu(MF_STRING, SourceCommandTestHost::Command, _T("&Execute once"));
	CQMenuItem command, label; CRect rect(10, 10, 180, 50), nameRect(10, 55, 350, 90);
	Check(command.Create(_T("Raw format"), rect, &host, &menu) && label.Create(_T("Read-only filename"), nameRect, &host, nullptr), "native command and read-only source label created");
	CRect measured(0,0,0,0); command.CalcRect(&measured);
	{ CClientDC dc(&command); CString text; command.DefaultSetting(&dc,text);
		const int dpi=q1view::WindowsUiDpi(command.m_hWnd);
		Check(measured.Width() >= dc.GetTextExtent(text).cx + q1view::WindowsUiPixels(8,dpi)*3 + q1view::WindowsUiPixels(10,dpi),
			"source command measurement reserves the same text, chevron and padding that painting uses"); }
	host.ShowWindow(SW_SHOW); host.SetForegroundWindow(); host.SetActiveWindow(); command.SetFocus(); Pump(30);
	wchar_t name[32] = {}; GetClassName(command.m_hWnd, name, _countof(name));
	Check(!_wcsicmp(name, L"Button") && (command.GetStyle() & WS_TABSTOP), "source popup is a native accessible tab-stop button");
	GetClassName(label.m_hWnd, name, _countof(name));
	Check(!_wcsicmp(name, L"Static") && !(label.GetStyle() & WS_TABSTOP), "filename is a native read-only label, not another action");
	command.SendMessage(WM_MOUSEMOVE, 0, MAKELPARAM(20, 20));
	Check(::GetCapture() != command.m_hWnd, "source-command hover does not capture the pointer");
	command.SendMessage(WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(20, 20));
	Check((command.SendMessage(WM_QUERYUISTATE) & UISF_HIDEFOCUS)!=0,
		"mouse activation hides the keyboard-only source focus cue");
	Check(host.opened == 0, "source command does not activate on mouse-down");
	host.SetTimer(119, 1500, nullptr);
	command.SendMessage(WM_LBUTTONUP, 0, MAKELPARAM(20, 20)); Pump(40); host.KillTimer(119);
	std::fprintf(report, "Source popup probe: opened=%d executed=%d timeout=%d queued=%d returned=%llu retained=%llu\n", host.opened, host.executed, int(host.timeout), int(host.keysQueued),
		static_cast<unsigned long long>(reinterpret_cast<UINT_PTR>(::GetPropW(host.m_hWnd,L"Q1ViewTestReturnedCommand"))),
		static_cast<unsigned long long>(reinterpret_cast<UINT_PTR>(::GetPropW(host.m_hWnd,L"Q1ViewTestRetainedControl")))); std::fflush(report);
	Check(!host.timeout && host.opened == 1 && host.executed == 1, "one native release opens one popup and executes one command");
	command.SendMessage(WM_KEYDOWN, VK_SPACE); command.SendMessage(WM_KEYDOWN, VK_SPACE, 1 | (KF_REPEAT << 16));
	host.SetTimer(119, 1500, nullptr);
	command.SendMessage(WM_KEYUP, VK_SPACE); Pump(40); host.KillTimer(119);
	Check(host.opened == 2 && host.executed == 2, "held Space activates once on release without duplicate command notification");
	Check((command.SendMessage(WM_QUERYUISTATE) & UISF_HIDEFOCUS)==0,
		"keyboard activation exposes the native source focus cue");
	host.SetTimer(119, 1500, nullptr);
	command.SendMessage(WM_KEYDOWN, VK_RETURN); Pump(40); host.KillTimer(119);
	Check(host.opened == 3 && host.executed == 3, "Enter activates the same existing popup command once");
	command.SendMessage(WM_KEYDOWN, VK_RETURN, 1 | (KF_REPEAT << 16));
	Pump(20);
	Check(host.opened == 3, "held Enter does not repeat the popup action");
	command.EnableWindow(FALSE); command.SendMessage(BM_CLICK);
	host.SendMessage(WM_COMMAND, MAKEWPARAM(0xffff, BN_CLICKED), reinterpret_cast<LPARAM>(command.m_hWnd)); Pump(20);
	Check(host.opened == 3 && host.executed == 3, "disabled control rejects native clicks and forged notifications");
	{
		ScopedWindowsUiPaintTestTheme restoreTheme;
		for (auto theme : {q1view::WindowsUiAppearance::Light,q1view::WindowsUiAppearance::Dark}) {
			q1view::WindowsUiAppearanceState().SetAppearance(theme);
			command.Invalidate(FALSE); command.UpdateWindow();
			const auto pixels=Pixels(command.m_hWnd);
			CRect bounds; command.GetClientRect(bounds);
			const auto edge=q1view::WindowsUiColorValue(q1view::WindowsUiColor::Boundary);
			const DWORD expected=(DWORD(GetRValue(edge))<<16)|(DWORD(GetGValue(edge))<<8)|GetBValue(edge);
			bool solid=true;
			for (int x=1;x<bounds.right-1;++x) solid=solid && pixels[x]==expected;
			Check(solid,"disabled source buttons keep the ordinary solid edge with no invented dashed pattern");
		}
	}
	IAccessible* accessible = nullptr;
	Check(SUCCEEDED(AccessibleObjectFromWindow(command.m_hWnd, OBJID_CLIENT, IID_IAccessible, reinterpret_cast<void**>(&accessible))), "source command exposes native accessibility metadata");
	VARIANT self; VariantInit(&self); self.vt = VT_I4; self.lVal = CHILDID_SELF;
	VARIANT role, state; VariantInit(&role); VariantInit(&state);
	accessible->get_accRole(self, &role); accessible->get_accState(self, &state);
	Check(role.vt == VT_I4 && role.lVal == ROLE_SYSTEM_PUSHBUTTON && state.vt == VT_I4 && (state.lVal & STATE_SYSTEM_UNAVAILABLE), "screen readers receive push-button role and actual disabled state");
	VariantClear(&role); VariantClear(&state); accessible->Release();
	host.DestroyWindow();
}
}

int RunComparerTypographyTests() {
	wchar_t output[MAX_PATH] = {};
	GetEnvironmentVariableW(L"Q1VIEW_COMPARER_TEST_REPORT", output, _countof(output));
	if (!output[0]) wcscpy_s(output, L"Tests\\bin\\x64\\Release\\comparer-typography-report.txt");
	report=_wfsopen(output,L"w",_SH_DENYNO);
	if (!report) return 2;
	try {
		CheckSourceCommandButton();
		auto* frame = static_cast<CMainFrame*>(AfxGetMainWnd());
		Check(frame != nullptr, "real Comparer frame created");
		auto* doc = static_cast<CComparatorDoc*>(frame->GetActiveDocument());
		Check(doc != nullptr, "real Comparer document created");
		doc->SelectMetric(METRIC_PSNR_IDX);
		// An isolated test profile may still restore a maximized startup state.
		// Normalize it before taking equal-size movement pixel baselines.
		frame->ShowWindow(SW_RESTORE);
		frame->MoveWindow(50, 50, 1100, 800); Pump(250);
		{
			ScopedWindowsUiPaintTestTheme restoreTheme;
			for (auto theme : {q1view::WindowsUiAppearance::Dark,q1view::WindowsUiAppearance::Light}) {
				q1view::WindowsUiAppearanceState().SetAppearance(theme);
				Check(doc->mFrmState.IsEmpty(),"empty result area is tested without a reference source");
				CheckWindowsUiErase(doc->mFrmInfoView->m_hWnd,q1view::WindowsUiColor::Surface,true,Check);
				CheckWindowsUiOwnerDraw(doc->mFrmsInfoView->GetDlgItem(0x7950)->m_hWnd,Check);
			}
		}
		auto* timeline = doc->mPosInfoView;
		Check(timeline && timeline->GetScrollBarCtrl(SB_VERT) && !timeline->GetScrollBarCtrl(SB_VERT)->IsWindowVisible() &&
			timeline->GetScrollPosition().y==0, "empty Timeline has no scrollbar or phantom scroll offset");
		Check(true, "test frame initial layout completed");
		for (int slot=0; slot<2; ++slot)
			Check(doc->mPane[slot].pView->mCsQMenu.IsWindowEnabled() && doc->mPane[slot].pView->IsCommandEnabled(ID_CS_START),
				"empty source slots allow RAW preconfiguration without a test-forced refresh");
		wchar_t first[MAX_PATH], second[MAX_PATH];
		GetFullPathNameW(L"docs\\images\\sample-colored-pencils-reference.png", MAX_PATH, first, nullptr);
		GetFullPathNameW(L"docs\\images\\sample-colored-pencils-changed-region.png", MAX_PATH, second, nullptr);
		std::vector<CString> files = {first, second};
		frame->ChangeNumOfViews(2);
		doc->OpenMultiFiles(files); Pump(500);
		CString relativeFiles = L"docs\\images\\sample-colored-pencils-reference.png";
		relativeFiles += CSV_SEPARATOR;
		relativeFiles += L"docs\\images\\sample-colored-pencils-changed-region.png";
		Check(doc->OnOpenDocument(relativeFiles) != FALSE,
			"ordinary document-open path accepts multiple relative source paths");
		Pump(100);
		Check(doc->mPane[0].pathName.CompareNoCase(first)==0 && doc->mPane[1].pathName.CompareNoCase(second)==0 &&
			doc->mPane[0].isAvail() && doc->mPane[1].isAvail(),
			"relative shell-open paths are normalized before MRU/source loading without changing source identity");
		// Opening a large source intentionally auto-maximizes the production
		// frame. Movement checks need a restored, fixed-size window instead.
		frame->ShowWindow(SW_RESTORE);
		frame->MoveWindow(50, 50, 1100, 800); Pump(250);
		Check(doc->mFrmCmpStrategy && doc->mPane[0].isAvail() && doc->mPane[1].isAvail(), "two real image sources and comparison strategy loaded");
		for (int slot = 0; slot < 2; ++slot) {
			auto* sourceView = doc->mPane[slot].pView;
			Check(!sourceView->mCsQMenu.IsWindowEnabled() && !sourceView->IsCommandEnabled(ID_CS_START),
				"real file opening disables decoded-source controls without a test-forced state refresh");
			CString label; sourceView->mCsQMenu.GetWindowText(label);
			const CRect controls = sourceView->mRcControls, close = sourceView->mRcClose;
			const int canvas = sourceView->mHCanvas;
			sourceView->UpdateCsLabel(L"RGBA1010102");
			CRect button; sourceView->mCsQMenu.GetWindowRect(button); sourceView->ScreenToClient(button);
			Check(button.bottom == controls.bottom && sourceView->mRcControls == controls && sourceView->mRcClose == close && sourceView->mHCanvas == canvas,
				"format label changes retain the measured header height, close target and image canvas");
			sourceView->UpdateCsLabel(label);
			Check(!sourceView->mCsQMenu.IsWindowEnabled() && !sourceView->IsCommandEnabled(ID_CS_START), "fixed decoded sources disable raw-format entry points");
			const auto cs = doc->mPane[slot].colorSpace; const auto generation = doc->mFileScanThread->pairGeneration(1);
			sourceView->SendMessage(WM_COMMAND, ID_CS_START + 1); sourceView->OnCsChange(ID_CS_START + 1);
			Check(doc->mPane[slot].colorSpace == cs && doc->mFileScanThread->pairGeneration(1) == generation, "disabled source interpretation cannot mutate pixels or invalidate analysis via alternate entry points");
		}
		CheckFooterAlignment(doc);
		Check(!timeline->GetScrollBarCtrl(SB_VERT)->IsWindowVisible(), "still-image Timeline does not show an unnecessary scrollbar");
		if (frame->mUiFrame.Custom()) {
			HWND help = GetDlgItem(frame->mUiFrame.MenuHost(), 7), zoom = GetDlgItem(frame->mUiFrame.MenuHost(), 8);
			frame->UpdateMagnication(doc->mN); Pump(50);
			MenuUpdateProbe helpProbe(help), zoomProbe(zoom);
			CRect helpBefore, zoomBefore, imageBefore;
			::GetWindowRect(help, helpBefore); ::GetWindowRect(zoom, zoomBefore);
			doc->mPane[0].pView->GetWindowRect(imageBefore);
			const int menuHeight = frame->mUiFrame.Height();
			for (int repeat = 0; repeat < 4; ++repeat) for (float value : {.01f, .67f, 1.13f, 9.99f, 10.f, 99.99f, 128.f}) {
				frame->UpdateMagnication(value); Pump(2);
				CRect helpAfter, zoomAfter, imageAfter;
				::GetWindowRect(help, helpAfter); ::GetWindowRect(zoom, zoomAfter);
				doc->mPane[0].pView->GetWindowRect(imageAfter);
				Check(helpAfter == helpBefore && zoomAfter == zoomBefore && imageAfter == imageBefore && frame->mUiFrame.Height() == menuHeight,
					"rapid magnification labels leave Help, zoom label, and source canvas geometry stable");
				CString label; label.Format(L"%.2fx", value);
				CString accessible; ::GetWindowTextW(zoom, accessible.GetBuffer(80), 80); accessible.ReleaseBuffer();
				Check(accessible == label && frame->mUiMenus.Text(frame->GetMenu()->GetSafeHmenu(), ID_MAGNIFY) == static_cast<LPCTSTR>(label),
					"rapid zoom retains full accessible label and matching HMENU text");
			}
			const float savedN = doc->mN, savedD = doc->mD, savedX = doc->mXOff, savedY = doc->mYOff;
			auto* view = doc->mPane[0].pView;
			CPoint center(view->mWCanvas / 2, view->mRcControls.bottom + view->mHCanvas / 2); view->ClientToScreen(&center);
			for (int repeat = 0; repeat < 4; ++repeat) for (short delta : {short(WHEEL_DELTA), short(-WHEEL_DELTA)}) {
				const float previousN = doc->mN;
				view->OnMouseWheel(0, delta, center); Pump(5);
				Check(doc->mN != previousN, "real image-wheel handler still zooms in and out");
				CRect helpAfter; ::GetWindowRect(help, helpAfter);
				Check(helpAfter == helpBefore, "real wheel handler does not move Help");
			}
			doc->mN = savedN; doc->mD = savedD; doc->mXOff = savedX; doc->mYOff = savedY; doc->setDstSize();
			for (int slot = 0; slot < CComparatorDoc::IMG_VIEW_MAX; ++slot)
				if (doc->mPane[slot].pView) doc->mPane[slot].pView->DeterminDestOriginCoord(doc);
			frame->UpdateMagnication(savedN); doc->UpdateAllViews(nullptr); Pump(20);
			fprintf(report, "ZOOM MENU PROBE: Help paints=%d positions=%d; zoom paints=%d positions=%d\n",
				helpProbe.paints, helpProbe.positions, zoomProbe.paints, zoomProbe.positions);
			Check(!helpProbe.paints && !helpProbe.positions && !zoomProbe.positions && zoomProbe.paints > 0,
				"wheel and label updates repaint only magnification, not Help or menu geometry");
			const int zoomPaints = zoomProbe.paints;
			frame->UpdateMagnication(savedN); Pump(20);
			Check(zoomProbe.paints == zoomPaints, "unchanged rounded zoom label schedules no redundant paint");
		}
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
		Check(frame->mUiMenus.Text(frame->GetMenu()->GetSafeHmenu(),5,true)==L"&Options" && GetMenuItemCount(options)==3 &&
			GetMenuItemID(options,0)==ID_OPTIONS_DIFF_RESOLUTION && GetMenuItemID(options,2)==ID_SELECTION_MODE,
			"Comparer Options retains resolution policy and adds selection mode, without theme/sync/clear actions");
		{
			const bool before=doc->mSelMode;
			auto* active=frame->GetActiveView();
			auto checkSelection=[&] {
				frame->SendMessage(WM_INITMENUPOPUP,reinterpret_cast<WPARAM>(options),5);
				Check(bool(GetMenuState(options,ID_SELECTION_MODE,MF_BYCOMMAND)&MF_CHECKED)==doc->mSelMode,
					"Comparer Options selection check follows shared document state");
				for(int slot=0;slot<2;++slot) {
					auto* paneView=doc->mPane[slot].pView;
					CCmdUI state; state.m_nID=ID_SELECTION_MODE; state.m_nIndex=0;
					state.m_nIndexMax=paneView->mMouseMenu.GetMenuItemCount(); state.m_pMenu=&paneView->mMouseMenu;
					Check(static_cast<CCmdTarget*>(paneView)->OnCmdMsg(ID_SELECTION_MODE,CN_UPDATE_COMMAND_UI,&state,nullptr)!=FALSE,
						"each pane routes selection command UI to the shared state");
					Check(bool(paneView->mMouseMenu.GetMenuState(ID_SELECTION_MODE,MF_BYCOMMAND)&MF_CHECKED)==doc->mSelMode,
						"all pane context selection checks agree with Options");
				}
			};
			frame->SetActiveView(timeline); timeline->SetFocus();
			frame->SendMessage(WM_COMMAND,ID_SELECTION_MODE);
			Check(doc->mSelMode!=before,"Options selection toggles once even with Timeline active"); checkSelection();
			doc->mPane[1].pView->SendMessage(WM_COMMAND,ID_SELECTION_MODE);
			Check(doc->mSelMode==before,"candidate pane context command toggles the same document mode"); checkSelection();
			frame->SetActiveView(active); Pump(50);
			doc->mPane[0].pView->SendMessage(WM_KEYDOWN,'S',1); Pump(50);
			Check(doc->mSelMode!=before,"Comparer S shortcut shares the Options/context mode"); checkSelection();
			doc->mPane[0].pView->SendMessage(WM_KEYDOWN,'S',1); Pump(50);
			Check(doc->mSelMode==before && doc->GetSelectionRect(l,t,r,b) && l==2 && t==3 && r==10 && b==12,
				"selection-mode changes preserve the shared ROI and restore the original mode");
		}
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
		fprintf(report,"Narrow layout: DPI=%u client canvas=%dx%d controls=%ld frame chrome=%d required canvas=%d\n",
			q1view::WindowsUiDpi(frame->m_hWnd),doc->mPane[0].pView->mWCanvas,doc->mPane[0].pView->mHCanvas,
			doc->mPane[0].pView->mRcControls.Height(),frame->mUiFrame.Height(),
			q1view::WindowsUiPixels(60,q1view::WindowsUiDpi(frame->m_hWnd)));
		Check(doc->mPane[0].pView->mHCanvas >= q1view::WindowsUiPixels(60, q1view::WindowsUiDpi(frame->m_hWnd)), "small window retains useful image canvas at current OS text size");
		Check(doc->OpenMultiFiles({first,second,first})!=FALSE,"three-source comparison opens an additional image and its footer");
		WaitForAnalysis(doc);
		Check(frame->mNumOfViews==3,"three sources retain three matching source columns");
		CheckFooterAlignment(doc);
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
			for (int slot=0; slot<2; ++slot)
				Check(doc->mPane[slot].pView->mCsQMenu.IsWindowEnabled() && doc->mPane[slot].pView->IsCommandEnabled(ID_CS_START),
					"switching decoded input to RAW re-enables the existing source button automatically");
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
		{
			RawTimelineFixture a(90), bSource(100,10), c(20,20), dSource(120,30);
			doc->mHasSelection=false;
			Check(doc->OpenMultiFiles({a.path,bSource.path,c.path,dSource.path}),"four unequal-length RAW sources open");
			WaitForAnalysis(doc);
			// The final result publication invalidates the view; flush that paint
			// before inspecting native button and tooltip presentation.
			doc->mFrmInfoView->UpdateWindow();
			Check(doc->ComparisonTargets()==std::vector<int>({1,2,3}) && doc->TimelineSources()==std::vector<int>({0,1,2,3}),
				"A reference produces three stable pairs and four Timeline lanes");
			Check(doc->mMaxFrames==120 && doc->PairFrames(1)==90 && doc->PairFrames(2)==20 && doc->PairFrames(3)==90,
				"Timeline uses max(all sources); each pair uses its own minimum frame count");
			auto* scanner=doc->mFileScanThread;
			Check(scanner->decodeCount(0)==90 && scanner->decodeCount(1)==90 && scanner->decodeCount(2)==20 && scanner->decodeCount(3)==90,
				"coordinated scan decodes A once per aligned frame and only eligible candidate frames");
			double values[3][QPLANES];
			for (int target=1;target<4;++target) {
				Check(scanner->copyMetrics(0,METRIC_PSNR_IDX,values[target-1],target),"real PSNR cache available for every pair");
				double expected=qPSNR(doc->mPane[0].origBuf,doc->mPane[target].origBuf,16,16,16,1);
				Check(fabs(values[target-1][0]-expected)<1e-10,"pair Y PSNR matches independent native-buffer calculation");
				Check(scanner->summary(target).measured[0]==doc->PairFrames(target),"progress and averages count only real eligible measurements");
				Check(doc->mFrmInfoView->mPairButtons[target-1].IsWindowVisible(),"one real accessible result button exists per candidate");
				auto& button=doc->mFrmInfoView->mPairButtons[target-1];
				CString body; button.GetWindowText(body);
				Check(body.Find(_T("\nA:"))<0 && body.Find(a.path.Mid(a.path.ReverseFind(_T('\\'))+1))<0,
					"compact result card does not repeat source filenames");
				const auto& result=doc->mPairResults[target-1];
				Check(!result.channels.IsEmpty() && body.Find(result.channels)<0,
					"channel detail is hidden from the default current-result card");
				Check(button.mInfoText.Find(result.channels)>=0 && button.mInfoText.Find(_T("PSNR channels (dB)"))>=0,
					"channel scores and metric units remain available in the info tooltip");
				Check(button.mInfoText.Find(doc->mPane[0].pathName)>=0 && button.mInfoText.Find(doc->mPane[target].pathName)>=0,
					"tooltip retains both full source paths");
				CString nativeTip; button.mInfoTip.GetText(nativeTip,&button);
				Check(nativeTip==button.mInfoText,"native tooltip contains the current channel and source details");
				Check(body.Find(_T("RGB aggregate"))<0,"comparison-basis annotation is omitted from default result text");
			}
			CString reference; doc->mFrmInfoView->mReferenceLabel.GetWindowText(reference);
			{
				ScopedWindowsUiPaintTestTheme restoreTheme;
				for (auto theme : {q1view::WindowsUiAppearance::Dark,q1view::WindowsUiAppearance::Light}) {
					q1view::WindowsUiAppearanceState().SetAppearance(theme);
					for (auto& button : doc->mFrmInfoView->mPairButtons) CheckWindowsUiOwnerDraw(button.m_hWnd,Check);
					CheckWindowsUiOwnerDraw(doc->mFrmInfoView->mReferenceLabel.m_hWnd,Check);
				}
			}
			Check(reference==_T("Reference \x00b7 #0"),"A footer is a non-interactive reference label with the actual frame number");
			CheckFooterAlignment(doc);
			Check(values[0][0]>values[1][0] && values[1][0]>values[2][0],"three distinct candidates have three distinct numerical results");
			Check(!scanner->copyMetrics(20,0,values[0],2) && !scanner->copyMetrics(90,0,values[0],3),"pair domains do not pad missing frames with final-frame scores");
			frame->ShowWindow(SW_RESTORE); frame->MoveWindow(50,50,1500,900); Pump(100);
			CheckFooterAlignment(doc);
			int firstWidth,minimum; frame->mCompSplitter.GetColumnInfo(0,firstWidth,minimum);
			frame->mCompSplitter.SetColumnInfo(0,firstWidth+30,minimum); frame->mCompSplitter.RecalcLayout(); Pump(50);
			CheckFooterAlignment(doc);
			auto* graphs=doc->mFrmsInfoView;
			CRect plot=graphs->PlotBounds(2); Check(!plot.IsRectEmpty(),"second candidate has its own real plot");
			CPoint wheelPoint=plot.CenterPoint(); wheelPoint.y-=graphs->GetScrollPos(SB_VERT); graphs->ClientToScreen(&wheelPoint);
			double span=graphs->VisibleFrameSpan(); long beforeFrame=doc->mPane[0].curFrameID;
			graphs->SendMessage(WM_MOUSEWHEEL,MAKEWPARAM(0,short(WHEEL_DELTA)),MAKELPARAM(wheelPoint.x,wheelPoint.y)); Pump(50);
			Check(graphs->VisibleFrameSpan()<span && doc->mPane[0].curFrameID==beforeFrame,"wheel zooms shared graph X viewport without seeking frames");
			graphs->SendMessage(WM_LBUTTONDBLCLK,0,MAKELPARAM(plot.CenterPoint().x,plot.CenterPoint().y)); Pump(50);
			Check(graphs->VisibleFirstFrame()==0 && graphs->VisibleFrameSpan()==120,"double-click resets all graph viewports to the full domain");
			graphs->SendMessage(WM_COMMAND,MAKEWPARAM(0x7950,BN_CLICKED),0); Pump(50);
			Check(graphs->mCollapsed && scanner->summary(2).measured[0]==20,"collapsing Sequence analysis retains pair caches");
			graphs->SendMessage(WM_COMMAND,MAKEWPARAM(0x7950,BN_CLICKED),0); Pump(50);
			frame->MoveWindow(50,50,800,500); Pump(100);
			CheckFooterAlignment(doc);
			Check(graphs->GetScrollBarCtrl(SB_VERT)->IsWindowVisible(),
				"narrow-window independent graphs retain themed overflow scrolling");
			Check(doc->mPane[0].pView->mHCanvas>=60,"stacked analysis does not consume the narrow-window image canvas");
			doc->SetScenes(10); WaitForAnalysis(doc);
			const auto beforeB=doc->mPairResults[0],beforeD=doc->mPairResults[2];
			const auto countB=scanner->currentMeasureCount(1),countC=scanner->currentMeasureCount(2),countD=scanner->currentMeasureCount(3);
			bool updated=false; doc->SetScene(12,doc->mPane+2,updated); doc->RefreshSelectionMetric();
			Check(doc->mPairResults[0].request==beforeB.request && doc->mPairResults[0].full==beforeB.full &&
				doc->mPairResults[0].state==_T("Ready") && doc->mPairResults[2].request==beforeD.request &&
				doc->mPairResults[2].full==beforeD.full && doc->mPairResults[2].state==_T("Ready"),
				"C-only seek keeps completed A/B and A/D current results visible without resubmitting them");
			WaitForAnalysis(doc);
			Check(SameCurrentResult(doc->mPairResults[0],beforeB) && SameCurrentResult(doc->mPairResults[2],beforeD) &&
				scanner->currentMeasureCount(1)==countB && scanner->currentMeasureCount(2)==countC+1 && scanner->currentMeasureCount(3)==countD,
				"C-only seek performs one A/C current measurement and preserves all B/D score fields and request IDs");
			CString expected; doc->PairStrategy(2)->CalMetrics(doc->mPane,doc->mPane+2,0,expected);
			expected.Replace(_T("inf"),_T("\x221e"));
			Check(doc->mPane[0].curFrameID==10 && doc->mPane[2].curFrameID==12 && doc->mPairResults[1].full==expected,
				"independent C seek measures displayed A10/C12, not aligned cached A10/C10");
			doc->mFrmInfoView->UpdateWindow();
			CString unaligned,aligned; doc->mFrmInfoView->mPairButtons[1].GetWindowText(unaligned);
			doc->mFrmInfoView->mPairButtons[0].GetWindowText(aligned);
			Check(unaligned.Find(_T("Unaligned"))>=0 && aligned.Find(_T("Unaligned"))<0,
				"only a mismatched pair explicitly warns that the displayed frame indices are unaligned");
			doc->SetScene(10,doc->mPane+2,updated); doc->RefreshSelectionMetric(); WaitForAnalysis(doc); doc->mFrmInfoView->UpdateWindow();
			doc->mFrmInfoView->mPairButtons[1].GetWindowText(aligned);
			Check(aligned.Find(_T("Unaligned"))<0 && SameCurrentResult(doc->mPairResults[0],beforeB) && SameCurrentResult(doc->mPairResults[2],beforeD),
				"realigning C removes the warning without changing other current results");
			doc->SetScene(13,doc->mPane+2,updated); doc->RefreshSelectionMetric();
			doc->SetScene(16,doc->mPane+3,updated); doc->RefreshSelectionMetric(); WaitForAnalysis(doc);
			Check(doc->mPairResults[1].state==_T("Ready") && doc->mPairResults[2].state==_T("Ready") &&
				SameCurrentResult(doc->mPairResults[0],beforeB) && scanner->currentMeasureCount(1)==countB,
				"back-to-back independent C/D seeks finish both current results without recalculating B");
			CheckCurrentRequestIsolation(doc);
			doc->RefreshSelectionMetric(); WaitForAnalysis(doc);
			unsigned long long beforeReferenceCounts[3];
			for (int target=1;target<4;++target) beforeReferenceCounts[target-1]=scanner->currentMeasureCount(target);
			doc->SetScene(11,doc->mPane,updated); doc->RefreshSelectionMetric(); WaitForAnalysis(doc);
			for (int target=1;target<4;++target) Check(scanner->currentMeasureCount(target)==beforeReferenceCounts[target-1]+1,
				"A-only seek refreshes every affected candidate current result");
			for (int target=1;target<4;++target) beforeReferenceCounts[target-1]=scanner->currentMeasureCount(target);
			doc->mHasSelection=true; doc->mSelStart=CPoint(0,0); doc->mSelCur=CPoint(7,7); doc->RefreshSelectionMetric(); WaitForAnalysis(doc);
			for (int target=1;target<4;++target) Check(scanner->currentMeasureCount(target)==beforeReferenceCounts[target-1]+1,
				"global ROI change refreshes every pair rather than reusing an old current crop");
			doc->mFrmInfoView->UpdateWindow();
			Check(doc->mFrmInfoView->GetScrollBarCtrl(SB_VERT)->IsWindowVisible(),"narrow aligned footers scroll wrapped ROI details without moving to another image column");
			CheckFooterAlignment(doc);
			for (int target=1;target<4;++target) Check(!doc->mPairResults[target-1].roi.IsEmpty(),"ROI result calculated independently for every pair");
			doc->SelectMetric(METRIC_SSIM_IDX); WaitForAnalysis(doc);
			for (int target=1;target<4;++target) Check(doc->mPairResults[target-1].full.Find(_T("SSIM"))>=0 &&
				!doc->mPairResults[target-1].roi.IsEmpty(),"global SSIM selection updates all current and ROI results");
			const auto beforeSsimB=doc->mPairResults[0],beforeSsimD=doc->mPairResults[2];
			const auto ssimCountB=scanner->currentMeasureCount(1),ssimCountC=scanner->currentMeasureCount(2),ssimCountD=scanner->currentMeasureCount(3);
			doc->SetScene(14,doc->mPane+2,updated); doc->RefreshSelectionMetric(); WaitForAnalysis(doc);
			Check(SameCurrentResult(doc->mPairResults[0],beforeSsimB) && SameCurrentResult(doc->mPairResults[2],beforeSsimD) &&
				scanner->currentMeasureCount(1)==ssimCountB && scanner->currentMeasureCount(2)==ssimCountC+1 && scanner->currentMeasureCount(3)==ssimCountD,
				"C-only SSIM seek retains unrelated current/ROI results and measures only A/C");
			CString crop; crop.Format(_T("crop 8x8 %s"),doc->PairStrategy(2)->CropScore(doc->mPane,doc->mPane+2,METRIC_SSIM_IDX,0,0,7,7).GetString());
			crop.Replace(_T("inf"),_T("\x221e"));
			Check(doc->mPairResults[1].roi==crop,"C-only SSIM crop equals a direct measurement of actual A11/C14 pixels");
			unsigned genB=scanner->pairGeneration(1),genC=scanner->pairGeneration(2),genD=scanner->pairGeneration(3);
			const auto beforeReloadB=doc->mPairResults[0],beforeReloadD=doc->mPairResults[2];
			const auto reloadCountB=scanner->currentMeasureCount(1),reloadCountC=scanner->currentMeasureCount(2),reloadCountD=scanner->currentMeasureCount(3);
			doc->ReloadPane(2);
			Check(SameCurrentResult(doc->mPairResults[0],beforeReloadB) && SameCurrentResult(doc->mPairResults[2],beforeReloadD),
				"C reload leaves unrelated completed current/ROI results visible during analysis restart");
			WaitForAnalysis(doc);
			Check(SameCurrentResult(doc->mPairResults[0],beforeReloadB) && SameCurrentResult(doc->mPairResults[2],beforeReloadD) &&
				scanner->currentMeasureCount(1)==reloadCountB && scanner->currentMeasureCount(2)==reloadCountC+1 && scanner->currentMeasureCount(3)==reloadCountD,
				"C reload recomputes only A/C current/ROI results while retaining A/B and A/D request IDs");
			Check(scanner->pairGeneration(1)==genB && scanner->pairGeneration(2)!=genC && scanner->pairGeneration(3)==genD,
				"replacing C invalidates A/C generation only");
			Check(!scanner->setLazyMetric(0,2,.5,genC,2),"stale asynchronous result cannot publish into replaced A/C generation");
			Check(scanner->decodeCount(0)==20 && scanner->decodeCount(2)==20 && !scanner->decodeCount(1) && !scanner->decodeCount(3),
				"C reload reuses completed A/B and A/D caches without decoding their frames again");
			doc->SelectComparison(3);
			Check(doc->GetOppositePane(doc->mPane)==doc->mPane+3 && doc->GetOppositePane(doc->mPane+2)==doc->mPane,
				"A overlay inspects selected A/D; every candidate overlay references A");
			doc->SetScenes(110); WaitForAnalysis(doc);
			Check(doc->mPane[0].curFrameID<90 && doc->mPane[1].curFrameID<100 && doc->mPane[2].curFrameID<20 && doc->mPane[3].curFrameID==110,
				"synchronized seek does not clamp or fabricate unavailable source frames");
			const auto beforeCloseC=doc->mPairResults[1],beforeCloseD=doc->mPairResults[2];
			doc->CloseSource(1); WaitForAnalysis(doc);
			Check(doc->mPane[1].pView->mCsQMenu.IsWindowEnabled() && doc->mPane[1].pView->IsCommandEnabled(ID_CS_START),
				"closing a source restores empty-slot RAW preconfiguration automatically");
			Check(SameCurrentResult(doc->mPairResults[1],beforeCloseC) && SameCurrentResult(doc->mPairResults[2],beforeCloseD),
				"closing B preserves unrelated completed A/C and A/D current/ROI results");
			doc->mFrmInfoView->UpdateWindow();
			Check(!doc->mFrmInfoView->mPairButtons[0].IsWindowVisible(),"closing B leaves its footer empty rather than moving C under B");
			CheckFooterAlignment(doc);
			Check(doc->ComparisonTargets()==std::vector<int>({2,3}) && doc->TimelineSources()==std::vector<int>({0,2,3}),
				"closing B retains A/C and A/D identities without renaming sources");
			doc->CloseSource(0); Pump(100);
			CString noReference; doc->mFrmInfoView->mPairButtons[1].GetWindowText(noReference);
			Check(noReference.Find(_T("Load reference A"))>=0 && !doc->mFrmInfoView->mPairButtons[1].IsWindowEnabled(),
				"candidate footer explains a missing reference without promoting or selecting another source");
			CheckFooterAlignment(doc);
			Check(doc->ComparisonTargets().empty() && doc->mFrmState.IsEmpty() && doc->mMaxFrames==120,
				"closing reference A clears comparisons but retains other source lanes and frame domain");
			{
				ScopedWindowsUiPaintTestTheme restoreTheme;
				for (auto theme : {q1view::WindowsUiAppearance::Dark,q1view::WindowsUiAppearance::Light}) {
					q1view::WindowsUiAppearanceState().SetAppearance(theme);
					CheckWindowsUiErase(doc->mFrmInfoView->m_hWnd,q1view::WindowsUiColor::Surface,true,Check);
					CheckWindowsUiOwnerDraw(doc->mFrmInfoView->mPairButtons[1].m_hWnd,Check);
				}
			}
			doc->OpenMultiFiles({first,second}); doc->mHasSelection=false; WaitForAnalysis(doc);
		}
		{
			RawTimelineFixture a(3), bSource(3,20), c(3,40,true), dSource(3,60);
			doc->SelectMetric(METRIC_PSNR_IDX);
			doc->mHasSelection=false;
			Check(doc->OpenMultiFiles({a.path,bSource.path,c.path,dSource.path}),"mixed native YUV and RGB sources open with a shared reference"); WaitForAnalysis(doc);
			doc->mFrmInfoView->UpdateWindow();
			CString rgbBody; doc->mFrmInfoView->mPairButtons[1].GetWindowText(rgbBody);
			Check(rgbBody.Find(_T("RGB aggregate"))<0 && doc->mFrmInfoView->mPairButtons[1].mInfoText.Find(_T("Comparison basis: RGB aggregate"))>=0,
				"RGB aggregate explanation is retained only in the corresponding tooltip");
			Check(doc->PairStrategy(1)==doc->mYuvCompare && doc->PairStrategy(2)==doc->mRgbCompare && doc->PairStrategy(3)==doc->mYuvCompare,
				"each pair independently selects native YUV or RGB comparison semantics");
			for (int target=1;target<4;++target) {
				CString direct; doc->PairStrategy(target)->CalMetrics(doc->mPane,doc->mPane+target,0,direct); direct.Replace(_T("inf"),_T("\x221e"));
				Check(doc->mPairResults[target-1].full==direct,"mixed-mode background current result equals direct native strategy result");
			}
			doc->OpenMultiFiles({first,second}); WaitForAnalysis(doc);
		}
		{
			RawTimelineFixture a(2), bSource(2,20,false,32,16), c(2,40,false,16,32), dSource(2,60,false,8,8);
			Check(doc->OpenMultiFiles({a.path,bSource.path,c.path,dSource.path}),"different-resolution sources use existing shared-canvas policy"); WaitForAnalysis(doc);
			Check(doc->mDiffRes && doc->ComparisonTargets().size()==3,"different-resolution resizing retains all three reference comparisons");
			doc->OpenMultiFiles({first,second}); WaitForAnalysis(doc);
		}
		{
			RawTimelineFixture a(2), bSource(2,20), c(2,40), dSource(2,60);
			doc->mHasSelection=true; doc->mSelStart=CPoint(0,0); doc->mSelCur=CPoint(7,7);
			Check(doc->OpenMultiFiles({a.path,bSource.path,c.path,dSource.path}),"small real RGB/RAW inputs prepared for both LPIPS backends");
			for (int metric : {2,3,0,2}) {
				doc->SelectMetric(metric); WaitForAnalysis(doc,30000);
				for (int target=1;target<4;++target) {
					Check(!doc->mPairResults[target-1].full.IsEmpty() && !doc->mPairResults[target-1].roi.IsEmpty(),
						"all three real current/ROI results survive metric/backend switches");
					double cached[QPLANES]; Check(doc->mFileScanThread->copyMetrics(0,metric,cached,target),"every pair has a completed backend-specific sequence score");
					if (qmetric_info_table[metric].lazy) {
						double direct=LpipsEngine::getInstance().distance(doc->mPane[0].rgbBuf,doc->mPane[target].rgbBuf,16,16,ROUNDUP_DWORD(16)*3);
						Check(std::isfinite(direct) && fabs(cached[0]-direct)<1e-6,"real ONNX sequence score matches direct inference for that pair/backend");
					}
				}
				if (qmetric_info_table[metric].lazy) {
					const auto beforeB=doc->mPairResults[0],beforeD=doc->mPairResults[2];
					const auto countB=doc->mFileScanThread->currentMeasureCount(1),countC=doc->mFileScanThread->currentMeasureCount(2),countD=doc->mFileScanThread->currentMeasureCount(3);
					bool updated=false; doc->SetScene(1,doc->mPane+2,updated); doc->RefreshSelectionMetric(); WaitForAnalysis(doc,30000);
					Check(SameCurrentResult(doc->mPairResults[0],beforeB) && SameCurrentResult(doc->mPairResults[2],beforeD) &&
						doc->mFileScanThread->currentMeasureCount(1)==countB && doc->mFileScanThread->currentMeasureCount(2)==countC+1 && doc->mFileScanThread->currentMeasureCount(3)==countD,
						"C-only seek preserves B/D current/ROI results and current-measurement counts for the real selected LPIPS backend");
					double direct=LpipsEngine::getInstance().distance(doc->mPane[0].rgbBuf,doc->mPane[2].rgbBuf,16,16,ROUNDUP_DWORD(16)*3);
					CString expected; expected.Format(_T("%s %.4f"),CString(CA2W(qmetric_info_table[metric].name)).GetString(),direct);
					Check(std::isfinite(direct) && doc->mPairResults[1].full==expected,"independent LPIPS seek measures actual A0/C1 instead of aligned sequence cache");
					double crop=LpipsEngine::getInstance().distance(doc->mPane[0].rgbBuf,doc->mPane[2].rgbBuf,8,8,ROUNDUP_DWORD(16)*3);
					CString cropExpected; cropExpected.Format(_T("crop 8x8 %s(%.4f)"),CString(CA2W(qmetric_info_table[metric].name)).GetString(),crop);
					Check(std::isfinite(crop) && doc->mPairResults[1].roi==cropExpected,"independent LPIPS ROI uses actual A0/C1 crop pixels for the selected backend");
					doc->SetScene(0,doc->mPane+2,updated); doc->RefreshSelectionMetric(); WaitForAnalysis(doc,30000);
				}
			}
			doc->mFileScanThread->setPairError(2,_T("test model failure"),2);
			doc->SelectMetric(2); WaitForAnalysis(doc);
			Check(doc->mFileScanThread->summary(2).metricError[2].IsEmpty(),"reselecting a backend clears stale failure state while retaining valid cached scores");
			doc->SelectMetric(0); doc->mHasSelection=false; doc->OpenMultiFiles({first,second}); WaitForAnalysis(doc);
		}
		fprintf(report, "Actual window DPI: %u; OS text factor: %.3f\n", q1view::WindowsUiDpi(frame->m_hWnd), q1view::WindowsUiSettings().Scale());
		fprintf(report, "ALL COMPARER TYPOGRAPHY CHECKS PASSED\n"); fclose(report); return 0;
	} catch(const std::exception& error) {
		fprintf(report, "ERROR: %s\n", error.what()); fclose(report); return 1;
	}
}
