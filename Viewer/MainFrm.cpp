// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"

#include <atlimage.h>

#include "Viewer.h"

#include "MainFrm.h"

#include "ViewerDoc.h"
#include "ViewerView.h"
#include "ThumbnailPane.h"
#include "StoreUpdate.h"

#include "QCommon.h"

#include "CustomSizeDlg.h"
#include "CustomFpsDlg.h"

#include <QImageStr.h>
#include <QViewerCmn.h>
#include "qimage_util.h"

#include "FrmSrc.h"

#include "Shlwapi.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// Thumbnail drawer geometry. The width min/max/default are shared with the Qt
// viewer (DRAWER_MIN_W / DRAWER_MAX_W / DRAWER_DEF_W in qimage_presets.h); the
// divider width DRAWER_SPLIT_BAR lives in MainFrm.h. Keep at least this much
// image view so dragging the divider can never squeeze the picture away.
#define DRAWER_MIN_IMAGE  160

// Slide animation: ~180ms split into short timer ticks.
#define DRAWER_ANIM_TIMER 0xD4A1
#define DRAWER_ANIM_STEPS 12
#define DRAWER_ANIM_MS    15

// One-shot timer that kicks off the background Store update check a few seconds
// after launch, so the check never competes with cold-start work.
#define STORE_CHECK_TIMER 0xD4A2
#define STORE_CHECK_DELAY 3000

const ULONG_PTR VIEWER_SYNC_INPUT_TOKEN =
	static_cast<ULONG_PTR>(::RegisterWindowMessage(_T("Q1View.Viewer.SyncInput.v1")));
const LPCTSTR VIEWER_SYNC_INPUT_PROPERTY =
	_T("Q1View.Viewer.SyncInput.Enabled.v1");

struct ViewerSyncBroadcast
{
	HWND source;
	COPYDATASTRUCT copyData;
};

static BOOL CALLBACK SendViewerSyncInput(HWND hwnd, LPARAM lParam)
{
	const ViewerSyncBroadcast *broadcast =
		reinterpret_cast<const ViewerSyncBroadcast *>(lParam);
	if (hwnd != broadcast->source &&
			::GetProp(hwnd, VIEWER_SYNC_INPUT_PROPERTY) != NULL) {
		DWORD_PTR result = 0;
		::SendMessageTimeout(hwnd, WM_COPYDATA,
			reinterpret_cast<WPARAM>(broadcast->source),
			reinterpret_cast<LPARAM>(&broadcast->copyData),
			SMTO_ABORTIFHUNG, 100, &result);
	}

	return TRUE;
}

// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	ON_WM_DROPFILES()
	ON_COMMAND(ID_VIEWER_HELP, &CMainFrame::OnHelp)
	ON_COMMAND(ID_FILE_OPEN, &CMainFrame::OnFileOpen)
	ON_COMMAND(ID_COMPARE, &CMainFrame::OnExecComparator)
	ON_COMMAND_RANGE(ID_RESOLUTION_START, ID_RESOLUTION_END, &CMainFrame::OnResolutionChange)
	ON_COMMAND_RANGE(ID_CS_START, ID_CS_END, &CMainFrame::OnCsChange)
	ON_COMMAND_RANGE(ID_FPS_START, ID_FPS_END, &CMainFrame::OnFpsChange)
	ON_WM_CREATE()
	ON_MESSAGE(WM_RELOAD, &CMainFrame::Reload)
	ON_WM_COPYDATA()
	ON_MESSAGE(WM_APPLY_SYNC_INPUT, &CMainFrame::OnApplySyncInput)
	ON_MESSAGE(WM_APPLY_SYNC_VIEW_STATE, &CMainFrame::OnApplySyncViewState)
	ON_COMMAND(ID_EDIT_COPY, &CMainFrame::OnEditCopy)
	ON_COMMAND(ID_EDIT_PASTE, &CMainFrame::OnEditPaste)
	ON_WM_SIZE()
	ON_WM_DESTROY()
	ON_WM_TIMER()
	ON_COMMAND(ID_TOGGLE_DRAWER, &CMainFrame::OnToggleDrawer)
	ON_UPDATE_COMMAND_UI(ID_TOGGLE_DRAWER, &CMainFrame::OnUpdateToggleDrawer)
	ON_COMMAND(ID_UPDATE, &CMainFrame::OnUpdateNow)
	ON_MESSAGE(WM_STORE_UPDATE_AVAILABLE, &CMainFrame::OnStoreUpdateAvailable)
	ON_MESSAGE(WM_STORE_UPDATE_DONE, &CMainFrame::OnStoreUpdateDone)
END_MESSAGE_MAP()


// ---------------------------------------------------------------------------
// CDrawerSplitter: thin, draggable divider between the image view and drawer
// ---------------------------------------------------------------------------

BEGIN_MESSAGE_MAP(CDrawerSplitter, CSplitterWnd)
	ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

void CDrawerSplitter::OnDrawSplitter(CDC *pDC, ESplitType nType, const CRect &rect)
{
	// pDC == NULL is the framework's layout/hit-test pass, not a paint; defer.
	if (pDC == NULL) {
		CSplitterWnd::OnDrawSplitter(pDC, nType, rect);
		return;
	}
	if (nType != splitBar)
		return;   // no 3D borders/intersections, just the flat bar

	// Flat fill matching the drawer background, with a 1px hairline so the
	// divider reads as a clean line rather than the default raised 3D bar.
	pDC->FillSolidRect(rect, Q1UI_COLOR_SURFACE_ALT);
	CRect line = rect;
	line.left += (rect.Width() - 1) / 2;
	line.right = line.left + 1;
	pDC->FillSolidRect(line, Q1UI_COLOR_BORDER);
}

void CDrawerSplitter::OnLButtonUp(UINT nFlags, CPoint point)
{
	BOOL wasTracking = m_bTracking;
	CSplitterWnd::OnLButtonUp(nFlags, point);   // applies the new split
	if (wasTracking && mFrame != NULL)
		mFrame->OnDrawerDividerDragged();
}


CMainFrame::CMainFrame()
: mSyncInput(false)
, mSyncViewStatePending(false)
, mpDrawer(NULL)
, mDrawerVisible(false)
, mDrawerWidth(DRAWER_DEF_W)
, mSplitterReady(false)
, mDrawerAnimating(false)
, mDrawerAnimOpening(false)
, mDrawerAnimStep(0)
, mDrawerAnimSteps(DRAWER_ANIM_STEPS)
, mUpdateMenuShown(false)
{
	// Default hidden: the first run looks exactly like the classic Viewer.
	mDrawerVisible = AfxGetApp()->GetProfileInt(_T("Drawer"), _T("Visible"), 0) != 0;
	mDrawerWidth = AfxGetApp()->GetProfileInt(_T("Drawer"), _T("Width"), DRAWER_DEF_W);
	if (mDrawerWidth < DRAWER_MIN_W) mDrawerWidth = DRAWER_MIN_W;
	if (mDrawerWidth > DRAWER_MAX_W) mDrawerWidth = DRAWER_MAX_W;

	mResolutionMenu.CreatePopupMenu();
	mCsMenu.CreatePopupMenu();
	mFpsMenu.CreatePopupMenu();

	BITMAPINFOHEADER &bmiHeader = mCopyBmi.bmiHeader;
	bmiHeader.biSize = (DWORD)sizeof(BITMAPINFOHEADER);
	bmiHeader.biPlanes = 1;
	bmiHeader.biBitCount = 24;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = 0L;
	bmiHeader.biXPelsPerMeter = 0L;
	bmiHeader.biYPelsPerMeter = 0L;
}

CMainFrame::~CMainFrame()
{
	mFpsMenu.DestroyMenu();
	mCsMenu.DestroyMenu();
	mResolutionMenu.DestroyMenu();

	delete mpDrawer;
	mpDrawer = NULL;
}

LRESULT CMainFrame::Reload(WPARAM wParam, LPARAM lParam)
{
	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());
	if (!pDoc->ReloadDocument())
		return E_FAIL;

	GetActiveView()->Invalidate(FALSE);
	return S_OK;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CFrameWnd::PreCreateWindow(cs))
		return FALSE;

	cs.style = WS_OVERLAPPED | WS_CAPTION | FWS_ADDTOTITLE
		 | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU | WS_THICKFRAME;

	cs.dwExStyle &= ~WS_EX_CLIENTEDGE;

	// The window always opens at the decided size; the drawer (if visible on
	// launch) takes its width from the image area rather than growing the
	// window, matching the Qt viewer (issue #76).
	CRect rcClient(0, 0, VIEWER_DEF_W, VIEWER_DEF_H);
	::AdjustWindowRectEx(&rcClient, cs.style, TRUE, cs.dwExStyle);

	cs.cx = rcClient.Width();
	cs.cy = rcClient.Height();

	return TRUE;
}

void CMainFrame::OnUpdateFrameTitle(BOOL bAddToTitle)
{
	if ((GetStyle() & FWS_ADDTOTITLE) == 0)
		return;

	CDocument* pDocument = GetActiveDocument();
	if (bAddToTitle && pDocument != NULL)
		SetWindowText((LPCTSTR)pDocument->GetTitle());
}

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWnd::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWnd::Dump(dc);
}

#endif //_DEBUG


void CMainFrame::OnHelp()
{
	CViewerView *pView = static_cast<CViewerView *>(GetActiveView());
	if (!pView) {
		CWnd *pWnd = GetDescendantWindow(AFX_IDW_PANE_FIRST, TRUE);
		if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CViewerView)))
			pView = static_cast<CViewerView *>(pWnd);
	}
	if (pView) {
		pView->ToggleHelp();
		pView->UpdateWindow();
	}
}

void CMainFrame::OnFileOpen()
{
	TCHAR name_filter[] =
		_T("All Files (*.*)|*.*|")
		_T("Image Files (*.bmp;*.jpg;*.jpeg;*.png;*.tif;*.tiff;*.webp;*.heic;*.heif;*.hif;*.avif)|*.bmp;*.jpg;*.jpeg;*.png;*.tif;*.tiff;*.webp;*.heic;*.heif;*.hif;*.avif|")
		_T("YUV Files (*.yuv)|*.yuv|")
		_T("RGB Files (*.rgb)|*.rgb|");

	CFileDialog ins_dlg(TRUE, _T("All Files (*.*)"), _T("*.*"),
		OFN_HIDEREADONLY, name_filter, NULL);

	// Default to common structured image formats, while still allowing raw/video sources.
	ins_dlg.m_ofn.nFilterIndex = 2;

	if (ins_dlg.DoModal() == IDOK) {
		CString path = ins_dlg.GetPathName();

		AfxGetApp()->OpenDocumentFile(path);
	}
}

#define MENU_POS_RESOLUTION 1
#define MENU_POS_COLORSPACE 2
#define MENU_POS_FPS        3

void CMainFrame::UpdateResolutionLabel(int w, int h)
{
	if (GetMenu() == NULL)
		return;

	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());

	UINT nFlags = MF_BYPOSITION | MF_POPUP;
	FrmSrc *frmSrc = pDoc->mFrmSrc;
	if (frmSrc && frmSrc->isFixed())
		nFlags |= MF_DISABLED;
	else
		nFlags |= MF_ENABLED;

	CString str;
	str.Format(_T("%d&x%d"), w, h);
	GetMenu()->ModifyMenu(MENU_POS_RESOLUTION, nFlags,
		(UINT_PTR)mResolutionMenu.m_hMenu, str);
}

void CMainFrame::UpdateCsLabel(const TCHAR *csLabel)
{
	if (GetMenu() == NULL)
		return;

	GetMenu()->ModifyMenu(MENU_POS_COLORSPACE, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mCsMenu.m_hMenu, csLabel);
}

void CMainFrame::UpdateFpsLabel(double fps)
{
	if (GetMenu() == NULL)
		return;

	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());

	CString str;
	str.Format(_T("%.2ff&ps"), fps);
	GetMenu()->ModifyMenu(MENU_POS_FPS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mFpsMenu.m_hMenu, str);
}

void CMainFrame::CheckResolutionRadio(int w, int h)
{
	if (GetMenu() == NULL)
		return;

	UINT id;
	int idx = q1::image_resolution_idx(w, h);
	if (idx >= 0)
		id = ID_RESOLUTION_START + (UINT)idx;
	else
		id = ID_RESOLUTION_START + ARRAY_SIZE(q1::resolution_info_table) - 1;

	CMenu *subMenu = GetMenu()->GetSubMenu(MENU_POS_RESOLUTION);

	subMenu->CheckMenuRadioItem(ID_RESOLUTION_START, ID_RESOLUTION_END,
		id, MF_CHECKED | MF_BYCOMMAND);
}

void CMainFrame::CheckCsRadio(int cs)
{
	if (GetMenu() == NULL)
		return;

	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(qcsc_info_table); i++) {
		if (qcsc_info_table[i].cs == cs)
			break;
	}

	ASSERT(i < ARRAY_SIZE(qcsc_info_table));

	UINT id = ID_CS_START + i;

	CMenu *subMenu = GetMenu()->GetSubMenu(MENU_POS_COLORSPACE);

	subMenu->CheckMenuRadioItem(ID_CS_START, ID_CS_END,
		id, MF_CHECKED | MF_BYCOMMAND);
}

void CMainFrame::CheckFpsRadio(double fps)
{
	if (GetMenu() == NULL)
		return;

	unsigned int i;
	int num = -1;
	for (i = 0; i < ARRAY_SIZE(qfps_info_table) - 1; i++) {
		q1::image_parse_num(qfps_info_table[i], &num);
		if (num == fps)
			break;
	}

	UINT id = ID_FPS_START + i;

	CMenu *subMenu = GetMenu()->GetSubMenu(MENU_POS_FPS);

	subMenu->CheckMenuRadioItem(ID_FPS_START, ID_FPS_END,
		id, MF_CHECKED | MF_BYCOMMAND);
}

void CMainFrame::AddMainMenu()
{
	CString str;

	str.Format(_T("%d&x%d"), VIEWER_DEF_W, VIEWER_DEF_H);
	GetMenu()->InsertMenu(MENU_POS_RESOLUTION, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mResolutionMenu.m_hMenu, str);

	str = CString(qcsc_info_table[QIMG_DEF_CS_IDX].name).MakeUpper();
	GetMenu()->InsertMenu(MENU_POS_COLORSPACE, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mCsMenu.m_hMenu, str);

	str.Format(_T("%0.2ff&ps"), VIEWER_DEF_FPS);
	GetMenu()->InsertMenu(MENU_POS_FPS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mFpsMenu.m_hMenu, str);

}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	int i;
	CString str;

	// size menu
	for (i = 0; i < ARRAY_SIZE(q1::resolution_info_table) - 1; i++) {
		mResolutionMenu.AppendMenu(MF_STRING, ID_RESOLUTION_START + i,
			CA2W(q1::resolution_info_table[i]));
	}

	mResolutionMenu.AppendMenu(MF_SEPARATOR);
	mResolutionMenu.AppendMenu(MF_STRING, ID_RESOLUTION_START + i,
		CA2W(q1::resolution_info_table[i]));

	// color space menu
	for (i = 0; i < ARRAY_SIZE(qcsc_info_table); i++) {
		str = qcsc_info_table[i].name;

		str.MakeUpper();
		mCsMenu.AppendMenu(MF_STRING, ID_CS_START + i, str);
	}

	// fps menu
	for (i = 0; i < ARRAY_SIZE(qfps_info_table) - 1; i++) {
		mFpsMenu.AppendMenu(MF_STRING, ID_FPS_START + i,
			CA2W(qfps_info_table[i]));
	}

	mFpsMenu.AppendMenu(MF_SEPARATOR);
	mFpsMenu.AppendMenu(MF_STRING, ID_FPS_START + i, CA2W(qfps_info_table[i]));

	AddMainMenu();

	CheckResolutionRadio(VIEWER_DEF_W, VIEWER_DEF_H);
	CheckCsRadio(qcsc_info_table[QIMG_DEF_CS_IDX].cs);
	CheckFpsRadio(VIEWER_DEF_FPS);

	DrawMenuBar();

	// Schedule the background Store update check (no-op on non-Store builds).
	if (q1::store::IsPackaged())
		SetTimer(STORE_CHECK_TIMER, STORE_CHECK_DELAY, NULL);

	return 0;
}

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	// Host the thumbnail drawer (column 0) and the image view (column 1) in a
	// static splitter. Fall back to the default single-view client if anything
	// fails, so the app still works without the drawer.
	if (!mwndSplitter.CreateStatic(this, 1, 2, WS_CHILD | WS_VISIBLE))
		return CFrameWnd::OnCreateClient(lpcs, pContext);

	mwndSplitter.mFrame = this;
	mwndSplitter.SetBarVisible(mDrawerVisible);

	// Image view in column 0 (left) and the thumbnail drawer in column 1
	// (right). The frame stays a fixed size; opening the drawer or dragging the
	// divider shrinks/grows the image-view column instead (issue #76).
	if (!mwndSplitter.CreateView(0, 0, pContext->m_pNewViewClass,
			CSize(200, 200), pContext))
		return FALSE;

	mpDrawer = new CThumbnailPane();
	if (!mpDrawer->CreatePane(&mwndSplitter, mwndSplitter.IdFromRowCol(0, 1)))
		return FALSE;

	mwndSplitter.SetColumnInfo(0, 200, DRAWER_MIN_IMAGE);
	mwndSplitter.SetColumnInfo(1, mDrawerVisible ? mDrawerWidth : 0, DRAWER_MIN_W);
	mwndSplitter.RecalcLayout();

	// The image view must remain the active view for doc/view command routing.
	SetActiveView(static_cast<CView *>(mwndSplitter.GetPane(0, 0)));
	mSplitterReady = true;
	return TRUE;
}

void CMainFrame::PinDrawerColumn()
{
	if (!mSplitterReady || !::IsWindow(mwndSplitter.GetSafeHwnd()))
		return;
	// During the slide the timer drives the column sizes; don't fight it.
	if (mDrawerAnimating)
		return;

	CRect rc;
	mwndSplitter.GetClientRect(&rc);

	mwndSplitter.SetBarVisible(mDrawerVisible);
	int bar = mwndSplitter.BarWidth();

	// The drawer keeps its width inside the fixed window; the image view takes
	// the rest. Never let the drawer squeeze the view below DRAWER_MIN_IMAGE.
	int drawerCol = mDrawerVisible ? mDrawerWidth : 0;
	int maxDrawer = rc.Width() - bar - DRAWER_MIN_IMAGE;
	if (maxDrawer < 0)
		maxDrawer = 0;
	if (drawerCol > maxDrawer)
		drawerCol = maxDrawer;
	int viewCol = rc.Width() - drawerCol - bar;
	if (viewCol < 0)
		viewCol = 0;

	mwndSplitter.SetColumnInfo(0, viewCol, DRAWER_MIN_IMAGE);  // image view (left)
	mwndSplitter.SetColumnInfo(1, drawerCol, DRAWER_MIN_W);    // drawer (right)
	mwndSplitter.RecalcLayout();
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	CFrameWnd::OnSize(nType, cx, cy);
	PinDrawerColumn();
}

void CMainFrame::OnToggleDrawer()
{
	if (!mSplitterReady || mDrawerAnimating)
		return;

	// mDrawerWidth is the fixed target width; it is never rederived from the
	// laid-out column, so repeated toggles can't accumulate any drift.
	StartDrawerAnimation(!mDrawerVisible);
}

void CMainFrame::StartDrawerAnimation(bool opening)
{
	mDrawerAnimOpening = opening;
	mDrawerAnimStep = 0;
	mDrawerAnimSteps = DRAWER_ANIM_STEPS;

	if (opening) {
		// Show the pane and start decoding thumbnails now so they appear during
		// the slide. The window stays fixed; the image view gives up the width.
		mDrawerVisible = true;
		if (mpDrawer && ::IsWindow(mpDrawer->GetSafeHwnd())) {
			CDocument *pDoc = GetActiveDocument();
			if (pDoc != NULL && !pDoc->GetPathName().IsEmpty())
				mpDrawer->SetCurrentFile(pDoc->GetPathName());
		}
	}
	// When closing, keep mDrawerVisible true so the pane stays drawn while it
	// collapses; FinalizeDrawerAnimation clears it at the end.

	mDrawerAnimating = true;
	SetTimer(DRAWER_ANIM_TIMER, DRAWER_ANIM_MS, NULL);

	// Paint the first frame immediately so there's no flash.
	OnTimer(DRAWER_ANIM_TIMER);
}

void CMainFrame::ApplyDrawerColumn(int drawerCol)
{
	if (drawerCol < 0)
		drawerCol = 0;

	CRect rc;
	mwndSplitter.GetClientRect(&rc);

	// The divider only exists while the drawer has width; hide it at the ends of
	// the slide so the closed state leaves no leftover gap.
	mwndSplitter.SetBarVisible(drawerCol > 0);
	int bar = mwndSplitter.BarWidth();

	int maxDrawer = rc.Width() - bar - DRAWER_MIN_IMAGE;
	if (maxDrawer < 0)
		maxDrawer = 0;
	if (drawerCol > maxDrawer)
		drawerCol = maxDrawer;
	int viewCol = rc.Width() - drawerCol - bar;
	if (viewCol < 0)
		viewCol = 0;

	mwndSplitter.SetColumnInfo(0, viewCol, DRAWER_MIN_IMAGE);  // image view, shrinks
	mwndSplitter.SetColumnInfo(1, drawerCol, 0);               // drawer, grows
	mwndSplitter.RecalcLayout();

	// The view column changed size, so refit the image into it as it slides.
	RefitView();
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	if (nIDEvent == DRAWER_ANIM_TIMER) {
		mDrawerAnimStep++;
		double t = (double)mDrawerAnimStep / mDrawerAnimSteps;
		if (t > 1.0)
			t = 1.0;
		double eased = 1.0 - (1.0 - t) * (1.0 - t);   // ease-out
		double frac = mDrawerAnimOpening ? eased : (1.0 - eased);
		ApplyDrawerColumn((int)(frac * mDrawerWidth + 0.5));

		if (mDrawerAnimStep >= mDrawerAnimSteps) {
			KillTimer(DRAWER_ANIM_TIMER);
			mDrawerAnimating = false;
			FinalizeDrawerAnimation();
		}
		return;
	}
	if (nIDEvent == STORE_CHECK_TIMER) {
		KillTimer(STORE_CHECK_TIMER);   // one-shot
		q1::store::CheckForUpdatesAsync(GetSafeHwnd(), WM_STORE_UPDATE_AVAILABLE);
		return;
	}
	CFrameWnd::OnTimer(nIDEvent);
}

void CMainFrame::FinalizeDrawerAnimation()
{
	if (!mDrawerAnimOpening)
		mDrawerVisible = false;

	// Snap to the exact final column/layout for the resting state, then refit.
	PinDrawerColumn();
	RefitView();

	if (mDrawerVisible && mpDrawer && ::IsWindow(mpDrawer->GetSafeHwnd()))
		mpDrawer->SetFocus();
}

void CMainFrame::OnUpdateToggleDrawer(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(mDrawerVisible ? 1 : 0);
}

int CMainFrame::GetDrawerReservedWidth() const
{
	return mDrawerVisible ? (mDrawerWidth + DRAWER_SPLIT_BAR) : 0;
}

void CMainFrame::OnDrawerDividerDragged()
{
	if (!mSplitterReady || mDrawerAnimating)
		return;

	// Adopt the width the user dragged the drawer column to, clamped to the
	// shared bounds, then re-pin (enforces the minimum image view) and refit.
	int cur = 0, mn = 0;
	mwndSplitter.GetColumnInfo(1, cur, mn);
	if (cur < DRAWER_MIN_W) cur = DRAWER_MIN_W;
	if (cur > DRAWER_MAX_W) cur = DRAWER_MAX_W;
	mDrawerWidth = cur;

	PinDrawerColumn();
	RefitView();
}

void CMainFrame::RefitView()
{
	CViewerView *pView = DYNAMIC_DOWNCAST(CViewerView, GetActiveView());
	if (pView != NULL) {
		pView->FitToWindow();
		pView->Invalidate(FALSE);
	}
}

void CMainFrame::OnDocPathChanged(LPCTSTR lpszPath)
{
	// Only sync (and decode thumbnails) while the drawer is actually visible.
	if (mDrawerVisible && mpDrawer && ::IsWindow(mpDrawer->GetSafeHwnd()))
		mpDrawer->SetCurrentFile(lpszPath);
}

void CMainFrame::OnDestroy()
{
	if (mDrawerWidth < DRAWER_MIN_W) mDrawerWidth = DRAWER_MIN_W;
	if (mDrawerWidth > DRAWER_MAX_W) mDrawerWidth = DRAWER_MAX_W;

	AfxGetApp()->WriteProfileInt(_T("Drawer"), _T("Visible"), mDrawerVisible ? 1 : 0);
	AfxGetApp()->WriteProfileInt(_T("Drawer"), _T("Width"), mDrawerWidth);

	CFrameWnd::OnDestroy();
}

void CMainFrame::RefreshView()
{
	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());

	if (pDoc->mDocState != DOC_JUSTLOAD) {
		pDoc->LoadSourceImage();
		Invalidate(FALSE);
	} else {
		CViewerView *pView = static_cast<CViewerView *>(GetActiveView());

		pView->AdjustWindowSize();
	}
}

void CMainFrame::OnResolutionChange(UINT nID)
{
	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());

	CString str;
	CMenu *subMenu = GetMenu()->GetSubMenu(MENU_POS_RESOLUTION);
	subMenu->GetMenuString(nID, str, MF_BYCOMMAND);

	int w = 0, h = 0;
	int error = q1::image_parse_w_h(CT2A(str), &w, &h);
	if (error) {
		w = pDoc->mW;
		h = pDoc->mH;

		CCustomSizeDlg dlg(w, h);

		if (IDOK != dlg.DoModal())
			return;
	}

	ApplyResolution(w, h);
}

void CMainFrame::NextResolution()
{
	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());

	// Sources with an intrinsic size (decoded images, video) own their
	// resolution; only raw input can be reinterpreted at a different size.
	FrmSrc *frmSrc = pDoc->mFrmSrc;
	if (frmSrc && frmSrc->isFixed())
		return;

	int w = 0, h = 0;
	q1::image_next_resolution(pDoc->mW, pDoc->mH, &w, &h);

	ApplyResolution(w, h);
}

void CMainFrame::ApplyResolution(int w, int h)
{
	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());

	if (pDoc->mW == w && pDoc->mH == h) // Do nothing, if same
		return;

	UpdateResolutionLabel(w, h);
	CheckResolutionRadio(w, h);

	pDoc->mW = w;
	pDoc->mH = h;

	RefreshView();

	ViewerSyncInputState input = {};
	input.command = VIEWER_SYNC_RESOLUTION;
	input.first = w;
	input.second = h;
	BroadcastSyncInput(input);
}

void CMainFrame::OnCsChange(UINT nID)
{
	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());

	CString str;
	CMenu *subMenu = GetMenu()->GetSubMenu(MENU_POS_COLORSPACE);
	subMenu->GetMenuString(nID, str, MF_BYCOMMAND);

	str.MakeLower();
	const struct qcsc_info * const ci =
		q1::image_find_cs(pDoc->mSortedCscInfo, CT2A(str));
	if (ci == NULL) {
		LOGERR("couldn't get the right index of color space");
		return;
	}

	if (ci->cs == pDoc->mColorSpace)
		return;

	UpdateCs(ci);

	ViewerSyncInputState input = {};
	input.command = VIEWER_SYNC_COLOR_SPACE;
	input.first = ci->cs;
	BroadcastSyncInput(input);
}

void CMainFrame::OnFpsChange(UINT nID)
{
	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());

	CString str;
	CMenu *subMenu = GetMenu()->GetSubMenu(MENU_POS_FPS);
	subMenu->GetMenuString(nID, str, MF_BYCOMMAND);

	double fps = _wtof(str);
	if (fps == 0) {
		fps = pDoc->mFps;

		CCustomFpsDlg dlg(fps);

		if (IDOK != dlg.DoModal())
			return;
	}

	if (pDoc->mFps == fps)
		return;

	CViewerView *pView = static_cast<CViewerView *>(GetActiveView());
	bool wasPlaying = false;
	if (pView->mIsPlaying) {
		wasPlaying = true;
		pView->KillPlayTimerSafe();
	}

	UpdateFpsLabel(fps);
	CheckFpsRadio(fps);

	pDoc->mFps = fps;
	pDoc->mHasTimingFps = fps > 0.0;

	DrawMenuBar();

	if (wasPlaying)
		pView->SetPlayTimer(pDoc);

	ViewerSyncInputState input = {};
	input.command = VIEWER_SYNC_FPS;
	input.scalar = fps;
	BroadcastSyncInput(input);
}

void CMainFrame::UpdateMagnication(float n, int wDst, int hDst)
{
	if (GetMenu() == NULL)
		return;

	CString str;
	str.Format(_T("%dx%d (%.2fx)"), wDst, hDst, n);

	GetMenu()->ModifyMenu(ID_MAGNIFY, MF_BYCOMMAND | MF_RIGHTJUSTIFY | MF_GRAYED, ID_MAGNIFY, str);
	DrawMenuBar();
}

void CMainFrame::ToggleSyncInput()
{
	mSyncInput = !mSyncInput;
	if (mSyncInput) {
		::SetProp(GetSafeHwnd(), VIEWER_SYNC_INPUT_PROPERTY,
			reinterpret_cast<HANDLE>(1));
	} else {
		::RemoveProp(GetSafeHwnd(), VIEWER_SYNC_INPUT_PROPERTY);
		mSyncViewStatePending = false;
	}
}

void CMainFrame::BroadcastSyncInput(const ViewerSyncInputState &input)
{
	if (!mSyncInput || VIEWER_SYNC_INPUT_TOKEN == 0)
		return;

	ViewerSyncBroadcast broadcast = {};
	broadcast.source = GetSafeHwnd();
	broadcast.copyData.dwData = VIEWER_SYNC_INPUT_TOKEN;
	broadcast.copyData.cbData = sizeof(input);
	broadcast.copyData.lpData = const_cast<ViewerSyncInputState *>(&input);
	::EnumWindows(SendViewerSyncInput,
		reinterpret_cast<LPARAM>(&broadcast));
}

BOOL CMainFrame::OnCopyData(CWnd *pWnd, COPYDATASTRUCT *pCopyDataStruct)
{
	if (!mSyncInput || pCopyDataStruct == NULL ||
			pCopyDataStruct->dwData != VIEWER_SYNC_INPUT_TOKEN ||
			pCopyDataStruct->cbData != sizeof(ViewerSyncInputState) ||
			pCopyDataStruct->lpData == NULL) {
		return CFrameWnd::OnCopyData(pWnd, pCopyDataStruct);
	}

	const ViewerSyncInputState *incoming =
		static_cast<const ViewerSyncInputState *>(pCopyDataStruct->lpData);
	if (incoming->command == VIEWER_SYNC_VIEW_STATE) {
		mPendingSyncViewState = *incoming;
		if (!mSyncViewStatePending) {
			mSyncViewStatePending = true;
			PostMessage(WM_APPLY_SYNC_VIEW_STATE);
		}
		return TRUE;
	}

	ViewerSyncInputState *pending = new ViewerSyncInputState(*incoming);
	if (!PostMessage(WM_APPLY_SYNC_INPUT, 0,
			reinterpret_cast<LPARAM>(pending))) {
		delete pending;
		return FALSE;
	}

	return TRUE;
}

LRESULT CMainFrame::OnApplySyncInput(WPARAM wParam, LPARAM lParam)
{
	ViewerSyncInputState *input =
		reinterpret_cast<ViewerSyncInputState *>(lParam);
	if (input == NULL)
		return 0;

	if (!mSyncInput) {
		delete input;
		return 0;
	}

	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());
	CViewerView *pView = static_cast<CViewerView *>(GetActiveView());

	switch (input->command) {
	case VIEWER_SYNC_RESOLUTION:
		if (pDoc != NULL && pDoc->mDocState != DOC_JUSTLOAD &&
				(!pDoc->mFrmSrc || !pDoc->mFrmSrc->isFixed()) &&
				(pDoc->mW != input->first || pDoc->mH != input->second)) {
			UpdateResolutionLabel(input->first, input->second);
			CheckResolutionRadio(input->first, input->second);
			pDoc->mW = input->first;
			pDoc->mH = input->second;
			RefreshView();
		}
		break;
	case VIEWER_SYNC_COLOR_SPACE:
		if (pDoc != NULL && pDoc->mDocState != DOC_JUSTLOAD &&
				pDoc->mColorSpace != input->first) {
			for (unsigned int i = 0; i < ARRAY_SIZE(qcsc_info_table); i++) {
				if (qcsc_info_table[i].cs == input->first) {
					UpdateCs(&qcsc_info_table[i]);
					break;
				}
			}
		}
		break;
	case VIEWER_SYNC_FPS:
		if (pDoc != NULL && pView != NULL &&
				pDoc->mDocState != DOC_JUSTLOAD &&
				pDoc->mFps != input->scalar) {
			bool wasPlaying = pView->mIsPlaying;
			if (wasPlaying)
				pView->KillPlayTimerSafe();

			pDoc->mFps = input->scalar;
			pDoc->mHasTimingFps = input->scalar > 0.0;
			UpdateFpsLabel(pDoc->mFps);
			CheckFpsRadio(pDoc->mFps);
			DrawMenuBar();

			if (wasPlaying)
				pView->SetPlayTimer(pDoc);
		}
		break;
	default:
		if (pView != NULL)
			pView->ApplySyncInput(*input);
		break;
	}

	delete input;
	return 0;
}

LRESULT CMainFrame::OnApplySyncViewState(WPARAM wParam, LPARAM lParam)
{
	mSyncViewStatePending = false;
	if (!mSyncInput)
		return 0;

	CViewerView *pView = static_cast<CViewerView *>(GetActiveView());
	if (pView != NULL)
		pView->ApplySyncInput(mPendingSyncViewState);

	return 0;
}

// Resolves the path to Comparator.exe, preferring the copy installed next to
// Viewer.exe and falling back to the local debug build tree. Returns FALSE when
// it cannot be located.
BOOL CMainFrame::ResolveComparatorPath(CString &cmperPath)
{
	TCHAR viewerPath[MAX_PATH] = {0, };
	DWORD viewerPathLength = ::GetModuleFileName(NULL, viewerPath, _countof(viewerPath));
	if (viewerPathLength == 0 || viewerPathLength >= _countof(viewerPath) ||
			!::PathRemoveFileSpec(viewerPath))
		return FALSE;

	cmperPath = viewerPath;
	cmperPath += _T("\\Comparator.exe");

	if (::PathFileExists(cmperPath))
		return TRUE;

#ifdef _DEBUG
	TCHAR curDir[MAX_PATH] = {0, };
	if (::GetCurrentDirectory(_countof(curDir), curDir) != 0) {
		cmperPath = curDir;
		cmperPath += _T("\\..\\Comparator\\x64\\Debug\\Comparator.exe");

		if (::PathFileExists(cmperPath))
			return TRUE;
	}
#endif

	return FALSE;
}

// Launches cmperPath with quotedArgs (each path already double-quoted and
// space-prefixed) appended to the command line. Returns the CreateProcess result.
BOOL CMainFrame::LaunchComparator(const CString &cmperPath, const CString &quotedArgs)
{
	STARTUPINFO startUpInfo = {0, };
	PROCESS_INFORMATION processInfo = {0, };
	startUpInfo.cb = sizeof(STARTUPINFO);

	CString cmdLine;
	cmdLine.Format(_T("\"%s\"%s"), cmperPath.GetString(), quotedArgs.GetString());

	BOOL ret = ::CreateProcess(cmperPath,
		cmdLine.GetBuffer(), NULL, NULL, FALSE, 0, NULL, NULL,
		&startUpInfo, &processInfo);
	cmdLine.ReleaseBuffer();
	if (!ret)
		return FALSE;

	::CloseHandle(processInfo.hThread);
	::CloseHandle(processInfo.hProcess);
	return TRUE;
}

void CMainFrame::OnExecComparator()
{
	CString cmperPath;
	if (!ResolveComparatorPath(cmperPath)) {
		MessageBox(_T("Couldn't find 'Comparator.exe' next to Viewer.exe."), _T("Warning"), MB_ICONWARNING);
		return;
	}

	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());

	CString args;
	if (!pDoc->mPathName.IsEmpty())
		args.Format(_T(" \"%s\""), pDoc->mPathName.GetString());

	if (!LaunchComparator(cmperPath, args)) {
		CString msg;
		msg.Format(_T("Failed to execute Comparator.exe. (error %lu)"), ::GetLastError());
		MessageBox(msg, _T("Warning"), MB_ICONWARNING);
	}
}

// Store update worker reported that an update is available: surface a compact
// "Update" item just left of the right-aligned "Compare" item in the menu bar.
LRESULT CMainFrame::OnStoreUpdateAvailable(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	if (mUpdateMenuShown || GetMenu() == NULL)
		return 0;

	// MF_RIGHTJUSTIFY keeps the item in the right-aligned group; inserting it
	// before ID_COMPARE places it immediately to the left of "Compare".
	GetMenu()->InsertMenu(ID_COMPARE, MF_BYCOMMAND | MF_STRING | MF_RIGHTJUSTIFY,
		ID_UPDATE, _T("&Update"));
	mUpdateMenuShown = true;
	DrawMenuBar();
	return 0;
}

void CMainFrame::OnUpdateNow()
{
	if (MessageBox(
			_T("A new version of Q1View is available in the Microsoft Store.\n\n")
			_T("Download and install it now? Q1View will restart to finish."),
			_T("Update available"), MB_ICONINFORMATION | MB_OKCANCEL) != IDOK)
		return;

	// Reflect progress on the menu label and avoid re-entrancy.
	if (GetMenu() != NULL) {
		GetMenu()->ModifyMenu(ID_UPDATE,
			MF_BYCOMMAND | MF_STRING | MF_RIGHTJUSTIFY | MF_GRAYED,
			ID_UPDATE, _T("Updating..."));
		DrawMenuBar();
	}

	q1::store::DownloadAndInstallAsync(GetSafeHwnd(), WM_STORE_UPDATE_DONE);
}

LRESULT CMainFrame::OnStoreUpdateDone(WPARAM wParam, LPARAM /*lParam*/)
{
	const bool ok = (wParam != 0);
	if (ok) {
		MessageBox(_T("Update installed. Q1View will now restart."),
			_T("Update complete"), MB_ICONINFORMATION | MB_OK);

		TCHAR exePath[MAX_PATH] = { 0 };
		if (::GetModuleFileName(NULL, exePath, _countof(exePath)) > 0)
			::ShellExecute(NULL, _T("open"), exePath, NULL, NULL, SW_SHOWNORMAL);
		PostMessage(WM_CLOSE);
		return 0;
	}

	// Restore the button so the user can retry.
	if (GetMenu() != NULL) {
		GetMenu()->ModifyMenu(ID_UPDATE,
			MF_BYCOMMAND | MF_STRING | MF_RIGHTJUSTIFY,
			ID_UPDATE, _T("&Update"));
		DrawMenuBar();
	}
	MessageBox(
		_T("The update could not be completed. Please try again later, or ")
		_T("update Q1View from the Microsoft Store."),
		_T("Update failed"), MB_ICONWARNING | MB_OK);
	return 0;
}

// Dropping two or more files implies an intent to compare them, so route them to
// Comparator. A single file keeps the standard Viewer load. If Comparator can't be
// located or launched, fall back to the default (first-file) drop behavior.
void CMainFrame::OnDropFiles(HDROP hDropInfo)
{
	UINT uDragCount = ::DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);

	CString cmperPath;
	if (uDragCount < 2 || !ResolveComparatorPath(cmperPath)) {
		CFrameWnd::OnDropFiles(hDropInfo);
		return;
	}

	CString args;
	for (UINT i = 0; i < uDragCount; ++i) {
		TCHAR szPathName[MAX_PATH] = {0, };
		::DragQueryFile(hDropInfo, i, szPathName, _countof(szPathName));
		args.AppendFormat(_T(" \"%s\""), szPathName);
	}

	if (!LaunchComparator(cmperPath, args)) {
		CFrameWnd::OnDropFiles(hDropInfo);
		return;
	}

	::DragFinish(hDropInfo);
}

void CMainFrame::ActivateFrame(int nCmdShow)
{
	CFrameWnd::ActivateFrame(nCmdShow);

	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());
	if (pDoc->mPendingFile != "") {
		pDoc->OnOpenDocument(pDoc->mPendingFile);
		pDoc->mPendingFile = "";
	}
}

void CMainFrame::OnEditCopy()
{
	CViewerView *pView = static_cast<CViewerView *>(GetActiveView());
	if (!pView->mStableRgbBufferInfo.addr)
		return;

	CDC dc, memDC;
	CBitmap bitmap;
	dc.CreateDCW(_T("DISPLAY"), NULL, NULL, NULL);
	memDC.CreateCompatibleDC(&dc);
	cv::Mat roi = pView->GetRoiMat();
	bitmap.CreateCompatibleBitmap(&dc, roi.cols, roi.rows);
	CBitmap *pBitmap = memDC.SelectObject(&bitmap);

	BITMAPINFOHEADER &bmiHeader = mCopyBmi.bmiHeader;
	bmiHeader.biWidth = ROUNDUP_DWORD(roi.cols);
	bmiHeader.biHeight = -roi.rows;
	SetDIBitsToDevice(memDC.m_hDC,
		0, 0, roi.cols, roi.rows,
		0, 0, 0, roi.rows, roi.data, &mCopyBmi, DIB_RGB_COLORS);

	pView->OpenClipboard();
	::EmptyClipboard();
	::SetClipboardData(CF_BITMAP, bitmap.GetSafeHandle());
	::CloseClipboard();

	memDC.SelectObject(pBitmap);
	bitmap.Detach();
}

void CMainFrame::OnEditPaste()
{
	CViewerView *pView = static_cast<CViewerView *>(GetActiveView());
	if (pView == NULL || !pView->OpenClipboard())
		return;

	// Prefer the device-independent bitmap. Going through CF_BITMAP (a DDB) and
	// CImage loses or garbles the pixels for many sources; CF_DIB carries the
	// real header plus bits, which we can write straight out as a .bmp.
	HANDLE hDib = ::GetClipboardData(CF_DIB);
	if (hDib == NULL)
		hDib = ::GetClipboardData(CF_DIBV5);
	if (hDib == NULL) {
		::CloseClipboard();
		return;
	}

	const BYTE *dib = static_cast<const BYTE *>(::GlobalLock(hDib));
	SIZE_T dibSize = ::GlobalSize(hDib);
	if (dib == NULL || dibSize < sizeof(BITMAPINFOHEADER)) {
		if (dib != NULL)
			::GlobalUnlock(hDib);
		::CloseClipboard();
		return;
	}

	// The clipboard DIB has no BITMAPFILEHEADER; synthesize one. bfOffBits must
	// skip the info header plus any color table / BI_BITFIELDS masks.
	const BITMAPINFOHEADER *bih = reinterpret_cast<const BITMAPINFOHEADER *>(dib);
	DWORD colorTableSize = 0;
	if (bih->biBitCount <= 8) {
		DWORD colors = bih->biClrUsed ? bih->biClrUsed : (1u << bih->biBitCount);
		colorTableSize = colors * sizeof(RGBQUAD);
	} else if (bih->biCompression == BI_BITFIELDS &&
			bih->biSize == sizeof(BITMAPINFOHEADER)) {
		colorTableSize = 3 * sizeof(DWORD);
	}

	BITMAPFILEHEADER bfh = {};
	bfh.bfType = 0x4D42; // 'BM'
	bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + bih->biSize + colorTableSize;
	bfh.bfSize = sizeof(BITMAPFILEHEADER) + static_cast<DWORD>(dibSize);

	// Write to an absolute, writable temp path. The old code used a relative,
	// extension-less name in the process working directory, which is not
	// reliably writable for the packaged (Store) app, so a stale/unrelated file
	// could end up being opened instead of the freshly pasted image.
	TCHAR tempDir[MAX_PATH];
	DWORD len = ::GetTempPath(MAX_PATH, tempDir);
	BOOL ok = FALSE;
	CString tempFile;
	if (len > 0 && len <= MAX_PATH) {
		tempFile.Format(_T("%sQ1ViewClipboard.bmp"), tempDir);
		CFile file;
		CFileException e;
		if (file.Open(tempFile,
				CFile::modeCreate | CFile::modeWrite | CFile::typeBinary, &e)) {
			try {
				file.Write(&bfh, sizeof(bfh));
				file.Write(dib, static_cast<UINT>(dibSize));
				ok = TRUE;
			} catch (CFileException *pe) {
				pe->Delete();
			}
			file.Close();
		}
	}

	::GlobalUnlock(hDib);
	::CloseClipboard();

	if (!ok) {
		AfxMessageBox(_T("Could not save the clipboard image to a temporary file."));
		return;
	}

	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());
	pDoc->OnOpenDocument(tempFile);
	pDoc->SetPathName(tempFile, TRUE);
	UpdateMagnication(pView->mN, pView->mWDst, pView->mHDst);

	Invalidate(FALSE);
}

void CMainFrame::UpdateCs(const struct qcsc_info* const ci)
{
	CViewerDoc* pDoc = static_cast<CViewerDoc*>(GetActiveDocument());

	UpdateCsLabel(CString(ci->name).MakeUpper());
	CheckCsRadio(ci->cs);

	pDoc->mColorSpace = ci->cs;
	pDoc->mCsc2Rgb888 = ci->csc2rgb888;
	pDoc->mCsLoadInfo = ci->cs_load_info;
	pDoc->mSampleNativePixel = ci->sample_native_pixel;

	RefreshView();
}
