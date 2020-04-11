// ViewerView.cpp : implementation of the CViewerView class
//

#include "stdafx.h"
#include "Viewer.h"
#include "MainFrm.h"

#include "ViewerDoc.h"
#include "ViewerView.h"

#include "QMath.h"
#include "QDebug.h"

#include "QImageViewerCmn.h"
#include "qimage_cs.h"
#include "qimage_util.h"

#include "FrmSrc.h"

#include <opencv2/imgproc/imgproc.hpp>

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

const bool printPlaySpeed = false;

enum QMouseMenuID
{
	QMouseMenuStart = 0x10000000,
	QMouseMenuSel = QMouseMenuStart,
};

struct QMouseMenuInfo {
	const char *name;
	const QMouseMenuID id;
};

static const QMouseMenuInfo QMouseMenu[] = {
	{ "Sel Mode", QMouseMenuSel },
};

using namespace std;

// CViewerView

IMPLEMENT_DYNCREATE(CViewerView, CView)

BEGIN_MESSAGE_MAP(CViewerView, CView)
	ON_COMMAND_RANGE(ID_MOUSEMENU_START, ID_MOUSEMENU_END, CViewerView::OnMouseMenu)
	ON_WM_ERASEBKGND()
	ON_WM_MOUSEWHEEL()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MOUSEMOVE()
	ON_WM_KEYDOWN()
	ON_WM_DESTROY()
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_SETCURSOR()
	ON_WM_RBUTTONUP()
END_MESSAGE_MAP()

// CViewerView construction/destruction

CViewerView::CViewerView()
: mW(VIEWER_DEF_W)
, mH(VIEWER_DEF_H)
, mD(0.f)
, mN(ZOOM_RATIO(mD))
, mIsClicked(false)
, mXOff(.0f)
, mYOff(.0f)
, mWDst(VIEWER_DEF_W)
, mHDst(VIEWER_DEF_H)
, mHProgress(0)
, mBarColor(COLOR_PROGRESS_BAR)
, mRgbBuf(NULL)
, mCaptRgbBuf(NULL)
, mYBuf(NULL)
, mRgbBufSize(0)
, mCaptRgbBufSize(0)
, mYBufSize(0)
, mNnOffsetBuf(NULL)
, mNnOffsetYBorderFlag(NULL)
, mNnOffsetXBorderFlag(NULL)
, mNnOffsetBufSize(0)
, mPreN(0.0f)
, mPreMaxL(0)
, mIsPlaying(false)
, mTimerID(0)
, mNewRgbBufferInfoQ(new SSafeCQ<BufferInfo>(1))
, mBufferPool(NULL)
, mKeyProcessing(false)
, mXCursor(-1)
, mYCursor(-1)
, mSelMode(false)
, mYMode(false)
, mInterpol(false)
, mFullMode(false)
, mShowHelp(false)
, mShowCoord(false)
, mShowBoxInfo(true)
, mWasZoomed(false)
, mHexMode(false)
, mRgbHex(_T("%02X\n%02X\n%02X"))
, mRgbDec(_T("%03d\n%03d\n%03d"))
, mRgbFormat(mRgbDec)
, mNewSel(false)
, mDeltaIdx(-1)
{
	LOGFONT lf;

	::ZeroMemory(&lf, sizeof(lf));
	lf.lfHeight = PROGRESS_FONT_H;
	lf.lfWeight = FW_BOLD;

	::lstrcpy(lf.lfFaceName, _T("Arial"));
	mProgressFont.CreateFontIndirect(&lf);

	lf.lfWeight = FW_NORMAL;
	::lstrcpy(lf.lfFaceName, _T("Courier New"));
	mDefPixelTextFont.CreateFontIndirect(&lf);

	lf.lfWeight = FW_NORMAL;
	::lstrcpy(lf.lfFaceName, _T("Consolas"));
	mConsolasFont.CreateFontIndirect(&lf);

	// setup basic bitmap info
	BITMAPINFOHEADER &bmiHeader = mBmi.bmiHeader;
	bmiHeader.biSize = (DWORD)sizeof(BITMAPINFOHEADER);
	bmiHeader.biPlanes = 1;
	bmiHeader.biBitCount = 24;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = 0L;
	bmiHeader.biXPelsPerMeter = 0L;
	bmiHeader.biYPelsPerMeter = 0L;

	// http://www.codeproject.com/Articles/1236/Timers-Tutorial
	timeGetDevCaps(&mTc, sizeof (TIMECAPS));
	timeBeginPeriod(mTc.wPeriodMin);

	memset(&mStableRgbBufferInfo, 0, sizeof(BufferInfo));

	mRcProgress.SetRectEmpty();

	mMouseMenu.CreatePopupMenu();
	CString str;
	for (int i = 0; i < ARRAY_SIZE(QMouseMenu); i++) {
		str = QMouseMenu[i].name;
		mMouseMenu.AppendMenu(MF_STRING, ID_MOUSEMENU_START + i, str);
	}

	mDeltaRect.SetRectEmpty();
}

CViewerView::~CViewerView()
{
	mSelRegions.clear();
	delete mNewRgbBufferInfoQ;

	mMouseMenu.DestroyMenu();

	if (mRgbBuf)
		_mm_free(mRgbBuf);

	if (mCaptRgbBuf)
		_mm_free(mCaptRgbBuf);

	if (mYBuf)
		_mm_free(mYBuf);

	if (mNnOffsetBuf)
		_mm_free(mNnOffsetBuf);

	if (mNnOffsetYBorderFlag)
		_mm_free(mNnOffsetYBorderFlag);

	if (mNnOffsetXBorderFlag)
		_mm_free(mNnOffsetXBorderFlag);
}

BOOL CViewerView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

void CViewerView::AdjustWindowSize()
{
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// The View's parent window of is the CMainFrame in SDI
	CWnd *pMainFrm = GetParent();

	// Whenever a file opens, restore from maximized state
	// to reproduce the issue (small img -> maximize -> open large img -> move)
	if (!mFullMode && pMainFrm->IsZoomed()) {
		pMainFrm->SendMessage(WM_SYSCOMMAND, SC_RESTORE,
			(LPARAM)pMainFrm->GetSafeHwnd());
	}

	DWORD dsStyle = pMainFrm->GetStyle();
	DWORD dsStyleEx = pMainFrm->GetExStyle();

	CRect curRcWin, curRcClient;

	GetWindowRect(&curRcWin); // get the current window rect
	GetClientRect(&curRcClient);

	// calculate the default gap between current window and client
	int wGap = curRcWin.Width() - curRcClient.Width();
	int hGap = curRcWin.Height() - curRcClient.Height();

	// get the parent window rect
	int wClient = max(VIEWER_DEF_W, pDoc->mW);
	int hClient = max(VIEWER_DEF_H, pDoc->mH);

	CRect rcWin(0, 0, wClient + wGap, hClient + mHProgress + hGap);
	::AdjustWindowRectEx(&rcWin, dsStyle, TRUE, dsStyleEx);

	if (!mFullMode) {
		pMainFrm->SetWindowPos(NULL, 0, 0,
			rcWin.Width(), rcWin.Height(), SWP_NOMOVE | SWP_FRAMECHANGED);
	}
}

void CViewerView::Initialize(int nFrame, size_t rgbStride, int w, int h)
{
	mW = w;
	mH = h;
	mD = 0.f;
	mN = ZOOM_RATIO(mD);
	mXOff = 0.0f;
	mYOff = 0.0f;

	if (nFrame > 1)
		mHProgress = PROGRESS_BAR_H;
	else
		mHProgress = 0;

	SetDstSize();

	// mWClient, mHClient, mWCanvas, mHCanvas were set in OnSize function

	mXDst = QDeterminDestPos(mWCanvas, mWDst, mXOff, mN);
	mYDst = QDeterminDestPos(mHCanvas, mHDst, mYOff, mN);

	memset(&mStableRgbBufferInfo, 0, sizeof(BufferInfo));

	BITMAPINFO *pBmi = &mBmi;
	BITMAPINFOHEADER &bmiHeader = pBmi->bmiHeader;
	bmiHeader.biWidth = LONG(rgbStride);
	bmiHeader.biHeight = -h;

	mPointS.Init();
	mPointE.Init();

	mNewRgbBufferInfoQ->init();
}

void CViewerView::ProgressiveDraw(CDC *pDC, CViewerDoc* pDoc, int frameID)
{
	const int barMargin = MARGIN_PROGESS_BAR;

	int frameMax = pDoc->mFrames - 1;
	int limit =
		ROUND2I((mWClient - (barMargin << 1)) * frameID / (float)frameMax);

	CString str;
	str.Format(_T("%d / %d"), frameID, frameMax);

	BitBlt(pDC->m_hDC, 0, mHCanvas, mWClient, mHProgress, 0, 0, 0, WHITENESS);

	CRect barTextRect, barRect;

	barRect.top = mHCanvas + barMargin;
	barRect.left = barMargin;
	barRect.bottom = mHClient - barMargin;
	barRect.right = barRect.left + limit;

	CBrush brush_back_ground(mBarColor);
	pDC->FillRect(&barRect, &brush_back_ground);

	barTextRect.top = mHCanvas;
	barTextRect.left = 0;
	barTextRect.bottom = mHClient;
	barTextRect.right = mWClient - barMargin;

	pDC->SelectObject(&mProgressFont);
	pDC->SetTextColor(COLOR_PROGRESS_TEXT);
	pDC->SetBkMode(TRANSPARENT);
	pDC->DrawText(str, &barTextRect, DT_SINGLELINE | DT_RIGHT |  DT_VCENTER);
}

void CViewerView::Interpolate(BYTE *src, long xStart, long xEnd, long yStart, long yEnd, BYTE *dst)
{
	int xLen = xEnd - xStart;
	int yLen = yEnd - yStart;
	cv::Mat patch;
	cv::Mat srcMat(mH, mW, CV_8UC3, src, ROUNDUP_DWORD(mW) * QIMG_DST_RGB_BYTES);
	cv::Mat dstMat(yLen, xLen, CV_8UC3, dst, ROUNDUP_DWORD(mWCanvas) * QIMG_DST_RGB_BYTES);
	cv::Size patchSize(
		(mNnOffsetBuf[xEnd - 1] - mNnOffsetBuf[xStart]) / QIMG_DST_RGB_BYTES + 1,
		(mNnOffsetBuf[yEnd - 1] - mNnOffsetBuf[yStart]) / QIMG_DST_RGB_BYTES + 1);
	float xSum = 0.f, ySum = 0.f;
	for (int x = xStart; x < xEnd; x++)
		xSum += mNnOffsetBuf[x];
	xSum /= xLen * QIMG_DST_RGB_BYTES;
	for (int y = yStart; y < yEnd; y++)
		ySum += mNnOffsetBuf[y];
	ySum /= yLen * QIMG_DST_RGB_BYTES;
	cv::Point2f center(xSum, ySum);
	cv::getRectSubPix(srcMat, patchSize, center, patch);
	cv::resize(patch, dstMat, dstMat.size(), 0, 0, cv::INTER_LANCZOS4);
}

void CViewerView::NearestNeighbor(BYTE *src, long xStart, long xEnd, long yStart, long yEnd,
						  long gap, QGridInfo &gi, BYTE *dst)
{
	long xBase = mXDst > 0 ? mXDst : 0;
	long yBase = mYDst > 0 ? mYDst : 0;

	// Nearest Neighborhood
	if (mN >= ZOOM_GRID_START) {
		// investigate y-axis pixel border
		investigatePixelBorder(mNnOffsetBuf, yStart, yEnd, yBase, mHDst,
			&gi.y, &gi.Hs, mNnOffsetYBorderFlag);

		// investigate x-axis border
		investigatePixelBorder(mNnOffsetBuf, xStart, xEnd, xBase, mWDst,
			&gi.x, &gi.Ws, mNnOffsetXBorderFlag);

		gi.pixelMap.create(int(gi.Hs.size()), int(gi.Ws.size()), CV_8UC3);
		for (int i = 0, y = yStart; i < gi.pixelMap.rows; y += gi.Hs[i], i++) {
			BYTE *src_y = src + mNnOffsetBuf[y] * ROUNDUP_DWORD(mW);
			for (int j = 0, x = xStart; j < gi.pixelMap.cols; x += gi.Ws[j], j++) {
				BYTE *src_x = src_y + mNnOffsetBuf[x];
				gi.pixelMap.at<cv::Vec3b>(i, j) = cv::Vec3b(src_x);
			}
		}
		scaleUsingOffset(src, yStart, yEnd, xStart, xEnd, ROUNDUP_DWORD(mW), gap,
			mNnOffsetYBorderFlag, mNnOffsetXBorderFlag, mNnOffsetBuf, dst);
	} else {
		scaleUsingOffset(src, yStart, yEnd, xStart, xEnd, ROUNDUP_DWORD(mW), gap,
			mNnOffsetBuf, dst);
	}
}

void CViewerView::_ScaleRgb(BYTE *src, BYTE *dst, int sDst, QGridInfo &gi)
{
	long gap, yStart, yEnd, xStart, xEnd;

	if (mYDst > 0 || mXDst > 0) // client window is bigger -> center
		memset(dst, 0xff, sDst * mHClient * QIMG_DST_RGB_BYTES);

	// [yStart, yEnd] is the scaled y-range to consider on the screen
	if (mYDst > 0) {
		dst += sDst * mYDst * QIMG_DST_RGB_BYTES;
		yStart = 0;
		yEnd = mHDst;
	} else {
		yStart = -mYDst;
		yEnd = QMIN(mHDst, mHCanvas - mYDst);
	}

	if (mXDst >= 0 && sDst >= mWDst) { // for the case of "mW + 1 == mWClient"
		gap = (sDst - mWDst) * QIMG_DST_RGB_BYTES;
		dst += mXDst * QIMG_DST_RGB_BYTES;
		xStart = 0;
		xEnd = mWDst;
	} else {
		gap = (sDst - mWClient) * QIMG_DST_RGB_BYTES;
		xStart = -mXDst;
		xEnd = QMIN(mWDst, mWCanvas - mXDst);
	}

	if (mInterpol) {
		Interpolate(src, xStart, xEnd, yStart, yEnd, dst);
	} else {
		NearestNeighbor(src, xStart, xEnd, yStart, yEnd, gap, gi, dst);
	}
}

// this function works in a different thread, be careful of mutual exclusion
static void CALLBACK PlayTimerThread(UINT uID, UINT uMsg, DWORD_PTR dwUser,
		DWORD_PTR dw1, DWORD_PTR dw2)
{
	CViewerView *pView = reinterpret_cast<CViewerView *>(dwUser);

	if (!pView->mIsPlaying)
		return;

	BufferInfo bi = {0, };
	SSafeCQ<BufferInfo> *pBufferQueue = pView->mBufferQueue;

	bool ok = pBufferQueue->try_pop(bi);
	if (ok) {
		SSafeCQ<BufferInfo> *newBuffer1Q = pView->mNewRgbBufferInfoQ;
		newBuffer1Q->push(bi);
		pView->Invalidate(FALSE);
	}
}

void CViewerView::SetPlayTimer(CViewerDoc* pDoc)
{
	pDoc->mPlayFrameID = pDoc->mCurFrameID;
	mBufferQueue->set_next_id(pDoc->mCurFrameID + 1);

	FrmSrc *frmSrc = pDoc->mFrmSrc;
	bool ok = frmSrc->Play(pDoc);
	if (!ok)
		return;

	mIsPlaying = true;
	mPreKeyFrameStamp = 0;
	mPlayFrameCount = 0;
	mTimerID = timeSetEvent(1000 / pDoc->mFps, mTc.wPeriodMin, PlayTimerThread,
		(DWORD_PTR)this, TIME_PERIODIC);
}

// can be also called by the timer thread
void CViewerView::KillPlayTimer()
{
	if (!mIsPlaying)
		return;

	mNewRgbBufferInfoQ->destroy();

	mIsPlaying = false;
	timeKillEvent(mTimerID);
	timeEndPeriod(mTc.wPeriodMin);

	CViewerDoc* pDoc = GetDocument();
	Sleep(1000 / pDoc->mFps); // make sure all timer event passed

	std::vector<FrmSrc *> &frmSrcs = pDoc->mFrmSrcs;
	for (auto it = std::begin(frmSrcs); it != std::end(frmSrcs); it++)
		(*it)->Stop();
}

void CViewerView::PrintPlaySpeed(int fps)
{
	mPlayFrameCount++;

	if (mPlayFrameCount % fps == 1) {
		ULONGLONG cur = ::GetTickCount64();
		if (mPreKeyFrameStamp)
			LOGINF("%d fps takes %llu msec", fps, cur - mPreKeyFrameStamp);

		mPreKeyFrameStamp = cur;
	}
}

void CViewerView::ScaleRgbBuf(BYTE *src, BYTE **pDst, QGridInfo &gi)
{
	int sDst = ROUNDUP_DWORD(mWClient);
	int rgbBufSize = sDst * mHClient * QIMG_DST_RGB_BYTES;

	if (mRgbBufSize < rgbBufSize) {
		if (*pDst)
			_mm_free(*pDst);

		mRgbBufSize = rgbBufSize;
		*pDst = static_cast<BYTE *>(_mm_malloc(mRgbBufSize, 16));
	}

	int maxL = QMAX(mWDst, mHDst);
	if (mPreN != mN || mPreMaxL < maxL) {
		if (mNnOffsetBufSize < maxL) {
			if (mNnOffsetBuf)
				_mm_free(mNnOffsetBuf);

			if (mNnOffsetYBorderFlag)
				_mm_free(mNnOffsetYBorderFlag);

			if (mNnOffsetXBorderFlag)
				_mm_free(mNnOffsetXBorderFlag);

			mNnOffsetBufSize = maxL;
			mNnOffsetBuf = static_cast<qu16 *>(_mm_malloc(mNnOffsetBufSize * sizeof(qu16), 16));
			mNnOffsetYBorderFlag = static_cast<qu8 *>(_mm_malloc(mNnOffsetBufSize * sizeof(qu8), 16));
			mNnOffsetXBorderFlag = static_cast<qu8 *>(_mm_malloc(mNnOffsetBufSize * sizeof(qu8), 16));
		}

		float ratio = 1 / mN;
		for (int i = 0; i < maxL; i++)
			mNnOffsetBuf[i] = int(i * ratio) * QIMG_DST_RGB_BYTES;

		mPreN = mN;
		mPreMaxL = maxL;
	}

	_ScaleRgb(src, *pDst, sDst, gi);

	BITMAPINFOHEADER &bmiHeader = mBmi.bmiHeader;
	bmiHeader.biWidth = sDst;
	bmiHeader.biHeight = -mHClient;
}

CRect CViewerView::CvtCoord2Show(const CRect &rt)
{
	CRect ret;

	// top-left vertice of the box
	ret.left = CEIL2I(rt.left * mN) + mXDst;
	ret.top = CEIL2I(rt.top *  mN) + mYDst;

	// Note also that CDC::Rectangle() requires 1px bigger input.
	// bottom-right vertice of the box:
	ret.right = CEIL2I((rt.right + 1) *  mN) + mXDst;
	ret.bottom = CEIL2I((rt.bottom + 1) * mN) + mYDst;

	return ret;
}

void CViewerView::DrawSelectRect(CDC *pDC)
{
	CRect transRect;

	pDC->SelectStockObject(NULL_BRUSH);

	if (mPointE.IsValid()) {
		CPen rPen(PS_SOLID, 1, COLOR_SELECTING_RECT);

		transRect.SetRect(int((-mXDst + mPointS.x) / mN),
						  int((-mYDst + mPointS.y) / mN),
						  int((-mXDst + mPointE.x) / mN),
						  int((-mYDst + mPointE.y) / mN));
		transRect = CvtCoord2Show(transRect);

		pDC->SelectObject(&rPen);
		pDC->Rectangle(transRect);
	}
	
	CPen pen(PS_SOLID, 1, COLOR_SELECTED_RECT);
	for (int i = 0; i < (int)mSelRegions.size(); i++) {
		CRect &selRect = mSelRegions[i].mSelRect;

		transRect.left = selRect.left;
		transRect.top = selRect.top;
		transRect.right = selRect.right;
		transRect.bottom = selRect.bottom;

		if (i == mDeltaIdx) {
			transRect.left += mDeltaRect.left;
			transRect.top += mDeltaRect.top;
			transRect.right += mDeltaRect.right;
			transRect.bottom += mDeltaRect.bottom;
		}
		transRect = CvtCoord2Show(transRect);

		pDC->SelectObject(&pen);
		pDC->Rectangle(transRect);
	}
}

static void Convert2YYY(BYTE *srcBgr, BYTE *dstYyy, int stride, int w, int h)
{
	int i, j;
	int gap = (stride - w) * 3;

	for (i = 0; i < h; i++)
	{
		for (j = 0; j < w; j++)
		{
			const double B = (double)srcBgr[0];
			const double G = (double)srcBgr[1];
			const double R = (double)srcBgr[2];

			BYTE y =  ROUND2I(0.299 * R + 0.587 * G + 0.114 * B);
			dstYyy[0] = dstYyy[1] = dstYyy[2] = y;

			srcBgr += 3;
			dstYyy += 3;
		}

		srcBgr += gap;
		dstYyy += gap;
	}
}

void CViewerView::DrawRgbText(CDC *pDC, QGridInfo &gi)
{
	CString label;
	CRect refRect;
	LOGFONT lf;
	CFont pixelTextFont;
	mDefPixelTextFont.GetLogFont(&lf);
	lf.lfHeight = LONG(mN * 4 / 15);
	pixelTextFont.CreateFontIndirect(&lf);
	pDC->SelectObject(&pixelTextFont);
	pDC->SetTextColor(COLOR_PIXEL_TEXT);
	pDC->DrawText(_T("000\n000\n000"), -1, refRect, DT_CENTER | DT_VCENTER | DT_CALCRECT);
	int y = gi.y;
	for (int i = 0; i < (int)gi.Hs.size(); i++) {
		int x = gi.x;
		for (int j = 0; j < (int)gi.Ws.size(); j++) {
			cv::Vec3b px = gi.pixelMap.at<cv::Vec3b>(i, j);
			label.Format(mRgbFormat, px[2], px[1], px[0]);
			CRect rect(x, y + (gi.Hs[i] - refRect.Height()) / 2,
				x + gi.Ws[j] - 1, y + gi.Hs[i] - 1);
			pDC->SetBkColor(RGB((0x80+px[2])/2, (0x80+px[1])/2, (0x80+px[0])/2));
			pDC->DrawText(label, &rect, DT_CENTER | DT_VCENTER);
			x += gi.Ws[j];
		}
		y += gi.Hs[i];
	}
}

void CViewerView::DrawHelpMenu(CDC *pDC)
{
	const int W_HELP = VIEWER_DEF_W;
	const int H_HELP = VIEWER_DEF_H;
	const int W_MARGIN = 10;
	const int H_MARGIN =  4;
	const int X_HELP = (mWCanvas - W_HELP) / 2;
	const int Y_HELP = (mHCanvas - H_HELP) / 2;
	CRect bgRect(X_HELP, Y_HELP, X_HELP + W_HELP, Y_HELP + H_HELP);
	pDC->FillSolidRect(bgRect, RGB(0xb6, 0xbc, 0xcc));

	CRect manualRect(bgRect.left + W_MARGIN, bgRect.top + H_MARGIN,
		bgRect.right - W_MARGIN, bgRect.bottom - H_MARGIN);
	LOGFONT lf;
	CFont manualFont;
	mConsolasFont.GetLogFont(&lf);
	lf.lfHeight = 16;
	manualFont.CreateFontIndirect(&lf);
	pDC->SelectObject(&manualFont);
	pDC->SetTextColor(RGB(0x00, 0x00, 0x00));
	CString manual(
		"Viewer, Version 1.0, Copyright (C) 2018 by Kyuwon Kim\n"
		"Show or save compressed and raw images\n"
		"\n"
		"?            : This help menu\n"
		"Drag && Drop  : Show an image\n"
		"Mouse Wheel  : Zoom in/out (x40 shows RGB values)\n"
		"Return       : Full screen\n"
		"H            : RGB value in hexadecimal\n"
		"Y            : Only Y in YCbCr\n"
		"R            : 90-degree clockwise rotation\n"
		"Page Up      : Previous file\n"
		"Page Down    : Next file\n"
		"S            : Selective capture mode\n"
		"Ctrl + C     : Capture viewer screen\n"
		"Ctrl + V     : Paste to viewer screen\n"
		"Left  (<-)   : Previous frame in video\n"
		"Right (->)   : Next frame in video\n"
		"Home         : First frame (image) in video (directory)\n"
		"End          : Last frame (image) in video (directory)\n"
		"Space        : Play/stop video\n"
		"Click bottom : Select a video frame\n"
		"C            : Show cursor coordinates\n"
		"B            : Show size of selected boxes\n"
		"I            : Interpolate pixel values\n"
		);
	pDC->DrawText(manual, &manualRect, DT_LEFT | DT_TOP);
}

void CViewerView::DrawCursorCoordinates(CDC *pDC)
{
	LOGFONT lf;
	CFont cursorCoordFont;
	mConsolasFont.GetLogFont(&lf);
	lf.lfHeight = 16;
	cursorCoordFont.CreateFontIndirect(&lf);
	pDC->SelectObject(&cursorCoordFont);

	CString coord;
	coord.Format(_T("x:%d,y:%d"), mXCursor, mYCursor);
	CRect refRect;
	pDC->DrawText(coord, -1, refRect, DT_CENTER | DT_VCENTER | DT_CALCRECT);
	CRect bgRect(mWCanvas - refRect.Width(), mHCanvas - refRect.Height(), mWCanvas, mHCanvas);
	bgRect.InflateRect(8, 2, 0, 0);
	pDC->FillSolidRect(bgRect, COLOR_COORDINATE_RECT);
	pDC->SetTextColor(RGB(0xea, 0xef, 0xee));
	pDC->DrawText(coord, &bgRect, DT_CENTER | DT_VCENTER);
}

int CViewerView::DrawBoxInfoText(CDC *pDC, CRect &rect, COLORREF color, int hAccumGap)
{
	LOGFONT lf;
	CFont cursorCoordFont;
	mConsolasFont.GetLogFont(&lf);
	lf.lfHeight = 16;
	cursorCoordFont.CreateFontIndirect(&lf);
	pDC->SelectObject(&cursorCoordFont);

	CString coord;
	coord.Format(_T("w:%d,h:%d"), rect.Width() + 1, rect.Height() + 1);
	CRect refRect;
	pDC->DrawText(coord, -1, refRect, DT_CENTER | DT_VCENTER | DT_CALCRECT);
	hAccumGap += refRect.Height();
	CRect bgRect(mWCanvas - refRect.Width(), hAccumGap - refRect.Height(), mWCanvas, hAccumGap);
	bgRect.InflateRect(8, 1, 0, 0);
	pDC->FillSolidRect(bgRect, COLOR_BOXINFO_RECT);
	pDC->SetTextColor(color);
	pDC->DrawText(coord, &bgRect, DT_CENTER | DT_VCENTER);
	return hAccumGap;
}

void CViewerView::DrawBoxInfo(CDC *pDC)
{
	LOGFONT lf;
	CFont cursorCoordFont;
	mConsolasFont.GetLogFont(&lf);
	lf.lfHeight = 16;
	cursorCoordFont.CreateFontIndirect(&lf);
	pDC->SelectObject(&cursorCoordFont);

	int x0, y0, xn, yn;
	int hAccumGap = 0;

	if (mPointE.IsValid()) {
		CRect transRect;
		transRect.SetRect(int((-mXDst + mPointS.x) / mN),
						  int((-mYDst + mPointS.y) / mN),
						  int((-mXDst + mPointE.x) / mN),
						  int((-mYDst + mPointE.y) / mN));
		x0 = min(transRect.left, transRect.right);
		xn = max(transRect.left, transRect.right);
		y0 = min(transRect.top, transRect.bottom);
		yn = max(transRect.top, transRect.bottom);
		transRect.SetRect(x0, y0, xn, yn);
		hAccumGap = DrawBoxInfoText(pDC, transRect, COLOR_SELECTING_RECT, hAccumGap);
	}

	for (int i = 0; i < mSelRegions.size(); i++) {
		CRect selRect = mSelRegions[i].mSelRect;
		if (i == mDeltaIdx) {
			x0 = QMAX(selRect.left + mDeltaRect.left, 0);
			xn = QMIN(selRect.right + mDeltaRect.right, mW - 1);
			y0 = QMAX(selRect.top + mDeltaRect.top, 0);
			yn = QMIN(selRect.bottom + mDeltaRect.bottom, mH - 1);

			selRect.SetRect(x0, y0, xn, yn);
		}

		hAccumGap = DrawBoxInfoText(pDC, selRect, COLOR_SELECTED_RECT, hAccumGap);
	}
}

void CViewerView::OnDraw(CDC *pDC)
{
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	CDC memDC;
	memDC.CreateCompatibleDC(pDC);

	CBitmap bitmap;
	bitmap.CreateCompatibleBitmap(pDC, mWClient, mHClient);

	memDC.SelectObject(bitmap);
	memDC.SetStretchBltMode(COLORONCOLOR);
	memDC.BitBlt(0, 0, mWClient, mHClient, NULL, 0, 0, WHITENESS);

	BufferInfo bi;
	bool ok = mNewRgbBufferInfoQ->try_pop(bi);
	if (ok && (bi.addr != mStableRgbBufferInfo.addr || bi.ID != mStableRgbBufferInfo.ID)) {
		if (bi.ID == MSG_QUIT) {
			KillPlayTimerSafe();
		} else {
			mBufferPool->turn_back(mStableRgbBufferInfo.addr);
			mStableRgbBufferInfo = bi;

			// For framerate
			if (mIsPlaying & printPlaySpeed)
				PrintPlaySpeed(pDoc->mFps);
		}
	}

	BYTE *src;
	if (!mYMode) {
		src = mStableRgbBufferInfo.addr;
	} else {
		int yBufSize = ROUNDUP_DWORD(mW) * mH * QIMG_DST_RGB_BYTES;
		if (mYBufSize < yBufSize) {
			if (mYBuf)
				_mm_free(mYBuf);

			mYBufSize = yBufSize;
			mYBuf = static_cast<BYTE *>(_mm_malloc(mYBufSize, 16));
		}
		Convert2YYY(mStableRgbBufferInfo.addr, mYBuf, ROUNDUP_DWORD(mW), mW, mH);
		src = mYBuf;
	}

	QGridInfo gi;
	if (src)
		ScaleRgbBuf(src, &mRgbBuf, gi);
#ifdef USE_STRETCH_DIB
	StretchDIBits(memDC.m_hDC,
		0, 0, mWClient, mHClient,
		0, 0, mWClient, mHClient,
		mRgbBuf, &mBmi, DIB_RGB_COLORS, SRCCOPY);
#else
	SetDIBitsToDevice(memDC.m_hDC,
		0, 0, mWClient, mHClient,
		0, 0, 0, mHClient,
		mRgbBuf, &mBmi, DIB_RGB_COLORS);
#endif
	if (mN > ZOOM_TEXT_START)
		DrawRgbText(&memDC, gi);

	DrawSelectRect(&memDC);

	if (mShowCoord)
		DrawCursorCoordinates(&memDC);
	if (mShowBoxInfo)
		DrawBoxInfo(&memDC);

	// progressive
	if (pDoc->mFrames > 1)
		ProgressiveDraw(&memDC, pDoc, mStableRgbBufferInfo.ID);
	if (mShowHelp)
		DrawHelpMenu(&memDC);

	if (pDoc->mDocState == DOC_NEWIMAGE)
		pDoc->mDocState = DOC_ADJUSTED;

	pDC->BitBlt(0, 0, mWClient, mHClient, &memDC, 0, 0, SRCCOPY);

	pDoc->mCurFrameID = mStableRgbBufferInfo.ID;

	if (mKeyProcessing == true)
		mKeyProcessing = false;
}


// CViewerView diagnostics

#ifdef _DEBUG
void CViewerView::AssertValid() const
{
	CView::AssertValid();
}

void CViewerView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CViewerDoc* CViewerView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CViewerDoc)));
	return (CViewerDoc*)m_pDocument;
}
#endif //_DEBUG

BOOL CViewerView::OnEraseBkgnd(CDC* pDC)
{
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return TRUE;

	BOOL ret;
	if (pDoc->mDocState != DOC_JUSTLOAD) {
		// prevent flickering
		ret = TRUE;
	} else {
		ret = CView::OnEraseBkgnd(pDC);
	}

	return ret;
}

void CViewerView::SetCursorCoordinates(const CPoint &pt)
{
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	if (pDoc->mDocState == DOC_JUSTLOAD)
		return;

	int xCoord = int((-mXDst + pt.x) / mN);
	int yCoord = int((-mYDst + pt.y) / mN);

	if (mXCursor == xCoord && mYCursor == yCoord)
		return;

	mXCursor = xCoord;
	mYCursor = yCoord;

	if (mShowCoord)
		Invalidate(FALSE);
}

void CViewerView::SetDstSize()
{
#if 1
	if (mN < 1.f) {
		mWDst = ROUND2I(mW * mN);
		mHDst = ROUND2I(mH * mN);
	} else {
		// This ceiling shows the hidden margin on right and bottom sides
		mWDst = CEIL2I(mW * mN);
		mHDst = CEIL2I(mH * mN);
	}
#else // the bottom three lines (including comment) makes an error for 1080x1920 portrait size
	// This ceiling shows the hidden margin on right and bottom sides no matter value mN has
	mWDst = CEIL2I(mW * mN);
	mHDst = CEIL2I(mH * mN);
#endif
}

void CViewerView::ChangeZoom(short zDelta, CPoint &pt)
{
	if (mN > ZOOM_MAX && zDelta > 0)
		return;

	CPoint clientPoint = pt;

	ScreenToClient(&clientPoint);

	float xShift = clientPoint.x - (float)mWCanvas / 2 + 0.5f;
	float yShift = clientPoint.y - (float)mHCanvas / 2 + 0.5f;

	mXOff -= xShift / mN;
	mYOff -= yShift / mN;

	// mN would be changed
	if (mN <= 1 && zDelta < 0) {
		// reduce the image size to less than its canvas size,
		// only if client area is small
		mN = QGetFitRatio(mN, mW, mH, mWCanvas, mHCanvas);
	} else {
		float d = mD + zDelta / WHEEL_DELTA;
		float fitN = QGetBestFitRatio(mW, mH, mWCanvas, mHCanvas);
		mN = QGetNextN(mN, fitN, d);
	}
	mD = ZOOM_DELTA(mN);

	SetDstSize();

	mXOff += xShift / mN;
	mYOff += yShift / mN;

	mXDst = QDeterminDestPos(mWCanvas, mWDst, mXOff, mN);
	mYDst = QDeterminDestPos(mHCanvas, mHDst, mYOff, mN);

	SetCursorCoordinates(clientPoint);

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	pMainFrm->UpdateMagnication(mN, mWDst, mHDst);
}

BOOL CViewerView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc || pDoc->mDocState == DOC_JUSTLOAD)
		goto OnMouseWheelDefault;

	ChangeZoom(zDelta, pt);
	Invalidate(FALSE);

OnMouseWheelDefault:
	return CView::OnMouseWheel(nFlags, zDelta, pt);
}

void CViewerView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (mRcProgress.PtInRect(point)) {
		CViewerDoc* pDoc = GetDocument();
		bool wasPlaying = false;

		if (mIsPlaying) {
			wasPlaying = true;
			KillPlayTimerSafe();
		}

		double R = (point.x + 1) / double(mWCanvas);
		int frameID = max(ROUND2I(R * pDoc->mFrames) - 1, 0);
		pDoc->SeekScene(frameID);
		Invalidate(FALSE);

		if (wasPlaying)
			SetPlayTimer(pDoc);
	} else {
		SetCapture();

		mIsClicked = true;
		mPointS.SetPoint(point.x, point.y);

		if (mSelMode && !(nFlags & MK_CONTROL)) {
			mPointE.Init();
			int i = (int)mSelRegions.size() - 1;
			for (; i >= 0; i--) {
				QSelRegion &selRegion = mSelRegions[i];
				CPoint transPoint(int((-mXDst + point.x) / mN), int((-mYDst + point.y) / mN));

				if (selRegion.IsInLR(transPoint))
					::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
				else if (selRegion.IsInTB(transPoint))
					::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS));
				else
					continue;

				break;
			}

			if (i < 0 /* && !(nFlags & MK_CONTROL) del. the comparison to use only 1 box */)
				mSelRegions.clear();

			Invalidate(FALSE);
		} else {
			mXInitOff = mXOff;
			mYInitOff = mYOff;

			// Change mouse pointer
			::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
		}
	}

	CView::OnLButtonDown(nFlags, point);
}

void CViewerView::OnLButtonUp(UINT nFlags, CPoint point)
{
	if (mSelMode && !(nFlags & MK_CONTROL) && mIsClicked) {
		CPoint transPointS, transPointE;
		QSelRegion selRegion;
		int x0, y0, xn, yn;

		if (mSelRegions.size() >= 0 && mDeltaIdx >= 0) {
			for (int i = 0; i < (int)mSelRegions.size(); i++) {
				if (i != mDeltaIdx)
					continue;

				CRect &selRect = mSelRegions[i].mSelRect;
				x0 = QMAX(selRect.left + mDeltaRect.left, 0);
				xn = QMIN(selRect.right + mDeltaRect.right, mW - 1);
				y0 = QMAX(selRect.top + mDeltaRect.top, 0);
				yn = QMIN(selRect.bottom + mDeltaRect.bottom, mH - 1);

				mSelRegions[i].SetRect(x0, y0, xn, yn);
				mDeltaIdx = -1;
				mDeltaRect.SetRectEmpty();
				goto DefaultOnLButtonUp;
			}
		}

		// Real Start Point
		transPointS.x = int((-mXDst + mPointS.x) / mN);
		transPointS.y = int((-mYDst + mPointS.y) / mN);

		// Real End Point
		transPointE.x = int((-mXDst + point.x) / mN);
		transPointE.y = int((-mYDst + point.y) / mN);

		x0 = min(transPointS.x, transPointE.x);
		x0 = max(x0, 0);
		xn = max(transPointS.x, transPointE.x);
		xn = min(xn, mW - 1);
		if (x0 >= xn || xn - x0 + 1 < LENGTH_MIN_SELECT)
			goto DefaultOnLButtonUp;

		y0 = min(transPointS.y, transPointE.y);
		y0 = max(y0, 0);
		yn = max(transPointS.y, transPointE.y);
		yn = min(yn, mH - 1);
		if (y0 >= yn || yn - y0 + 1 < LENGTH_MIN_SELECT)
			goto DefaultOnLButtonUp;

		selRegion.SetRect(x0, y0, xn, yn);
		mSelRegions.push_back(selRegion);

DefaultOnLButtonUp:
		mPointE.Init();

		Invalidate(FALSE);
	}

	mIsClicked = false;

	CView::OnLButtonUp(nFlags, point);

	::ReleaseCapture();
}

void CViewerView::OnMouseMove(UINT nFlags, CPoint point)
{
	if (mSelMode && !(nFlags & MK_CONTROL)) {
		CPoint transPointS(int((-mXDst + mPointS.x) / mN), int((-mYDst + mPointS.y) / mN));
		CPoint transPointE(int((-mXDst + point.x) / mN), int((-mYDst + point.y) / mN));

		if (mIsClicked) {
			int i = (int)mSelRegions.size() - 1;
			for (; i >= 0; i--) {
				QSelRegion &selRegion = mSelRegions[i];
				CRect &selRect = selRegion.mSelRect;
				CPoint deltaPt = transPointE - transPointS;

				LONG minValue;
				if (selRegion.IsInLR(transPointS)) {
					::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));

					if (nFlags & MK_SHIFT) {
						mDeltaRect.right = deltaPt.x;
						mDeltaRect.left = deltaPt.x;
					} else {
						minValue = LENGTH_MIN_SELECT - selRect.right + selRect.left - 1;
						if (selRegion.IsInR(transPointS)) {
							mDeltaRect.right = max(deltaPt.x, minValue);
						} else {
							mDeltaRect.left = min(deltaPt.x, -minValue);
						}
					}

					mDeltaIdx = i;
				} else if (selRegion.IsInTB(transPointS)) {
					::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS));

					if (nFlags & MK_SHIFT) {
						mDeltaRect.bottom = deltaPt.y;
						mDeltaRect.top = deltaPt.y;
					} else {
						minValue = LENGTH_MIN_SELECT - selRect.bottom + selRect.top - 1;
						if (selRegion.IsInB(transPointS)) {
							mDeltaRect.bottom = max(deltaPt.y, minValue);
						} else {
							mDeltaRect.top = min(deltaPt.y, -minValue);
						}
					}

					mDeltaIdx = i;
				} else {
					continue;
				}

				break;
			}

			if (i < 0) // when any boxes are not being modified
				mPointE.SetPoint(point.x, point.y);
			Invalidate(FALSE);
		} else {
			for (int i = (int)mSelRegions.size() - 1; i >= 0; i--) {
				QSelRegion &selRegion = mSelRegions[i];

				if (selRegion.IsInLR(transPointE))
					::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZEWE));
				else if (selRegion.IsInTB(transPointE))
					::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_SIZENS));
				else
					continue;

				break;
			}
		}
	} else { // if not the SelMode
		if (mIsClicked) {
			mXOff = mXInitOff + (point.x - mPointS.x) / mN;
			mYOff = mYInitOff + (point.y - mPointS.y) / mN;

			mXDst = QDeterminDestPos(mWCanvas, mWDst, mXOff, mN);
			mYDst = QDeterminDestPos(mHCanvas, mHDst, mYOff, mN);

			::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
			Invalidate(FALSE);
		}
	}

	SetCursorCoordinates(point);

	CView::OnMouseMove(nFlags, point);
}

void CViewerView::KillPlayTimerSafe()
{
	mBufferPool->disable();
	mBufferQueue->destroy();
	KillPlayTimer();

	CViewerDoc* pDoc = GetDocument();

	FrmSrc *frmSrc = pDoc->mFrmSrc;
	frmSrc->SetFramePos(pDoc, mStableRgbBufferInfo.ID + 1);
	mStableRgbBufferInfo.ID = QMIN(mStableRgbBufferInfo.ID, pDoc->mFrames - 1);

	mBufferPool->enable(mStableRgbBufferInfo.addr);
	mBufferQueue->init();
	mNewRgbBufferInfoQ->init();
}

cv::Mat CViewerView::CreateRoiMat(int x0, int y0, int w, int h)
{
	int wStride = ROUNDUP_DWORD(w) * QIMG_DST_RGB_BYTES;
	int captBufSize = wStride * h;
	if (mCaptRgbBufSize < captBufSize) {
		if (mCaptRgbBuf)
			_mm_free(mCaptRgbBuf);

		mCaptRgbBufSize = captBufSize;
		mCaptRgbBuf = static_cast<BYTE *>(_mm_malloc(mCaptRgbBufSize, 16));
	}
	qimage_crop(mStableRgbBufferInfo.addr, ROUNDUP_DWORD(mW) * QIMG_DST_RGB_BYTES,
		mCaptRgbBuf, x0 * QIMG_DST_RGB_BYTES, y0, wStride, h);
	cv::Mat roiMat(h, w, CV_8UC3, mCaptRgbBuf, wStride);

	return roiMat;
}

cv::Mat CViewerView::GetRoiMat()
{
	int x0, y0, w, h;
	if (!mSelMode || !mSelRegions.size()) {
		x0 = 0;
		y0 = 0;
		w = mW;
		h = mH;
	} else {
		int xn, yn;
		CRect *selRegion = &mSelRegions[0].mSelRect;
		x0 = selRegion->left;
		xn = selRegion->right;
		y0 = selRegion->top;
		yn = selRegion->bottom;
		w = xn - x0 + 1;
		h = yn - y0 + 1;
	}

	return CreateRoiMat(x0, y0, w, h);
}

inline void FindFirstFileName(CString *candidate, CFileFind &finder,
						  CString &rPathName, BOOL &bWorking)
{
	while (bWorking) {
		bWorking = finder.FindNextFile();

		if (finder.IsDirectory())
			continue;

		CString &curPath = finder.GetFilePath();
		if (rPathName == curPath)
			break;

		*candidate = curPath;
		break;
	}
}

inline void FindLastFileName(CString *candidate, CFileFind &finder, BOOL &bWorking)
{
	while (bWorking) {
		bWorking = finder.FindNextFile();
		if (finder.IsDirectory())
			continue;

		*candidate = finder.GetFilePath();
	}
}

inline void FindPriorFileName(CString *candidate, CFileFind &finder,
						  CString &rPathName, BOOL &bWorking)
{
	while (bWorking) {
		bWorking = finder.FindNextFile();

		if (finder.IsDirectory())
			continue;

		CString &curPath = finder.GetFilePath();
		if (rPathName == curPath)
			break;
		else
			*candidate = curPath;
	}
}

inline void FindNextFileName(CString *candidate, CFileFind &finder,
						  CString &rPathName, BOOL &bWorking)
{
	while (bWorking) {
		bWorking = finder.FindNextFile();

		if (finder.IsDirectory())
			continue;

		if (rPathName != finder.GetFilePath())
			continue;

		while (bWorking) {
			bWorking = finder.FindNextFile();

			if (finder.IsDirectory())
				continue;

			*candidate = finder.GetFilePath();
			break;
		}
		break;
	}
}

void CViewerView::FindFile(CViewerDoc* pDoc, UINT nChar)
{
	CFileFind finder;
	BOOL bWorking = finder.FindFile(pDoc->mPurePathRegex);
	CString candidate = _T("");
	switch (nChar) {
	case VK_PRIOR:
		FindPriorFileName(&candidate, finder, pDoc->mPathName, bWorking);
		break;
	case VK_NEXT:
		FindNextFileName(&candidate, finder, pDoc->mPathName, bWorking);
		break;
	case VK_HOME:
		FindFirstFileName(&candidate, finder, pDoc->mPathName, bWorking);
		break;
	case VK_END:
		FindLastFileName(&candidate, finder, bWorking);
		break;
	}
	finder.Close();

	if (candidate != _T("")) {
		pDoc->OnOpenDocument(candidate);
		pDoc->SetPathName(candidate, TRUE);
	} else {
		LOGWRN("reach the end in this directory");
	}
}

// from: http://pyrisind.tistory.com/29
void CViewerView::ToggleFullScreen()
{
	// The View's parent window of is the CMainFrame in SDI
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	mFullMode = !mFullMode;

	if (mFullMode) {
		pMainFrm->SetMenu(NULL);
		if (pMainFrm->IsZoomed()) {
			pMainFrm->SendMessage(WM_SYSCOMMAND, SC_RESTORE,
				(LPARAM)pMainFrm->GetSafeHwnd());
			mWasZoomed = true;
		}
		pMainFrm->ModifyStyle(WS_CAPTION | WS_SYSMENU, WS_POPUP, SWP_FRAMECHANGED);
		pMainFrm->SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE,
			(LPARAM)pMainFrm->GetSafeHwnd());
	} else {
		HMENU hMenu = ::LoadMenu(theApp.m_hInstance, MAKEINTRESOURCE(IDR_MAINFRAME));
		::SetMenu(pMainFrm->GetSafeHwnd(), hMenu);
		pMainFrm->ModifyStyle(WS_POPUP, WS_CAPTION | WS_SYSMENU, SWP_FRAMECHANGED);
		pMainFrm->AddMainMenu();
		GetDocument()->UpdateMenu();
		pMainFrm->SendMessage(WM_SYSCOMMAND, SC_RESTORE,
			(LPARAM)pMainFrm->GetSafeHwnd());
		if (mWasZoomed) {
			pMainFrm->SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE,
				(LPARAM)pMainFrm->GetSafeHwnd());
			mWasZoomed = false;
		}
	}
}

void CViewerView::ToggleHelp()
{
	mShowHelp = !mShowHelp;
	Invalidate(FALSE);
}

void CViewerView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// to regardless of doc state
	switch (nChar) {
	case VK_RETURN: // handle full-screen order
		ToggleFullScreen();
		goto OnKeyDefault;
	case VK_OEM_2: // '/?' for US
		ToggleHelp();
		goto OnKeyDefault;
	case 'S':
		ToggleSelMode();
		goto OnKeyDefault;
	}

	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc || pDoc->mDocState == DOC_JUSTLOAD)
		goto OnKeyDefault;

	if (mKeyProcessing)
		return;

	switch (nChar) {
	case VK_RIGHT:
		if (mIsPlaying)
			break;
		
		pDoc->NextScene();
		break;
	case VK_LEFT:
		if (mIsPlaying)
			break;

		pDoc->PrevScene();
		break;
	case VK_HOME:
		if (mIsPlaying)
			KillPlayTimerSafe();

		if (pDoc->mFrames > 1)
			pDoc->FirstScene();
		else
			FindFile(pDoc, nChar);
		break;
	case VK_END:
		if (mIsPlaying)
			KillPlayTimerSafe();

		if (pDoc->mFrames > 1)
			pDoc->LastScene();
		else
			FindFile(pDoc, nChar);
		break;
	case VK_SPACE:
		// no need to lock here, since the timer can't change the state
		if (!mIsPlaying) {
			if (pDoc->mCurFrameID == pDoc->mFrames - 1) {
				int ret = pDoc->FirstScene();
				if (ret < 0)
					break;
			}

			SetPlayTimer(pDoc);
		} else {
			KillPlayTimerSafe();
		}
		break;
	case 'H': // hex RGB value
		mHexMode = !mHexMode;
		mRgbFormat = mHexMode ? mRgbHex : mRgbDec;
		break;
	case 'Y': // show only Y among YCbCr
		mYMode = !mYMode;
		break;
	case 'I':
		mInterpol = !mInterpol;
		break;
	case 'R':
		pDoc->Rotate90();
		goto OnKeyDefault;
	case 'C':
		mShowCoord = !mShowCoord;
		break;
	case 'B':
		mShowBoxInfo = !mShowBoxInfo;
		break;
	case VK_PRIOR:
	case VK_NEXT:
		if (mIsPlaying)
			KillPlayTimerSafe();
		FindFile(pDoc, nChar);
		break;
	}

	// Once a key is pressed, update all views any way.
	// This scheme makes code prettier.
	mKeyProcessing = true;
	Invalidate(FALSE);

OnKeyDefault:

	CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

void CViewerView::OnDestroy()
{
	CView::OnDestroy();

	mBufferPool->disable();
	mBufferQueue->destroy();
	KillPlayTimer();
}

int CViewerView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CView::OnCreate(lpCreateStruct) == -1)
		return -1;

	ModifyStyleEx(WS_EX_CLIENTEDGE, 0, SWP_FRAMECHANGED);

	// Here is the earliest function I can access the pDoc
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		goto OnCreateDefault;

	mBufferPool = pDoc->mBufferPool;
	mBufferQueue = pDoc->mBufferQueue;

OnCreateDefault:
	return 0;
}

void CViewerView::OnSize(UINT nType, int cx, int cy)
{
	CView::OnSize(nType, cx, cy);

	mWClient = cx;
	mHClient = cy;

	mWCanvas = mWClient;
	mHCanvas = mHClient - mHProgress;

	mXDst = QDeterminDestPos(mWCanvas, mWDst, mXOff, mN);
	mYDst = QDeterminDestPos(mHCanvas, mHDst, mYOff, mN);

	mRcProgress.SetRect(0, mHCanvas, mWClient, mHClient);
}

void CViewerView::OnRButtonUp(UINT nFlags, CPoint point)
{
	CPoint screenPoint = point;

	ClientToScreen(&screenPoint);
	mMouseMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN | TPM_LEFTBUTTON,
		screenPoint.x, screenPoint.y, this);

	CView::OnRButtonUp(nFlags, point);
}

void CViewerView::ToggleSelMode()
{
	UINT nFlags = MF_BYCOMMAND;
	mSelMode = !mSelMode;
	if (!mSelMode) {
		mPointS.Init();
		mPointE.Init();
		Invalidate(FALSE);
	}
	nFlags |= mSelMode ? MF_CHECKED : MF_UNCHECKED;
	mMouseMenu.CheckMenuItem(QMouseMenuSel - QMouseMenuStart + ID_MOUSEMENU_START, nFlags);
}

void CViewerView::OnMouseMenu(UINT nID)
{
	int idx = nID - ID_MOUSEMENU_START;
	switch(QMouseMenu[idx].id) {
	case QMouseMenuSel:
		ToggleSelMode();
		break;
	default:
		QASSERT(0);
	}
}
