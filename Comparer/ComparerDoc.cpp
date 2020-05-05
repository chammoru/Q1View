
// ComparerDoc.cpp : implementation of the CComparerDoc class
//

#include "stdafx.h"
#include "Comparer.h"

#include "ComparerDoc.h"
#include "ComparerViewC.h"
#include "PosInfoView.h"
#include "MainFrm.h"
#include "FileScanThread.h"
#include "YuvFrmCmpStrategy.h"
#include "RgbFrmCmpStrategy.h"
#include "MetricCal.h"
#include "QImageViewerCmn.h"

#include "QImageStr.h"

#include "QImageViewerCmn.h"
#include "QDebug.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// #include "vld.h"

// CComparerDoc

IMPLEMENT_DYNCREATE(CComparerDoc, CDocument)

BEGIN_MESSAGE_MAP(CComparerDoc, CDocument)
END_MESSAGE_MAP()


// CComparerDoc construction/destruction

CComparerDoc::CComparerDoc()
: mPane(new ComparerPane[IMG_VIEW_MAX])
, mW(CANVAS_DEF_W), mH(CANVAS_DEF_H)
, mWDst(mW), mHDst(mH)
, mXOff(.0f), mYOff(.0f)
, mD(0.f)
, mN(ZOOM_RATIO(mD))
, mFrmState("")
, mMaxFrames(0)
, mMinFrames(0)
, mFrmCmpStrategy(NULL)
, mYuvCompare(new YuvFrmCmpStrategy())
, mRgbCompare(new RgbFrmCmpStrategy())
, mIsPlaying(false)
, mPreN(0.0f)
, mPreMaxL(0)
, mNnOffsetBufSize(0)
, mNnOffsetBuf(NULL)
, mNnOffsetYBorderFlag(NULL)
, mNnOffsetXBorderFlag(NULL)
, mHexMode(false)
, mRgbHex(_T("%02X\n%02X\n%02X"))
, mRgbDec(_T("%03d\n%03d\n%03d"))
, mRgbFormat(mRgbDec)
, mFps(COMPARER_DEF_FPS)
{
	// TODO: add one-time construction code here
	BITMAPINFOHEADER &bmiHeader = mBmi.bmiHeader;
	bmiHeader.biSize = (DWORD)sizeof(BITMAPINFOHEADER);
	bmiHeader.biPlanes = 1;
	bmiHeader.biBitCount = 24;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = 0L;
	bmiHeader.biXPelsPerMeter = 0L;
	bmiHeader.biYPelsPerMeter = 0L;

	mFileScanThread = new FileScanThread(this);

	mSortedCscInfo =
		static_cast<struct qcsc_info *>(malloc(sizeof(qcsc_info_table)));
	ASSERT(mSortedCscInfo != NULL);

	qimage_sort_cs(mSortedCscInfo);
}

CComparerDoc::~CComparerDoc()
{
	mFileScanThread->requestExitAndWait();

	delete [] mPane;

	delete mFileScanThread;
	delete mRgbCompare;
	delete mYuvCompare;
	delete mSortedCscInfo;

	if (mNnOffsetBuf)
		_mm_free(mNnOffsetBuf);

	if (mNnOffsetYBorderFlag)
		_mm_free(mNnOffsetYBorderFlag);

	if (mNnOffsetXBorderFlag)
		_mm_free(mNnOffsetXBorderFlag);
}

BOOL CComparerDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)

	return TRUE;
}




// CComparerDoc serialization

void CComparerDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}


// CComparerDoc diagnostics

#ifdef _DEBUG
void CComparerDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CComparerDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// CComparerDoc commands

bool CComparerDoc::ReadSource4View(ComparerPane *pane)
{
	if (!pane->isAvail()) {
		LOGWRN("Pane is not available");
		return false;
	}

	long nextFrameID = pane->GetNextFrameID();
	if (nextFrameID >= pane->frames) {
		LOGWRN("Invalid frameID(%d) for %s", nextFrameID, CT2A(pane->pathName).m_psz);
		return false;
	}

	if (!pane->FillSceneBuf(pane->origBuf))
		return false;

	BYTE *y = pane->origBuf;
	BYTE *u = pane->origBuf + pane->bufOffset2;
	BYTE *v = pane->origBuf + pane->bufOffset3;
	BYTE *RGB = pane->rgbBuf;

	pane->csc2rgb888(RGB, y, u, v, ROUNDUP_DWORD(mW), mW, mH);

	return true;
}

void CComparerDoc::setDstSize()
{
	if (mN < 1.f) {
		mWDst = int(mW * mN);
		mHDst = int(mH * mN);
	} else {
		mWDst = CEIL2I(mW * mN);
		mHDst = CEIL2I(mH * mN);
	}
}

void CComparerDoc::LoadSourceImage(ComparerPane *pane)
{
	pane->origSceneSize = pane->csLoadInfo(mW, mH, &pane->bufOffset2, &pane->bufOffset3);
	if (pane->origBufSize < pane->origSceneSize) {
		if (pane->origBuf)
			delete [] pane->origBuf;

		pane->origBuf = new BYTE[pane->origSceneSize];
		pane->origBufSize = pane->origSceneSize;
	}

	if (pane->csc2yuv420) {
		int bufOffset2 = 0, bufOffset3 = 0;

		size_t yuv420SceneSize =
			qimage_yuv420_load_info(mW, mH, &bufOffset2, &bufOffset3);

		if (pane->yuv420BufSize < yuv420SceneSize) {
			if (pane->yuv420Buf)
				delete [] pane->yuv420Buf;

			pane->yuv420Buf = new BYTE[yuv420SceneSize];
			pane->yuv420BufSize = yuv420SceneSize;
		}
	}

	pane->OpenFrmSrc();

	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	double fps = pane->GetFps();
	if (fps > 0) {
		mFps = fps;
		pMainFrm->UpdateFpsLabel(fps);
		pMainFrm->CheckFpsRadio(fps);
		pMainFrm->DrawMenuBar();
	}

	setDstSize();

	for (int i = 0; i < IMG_VIEW_MAX; i++) {
		CComparerViewC *pView = mPane[i].pView;

		pView->Initialize(this);
	}

	int stride = ROUNDUP_DWORD(mW);
	size_t rgbBufSize = stride * mH * QIMG_DST_RGB_BYTES;
	if (rgbBufSize > pane->rgbBufSize) {
		pane->rgbBufSize = rgbBufSize;

		if (pane->rgbBuf)
			delete [] pane->rgbBuf;

		pane->rgbBuf = new BYTE[rgbBufSize];
	}

	BITMAPINFO *pBmi = &mBmi;
	BITMAPINFOHEADER &bmiHeader = pBmi->bmiHeader;
	bmiHeader.biWidth = stride;
	bmiHeader.biHeight = -mH;

	ReadSource4View(pane);

	pMainFrm->UpdateMagnication(mN);
}

bool CComparerDoc::IsRGBCompare(const SQPane *paneA,
								const SQPane *paneB) const
{
	if (!(paneA->colorSpace & QIMAGE_CS_YUV) ||
		!(paneB->colorSpace & QIMAGE_CS_YUV) ||
		(paneA->colorSpace != QIMAGE_CS_YUV420 && paneA->csc2yuv420 == NULL) ||
		(paneB->colorSpace != QIMAGE_CS_YUV420 && paneB->csc2yuv420 == NULL)) {
		return true;
	} else {
		return false;
	}
}

#ifdef MORU_FMAT_HW
#include "QImageEpipol.h"
#endif

void CComparerDoc::RefleshPaneImages(ComparerPane *paneA, bool settingChanged)
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	pMainFrm->KillTimer(CTI_ID_POS_INVALIDATE);
	KillPlayTimer();

	LoadSourceImage(paneA);

	ComparerPane *paneB = GetOppositePane(paneA);
	if (paneB->isAvail()) {
		if (settingChanged)
			LoadSourceImage(paneB);

		// This routine should be located after checking settingChanged
		// And should be before mFileScanThread->setup()
		mMaxFrames = max(paneA->frames, paneB->frames);
		mMinFrames = min(paneA->frames, paneB->frames);

#ifdef MORU_FMAT_HW
		if (paneA->isOcvMat() && paneB->isOcvMat()) {
			if (mPane + IMG_VIEW_R == paneB)
				mFundamental = qGetFundamentalMat(paneA->mOcvMat, paneB->mOcvMat);
			else
				mFundamental = qGetFundamentalMat(paneB->mOcvMat, paneA->mOcvMat);
		}
#endif

		bool rgbCompare = IsRGBCompare(paneA, paneB);
		if (rgbCompare)
			mFrmCmpStrategy = mRgbCompare;
		else
			mFrmCmpStrategy = mYuvCompare;

		mFrmCmpStrategy->Setup(mW, mH);
		mFrmCmpStrategy->CalMetrics(paneA, paneB, mFrmState);

		mFileScanThread->requestExitAndWait();
		mFileScanThread->setup(); // uses mMinFrames and frmCmpInfo is updated

		MetricCal *psnrCal = mFrmsInfoView->mPsnrCal;
		const FrmCmpInfo *frmCmpInfo = mFileScanThread->getFrmCmpInfo();
		psnrCal->Update(frmCmpInfo, mMinFrames); // uses mMinFrames and frmCmpInfo

		mFileScanThread->run();

		pMainFrm->SetTimer(CTI_ID_POS_INVALIDATE, VIT_INVALIDATE_DUR, NULL);
	} else {
		mMaxFrames = max(paneA->frames, paneB->frames);
		mMinFrames = 0;
	}

	// This routine should be located after mMaxFrames set
	mPosInfoView->ConfigureScrollSizes(this);
}

void CComparerDoc::ProcessDocument(ComparerPane *pane)
{
	int w = 0, h = 0;
	bool success = pane->GetResolution(pane->pathName , &w, &h);
	if (!success) {
		LOGWRN("FrmSrc failed to get resolution info");
		return;
	}

	bool settingChanged = false;
	if (mW != w || mH != h) {
		LOGINF("new width (%d) and height (%d)", w, h);
		ComparerPane* other = GetOppositePane(pane);
		if (!other->pathName.IsEmpty()) {
			other->Release();
		}

		mW = w;
		mH = h;

		settingChanged = true;

		CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
		pMainFrm->UpdateResolutionLabel(mW, mH);
		pMainFrm->CheckResolutionRadio(mW, mH);
	}

	const struct qcsc_info* ci = pane->GetColorSpace(pane->pathName, mSortedCscInfo);
	if (ci == NULL) {
		LOGWRN("color space is not found");
	} else {
		// Since I just changed the menu title and id when It clicked,
		// Whole new setting might be necessary
		pane->SetColorInfo(ci);

		CString str = CA2W(ci->name);
		CComparerViewC *pView = pane->pView;

		pView->UpdateCsLabel(str.MakeUpper());
		pView->CheckCsRadio(ci->cs);
	}

	RefleshPaneImages(pane, settingChanged);
}

void CComparerDoc::ViewOnMouseWheel(short zDelta, int wCanvas, int hCanvas)
{
	if (mN > ZOOM_MAX && zDelta > 0)
		return;

	if (mN <= 1 && zDelta < 0) {
		mN = QGetFitRatio(mN, mW, mH, wCanvas, hCanvas);
	} else {
		float d = mD + zDelta / WHEEL_DELTA;
		float fitN = QGetBestFitRatio(mW, mH, wCanvas, hCanvas);
		mN = QGetNextN(mN, fitN, d);
	}
	mD = ZOOM_DELTA(mN);

	setDstSize();

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	pMainFrm->UpdateMagnication(mN);
}

BOOL CComparerDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	if (!CDocument::OnOpenDocument(lpszPathName))
		return FALSE;

	// TODO:  Add your specialized creation code here
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	if (pMainFrm == NULL) {
		// take care of the open operation in CMainFrame::ActivateFrame()
		mPendingFile = lpszPathName;
		::CoInitialize(NULL); // TODO: Is this really right solution?
		return TRUE;
	}

	// Currently update only the left pane
	ComparerPane *paneL = mPane + IMG_VIEW_L;

	if (paneL->pathName == lpszPathName)
		return TRUE;

	paneL->pathName = lpszPathName;

	CComparerViewC *pView = paneL->pView;
	pView->ProcessDocument(this);
	pView->AdjustWindowSize();
	UpdateAllViews(NULL);

	return TRUE;
}

void CComparerDoc::SetScene(long frameID, ComparerPane* pane, bool& updated)
{
	if (frameID < 0) {
		pane->SetNextFrameID(0);
	} else if (frameID < pane->frames) {
		pane->SetNextFrameID(frameID);
		updated = true;
	} else {
		pane->SetNextFrameID(pane->frames - 1);
	}

	ReadSource4View(pane);
}

bool CComparerDoc::OffsetScenes(long offset)
{
	bool updated = false;

	for (int i = 0; i < IMG_VIEW_MAX; i++) {
		ComparerPane *pane = &mPane[i];
		if (!pane->isAvail())
			continue;

		SetScene(pane->curFrameID + offset, pane, updated);
	}

	return updated;
}

bool CComparerDoc::SetScenes(long frameID)
{
	bool updated = false;

	for (int i = 0; i < IMG_VIEW_MAX; i++) {
		ComparerPane *pane = &mPane[i];
		if (!pane->isAvail())
			continue;

		SetScene(frameID, pane, updated);
	}

	return updated;
}

bool CComparerDoc::NextScenes()
{
	bool updated = false;

	for (int i = 0; i < IMG_VIEW_MAX; i++) {
		ComparerPane* pane = &mPane[i];

		if (!pane->isAvail())
			continue;

		long before = pane->curFrameID;
		if (pane->curFrameID < pane->frames - 1)
			ReadSource4View(pane);
		long after = pane->curFrameID;
		if (before != after)
			updated = true;
	}

	return updated;
}

void CComparerDoc::KillPlayTimer()
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());

	pMainFrm->KillTimer(CTI_ID_PLAY);
	mIsPlaying = false;
}

void CComparerDoc::MarkImgViewProcessing()
{
	for (int i = 0; i < IMG_VIEW_MAX; i++) {
		ComparerPane *pane = &mPane[i];

		if (!pane->isAvail())
			continue;

		CComparerViewC *view = pane->pView;
		view->mProcessing = true;
	}
}

bool CComparerDoc::CheckImgViewProcessing()
{
	bool isProcessing = false;
	for (int i = 0; i < IMG_VIEW_MAX; i++) {
		ComparerPane *pane = &mPane[i];
		CComparerViewC *view = pane->pView;
		if (view->mProcessing)
			isProcessing= true;
	}

	return isProcessing;
}
