#include "../QVisionCore/Q1UiHelpWin.h"
#include <cstdio>
#include <cstdlib>
#include "../QVisionCore/Q1UiMenuWin.h"

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

int main()
{
	using namespace q1view;
	TestMenus();
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
