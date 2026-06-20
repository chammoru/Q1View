// ThumbnailPane.cpp : implementation of CThumbnailPane
//
// A CListCtrl thumbnail browser with two layouts driven by one Ctrl+wheel
// "view size" control: step 0 is the compact list (report view, small thumb at
// the left of each row); steps >= 1 are gallery grids (icon view) with growing
// thumbnails. Thumbnails are produced on a small worker pool, preferring the
// Windows shell thumbnail cache (IShellItemImageFactory -- EXIF thumbnails and
// thumbcache.db, the same path Explorer uses) and falling back to an OpenCV
// decode. Only the thumbnails on (or near) screen are decoded, so a folder with
// thousands of images stays responsive while scrolling.

#include "stdafx.h"
#include "Viewer.h"
#include "ThumbnailPane.h"
#include "ViewerDoc.h"

#include "QViewerCmn.h"
#include "QCvUtil.h"

#include <opencv2/imgproc/imgproc.hpp>

#include "Shlwapi.h"
#include <shlobj.h>      // SHCreateItemFromParsingName, IShellItemImageFactory
#include <algorithm>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "msimg32.lib")   // AlphaBlend (selected-tile lift)

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static CString ExtensionOf(const CString &path);   // defined below

// List (report) step thumbnail edge -- the smallest, default view.
static const int kListThumb = 44;
// Grid steps pack a whole number of columns across the pane width; the tile edge
// is the pane width divided by the column count, minus a thin gutter so adjacent
// thumbnails stay visually separated (issue #80). Index 0 is the list step
// (unused here); higher steps zoom in by showing fewer, larger columns.
static const int kGridCols[] = { 0, 5, 4, 3, 2, 1 };
// Hairline gutter (px) between grid tiles -- a thin separator in the surface
// colour so similar-coloured neighbours don't blend (issue #80).
static const int kGridGutter = 1;
// Default step 0 = the compact list (smallest thumbnail). Ctrl+wheel up grows
// into the gallery grids.
static const int kDefaultStep = 0;
// Debounced timers: rescan visible after scrolling; re-fit the grid after a resize.
static const UINT_PTR kScanTimerId = 0x7100;
static const UINT_PTR kRelayoutTimerId = 0x7101;

BEGIN_MESSAGE_MAP(CThumbnailPane, CListCtrl)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_MOUSEWHEEL()
	ON_WM_VSCROLL()
	ON_WM_KEYDOWN()
	ON_WM_TIMER()
	ON_NOTIFY_REFLECT(NM_DBLCLK, &CThumbnailPane::OnItemActivate)
	ON_NOTIFY_REFLECT(NM_RETURN, &CThumbnailPane::OnItemActivate)
	ON_NOTIFY_REFLECT(LVN_GETINFOTIP, &CThumbnailPane::OnGetInfoTip)
	ON_NOTIFY_REFLECT(NM_CUSTOMDRAW, &CThumbnailPane::OnCustomDraw)
	ON_MESSAGE(WM_THUMB_READY, &CThumbnailPane::OnThumbReady)
	ON_MESSAGE(WM_DRAWER_ACTIVATE, &CThumbnailPane::OnActivatePosted)
END_MESSAGE_MAP()

CThumbnailPane::CThumbnailPane()
: mThumb(kListThumb)
, mViewStep(kDefaultStep)
, mSlideWidth(0)
, mLoadingImg(-1)
, mFolderImg(-1)
, mCacheCap(512)
, mStop(false)
, mGen(0)
, mWorkerStarted(false)
{
	LoadViewStep();
}

CThumbnailPane::~CThumbnailPane()
{
	Shutdown();
}

int CThumbnailPane::ViewStepCount()
{
	return _countof(kGridCols);
}

int CThumbnailPane::GridColsForStep(int step)
{
	if (step < 1) return 1;
	if (step >= ViewStepCount()) step = ViewStepCount() - 1;
	return kGridCols[step];
}

// List: a fixed small edge. Grid: divide the current pane width evenly among the
// step's columns so the tiles fill the width and butt together.
void CThumbnailPane::RecalcThumbSize()
{
	if (!IsGrid()) {
		mThumb = kListThumb;
		return;
	}
	int cols = GridColsForStep(mViewStep);
	// While sliding open/closed, lay out for the drawer's final width so the tile
	// size stays fixed and the content isn't repopulated at each intermediate width.
	// Otherwise use the full pane width (the window rect spans the scrollbar region,
	// so it doesn't change when the scrollbar shows or hides).
	int w = mSlideWidth;
	if (w <= 0) {
		CRect rc;
		GetWindowRect(&rc);
		w = rc.Width();
	}
	if (w <= 0)
		w = cols * 96;                       // no window yet; pick a sane default
	// Always reserve the vertical scrollbar's width so the tile size and column count
	// don't change when the scrollbar appears or disappears -- e.g. a view-step change
	// that alters the row count would otherwise reflow the whole grid by a
	// scrollbar-width as the bar toggled, a visible jolt.
	w = std::max(cols * 16, w - ::GetSystemMetrics(SM_CXVSCROLL));
	// The cell (tile + one gutter) tiles the width; the image fills the cell minus
	// the gutter, so a hairline of background shows between neighbours.
	mThumb = std::max(16, w / cols - kGridGutter);
}

void CThumbnailPane::LoadViewStep()
{
	int step = AfxGetApp()->GetProfileInt(_T("Drawer"), _T("ThumbStep"), kDefaultStep);
	if (step < 0) step = 0;
	if (step >= ViewStepCount()) step = ViewStepCount() - 1;
	mViewStep = step;
	// mThumb is finalized by RecalcThumbSize() once the pane has a width.
}

void CThumbnailPane::SaveViewStep() const
{
	AfxGetApp()->WriteProfileInt(_T("Drawer"), _T("ThumbStep"), mViewStep);
}

BOOL CThumbnailPane::CreatePane(CWnd *pParent, UINT nID)
{
	// Owner-draw report rows for the list step; icon view (standard draw) for the
	// grid steps. The view style is finalized in ApplyViewStep() after creation.
	// LVS_AUTOARRANGE keeps the grid (icon view) items reflowed into as many
	// columns as the drawer width allows; it is ignored in the report (list) step.
	DWORD style = WS_CHILD | WS_VISIBLE | LVS_OWNERDRAWFIXED |
		LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER |
		LVS_AUTOARRANGE;
	return Create(style, CRect(0, 0, 0, 0), pParent, nID);
}

int CThumbnailPane::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CListCtrl::OnCreate(lpCreateStruct) == -1)
		return -1;

	// No border/edge lines around the drawer (top or sides).
	ModifyStyle(WS_BORDER, 0);
	ModifyStyleEx(WS_EX_CLIENTEDGE | WS_EX_STATICEDGE | WS_EX_WINDOWEDGE, 0,
		SWP_FRAMECHANGED);

	SetExtendedStyle(GetExtendedStyle() |
		LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_INFOTIP);

	mImages.Create(mThumb, mThumb, ILC_COLOR32, 0, 64);
	SetImageList(&mImages, LVSIL_SMALL);
	SetImageList(&mImages, LVSIL_NORMAL);

	InsertColumn(0, _T(""), LVCFMT_LEFT, 0);

	mLabelFont.CreatePointFont(85, _T("Segoe UI"));
	SetFont(&mLabelFont);

	LOGFONT lf = {};
	lstrcpyn(lf.lfFaceName, _T("Segoe UI"), LF_FACESIZE);
	lf.lfHeight = -(mThumb / 4);
	lf.lfWeight = FW_BOLD;
	mExtFont.CreateFontIndirect(&lf);

	SetBkColor(Q1UI_COLOR_SURFACE_ALT);
	SetTextBkColor(Q1UI_COLOR_SURFACE_ALT);
	SetTextColor(Q1UI_COLOR_TEXT);

	// A small pool of decode workers fills the visible grid quickly. Cap it so a
	// big folder scroll doesn't saturate every core.
	unsigned hw = std::thread::hardware_concurrency();
	unsigned n = hw ? std::min<unsigned>(hw, 4u) : 2u;
	mStop = false;
	for (unsigned i = 0; i < n; i++)
		mWorkers.emplace_back(&CThumbnailPane::WorkerLoop, this);
	mWorkerStarted = true;

	// Apply the persisted step (sets report/icon style + spacing).
	ApplyViewStep(mViewStep, false);

	return 0;
}

void CThumbnailPane::OnSize(UINT nType, int cx, int cy)
{
	CListCtrl::OnSize(nType, cx, cy);
	if (!IsGrid()) {
		// Single column fills the client width so there is no horizontal scrollbar.
		SetColumnWidth(0, cx);
	} else if (mSlideWidth <= 0) {
		// The drawer is user-resizable, so the per-tile width (= pane width / cols)
		// changes with it; re-fit the tiles once the resize settles. Suppressed while
		// an open/close slide is in flight -- the tiles are already sized for the
		// final width, and the icon view reveals them as the column grows/shrinks
		// without resizing or repopulating each frame.
		ScheduleRelayout();
	}
	ShowScrollBar(SB_HORZ, FALSE);
	ScheduleVisibleScan();
}

// ---------------------------------------------------------------------------
// View size (Ctrl+wheel): list step 0, grid steps 1..N
// ---------------------------------------------------------------------------

void CThumbnailPane::ApplyViewStep(int step, bool persist)
{
	if (!GetSafeHwnd())
		return;
	if (step < 0) step = 0;
	if (step >= ViewStepCount()) step = ViewStepCount() - 1;

	// Remember the selected image so it stays selected across the mode switch.
	CString current;
	int sel = GetNextItem(-1, LVNI_SELECTED);
	if (sel >= 0 && sel < (int)mEntries.size() && mEntries[sel].kind == ENTRY_FILE)
		current = mEntries[sel].path;

	mViewStep = step;
	if (persist)
		SaveViewStep();

	// Switching size invalidates in-flight (old-size) decodes; bump the
	// generation so their results are dropped, and clear the queue.
	mGen++;
	{
		std::lock_guard<std::mutex> lk(mMutex);
		mTasks.clear();
	}

	// Report view owns its row layout via owner-draw; icon view draws standard
	// tiles (thumbnail only), so the owner-draw bit must be off there.
	if (IsGrid())
		ModifyStyle(LVS_TYPEMASK | LVS_OWNERDRAWFIXED, LVS_ICON | LVS_AUTOARRANGE);
	else
		ModifyStyle(LVS_TYPEMASK, LVS_REPORT | LVS_OWNERDRAWFIXED);

	// Re-list the folder: the grid steps show only image tiles (no folders or
	// names) while the list step shows folders + names, and the tile size differs
	// per step, so the simplest correct refresh is a full repopulate.
	Populate(mFolder, current);
}

BOOL CThumbnailPane::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	if (nFlags & MK_CONTROL) {
		// Ctrl+wheel grows/shrinks the thumbnails (and crosses list<->grid),
		// like Explorer. One step per notch.
		int step = mViewStep + (zDelta > 0 ? 1 : -1);
		if (step < 0) step = 0;
		if (step >= ViewStepCount()) step = ViewStepCount() - 1;
		if (step != mViewStep)
			ApplyViewStep(step, true);
		return TRUE;   // consume; don't also scroll
	}

	BOOL ret = CListCtrl::OnMouseWheel(nFlags, zDelta, pt);
	ScheduleVisibleScan();
	return ret;
}

void CThumbnailPane::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pScrollBar)
{
	CListCtrl::OnVScroll(nSBCode, nPos, pScrollBar);
	ScheduleVisibleScan();
}

void CThumbnailPane::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// PgUp/PgDn step the selection to the previous/next file, matching the main
	// view's file navigation, instead of the list control's page scroll -- which in
	// the grid (icon) view pages by the whole list and so just duplicated Home/End.
	// (Selection only; Enter/double-click still opens.)
	if (nChar == VK_PRIOR || nChar == VK_NEXT) {
		StepFile(nChar == VK_NEXT);
		ScheduleVisibleScan();
		return;
	}

	CListCtrl::OnKeyDown(nChar, nRepCnt, nFlags);
	switch (nChar) {
	case VK_UP: case VK_DOWN: case VK_LEFT: case VK_RIGHT:
	case VK_HOME: case VK_END:
		ScheduleVisibleScan();
		break;
	default:
		break;
	}
}

// Move the selection one file earlier/later in list order, skipping folders and
// the ".." entry so PgUp/PgDn walk only files (like the main view). Clamps at the
// first/last file; does not open the file.
void CThumbnailPane::StepFile(bool next)
{
	int n = (int)mEntries.size();
	if (n == 0)
		return;

	int cur = GetNextItem(-1, LVNI_SELECTED);
	if (cur < 0)
		cur = GetNextItem(-1, LVNI_FOCUSED);

	int target = -1;
	if (next) {
		for (int i = cur + 1; i < n; i++)           // cur < 0 -> starts at the first row
			if (mEntries[i].kind == ENTRY_FILE) { target = i; break; }
	} else {
		int from = (cur < 0) ? n - 1 : cur - 1;     // no selection -> start from the last row
		for (int i = from; i >= 0; i--)
			if (mEntries[i].kind == ENTRY_FILE) { target = i; break; }
	}
	if (target < 0)
		return;                                     // already at the first/last file

	SetItemState(target, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	EnsureVisible(target, FALSE);
}

void CThumbnailPane::ScheduleVisibleScan()
{
	if (GetSafeHwnd())
		SetTimer(kScanTimerId, 90, NULL);   // coalesce rapid scroll into one scan
}

void CThumbnailPane::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == kScanTimerId) {
		KillTimer(kScanTimerId);
		QueueVisibleThumbs();
		return;
	}
	if (nIDEvent == kRelayoutTimerId) {
		KillTimer(kRelayoutTimerId);
		RelayoutGrid();
		return;
	}
	CListCtrl::OnTimer(nIDEvent);
}

void CThumbnailPane::ScheduleRelayout()
{
	if (GetSafeHwnd())
		SetTimer(kRelayoutTimerId, 120, NULL);   // coalesce rapid resizes into one re-fit
}

// The drawer is user-resizable, so the grid's column width (and thus tile size)
// changes as it is dragged. Repopulate at the new tile size only when the edge
// actually changed; otherwise just reflow.
void CThumbnailPane::RelayoutGrid()
{
	if (!IsGrid() || !GetSafeHwnd())
		return;

	int old = mThumb;
	RecalcThumbSize();
	if (mThumb == old) {
		Arrange(LVA_DEFAULT);
		QueueVisibleThumbs();
		return;
	}

	CString current;
	int sel = GetNextItem(-1, LVNI_SELECTED);
	if (sel >= 0 && sel < (int)mEntries.size() && mEntries[sel].kind == ENTRY_FILE)
		current = mEntries[sel].path;
	Populate(mFolder, current);
}

// Pin every grid tile to its final N-column position and stop the icon view from
// auto-arranging, so that resizing the pane no longer reflows the columns. The
// open/close slide then just reveals or clips this fixed layout instead of
// re-packing the tiles into 1, 2, 3 ... N columns as the width passes through.
void CThumbnailPane::FreezeGridLayout()
{
	if (!IsGrid() || !GetSafeHwnd())
		return;
	int n = GetItemCount();
	if (n == 0) {
		ModifyStyle(LVS_AUTOARRANGE, 0);
		return;
	}
	// The items are still auto-arranged here, so item 0 sits at the icon view's own
	// top-left inset. Anchor the frozen grid to that same inset so it lines up
	// exactly with the auto-arranged layout EndSlide restores -- otherwise every row
	// snapped by the inset (a couple of px) when the slide ended.
	CPoint base(0, 0);
	GetItemPosition(0, &base);
	ModifyStyle(LVS_AUTOARRANGE, 0);
	int cols = GridColsForStep(mViewStep);
	int cell = mThumb + kGridGutter;          // same pitch SetIconSpacing uses
	for (int i = 0; i < n; i++)
		SetItemPosition(i, CPoint(base.x + (i % cols) * cell, base.y + (i / cols) * cell));
}

// The drawer is about to slide open or closed. Lock the grid to its final width and
// column layout for the whole animation: the tiles keep one size (mSlideWidth feeds
// RecalcThumbSize) and one column count (FreezeGridLayout pins them), so the slide
// reveals/hides a stable grid instead of resizing and reflowing it frame by frame.
void CThumbnailPane::BeginSlide(int targetWidth)
{
	// The drawer's final pane width; RecalcThumbSize reserves the scrollbar from it,
	// the same way it does at rest, so the slide and resting layouts match exactly.
	mSlideWidth = std::max(1, targetWidth);
	// Freeze now if the grid already holds items (reopen / closing). On a fresh open
	// the items are inserted right after, by SetCurrentFile -> Populate, which
	// freezes them itself while sliding (mSlideWidth > 0).
	if (IsGrid() && GetSafeHwnd() && GetItemCount() > 0)
		FreezeGridLayout();
}

// The slide finished: drop the locks and settle into the real (now final) width,
// restoring normal auto-arrange so later resizes (divider drags) reflow as usual.
void CThumbnailPane::EndSlide()
{
	mSlideWidth = 0;
	if (IsGrid() && GetSafeHwnd()) {
		// Re-enable auto-arrange for later resizes. The frozen tiles already sit at
		// their canonical positions, so no Arrange() is needed here. Scroll the
		// current image into view now that the pane is full width -- the
		// EnsureVisible during the (zero-width) slide couldn't.
		ModifyStyle(0, LVS_AUTOARRANGE);
		int sel = GetNextItem(-1, LVNI_SELECTED);
		if (sel >= 0)
			EnsureVisible(sel, FALSE);
		QueueVisibleThumbs();
	}
}

// Queue decodes only for files at (or one screen beyond) the visible region, so
// scrolling never waits behind a folder-sized backlog. Cached thumbnails are
// applied inline; raw/folders never decode.
void CThumbnailPane::QueueVisibleThumbs()
{
	int n = (int)mEntries.size();
	if (n == 0 || !GetSafeHwnd())
		return;

	CRect rc0;
	if (!GetItemRect(0, &rc0, LVIR_BOUNDS))
		return;

	CRect client;
	GetClientRect(&client);

	// Row/column pitch used to map the scroll offset to a band of item indices.
	// In the grid (icon view) the LVIR_BOUNDS height includes the empty label line
	// below each tile, which is taller than the actual cell pitch; using it as the
	// row stride undercounts firstRow, so after a large Home/End jump the queued
	// band lands above the real viewport and the newly revealed tiles never decode
	// (issue #84). The grid lays tiles out on the square spacing we set, so use that
	// pitch. The list (report) step has one column whose row height is the bounds
	// height, so keep rc0 there.
	int cw, chh;
	if (IsGrid()) {
		cw = chh = std::max(1, mThumb + kGridGutter);
	} else {
		cw = std::max(1, (int)rc0.Width());
		chh = std::max(1, (int)rc0.Height());
	}
	int scrollY = std::max(0, (int)-rc0.top);        // pixels scrolled past item 0
	int cols = IsGrid() ? std::max(1, client.Width() / cw) : 1;
	int firstRow = scrollY / chh;
	int rowsVisible = client.Height() / chh + 2;
	int look = rowsVisible;                           // ~one screen of lookahead

	int first = std::max(0, (firstRow - look) * cols);
	int last = std::min(n - 1, (firstRow + rowsVisible + look) * cols + cols - 1);

	for (int i = first; i <= last; i++) {
		Entry &e = mEntries[i];
		if (e.kind != ENTRY_FILE || e.queued)
			continue;
		if (e.badge)                                  // raw + non-thumbnailable types
			continue;
		if (e.img >= 0 && e.img != mLoadingImg)
			continue;                                 // already has a real thumb

		HBITMAP cached = CacheFind(e.path);
		if (cached) {
			int idx = AddImageCopy(cached);
			e.img = idx;
			SetItem(i, 0, LVIF_IMAGE, NULL, idx, 0, 0, 0);
			RedrawItems(i, i);
		} else {
			e.queued = true;
			QueueThumb(i, e.path);
		}
	}
}

void CThumbnailPane::DrawItem(LPDRAWITEMSTRUCT dis)
{
	// Owner-draw is only used in the list (report) step; the grid steps draw
	// standard icon tiles.
	int i = (int)dis->itemID;
	if (i < 0 || i >= (int)mEntries.size())
		return;

	CDC *pDC = CDC::FromHandle(dis->hDC);
	CRect rc = dis->rcItem;
	const Entry &e = mEntries[i];
	bool sel = (dis->itemState & ODS_SELECTED) != 0;

	pDC->FillSolidRect(rc, sel ? Q1UI_COLOR_ACCENT_SOFT : Q1UI_COLOR_SURFACE_ALT);
	pDC->SetBkMode(TRANSPARENT);

	const int pad = 4;
	int box = rc.Height();          // square cell on the left == row height
	int x = rc.left + pad;

	if (e.kind == ENTRY_FILE) {
		CString ext = ExtensionOf(e.path);
		if (e.badge) {
			// Raw formats and non-thumbnailable types: extension badge.
			CRect badge(x, rc.top + 3, x + box - 6, rc.bottom - 3);
			CBrush fill(Q1UI_COLOR_ACCENT_SOFT);
			CPen   pen(PS_SOLID, 1, Q1UI_COLOR_ACCENT);
			CBrush *ob = pDC->SelectObject(&fill);
			CPen   *op = pDC->SelectObject(&pen);
			pDC->RoundRect(badge, CPoint(6, 6));
			pDC->SelectObject(ob);
			pDC->SelectObject(op);

			CString tag = ext; tag.MakeUpper();
			pDC->SetTextColor(Q1UI_COLOR_ACCENT);
			CFont *of = pDC->SelectObject(&mExtFont);
			pDC->DrawText(tag, badge, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
			pDC->SelectObject(of);
		} else {
			int idx = (e.img >= 0) ? e.img : mLoadingImg;
			if (idx >= 0)
				mImages.Draw(pDC, idx, CPoint(x, rc.top + (box - mThumb) / 2), ILD_NORMAL);
		}
		x += box + pad;

		pDC->SetTextColor(Q1UI_COLOR_TEXT);
		CFont *of = pDC->SelectObject(&mLabelFont);
		CRect tr(x, rc.top, rc.right - 2, rc.bottom);
		pDC->DrawText(PathFindFileName(e.path), tr,
			DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
		pDC->SelectObject(of);
	} else {
		// Folders/parent: no left box, so the name fills the row; the accent
		// colour and bold weight distinguish folders from files.
		CString name;
		if (e.kind == ENTRY_PARENT) {
			name = _T("..");
		} else {
			name = e.path;
			if (!name.IsEmpty() && name[name.GetLength() - 1] == _T('\\'))
				name = name.Left(name.GetLength() - 1);
			CString leaf = PathFindFileName(name);
			name = leaf.IsEmpty() ? e.path : leaf;
		}

		pDC->SetTextColor(Q1UI_COLOR_ACCENT);
		CFont *of = pDC->SelectObject(&mExtFont);
		CRect tr(rc.left + pad, rc.top, rc.right - 2, rc.bottom);
		pDC->DrawText(name, tr,
			DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
		pDC->SelectObject(of);
	}
}

void CThumbnailPane::OnDestroy()
{
	Shutdown();
	CListCtrl::OnDestroy();
}

void CThumbnailPane::Shutdown()
{
	if (mWorkerStarted) {
		{
			std::lock_guard<std::mutex> lk(mMutex);
			mStop = true;
		}
		mCv.notify_all();
		for (std::thread &t : mWorkers) {
			if (t.joinable())
				t.join();
		}
		mWorkers.clear();
		mWorkerStarted = false;

		// Drain any results the workers posted but we never processed.
		if (GetSafeHwnd()) {
			MSG msg;
			while (::PeekMessage(&msg, m_hWnd, WM_THUMB_READY, WM_THUMB_READY, PM_REMOVE)) {
				Result *r = reinterpret_cast<Result *>(msg.wParam);
				if (r) {
					if (r->hbmp)
						::DeleteObject(r->hbmp);
					delete r;
				}
			}
		}
	}
	CacheClear();
}

// ---------------------------------------------------------------------------
// Extension classification
// ---------------------------------------------------------------------------

bool CThumbnailPane::IsDecodableExt(const CString &ext)
{
	static const LPCTSTR kExts[] = {
		_T("bmp"), _T("jpg"), _T("jpeg"), _T("png"), _T("tif"), _T("tiff"),
		_T("webp"), _T("heic"), _T("heif"), _T("hif"), _T("avif")
	};
	for (int i = 0; i < _countof(kExts); i++)
		if (ext.CompareNoCase(kExts[i]) == 0)
			return true;
	return false;
}

bool CThumbnailPane::IsVideoExt(const CString &ext)
{
	static const LPCTSTR kExts[] = {
		_T("mp4"), _T("m4v"), _T("mov"), _T("avi"), _T("mkv"), _T("webm"),
		_T("wmv"), _T("mpg"), _T("mpeg"), _T("flv"), _T("3gp"), _T("ts"), _T("m2ts")
	};
	for (int i = 0; i < _countof(kExts); i++)
		if (ext.CompareNoCase(kExts[i]) == 0)
			return true;
	return false;
}

// Images and videos can be turned into a real pixel thumbnail (the Windows shell
// thumbnails videos, EXIF-embedded JPEGs, etc.; OpenCV covers the rest). Raw
// formats (yuv/rgb -- no known pixel layout) and every other type fall back to an
// extension badge.
bool CThumbnailPane::IsThumbnailable(const CString &ext)
{
	return IsDecodableExt(ext) || IsVideoExt(ext);
}

static CString ExtensionOf(const CString &path)
{
	int dot = path.ReverseFind(_T('.'));
	if (dot < 0)
		return _T("");
	return path.Mid(dot + 1);
}

// ---------------------------------------------------------------------------
// Population
// ---------------------------------------------------------------------------

void CThumbnailPane::SetCurrentFile(LPCTSTR lpszPath)
{
	if (!GetSafeHwnd() || lpszPath == NULL || lpszPath[0] == _T('\0'))
		return;

	CString path(lpszPath);
	path.Replace(_T('/'), _T('\\'));

	int slash = path.ReverseFind(_T('\\'));
	if (slash < 0)
		return;
	CString folder = path.Left(slash + 1);

	if (folder.CompareNoCase(mFolder) == 0 && GetItemCount() > 0) {
		SelectByPath(path);
		return;
	}

	Populate(folder, path);
}

// Returns the parent folder (with trailing '\\') of a folder, or "" if it has
// none (e.g. a drive root such as "C:\").
static CString ParentFolderOf(const CString &folder)
{
	CString f = folder;
	if (f.GetLength() > 0 && f[f.GetLength() - 1] == _T('\\'))
		f = f.Left(f.GetLength() - 1);
	int slash = f.ReverseFind(_T('\\'));
	if (slash < 0)
		return _T("");
	// "C:" -> no parent (it's a drive root once the leaf is removed).
	if (slash == f.GetLength() - 1)
		return _T("");
	CString parent = f.Left(slash + 1);
	// A bare "C:\" has no navigable parent.
	if (parent.GetLength() <= 3)
		return parent.CompareNoCase(folder) == 0 ? _T("") : parent;
	return parent;
}

void CThumbnailPane::Populate(const CString &folder, const CString &current)
{
	// Invalidate any in-flight decode tasks from the previous folder.
	mGen++;
	{
		std::lock_guard<std::mutex> lk(mMutex);
		mTasks.clear();
	}

	SetRedraw(FALSE);
	DeleteAllItems();
	mEntries.clear();
	// Size the tiles for the current mode/width before (re)building the image list.
	RecalcThumbSize();
	ResetImageList();
	mFolder = folder;

	// The grid steps are a pure image gallery: no parent (".."), folders, or names
	// -- only image thumbnails. Folder navigation lives in the list step.
	const bool grid = IsGrid();
	int row = 0;

	if (folder.IsEmpty()) {
		// Top level: list the logical drives so any directory is reachable (list
		// step only -- there are no images to show in a grid here).
		if (!grid) {
			TCHAR buf[512] = {0, };
			::GetLogicalDriveStrings(_countof(buf) - 1, buf);
			for (TCHAR *d = buf; *d; d += lstrlen(d) + 1) {
				InsertItem(row, d, FolderIconIndex());
				Entry e; e.kind = ENTRY_DIR; e.path = d; e.img = -1; e.queued = false; e.badge = false;
				mEntries.push_back(e);
				row++;
			}
		}
	} else {
		// Collect sub-directories and supported files separately.
		std::vector<CString> dirs;
		std::vector<CString> files;
		CFileFind finder;
		BOOL working = finder.FindFile(folder + _T("*.*"));
		while (working) {
			working = finder.FindNextFile();
			if (finder.IsDots())
				continue;
			if (finder.IsDirectory()) {
				if (!finder.IsHidden() && !finder.IsSystem())
					dirs.push_back(finder.GetFilePath());
			} else {
				// List every file, not just images: the main view pages through
				// the whole folder (PgUp/PgDn), so the drawer must hold each of
				// those files for its selection to stay in sync (issue #84).
				files.push_back(finder.GetFilePath());
			}
		}
		finder.Close();

		struct ByName {
			bool operator()(const CString &a, const CString &b) const
			{
				// Ordinal, case-insensitive compare so the drawer order matches the
				// main view's CFileFind/NTFS enumeration. A locale word-sort
				// (lstrcmpi) puts names beginning with '_' first, but the filesystem
				// -- and the viewer's PgUp/PgDn file navigation -- list them last; the
				// mismatch meant the drawer's "first" file could actually be near the
				// end, so paging from it stalled almost immediately (issue #84).
				return CompareStringOrdinal(PathFindFileName(a), -1,
					PathFindFileName(b), -1, TRUE) == CSTR_LESS_THAN;
			}
		};
		std::sort(dirs.begin(), dirs.end(), ByName());
		std::sort(files.begin(), files.end(), ByName());

		// Parent ("..") and sub-folders appear in the list step only; the grid is
		// images-only.
		if (!grid) {
			// ".." goes to the parent folder, or to the drive list at a drive root.
			InsertItem(row, _T(".."), FolderIconIndex());
			Entry pe; pe.kind = ENTRY_PARENT; pe.path = ParentFolderOf(folder); pe.img = -1; pe.queued = false; pe.badge = false;
			mEntries.push_back(pe);
			row++;

			for (size_t i = 0; i < dirs.size(); i++) {
				InsertItem(row, PathFindFileName(dirs[i]), FolderIconIndex());
				Entry e; e.kind = ENTRY_DIR; e.path = dirs[i] + _T("\\"); e.img = -1; e.queued = false; e.badge = false;
				mEntries.push_back(e);
				row++;
			}
		}

		for (size_t i = 0; i < files.size(); i++) {
			const CString &full = files[i];
			CString ext = ExtensionOf(full);
			bool thumbable = IsThumbnailable(ext);

			// Images and videos get a thumbnail (a light placeholder until decoded,
			// applied lazily as the row scrolls into view); raw formats and any other
			// type (documents, archives, ...) show an extension badge so the file is
			// still listed and stays selectable during PgUp/PgDn navigation.
			int img;
			if (thumbable) {
				HBITMAP cached = CacheFind(full);
				img = cached ? AddImageCopy(cached) : mLoadingImg;
			} else {
				img = BadgeForExt(ext);
			}

			// The grid hides names; the list step draws the name itself (owner-draw)
			// so its item text is only needed for keyboard type-ahead.
			InsertItem(row, grid ? _T("") : PathFindFileName(full), img);
			Entry e; e.kind = ENTRY_FILE; e.path = full;
			e.badge = !thumbable;
			e.img = thumbable ? img : -1; e.queued = false;
			mEntries.push_back(e);
			row++;
		}
	}

	if (grid) {
		// Cell = tile + a hairline gutter, so a thin background separator shows
		// between tiles (issue #80); then reflow into as many columns as fit.
		SetIconSpacing(mThumb + kGridGutter, mThumb + kGridGutter);
		if (mSlideWidth > 0)
			FreezeGridLayout();    // mid-slide: pin the N-column layout, don't reflow
		else
			Arrange(LVA_DEFAULT);
	} else {
		CRect rc; GetClientRect(&rc);
		SetColumnWidth(0, rc.Width());
	}

	SetRedraw(TRUE);
	Invalidate();

	SelectByPath(current);
	QueueVisibleThumbs();
}

void CThumbnailPane::NavigateTo(const CString &folder)
{
	// Browse to another folder without changing the main view; selecting a file
	// there loads it.
	Populate(folder, _T(""));
}

void CThumbnailPane::ActivateIndex(int index, bool allowNavigate)
{
	if (index < 0 || index >= (int)mEntries.size())
		return;

	const Entry &e = mEntries[index];
	if (e.kind != ENTRY_FILE && !allowNavigate)
		return;

	// Defer the actual load/navigate: doing it here (inside the list's own
	// click/return notification) would repopulate the list while it is still
	// using the clicked item, which can crash. Capture by value, run after.
	mPending = e;
	PostMessage(WM_DRAWER_ACTIVATE);
}

LRESULT CThumbnailPane::OnActivatePosted(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	Entry e = mPending;
	if (e.kind == ENTRY_FILE) {
		// Opening routes through CViewerDoc::OnOpenDocument (raw files included).
		// Thumbnail selection keeps the current window size and fits the image
		// into the existing viewport, like folder navigation (issue #76), instead
		// of resizing the frame to each image as a fresh File>Open does. Save and
		// restore the layout so a later File>Open still sizes the window.
		CFrameWnd *pFrame = DYNAMIC_DOWNCAST(CFrameWnd, AfxGetMainWnd());
		CViewerDoc *pDoc = pFrame ?
			DYNAMIC_DOWNCAST(CViewerDoc, pFrame->GetActiveDocument()) : NULL;
		if (pDoc) {
			LoadLayout prevLayout = pDoc->mLoadLayout;
			pDoc->mLoadLayout = LOAD_FIT_TO_WINDOW;
			AfxGetApp()->OpenDocumentFile(e.path);
			pDoc->mLoadLayout = prevLayout;
		} else {
			AfxGetApp()->OpenDocumentFile(e.path);
		}
	} else {
		NavigateTo(e.path);
	}
	return 0;
}

void CThumbnailPane::SelectByPath(const CString &path)
{
	for (size_t i = 0; i < mEntries.size(); i++) {
		if (mEntries[i].kind == ENTRY_FILE &&
				mEntries[i].path.CompareNoCase(path) == 0) {
			SetItemState((int)i, LVIS_SELECTED | LVIS_FOCUSED,
				LVIS_SELECTED | LVIS_FOCUSED);
			EnsureVisible((int)i, FALSE);
			return;
		}
	}
	// Current file is not in this folder list; clear selection.
	SetItemState(-1, 0, LVIS_SELECTED | LVIS_FOCUSED);
}

// Double-click / Enter: open an image or navigate into a folder.
void CThumbnailPane::OnItemActivate(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
	int index = -1;
	if (pNMHDR->code == NM_DBLCLK)
		index = reinterpret_cast<NMITEMACTIVATE *>(pNMHDR)->iItem;
	else
		index = GetNextItem(-1, LVNI_SELECTED);   // NM_RETURN
	ActivateIndex(index, true);
}

// Hover tooltip: show the full name (the rows truncate it with an ellipsis).
void CThumbnailPane::OnGetInfoTip(NMHDR *pNMHDR, LRESULT *pResult)
{
	*pResult = 0;
	NMLVGETINFOTIP *tip = reinterpret_cast<NMLVGETINFOTIP *>(pNMHDR);
	int i = tip->iItem;
	if (i < 0 || i >= (int)mEntries.size() ||
			tip->pszText == NULL || tip->cchTextMax <= 0)
		return;

	const Entry &e = mEntries[i];
	CString name;
	if (e.kind == ENTRY_PARENT) {
		name = _T("..");
	} else if (e.kind == ENTRY_DIR) {
		name = e.path;
		if (!name.IsEmpty() && name[name.GetLength() - 1] == _T('\\'))
			name = name.Left(name.GetLength() - 1);
		CString leaf = PathFindFileName(name);
		name = leaf.IsEmpty() ? e.path : leaf;
	} else {
		name = PathFindFileName(e.path);
	}

	lstrcpyn(tip->pszText, name, tip->cchTextMax);
}

// Selected grid tile "lift": a subtle accent-soft wash over the chosen tile (no
// frame), consistent with the accent-soft selection fill the list step uses
// (issue #80). Tiles are opaque full-bleed images, so the only way to show
// selection is to draw over the tile -- done here in custom-draw post-paint
// (icon view can't be owner-drawn). The list step keeps its own owner-draw.
void CThumbnailPane::OnCustomDraw(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLVCUSTOMDRAW cd = reinterpret_cast<LPNMLVCUSTOMDRAW>(pNMHDR);
	switch (cd->nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
		*pResult = IsGrid() ? CDRF_NOTIFYITEMDRAW : CDRF_DODEFAULT;
		return;
	case CDDS_ITEMPREPAINT: {
		int i = (int)cd->nmcd.dwItemSpec;
		bool sel = (GetItemState(i, LVIS_SELECTED) & LVIS_SELECTED) != 0;
		*pResult = sel ? CDRF_NOTIFYPOSTPAINT : CDRF_DODEFAULT;
		return;
	}
	case CDDS_ITEMPOSTPAINT: {
		int i = (int)cd->nmcd.dwItemSpec;
		CRect rc;
		if (GetItemRect(i, &rc, LVIR_ICON)) {
			// Constant-alpha blend of accent-soft over the tile (stretch a 1x1
			// source); ~38% reads as selected while staying gentle.
			BITMAPINFO bmi = {};
			bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			bmi.bmiHeader.biWidth = 1;
			bmi.bmiHeader.biHeight = 1;
			bmi.bmiHeader.biPlanes = 1;
			bmi.bmiHeader.biBitCount = 32;
			bmi.bmiHeader.biCompression = BI_RGB;
			void *bits = NULL;
			HDC dst = cd->nmcd.hdc;
			HDC mem = ::CreateCompatibleDC(dst);
			HBITMAP dib = ::CreateDIBSection(dst, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
			if (mem && dib && bits) {
				BYTE *p = static_cast<BYTE *>(bits);
				p[0] = GetBValue(Q1UI_COLOR_ACCENT_SOFT);
				p[1] = GetGValue(Q1UI_COLOR_ACCENT_SOFT);
				p[2] = GetRValue(Q1UI_COLOR_ACCENT_SOFT);
				p[3] = 0;
				HBITMAP old = (HBITMAP)::SelectObject(mem, dib);
				BLENDFUNCTION bf = { AC_SRC_OVER, 0, 96, 0 };
				::AlphaBlend(dst, rc.left, rc.top, rc.Width(), rc.Height(),
					mem, 0, 0, 1, 1, bf);
				::SelectObject(mem, old);
			}
			if (dib) ::DeleteObject(dib);
			if (mem) ::DeleteDC(mem);
		}
		*pResult = CDRF_DODEFAULT;
		return;
	}
	default:
		*pResult = CDRF_DODEFAULT;
		return;
	}
}

// ---------------------------------------------------------------------------
// Image list helpers
// ---------------------------------------------------------------------------

void CThumbnailPane::ResetImageList()
{
	if (mImages.GetSafeHandle())
		mImages.DeleteImageList();
	mImages.Create(mThumb, mThumb, ILC_COLOR32, 0, 64);
	SetImageList(&mImages, LVSIL_SMALL);
	SetImageList(&mImages, LVSIL_NORMAL);

	// Image-list indices are invalidated by the recreate; re-seed the shared
	// placeholder and forget the per-extension badge cache.
	mFolderImg = -1;
	mBadgeByExt.clear();

	HBITMAP loading = MakePlaceholder(_T(""));
	mLoadingImg = AddImageCopy(loading);
	if (loading)
		::DeleteObject(loading);
}

int CThumbnailPane::AddImageCopy(HBITMAP hbmp)
{
	if (!hbmp)
		return mLoadingImg;
	CBitmap bmp;
	bmp.Attach(hbmp);
	int idx = mImages.Add(&bmp, (CBitmap *)NULL);
	bmp.Detach();      // the image list copied the pixels; keep the source alive
	return idx;
}

int CThumbnailPane::FolderIconIndex()
{
	if (mFolderImg >= 0)
		return mFolderImg;

	// A simple folder tile: an accent-tinted card. The name underneath (icon
	// view) or beside it (list) identifies the folder.
	void *bits = NULL;
	HBITMAP hbmp = NULL;
	{
		BITMAPINFO bmi = {};
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = mThumb;
		bmi.bmiHeader.biHeight = -mThumb;
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		HDC screen = ::GetDC(NULL);
		hbmp = ::CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
		::ReleaseDC(NULL, screen);
	}
	if (!hbmp)
		return mLoadingImg;

	HDC screen = ::GetDC(NULL);
	HDC mem = ::CreateCompatibleDC(screen);
	HBITMAP old = (HBITMAP)::SelectObject(mem, hbmp);
	CDC dc; dc.Attach(mem);

	CRect rc(0, 0, mThumb, mThumb);
	dc.FillSolidRect(rc, Q1UI_COLOR_SURFACE_ALT);

	// Folder glyph: a tab + body rounded rectangle.
	CRect body = rc; body.DeflateRect(mThumb / 6, mThumb / 4);
	body.top += mThumb / 12;
	CBrush fill(Q1UI_COLOR_ACCENT_SOFT);
	CPen   pen(PS_SOLID, std::max(1, mThumb / 48), Q1UI_COLOR_ACCENT);
	CBrush *ob = dc.SelectObject(&fill);
	CPen   *op = dc.SelectObject(&pen);
	CRect tab(body.left, body.top - mThumb / 12, body.left + body.Width() / 2, body.top + mThumb / 16);
	dc.RoundRect(tab, CPoint(mThumb / 16, mThumb / 16));
	dc.RoundRect(body, CPoint(mThumb / 12, mThumb / 12));
	dc.SelectObject(ob);
	dc.SelectObject(op);

	dc.Detach();
	::SelectObject(mem, old);
	::DeleteDC(mem);
	::ReleaseDC(NULL, screen);

	BYTE *p = static_cast<BYTE *>(bits);
	for (int i = 0; i < mThumb * mThumb; i++)
		p[i * 4 + 3] = 255;

	mFolderImg = AddImageCopy(hbmp);
	::DeleteObject(hbmp);
	return mFolderImg;
}

int CThumbnailPane::BadgeForExt(const CString &ext)
{
	CString key = ext; key.MakeLower();
	std::map<CString, int>::iterator it = mBadgeByExt.find(key);
	if (it != mBadgeByExt.end())
		return it->second;

	HBITMAP hbmp = MakePlaceholder(ext);
	int idx = AddImageCopy(hbmp);
	if (hbmp)
		::DeleteObject(hbmp);
	mBadgeByExt[key] = idx;
	return idx;
}

void CThumbnailPane::QueueThumb(int index, const CString &path)
{
	Task t;
	t.gen = mGen.load();
	t.index = index;
	t.size = mThumb;
	t.crop = IsGrid();          // grid tiles fill the square (crop); list fits
	t.path = path;
	{
		std::lock_guard<std::mutex> lk(mMutex);
		mTasks.push_back(t);
	}
	mCv.notify_one();
}

LRESULT CThumbnailPane::OnThumbReady(WPARAM wParam, LPARAM /*lParam*/)
{
	Result *r = reinterpret_cast<Result *>(wParam);
	if (r == NULL)
		return 0;

	bool current = r->gen == mGen.load() && r->size == mThumb &&
		r->index >= 0 && r->index < (int)mEntries.size() &&
		mEntries[r->index].kind == ENTRY_FILE;
	if (r->hbmp && current) {
		int img = AddImageCopy(r->hbmp);                // image list keeps its own copy
		CacheStore(mEntries[r->index].path, r->hbmp);   // cache takes ownership

		mEntries[r->index].img = img;                   // owner-draw (list) reads this
		SetItem(r->index, 0, LVIF_IMAGE, NULL, img, 0, 0, 0); // icon view reads this
		RedrawItems(r->index, r->index);
	} else if (current) {
		// The decode produced nothing (e.g., a video whose codec the shell can't
		// thumbnail): fall back to an extension badge so the file is still shown and
		// stays selectable, instead of a permanent blank placeholder (issue #84).
		int badge = BadgeForExt(ExtensionOf(mEntries[r->index].path));
		mEntries[r->index].badge = true;
		mEntries[r->index].img = -1;
		SetItem(r->index, 0, LVIF_IMAGE, NULL, badge, 0, 0, 0);
		RedrawItems(r->index, r->index);
	} else if (r->hbmp) {
		::DeleteObject(r->hbmp);
	}

	delete r;
	return 0;
}

// ---------------------------------------------------------------------------
// LRU cache (keyed by path + size so each view step caches independently)
// ---------------------------------------------------------------------------

CString CThumbnailPane::CacheKey(const CString &path) const
{
	// Size and crop both change the pixels, so key on them (a 44px fit list thumb
	// and a 44px cropped grid thumb must not alias to the same cache entry).
	CString key;
	key.Format(_T("%d|%d|%s"), mThumb, IsGrid() ? 1 : 0, (LPCTSTR)path);
	return key;
}

HBITMAP CThumbnailPane::CacheFind(const CString &path)
{
	CString key = CacheKey(path);
	std::map<CString, HBITMAP>::iterator it = mCache.find(key);
	if (it == mCache.end())
		return NULL;
	mCacheOrder.remove(key);
	mCacheOrder.push_back(key);
	return it->second;
}

void CThumbnailPane::CacheStore(const CString &path, HBITMAP hbmp)
{
	CString key = CacheKey(path);
	std::map<CString, HBITMAP>::iterator it = mCache.find(key);
	if (it != mCache.end()) {
		// Already cached; drop the duplicate.
		if (it->second != hbmp)
			::DeleteObject(hbmp);
		mCacheOrder.remove(key);
		mCacheOrder.push_back(key);
		return;
	}

	mCache[key] = hbmp;
	mCacheOrder.push_back(key);

	while (mCache.size() > mCacheCap && !mCacheOrder.empty()) {
		CString oldest = mCacheOrder.front();
		mCacheOrder.pop_front();
		std::map<CString, HBITMAP>::iterator victim = mCache.find(oldest);
		if (victim != mCache.end()) {
			if (victim->second)
				::DeleteObject(victim->second);
			mCache.erase(victim);
		}
	}
}

void CThumbnailPane::CacheClear()
{
	for (std::map<CString, HBITMAP>::iterator it = mCache.begin();
			it != mCache.end(); ++it) {
		if (it->second)
			::DeleteObject(it->second);
	}
	mCache.clear();
	mCacheOrder.clear();
}

// ---------------------------------------------------------------------------
// Bitmap rendering
// ---------------------------------------------------------------------------

// Forces every pixel opaque so a GDI-drawn 32bpp DIB isn't treated as fully
// transparent by an ILC_COLOR32 image list.
static void ForceOpaque(void *bits, int size)
{
	BYTE *p = static_cast<BYTE *>(bits);
	for (int i = 0; i < size * size; i++)
		p[i * 4 + 3] = 255;
}

static HBITMAP CreateThumbDib(int size, void **ppBits)
{
	BITMAPINFO bmi = {};
	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bmi.bmiHeader.biWidth = size;
	bmi.bmiHeader.biHeight = -size;     // top-down
	bmi.bmiHeader.biPlanes = 1;
	bmi.bmiHeader.biBitCount = 32;
	bmi.bmiHeader.biCompression = BI_RGB;

	HDC screen = ::GetDC(NULL);
	HBITMAP hbmp = ::CreateDIBSection(screen, &bmi, DIB_RGB_COLORS, ppBits, NULL, 0);
	::ReleaseDC(NULL, screen);
	return hbmp;
}

// Composite an arbitrary (possibly non-square) source bitmap onto a size x size
// opaque tile. crop = scale to cover and center-crop the overflow (fills the
// tile, no letterbox); otherwise scale to fit and center on a bg fill. Used to
// normalize shell thumbnails into the square cells the image list expects.
static HBITMAP CompositeOntoSquare(HBITMAP src, int size, bool crop, COLORREF bg)
{
	BITMAP bm = {};
	if (!::GetObject(src, sizeof(bm), &bm) || bm.bmWidth <= 0 || bm.bmHeight <= 0)
		return NULL;

	void *bits = NULL;
	HBITMAP tile = CreateThumbDib(size, &bits);
	if (!tile)
		return NULL;

	HDC screen = ::GetDC(NULL);
	HDC dst = ::CreateCompatibleDC(screen);
	HDC sdc = ::CreateCompatibleDC(screen);
	HBITMAP odst = (HBITMAP)::SelectObject(dst, tile);
	HBITMAP osrc = (HBITMAP)::SelectObject(sdc, src);

	RECT rc = { 0, 0, size, size };
	HBRUSH bgBrush = ::CreateSolidBrush(bg);
	::FillRect(dst, &rc, bgBrush);
	::DeleteObject(bgBrush);

	// Cover (crop) scales by the larger ratio so the tile is fully covered, with
	// the overflow clipped to the tile DC; fit scales by the smaller ratio. Both
	// center the image.
	double scale = crop
		? std::max((double)size / bm.bmWidth, (double)size / bm.bmHeight)
		: std::min((double)size / bm.bmWidth, (double)size / bm.bmHeight);
	int nw = std::max(1, (int)(bm.bmWidth * scale));
	int nh = std::max(1, (int)(bm.bmHeight * scale));
	int ox = (size - nw) / 2;       // negative when cropping (overflow clipped)
	int oy = (size - nh) / 2;

	::SetStretchBltMode(dst, HALFTONE);
	::SetBrushOrgEx(dst, 0, 0, NULL);
	::StretchBlt(dst, ox, oy, nw, nh, sdc, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);

	::SelectObject(dst, odst);
	::SelectObject(sdc, osrc);
	::DeleteDC(dst);
	::DeleteDC(sdc);
	::ReleaseDC(NULL, screen);

	ForceOpaque(bits, size);
	return tile;
}

// Fast path: the Windows shell thumbnail (thumbcache + EXIF), the same source
// Explorer uses. THUMBNAILONLY makes non-previewable types fail here so we fall
// back to our own decode (or a placeholder) instead of getting a generic icon.
HBITMAP CThumbnailPane::DecodeThumbnailShell(const CString &path, int size, bool crop, COLORREF bg)
{
	IShellItemImageFactory *factory = NULL;
	HRESULT hr = SHCreateItemFromParsingName(path, NULL, IID_PPV_ARGS(&factory));
	if (FAILED(hr) || !factory)
		return NULL;

	SIZE sz = { size, size };
	// Cropping needs enough source pixels to cover the short axis, so allow the
	// shell to hand back a bigger (still aspect-correct) thumbnail; fitting just
	// wants it shrunk to fit.
	DWORD flags = SIIGBF_THUMBNAILONLY | (crop ? SIIGBF_BIGGERSIZEOK : SIIGBF_RESIZETOFIT);
	HBITMAP raw = NULL;
	hr = factory->GetImage(sz, flags, &raw);
	factory->Release();
	if (FAILED(hr) || !raw)
		return NULL;

	HBITMAP square = CompositeOntoSquare(raw, size, crop, bg);
	::DeleteObject(raw);
	return square;
}

// Fallback decode via OpenCV (covers HEIF/AVIF when the shell has no codec, and
// any format the shell declines to thumbnail).
HBITMAP CThumbnailPane::DecodeThumbnailCv(const CString &path, int size, bool crop, COLORREF bg)
{
	cv::Mat img = q1::imreadW(path.GetString());
	if (img.empty())
		return NULL;

	cv::Mat bgr;
	if (img.channels() == 4)
		cv::cvtColor(img, bgr, cv::COLOR_BGRA2BGR);
	else if (img.channels() == 1)
		cv::cvtColor(img, bgr, cv::COLOR_GRAY2BGR);
	else
		bgr = img;

	// Cover (crop) scales by the larger ratio so both axes reach >= size; fit uses
	// the smaller ratio and pads with bg.
	double scale = crop
		? std::max((double)size / bgr.cols, (double)size / bgr.rows)
		: std::min((double)size / bgr.cols, (double)size / bgr.rows);
	int nw = std::max(1, (int)(bgr.cols * scale));
	int nh = std::max(1, (int)(bgr.rows * scale));

	cv::Mat resized;
	cv::resize(bgr, resized, cv::Size(nw, nh), 0, 0, cv::INTER_AREA);

	cv::Mat canvas;
	if (crop) {
		// Center-crop the size x size region (nw, nh are both >= size here).
		int x0 = std::min(nw - size, std::max(0, (nw - size) / 2));
		int y0 = std::min(nh - size, std::max(0, (nh - size) / 2));
		resized(cv::Rect(x0, y0, size, size)).copyTo(canvas);
	} else {
		cv::Scalar bgScalar(GetBValue(bg), GetGValue(bg), GetRValue(bg));
		canvas = cv::Mat(size, size, CV_8UC3, bgScalar);
		int ox = (size - nw) / 2;
		int oy = (size - nh) / 2;
		resized.copyTo(canvas(cv::Rect(ox, oy, nw, nh)));
	}

	void *bits = NULL;
	HBITMAP hbmp = CreateThumbDib(size, &bits);
	if (!hbmp)
		return NULL;

	BYTE *dst = static_cast<BYTE *>(bits);
	for (int y = 0; y < size; y++) {
		const cv::Vec3b *src = canvas.ptr<cv::Vec3b>(y);
		BYTE *row = dst + (size_t)y * size * 4;
		for (int x = 0; x < size; x++) {
			row[x * 4 + 0] = src[x][0];   // B
			row[x * 4 + 1] = src[x][1];   // G
			row[x * 4 + 2] = src[x][2];   // R
			row[x * 4 + 3] = 255;         // A
		}
	}
	return hbmp;
}

HBITMAP CThumbnailPane::DecodeThumbnail(const CString &path, int size, bool crop, COLORREF bg)
{
	HBITMAP h = DecodeThumbnailShell(path, size, crop, bg);
	if (h)
		return h;
	return DecodeThumbnailCv(path, size, crop, bg);
}

HBITMAP CThumbnailPane::MakePlaceholder(const CString &ext)
{
	void *bits = NULL;
	HBITMAP hbmp = CreateThumbDib(mThumb, &bits);
	if (!hbmp)
		return NULL;

	HDC screen = ::GetDC(NULL);
	HDC mem = ::CreateCompatibleDC(screen);
	HBITMAP old = (HBITMAP)::SelectObject(mem, hbmp);

	CDC dc;
	dc.Attach(mem);

	CRect rc(0, 0, mThumb, mThumb);
	dc.FillSolidRect(rc, Q1UI_COLOR_SURFACE_ALT);

	// A simple rounded "file card".
	CRect card = rc;
	card.DeflateRect(mThumb / 6, mThumb / 8);
	CBrush cardBrush(Q1UI_COLOR_SURFACE);
	CPen border(PS_SOLID, 1, Q1UI_COLOR_BORDER);
	CBrush *pOldBrush = dc.SelectObject(&cardBrush);
	CPen *pOldPen = dc.SelectObject(&border);
	dc.RoundRect(card, CPoint(mThumb / 10, mThumb / 10));
	dc.SelectObject(pOldBrush);
	dc.SelectObject(pOldPen);

	if (!ext.IsEmpty()) {
		CString label = ext;
		label.MakeUpper();
		dc.SetBkMode(TRANSPARENT);
		dc.SetTextColor(Q1UI_COLOR_ACCENT);
		CFont *pOldFont = dc.SelectObject(&mExtFont);
		dc.DrawText(label, card, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		dc.SelectObject(pOldFont);
	}

	dc.Detach();
	::SelectObject(mem, old);
	::DeleteDC(mem);
	::ReleaseDC(NULL, screen);

	ForceOpaque(bits, mThumb);
	return hbmp;
}

// ---------------------------------------------------------------------------
// Worker
// ---------------------------------------------------------------------------

void CThumbnailPane::WorkerLoop()
{
	// Shell thumbnail extraction is COM; each worker needs its own apartment.
	HRESULT hrCo = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);

	for (;;) {
		Task task;
		{
			std::unique_lock<std::mutex> lk(mMutex);
			mCv.wait(lk, [this] { return mStop || !mTasks.empty(); });
			if (mStop)
				break;
			task = mTasks.front();
			mTasks.pop_front();
		}

		if (task.gen != mGen.load())
			continue;

		HBITMAP hbmp = DecodeThumbnail(task.path, task.size, task.crop, Q1UI_COLOR_SURFACE_ALT);

		if (task.gen != mGen.load() || !GetSafeHwnd() || !::IsWindow(m_hWnd)) {
			if (hbmp)
				::DeleteObject(hbmp);
			continue;
		}

		Result *r = new Result();
		r->gen = task.gen;
		r->index = task.index;
		r->size = task.size;
		r->hbmp = hbmp;
		if (!::PostMessage(m_hWnd, WM_THUMB_READY, reinterpret_cast<WPARAM>(r), 0)) {
			if (hbmp)
				::DeleteObject(hbmp);
			delete r;
		}
	}

	if (SUCCEEDED(hrCo))
		::CoUninitialize();
}
