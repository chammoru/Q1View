
// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "Comparator.h"

#include "ComparatorDoc.h"
#include "ComparatorView.h"
#include "ComparatorViews.h"
#include "MainFrm.h"

#include "FrmsInfoView.h"
#include "FrmInfoView.h"
#include "PosInfoView.h"

#include "FrmCmpInfo.h"
#include "FileScanThread.h"

#include "CustomSizeDlg.h"
#include "CustomFpsDlg.h"
#include "qimage_util.h"

#include <QImageStr.h>
#include <QViewerCmn.h>
#include <QDebug.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#ifdef _DEBUG
#ifdef UNICODE
#pragma comment(linker, "/entry:wWinMainCRTStartup /subsystem:console")
#else
#pragma comment(linker, "/entry:WinMainCRTStartup /subsystem:console")
#endif
#endif

// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
	ON_COMMAND_RANGE(ID_RESOLUTION_START, ID_RESOLUTION_END, CMainFrame::OnResolutionChange)
	ON_COMMAND_RANGE(ID_METRIC_START, ID_METRIC_END, CMainFrame::OnMetricChange)
	ON_COMMAND_RANGE(ID_FPS_START, ID_FPS_END, &CMainFrame::OnFpsChange)
	ON_COMMAND_RANGE(ID_VIEWS_START, ID_VIEWS_END, &CMainFrame::OnViewsChange)
	ON_COMMAND_RANGE(ID_OPTIONS_START, ID_OPTIONS_END, &CMainFrame::OnOptionsChange)
	ON_WM_SIZE()
	ON_WM_TIMER()
	ON_WM_DESTROY()
	ON_WM_CREATE()
	ON_MESSAGE(WM_OPEN_PENDING_FILE, &CMainFrame::OnOpenPendingFile)
	ON_MESSAGE(WM_RELOAD_PANE, &CMainFrame::OnReloadPane)
END_MESSAGE_MAP()

CMainFrame::CMainFrame()
: mSplitMargin(0)
, mSplitBarW(0)
, mMetricIdx(METRIC_PSNR_IDX)
, mNumOfViews(COMPARER_DEF_VIEWS)
{
	mResolutionMenu.CreatePopupMenu();
	mMetricMenu.CreatePopupMenu();
	mFpsMenu.CreatePopupMenu();
	mViewsMenu.CreatePopupMenu();
	mOptionsMenu.CreatePopupMenu();
}

CMainFrame::~CMainFrame()
{
	mViewsMenu.DestroyMenu();
	mFpsMenu.DestroyMenu();
	mMetricMenu.DestroyMenu();
	mResolutionMenu.DestroyMenu();
	mOptionsMenu.DestroyMenu();
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CFrameWnd::PreCreateWindow(cs))
		return FALSE;

	cs.style &= ~FWS_ADDTOTITLE;

	CRect rcClient(0, 0, COMPARER_DEF_W, COMPARER_DEF_H);
	::AdjustWindowRectEx(&rcClient, cs.style, TRUE, cs.dwExStyle);

	cs.cx = rcClient.Width();
	cs.cy = rcClient.Height();

	return TRUE;
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


BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	CSize sz;
	CRect rcClient;
	GetClientRect(&rcClient);

	if (!mGraphSplitter.CreateStatic(this, 2, 1))
		return FALSE;

	if (!mPosInfoSplitter.CreateStatic(&mGraphSplitter, 1, 2,
			WS_CHILD | WS_VISIBLE, mGraphSplitter.IdFromRowCol(0, 0)))
		return FALSE;

	sz.SetSize(POS_INFO_W, rcClient.Height() - FRAMES_INFO_H);
	if (!mPosInfoSplitter.CreateView(0, 0, RUNTIME_CLASS(CPosInfoView), sz, pContext))
		return FALSE;

	if (!mFrmInfoSplitter.CreateStatic(&mPosInfoSplitter, 2, 1,
			WS_CHILD | WS_VISIBLE, mPosInfoSplitter.IdFromRowCol(0, 1)))
		return FALSE;

	if (!mCompSplitter.CreateStatic(&mFrmInfoSplitter, 1, CComparatorDoc::IMG_VIEW_MAX,
			WS_CHILD | WS_VISIBLE, mFrmInfoSplitter.IdFromRowCol(0, 0)))
		return FALSE;

	sz.SetSize((rcClient.Width() - POS_INFO_W) / mNumOfViews,
		rcClient.Height() - FRAMES_INFO_H - FRAME_INFO_H);
	if (!mCompSplitter.CreateView(0, 0, RUNTIME_CLASS(CComparatorView1), sz, pContext) ||
		!mCompSplitter.CreateView(0, 1, RUNTIME_CLASS(CComparatorView2), sz, pContext) ||
		!mCompSplitter.CreateView(0, 2, RUNTIME_CLASS(CComparatorView3), sz, pContext) ||
		!mCompSplitter.CreateView(0, 3, RUNTIME_CLASS(CComparatorView4), sz, pContext))
		return FALSE;

	sz.SetSize(rcClient.Width() - POS_INFO_W, FRAME_INFO_H);
	if (!mFrmInfoSplitter.CreateView(1, 0, RUNTIME_CLASS(CFrmInfoView), sz, pContext))
		return FALSE;

	sz.SetSize(rcClient.Width(), FRAMES_INFO_H);
	if (!mGraphSplitter.CreateView(1, 0, RUNTIME_CLASS(CFrmsInfoView), sz, pContext))
		return FALSE;

	return TRUE;
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	CFrameWnd::OnSize(nType, cx, cy);

	if (cx <= 0)
		return;

	int hOutside, hInside, wOutside, wInside;
	int realW0, realW1, ignore;

	if (!mGraphSplitter.IsWindowVisible())
		return;

	hOutside = cy - FRAMES_INFO_H - mSplitMargin;
	mGraphSplitter.SetRowInfo(0, hOutside, MIN_SIDE);
	mGraphSplitter.SetRowInfo(1, FRAMES_INFO_H, MIN_SIDE);
	mGraphSplitter.RecalcLayout();

	if (mSplitMargin == 0) {
		mGraphSplitter.GetRowInfo(1, realW1, ignore);

		mSplitMargin = FRAMES_INFO_H - realW1;

		hOutside = cy - FRAMES_INFO_H - mSplitMargin;
		mGraphSplitter.SetRowInfo(0, hOutside, MIN_SIDE);
		mGraphSplitter.SetRowInfo(1, FRAMES_INFO_H, MIN_SIDE);
		mGraphSplitter.RecalcLayout();
	}

	wOutside = cx - POS_INFO_W - mSplitMargin;
	mPosInfoSplitter.SetColumnInfo(0, POS_INFO_W, MIN_SIDE);
	mPosInfoSplitter.SetColumnInfo(1, wOutside, MIN_SIDE);
	mPosInfoSplitter.RecalcLayout();

	hInside = hOutside - FRAME_INFO_H - mSplitBarW;
	mFrmInfoSplitter.SetRowInfo(0, hInside, MIN_SIDE);
	mFrmInfoSplitter.SetRowInfo(1, FRAME_INFO_H, MIN_SIDE);
	mFrmInfoSplitter.RecalcLayout();

	if (mSplitBarW == 0) {
		mFrmInfoSplitter.GetRowInfo(0, realW0, ignore);
		mFrmInfoSplitter.GetRowInfo(1, realW1, ignore);
		mSplitBarW = hOutside - realW0 - realW1;

		hInside = hOutside - FRAME_INFO_H - mSplitBarW;
		mFrmInfoSplitter.SetRowInfo(0, hInside, MIN_SIDE);
		mFrmInfoSplitter.SetRowInfo(1, FRAME_INFO_H, MIN_SIDE);
		mFrmInfoSplitter.RecalcLayout();
	}

	wInside = (wOutside - mSplitBarW * (mNumOfViews - 1)) / mNumOfViews;
	int i = 0;
	for (; i < mNumOfViews; i++) {
		mCompSplitter.SetColumnInfo(i, wInside, MIN_SIDE);
	}
	for (; i < CComparatorDoc::IMG_VIEW_MAX; i++) {
		mCompSplitter.SetColumnInfo(i, 0, 0);
	}
	mCompSplitter.RecalcLayout();
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	CComparatorDoc *pDoc = static_cast<CComparatorDoc *>(GetActiveDocument());

	CPosInfoView *pPosInfoView;
	CFrmsInfoView *pFrmsInfoView;
	FileScanThread *fileScanThread;

	switch (nIDEvent) {
	case CTI_ID_POS_INVALIDATE:
		pPosInfoView = pDoc->mPosInfoView;
		pFrmsInfoView = pDoc->mFrmsInfoView;

		pPosInfoView->Invalidate(FALSE);
		pFrmsInfoView->Invalidate(FALSE);

		// A lazy metric (e.g. LPIPS) is filled by a background worker after the
		// base scan, so refresh its current-frame label live and keep the timer
		// running until that worker also finishes.
		if (pDoc->IsLpipsScanRunning()) {
			pDoc->UpdateCurrentMetricState(mMetricIdx);
			if (pDoc->mFrmInfoView)
				pDoc->mFrmInfoView->Invalidate(FALSE);
		}

		fileScanThread = pDoc->mFileScanThread;
		if (fileScanThread->isScanComplete() && !pDoc->IsLpipsScanRunning()) {
			KillTimer(CTI_ID_POS_INVALIDATE);
			// One last label refresh so a value/"N/A" replaces "computing...".
			pDoc->UpdateCurrentMetricState(mMetricIdx);
			if (pDoc->mFrmInfoView)
				pDoc->mFrmInfoView->Invalidate(FALSE);
		}

		break;
	case CTI_ID_PLAY:
		bool isProcessing = pDoc->CheckImgViewProcessing();
		if (isProcessing)
			break;

		bool updated = pDoc->NextScenes();
		pDoc->MarkImgViewProcessing();

		// Updating only the image panes avoids the cost of redrawing every view.
		for (int i = 0; i < mNumOfViews; i++) {
			CComparatorView *pView = pDoc->mPane[i].pView;
			pView->Invalidate(FALSE);
		}
		pPosInfoView = pDoc->mPosInfoView;
		pPosInfoView->Invalidate(FALSE);

		if (!updated)
			pDoc->KillPlayTimer();

		break;
	}

	CFrameWnd::OnTimer(nIDEvent);
}

void CMainFrame::OnDestroy()
{
	CFrameWnd::OnDestroy();

	for (int i = 0; i < CTI_ID_MAX; i++)
		KillTimer(i);
}

#define MENU_POS_RESOLUTION       1
#define MENU_POS_METRIC           2
#define MENU_POS_FPS              3
#define MENU_POS_VIEWS            4
#define MENU_POS_OPTIONS          5

void CMainFrame::CheckResolutionRadio(int w, int h)
{
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

void CMainFrame::UpdateResolutionLabel(int w, int h)
{
	if (GetMenu() == NULL)
		return;

	CComparatorDoc* pDoc = static_cast<CComparatorDoc*>(GetActiveDocument());

	UINT nFlags = MF_BYPOSITION | MF_POPUP;
	if (pDoc->isFixedResolution())
		nFlags |= MF_DISABLED;
	else
		nFlags |= MF_ENABLED;

	CString str;
	str.Format(_T("%d&x%d"), w, h);
	GetMenu()->ModifyMenu(MENU_POS_RESOLUTION, nFlags,
		(UINT_PTR)mResolutionMenu.m_hMenu, str);
}

void CMainFrame::UpdateFpsLabel(double fps)
{
	if (GetMenu() == NULL)
		return;

	CComparatorDoc* pDoc = static_cast<CComparatorDoc*>(GetActiveDocument());

	CString str;
	str.Format(_T("%.2ff&ps"), fps);
	GetMenu()->ModifyMenu(MENU_POS_FPS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mFpsMenu.m_hMenu, str);
}

void CMainFrame::UpdateViewsLabel(int views)
{
	if (GetMenu() == NULL)
		return;

	CComparatorDoc* pDoc = static_cast<CComparatorDoc*>(GetActiveDocument());

	CString str;
	str.Format(_T("%d VIEWS"), views);
	GetMenu()->ModifyMenu(MENU_POS_VIEWS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mViewsMenu.m_hMenu, str);
}

void CMainFrame::OnResolutionChange(UINT nID)
{
	CComparatorDoc *pDoc = static_cast<CComparatorDoc *>(GetActiveDocument());

	CString str;
	CMenu *subMenu = GetMenu()->GetSubMenu(MENU_POS_RESOLUTION);
	subMenu->GetMenuString(nID, str, MF_BYCOMMAND);

	int w = 0, h = 0;
	int error = q1::image_parse_w_h(CT2A(str), &w, &h);
	if (error != 0) {
		w = pDoc->mW;
		h = pDoc->mH;

		CCustomSizeDlg dlg(w, h);

		if (IDOK != dlg.DoModal())
			return;
	}

	if (pDoc->mW == w && pDoc->mH == h) // Do nothing, if same
		return;

	UpdateResolutionLabel(w, h);
	CheckResolutionRadio(w, h);

	pDoc->mW = w;
	pDoc->mH = h;

	DrawMenuBar();
	RefreshAllViews();
}

void CMainFrame::OnFpsChange(UINT nID)
{
	CComparatorDoc* pDoc = static_cast<CComparatorDoc*>(GetActiveDocument());

	CString str;
	CMenu* subMenu = GetMenu()->GetSubMenu(MENU_POS_FPS);
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

	UpdateFpsLabel(fps);
	CheckFpsRadio(fps);

	pDoc->mFps = fps;

	DrawMenuBar();
}

void CMainFrame::OnViewsChange(UINT nID)
{
	CString str;
	CMenu* subMenu = GetMenu()->GetSubMenu(MENU_POS_VIEWS);
	subMenu->GetMenuString(nID, str, MF_BYCOMMAND);

	int views = _wtoi(str);

	if (mNumOfViews == views)
		return;

	int oldNumOfViews = ChangeNumOfViews(views);

	DrawMenuBar();

	CComparatorDoc* pDoc = static_cast<CComparatorDoc*>(GetActiveDocument());
	const CComparatorView* firstView = pDoc->mPane->pView;
	firstView->AdjustWindowSize(oldNumOfViews, mSplitBarW * (mNumOfViews - oldNumOfViews));
}

int CMainFrame::ChangeNumOfViews(int newNumOfViews) {
	CComparatorDoc* pDoc = static_cast<CComparatorDoc*>(GetActiveDocument());

	UpdateViewsLabel(newNumOfViews);
	CheckViewsRadio(newNumOfViews);

	int oldNumOfViews = mNumOfViews;
	mNumOfViews = newNumOfViews;

	for (int i = newNumOfViews; i < CComparatorDoc::IMG_VIEW_MAX; i++) {
		ComparatorPane *pPane = pDoc->mPane + i;
		CComparatorView* pView = pPane->pView;
		if (pView != nullptr) {
			pView->UpdateFileName(_T(""));
		}
		pPane->Release();
	}

	if (IsZoomed())
		SendMessage(WM_SYSCOMMAND, SC_RESTORE, (LPARAM)GetSafeHwnd());

	return oldNumOfViews;
}

void CMainFrame::OnOptionsChange(UINT nID)
{
	CComparatorDoc* pDoc = static_cast<CComparatorDoc*>(GetActiveDocument());

	switch (nID) {
	case ID_OPTIONS_DIFF_RESOLUTION:
		pDoc->mDiffRes = !pDoc->mDiffRes;
		CheckOptionsRadio(nID, pDoc->mDiffRes);
		break;
	default:
		LOGWRN("No matching nID for %x", nID);
		break;
	}
}

void CMainFrame::CheckMetricRadio()
{
	CMenu *subMenu = GetMenu()->GetSubMenu(MENU_POS_METRIC);
	subMenu->CheckMenuRadioItem(ID_METRIC_START, ID_METRIC_END,
		ID_METRIC_START + mMetricIdx, MF_CHECKED | MF_BYCOMMAND);
}

void CMainFrame::UpdateMetricLabel()
{
	CString metric = CA2W(qmetric_info_table[mMetricIdx].name);

	GetMenu()->ModifyMenu(MENU_POS_METRIC, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mMetricMenu.m_hMenu, metric);
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

	CMenu* subMenu = GetMenu()->GetSubMenu(MENU_POS_FPS);

	subMenu->CheckMenuRadioItem(ID_FPS_START, ID_FPS_END,
		id, MF_CHECKED | MF_BYCOMMAND);
}

void CMainFrame::CheckViewsRadio(int numOfViews)
{
	if (GetMenu() == NULL)
		return;

	UINT id = ID_VIEWS_START + numOfViews - COMPARER_DEF_VIEWS;

	CMenu* subMenu = GetMenu()->GetSubMenu(MENU_POS_VIEWS);

	subMenu->CheckMenuRadioItem(ID_VIEWS_START, ID_VIEWS_END,
		id, MF_CHECKED | MF_BYCOMMAND);
}

void CMainFrame::CheckOptionsRadio(UINT nOptionID, bool checked)
{
	if (GetMenu() == NULL)
		return;

	CMenu* subMenu = GetMenu()->GetSubMenu(MENU_POS_OPTIONS);

	UINT checkFlag = checked ? MF_CHECKED : MF_UNCHECKED;
	subMenu->CheckMenuItem(nOptionID, checkFlag | MF_BYCOMMAND);
}

void CMainFrame::UpdateMagnication(float n)
{
	CString str;

	str.Format(_T("%.2fx"), n);

	GetMenu()->ModifyMenu(ID_MAGNIFY, MF_BYCOMMAND | MF_RIGHTJUSTIFY | MF_GRAYED, ID_MAGNIFY, str);
	DrawMenuBar();
}

void CMainFrame::OnMetricChange(UINT nID)
{
	mMetricIdx = nID - ID_METRIC_START;

	UpdateMetricLabel();
	CheckMetricRadio();

	DrawMenuBar();

	// A metric switch is metric-only: all per-plane metrics are already cached
	// and a lazy metric (LPIPS) is started by SelectMetric, which also handles
	// view invalidation. Do NOT call RefreshAllViews() here -- it reloads the
	// images and restarts the base scan, which would also kill the LPIPS worker.
	CComparatorDoc *pDoc = static_cast<CComparatorDoc *>(GetActiveDocument());
	if (pDoc) {
		pDoc->SelectMetric(mMetricIdx);
	}
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	int i;
	CString str;

	// Resolution menu.
	for (i = 0; i < ARRAY_SIZE(q1::resolution_info_table) - 1; i++) {
		mResolutionMenu.AppendMenu(MF_STRING, (UINT_PTR)ID_RESOLUTION_START + i,
			CA2W(q1::resolution_info_table[i]));
	}

	mResolutionMenu.AppendMenu(MF_SEPARATOR);
	mResolutionMenu.AppendMenu(MF_STRING, (UINT_PTR)ID_RESOLUTION_START + i,
		CA2W(q1::resolution_info_table[i]));

	str.Format(_T("%d&x%d"), CANVAS_DEF_W, CANVAS_DEF_H);
	GetMenu()->InsertMenu(MENU_POS_RESOLUTION, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mResolutionMenu.m_hMenu, str);

	CheckResolutionRadio(CANVAS_DEF_W, CANVAS_DEF_H);

	// Metric menu.
	const qmetric_info* qmi;
	for (i = 0; i < METRIC_COUNT; i++) {
		qmi = qmetric_info_table + i;

		mMetricMenu.AppendMenu(MF_STRING, (UINT_PTR)ID_METRIC_START + i, CA2W(qmi->name));
	}

	// PSNR is the default metric.
	str = qmetric_info_table[mMetricIdx].name;
	GetMenu()->InsertMenu(MENU_POS_METRIC, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mMetricMenu.m_hMenu, str);

	CheckMetricRadio();

	// FPS menu.
	for (i = 0; i < ARRAY_SIZE(qfps_info_table) - 1; i++) {
		mFpsMenu.AppendMenu(MF_STRING, (UINT_PTR)ID_FPS_START + i,
			CA2W(qfps_info_table[i]));
	}

	mFpsMenu.AppendMenu(MF_SEPARATOR);
	mFpsMenu.AppendMenu(MF_STRING, (UINT_PTR)ID_FPS_START + i, CA2W(qfps_info_table[i]));

	str.Format(_T("%0.2ff&ps"), COMPARER_DEF_FPS);
	GetMenu()->InsertMenu(MENU_POS_FPS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mFpsMenu.m_hMenu, str);

	CheckFpsRadio(COMPARER_DEF_FPS);

	// View-count menu.
	for (i = 0; i < CComparatorDoc::IMG_VIEW_MAX - 1; i++) {
		str.Format(_T("%d VIEWS"), i + COMPARER_DEF_VIEWS);
		mViewsMenu.AppendMenu(MF_STRING, (UINT_PTR)ID_VIEWS_START + i, str);
	}
	str.Format(_T("%d VIEWS"), mNumOfViews);
	GetMenu()->InsertMenu(MENU_POS_VIEWS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mViewsMenu.m_hMenu, str);

	CheckViewsRadio(mNumOfViews);

	// Options menu.
	str.Format(_T("Allow Different Resolutions"));
	mOptionsMenu.AppendMenu(MF_STRING, (UINT_PTR)ID_OPTIONS_DIFF_RESOLUTION, str);

	str.Format(_T("OPTIONS"));
	GetMenu()->InsertMenu(MENU_POS_OPTIONS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mOptionsMenu.m_hMenu, str);

	return 0;
}

void CMainFrame::RefreshFrmsInfoView()
{
	CComparatorDoc *pDoc = static_cast<CComparatorDoc *>(GetActiveDocument());
	CFrmsInfoView *frmInfoView = pDoc->mFrmsInfoView;
	frmInfoView->Invalidate(FALSE);
}

void CMainFrame::RefreshAllViews()
{
	CComparatorDoc *pDoc = static_cast<CComparatorDoc *>(GetActiveDocument());

	bool refreshed = false;
	for (int i = 0; i < mNumOfViews; i++) {
		ComparatorPane* pane = &pDoc->mPane[i];
		if (pane->isAvail()) {
			pane->srcW = pDoc->mW;
			pane->srcH = pDoc->mH;
			pDoc->RefleshPaneImages(pane, true);
			refreshed = true;
			break;
		}
	}

	if (refreshed) {
		CPosInfoView* posInfoView = pDoc->mPosInfoView;
		posInfoView->ConfigureScrollSizes(pDoc);

		const CComparatorView* firstView = pDoc->mPane->pView;
		firstView->AdjustWindowSize(mNumOfViews);

		pDoc->UpdateAllViews(NULL);
	}
}

void CMainFrame::ActivateFrame(int nCmdShow)
{
	CFrameWnd::ActivateFrame(nCmdShow);

	CComparatorDoc *pDoc = static_cast<CComparatorDoc *>(GetActiveDocument());
	if (pDoc != NULL && !pDoc->mPendingFile.IsEmpty())
		PostMessage(WM_OPEN_PENDING_FILE);
}

LRESULT CMainFrame::OnOpenPendingFile(WPARAM, LPARAM)
{
	CComparatorDoc *pDoc = static_cast<CComparatorDoc *>(GetActiveDocument());
	if (pDoc == NULL || pDoc->mPendingFile.IsEmpty())
		return 0;

	CComparatorView *firstView = pDoc->mPane[CComparatorDoc::IMG_VIEW_1].pView;
	if (!IsWindowVisible() || pDoc->mPosInfoView == NULL ||
		pDoc->mFrmsInfoView == NULL || firstView == NULL ||
		firstView->mWCanvas <= 0 || firstView->mHCanvas <= 0) {
		PostMessage(WM_OPEN_PENDING_FILE);
		return 0;
	}

	// Multi-source open starts metric calculation and the scan worker; run it
	// only after the startup frame layout supplies usable view dimensions.
	CString pendingFile = pDoc->mPendingFile;
	pDoc->mPendingFile.Empty();
	pDoc->OnOpenDocument(pendingFile);

	return 0;
}

LRESULT CMainFrame::OnReloadPane(WPARAM wParam, LPARAM /*lParam*/)
{
	CComparatorDoc *pDoc = static_cast<CComparatorDoc *>(GetActiveDocument());
	if (pDoc == NULL)
		return 0;

	pDoc->ReloadPane(static_cast<int>(wParam));
	return 0;
}
