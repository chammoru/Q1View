// ThumbnailPane.cpp : implementation of CThumbnailPane
//

#include "stdafx.h"
#include "Viewer.h"
#include "ThumbnailPane.h"

#include "QViewerCmn.h"
#include "QCvUtil.h"

#include <opencv2/imgproc/imgproc.hpp>

#include "Shlwapi.h"
#include <algorithm>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

static CString ExtensionOf(const CString &path);   // defined below

BEGIN_MESSAGE_MAP(CThumbnailPane, CListCtrl)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_NOTIFY_REFLECT(NM_DBLCLK, &CThumbnailPane::OnItemActivate)
	ON_NOTIFY_REFLECT(NM_RETURN, &CThumbnailPane::OnItemActivate)
	ON_NOTIFY_REFLECT(LVN_GETINFOTIP, &CThumbnailPane::OnGetInfoTip)
	ON_MESSAGE(WM_THUMB_READY, &CThumbnailPane::OnThumbReady)
	ON_MESSAGE(WM_DRAWER_ACTIVATE, &CThumbnailPane::OnActivatePosted)
END_MESSAGE_MAP()

CThumbnailPane::CThumbnailPane()
: mThumb(48)
, mLoadingImg(-1)
, mCacheCap(256)
, mStop(false)
, mGen(0)
, mWorkerStarted(false)
{
}

CThumbnailPane::~CThumbnailPane()
{
	Shutdown();
}

BOOL CThumbnailPane::CreatePane(CWnd *pParent, UINT nID)
{
	// Compact report list (no border so there is no dark divider line; the
	// drawer's background colour separates it from the image view).
	DWORD style = WS_CHILD | WS_VISIBLE | LVS_OWNERDRAWFIXED |
		LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER;
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

	// Small thumbnails sit at the left of each row; the row height follows them.
	mImages.Create(mThumb, mThumb, ILC_COLOR32, 0, 64);
	SetImageList(&mImages, LVSIL_SMALL);

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

	mStop = false;
	mWorker = std::thread(&CThumbnailPane::WorkerLoop, this);
	mWorkerStarted = true;

	return 0;
}

void CThumbnailPane::OnSize(UINT nType, int cx, int cy)
{
	CListCtrl::OnSize(nType, cx, cy);
	// Single column fills the client width so there is no horizontal scrollbar.
	SetColumnWidth(0, cx);
	ShowScrollBar(SB_HORZ, FALSE);
}

void CThumbnailPane::DrawItem(LPDRAWITEMSTRUCT dis)
{
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
		if (IsRawExt(ext)) {
			// Raw formats: extension badge instead of a thumbnail.
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
		if (mWorker.joinable())
			mWorker.join();
		mWorkerStarted = false;

		// Drain any results the worker posted but we never processed.
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

bool CThumbnailPane::IsRawExt(const CString &ext)
{
	return ext.CompareNoCase(_T("yuv")) == 0 || ext.CompareNoCase(_T("rgb")) == 0;
}

bool CThumbnailPane::IsSupportedExt(const CString &ext)
{
	return IsDecodableExt(ext) || IsRawExt(ext);
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
	ResetImageList();
	mFolder = folder;

	int row = 0;

	if (folder.IsEmpty()) {
		// Top level: list the logical drives so any directory is reachable.
		TCHAR buf[512] = {0, };
		::GetLogicalDriveStrings(_countof(buf) - 1, buf);
		for (TCHAR *d = buf; *d; d += lstrlen(d) + 1) {
			InsertItem(row, d, I_IMAGENONE);
			Entry e; e.kind = ENTRY_DIR; e.path = d; e.img = -1;
			mEntries.push_back(e);
			row++;
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
				CString full = finder.GetFilePath();
				if (IsSupportedExt(ExtensionOf(full)))
					files.push_back(full);
			}
		}
		finder.Close();

		struct ByName {
			bool operator()(const CString &a, const CString &b) const
			{ return lstrcmpi(PathFindFileName(a), PathFindFileName(b)) < 0; }
		};
		std::sort(dirs.begin(), dirs.end(), ByName());
		std::sort(files.begin(), files.end(), ByName());

		// ".." goes to the parent folder, or to the drive list at a drive root.
		InsertItem(row, _T(".."), I_IMAGENONE);
		Entry pe; pe.kind = ENTRY_PARENT; pe.path = ParentFolderOf(folder); pe.img = -1;
		mEntries.push_back(pe);
		row++;

		for (size_t i = 0; i < dirs.size(); i++) {
			InsertItem(row, PathFindFileName(dirs[i]), I_IMAGENONE);
			Entry e; e.kind = ENTRY_DIR; e.path = dirs[i] + _T("\\"); e.img = -1;
			mEntries.push_back(e);
			row++;
		}

		for (size_t i = 0; i < files.size(); i++) {
			const CString &full = files[i];
			bool raw = IsRawExt(ExtensionOf(full));

			// Images get a thumbnail (a light placeholder until decoded); raw
			// files are drawn with an extension badge instead.
			int img = -1;
			if (!raw) {
				HBITMAP cached = CacheFind(full);
				img = cached ? AddImageCopy(cached) : mLoadingImg;
			}

			InsertItem(row, PathFindFileName(full), I_IMAGENONE);
			Entry e; e.kind = ENTRY_FILE; e.path = full; e.img = img;
			mEntries.push_back(e);

			if (!raw && img == mLoadingImg)
				QueueThumb(row, full);
			row++;
		}
	}

	SetRedraw(TRUE);
	Invalidate();

	SelectByPath(current);
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
		AfxGetApp()->OpenDocumentFile(e.path);
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

// ---------------------------------------------------------------------------
// Image list helpers
// ---------------------------------------------------------------------------

void CThumbnailPane::ResetImageList()
{
	if (mImages.GetSafeHandle())
		mImages.DeleteImageList();
	mImages.Create(mThumb, mThumb, ILC_COLOR32, 0, 64);
	SetImageList(&mImages, LVSIL_SMALL);

	// Folders and raw files render as text (no icon); only decoding images need
	// a light placeholder until their thumbnail arrives.
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

void CThumbnailPane::QueueThumb(int index, const CString &path)
{
	Task t;
	t.gen = mGen.load();
	t.index = index;
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

	if (r->hbmp && r->gen == mGen.load() &&
			r->index >= 0 && r->index < (int)mEntries.size() &&
			mEntries[r->index].kind == ENTRY_FILE) {
		int img = AddImageCopy(r->hbmp);   // image list keeps its own copy
		CacheStore(mEntries[r->index].path, r->hbmp);  // cache takes ownership

		mEntries[r->index].img = img;       // owner-draw reads this
		RedrawItems(r->index, r->index);
	} else if (r->hbmp) {
		::DeleteObject(r->hbmp);
	}

	delete r;
	return 0;
}

// ---------------------------------------------------------------------------
// LRU cache
// ---------------------------------------------------------------------------

HBITMAP CThumbnailPane::CacheFind(const CString &path)
{
	std::map<CString, HBITMAP>::iterator it = mCache.find(path);
	if (it == mCache.end())
		return NULL;
	mCacheOrder.remove(path);
	mCacheOrder.push_back(path);
	return it->second;
}

void CThumbnailPane::CacheStore(const CString &path, HBITMAP hbmp)
{
	std::map<CString, HBITMAP>::iterator it = mCache.find(path);
	if (it != mCache.end()) {
		// Already cached; drop the duplicate.
		if (it->second != hbmp)
			::DeleteObject(hbmp);
		mCacheOrder.remove(path);
		mCacheOrder.push_back(path);
		return;
	}

	mCache[path] = hbmp;
	mCacheOrder.push_back(path);

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

HBITMAP CThumbnailPane::DecodeThumbnail(const CString &path, int size, COLORREF bg)
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

	double scale = std::min((double)size / bgr.cols, (double)size / bgr.rows);
	int nw = std::max(1, (int)(bgr.cols * scale));
	int nh = std::max(1, (int)(bgr.rows * scale));

	cv::Mat resized;
	cv::resize(bgr, resized, cv::Size(nw, nh), 0, 0, cv::INTER_AREA);

	cv::Scalar bgScalar(GetBValue(bg), GetGValue(bg), GetRValue(bg));
	cv::Mat canvas(size, size, CV_8UC3, bgScalar);
	int ox = (size - nw) / 2;
	int oy = (size - nh) / 2;
	resized.copyTo(canvas(cv::Rect(ox, oy, nw, nh)));

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
	for (;;) {
		Task task;
		{
			std::unique_lock<std::mutex> lk(mMutex);
			mCv.wait(lk, [this] { return mStop || !mTasks.empty(); });
			if (mStop)
				return;
			task = mTasks.front();
			mTasks.pop_front();
		}

		if (task.gen != mGen.load())
			continue;

		HBITMAP hbmp = DecodeThumbnail(task.path, mThumb, Q1UI_COLOR_SURFACE_ALT);

		if (task.gen != mGen.load() || !GetSafeHwnd() || !::IsWindow(m_hWnd)) {
			if (hbmp)
				::DeleteObject(hbmp);
			continue;
		}

		Result *r = new Result();
		r->gen = task.gen;
		r->index = task.index;
		r->hbmp = hbmp;
		if (!::PostMessage(m_hWnd, WM_THUMB_READY, reinterpret_cast<WPARAM>(r), 0)) {
			if (hbmp)
				::DeleteObject(hbmp);
			delete r;
		}
	}
}
