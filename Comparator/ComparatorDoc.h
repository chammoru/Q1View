
// ComparatorDoc.h : interface of the CComparatorDoc class
//


#pragma once

#include "afx.h"

#include "ComparatorPane.h"

#define CANVAS_DEF_W          352
#define CANVAS_DEF_H          288
#define CONTROLS_H_GUESSED    21  // QMENUITEM_IN_MARGIN_H + alpha, in CComparatorView::OnCreate()
#define PANE_DEF_W            CANVAS_DEF_W
#define PANE_DEF_H            (CANVAS_DEF_H + CONTROLS_H_GUESSED)
#define MIN_SIDE              10
#define COMPARER_DEF_FPS      30.0
#define FPS_ADJUSTMENT        7
#define COMPARER_DEF_VIEWS    2
#define COMPARING_VIEWS_NUM   2

class CComparatorDoc;
class FileScanThread;
class LpipsScanThread;
struct IFrmCmpStrategy;
class YuvFrmCmpStrategy;
class RgbFrmCmpStrategy;
struct SQPane;
class CComparatorView;
class CPosInfoView;
class CFrmsInfoView;
class CFrmInfoView;

class CComparatorDoc : public CDocument
{
protected: // create from serialization only
	CComparatorDoc();
	DECLARE_DYNCREATE(CComparatorDoc)

// Attributes
public:
	enum PANE_ID
	{
		IMG_VIEW_1,
		IMG_VIEW_2,
		IMG_VIEW_3,
		IMG_VIEW_4,
		IMG_VIEW_MAX,
	};

	ComparatorPane *mPane;

	float mD, mN;
	int mW, mH;
	int mWDst, mHDst;
	float mXInitOff, mYInitOff;
	float mXOff, mYOff;

	struct qcsc_info *mSortedCscInfo;
	BITMAPINFO mBmi;

	CString mFrmState;
	CPosInfoView *mPosInfoView;
	CFrmsInfoView *mFrmsInfoView;
	CFrmInfoView *mFrmInfoView;
	int mMaxFrames, mMinFrames;
	FileScanThread *mFileScanThread;
	LpipsScanThread *mLpipsScanThread;
	class FileChangeNotiThread *mFileChangeNotiThreads[IMG_VIEW_MAX];
	IFrmCmpStrategy *mFrmCmpStrategy;
	YuvFrmCmpStrategy *mYuvCompare;
	RgbFrmCmpStrategy *mRgbCompare;
	CString mPendingFile;
	bool mIsPlaying;
	float mPreN;
	int mPreMaxL;
	int mNnOffsetBufSize;
	qu16 *mNnOffsetBuf;
	qu8 *mNnOffsetYBorderFlag, *mNnOffsetXBorderFlag;
	bool mHexMode;
	const CString mRgbHex;
	const CString mRgbDec;
	CString mRgbFormat;
	double mFps;
	bool mInterpol;
	bool mDiffRes;
	bool mDiffOverlay;
	bool mShowCursorCoord;
	CComparatorView *mCursorView;
	int mCursorX, mCursorY;

// Operations
public:
	void ProcessDocument(ComparatorPane *pane);
	void LoadSourceImage(ComparatorPane *pane);
	bool ReadSource4View(ComparatorPane *pane);
	void ReloadPane(int paneIdx);
	bool IsRGBCompare(const SQPane *paneA, const SQPane *paneB) const;
	void RefleshPaneImages(ComparatorPane *pane, bool settingChanged);
	inline ComparatorPane* GetOppositePane(ComparatorPane* pane) {
		if (pane == mPane + IMG_VIEW_1) {
			return mPane + IMG_VIEW_2;
		} else if (pane == mPane + IMG_VIEW_2) {
			return mPane + IMG_VIEW_1;
		} else {
			return nullptr;
		}
	}
	std::vector<ComparatorPane*> GetOtherPanes(ComparatorPane* pane);
	void KillPlayTimer();
	void TogglePlay();
	bool OffsetScenes(long offset);
	void MarkImgViewProcessing();
	bool CheckImgViewProcessing();
	bool SetScenes(long frameID);
	void SetScene(long frameID, ComparatorPane* pane, bool& updated);
	bool NextScenes();
	void setDstSize();
	void ResetViewToFit();
	bool isFixedResolution();
	BOOL OpenMultiFiles(const std::vector<CString>& filenames);
	void SelectMetric(int metricIdx);
	void UpdateCurrentMetricState(int metricIdx);
	bool IsLpipsScanRunning() const;
	void StopLpipsScanThread();

// Overrides
public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);

// Implementation
public:
	virtual ~CComparatorDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
public:
	void ViewOnMouseWheel(short zDelta, int wCanvas, int hCanvas);
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
};

