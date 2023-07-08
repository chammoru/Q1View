
// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"
#include "Comparer.h"

#include "ComparerDoc.h"
#include "ComparerView.h"
#include "ComparerViews.h"
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

#define ID_OPTIONS_DIFF_RESOLUTION (ID_OPTIONS_START + 0)

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
END_MESSAGE_MAP()

// CMainFrame construction/destruction

CMainFrame::CMainFrame()
: mSplitMargin(0)
, mSplitBarW(0)
, mMetricIdx(METRIC_PSNR_IDX)
, mViews(COMPARER_DEF_VIEWS)
{
	// TODO: add member initialization code here
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
	if( !CFrameWnd::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs
	cs.style &= ~FWS_ADDTOTITLE;

	CRect rcClient(0, 0, COMPARER_DEF_W, COMPARER_DEF_H);
	::AdjustWindowRectEx(&rcClient, cs.style, TRUE, cs.dwExStyle);

	cs.cx = rcClient.Width();
	cs.cy = rcClient.Height();

	return TRUE;
}

// CMainFrame diagnostics

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


// CMainFrame message handlers

BOOL CMainFrame::OnCreateClient(LPCREATESTRUCT lpcs, CCreateContext* pContext)
{
	// TODO: Add your specialized code here and/or call the base class
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

	if (!mCompSplitter.CreateStatic(&mFrmInfoSplitter, 1, CComparerDoc::IMG_VIEW_MAX,
			WS_CHILD | WS_VISIBLE, mFrmInfoSplitter.IdFromRowCol(0, 0)))
		return FALSE;

	sz.SetSize((rcClient.Width() - POS_INFO_W) / mViews,
		rcClient.Height() - FRAMES_INFO_H - FRAME_INFO_H);
	if (!mCompSplitter.CreateView(0, 0, RUNTIME_CLASS(CComparerView1), sz, pContext) ||
		!mCompSplitter.CreateView(0, 1, RUNTIME_CLASS(CComparerView2), sz, pContext) ||
		!mCompSplitter.CreateView(0, 2, RUNTIME_CLASS(CComparerView3), sz, pContext) ||
		!mCompSplitter.CreateView(0, 3, RUNTIME_CLASS(CComparerView4), sz, pContext))
		return FALSE;

	sz.SetSize(rcClient.Width() - POS_INFO_W, FRAME_INFO_H);
	if (!mFrmInfoSplitter.CreateView(1, 0, RUNTIME_CLASS(CFrmInfoView), sz, pContext))
		return FALSE;

	sz.SetSize(rcClient.Width(), FRAMES_INFO_H);
	if (!mGraphSplitter.CreateView(1, 0, RUNTIME_CLASS(CFrmsInfoView), sz, pContext))
		return FALSE;

	return TRUE;

	// commented out for the splitted window
	// return CFrameWnd::OnCreateClient(lpcs, pContext);
}

void CMainFrame::OnSize(UINT nType, int cx, int cy)
{
	CFrameWnd::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here
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

	wInside = (wOutside - mSplitBarW * (mViews - 1)) / mViews;
	int i = 0;
	for (; i < mViews; i++) {
		mCompSplitter.SetColumnInfo(i, wInside, MIN_SIDE);
	}
	for (; i < CComparerDoc::IMG_VIEW_MAX; i++) {
		mCompSplitter.SetColumnInfo(i, 0, 0);
	}
	mCompSplitter.RecalcLayout();
}

void CMainFrame::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default
	CComparerDoc *pDoc = static_cast<CComparerDoc *>(GetActiveDocument());

	CPosInfoView *pPosInfoView;
	CFrmsInfoView *pFrmsInfoView;
	const FrmCmpInfo *frmCmpInfo;
	FileScanThread *fileScanThread;
	int lastIdx;

	switch (nIDEvent) {
	case CTI_ID_POS_INVALIDATE:
		pPosInfoView = pDoc->mPosInfoView;
		pFrmsInfoView = pDoc->mFrmsInfoView;

		pPosInfoView->Invalidate(FALSE);
		pFrmsInfoView->Invalidate(FALSE);

		fileScanThread = pDoc->mFileScanThread;
		frmCmpInfo = fileScanThread->getFrmCmpInfo();
		lastIdx = pDoc->mMinFrames - 1;
		if (lastIdx < 0 || frmCmpInfo[lastIdx].mParseDone)
			KillTimer(CTI_ID_POS_INVALIDATE);

		break;
	case CTI_ID_PLAY:
		bool isProcessing = pDoc->CheckImgViewProcessing();
		if (isProcessing)
			break;

		bool updated = pDoc->NextScenes();
		pDoc->MarkImgViewProcessing();

		// pDoc->UpdateAllViews(NULL) is slow than the below code 
		for (int i = 0; i < mViews; i++) {
			CComparerView *pView = pDoc->mPane[i].pView;
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

	// TODO: Add your message handler code here
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

	CComparerDoc* pDoc = static_cast<CComparerDoc*>(GetActiveDocument());

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

	CComparerDoc* pDoc = static_cast<CComparerDoc*>(GetActiveDocument());

	CString str;
	str.Format(_T("%.2ff&ps"), fps);
	GetMenu()->ModifyMenu(MENU_POS_FPS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mFpsMenu.m_hMenu, str);
}

void CMainFrame::UpdateViewsLabel(int views)
{
	if (GetMenu() == NULL)
		return;

	CComparerDoc* pDoc = static_cast<CComparerDoc*>(GetActiveDocument());

	CString str;
	str.Format(_T("%d VIEWS"), views);
	GetMenu()->ModifyMenu(MENU_POS_VIEWS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mViewsMenu.m_hMenu, str);
}

void CMainFrame::OnResolutionChange(UINT nID)
{
	CComparerDoc *pDoc = static_cast<CComparerDoc *>(GetActiveDocument());

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
	CComparerDoc* pDoc = static_cast<CComparerDoc*>(GetActiveDocument());

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
	CComparerDoc* pDoc = static_cast<CComparerDoc*>(GetActiveDocument());

	CString str;
	CMenu* subMenu = GetMenu()->GetSubMenu(MENU_POS_VIEWS);
	subMenu->GetMenuString(nID, str, MF_BYCOMMAND);

	int views = _wtoi(str);

	if (mViews == views)
		return;

	UpdateViewsLabel(views);
	CheckViewsRadio(views);

	int preViews = mViews;
	mViews = views;

	for (int i = views; i < CComparerDoc::IMG_VIEW_MAX; i++)
		pDoc->mPane[i].Release();

	if (IsZoomed())
		SendMessage(WM_SYSCOMMAND, SC_RESTORE, (LPARAM)GetSafeHwnd());

	DrawMenuBar();

	const CComparerView* firstView = pDoc->mPane[0].pView;
	firstView->AdjustWindowSize(preViews, mSplitBarW * (mViews - preViews));
}

void CMainFrame::OnOptionsChange(UINT nID)
{
	CComparerDoc* pDoc = static_cast<CComparerDoc*>(GetActiveDocument());

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

void CMainFrame::CheckViewsRadio(int views)
{
	if (GetMenu() == NULL)
		return;

	UINT id = ID_VIEWS_START + views - COMPARER_DEF_VIEWS;

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
	RefreshAllViews(); // RefreshFrmsInfoView was used before 2021.01.03
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWnd::OnCreate(lpCreateStruct) == -1)
		return -1;

	int i;
	CString str;

	// size menu
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

	// metric menu
	const qmetric_info* qmi;
	for (i = 0; i < METRIC_COUNT; i++) {
		qmi = qmetric_info_table + i;

		mMetricMenu.AppendMenu(MF_STRING, (UINT_PTR)ID_METRIC_START + i, CA2W(qmi->name));
	}

	// PSNR is default and is dealt with differently from other metrics
	str = qmetric_info_table[mMetricIdx].name;
	GetMenu()->InsertMenu(MENU_POS_METRIC, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mMetricMenu.m_hMenu, str);

	CheckMetricRadio();

	// fps menu
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

	// views menu
	for (i = 0; i < CComparerDoc::IMG_VIEW_MAX - 1; i++) {
		str.Format(_T("%d VIEWS"), i + COMPARER_DEF_VIEWS);
		mViewsMenu.AppendMenu(MF_STRING, (UINT_PTR)ID_VIEWS_START + i, str);
	}
	str.Format(_T("%d VIEWS"), mViews);
	GetMenu()->InsertMenu(MENU_POS_VIEWS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mViewsMenu.m_hMenu, str);

	CheckViewsRadio(mViews);

	// option menu
	str.Format(_T("Allow Different Resolution"));
	mOptionsMenu.AppendMenu(MF_STRING, (UINT_PTR)ID_OPTIONS_DIFF_RESOLUTION, str);

	str.Format(_T("OPTIONS"));
	GetMenu()->InsertMenu(MENU_POS_OPTIONS, MF_BYPOSITION | MF_POPUP,
		(UINT_PTR)mOptionsMenu.m_hMenu, str);

	return 0;
}

void CMainFrame::RefreshFrmsInfoView()
{
	CComparerDoc *pDoc = static_cast<CComparerDoc *>(GetActiveDocument());
	CFrmsInfoView *frmInfoView = pDoc->mFrmsInfoView;
	frmInfoView->Invalidate(FALSE);
}

void CMainFrame::RefreshAllViews()
{
	CComparerDoc *pDoc = static_cast<CComparerDoc *>(GetActiveDocument());

	bool refreshed = false;
	for (int i = 0; i < mViews; i++) {
		ComparerPane* pane = &pDoc->mPane[i];
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

		const CComparerView* firstView = pDoc->mPane[0].pView;
		firstView->AdjustWindowSize(mViews); // Adjust the whole window size using the first ComparerView

		pDoc->UpdateAllViews(NULL);
	}
}

void CMainFrame::ActivateFrame(int nCmdShow)
{
	// TODO: Add your specialized code here and/or call the base class

	CFrameWnd::ActivateFrame(nCmdShow);

	CComparerDoc *pDoc = static_cast<CComparerDoc *>(GetActiveDocument());
	if (pDoc->mPendingFile != "") {
		pDoc->OnOpenDocument(pDoc->mPendingFile);
		pDoc->mPendingFile = "";
	}
}
