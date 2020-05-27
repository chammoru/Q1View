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

// CMainFrame

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWnd)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWnd)
//	ON_WM_DROPFILES()
	ON_COMMAND(ID_VIEWER_HELP, &CMainFrame::OnHelp)
	ON_COMMAND(ID_FILE_OPEN, &CMainFrame::OnFileOpen)
	ON_COMMAND(ID_COMPARE, &CMainFrame::OnExecComparer)
	ON_COMMAND_RANGE(ID_RESOLUTION_START, ID_RESOLUTION_END, &CMainFrame::OnResolutionChange)
	ON_COMMAND_RANGE(ID_CS_START, ID_CS_END, &CMainFrame::OnCsChange)
	ON_COMMAND_RANGE(ID_FPS_START, ID_FPS_END, &CMainFrame::OnFpsChange)
	ON_WM_CREATE()
	ON_MESSAGE(WM_RELOAD, &CMainFrame::Reload)
	ON_COMMAND(ID_EDIT_COPY, &CMainFrame::OnEditCopy)
	ON_COMMAND(ID_EDIT_PASTE, &CMainFrame::OnEditPaste)
END_MESSAGE_MAP()


// CMainFrame construction/destruction

CMainFrame::CMainFrame()
{
	// TODO: add member initialization code here
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
	pDoc->OnOpenDocument(pDoc->mPathName);
	Invalidate(FALSE);
	return S_OK;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if (!CFrameWnd::PreCreateWindow(cs))
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

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


//void CMainFrame::OnDropFiles(HDROP hDropInfo)
//{
//	// TODO: Add your message handler code here and/or call default
//	TCHAR path[MAX_PATH];
//
//	DragQueryFile(hDropInfo, 0, path, MAX_PATH);
//
//	LOGINF("%s", path);
//
//	CFrameWnd::OnDropFiles(hDropInfo);
//}

void CMainFrame::OnHelp()
{
	CViewerView *pView = static_cast<CViewerView *>(GetActiveView());
	pView->ToggleHelp();
}

void CMainFrame::OnFileOpen()
{
	// TODO: Add your command handler code here
	TCHAR name_filter[] =
		_T("All Files (*.*)|*.*|")
		_T("BMP Files (*.bmp)|*.bmp|")
		_T("YUV Files (*.yuv)|*.yuv|")
		_T("RGB Files (*.rgb)|*.rgb|");

	CFileDialog ins_dlg(TRUE, _T("All Files (*.*)"), _T("*.*"),
		OFN_HIDEREADONLY, name_filter, NULL);

	// pre-choose the second item in the combo box for file type
	ins_dlg.m_ofn.nFilterIndex = 2;

	if (ins_dlg.DoModal() == IDOK) {
		CString path = ins_dlg.GetPathName();

		// to show the file path in the upper bar
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

	UpdateCsLabel(str.MakeUpper());
	CheckCsRadio(ci->cs);

	pDoc->mColorSpace = ci->cs;
	pDoc->mCsc2Rgb888 = ci->csc2rgb888;
	pDoc->mCsLoadInfo = ci->cs_load_info;

	RefreshView();
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

void CMainFrame::OnExecComparer()
{
	TCHAR curDir[MAX_PATH];
	::GetCurrentDirectory(MAX_PATH, curDir);

	CString cmperPath;
	BOOL cmperExists;
	STARTUPINFO startUpInfo = {0, };
	PROCESS_INFORMATION processInfo;
	startUpInfo.cb = sizeof(STARTUPINFO);

	cmperPath = curDir;
	cmperPath += _T("\\Comparer.exe");

	cmperExists = ::PathFileExists(cmperPath);

#ifdef _DEBUG
	if (!cmperExists) {
		cmperPath = curDir;
		cmperPath += _T("\\..\\Comparer\\x64\\Debug\\Comparer.exe");

		cmperExists = ::PathFileExists(cmperPath);
	}
#endif

	if (!cmperExists) {
		MessageBox(_T("Coundn't find the 'Comparer.exe'"), _T("Warning"), MB_ICONWARNING);
		return;
	}

	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());

	TCHAR cmdLine[MAX_PATH] = {0, };
	if (pDoc->mPathName) {
		CString str;
		TCHAR buf[MAX_PATH];

		StrNCpy(buf, cmperPath, MAX_PATH);
		PathQuoteSpaces(buf);
		str = buf;

		str += _T(" ");

		StrCpyN(buf, pDoc->mPathName, MAX_PATH);
		PathQuoteSpaces(buf);
		str += buf;

		StrCpyN(cmdLine, str, MAX_PATH);
	}

	BOOL ret = ::CreateProcess(cmperPath,
		cmdLine, NULL, NULL, FALSE, 0, NULL, NULL,
		&startUpInfo, &processInfo);
}

void CMainFrame::ActivateFrame(int nCmdShow)
{
	// TODO: Add your specialized code here and/or call the base class

	CFrameWnd::ActivateFrame(nCmdShow);

	CViewerDoc *pDoc = static_cast<CViewerDoc *>(GetActiveDocument());
	if (pDoc->mPendingFile != "") {
		pDoc->OnOpenDocument(pDoc->mPendingFile);
		pDoc->mPendingFile = "";
	}
}

void CMainFrame::OnEditCopy()
{
	// TODO: Add your command handler code here
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
	// TODO: Add your command handler code here
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
