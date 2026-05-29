// MainFrm.cpp : implementation of the CMainFrame class
//

#include "stdafx.h"

#include <atlimage.h>

#include "Viewer.h"

#include "MainFrm.h"

#include "ViewerDoc.h"
#include "ViewerView.h"

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
END_MESSAGE_MAP()


CMainFrame::CMainFrame()
: mSyncInput(false)
, mSyncViewStatePending(false)
{
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

	return 0;
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
	pView->OpenClipboard();
	HBITMAP handle = (HBITMAP)::GetClipboardData(CF_BITMAP);
	if (handle == NULL) {
		::CloseClipboard();
		return;
	}

	CImage cimg;
	cimg.Attach(handle);
	const LPCTSTR tempClipboardFilename = _T("ViewerClipboard");
	cimg.Save(tempClipboardFilename, Gdiplus::ImageFormatBMP);
	::CloseClipboard();

	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());
	pDoc->OnOpenDocument(tempClipboardFilename);
	pDoc->SetPathName(tempClipboardFilename, TRUE);
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
