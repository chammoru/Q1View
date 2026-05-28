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

#include <gdiplus.h>

// CComparerView

CComparerView::CComparerView()
: mXDst(0)
, mYDst(0)
, mWClient(0)
, mHClient(0)
, mWCanvas(0)
, mHCanvas(0)
, mIsClicked(false)
, mProcessing(false)
, mShowHelp(false)
, mRgbBufSize(0)
, mRgbBuf(NULL)
, mPixelTextFontHeight(0)
, mDiffCellPen(nullptr)
, mDiffDotBrush(nullptr)
{
	LOGFONT lf;

	::ZeroMemory(&lf, sizeof(lf));

	lf.lfWeight = FW_NORMAL;
	::lstrcpy(lf.lfFaceName, Q1UI_FONT_MONO);
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

	if (mMemDC.GetSafeHdc())
		mMemDC.DeleteDC();
	mMemBitmap.DeleteObject();

	delete mDiffCellPen;
	delete mDiffDotBrush;
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

void CComparerView::OnInitialUpdate()
{
	CScrollView::OnInitialUpdate();

	CSize sizeTotal;
	sizeTotal.cx = sizeTotal.cy = MIN_SIDE;
	SetScrollSizes(MM_TEXT, sizeTotal);
}

void CComparerView::ScaleNearestNeighbor(CComparerDoc *pDoc, BYTE *src, BYTE *dst, int sDst,
										  q1::GridInfo &gi)
{
	long gap, yStart, yEnd, xStart, xEnd;

	if (mYDst > 0 || mXDst > 0) // The image is smaller than the canvas.
		memset(dst, 0xf7, sDst * mHClient * QIMG_DST_RGB_BYTES);

	// Visible range of the scaled image on the canvas.
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

void CComparerView::EnsureNnOffsetBuf(CComparerDoc *pDoc)
{
	int maxL = QMAX(pDoc->mWDst, pDoc->mHDst);
	if (pDoc->mPreN == pDoc->mN && pDoc->mPreMaxL >= maxL)
		return;

	if (pDoc->mNnOffsetBufSize < maxL) {
		if (pDoc->mNnOffsetBuf)
			_mm_free(pDoc->mNnOffsetBuf);

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

void CComparerView::EnsurePixelTextFont(float n)
{
	LONG h = LONG(n * 4 / 15);
	if (h == mPixelTextFontHeight && mPixelTextFont.GetSafeHandle())
		return;

	if (mPixelTextFont.GetSafeHandle())
		mPixelTextFont.DeleteObject();

	LOGFONT lf;
	mDefPixelTextFont.GetLogFont(&lf);
	lf.lfHeight = h;
	mPixelTextFont.CreateFontIndirect(&lf);
	mPixelTextFontHeight = h;
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

	EnsureNnOffsetBuf(pDoc);

	ScaleNearestNeighbor(pDoc, rgbBuffer, mRgbBuf, sDst, gi);

	BITMAPINFOHEADER &bmiHeader = pDoc->mBmi.bmiHeader;
	bmiHeader.biWidth = sDst;
	bmiHeader.biHeight = -mHClient;
}

void CComparerView::OnDraw(CDC *pDC)
{
	CComparerDoc *pDoc = GetDocument();

	ComparerPane *pane = GetPane(pDoc);
	if (!pane->isAvail()) {
		DrawEmptyPane(pDC, pDoc);
		return;
	}

	if (!mMemDC.GetSafeHdc())
		return;

	if (pDoc->mWDst < mWCanvas || pDoc->mHDst < mHCanvas)
		mMemDC.FillSolidRect(CRect(0, 0, mWCanvas, mHCanvas), Q1UI_COLOR_CANVAS_BG);

	if (pDoc->mN > ZOOM_TEXT_START) {
		// At this zoom level each source pixel becomes a large display cell.
		// Draw each cell directly (dimmed 1px border + solid interior + label)
		// instead of synthesizing a full canvas-sized DIB and blitting it.
		DrawHighZoomCells(pDoc, pane);
	} else {
		q1::GridInfo gi;
		ScaleRgbBuf(pDoc, pane->rgbBuf, gi);
#ifdef USE_STRETCH_DIB
		StretchDIBits(mMemDC.m_hDC,
			mXDst, mYDst, pDoc->mWDst, pDoc->mHDst,
			0, 0, pDoc->mW, pDoc->mH,
			pane->rgbBuf, &pDoc->mBmi, DIB_RGB_COLORS, SRCCOPY); // main image
#else
		SetDIBitsToDevice(mMemDC.m_hDC,
			0, 0, mWClient, mHClient,
			0, 0, 0, mHClient,
			mRgbBuf, &pDoc->mBmi, DIB_RGB_COLORS);
#endif
	}

	DrawDiffOverlay(&mMemDC, pDoc, pane);

	if (pDoc->mShowCursorCoord && pDoc->mCursorView == this)
		DrawCursorCoord(&mMemDC, pDoc);

	if (mShowHelp)
		DrawHelpMenu(&mMemDC);

	pDC->BitBlt(0, mRcControls.bottom, mWCanvas, mHCanvas, &mMemDC, 0, 0, SRCCOPY);

	mProcessing = false;
}

void CComparerView::ToggleHelp()
{
	mShowHelp = !mShowHelp;
	Invalidate(FALSE);
}

void CComparerView::DrawHelpMenu(CDC *pDC)
{
	const int W_HELP = 460;
	const int H_HELP = 280;
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
	mDefPixelTextFont.GetLogFont(&lf);
	lf.lfHeight = 14;
	lf.lfWeight = FW_NORMAL;
	manualFont.CreateFontIndirect(&lf);
	pDC->SetBkMode(TRANSPARENT);
	CFont *prevFont = pDC->SelectObject(&manualFont);
	pDC->SetTextColor(Q1UI_COLOR_TEXT);
	CString manual(
		"Comparer shortcuts\n"
		"\n"
		"?              Show or hide this panel\n"
		"Drag && Drop    Open a source into a pane\n"
		"Mouse Wheel    Zoom in or out; high zoom shows pixel values\n"
		"Left/Right     Previous or next video frame\n"
		"Space          Play or pause\n"
		"H              Toggle hex pixel values\n"
		"I              Interpolate pixels\n"
		"D              Toggle pink diff overlay (grid + dots)\n"
		"C              Toggle cursor pixel coordinates\n"
		"Click timeline Pick a video frame (left/right pane)\n"
		);
	pDC->DrawText(manual, &manualRect, DT_LEFT | DT_TOP);
	pDC->SelectObject(prevFont);
}

// Pilot: draw a semi-transparent pink cell outline + center dot in every
// grid cell that contains at least one pixel that differs from the reference
// pane. Cell size is fixed in display pixels, so zooming in implicitly
// subdivides the source region each cell covers — at maximum zoom each dot
// resolves to a single differing source pixel.
void CComparerView::DrawDiffOverlay(CDC *pDC, CComparerDoc *pDoc, ComparerPane *pane)
{
	if (!pDoc->mDiffOverlay)
		return;
	// At high zoom the per-pixel value labels already convey the diff
	// information; the grid and dots would just clutter the view.
	if (pDoc->mN > ZOOM_TEXT_START)
		return;
	if (!pane || !pane->isAvail() || !pane->rgbBuf)
		return;

	// Find any other available pane to compare against (pilot assumes 2 panes).
	ComparerPane *other = nullptr;
	for (int i = 0; i < CComparerDoc::IMG_VIEW_MAX; i++) {
		ComparerPane *p = &pDoc->mPane[i];
		if (p == pane)
			continue;
		if (p->isAvail() && p->rgbBuf) {
			other = p;
			break;
		}
	}
	if (!other || pDoc->mW <= 0 || pDoc->mH <= 0)
		return;

	const int rowBytes = ROUNDUP_DWORD(pDoc->mW) * QIMG_DST_RGB_BYTES;
	const BYTE *bufA = pane->rgbBuf;
	const BYTE *bufB = other->rgbBuf;

	const int cellPx = 48;   // grid cell size in display pixels
	const int dotR   = 3;    // center dot radius

	if (!mDiffCellPen) {
		const Gdiplus::Color cellColor(200, 0xff, 0x3d, 0x8a);
		const Gdiplus::Color dotColor (235, 0xff, 0x3d, 0x8a);
		mDiffCellPen = new Gdiplus::Pen(cellColor, 1.0f);
		mDiffCellPen->SetAlignment(Gdiplus::PenAlignmentInset);
		mDiffDotBrush = new Gdiplus::SolidBrush(dotColor);
	}

	Gdiplus::Graphics g(pDC->m_hDC);
	g.SetCompositingMode(Gdiplus::CompositingModeSourceOver);

	const float invN = (pDoc->mN > 0.0f) ? (1.0f / pDoc->mN) : 1.0f;

	for (int cy = 0; cy < mHCanvas; cy += cellPx) {
		int cellBot = cy + cellPx;
		if (cellBot > mHCanvas) cellBot = mHCanvas;

		int sy0 = (int)floorf((cy      - mYDst) * invN);
		int sy1 = (int)ceilf ((cellBot - mYDst) * invN);
		if (sy0 < 0)         sy0 = 0;
		if (sy1 > pDoc->mH)  sy1 = pDoc->mH;
		if (sy0 >= sy1)      continue;

		for (int cx = 0; cx < mWCanvas; cx += cellPx) {
			int cellRight = cx + cellPx;
			if (cellRight > mWCanvas) cellRight = mWCanvas;

			int sx0 = (int)floorf((cx        - mXDst) * invN);
			int sx1 = (int)ceilf ((cellRight - mXDst) * invN);
			if (sx0 < 0)        sx0 = 0;
			if (sx1 > pDoc->mW) sx1 = pDoc->mW;
			if (sx0 >= sx1)     continue;

			bool hasDiff = false;
			const int byteStart = sx0 * QIMG_DST_RGB_BYTES;
			const int byteLen   = (sx1 - sx0) * QIMG_DST_RGB_BYTES;
			for (int sy = sy0; sy < sy1; sy++) {
				const BYTE *ra = bufA + sy * rowBytes + byteStart;
				const BYTE *rb = bufB + sy * rowBytes + byteStart;
				if (memcmp(ra, rb, byteLen) != 0) {
					hasDiff = true;
					break;
				}
			}

			if (hasDiff) {
				g.DrawRectangle(mDiffCellPen,
					(float)cx, (float)cy,
					(float)(cellRight - cx - 1), (float)(cellBot - cy - 1));

				float dotX = (cx + cellRight) * 0.5f;
				float dotY = (cy + cellBot)   * 0.5f;
				g.FillEllipse(mDiffDotBrush,
					dotX - dotR, dotY - dotR, 2.0f * dotR, 2.0f * dotR);
			}
		}
	}
}

// Mirrors the dimmed-border + solid-interior pattern that
// q1::ScaleUsingOffset produces via nOffsetBorderFlag, but without going
// through the full canvas-sized RGB buffer + SetDIBitsToDevice. Each visible
// source pixel is drawn as one cell rectangle, so the work is O(visible
// cells) instead of O(canvas pixels).
void CComparerView::DrawHighZoomCells(CComparerDoc *pDoc, ComparerPane *pane)
{
	if (!pane || !pane->rgbBuf)
		return;

	EnsureNnOffsetBuf(pDoc);

	// Visible region in scaled-image coordinates. This intentionally matches
	// ScaleNearestNeighbor's computation so InvestigatePixelBorder receives
	// identical inputs.
	long yStart, yEnd, xStart, xEnd;
	if (mYDst > 0) {
		yStart = 0;
		yEnd = pDoc->mHDst;
	} else {
		yStart = -mYDst;
		yEnd = mHClient - mYDst - mRcControls.bottom;
	}
	if (mXDst > 0) {
		xStart = 0;
		xEnd = pDoc->mWDst;
	} else {
		xStart = -mXDst;
		xEnd = mWClient - mXDst;
	}

	long yBase = mYDst > 0 ? mYDst : 0;
	long xBase = mXDst > 0 ? mXDst : 0;

	q1::GridInfo gi;
	q1::InvestigatePixelBorder(pDoc->mNnOffsetBuf, yStart, yEnd, yBase, pDoc->mHDst,
		&gi.y, &gi.Hs, pDoc->mNnOffsetYBorderFlag);
	q1::InvestigatePixelBorder(pDoc->mNnOffsetBuf, xStart, xEnd, xBase, pDoc->mWDst,
		&gi.x, &gi.Ws, pDoc->mNnOffsetXBorderFlag);

	if (gi.Hs.empty() || gi.Ws.empty())
		return;

	EnsurePixelTextFont(pDoc->mN);

	CFont *prevFont = mMemDC.SelectObject(&mPixelTextFont);
	int prevBkMode = mMemDC.SetBkMode(OPAQUE);
	COLORREF prevTextColor = mMemDC.SetTextColor(COLOR_PIXEL_TEXT);

	// Measured once with the cached font; reused for every cell.
	CRect calcRect;
	mMemDC.DrawText(_T("000\n000\n000"), -1, calcRect, DT_CENTER | DT_CALCRECT);
	int textHeight = calcRect.Height();

	const int srcStride = ROUNDUP_DWORD(pDoc->mW);
	const qu16 *nOffset = pDoc->mNnOffsetBuf;

	int y = gi.y;
	int srcY = yStart;
	for (size_t i = 0; i < gi.Hs.size(); i++) {
		int cellH = gi.Hs[i];
		const BYTE *src_y = pane->rgbBuf + nOffset[srcY] * srcStride;

		int x = gi.x;
		int srcX = xStart;
		for (size_t j = 0; j < gi.Ws.size(); j++) {
			int cellW = gi.Ws[j];
			const BYTE *src_x = src_y + nOffset[srcX];
			BYTE b = src_x[0];
			BYTE g = src_x[1];
			BYTE r = src_x[2];

			COLORREF solid = RGB(r, g, b);
			COLORREF dim   = RGB((0x80 + r) >> 1, (0x80 + g) >> 1, (0x80 + b) >> 1);

			// Dim the full cell first; the interior is then overwritten with
			// the true pixel color, leaving the 1px border dim. This matches
			// the GIRD_SIDE_START / GRID_SIDE_END branch in ScaleUsingOffset.
			mMemDC.FillSolidRect(x, y, cellW, cellH, dim);
			if (cellW > 2 && cellH > 2)
				mMemDC.FillSolidRect(x + 1, y + 1, cellW - 2, cellH - 2, solid);

			CString label;
			label.Format(pDoc->mRgbFormat, r, g, b);
			mMemDC.SetBkColor(dim);
			// Same off-by-one rect as the original loop: width cellW-1,
			// bottom y+cellH-1 -- leaves the rightmost column and bottom
			// row untouched so the dim border on those sides is preserved
			// even when OPAQUE text bg would otherwise repaint them.
			CRect textRect(x, y + (cellH - textHeight) / 2,
				x + cellW - 1, y + cellH - 1);
			mMemDC.DrawText(label, &textRect, DT_CENTER | DT_VCENTER);

			x += cellW;
			srcX += cellW;
		}
		y += cellH;
		srcY += cellH;
	}

	mMemDC.SetBkMode(prevBkMode);
	mMemDC.SetTextColor(prevTextColor);
	mMemDC.SelectObject(prevFont);
}

void CComparerView::UpdateCursorCoord(const CPoint &clientPoint)
{
	CComparerDoc *pDoc = GetDocument();
	if (!pDoc || pDoc->mW <= 0 || pDoc->mH <= 0)
		return;

	// clientPoint is in window-client space; the canvas starts at y =
	// mRcControls.bottom (the controls strip sits above it).
	int canvasX = clientPoint.x;
	int canvasY = clientPoint.y - mRcControls.bottom;

	int xCoord = int((canvasX - mXDst) / pDoc->mN);
	int yCoord = int((canvasY - mYDst) / pDoc->mN);

	// Hide the overlay whenever the cursor is over the controls strip or off
	// the image itself; otherwise the displayed coord would be misleading
	// (negative or past mW/mH).
	bool outside = (canvasY < 0) ||
		(xCoord < 0) || (xCoord >= pDoc->mW) ||
		(yCoord < 0) || (yCoord >= pDoc->mH);

	if (outside) {
		if (pDoc->mCursorView == this) {
			pDoc->mCursorView = NULL;
			if (pDoc->mShowCursorCoord)
				Invalidate(FALSE);
		}
		return;
	}

	if (pDoc->mCursorView == this &&
		pDoc->mCursorX == xCoord && pDoc->mCursorY == yCoord) {
		return;
	}

	CComparerView *prev = pDoc->mCursorView;
	pDoc->mCursorView = this;
	pDoc->mCursorX = xCoord;
	pDoc->mCursorY = yCoord;

	if (pDoc->mShowCursorCoord) {
		if (prev != NULL && prev != this)
			prev->Invalidate(FALSE);
		Invalidate(FALSE);
	}
}

void CComparerView::DrawCursorCoord(CDC *pDC, CComparerDoc *pDoc)
{
	LOGFONT lf;
	mDefPixelTextFont.GetLogFont(&lf);
	lf.lfHeight = 16;
	lf.lfWeight = FW_NORMAL;
	CFont coordFont;
	coordFont.CreateFontIndirect(&lf);

	CFont *prevFont = pDC->SelectObject(&coordFont);

	CString coord;
	coord.Format(_T("x:%d,y:%d"), pDoc->mCursorX, pDoc->mCursorY);

	CRect refRect;
	pDC->DrawText(coord, -1, refRect, DT_CENTER | DT_VCENTER | DT_CALCRECT);
	CRect bgRect(mWCanvas - refRect.Width(), mHCanvas - refRect.Height(),
		mWCanvas, mHCanvas);
	bgRect.InflateRect(8, 2, 0, 0);
	pDC->FillSolidRect(bgRect, Q1UI_COLOR_OVERLAY);

	int prevBkMode = pDC->SetBkMode(TRANSPARENT);
	COLORREF prevTextColor = pDC->SetTextColor(Q1UI_COLOR_OVERLAY_TEXT);
	pDC->DrawText(coord, &bgRect, DT_CENTER | DT_VCENTER);
	pDC->SetTextColor(prevTextColor);
	pDC->SetBkMode(prevBkMode);
	pDC->SelectObject(prevFont);
}

void CComparerView::DrawEmptyPane(CDC *pDC, CComparerDoc *pDoc)
{
	CRect canvas(0, mRcControls.bottom, mWClient, mHClient);
	pDC->FillSolidRect(canvas, Q1UI_COLOR_CANVAS_BG);

	LOGFONT lf;
	mDefPixelTextFont.GetLogFont(&lf);
	::lstrcpy(lf.lfFaceName, Q1UI_FONT_TEXT);

	CFont titleFont;
	lf.lfHeight = 18;
	lf.lfWeight = FW_SEMIBOLD;
	titleFont.CreateFontIndirect(&lf);

	CFont bodyFont;
	lf.lfHeight = 13;
	lf.lfWeight = FW_NORMAL;
	bodyFont.CreateFontIndirect(&lf);

	CString title(_T("Drop a frame source"));
	CString body(_T("Use 2-4 panes to compare images, raw dumps, or video frames"));

	CRect textRect = canvas;
	textRect.DeflateRect(12, 12);
	CRect titleRect = textRect;
	titleRect.bottom = canvas.CenterPoint().y - 3;
	CRect bodyRect = textRect;
	bodyRect.top = canvas.CenterPoint().y + 6;

	pDC->SetBkMode(TRANSPARENT);
	CFont *prevFont = pDC->SelectObject(&titleFont);
	pDC->SetTextColor(Q1UI_COLOR_TEXT);
	pDC->DrawText(title, &titleRect, DT_SINGLELINE | DT_CENTER | DT_BOTTOM | DT_END_ELLIPSIS);
	pDC->SelectObject(&bodyFont);
	pDC->SetTextColor(Q1UI_COLOR_TEXT_MUTED);
	pDC->DrawText(body, &bodyRect, DT_SINGLELINE | DT_CENTER | DT_TOP | DT_END_ELLIPSIS);
	pDC->SelectObject(prevFont);
}


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


int CComparerView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CScrollView::OnCreate(lpCreateStruct) == -1)
		return -1;

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
	UINT uDragCount = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
	if (uDragCount <= 0) {
		CScrollView::OnDropFiles(hDropInfo);
		return;
	}

	std::vector<CString> filenames(uDragCount);

	for (UINT i = 0; i < uDragCount; i++) {
		TCHAR szPathName[MAX_PATH];
		DragQueryFile(hDropInfo, i, szPathName, MAX_PATH);
		filenames[i] = szPathName;
	}

	CComparerDoc *pDoc = GetDocument();

	if (uDragCount == 1) {
		ComparerPane* pane = GetPane(pDoc);
		const int previousW = pDoc->mW;
		const int previousH = pDoc->mH;

		pane->pathName = filenames[0];
		pDoc->ProcessDocument(mPane);

		CPosInfoView* posInfoView = pDoc->mPosInfoView;
		posInfoView->ConfigureScrollSizes(pDoc);

		CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
		pMainFrm->UpdateResolutionLabel(pDoc->mW, pDoc->mH);
		pMainFrm->CheckResolutionRadio(pDoc->mW, pDoc->mH);

		AdjustWindowSize(pMainFrm->mNumOfViews);
		if (previousW != pDoc->mW || previousH != pDoc->mH)
			pDoc->ResetViewToFit();
		pDoc->UpdateAllViews(NULL);
	} else {
		pDoc->OpenMultiFiles(filenames);
	}

	CScrollView::OnDropFiles(hDropInfo);
}

BOOL CComparerView::OnEraseBkgnd(CDC* pDC)
{
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

	// Preserve the current frame overhead while resizing for the active panes.
	int wGap = mainRcClient.Width() - viewRcClient.Width() * numPrevViews;
	int hGap = mainRcClient.Height() - viewRcClient.Height();

	int wViewClient = MAX(CANVAS_DEF_W, pDoc->mW);
	int hViewClient = MAX(CANVAS_DEF_H, pDoc->mH) + mRcControls.bottom;

	int wMainWindow = wViewClient * pMainFrm->mNumOfViews + wGap + splitBarChange;
	int hMainWindow = hViewClient + hGap;

	if (wMainWindow >= fullScnSz.cx || hMainWindow >= fullScnSz.cy) {
		if (!pMainFrm->IsZoomed()) {
			pMainFrm->SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE, 
				(LPARAM)pMainFrm->GetSafeHwnd());
		}

		return;
	} else if (pMainFrm->IsZoomed()) {
		// Restore first so resizing is based on normal frame geometry.
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
	// OnSize has already updated the client and canvas dimensions.

	DeterminDestOriginCoord(pDoc);
}

BOOL CComparerView::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
{
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

	// Reposition cursor coord at the new zoom; clientPoint already excludes
	// the controls strip, so add it back so UpdateCursorCoord can subtract.
	CPoint cursorPoint(clientPoint.x, clientPoint.y + mRcControls.bottom);
	UpdateCursorCoord(cursorPoint);

	pDoc->UpdateAllViews(NULL);

OnMouseWheelDefault:
	return CScrollView::OnMouseWheel(nFlags, zDelta, pt);
}

void CComparerView::OnSize(UINT nType, int cx, int cy)
{
	CScrollView::OnSize(nType, cx, cy);

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

	if (mWCanvas > 0 && mHCanvas > 0) {
		CClientDC dc(this);
		if (mMemDC.GetSafeHdc())
			mMemDC.DeleteDC();
		mMemBitmap.DeleteObject();
		mMemDC.CreateCompatibleDC(&dc);
		mMemBitmap.CreateCompatibleBitmap(&dc, mWCanvas, mHCanvas);
		mMemDC.SelectObject(&mMemBitmap);
		mMemDC.SetStretchBltMode(COLORONCOLOR);
	}
}

void CComparerView::OnMouseMove(UINT nFlags, CPoint point)
{
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

	UpdateCursorCoord(point);

OnMouseMoveDefault:
	CScrollView::OnMouseMove(nFlags, point);
}

void CComparerView::OnLButtonDown(UINT nFlags, CPoint point)
{
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
	mIsClicked = false;

	::SetCursor(AfxGetApp()->LoadStandardCursor(IDC_ARROW));

	CScrollView::OnLButtonUp(nFlags, point);

	::ReleaseCapture();
}

BOOL CComparerView::OnSetCursor(CWnd* pWnd, UINT nHitTest, UINT message)
{
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
		// Recompute timeline geometry after frame counts are refreshed.
		CPosInfoView* posInfoView = pDoc->mPosInfoView;
		posInfoView->ConfigureScrollSizes(pDoc);
		pDoc->UpdateAllViews(NULL);
	}
}

void CComparerView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	CComparerDoc* pDoc = GetDocument();

	bool isProcessing = pDoc->CheckImgViewProcessing();
	if (nChar != VK_SPACE && isProcessing)
		return;

	switch (nChar) {
	case VK_RIGHT:
		pDoc->OffsetScenes(1);
		break;
	case VK_LEFT:
		pDoc->OffsetScenes(-1);
		break;
	case VK_SPACE:
		pDoc->TogglePlay();
		break;
	case 'H': // Toggle hexadecimal pixel values.
		pDoc->mHexMode = !pDoc->mHexMode;
		pDoc->mRgbFormat = pDoc->mHexMode ? pDoc->mRgbHex : pDoc->mRgbDec;
		Invalidate(FALSE);
		break;
	case 'I':
		pDoc->mInterpol = !pDoc->mInterpol;
		Invalidate(FALSE);
		break;
	case 'D':
		pDoc->mDiffOverlay = !pDoc->mDiffOverlay;
		break;
	case 'C':
		pDoc->mShowCursorCoord = !pDoc->mShowCursorCoord;
		// Repaint all panes: the active one needs the overlay drawn (or
		// cleared); the others may have been left tracking a stale cursor.
		for (auto view : GetOhterViews(pDoc))
			view->Invalidate(FALSE);
		Invalidate(FALSE);
		break;
	case VK_OEM_2: // '?' / '/' on US keyboards.
		ToggleHelp();
		break;
	}

	// Most shortcuts affect shared document/view state, so refresh once at the end.
	pDoc->MarkImgViewProcessing();
	pDoc->UpdateAllViews(NULL);

	CScrollView::OnKeyDown(nChar, nRepCnt, nFlags);
}

std::vector<CComparerView *> CComparerView::GetOhterViews(CComparerDoc* pDoc)
{
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	std::vector<CComparerView *> otherViews;

	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
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
