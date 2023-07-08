// ComparerViewC.cpp : implementation file
//

#include "stdafx.h"
#include "Comparer.h"
#include "ComparerDoc.h"
#include "ComparerView.h"
#include "ComparerViews.h"
#include "MainFrm.h"
#include "PosInfoView.h"

#include "QCommon.h"
#include "QDebug.h"
#include "QMath.h"

#include <QViewerCmn.h>
#include <QImageStr.h>

// CComparerView

CComparerView::CComparerView()
: mIsClicked(false)
, mProcessing(false)
, mRgbBufSize(0)
, mRgbBuf(NULL)
{
	LOGFONT lf;

	::ZeroMemory(&lf, sizeof(lf));

	lf.lfWeight = FW_NORMAL;
	::lstrcpy(lf.lfFaceName, _T("Courier New"));
	mDefPixelTextFont.CreateFontIndirect(&lf);

	mCsMenu.CreatePopupMenu();

	CString str;
	for (int i = 0; i < ARRAY_SIZE(qcsc_info_table); i++) {
		str = qcsc_info_table[i].name;

		str.MakeUpper();
		mCsMenu.AppendMenu(MF_STRING, ID_CS_START + i, str);
	}

	CheckCsRadio(qcsc_info_table[QIMG_DEF_CS_IDX].cs);

	mRcControls.SetRectEmpty();
	mRcCsQMenu.SetRectEmpty();
	mRcNameQMenu.SetRectEmpty();
}

CComparerView::~CComparerView()
{
	mCsMenu.DestroyMenu();

	if (mRgbBuf)
		_mm_free(mRgbBuf);
}


BEGIN_MESSAGE_MAP(CComparerView, CScrollView)
	ON_COMMAND_RANGE(ID_CS_START, ID_CS_END, CComparerView::OnCsChange)
	ON_WM_CREATE()
	ON_WM_DROPFILES()
	ON_WM_ERASEBKGND()
	ON_WM_MOUSEWHEEL()
	ON_WM_SIZE()
	ON_WM_MOUSEMOVE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_SETCURSOR()
	ON_WM_KEYDOWN()
	ON_WM_RBUTTONDOWN()
END_MESSAGE_MAP()

enum {
	STATIC_CS_EVENT_ID = 0x1000,

	STATIC_BLANK_EVENT_ID,
};

// CComparerView drawing

void CComparerView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();

	CSize sizeTotal;
	// TODO: calculate the total size of this view
	sizeTotal.cx = sizeTotal.cy = MIN_SIDE;
	SetScrollSizes(MM_TEXT, sizeTotal);
}

void CComparerView::ScaleNearestNeighbor(CComparerDoc *pDoc, BYTE *src, BYTE *dst, int sDst,
										  q1::GridInfo &gi)
{
	long gap, yStart, yEnd, xStart, xEnd;

	if (mYDst > 0 || mXDst > 0) // client window is bigger -> center
		memset(dst, 0xff, sDst * mHClient * QIMG_DST_RGB_BYTES);

	// [yStart, yEnd] is the scaled y-range to consider on the screen
	if (mYDst > 0) {
		dst += sDst * mYDst * QIMG_DST_RGB_BYTES;
		yStart = 0;
		yEnd = pDoc->mHDst;
	} else {
		yStart = -mYDst;
		yEnd = mHClient - mYDst - mRcControls.bottom;
	}

	if (mXDst > 0) {
		gap = (sDst - pDoc->mWDst) * QIMG_DST_RGB_BYTES;
		dst += mXDst * QIMG_DST_RGB_BYTES;
		xStart = 0;
		xEnd = pDoc->mWDst;
	} else {
		gap = (sDst - mWClient) * QIMG_DST_RGB_BYTES;
		xStart = -mXDst;
		xEnd = mWClient - mXDst;
	}

	if (pDoc->mInterpol) {
		q1::Interpolate(src, pDoc->mH, pDoc->mW, mWCanvas, xStart, xEnd, yStart, yEnd, pDoc->mNnOffsetBuf, dst);
	} else {
		q1::NearestNeighbor(src, pDoc->mH, pDoc->mW, pDoc->mHDst, pDoc->mWDst, mXDst, mYDst, pDoc->mN,
			xStart, xEnd, yStart, yEnd, gap, gi,
			pDoc->mNnOffsetBuf, pDoc->mNnOffsetYBorderFlag, pDoc->mNnOffsetXBorderFlag, dst);
	}
}

void CComparerView::ScaleRgbBuf(CComparerDoc *pDoc, BYTE *rgbBuffer, q1::GridInfo &gi)
{
	int sDst = ROUNDUP_DWORD(mWClient);
	int rgbBufSize = sDst * mHClient * QIMG_DST_RGB_BYTES;

	if (mRgbBufSize < rgbBufSize) {
		if (mRgbBuf)
			_mm_free(mRgbBuf);

		mRgbBufSize = rgbBufSize;
		mRgbBuf = static_cast<BYTE *>(_mm_malloc(mRgbBufSize, 16));
	}

	int maxL = QMAX(pDoc->mWDst, pDoc->mHDst);
	if (pDoc->mPreN != pDoc->mN || pDoc->mPreMaxL < maxL) {
		if (pDoc->mNnOffsetBufSize < maxL) {
			if (pDoc->mNnOffsetBuf)
				_mm_free(pDoc->mNnOffsetBuf);

			// TODO: buf here
			if (pDoc->mNnOffsetYBorderFlag)
				_mm_free(pDoc->mNnOffsetYBorderFlag);

			if (pDoc->mNnOffsetXBorderFlag)
				_mm_free(pDoc->mNnOffsetXBorderFlag);

			pDoc->mNnOffsetBufSize = maxL;
			pDoc->mNnOffsetBuf =
				static_cast<qu16 *>(_mm_malloc(pDoc->mNnOffsetBufSize * sizeof(qu16), 16));
			pDoc->mNnOffsetYBorderFlag =
				static_cast<qu8 *>(_mm_malloc(pDoc->mNnOffsetBufSize * sizeof(qu8), 16));
			pDoc->mNnOffsetXBorderFlag =
				static_cast<qu8 *>(_mm_malloc(pDoc->mNnOffsetBufSize * sizeof(qu8), 16));
		}

		float ratio = 1 / pDoc->mN;
		for (int i = 0; i < maxL; i++)
			pDoc->mNnOffsetBuf[i] = int(i * ratio) * QIMG_DST_RGB_BYTES;

		pDoc->mPreN = pDoc->mN;
		pDoc->mPreMaxL = maxL;
	}

	ScaleNearestNeighbor(pDoc, rgbBuffer, mRgbBuf, sDst, gi);

	BITMAPINFOHEADER &bmiHeader = pDoc->mBmi.bmiHeader;
	bmiHeader.biWidth = sDst;
	bmiHeader.biHeight = -mHClient;
}

void CComparerView::OnDraw(CDC *pDC)
{
	CComparerDoc *pDoc = GetDocument();
	// TODO: add draw code here
	// Let's try to avoid using IsKindOf(RUNTIME_CLASS(CComparerView1)

	ComparerPane *pane = GetPane(pDoc);
	if (!pane->isAvail()) {
		pDC->BitBlt(0, mRcControls.bottom, mWCanvas, mHCanvas, NULL, 0, 0, WHITENESS);
		return;
	}

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);

	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(pDC, mWCanvas, mHCanvas);

	memDC.SelectObject(bitmap);
	memDC.SetStretchBltMode(COLORONCOLOR);
	if (pDoc->mWDst < mWCanvas || pDoc->mHDst < mHCanvas)
		memDC.BitBlt(0, 0, mWCanvas, mHCanvas, NULL, 0, 0, WHITENESS);

	q1::GridInfo gi;
	ScaleRgbBuf(pDoc, pane->rgbBuf, gi); // update mRgbBuf
#ifdef USE_STRETCH_DIB
	StretchDIBits(memDC.m_hDC,
		mXDst, mYDst, pDoc->mWDst, pDoc->mHDst,
		0, 0, pDoc->mW, pDoc->mH,
		pane->rgbBuf, &pDoc->mBmi, DIB_RGB_COLORS, SRCCOPY); // main image
#else
	SetDIBitsToDevice(memDC.m_hDC,
		0, 0, mWClient, mHClient,
		0, 0, 0, mHClient,
		mRgbBuf, &pDoc->mBmi, DIB_RGB_COLORS);
#endif
	if (pDoc->mN > ZOOM_TEXT_START) {
		CString label;
		CRect rect;
		LOGFONT lf;
		CFont pixelTextFont;
		mDefPixelTextFont.GetLogFont(&lf);
		lf.lfHeight = LONG(pDoc->mN * 4 / 15);
		pixelTextFont.CreateFontIndirect(&lf);
		memDC.SelectObject(&pixelTextFont);
		memDC.SetTextColor(COLOR_PIXEL_TEXT);
		memDC.DrawText(_T("000\n000\n000"), -1, rect, DT_CENTER | DT_CALCRECT);
		int y = gi.y;
		for (int i = 0; i < (int)gi.Hs.size(); i++) {
			int x = gi.x;
			for (int j = 0; j < (int)gi.Ws.size(); j++) {
				cv::Vec3b px = gi.pixelMap.at<cv::Vec3b>(i, j);
				label.Format(pDoc->mRgbFormat, px[2], px[1], px[0]);
				CRect rect(x, y + (gi.Hs[i] - rect.Height()) / 2,
					x + gi.Ws[j] - 1, y + gi.Hs[i] - 1);
				memDC.SetBkColor(RGB((0x80+px[2])/2, (0x80+px[1])/2, (0x80+px[0])/2));
				memDC.DrawText(label, &rect, DT_CENTER |  DT_VCENTER);
				x += gi.Ws[j];
			}
			y += gi.Hs[i];
		}
	}

	pDC->BitBlt(0, mRcControls.bottom, mWCanvas, mHCanvas, &memDC, 0, 0, SRCCOPY);

	mProcessing = false;
}


// CComparerView diagnostics

#ifdef _DEBUG
void CComparerView::AssertValid() const
{
	CScrollView::AssertValid();
}

#ifndef _WIN32_WCE
void CComparerView::Dump(CDumpContext& dc) const
{
	CScrollView::Dump(dc);
}
#endif

CComparerDoc* CComparerView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CComparerDoc)));
	return (CComparerDoc*)m_pDocument;
}
#endif //_DEBUG


// CComparerView message handlers

int CComparerView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CScrollView::OnCreate(lpCreateStruct) == -1)
		return -1;

	// TODO:  Add your specialized creation code here
	DragAcceptFiles(TRUE);

	CString str = CA2W(qcsc_info_table[QIMG_DEF_CS_IDX].name);
	str.MakeUpper();

	mCsQMenu.Create(str, mRcCsQMenu, this, &mCsMenu);
	mCsQMenu.CalcRect(&mRcCsQMenu);
	mRcCsQMenu.InflateRect(0, 0, 0, QMENUITEM_IN_MARGIN_H);
	mCsQMenu.MoveWindow(&mRcCsQMenu);

	mRcControls.bottom = mRcCsQMenu.bottom;

	mRcNameQMenu.left = mRcCsQMenu.right;
	mNameQMenu.Create(CString(""), mRcNameQMenu, this, NULL, DT_RIGHT);
	mNameQMenu.CalcRect(&mRcNameQMenu);
	mRcNameQMenu.InflateRect(0, 0, 0, QMENUITEM_IN_MARGIN_H);
	mNameQMenu.MoveWindow(&mRcNameQMenu);

	return 0;
}

void CComparerView::OnDropFiles(HDROP hDropInfo)
{
	// TODO: Add your message handler code here and/or call default
	TCHAR szPathName[MAX_PATH];

	UINT uDragCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
	if (uDragCount <= 0)
		goto OnDropFilesDefault;

	DragQueryFile(hDropInfo, 0, szPathName, MAX_PATH);

	CComparerDoc *pDoc = GetDocument();
	ComparerPane *pane = GetPane(pDoc);

	if (pane->pathName == szPathName)
		goto OnDropFilesDefault;

	pane->pathName = szPathName;
	pDoc->ProcessDocument(mPane);

	CPosInfoView* posInfoView = pDoc->mPosInfoView;
	posInfoView->ConfigureScrollSizes(pDoc);

	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	pMainFrm->UpdateResolutionLabel(pDoc->mW, pDoc->mH); // also, disable the resolution change if necessary
	pMainFrm->CheckResolutionRadio(pDoc->mW, pDoc->mH);

	AdjustWindowSize(pMainFrm->mViews);
	pDoc->UpdateAllViews(NULL);

OnDropFilesDefault:
	CScrollView::OnDropFiles(hDropInfo);
}

BOOL CComparerView::OnEraseBkgnd(CDC* pDC)
{
	// TODO: Add your message handler code here and/or call default
	return TRUE;
}

void CComparerView::AdjustWindowSize(int numPrevViews, int splitBarChange) const
{
	CComparerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	CSize fullScnSz;
	fullScnSz.cx = (LONG)::GetSystemMetrics(SM_CXFULLSCREEN);
	fullScnSz.cy = (LONG)::GetSystemMetrics(SM_CYFULLSCREEN);

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());

	DWORD dsStyle = pMainFrm->GetStyle();
	DWORD dsStyleEx = pMainFrm->GetExStyle();

	CRect mainRcClient, viewRcClient;

	pMainFrm->GetClientRect(&mainRcClient);
	GetClientRect(&viewRcClient);

	// calculate the default gap between current window and client
	int wGap = mainRcClient.Width() - viewRcClient.Width() * numPrevViews;
	int hGap = mainRcClient.Height() - viewRcClient.Height();

	// get the parent window rect
	int wViewClient = MAX(CANVAS_DEF_W, pDoc->mW);
	int hViewClient = MAX(CANVAS_DEF_H, pDoc->mH) + mRcControls.bottom;

	int wMainWindow = wViewClient * pMainFrm->mViews + wGap + splitBarChange;
	int hMainWindow = hViewClient + hGap;

	if (wMainWindow >= fullScnSz.cx || hMainWindow >= fullScnSz.cy) {
		if (!pMainFrm->IsZoomed()) {
			pMainFrm->SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE, 
				(LPARAM)pMainFrm->GetSafeHwnd());
		}

		return;
	} else if (pMainFrm->IsZoomed()) {
		// Whenever a file opens, restore from maximized state
		// to handle the issue (small img -> maximize -> open large img -> move)
		pMainFrm->SendMessage(WM_SYSCOMMAND, SC_RESTORE,
			(LPARAM)pMainFrm->GetSafeHwnd());
	}

	CRect rcWin(0, 0, wMainWindow, hMainWindow);
	::AdjustWindowRectEx(&rcWin, dsStyle, TRUE, dsStyleEx);

	pMainFrm->SetWindowPos(NULL, 0, 0,
		rcWin.Width(), rcWin.Height(), SWP_NOMOVE | SWP_FRAMECHANGED);
}

void CComparerView::DeterminDestOriginCoord(CComparerDoc *pDoc)
{
	mXDst = q1::DeterminDestPos(mWCanvas, pDoc->mWDst, pDoc->mXOff, pDoc->mN);
	mYDst = q1::DeterminDestPos(mHCanvas, pDoc->mHDst, pDoc->mYOff, pDoc->mN);
}

void CComparerView::Initialize(CComparerDoc *pDoc)
{
	// mWClient, mHClient, mWCanvas, mHCanvas were set in OnSize function

	DeterminDestOriginCoord(pDoc);
}

BOOL CComparerView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	// TODO: Add your message handler code here and/or call default
	CPoint clientPoint;
	CComparerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		goto OnMouseWheelDefault;

	clientPoint.x = pt.x;
	clientPoint.y = pt.y - mRcCsQMenu.bottom;

	ScreenToClient(&clientPoint);

	float xShift = clientPoint.x - (float)mWCanvas / 2 + 0.5f;
	float yShift = clientPoint.y - (float)mHCanvas / 2 + 0.5f;

	pDoc->mXOff -= xShift / pDoc->mN;
	pDoc->mYOff -= yShift / pDoc->mN;

	pDoc->ViewOnMouseWheel(zDelta, mWCanvas, mHCanvas);

	pDoc->mXOff += xShift / pDoc->mN;
	pDoc->mYOff += yShift / pDoc->mN;

	DeterminDestOriginCoord(pDoc);
	for (auto view : GetOhterViews(pDoc))
		view->DeterminDestOriginCoord(pDoc);

	pDoc->UpdateAllViews(NULL);

OnMouseWheelDefault:
	return CScrollView::OnMouseWheel(nFlags, zDelta, pt);
}

void CComparerView::OnSize(UINT nType, int cx, int cy)
{
	CScrollView::OnSize(nType, cx, cy);

	// TODO: Add your message handler code here
	CComparerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	mRcControls.right = cx;

	mWClient = cx;
	mHClient = cy;

	mWCanvas = mWClient;
	mHCanvas = mHClient - mRcControls.bottom;

	mRcNameQMenu.right = mWClient;
	if (mNameQMenu.GetSafeHwnd())
		mNameQMenu.MoveWindow(mRcNameQMenu);

	DeterminDestOriginCoord(pDoc);
}

void CComparerView::OnMouseMove(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	CComparerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		goto OnMouseMoveDefault;

	if (mIsClicked) {
		pDoc->mXOff = pDoc->mXInitOff + (point.x - mPointS.x) / pDoc->mN;
		pDoc->mYOff = pDoc->mYInitOff + (point.y - mPointS.y) / pDoc->mN;

		DeterminDestOriginCoord(pDoc);
		for (auto view : GetOhterViews(pDoc))
			view->DeterminDestOriginCoord(pDoc);

		Invalidate(FALSE);
		for (auto view : GetOhterViews(pDoc))
			view->Invalidate(FALSE);
	}

OnMouseMoveDefault:
	CScrollView::OnMouseMove(nFlags, point);
}

void CComparerView::OnLButtonDown(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	SetCapture();

	CComparerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc || mRcControls.PtInRect(point)) {
		CScrollView::OnLButtonDown(nFlags, point);
		return;
	}

	mIsClicked = true;
	mPointS = point;
	pDoc->mXInitOff = pDoc->mXOff;
	pDoc->mYInitOff = pDoc->mYOff;

	::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));

	CScrollView::OnLButtonDown(nFlags, point);
}

void CComparerView::OnLButtonUp(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default
	mIsClicked = false;

	::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));

	CScrollView::OnLButtonUp(nFlags, point);

	::ReleaseCapture();
}

BOOL CComparerView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
	// TODO: Add your message handler code here and/or call default
	if (mIsClicked) {
		::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));

		return TRUE;
	}

	return CScrollView::OnSetCursor(pWnd, nHitTest, message);
}

void CComparerView::CheckCsRadio(int cs)
{
	unsigned int i;
	for (i = 0; i < ARRAY_SIZE(qcsc_info_table); i++) {
		if (qcsc_info_table[i].cs == cs)
			break;
	}

	ASSERT(i < ARRAY_SIZE(qcsc_info_table));

	UINT id = ID_CS_START + i;

	mCsMenu.CheckMenuRadioItem(ID_CS_START, ID_CS_END,
		id, MF_CHECKED | MF_BYCOMMAND);
}

void CComparerView::UpdateCsLabel(const TCHAR *csLabel)
{
	mCsQMenu.SetWindowText(csLabel);
	mCsQMenu.CalcRect(&mRcCsQMenu);
	mRcCsQMenu.InflateRect(0, 0, 0, QMENUITEM_IN_MARGIN_H);
	mCsQMenu.MoveWindow(&mRcCsQMenu);

	mRcNameQMenu.left = mRcCsQMenu.right;
	mNameQMenu.MoveWindow(mRcNameQMenu);
}

void CComparerView::UpdateFileName(const TCHAR* filename)
{
	mNameQMenu.SetWindowText(filename);
	mCsQMenu.CalcRect(&mRcCsQMenu);

	mRcNameQMenu.left = mRcCsQMenu.right;
	mNameQMenu.MoveWindow(mRcNameQMenu);
}

void CComparerView::OnCsChange(UINT nID)
{
	// TODO: Add your command handler code here
	CComparerDoc* pDoc = GetDocument();

	CString str;
	mCsMenu.GetMenuString(nID, str, MF_BYCOMMAND);
	str.MakeLower();

	const struct qcsc_info * const ci =
		q1::image_find_cs(pDoc->mSortedCscInfo, CT2A(str));
	if (ci == NULL) {
		LOGERR("couldn't get the right index of color space");
		return;
	}

	ComparerPane *pane = GetPane(pDoc);
	if (ci->cs == pane->colorSpace)
		return;

	UpdateCsLabel(str.MakeUpper());
	CheckCsRadio(ci->cs);

	pane->SetColorInfo(ci);

	if (pane->isAvail()) {
		pDoc->RefleshPaneImages(pane, true);
		// This routine should be located after mMaxFrames set
		CPosInfoView* posInfoView = pDoc->mPosInfoView;
		posInfoView->ConfigureScrollSizes(pDoc);
		pDoc->UpdateAllViews(NULL);
	}
}

void CComparerView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// TODO: Add your message handler code here and/or call default
	CComparerDoc* pDoc = GetDocument();

	bool isProcessing = pDoc->CheckImgViewProcessing();
	if (nChar != VK_SPACE && isProcessing)
		return;

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());

	switch (nChar) {
	case VK_RIGHT:
		pDoc->OffsetScenes(1);
		break;
	case VK_LEFT:
		pDoc->OffsetScenes(-1);
		break;
	case VK_SPACE:
		if (!pDoc->mIsPlaying) {
			pDoc->mIsPlaying = true;
			pMainFrm->SetTimer(CTI_ID_PLAY, ROUND2I((1000 / pDoc->mFps) - FPS_ADJUSTMENT), NULL);
			bool changed = pDoc->NextScenes();
			if (!changed)
				pDoc->SetScenes(0);
		} else {
			pDoc->KillPlayTimer();
		}
		break;
	case 'H': // hex RGB value
		pDoc->mHexMode = !pDoc->mHexMode;
		pDoc->mRgbFormat = pDoc->mHexMode ? pDoc->mRgbHex : pDoc->mRgbDec;
		Invalidate(FALSE);
		break;
	case 'I':
		pDoc->mInterpol = !pDoc->mInterpol;
		Invalidate(FALSE);
		break;
	}

	// Once a key is pressed, update all views any way.
	// This scheme makes code prettier.
	pDoc->MarkImgViewProcessing();
	pDoc->UpdateAllViews(NULL);

	CScrollView::OnKeyDown(nChar, nRepCnt, nFlags);
}

std::vector<CComparerView *> CComparerView::GetOhterViews(CComparerDoc* pDoc)
{
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	std::vector<CComparerView *> otherViews;

	for (int i = 0; i < pMainFrm->mViews; i++) {
		CComparerView* pView = pDoc->mPane[i].pView;
		if (pView == this)
			continue;
		otherViews.push_back(pView);
	}

	return otherViews;
}

ComparerPane* CComparerView::GetPane(CComparerDoc* pDoc) const
{
	for (int i = 0; i < CComparerDoc::IMG_VIEW_MAX; i++) {
		CComparerView* pView = pDoc->mPane[i].pView;
		if (pView == this)
			return &pDoc->mPane[i];
	}

	return nullptr;
}

void CComparerView::OnRButtonDown(UINT nFlags, CPoint point)
{
	CScrollView::OnRButtonDown(nFlags, point);
}
