// ViewerView.h : interface of the CViewerView class
//


#pragma once

struct ViewerSyncInputState;

#include <Mmsystem.h>

#include "SMutex.h"

#include <QViewerCmn.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#define PROGRESS_BAR_H        16
#define PROGRESS_FONT_H       13
#define PROGRESS_PIXEL_TEXT_H 14
#define MARGIN_PROGESS_BAR    3
#define COLOR_PROGRESS_BAR    Q1UI_COLOR_ACCENT
#define COLOR_PROGRESS_TEXT   Q1UI_COLOR_TEXT
#define COLOR_SELECTING_RECT  Q1UI_COLOR_WARNING
#define COLOR_SELECTED_RECT   Q1UI_COLOR_SUCCESS
#define COLOR_COORDINATE_RECT Q1UI_COLOR_OVERLAY
#define COLOR_BOXINFO_RECT    Q1UI_COLOR_OVERLAY

#define LENGTH_MIN_SELECT     8
#define INVAL                 INT_MIN

class QPoint : public CPoint
{
public:
	QPoint()
	{
		Init();
	}

	inline void Init()
	{
		SetPoint(INVAL, INVAL);
	}

	inline bool IsValid()
	{
		return x != INVAL && y != INVAL;
	}
};

struct QSelRegion
{
	inline void setBoxes()
	{
		l.SetRect(mSelRect.left - 2, mSelRect.top + 2, mSelRect.left + 3, mSelRect.bottom - 1);
		t.SetRect(mSelRect.left + 2, mSelRect.top - 2, mSelRect.right - 1, mSelRect.top + 3);
		r.SetRect(mSelRect.right - 2, mSelRect.top + 2, mSelRect.right + 3, mSelRect.bottom - 1);
		b.SetRect(mSelRect.left + 2, mSelRect.bottom - 2, mSelRect.right - 1, mSelRect.bottom + 3);
	}

	inline BOOL IsInL(CPoint pt) { return l.PtInRect(pt); }
	inline BOOL IsInT(CPoint pt) { return t.PtInRect(pt); }
	inline BOOL IsInR(CPoint pt) { return r.PtInRect(pt); }
	inline BOOL IsInB(CPoint pt) { return b.PtInRect(pt); }
	inline BOOL IsInLR(CPoint &pt) { return IsInL(pt) || IsInR(pt); }
	inline BOOL IsInTB(CPoint &pt) { return IsInT(pt) || IsInB(pt); }

	CRect mSelRect;
	CRect l, t, r, b;

	inline void SetRect(int x1, int y1, int x2, int y2)
	{
		mSelRect.SetRect(x1, y1, x2, y2);
		setBoxes();
	}

	inline void Init()
	{
		mSelRect.SetRect(INVAL, INVAL, INVAL, INVAL);
	}

	inline bool IsValid() const
	{
		return mSelRect.left != INVAL && mSelRect.right != INVAL &&
			   mSelRect.bottom != INVAL && mSelRect.top != INVAL;
	}
};

class CViewerView : public CView
{
protected: // create from serialization only
	CViewerView();
	DECLARE_DYNCREATE(CViewerView)

// Attributes
public:
	CViewerDoc* GetDocument() const;

	// Basic Image information
	int mW, mH;

	// Zoom & Move
	float mD, mN;
	QPoint mPointS, mPointE;
	bool mIsClicked;
	float mXOff, mYOff;
	float mXInitOff, mYInitOff;
	ULONGLONG mLastSyncViewStateTick;
	int mXDst, mYDst;
	int mWDst, mHDst;

public:
	int mWClient, mHClient;
	int mWCanvas, mHCanvas;
	int mXCursor, mYCursor;

	// Progressive
	int mHProgress;
	CFont mProgressFont, mDefPixelTextFont, mConsolasFont;
	COLORREF mBarColor;

	// RGB buffers used by SetDIBitsToDevice.
	BITMAPINFO  mBmi;
	BYTE *mRgbBuf, *mCaptRgbBuf, *mYBuf;
	int mRgbBufSize, mCaptRgbBufSize, mYBufSize;
	qu16 *mNnOffsetBuf;
	qu8 *mNnOffsetYBorderFlag, *mNnOffsetXBorderFlag;
	int mNnOffsetBufSize;
	float mPreN;
	int mPreMaxL;

	// Play
	bool mIsPlaying;
	ULONGLONG mPreKeyFrameStamp;
	int mPlayFrameCount;
	TIMECAPS mTc;
	UINT mTimerID;
	LARGE_INTEGER mPlaybackClockFrequency;
	LARGE_INTEGER mPlaybackStartCounter;
	long mPlaybackStartFrameID;
	long mDroppedFrameCount;
	double mPlaybackRate;
	bool mPlaybackEndPending;
	volatile LONG mPlayTickPosted;

	// Frame queues shared with the playback worker.
	SSafeCQ<BufferInfo> *mBufferQueue;
	SSafeCQ<BufferInfo> *mNewRgbBufferInfoQ;
	BufferInfo 	mStableRgbBufferInfo;
	SBufferPool *mBufferPool;
	bool mKeyProcessing;

	CRect mRcProgress;
	CMenu mMouseMenu;

	bool mSelMode;
	bool mYMode;
	bool mInterpol;
	bool mFullMode;
	bool mShowHelp;
	bool mShowCoord;
	bool mShowBoxInfo;
	bool mWasZoomed;
	bool mHexMode;
	bool mShowSourceYuv;
	const CString mRgbHex;
	const CString mRgbDec;
	CString mRgbFormat;
	std::vector<QSelRegion> mSelRegions;
	CRect mDeltaRect;
	bool mNewSel;
	int mDeltaIdx;

// Operations
public:
	void AdjustWindowSize();
	void ProgressiveDraw(CDC *pDC, CViewerDoc* pDoc, int frameID);
	void SetPlayTimer(CViewerDoc* pDoc);
	void KillPlayTimer();
	void KillPlayTimerSafe();
	void PrintPlaySpeed(double fps);
	void ScaleRgbBuf(BYTE *rgbBuffer, BYTE **rgbDst, q1::GridInfo &gi);
	void SetCursorCoordinates(const CPoint &pt);
	void Initialize(int nFrame, size_t rgbStride, int w, int h, bool preserveViewState = false);
	void OnMouseMenu(UINT nID);
	void DrawSelectRect(CDC *pDC);
	cv::Mat GetRoiMat();
	void ChangeZoom(short zDelta, CPoint &pt);
	cv::Mat CreateRoiMat(int x0, int y0, int w, int h);
	void ToggleFullScreen();
	void DrawPixelText(CDC *pDC, q1::GridInfo &gi);
	void DrawPixelValueMode(CDC *pDC);
	void ToggleHelp();
	void DrawHelpMenu(CDC *pDC);
	void DrawCursorCoordinates(CDC *pDC);
	void DrawBoxInfo(CDC *pDC);
	void DrawEmptyState(CDC *pDC);
	void ToggleSelMode();
	void ApplySyncInput(const ViewerSyncInputState &input);

// Helper
private:
	CRect CvtCoord2Show(const CRect &rt);
	bool FindFile(CViewerDoc* pDoc, UINT nChar);
	bool HandleNavigationKey(UINT nChar);
	UINT GetDisplayOptions() const;
	void BroadcastDisplayOptions();
	void BroadcastViewState(bool force = false);
	void ApplyViewState(float zoom, float xOff, float yOff);
	void ApplyPlaybackState(bool play);
	double GetEffectivePlaybackFps(const CViewerDoc* pDoc) const;
	long GetDuePlaybackFrameID(const CViewerDoc* pDoc) const;
	void SetDstSize();
	void _ScaleRgb(BYTE *src, BYTE *dst, int sDst, q1::GridInfo &gi);
	int DrawBoxInfoText(CDC *pDC, CRect &rect, COLORREF color, int hAccumGap);

// Overrides
public:
	virtual void OnDraw(CDC* pDC);  // overridden to draw this view
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
protected:

// Implementation
public:
	virtual ~CViewerView();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
public:

	afx_msg void OnFileOpen();
	afx_msg BOOL OnEraseBkgnd(CDC* pDC);
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnDestroy();
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg LRESULT OnPlayTimer(WPARAM wParam, LPARAM lParam);
};

#ifndef _DEBUG  // debug version in ViewerView.cpp
inline CViewerDoc* CViewerView::GetDocument() const
	{ return reinterpret_cast<CViewerDoc*>(m_pDocument); }
#endif

