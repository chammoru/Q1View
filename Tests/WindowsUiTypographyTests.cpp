#include "../QVisionCore/Q1UiHelpWin.h"
#include <cstdio>
#include <cstdlib>
#include "../QVisionCore/Q1UiMenuWin.h"
#include "../QVisionCore/Q1UiFrameWin.h"

static void Require(bool value, const char* message)
{ if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); } }

static void TestMenus()
{
	using namespace q1view;
	HMENU root = CreateMenu(), popup = CreatePopupMenu();
	AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(popup), L"&File");
	AppendMenuW(root, MF_STRING | MF_RIGHTJUSTIFY, 103, L"&Help");
	AppendMenuW(popup, MF_STRING, 100, L"&Open 한글...\tCtrl+O");
	AppendMenuW(popup, MF_STRING, 101, L"1920x1080");
	AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
	AppendMenuW(popup, MF_STRING | MF_GRAYED, 102, L"&Unavailable");
	WindowsUiMenus menus;
	menus.Sync(nullptr, root); menus.Sync(nullptr, root);
	Require(menus.Text(popup, 101) == L"1920x1080", "owner-drawn menu retains parser source text across repeated synchronization");
	MENUITEMINFOW info = {sizeof(info)}; info.fMask = MIIM_DATA | MIIM_FTYPE | MIIM_STATE;
	Require(GetMenuItemInfoW(root, 0, TRUE, &info) != FALSE, "styled native menu item exists");
	auto* accessible = reinterpret_cast<MSAAMENUINFO*>(info.dwItemData);
	Require(accessible && accessible->dwMSAASignature == MSAA_MENU_SIG &&
		wcscmp(accessible->pszWText, L"&File") == 0, "native MSAA menu name is preserved");
	MEASUREITEMSTRUCT measure = {}; measure.CtlType = ODT_MENU; measure.itemData = info.dwItemData;
	Require(menus.Measure(&measure) && measure.itemHeight >= 30, "menu row includes 30 DIP breathing room");
	Require(GetMenuItemInfoW(root, 1, TRUE, &info) && (info.fType & MFT_RIGHTJUSTIFY), "right-aligned help stays right-aligned");
	Require(GetMenuItemInfoW(popup, 3, TRUE, &info) && (info.fState & MFS_GRAYED), "disabled item remains disabled");
	CheckMenuRadioItem(popup, 100, 101, 101, MF_BYCOMMAND);
	Require(GetMenuItemInfoW(popup, 1, TRUE, &info) && (info.fState & MFS_CHECKED) && (info.fType & MFT_RADIOCHECK), "radio state remains native");
	LRESULT result = 0;
	Require(menus.MenuChar('f', root, result) && HIWORD(result) == MNC_EXECUTE && LOWORD(result) == 0, "Alt mnemonic selects the original popup");
	Require(!menus.MenuChar('u', popup, result), "disabled mnemonic cannot execute");
	ModifyMenuW(root, 1, MF_BYPOSITION | MF_STRING | MF_RIGHTJUSTIFY, 103, L"&Help updated");
	ModifyMenuW(popup, 101, MF_BYCOMMAND | MF_STRING, 101, L"3840x2160");
	menus.Sync(nullptr, root);
	Require(menus.Text(popup, 101) == L"3840x2160", "dynamic labels survive native ModifyMenu");
	Require(menus.Text(root, 1, true) == L"&Help updated", "dynamic main menu source label is refreshed");
	InsertMenuW(root, 0, MF_BYPOSITION | MF_STRING, 104, L"&Added"); menus.Sync(nullptr, root);
	Require(menus.Text(root, 0, true) == L"&Added" && menus.Text(root, 1, true) == L"&File", "inserting a menu never aliases existing item metadata");
	HDC dc = CreateCompatibleDC(nullptr);
	BITMAPINFO bitmapInfo = {}; bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = 320; bitmapInfo.bmiHeader.biHeight = -30;
	bitmapInfo.bmiHeader.biPlanes = 1; bitmapInfo.bmiHeader.biBitCount = 32; bitmapInfo.bmiHeader.biCompression = BI_RGB;
	void* pixels = nullptr;
	HBITMAP bitmap = CreateDIBSection(dc, &bitmapInfo, DIB_RGB_COLORS, &pixels, nullptr, 0);
	Require(bitmap != nullptr, "menu paint bitmap created");
	HGDIOBJ oldBitmap = SelectObject(dc, bitmap);
	GetMenuItemInfoW(popup, 0, TRUE, &info);
	const auto original = GetCurrentObject(dc, OBJ_FONT);
	DRAWITEMSTRUCT draw = {}; draw.CtlType = ODT_MENU; draw.itemData = info.dwItemData;
	draw.hDC = dc; draw.rcItem = {0, 0, 320, 30};
	Require(menus.Draw(&draw) && GetCurrentObject(dc, OBJ_FONT) == original, "menu painter restores selected font");
	MENUITEMINFOW radio = {sizeof(radio)}; radio.fMask = MIIM_FTYPE; radio.fType = MFT_OWNERDRAW | MFT_RADIOCHECK;
	SetMenuItemInfoW(popup, 0, TRUE, &radio); draw.itemState = ODS_CHECKED;
	Require(menus.Draw(&draw) && GetPixel(dc, 14, 15) == GetSysColor(COLOR_MENUTEXT), "selected radio marker is actually painted, not just stored in metadata");
	SelectObject(dc, oldBitmap); DeleteObject(bitmap); DeleteDC(dc); DestroyMenu(root);
}

struct FrameTestHost {
	q1view::WindowsUiMenus menus;
	q1view::WindowsUiFrame frame;
	static LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM wp, LPARAM lp) {
		auto* host = reinterpret_cast<FrameTestHost*>(GetWindowLongPtrW(window, GWLP_USERDATA));
		if (message == WM_NCCREATE) {
			host = static_cast<FrameTestHost*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
			SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(host));
		}
		LRESULT result = 0;
		if (host && host->frame.Before(message, wp, lp, result)) return result;
		if (host && message == WM_MEASUREITEM && host->menus.Measure(reinterpret_cast<MEASUREITEMSTRUCT*>(lp))) return TRUE;
		if (host && message == WM_DRAWITEM && host->menus.Draw(reinterpret_cast<DRAWITEMSTRUCT*>(lp))) return TRUE;
		result = DefWindowProcW(window, message, wp, lp);
		if (host && message == WM_DESTROY) { host->frame.Destroy(); return result; }
		if (host) host->frame.After(message);
		return result;
	}
};

static void TestFrames()
{
	WNDCLASSW cls = {}; cls.lpfnWndProc = FrameTestHost::Proc; cls.hInstance = GetModuleHandleW(nullptr);
	cls.lpszClassName = L"Q1View.FrameRegressionTest"; RegisterClassW(&cls);
	FrameTestHost host;
	HMENU root = CreateMenu(), popup = CreatePopupMenu();
	AppendMenuW(root, MF_POPUP, reinterpret_cast<UINT_PTR>(popup), L"&File");
	AppendMenuW(root, MF_STRING, 100, L"Resolution 한글");
	AppendMenuW(root, MF_STRING | MF_RIGHTJUSTIFY, 101, L"&Help");
	AppendMenuW(root, MF_STRING | MF_GRAYED | MF_RIGHTJUSTIFY, 102, L"1.00x");
	AppendMenuW(popup, MF_STRING, 103, L"&Open\tCtrl+O");
	HWND window = CreateWindowExW(0, cls.lpszClassName, L"긴 한글 제목 & literal ampersand — Q1View", WS_OVERLAPPEDWINDOW,
		-20000, -20000, 640, 480, nullptr, root, cls.hInstance, &host);
	Require(window != nullptr, "isolated custom-frame host created");
	host.frame.Initialize(window, host.menus);
	Require(host.frame.Menu() == root && host.frame.RetainedMenu() == root, "custom chrome retains the original HMENU and commands");
	Require(host.frame.MenuHost() != nullptr, "menu host uses a real child window");
	if (host.frame.Custom()) {
		Require(GetMenu(window) == nullptr, "native bar detached without destroying its popups");
		WINDOWPOS move = {window, nullptr, -19990, -19990, 640, 480, SWP_NOSIZE | SWP_NOZORDER};
		LRESULT moveResult = 0; host.frame.Before(WM_WINDOWPOSCHANGING, 0, reinterpret_cast<LPARAM>(&move), moveResult);
		Require((move.flags & SWP_NOCOPYBITS) != 0, "custom client origin does not reuse native move pixels");
		Require(host.frame.TitleHeight() >= 32 && host.frame.Height() >= 62, "title and main-menu hit areas have measured breathing room");
		HWND file = GetDlgItem(host.frame.MenuHost(), 1);
		wchar_t name[80] = {}; GetWindowTextW(file, name, 80);
		Require(wcscmp(name, L"&File") == 0, "real menu button exposes its accessible name");
		Require(!IsWindowEnabled(GetDlgItem(host.frame.MenuHost(), 4)), "disabled zoom label cannot receive input");
		for (int width : {200, 320, 640, 1920}) {
			SetWindowPos(window, nullptr, 0, 0, width, 480, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
			host.frame.Layout(); RECT client; GetClientRect(window, &client);
			const auto& rects = host.frame.MenuRects();
			for (size_t i = 0; i < rects.size(); ++i) {
				Require(rects[i].left >= 0 && rects[i].right <= client.right && rects[i].bottom <= host.frame.Height() - host.frame.TitleHeight(), "narrow-window menu button stays within its wrapped row");
				for (size_t j = i + 1; j < rects.size(); ++j) { RECT overlap; Require(!IntersectRect(&overlap, &rects[i], &rects[j]), "wrapped and right-aligned menu buttons never overlap"); }
			}
		}
		ModifyMenuW(root, 1, MF_BYPOSITION | MF_STRING, 100, L"3840x2160"); host.frame.Sync();
		GetWindowTextW(GetDlgItem(host.frame.MenuHost(), 2), name, 80);
		Require(wcscmp(name, L"3840x2160") == 0, "document-driven dynamic menu labels update in custom chrome");
		RECT client; GetClientRect(window, &client);
		const UINT dpi = q1view::WindowsUiDpi(window);
		POINT maximum = {client.right - q1view::WindowsUiPixels(69, dpi), host.frame.TitleHeight() / 2};
		ClientToScreen(window, &maximum);
		const LRESULT hit = SendMessageW(window, WM_NCHITTEST, 0, MAKELPARAM(maximum.x, maximum.y));
		std::printf("Caption maximize hit: %lld\n", static_cast<long long>(hit));
		Require(hit == HTMAXBUTTON, "native maximize area keeps its non-client hit code for click and Snap layouts");
		HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
		HBITMAP image = CreateCompatibleBitmap(screen, 1920, 80); ReleaseDC(nullptr, screen);
		const auto old = SelectObject(dc, image);
		host.frame.Paint(dc); const DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
		for (int i = 0; i < 100; ++i) host.frame.Paint(dc);
		Require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == before, "buffered caption painting has no accumulating GDI resources");
		SelectObject(dc, old); DeleteObject(image); DeleteDC(dc);
		const LONG_PTR style = GetWindowLongPtrW(window, GWL_STYLE);
		host.frame.SetMenu(nullptr); SetWindowLongPtrW(window, GWL_STYLE, style & ~WS_CAPTION); host.frame.Layout();
		Require(host.frame.Height() == 0 && !IsWindowVisible(host.frame.MenuHost()), "full screen has no custom title or menu reservation");
		SetWindowLongPtrW(window, GWL_STYLE, style); host.frame.SetMenu(root);
		Require(host.frame.Menu() == root && host.frame.Height() >= 62, "leaving full screen restores the same menu instead of recreating it");
	}
	DestroyWindow(window); if (IsMenu(root)) DestroyMenu(root);
	std::puts("Windows custom-frame tests passed (native menu retention, accessible buttons, wrapping, full screen, caption GDI lifetime).");
}

int main()
{
	using namespace q1view;
	TestMenus();
	TestFrames();
	HDC dc = CreateCompatibleDC(nullptr);
	Require(dc != nullptr, "GDI test DC");
	const std::vector<WindowsUiHelpRow> rows = {
		{L"Drag & Drop", L"긴 한글 파일명과 English 설명을 함께 표시합니다."},
		{L"Mouse Wheel", L"Zoom in or out; high zoom shows pixel values"},
		{L"Page Up/Down", L"Previous or next file"},
		{L"I", L"Cycle scaling: Auto -> Smooth -> Pixel Exact"}
	};
	const HGDIOBJ original = GetCurrentObject(dc, OBJ_FONT);
	WindowsUiTextFontFamily(); WindowsUiNumericFontFamily();
	GetStockObject(DEFAULT_GUI_FONT); // warm up lazily created OS stock resources
	{
		WindowsUiFontCache warm;
		RECT bounds = {0, 0, 320, 320}; int offset = 0;
		DrawWindowsUiHelp(dc, bounds, L"Help", L"Version", rows, warm, 96, 1.0, offset);
	} // GDI lazily creates a DC clipping region on the first clipped draw.
	const DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
	{
		WindowsUiFontCache fonts;
		HFONT specimenBody = fonts.Get(WindowsUiFontRole::Body, 96, 1.0);
		HGDIOBJ previous = SelectObject(dc, specimenBody);
		wchar_t actualFace[LF_FACESIZE] = {};
		GetTextFaceW(dc, LF_FACESIZE, actualFace);
		Require(wcsstr(actualFace, L"Pretendard") != nullptr, "bundled UI font actually resolves, not just a requested family name");
		const wchar_t* mixed = L"한글English";
		WORD glyphs[9] = {};
		Require(GetGlyphIndicesW(dc, mixed, 9, glyphs, GGI_MARK_NONEXISTING_GLYPHS) != GDI_ERROR, "mixed script glyph query");
		for (WORD glyph : glyphs) Require(glyph != 0xffff, "Korean/English glyphs exist in bundled font");
		SelectObject(dc, previous);
		for (UINT dpi : {96u, 120u, 144u, 192u}) for (double scale : {1.0, 1.25, 1.5, 2.0, 2.25}) {
			HFONT body = fonts.Get(WindowsUiFontRole::Body, dpi, scale);
			LOGFONT actual = {}; GetObject(body, sizeof(actual), &actual);
			Require(actual.lfHeight == -WindowsUiPixels(14, dpi, scale), "DIP character height, not points/cell height");
			Require(lstrcmp(actual.lfFaceName, WindowsUiTextFontFamily()) == 0, "prose uses shared UI family");
			Require(fonts.Get(WindowsUiFontRole::Body, dpi, scale) == body, "same-role font is reused");
			{
				WindowsUiDcState frame(dc);
				SelectObject(dc, body);
			}
			Require(GetCurrentObject(dc, OBJ_FONT) == original, "persistent render DC releases selected role font before replacement");
			HFONT numeric = fonts.Get(WindowsUiFontRole::Numeric, dpi, scale);
			GetObject(numeric, sizeof(actual), &actual);
			Require(lstrcmp(actual.lfFaceName, WindowsUiNumericFontFamily()) == 0, "numeric font has explicit fallback");
			for (int width : {320, 640, 960}) {
				RECT bounds = {0, 0, WindowsUiPixels(width, dpi), WindowsUiPixels(320, dpi)};
				const auto layout = MeasureWindowsUiHelp(dc, bounds, rows, fonts, dpi, scale);
				Require(layout.rows.size() == rows.size(), "all help rows survive measurement");
				LONG previousBottom = 0;
				for (const auto& row : layout.rows) {
					Require(row.key.top >= previousBottom, "wrapped rows do not overlap");
					Require(row.key.right < row.description.left && row.description.right == bounds.right, "independent measured columns");
					previousBottom = row.key.bottom;
				}
				int offset = 100000;
				const int maximum = DrawWindowsUiHelp(dc, bounds, L"도움말 Help", L"Version 0.0.0.dev", rows, fonts, dpi, scale, offset);
				Require(offset == maximum, "scroll clamps to end, exposing final row");
				Require(GetCurrentObject(dc, OBJ_FONT) == original, "help renderer restores selected GDI font");
			}
		}
		for (int i = 0; i < 1000; ++i) {
			const UINT dpi = i % 2 ? 96 : 192;
			for (int role = 0; role < int(WindowsUiFontRole::Count); ++role)
				fonts.Get(static_cast<WindowsUiFontRole>(role), dpi, i % 3 ? 1.0 : 2.25);
		}
		Require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= before + 16, "font cache remains bounded across settings changes");
	}
	const DWORD after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
	std::printf("GDI objects: before=%lu after=%lu\n", before, after);
	Require(after <= before, "cache releases owned GDI fonts");
	Require(WindowsUiSettings().Scale() >= 1.0, "system text scale has a safe fallback");
	MSG message = {}; message.message = WM_KEYDOWN; message.wParam = VK_NEXT;
	int offset = 0;
	Require(WindowsUiHelpNavigation(&message, 200, 500, offset) && offset == 200, "Page Down scrolls help instead of the image");
	message.wParam = VK_END;
	Require(WindowsUiHelpNavigation(&message, 200, 500, offset) && offset == 500, "End exposes the final help row");
	message.wParam = 'E';
	Require(!WindowsUiHelpNavigation(&message, 200, 500, offset), "help does not reroute unrelated shortcuts");
	DeleteDC(dc);
	std::puts("Windows UI typography tests passed (4 DPIs x 5 text scales x 3 widths, cache lifetime).");
	return 0;
}
