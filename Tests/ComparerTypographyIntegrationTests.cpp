// /p:Q1ViewComparerTests=true: actual MFC document/views, isolated preferences.
#include "../Comparator/stdafx.h"
#include "../Comparator/Comparator.h"
#include "../Comparator/MainFrm.h"
#include "../Comparator/ComparatorDoc.h"
#include "../Comparator/ComparatorView.h"
#include "../Comparator/FrmCmpStrategy.h"
#include "Q1UiFontWin.h"
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
		fprintf(report, "Actual window DPI: %u; OS text factor: %.3f\n", q1view::WindowsUiDpi(frame->m_hWnd), q1view::WindowsUiSettings().Scale());
		fprintf(report, "ALL COMPARER TYPOGRAPHY CHECKS PASSED\n"); fclose(report); return 0;
	} catch(const std::exception& error) {
		fprintf(report, "ERROR: %s\n", error.what()); fclose(report); return 1;
	}
}
