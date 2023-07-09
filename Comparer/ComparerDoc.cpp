
// ComparerDoc.cpp : implementation of the CComparerDoc class
//

#include "stdafx.h"
#include "Comparer.h"

#include "ComparerDoc.h"
#include "ComparerView.h"
#include "PosInfoView.h"
#include "MainFrm.h"
#include "FileScanThread.h"
#include "YuvFrmCmpStrategy.h"
#include "RgbFrmCmpStrategy.h"
#include "MetricCal.h"
#include "QViewerCmn.h"
#include "ComparerUtil.h"

#include "QImageStr.h"

#include "QViewerCmn.h"
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
, mInterpol(false)
, mDiffRes(false)
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

	q1::image_sort_cs(mSortedCscInfo);
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

	pane->OpenFrmSrc(mSortedCscInfo, mW, mH);

	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	double fps = pane->GetFps();
	if (fps > 0) {
		mFps = fps;
		pMainFrm->UpdateFpsLabel(fps);
		pMainFrm->CheckFpsRadio(fps);
		pMainFrm->DrawMenuBar();
	}

	setDstSize();

	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
		CComparerView *pView = mPane[i].pView;
		pView->Initialize(this);
	}

	int stride = ROUNDUP_DWORD(mW);
	size_t rgbBufSize = stride * mH * ((int)QIMG_DST_RGB_BYTES);
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

void CComparerDoc::RefleshPaneImages(ComparerPane *pane, bool settingChanged)
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	pMainFrm->KillTimer(CTI_ID_POS_INVALIDATE);
	KillPlayTimer();

	LoadSourceImage(pane);

	ComparerPane *opposite = GetOppositePane(pane);
	if (opposite->isAvail()) {
		if (settingChanged)
			LoadSourceImage(opposite);

		// This routine should be located after checking settingChanged
		// And should be before mFileScanThread->setup()
		mMaxFrames = max(pane->frames, opposite->frames);
		mMinFrames = min(pane->frames, opposite->frames);

		bool rgbCompare = IsRGBCompare(pane, opposite);
		if (rgbCompare)
			mFrmCmpStrategy = mRgbCompare;
		else
			mFrmCmpStrategy = mYuvCompare;

		mFrmCmpStrategy->Setup(mW, mH);
		mFrmCmpStrategy->CalMetrics(pane, opposite, pMainFrm->mMetricIdx, mFrmState);

		mFileScanThread->requestExitAndWait();
		mFileScanThread->setup(); // uses mMinFrames and frmCmpInfo is updated

		MetricCal *psnrCal = mFrmsInfoView->mPsnrCal;
		const FrmCmpInfo *frmCmpInfo = mFileScanThread->getFrmCmpInfo();
		psnrCal->Update(frmCmpInfo, mMinFrames); // uses mMinFrames and frmCmpInfo

		mFileScanThread->run();

		pMainFrm->SetTimer(CTI_ID_POS_INVALIDATE, VIT_INVALIDATE_DUR, NULL);
	} else {
		mMaxFrames = max(pane->frames, opposite->frames);
		mMinFrames = 0;
	}
}

void CComparerDoc::ProcessDocument(ComparerPane *pane)
{
	int srcW = 0, srcH = 0;
	bool success = pane->GetResolution(pane->pathName , &srcW, &srcH);
	if (!success) {
		LOGWRN("FrmSrc failed to get resolution info");
		return;
	}
	pane->srcW = srcW;
	pane->srcH = srcH;

	bool settingChanged = false;
	if (!mDiffRes && (mW != srcW || mH != srcH)) {
		LOGINF("new width (%d) and height (%d)", srcW, srcH);
		for (auto other : GetOtherPanes(pane)) {
			if (!other->pathName.IsEmpty()) {
				CComparerView* otherView = other->pView;
				if (otherView != nullptr) {
					otherView->UpdateFileName(_T(""));
				}

				other->Release();
			}
		}

		mW = srcW;
		mH = srcH;

		settingChanged = true;
	}

	bool doReisze = srcW != mW || srcH != mH;
	CComparerView* pView = pane->pView;
	const struct qcsc_info* ci = pane->GetColorSpace(pane->pathName, mSortedCscInfo, doReisze);
	if (ci == NULL) {
		LOGWRN("color space is not found");
	} else {
		// Since I just changed the menu title and id when It clicked,
		// Whole new setting might be necessary
		pane->SetColorInfo(ci);

		CString str = CA2W(ci->name);
		pView->UpdateCsLabel(str.MakeUpper());
		pView->CheckCsRadio(ci->cs);
	}

	CString fileName = getBaseName(pane->pathName);
	pView->UpdateFileName(fileName);

	RefleshPaneImages(pane, settingChanged);
}

void CComparerDoc::ViewOnMouseWheel(short zDelta, int wCanvas, int hCanvas)
{
	if (mN > ZOOM_MAX && zDelta > 0)
		return;

	if (mN <= 1 && zDelta < 0) {
		mN = q1::GetFitRatio(mN, mW, mH, wCanvas, hCanvas);
	} else {
		float d = mD + zDelta / WHEEL_DELTA;
		float fitN = q1::GetBestFitRatio(mW, mH, wCanvas, hCanvas);
		mN = q1::GetNextN(mN, fitN, d);
	}
	mD = ZOOM_DELTA(mN);

	setDstSize();

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	pMainFrm->UpdateMagnication(mN);
}

struct ImageProperty
{
	int width, height, index;
	ImageProperty(int w, int h, int i) :
		width(w), height(h), index(i) {}
};

struct ImagePropertyCompare
{
	bool operator()(const ImageProperty &l, const ImageProperty &r) {
		return l.width * l.height > r.width * r.height;
	}
};

BOOL CComparerDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	if (pMainFrm == NULL) {
		// take care of the open operation in CMainFrame::ActivateFrame()
		mPendingFile = lpszPathName;
		::CoInitialize(NULL); // TODO: Is this really right solution?
		return TRUE;
	}

	CString filenamesCsv = lpszPathName;
	std::vector<CString> filenames;
	int pos = 0;
	CString token = filenamesCsv.Tokenize(CSV_SEPARATOR, pos);

	while (!token.IsEmpty()) {
		filenames.push_back(token);
		if (!CDocument::OnOpenDocument(token)) {
			LOGWRN("Wrong file path : %s", CT2A(token).m_psz);
			return FALSE;
		}
		AfxGetApp()->AddToRecentFileList(token);
		token = filenamesCsv.Tokenize(CSV_SEPARATOR, pos);
	}

	int numOfFiles = (int) filenames.size();
	if (numOfFiles > pMainFrm->mNumOfViews) {
		pMainFrm->ChangeNumOfViews(numOfFiles);
	}

	// Clear the views that show previous images
	for (int i = numOfFiles; i < pMainFrm->mNumOfViews; i++) {
		ComparerPane* other = mPane + IMG_VIEW_1 + i;
		if (!other->pathName.IsEmpty()) {
			CComparerView* otherView = other->pView;
			if (otherView != nullptr) {
				otherView->UpdateFileName(_T(""));
			}

			other->Release();
		}
	}

	// Check all resolutions
	vector<ImageProperty> imageProperties;
	int prevSrcW = 0, prevSrcH = 0;
	bool isDiffRes = false;
	for (int i = 0; i < numOfFiles; i++) {
		ComparerPane* pane = mPane + IMG_VIEW_1 + i;
		pane->pathName = filenames[i];
		int srcW = 0, srcH = 0;
		bool success = pane->GetResolution(pane->pathName, &srcW, &srcH);
		if (!success) {
			LOGWRN("FrmSrc failed to get resolution info for file idx %d", i);
			return FALSE;
		}
		imageProperties.emplace_back(srcW, srcH, i);
		if (prevSrcW == 0 && prevSrcH == 0) {
			prevSrcW = srcW;
			prevSrcH = srcH;
		} else if (prevSrcW != srcW || prevSrcH != srcH) {
			isDiffRes = true;
		}
	}
	stable_sort(imageProperties.begin(), imageProperties.end(), ImagePropertyCompare());

	// Turn on "Allow Different Resolution" mode if resolutions are different
	mDiffRes = isDiffRes;
	pMainFrm->CheckOptionsRadio(ID_OPTIONS_DIFF_RESOLUTION, mDiffRes);
	if (mDiffRes) {
		mW = imageProperties[0].width;
		mH = imageProperties[0].height;
	}

	// Show the file in filenames to the i-th view
	for (const auto &imageProperty : imageProperties) {
		int index = imageProperty.index;
		ComparerPane* pane = mPane + IMG_VIEW_1 + index;
		pane->pathName = filenames[index];
		ProcessDocument(pane);
	}

	mPosInfoView->ConfigureScrollSizes(this);

	pMainFrm->UpdateResolutionLabel(mW, mH); // also, disable the resolution change if necessary
	pMainFrm->CheckResolutionRadio(mW, mH);

	const CComparerView* firstView = mPane->pView;
	if (firstView != nullptr) {
		firstView->AdjustWindowSize(pMainFrm->mNumOfViews);
	}

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
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	bool updated = false;

	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
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
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	bool updated = false;

	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
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

inline std::vector<ComparerPane*> CComparerDoc::GetOtherPanes(ComparerPane* pane)
{
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	std::vector<ComparerPane*> otherPanes;
	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
		if (mPane + i != pane)
			otherPanes.push_back(mPane + i);
	}

	return otherPanes;
}

void CComparerDoc::KillPlayTimer()
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());

	pMainFrm->KillTimer(CTI_ID_PLAY);
	mIsPlaying = false;
}

void CComparerDoc::MarkImgViewProcessing()
{
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
		ComparerPane *pane = &mPane[i];

		if (!pane->isAvail())
			continue;

		CComparerView *view = pane->pView;
		view->mProcessing = true;
	}
}

bool CComparerDoc::CheckImgViewProcessing()
{
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	bool isProcessing = false;
	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
		ComparerPane *pane = &mPane[i];
		CComparerView *view = pane->pView;
		if (view->mProcessing)
			isProcessing= true;
	}

	return isProcessing;
}

bool CComparerDoc::isFixedResolution()
{
	for (int i = 0; i < CComparerDoc::IMG_VIEW_MAX; i++) {
		ComparerPane* pane = &mPane[i];
		if (!pane)
			continue;
		FrmSrc* frmSrc = pane->frmSrc;
		if (frmSrc && frmSrc->isFixedResolution())
			return true;
	}

	return false;
}
