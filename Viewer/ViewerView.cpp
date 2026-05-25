// ViewerView.cpp : implementation of the CViewerView class
//

#include "stdafx.h"
#include "Viewer.h"
#include "MainFrm.h"

#include "ViewerDoc.h"
#include "ViewerView.h"

#include "QMath.h"
#include "QDebug.h"

#include "QViewerCmn.h"
#include "qimage_cs.h"
#include "qimage_util.h"

#include "FrmSrc.h"

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

#define WM_VIEWER_PLAY_TIMER (WM_APP + 1)

enum QMouseMenuID
{
	QMouseMenuStart = 0x10000000,
	QMouseMenuSel = QMouseMenuStart,
	QMouseMenuSyncInput,
};

struct QMouseMenuInfo {
	const char *name;
	const QMouseMenuID id;
};

static const QMouseMenuInfo QMouseMenu[] = {
	{ "Sel Mode", QMouseMenuSel },
	{ "Sync Input", QMouseMenuSyncInput },
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
	ON_MESSAGE(WM_VIEWER_PLAY_TIMER, CViewerView::OnPlayTimer)
END_MESSAGE_MAP()

CViewerView::CViewerView()
: mW(VIEWER_DEF_W)
, mH(VIEWER_DEF_H)
, mD(0.f)
, mN(ZOOM_RATIO(mD))
, mIsClicked(false)
, mXOff(.0f)
, mYOff(.0f)
, mLastSyncViewStateTick(0)
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
, mPlaybackStartFrameID(0)
, mDroppedFrameCount(0)
, mPlaybackRate(1.0)
, mPlaybackEndPending(false)
, mPlayTickPosted(0)
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
, mShowSourceYuv(true)
, mRgbHex(_T("%02X\n%02X\n%02X"))
, mRgbDec(_T("%03d\n%03d\n%03d"))
, mRgbFormat(mRgbDec)
, mNewSel(false)
, mDeltaIdx(-1)
{
	LOGFONT lf;

	::ZeroMemory(&lf, sizeof(lf));
	lf.lfHeight = PROGRESS_FONT_H;
	lf.lfWeight = FW_SEMIBOLD;

	::lstrcpy(lf.lfFaceName, Q1UI_FONT_TEXT);
	mProgressFont.CreateFontIndirect(&lf);

	lf.lfWeight = FW_NORMAL;
	::lstrcpy(lf.lfFaceName, Q1UI_FONT_MONO);
	mDefPixelTextFont.CreateFontIndirect(&lf);

	lf.lfWeight = FW_NORMAL;
	::lstrcpy(lf.lfFaceName, Q1UI_FONT_MONO);
	mConsolasFont.CreateFontIndirect(&lf);

	// Bitmap header reused when copying RGB buffers to the view.
	BITMAPINFOHEADER &bmiHeader = mBmi.bmiHeader;
	bmiHeader.biSize = (DWORD)sizeof(BITMAPINFOHEADER);
	bmiHeader.biPlanes = 1;
	bmiHeader.biBitCount = 24;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = 0L;
	bmiHeader.biXPelsPerMeter = 0L;
	bmiHeader.biYPelsPerMeter = 0L;

	// Query timer capabilities now; request resolution only during playback.
	timeGetDevCaps(&mTc, sizeof (TIMECAPS));
	QueryPerformanceFrequency(&mPlaybackClockFrequency);
	mPlaybackStartCounter.QuadPart = 0;

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
	return CView::PreCreateWindow(cs);
}

void CViewerView::AdjustWindowSize()
{
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	// In this SDI app the view's parent is CMainFrame.
	CWnd *pMainFrm = GetParent();

	// Restore before resizing so the client area is recalculated from a
	// normal frame, not from a previously maximized image.
	if (!mFullMode && pMainFrm->IsZoomed()) {
		pMainFrm->SendMessage(WM_SYSCOMMAND, SC_RESTORE,
			(LPARAM)pMainFrm->GetSafeHwnd());
	}

	DWORD dsStyle = pMainFrm->GetStyle();
	DWORD dsStyleEx = pMainFrm->GetExStyle();

	CRect curRcWin, curRcClient;

	GetWindowRect(&curRcWin);
	GetClientRect(&curRcClient);

	// Preserve the non-client area while sizing the frame around the image.
	int wGap = curRcWin.Width() - curRcClient.Width();
	int hGap = curRcWin.Height() - curRcClient.Height();

	int wClient = max(VIEWER_DEF_W, pDoc->mW);
	int hClient = max(VIEWER_DEF_H, pDoc->mH);

	CRect rcWin(0, 0, wClient + wGap, hClient + mHProgress + hGap);
	::AdjustWindowRectEx(&rcWin, dsStyle, TRUE, dsStyleEx);

	if (!mFullMode) {
		pMainFrm->SetWindowPos(NULL, 0, 0,
			rcWin.Width(), rcWin.Height(), SWP_NOMOVE | SWP_FRAMECHANGED);
	}
}

void CViewerView::Initialize(int nFrame, size_t rgbStride, int w, int h, bool preserveViewState)
{
	float prevD = mD;
	float prevN = mN;
	float prevXOff = mXOff;
	float prevYOff = mYOff;

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

	if (preserveViewState) {
		mD = prevD;
		mN = prevN;
		mXOff = prevXOff;
		mYOff = prevYOff;
		SetDstSize();
	}

	// OnSize has already updated the client and canvas dimensions.

	mXDst = q1::DeterminDestPos(mWCanvas, mWDst, mXOff, mN);
	mYDst = q1::DeterminDestPos(mHCanvas, mHDst, mYOff, mN);

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

	CRect progressBand(0, mHCanvas, mWClient, mHClient);
	pDC->FillSolidRect(progressBand, Q1UI_COLOR_SURFACE_ALT);

	CRect barTextRect, barRect;
	CRect trackRect;

	trackRect.top = mHCanvas + barMargin;
	trackRect.left = barMargin;
	trackRect.bottom = mHClient - barMargin;
	trackRect.right = mWClient - barMargin;
	pDC->FillSolidRect(trackRect, Q1UI_COLOR_BORDER_SOFT);

	barRect = trackRect;
	barRect.right = barRect.left + limit;
	pDC->FillSolidRect(&barRect, mBarColor);

	barTextRect.top = mHCanvas;
	barTextRect.left = 0;
	barTextRect.bottom = mHClient;
	barTextRect.right = mWClient - barMargin;

	pDC->SelectObject(&mProgressFont);
	pDC->SetTextColor(COLOR_PROGRESS_TEXT);
	pDC->SetBkMode(TRANSPARENT);
	pDC->DrawText(str, &barTextRect, DT_SINGLELINE | DT_RIGHT |  DT_VCENTER);
}

void CViewerView::_ScaleRgb(BYTE *src, BYTE *dst, int sDst, q1::GridInfo &gi)
{
	long gap, yStart, yEnd, xStart, xEnd;

	if (mYDst > 0 || mXDst > 0) // The image is smaller than the canvas.
		memset(dst, 0xf7, sDst * mHClient * QIMG_DST_RGB_BYTES);

	// Visible range of the scaled image on the canvas.
	if (mYDst > 0) {
		dst += sDst * mYDst * QIMG_DST_RGB_BYTES;
		yStart = 0;
		yEnd = mHDst;
	} else {
		yStart = -mYDst;
		yEnd = QMIN(mHDst, mHCanvas - mYDst);
	}

	if (mXDst >= 0 && sDst >= mWDst) {
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
		q1::Interpolate(src, mH, mW, mWCanvas, xStart, xEnd, yStart, yEnd, mNnOffsetBuf, dst);
	} else {
		q1::NearestNeighbor(src, mH, mW, mHDst, mWDst, mXDst, mYDst, mN, xStart, xEnd,
			yStart, yEnd, gap, gi, mNnOffsetBuf, mNnOffsetYBorderFlag, mNnOffsetXBorderFlag, dst);
	}
}

// Multimedia timer callbacks run outside the UI thread. Keep them lightweight:
// the UI thread owns presentation decisions and buffer lifetime.
static void CALLBACK PlayTimerThread(UINT uID, UINT uMsg, DWORD_PTR dwUser,
		DWORD_PTR dw1, DWORD_PTR dw2)
{
	CViewerView *pView = reinterpret_cast<CViewerView *>(dwUser);
	if (::InterlockedExchange(&pView->mPlayTickPosted, 1) == 0)
		::PostMessage(pView->m_hWnd, WM_VIEWER_PLAY_TIMER, 0, 0);
}

LRESULT CViewerView::OnPlayTimer(WPARAM wParam, LPARAM lParam)
{
	::InterlockedExchange(&mPlayTickPosted, 0);
	if (!mIsPlaying)
		return 0;

	CViewerDoc* pDoc = GetDocument();
	long dueFrameID = GetDuePlaybackFrameID(pDoc);
	BufferInfo latest = {0, };
	bool hasLatest = mNewRgbBufferInfoQ->try_pop(latest);
	long candidateFrameID = mStableRgbBufferInfo.ID;

	if (hasLatest) {
		if (latest.ID == MSG_QUIT) {
			mNewRgbBufferInfoQ->push(latest);
			Invalidate(FALSE);
			return 0;
		}
		candidateFrameID = latest.ID;
	}

	while (candidateFrameID < dueFrameID) {
		BufferInfo candidate = {0, };
		if (!mBufferQueue->try_pop(candidate))
			break;

		if (candidate.ID == MSG_QUIT) {
			if (hasLatest) {
				mPlaybackEndPending = true;
			} else {
				latest = candidate;
				hasLatest = true;
			}
			break;
		}

		if (hasLatest && latest.addr) {
			mBufferPool->turn_back(latest.addr);
			mDroppedFrameCount++;
		}
		latest = candidate;
		hasLatest = true;
		candidateFrameID = candidate.ID;
	}

	if (hasLatest) {
		if (mNewRgbBufferInfoQ->push(latest)) {
			Invalidate(FALSE);
		} else if (latest.addr) {
			mBufferPool->turn_back(latest.addr);
			mDroppedFrameCount++;
		}
	}
	return 0;
}

double CViewerView::GetEffectivePlaybackFps(const CViewerDoc* pDoc) const
{
	double fps = pDoc->mFps * mPlaybackRate;
	return fps > 0.0 ? fps : VIEWER_DEF_FPS;
}

long CViewerView::GetDuePlaybackFrameID(const CViewerDoc* pDoc) const
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);

	double elapsedSeconds = (now.QuadPart - mPlaybackStartCounter.QuadPart) /
		static_cast<double>(mPlaybackClockFrequency.QuadPart);
	return mPlaybackStartFrameID +
		static_cast<long>(elapsedSeconds * GetEffectivePlaybackFps(pDoc));
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
	mPlaybackStartFrameID = pDoc->mCurFrameID;
	mDroppedFrameCount = 0;
	mPlaybackEndPending = false;
	::InterlockedExchange(&mPlayTickPosted, 0);
	QueryPerformanceCounter(&mPlaybackStartCounter);

	double pollMilliseconds = 500.0 / GetEffectivePlaybackFps(pDoc);
	if (pollMilliseconds > 5.0)
		pollMilliseconds = 5.0;
	if (pollMilliseconds < static_cast<double>(mTc.wPeriodMin))
		pollMilliseconds = static_cast<double>(mTc.wPeriodMin);
	UINT pollPeriod = static_cast<UINT>(pollMilliseconds + 0.999);

	timeBeginPeriod(mTc.wPeriodMin);
	mTimerID = timeSetEvent(pollPeriod, mTc.wPeriodMin, PlayTimerThread,
		(DWORD_PTR)this, TIME_PERIODIC | TIME_KILL_SYNCHRONOUS);
	if (mTimerID == 0) {
		mIsPlaying = false;
		timeEndPeriod(mTc.wPeriodMin);
		frmSrc->Stop();
		return;
	}

	double startSec = pDoc->mFps > 0.0 ? pDoc->mCurFrameID / pDoc->mFps : 0.0;
	if (mAudioPlayer.Open(pDoc->mPathName.GetString()))
		mAudioPlayer.Play(startSec);
}

// Timer callbacks only post clock ticks, so pausing does not need to wait for
// a callback that might own a frame buffer.
void CViewerView::KillPlayTimer()
{
	if (!mIsPlaying)
		return;

	mAudioPlayer.Pause();
	mIsPlaying = false;
	timeKillEvent(mTimerID);
	mTimerID = 0;
	timeEndPeriod(mTc.wPeriodMin);
	::InterlockedExchange(&mPlayTickPosted, 0);
	mPlaybackEndPending = false;

	mNewRgbBufferInfoQ->destroy();

	CViewerDoc* pDoc = GetDocument();
	std::vector<FrmSrc *> &frmSrcs = pDoc->mFrmSrcs;
	for (auto it = std::begin(frmSrcs); it != std::end(frmSrcs); it++)
		(*it)->Stop();
}

void CViewerView::PrintPlaySpeed(double fps)
{
	mPlayFrameCount++;

	if (mPlayFrameCount % ROUND2I(fps) == 1) {
		ULONGLONG cur = ::GetTickCount64();
		if (mPreKeyFrameStamp)
			LOGINF("%f fps takes %llu msec", fps, cur - mPreKeyFrameStamp);

		mPreKeyFrameStamp = cur;
	}
}

void CViewerView::ScaleRgbBuf(BYTE *src, BYTE **pDst, q1::GridInfo &gi)
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

	// Top-left vertex of the box.
	ret.left = CEIL2I(rt.left * mN) + mXDst;
	ret.top = CEIL2I(rt.top *  mN) + mYDst;

	// CDC::Rectangle() excludes the bottom-right edge, so expand by 1px.
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

void CViewerView::DrawPixelText(CDC *pDC, q1::GridInfo &gi)
{
	CViewerDoc* pDoc = GetDocument();
	CString label;
	CRect refRect;
	LOGFONT lf;
	CFont pixelTextFont;
	mDefPixelTextFont.GetLogFont(&lf);
	lf.lfHeight = LONG(mN * 4 / 15);
	pixelTextFont.CreateFontIndirect(&lf);
	pDC->SelectObject(&pixelTextFont);
	pDC->SetTextColor(COLOR_PIXEL_TEXT);
	pDC->DrawText(_T("0000\n0000\n0000"), -1, refRect,
		DT_CENTER | DT_VCENTER | DT_CALCRECT);
	int y = gi.y;
	for (int i = 0; i < (int)gi.Hs.size(); i++) {
		int x = gi.x;
		for (int j = 0; j < (int)gi.Ws.size(); j++) {
			CRect rect(x, y + (gi.Hs[i] - refRect.Height()) / 2,
				x + gi.Ws[j] - 1, y + gi.Hs[i] - 1);
			cv::Vec3b px = gi.pixelMap.at<cv::Vec3b>(i, j);
			pDC->SetBkColor(RGB((0x80 + px[2]) / 2, (0x80 + px[1]) / 2, (0x80 + px[0]) / 2));

			int viewY = gi.pixelCoordMap.at<cv::Vec2w>(i, j)[0] / QIMG_DST_RGB_BYTES;
			int viewX = gi.pixelCoordMap.at<cv::Vec2w>(i, j)[1] / QIMG_DST_RGB_BYTES;
			QIMAGE_NATIVE_PIXEL_SAMPLE sample = {};
			bool hasNative = pDoc->GetNativePixelSample(viewX, viewY, &sample);
			if (mShowSourceYuv && hasNative &&
					sample.model == QIMAGE_PIXEL_MODEL_YUV) {
				int digits = mHexMode ? (sample.bit_depth + 3) / 4 :
					(sample.bit_depth > 8 ? 4 : 3);
				if (mHexMode) {
					label.Format(_T("%0*X\n%0*X\n%0*X"),
						digits, sample.component[0], digits, sample.component[1],
						digits, sample.component[2]);
				} else {
					label.Format(_T("%0*d\n%0*d\n%0*d"),
						digits, sample.component[0], digits, sample.component[1],
						digits, sample.component[2]);
				}
			} else if (hasNative && sample.model == QIMAGE_PIXEL_MODEL_RGB) {
				int digits = mHexMode ? (sample.bit_depth + 3) / 4 :
					(sample.bit_depth > 8 ? 4 : 3);
				if (mHexMode) {
					label.Format(_T("%0*X\n%0*X\n%0*X"),
						digits, sample.component[0], digits, sample.component[1],
						digits, sample.component[2]);
				} else {
					label.Format(_T("%0*d\n%0*d\n%0*d"),
						digits, sample.component[0], digits, sample.component[1],
						digits, sample.component[2]);
				}
			} else {
				label.Format(mRgbFormat, px[2], px[1], px[0]);
			}

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
	const int W_MARGIN = 18;
	const int H_MARGIN = 14;
	const int X_HELP = (mWCanvas - W_HELP) / 2;
	const int Y_HELP = (mHCanvas - H_HELP) / 2;
	CRect bgRect(X_HELP, Y_HELP, X_HELP + W_HELP, Y_HELP + H_HELP);
	pDC->FillSolidRect(bgRect, Q1UI_COLOR_SURFACE);
	CPen borderPen(PS_SOLID, 1, Q1UI_COLOR_BORDER);
	CPen *prevPen = pDC->SelectObject(&borderPen);
	pDC->SelectStockObject(NULL_BRUSH);
	pDC->Rectangle(bgRect);
	pDC->SelectObject(prevPen);

	CRect manualRect(bgRect.left + W_MARGIN, bgRect.top + H_MARGIN,
		bgRect.right - W_MARGIN, bgRect.bottom - H_MARGIN);
	LOGFONT lf;
	CFont manualFont;
	mConsolasFont.GetLogFont(&lf);
	lf.lfHeight = 14;
	lf.lfWeight = FW_NORMAL;
	manualFont.CreateFontIndirect(&lf);
	pDC->SelectObject(&manualFont);
	pDC->SetTextColor(Q1UI_COLOR_TEXT);
	CString manual(
		"Viewer shortcuts\n"
		"\n"
		"?              Show or hide this panel\n"
		"Drag && Drop    Open an image\n"
		"Mouse Wheel    Zoom in or out; high zoom shows pixel values\n"
		"Return         Full screen\n"
		"H              Toggle hex pixel values\n"
		"Y              Toggle Y-only view\n"
		"V              Toggle RGB/source YUV pixel values\n"
		"R              Rotate 90 degrees clockwise\n"
		"Page Up/Down   Previous or next file\n"
		"S              Selection capture mode\n"
		"Ctrl + C       Capture viewer screen or selected region\n"
		"Ctrl + V       Paste image from clipboard\n"
		"Left/Right     Previous or next video frame\n"
		"Home/End       First or last frame/file\n"
		"Space          Play or stop video\n"
		"Click bottom   Seek to a video frame\n"
		"C              Cursor coordinates\n"
		"B              Selected box size\n"
		"I              Interpolate pixels\n"
		"N              Next color space\n"
		);
	pDC->DrawText(manual, &manualRect, DT_LEFT | DT_TOP);
}

void CViewerView::DrawEmptyState(CDC *pDC)
{
	pDC->FillSolidRect(CRect(0, 0, mWClient, mHClient), Q1UI_COLOR_CANVAS_BG);

	LOGFONT lf;
	mConsolasFont.GetLogFont(&lf);
	::lstrcpy(lf.lfFaceName, Q1UI_FONT_TEXT);

	CFont titleFont;
	lf.lfHeight = 22;
	lf.lfWeight = FW_SEMIBOLD;
	titleFont.CreateFontIndirect(&lf);

	CFont bodyFont;
	lf.lfHeight = 14;
	lf.lfWeight = FW_NORMAL;
	bodyFont.CreateFontIndirect(&lf);

	CString title(_T("Open or drop an image"));
	CString body(_T("Ctrl+O to open, Ctrl+V to paste, mouse wheel to zoom"));

	CRect rect(0, 0, mWClient, mHClient);
	rect.DeflateRect(24, 24);
	CRect titleRect = rect;
	titleRect.bottom = rect.CenterPoint().y - 4;
	CRect bodyRect = rect;
	bodyRect.top = rect.CenterPoint().y + 6;

	pDC->SetBkMode(TRANSPARENT);
	CFont *prevFont = pDC->SelectObject(&titleFont);
	pDC->SetTextColor(Q1UI_COLOR_TEXT);
	pDC->DrawText(title, &titleRect, DT_SINGLELINE | DT_CENTER | DT_BOTTOM | DT_END_ELLIPSIS);
	pDC->SelectObject(&bodyFont);
	pDC->SetTextColor(Q1UI_COLOR_TEXT_MUTED);
	pDC->DrawText(body, &bodyRect, DT_SINGLELINE | DT_CENTER | DT_TOP | DT_END_ELLIPSIS);
	pDC->SelectObject(prevFont);
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
	pDC->SetTextColor(Q1UI_COLOR_OVERLAY_TEXT);
	pDC->DrawText(coord, &bgRect, DT_CENTER | DT_VCENTER);
}

void CViewerView::DrawPixelValueMode(CDC *pDC)
{
	QIMAGE_NATIVE_PIXEL_SAMPLE sample = {};
	if (!GetDocument()->GetNativePixelSample(0, 0, &sample) ||
			sample.model != QIMAGE_PIXEL_MODEL_YUV) {
		return;
	}

	LOGFONT lf;
	CFont modeFont;
	mConsolasFont.GetLogFont(&lf);
	lf.lfHeight = 14;
	modeFont.CreateFontIndirect(&lf);
	CFont *prevFont = pDC->SelectObject(&modeFont);

	CString mode = mShowSourceYuv ? _T("PIXEL: Y/U/V SOURCE") :
		_T("PIXEL: R/G/B DISPLAY");
	CRect refRect;
	pDC->DrawText(mode, -1, refRect, DT_SINGLELINE | DT_CALCRECT);
	CRect bgRect(0, 0, refRect.Width(), refRect.Height());
	bgRect.InflateRect(8, 3, 8, 3);
	pDC->FillSolidRect(bgRect, COLOR_COORDINATE_RECT);
	int prevBkMode = pDC->SetBkMode(TRANSPARENT);
	pDC->SetTextColor(Q1UI_COLOR_OVERLAY_TEXT);
	pDC->DrawText(mode, &bgRect, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
	pDC->SetBkMode(prevBkMode);
	pDC->SelectObject(prevFont);
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
	pDC->SetTextColor(Q1UI_COLOR_OVERLAY_TEXT);
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
	memDC.FillSolidRect(CRect(0, 0, mWClient, mHClient), Q1UI_COLOR_CANVAS_BG);

	bool stopAfterPresent = false;
	BufferInfo bi;
	bool ok = mNewRgbBufferInfoQ->try_pop(bi);
	if (ok && (bi.addr != mStableRgbBufferInfo.addr || bi.ID != mStableRgbBufferInfo.ID)) {
		if (bi.ID == MSG_QUIT) {
			KillPlayTimerSafe();
			pDoc->RefreshNativePixelSource();
		} else {
			mBufferPool->turn_back(mStableRgbBufferInfo.addr);
			mStableRgbBufferInfo = bi;

			// Optional playback timing trace.
			if (mIsPlaying & printPlaySpeed)
				PrintPlaySpeed(pDoc->mFps);

			stopAfterPresent = mPlaybackEndPending;
			mPlaybackEndPending = false;
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

	if (!src) {
		DrawEmptyState(&memDC);
		if (mShowHelp)
			DrawHelpMenu(&memDC);
		pDC->BitBlt(0, 0, mWClient, mHClient, &memDC, 0, 0, SRCCOPY);
		return;
	}

	q1::GridInfo gi;
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
	if (!mIsPlaying && mN > ZOOM_TEXT_START) {
		DrawPixelText(&memDC, gi);
		DrawPixelValueMode(&memDC);
	}

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

	if (stopAfterPresent && mIsPlaying) {
		KillPlayTimerSafe();
		pDoc->RefreshNativePixelSource();
	}

	if (mKeyProcessing == true)
		mKeyProcessing = false;
}


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
		// The view is fully repainted from an off-screen buffer.
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
		mN = q1::GetFitRatio(mN, mW, mH, mWCanvas, mHCanvas);
	} else {
		float d = mD + zDelta / WHEEL_DELTA;
		float fitN = q1::GetBestFitRatio(mW, mH, mWCanvas, mHCanvas);
		mN = q1::GetNextN(mN, fitN, d);
	}
	mD = ZOOM_DELTA(mN);

	SetDstSize();

	mXOff += xShift / mN;
	mYOff += yShift / mN;

	mXDst = q1::DeterminDestPos(mWCanvas, mWDst, mXOff, mN);
	mYDst = q1::DeterminDestPos(mHCanvas, mHDst, mYOff, mN);

	SetCursorCoordinates(clientPoint);

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	pMainFrm->UpdateMagnication(mN, mWDst, mHDst);
}

void CViewerView::ApplyViewState(float zoom, float xOff, float yOff)
{
	float previousZoom = mN;
	int previousWidth = mWDst;
	int previousHeight = mHDst;

	mN = zoom;
	mD = ZOOM_DELTA(mN);
	mXOff = xOff;
	mYOff = yOff;

	SetDstSize();
	mXDst = q1::DeterminDestPos(mWCanvas, mWDst, mXOff, mN);
	mYDst = q1::DeterminDestPos(mHCanvas, mHDst, mYOff, mN);

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	if (previousZoom != mN || previousWidth != mWDst ||
			previousHeight != mHDst) {
		pMainFrm->UpdateMagnication(mN, mWDst, mHDst);
	}
	Invalidate(FALSE);
}

void CViewerView::BroadcastViewState(bool force)
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	ULONGLONG now = ::GetTickCount64();
	if (!force && now - mLastSyncViewStateTick < 16)
		return;

	mLastSyncViewStateTick = now;
	ViewerSyncInputState input = {};
	input.command = VIEWER_SYNC_VIEW_STATE;
	input.scalar = mN;
	input.x = mXOff;
	input.y = mYOff;
	pMainFrm->BroadcastSyncInput(input);
}

BOOL CViewerView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc || pDoc->mDocState == DOC_JUSTLOAD)
		goto OnMouseWheelDefault;

	ChangeZoom(zDelta, pt);
	Invalidate(FALSE);
	BroadcastViewState(true);

OnMouseWheelDefault:
	return CView::OnMouseWheel(nFlags, zDelta, pt);
}

void CViewerView::OnLButtonDown(UINT nFlags, CPoint point)
{
	if (mRcProgress.PtInRect(point)) {
		CViewerDoc* pDoc = GetDocument();
		CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
		bool wasPlaying = false;

		if (mIsPlaying) {
			wasPlaying = true;
			KillPlayTimerSafe();
		}

		double R = (point.x + 1) / double(mWCanvas);
		int frameID = max(ROUND2I(R * pDoc->mFrames) - 1, 0);
		if (pDoc->SeekScene(frameID) >= 0) {
			mKeyProcessing = true;
			Invalidate(FALSE);
			ViewerSyncInputState input = {};
			input.command = VIEWER_SYNC_SEEK_FRAME;
			input.first = frameID;
			pMainFrm->BroadcastSyncInput(input);
		}

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

	if (mIsClicked && (!mSelMode || (nFlags & MK_CONTROL)))
		BroadcastViewState(true);

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

			mXDst = q1::DeterminDestPos(mWCanvas, mWDst, mXOff, mN);
			mYDst = q1::DeterminDestPos(mHCanvas, mHDst, mYOff, mN);

			::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_HAND));
			Invalidate(FALSE);
			BroadcastViewState();
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

bool CViewerView::FindFile(CViewerDoc* pDoc, UINT nChar)
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
		if (pDoc->OnOpenDocument(candidate)) {
			pDoc->SetPathName(candidate, TRUE);
			return true;
		}
	} else {
		LOGWRN("reach the end in this directory");
	}

	return false;
}

void CViewerView::ToggleFullScreen()
{
	// In this SDI app the view's parent is CMainFrame.
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

UINT CViewerView::GetDisplayOptions() const
{
	UINT options = 0;
	if (mHexMode)
		options |= VIEWER_DISPLAY_HEX_PIXEL;
	if (mYMode)
		options |= VIEWER_DISPLAY_Y_ONLY;
	if (mInterpol)
		options |= VIEWER_DISPLAY_INTERPOLATE;
	if (mShowCoord)
		options |= VIEWER_DISPLAY_COORDINATES;
	if (mShowBoxInfo)
		options |= VIEWER_DISPLAY_BOX_INFO;
	if (mShowSourceYuv)
		options |= VIEWER_DISPLAY_SOURCE_YUV;

	return options;
}

void CViewerView::BroadcastDisplayOptions()
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	ViewerSyncInputState input = {};
	input.command = VIEWER_SYNC_DISPLAY_OPTIONS;
	input.first = GetDisplayOptions();
	pMainFrm->BroadcastSyncInput(input);
}

void CViewerView::ApplyPlaybackState(bool play)
{
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc || pDoc->mDocState == DOC_JUSTLOAD)
		return;

	if (play) {
		if (!mIsPlaying) {
			if (pDoc->mCurFrameID == pDoc->mFrames - 1 &&
					pDoc->FirstScene() < 0) {
				return;
			}
			SetPlayTimer(pDoc);
		}
	} else if (mIsPlaying) {
		KillPlayTimerSafe();
		pDoc->QueueSource2View();
	}

	mKeyProcessing = true;
	Invalidate(FALSE);
}

void CViewerView::ApplySyncInput(const ViewerSyncInputState &input)
{
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc || pDoc->mDocState == DOC_JUSTLOAD)
		return;

	int result = -1;
	switch (input.command) {
	case VIEWER_SYNC_SEEK_FRAME: {
		bool wasPlaying = mIsPlaying;
		if (wasPlaying)
			KillPlayTimerSafe();
		result = pDoc->SeekScene(static_cast<int>(input.first));
		if (wasPlaying)
			SetPlayTimer(pDoc);
		break;
	}
	case VIEWER_SYNC_FIRST_FILE:
	case VIEWER_SYNC_LAST_FILE:
	case VIEWER_SYNC_PREVIOUS_FILE:
	case VIEWER_SYNC_NEXT_FILE: {
		if (mIsPlaying)
			KillPlayTimerSafe();
		UINT key = VK_PRIOR;
		if (input.command == VIEWER_SYNC_FIRST_FILE)
			key = VK_HOME;
		else if (input.command == VIEWER_SYNC_LAST_FILE)
			key = VK_END;
		else if (input.command == VIEWER_SYNC_NEXT_FILE)
			key = VK_NEXT;
		result = FindFile(pDoc, key) ? 0 : -1;
		break;
	}
	case VIEWER_SYNC_VIEW_STATE:
		ApplyViewState(static_cast<float>(input.scalar),
			static_cast<float>(input.x), static_cast<float>(input.y));
		return;
	case VIEWER_SYNC_ROTATE:
		pDoc->Rotate90();
		return;
	case VIEWER_SYNC_PLAYBACK:
		ApplyPlaybackState(input.first != 0);
		return;
	case VIEWER_SYNC_DISPLAY_OPTIONS:
		mHexMode = (input.first & VIEWER_DISPLAY_HEX_PIXEL) != 0;
		mYMode = (input.first & VIEWER_DISPLAY_Y_ONLY) != 0;
		mInterpol = (input.first & VIEWER_DISPLAY_INTERPOLATE) != 0;
		mShowCoord = (input.first & VIEWER_DISPLAY_COORDINATES) != 0;
		mShowBoxInfo = (input.first & VIEWER_DISPLAY_BOX_INFO) != 0;
		mShowSourceYuv = (input.first & VIEWER_DISPLAY_SOURCE_YUV) != 0;
		mRgbFormat = mHexMode ? mRgbHex : mRgbDec;
		mKeyProcessing = true;
		Invalidate(FALSE);
		return;
	default:
		return;
	}

	if (result < 0)
		return;

	mKeyProcessing = true;
	Invalidate(FALSE);
}

bool CViewerView::HandleNavigationKey(UINT nChar)
{
	CViewerDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	ViewerSyncInputState input = {};
	int result = -1;
	bool stoppedPlayback = false;

	switch (nChar) {
	case VK_LEFT:
		if (!mIsPlaying)
			result = pDoc->PrevScene();
		break;
	case VK_RIGHT:
		if (!mIsPlaying)
			result = pDoc->NextScene();
		break;
	case VK_HOME:
		if (mIsPlaying) {
			KillPlayTimerSafe();
			stoppedPlayback = true;
		}
		if (pDoc->mFrames > 1) {
			result = pDoc->FirstScene();
		} else if (FindFile(pDoc, VK_HOME)) {
			result = 0;
			input.command = VIEWER_SYNC_FIRST_FILE;
		}
		break;
	case VK_END:
		if (mIsPlaying) {
			KillPlayTimerSafe();
			stoppedPlayback = true;
		}
		if (pDoc->mFrames > 1) {
			result = pDoc->LastScene();
		} else if (FindFile(pDoc, VK_END)) {
			result = 0;
			input.command = VIEWER_SYNC_LAST_FILE;
		}
		break;
	case VK_PRIOR:
		if (mIsPlaying) {
			KillPlayTimerSafe();
			stoppedPlayback = true;
		}
		if (FindFile(pDoc, VK_PRIOR)) {
			result = 0;
			input.command = VIEWER_SYNC_PREVIOUS_FILE;
		}
		break;
	case VK_NEXT:
		if (mIsPlaying) {
			KillPlayTimerSafe();
			stoppedPlayback = true;
		}
		if (FindFile(pDoc, VK_NEXT)) {
			result = 0;
			input.command = VIEWER_SYNC_NEXT_FILE;
		}
		break;
	default:
		return false;
	}

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	if (stoppedPlayback) {
		ViewerSyncInputState playback = {};
		playback.command = VIEWER_SYNC_PLAYBACK;
		playback.first = FALSE;
		pMainFrm->BroadcastSyncInput(playback);
	}

	if (result >= 0) {
		if (input.command == 0) {
			input.command = VIEWER_SYNC_SEEK_FRAME;
			input.first = result;
		}
		pMainFrm->BroadcastSyncInput(input);
		mKeyProcessing = true;
		Invalidate(FALSE);
	}

	return true;
}

void CViewerView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// These keys work even before a document is loaded.
	switch (nChar) {
	case VK_RETURN:
		ToggleFullScreen();
		goto OnKeyDefault;
	case VK_OEM_2: // '?'/'/' key on a US keyboard.
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

	if (HandleNavigationKey(nChar))
		goto OnKeyDefault;

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	switch (nChar) {
	case VK_SPACE:
		// No lock is needed here; playback callbacks are marshalled to the UI thread.
		if (!mIsPlaying) {
			if (pDoc->mCurFrameID == pDoc->mFrames - 1) {
				int ret = pDoc->FirstScene();
				if (ret < 0)
					break;
			}

			SetPlayTimer(pDoc);
		} else {
			KillPlayTimerSafe();
			pDoc->QueueSource2View();
		}
		{
			ViewerSyncInputState input = {};
			input.command = VIEWER_SYNC_PLAYBACK;
			input.first = mIsPlaying ? TRUE : FALSE;
			pMainFrm->BroadcastSyncInput(input);
		}
		break;
	case 'H': // Toggle hexadecimal pixel values.
		mHexMode = !mHexMode;
		mRgbFormat = mHexMode ? mRgbHex : mRgbDec;
		BroadcastDisplayOptions();
		break;
	case 'Y': // Show only the luma channel.
		mYMode = !mYMode;
		BroadcastDisplayOptions();
		break;
	case 'V': // Toggle source-native YUV values for raw YUV input.
		mShowSourceYuv = !mShowSourceYuv;
		BroadcastDisplayOptions();
		break;
	case 'I':
		mInterpol = !mInterpol;
		BroadcastDisplayOptions();
		break;
	case 'R':
		pDoc->Rotate90();
		{
			ViewerSyncInputState input = {};
			input.command = VIEWER_SYNC_ROTATE;
			pMainFrm->BroadcastSyncInput(input);
		}
		goto OnKeyDefault;
	case 'C':
		mShowCoord = !mShowCoord;
		BroadcastDisplayOptions();
		break;
	case 'B':
		mShowBoxInfo = !mShowBoxInfo;
		BroadcastDisplayOptions();
		break;
	case 'N':
		pDoc->NextCs();
		{
			ViewerSyncInputState input = {};
			input.command = VIEWER_SYNC_COLOR_SPACE;
			input.first = pDoc->mColorSpace;
			pMainFrm->BroadcastSyncInput(input);
		}
		break;
	}

	// Most shortcuts affect shared document/view state, so refresh once at the end.
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

	// First lifecycle point where the view can safely reach its document.
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

	mXDst = q1::DeterminDestPos(mWCanvas, mWDst, mXOff, mN);
	mYDst = q1::DeterminDestPos(mHCanvas, mHDst, mYOff, mN);

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
	switch (QMouseMenu[idx].id) {
	case QMouseMenuSel:
		ToggleSelMode();
		break;
	case QMouseMenuSyncInput: {
		CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
		pMainFrm->ToggleSyncInput();
		UINT nFlags = MF_BYCOMMAND |
			(pMainFrm->IsSyncInputEnabled() ? MF_CHECKED : MF_UNCHECKED);
		mMouseMenu.CheckMenuItem(nID, nFlags);
		break;
	}
	default:
		QASSERT(0);
	}
}
