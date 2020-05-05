
// ComparerDoc.h : interface of the CComparerDoc class
//


#pragma once

#include "afx.h"

#include "ComparerPane.h"

#define CANVAS_DEF_W          352
#define CANVAS_DEF_H          288
#define GUESS_CONTROLS_H      18
#define PANE_DEF_W            CANVAS_DEF_W
#define PANE_DEF_H            (CANVAS_DEF_H + GUESS_CONTROLS_H)
#define MIN_SIDE              10
#define COMPARER_DEF_FPS      30.0
#define FPS_ADJUSTMENT        7

class CComparerDoc;
class FileScanThread;
struct IFrmCmpStrategy;
class YuvFrmCmpStrategy;
class RgbFrmCmpStrategy;
struct SQPane;
class CPosInfoView;
class CFrmsInfoView;

class CComparerDoc : public CDocument
{
protected: // create from serialization only
	CComparerDoc();
	DECLARE_DYNCREATE(CComparerDoc)

// Attributes
public:
	enum PANE_INDEX
	{
		IMG_VIEW_R = 0,
		IMG_VIEW_L = 1,
		IMG_VIEW_MAX,
	};

	ComparerPane *mPane;

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
	int mMaxFrames, mMinFrames;
	FileScanThread *mFileScanThread;
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

#ifdef MORU_FMAT_HW
	cv::Mat mFundamental;
#endif

// Operations
public:
	void ProcessDocument(ComparerPane *pane);
	void LoadSourceImage(ComparerPane *pane);
	bool ReadSource4View(ComparerPane *pane);
	bool IsRGBCompare(const SQPane *paneA, const SQPane *paneB) const;
	void RefleshPaneImages(ComparerPane *paneA, bool settingChanged);
	inline ComparerPane* GetOppositePane(ComparerPane* pane)
	{
		return pane == mPane + IMG_VIEW_L ? mPane + IMG_VIEW_R : mPane + IMG_VIEW_L;
	}
	void KillPlayTimer();
	bool OffsetScenes(long offset);
	void MarkImgViewProcessing();
	bool CheckImgViewProcessing();
	bool SetScenes(long frameID);
	void SetScene(long frameID, ComparerPane* pane, bool& updated);
	bool NextScenes();
	void setDstSize();

// Overrides
public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);

// Implementation
public:
	virtual ~CComparerDoc();
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

