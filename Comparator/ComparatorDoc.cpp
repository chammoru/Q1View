
// ComparatorDoc.cpp : implementation of the CComparatorDoc class
//

#include "stdafx.h"
#include "Comparator.h"

#include "ComparatorDoc.h"
#include "ComparatorView.h"
#include "PosInfoView.h"
#include "MainFrm.h"
#include "FileScanThread.h"
#include "YuvFrmCmpStrategy.h"
#include "RgbFrmCmpStrategy.h"
#include "MetricCal.h"
#include "QViewerCmn.h"
#include "ComparatorUtil.h"
#include "FileChangeNotiThread.h"
#include "FrmInfoView.h"
#include "LpipsScanThread.h"
#include "LpipsEngine.h"

#include "QImageStr.h"

#include "QViewerCmn.h"
#include "QDebug.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// #include "vld.h"

// CComparatorDoc

IMPLEMENT_DYNCREATE(CComparatorDoc, CDocument)

BEGIN_MESSAGE_MAP(CComparatorDoc, CDocument)
END_MESSAGE_MAP()


CComparatorDoc::CComparatorDoc()
: mPane(new ComparatorPane[IMG_VIEW_MAX])
, mW(CANVAS_DEF_W), mH(CANVAS_DEF_H)
, mWDst(mW), mHDst(mH)
, mXOff(.0f), mYOff(.0f)
, mD(0.f)
, mN(ZOOM_RATIO(mD))
, mFrmState("")
, mPosInfoView(NULL)
, mFrmsInfoView(NULL)
, mFrmInfoView(NULL)
, mMaxFrames(0)
, mMinFrames(0)
, mLpipsScanThread(nullptr)
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
, mDiffOverlay(false)
, mShowCursorCoord(false)
, mCursorView(NULL)
, mCursorX(0)
, mCursorY(0)
, mSelMode(false)
, mSelecting(false)
, mHasSelection(false)
, mSelStart(0, 0)
, mSelCur(0, 0)
{
	BITMAPINFOHEADER &bmiHeader = mBmi.bmiHeader;
	bmiHeader.biSize = (DWORD)sizeof(BITMAPINFOHEADER);
	bmiHeader.biPlanes = 1;
	bmiHeader.biBitCount = 24;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = 0L;
	bmiHeader.biXPelsPerMeter = 0L;
	bmiHeader.biYPelsPerMeter = 0L;

	mFileScanThread = new FileScanThread(this);
	for (int i = 0; i < IMG_VIEW_MAX; i++)
		mFileChangeNotiThreads[i] = new FileChangeNotiThread();

	mSortedCscInfo =
		static_cast<struct qcsc_info *>(malloc(sizeof(qcsc_info_table)));
	ASSERT(mSortedCscInfo != NULL);

	q1::image_sort_cs(mSortedCscInfo);
}

CComparatorDoc::~CComparatorDoc()
{
	StopLpipsScanThread();

	mFileScanThread->requestExitAndWait();

	for (int i = 0; i < IMG_VIEW_MAX; i++) {
		delete mFileChangeNotiThreads[i];
		mFileChangeNotiThreads[i] = NULL;
	}

	delete [] mPane;

	delete mFileScanThread;
	delete mRgbCompare;
	delete mYuvCompare;
	free(mSortedCscInfo);

	if (mNnOffsetBuf)
		_mm_free(mNnOffsetBuf);

	if (mNnOffsetYBorderFlag)
		_mm_free(mNnOffsetYBorderFlag);

	if (mNnOffsetXBorderFlag)
		_mm_free(mNnOffsetXBorderFlag);
}

BOOL CComparatorDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	return TRUE;
}




// CComparatorDoc serialization

void CComparatorDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
	}
	else
	{
	}
}


#ifdef _DEBUG
void CComparatorDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CComparatorDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// CComparatorDoc commands

bool CComparatorDoc::ReadSource4View(ComparatorPane *pane)
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
	pane->rgbValid = true;

	return true;
}

void CComparatorDoc::setDstSize()
{
	if (mN < 1.f) {
		mWDst = int(mW * mN);
		mHDst = int(mH * mN);
	} else {
		mWDst = CEIL2I(mW * mN);
		mHDst = CEIL2I(mH * mN);
	}
}

void CComparatorDoc::ResetViewToFit()
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	CComparatorView *firstView = mPane[IMG_VIEW_1].pView;
	if (pMainFrm == NULL || firstView == NULL ||
		firstView->mWCanvas <= 0 || firstView->mHCanvas <= 0)
		return;

	mXOff = 0.0f;
	mYOff = 0.0f;
	mN = q1::GetBestFitRatio(mW, mH, firstView->mWCanvas, firstView->mHCanvas);
	mD = ZOOM_DELTA(mN);

	setDstSize();
	for (int i = 0; i < pMainFrm->mNumOfViews; i++)
		mPane[i].pView->Initialize(this);

	pMainFrm->UpdateMagnication(mN);
}

void CComparatorDoc::LoadSourceImage(ComparatorPane *pane)
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
		CComparatorView *pView = mPane[i].pView;
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

bool CComparatorDoc::IsRGBCompare(const SQPane *paneA,
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

void CComparatorDoc::RefleshPaneImages(ComparatorPane *pane, bool settingChanged)
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	pMainFrm->KillTimer(CTI_ID_POS_INVALIDATE);
	KillPlayTimer();
	StopAnalysis();

	LoadSourceImage(pane);
	if (settingChanged)
		for (auto other : GetOtherPanes(pane)) if (other->isAvail()) LoadSourceImage(other);
	if (!mBatchOpening) RebuildComparisons(settingChanged ? 0xf : 1u << (pane - mPane));
}

void CComparatorDoc::StopAnalysis()
{
	StopLpipsScanThread();
	mFileScanThread->requestExitAndWait();
	mFileScanThread->cancelCurrent();
	// Completed unchanged pairs remain visible. Cancelled/error work must be
	// resubmitted even when its displayed frame/ROI key has not changed.
	for (int i=0;i<3;++i)
		if (mPairResults[i].state!=_T("Ready")) mCurrentSignature[i].Empty();
}

std::vector<int> CComparatorDoc::TimelineSources() const
{
	std::vector<int> slots;
	for (int i = 0; i < 4; ++i) if (mPane[i].isAvail()) slots.push_back(i);
	return slots;
}

std::vector<int> CComparatorDoc::ComparisonTargets() const
{
	std::vector<int> slots;
	if (mPane[0].isAvail())
		for (int i = 1; i < 4; ++i) if (mPane[i].isAvail()) slots.push_back(i);
	return slots;
}

int CComparatorDoc::PairFrames(int target) const
{
	return target >= 1 && target <= 3 && mPane[0].isAvail() && mPane[target].isAvail()
		? (int)min(mPane[0].frames, mPane[target].frames) : 0;
}

IFrmCmpStrategy* CComparatorDoc::PairStrategy(int target) const
{
	return target >= 1 && target <= 3 ? mPairStrategies[target-1] : nullptr;
}

CString CComparatorDoc::PairTitle(int target) const
{
	CString title;
	title.Format(_T("A \x2194 %c"), _T('A') + target);
	return title;
}

void CComparatorDoc::SelectComparison(int target)
{
	if (!PairFrames(target)) return;
	mActiveTarget = target;
	UpdateAllViews(nullptr);
}

void CComparatorDoc::RebuildComparisons(unsigned changedSlots)
{
	StopAnalysis();
	mRequestedFrame=-1;
	mMaxFrames = mMinFrames = 0;
	for (int slot : TimelineSources()) mMaxFrames = max(mMaxFrames, (int)mPane[slot].frames);
	mRgbCompare->Setup(mW, mH); mYuvCompare->Setup(mW, mH);
	for (int t = 1; t < 4; ++t) {
		mPairStrategies[t-1] = PairFrames(t) ? (IsRGBCompare(mPane, mPane+t)
			? static_cast<IFrmCmpStrategy*>(mRgbCompare) : static_cast<IFrmCmpStrategy*>(mYuvCompare)) : nullptr;
		mMinFrames = max(mMinFrames, PairFrames(t));
		if (!PairFrames(t)) {
			mPairResults[t-1] = PairCurrentResult();
			mCurrentSignature[t-1].Empty(); mCurrentPairRequests[t-1]=0;
		}
	}
	mFrmCmpStrategy = PairStrategy(1); // compatibility: this alias always means A/B.
	auto targets = ComparisonTargets();
	if (!PairFrames(mActiveTarget)) mActiveTarget = targets.empty() ? 1 : targets.front();
	mFileScanThread->setup(changedSlots);
	if ((changedSlots&1) && mFrmsInfoView) mFrmsInfoView->ResetViewport();
	if (mPosInfoView) mPosInfoView->ConfigureScrollSizes(this);
	auto* frame = static_cast<CMainFrame*>(AfxGetMainWnd());
	SelectMetric(frame->mMetricIdx);
	CRect rc; frame->GetClientRect(rc);
	frame->SendMessage(WM_SIZE, SIZE_RESTORED, MAKELPARAM(rc.Width(), rc.Height()));
	UpdateAllViews(nullptr);
}

void CComparatorDoc::CloseSource(int slot)
{
	if (slot < 0 || slot > 3) return;
	KillPlayTimer(); StopAnalysis();
	mPane[slot].Release();
	if (mPane[slot].pView) mPane[slot].pView->UpdateFileName(_T(""));
	if (!mBatchOpening) RebuildComparisons(1u << slot);
}

void CComparatorDoc::ProcessDocument(ComparatorPane *pane)
{
	StopAnalysis();

	pane->pathName.Replace(_T('/'), _T('\\'));

	int srcW = 0, srcH = 0;
	bool success = pane->GetResolution(pane->pathName , &srcW, &srcH);
	if (!success) {
		LOGWRN("FrmSrc failed to get resolution info");
		CloseSource((int)(pane - mPane));
		return;
	}
	pane->srcW = srcW;
	pane->srcH = srcH;

	bool settingChanged = false;
	if (!mDiffRes && (mW != srcW || mH != srcH)) {
		LOGINF("new width (%d) and height (%d)", srcW, srcH);
		for (auto other : GetOtherPanes(pane)) {
			if (!other->pathName.IsEmpty()) {
				CComparatorView* otherView = other->pView;
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

	bool doResize = srcW != mW || srcH != mH;
	CComparatorView* pView = pane->pView;
	const struct qcsc_info* ci = pane->GetColorSpace(pane->pathName, mSortedCscInfo, doResize);
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

	// Start (or rebind) the file-change watcher for this pane so disk edits
	// trigger a refresh.
	int paneIdx = static_cast<int>(pane - mPane);
	if (paneIdx >= 0 && paneIdx < IMG_VIEW_MAX && mFileChangeNotiThreads[paneIdx]) {
		CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
		mFileChangeNotiThreads[paneIdx]->fire(pMainFrm, pane->pathName, paneIdx);
	}
}

void CComparatorDoc::ReloadPane(int paneIdx)
{
	if (paneIdx < 0 || paneIdx >= IMG_VIEW_MAX)
		return;
	ComparatorPane *pane = &mPane[paneIdx];
	if (!pane->isAvail() || pane->pathName.IsEmpty())
		return;

	// Re-read the current frame from disk. ProcessDocument handles a possible
	// resolution / color-space change after the file was rewritten.
	ProcessDocument(pane);

	RefreshSelectionMetric();

	UpdateAllViews(NULL);
}

void CComparatorDoc::ViewOnMouseWheel(short zDelta, int wCanvas, int hCanvas)
{
	if (mN > ZOOM_MAX && zDelta > 0)
		return;

	if (mN <= 1 && zDelta < 0) {
		mN = q1::GetFitRatio(mN, mW, mH, wCanvas, hCanvas);
	} else {
		float d = mD + float(zDelta) / WHEEL_DELTA;
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

static bool fileExists(const CString& path)
{
	DWORD attributes = ::GetFileAttributes(path);
	return attributes != INVALID_FILE_ATTRIBUTES &&
		(attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static std::vector<CString> parseFileList(const CString& fileList)
{
	std::vector<CString> filenames;

	int pos = 0;
	CString token = fileList.Tokenize(CSV_SEPARATOR, pos);
	while (!token.IsEmpty()) {
		filenames.push_back(token);
		token = fileList.Tokenize(CSV_SEPARATOR, pos);
	}

	return filenames;
}

static CString NormalizeInputPath(const CString& pathName)
{
	CString fullPath;
	DWORD capacity = MAX_PATH;
	for (;;) {
		std::vector<TCHAR> buffer(capacity);
		DWORD length = GetFullPathName(pathName, capacity, buffer.data(), NULL);
		if (length == 0)
			break;
		if (length < capacity) {
			fullPath.SetString(buffer.data(), static_cast<int>(length));
			break;
		}
		capacity = length + 1;
	}

	if (fullPath.IsEmpty())
		fullPath = pathName;

	fullPath.Replace(_T('/'), _T('\\'));
	return fullPath;
}

static CString NormalizeParentDirectory(const CString& pathName)
{
	const CString fullPath = NormalizeInputPath(pathName);
	int slash = fullPath.ReverseFind(_T('\\'));
	if (slash < 0)
		return fullPath;

	CString parent = fullPath.Left(slash);
	if (parent.IsEmpty())
		parent = _T("\\");
	parent.MakeLower();
	return parent;
}

BOOL CComparatorDoc::OpenMultiFiles(const std::vector<CString> &filenames) {
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());

	int numOfFiles = (int)filenames.size();
	if (!numOfFiles) return FALSE;
	if (numOfFiles > IMG_VIEW_MAX) {
		pMainFrm->MessageBox(
			_T("Comparator supports up to 4 files.\nPlease select 4 files or fewer."),
			_T("Too many files"),
			MB_ICONWARNING | MB_OK);
		return FALSE;
	}

	StopAnalysis();
	mBatchOpening = true;
	if (numOfFiles > pMainFrm->mNumOfViews) {
		pMainFrm->ChangeNumOfViews(numOfFiles);
	}

	// Clear the views that show previous images
	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
		ComparatorPane* other = mPane + IMG_VIEW_1 + i;
		if (!other->pathName.IsEmpty()) {
			CComparatorView* otherView = other->pView;
			if (otherView != nullptr) {
				otherView->UpdateFileName(_T(""));
			}

			other->Release();
		}
	}

	// Check all resolutions
	vector<ImageProperty> imageProperties;
	vector<int> srcWidths(numOfFiles);
	vector<int> srcHeights(numOfFiles);
	vector<bool> disableImageSequences(numOfFiles, false);
	vector<bool> isStillImage(numOfFiles, false);
	vector<CString> parentDirectories(numOfFiles);
	int prevSrcW = 0, prevSrcH = 0;
	bool isDiffRes = false;
	for (int i = 0; i < numOfFiles; i++) {
		ComparatorPane* pane = mPane + IMG_VIEW_1 + i;
		pane->pathName = filenames[i];
		pane->disableImageSequence = false;
		isStillImage[i] = MatFrmSrc::IsStillImagePath(filenames[i]);
		if (isStillImage[i])
			parentDirectories[i] = NormalizeParentDirectory(filenames[i]);
		AfxGetApp()->AddToRecentFileList(filenames[i]);
		int srcW = 0, srcH = 0;
		bool success = pane->GetResolution(pane->pathName, &srcW, &srcH);
		if (!success) {
			LOGWRN("FrmSrc failed to get resolution info for file idx %d", i);
			mBatchOpening = false;
			for (int slot = 0; slot < 4; ++slot) mPane[slot].Release();
			RebuildComparisons();
			return FALSE;
		}
		srcWidths[i] = srcW;
		srcHeights[i] = srcH;
		imageProperties.emplace_back(srcW, srcH, i);
		if (prevSrcW == 0 && prevSrcH == 0) {
			prevSrcW = srcW;
			prevSrcH = srcH;
		} else if (prevSrcW != srcW || prevSrcH != srcH) {
			isDiffRes = true;
		}
	}

	// Multiple still images selected from one directory are comparison inputs,
	// not independent anchors into the same image sequence. Decide this from
	// the selected paths so sequence detection never scans unselected files
	// merely to discover that the inputs belong to the same folder.
	for (int i = 0; i < numOfFiles; i++) {
		if (!isStillImage[i])
			continue;

		int sameDirectoryCount = 0;
		for (int j = 0; j < numOfFiles; j++) {
			if (isStillImage[j] &&
				parentDirectories[i].CompareNoCase(parentDirectories[j]) == 0)
				sameDirectoryCount++;
		}

		if (sameDirectoryCount >= 2)
			disableImageSequences[i] = true;
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
	for (const auto& imageProperty : imageProperties) {
		int index = imageProperty.index;
		ComparatorPane* pane = mPane + IMG_VIEW_1 + index;
		pane->pathName = filenames[index];
		pane->disableImageSequence = disableImageSequences[index];
		ProcessDocument(pane);
	}

	mBatchOpening = false;
	RebuildComparisons();

	pMainFrm->UpdateResolutionLabel(mW, mH); // also, disable the resolution change if necessary
	pMainFrm->CheckResolutionRadio(mW, mH);

	const CComparatorView* firstView = mPane->pView;
	if (firstView != nullptr) {
		firstView->AdjustWindowSize(pMainFrm->mNumOfViews);
	}

	ResetViewToFit();
	UpdateAllViews(NULL);

	return TRUE;
}

BOOL CComparatorDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	CString fileList = lpszPathName == NULL ? _T("") : lpszPathName;

	if (pMainFrm == NULL || !pMainFrm->IsWindowVisible()) {
		// A startup shell-open can run before the frame views have received
		// their initial layout. Delay loading until there is a drawable pane.
		mPendingFile = fileList;
		if (pMainFrm == NULL)
			::CoInitialize(NULL);
		else
			pMainFrm->PostMessage(WM_OPEN_PENDING_FILE);
		return TRUE;
	}

	std::vector<CString> filenames = parseFileList(fileList);
	if (filenames.empty())
		return FALSE;

	for (auto& filename : filenames) {
		// Native MRU/document bookkeeping and all source loaders receive the
		// same absolute path, including relative command-line input.
		filename = NormalizeInputPath(filename);
		if (!fileExists(filename)) {
			CString msg;
			msg.Format(_T("%s does not exist or is not accessible."), filename.GetString());
			AfxMessageBox(msg, MB_ICONWARNING | MB_OK);
			LOGWRN("Wrong file path : %s", CT2A(filename).m_psz);
			return FALSE;
		}
	}

	if (!filenames.empty())
		SetPathName(filenames[0], FALSE);

	return OpenMultiFiles(filenames);
}

void CComparatorDoc::SetScene(long frameID, ComparatorPane* pane, bool& updated)
{
	if (!pane->isAvail() || frameID < 0 || frameID >= pane->frames) return;
	if (frameID < 0) {
		pane->SetNextFrameID(0);
	} else if (frameID < pane->frames) {
		pane->SetNextFrameID(frameID);
		updated = true;
	} else {
		pane->SetNextFrameID(pane->frames - 1);
	}

	updated = ReadSource4View(pane) || updated;
}

bool CComparatorDoc::OffsetScenes(long offset)
{
	mRequestedFrame=-1;
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	bool updated = false;

	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
		ComparatorPane *pane = &mPane[i];
		if (!pane->isAvail())
			continue;

		SetScene(pane->curFrameID + offset, pane, updated);
	}

	RefreshSelectionMetric();
	return updated;
}

bool CComparatorDoc::SetScenes(long frameID)
{
	mRequestedFrame=frameID;
	bool updated = false;

	for (int i = 0; i < IMG_VIEW_MAX; i++) {
		ComparatorPane *pane = &mPane[i];
		if (!pane->isAvail())
			continue;

		SetScene(frameID, pane, updated);
	}

	RefreshSelectionMetric();
	return updated;
}

bool CComparatorDoc::NextScenes()
{
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	bool updated = false;

	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
		ComparatorPane* pane = &mPane[i];

		if (!pane->isAvail())
			continue;

		long before = pane->curFrameID;
		if (pane->curFrameID < pane->frames - 1)
			ReadSource4View(pane);
		long after = pane->curFrameID;
		if (before != after)
			updated = true;
	}

	RefreshSelectionMetric();
	return updated;
}

inline std::vector<ComparatorPane*> CComparatorDoc::GetOtherPanes(ComparatorPane* pane)
{
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	std::vector<ComparatorPane*> otherPanes;
	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
		if (mPane + i != pane)
			otherPanes.push_back(mPane + i);
	}

	return otherPanes;
}

void CComparatorDoc::KillPlayTimer()
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());

	pMainFrm->KillTimer(CTI_ID_PLAY);
	mIsPlaying = false;
}

void CComparatorDoc::TogglePlay()
{
	if (mIsPlaying) {
		KillPlayTimer();
		return;
	}

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	mIsPlaying = true;
	pMainFrm->SetTimer(CTI_ID_PLAY, ROUND2I((1000 / mFps) - FPS_ADJUSTMENT), NULL);
	if (!NextScenes())
		SetScenes(0);
}

void CComparatorDoc::MarkImgViewProcessing()
{
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
		ComparatorPane *pane = &mPane[i];

		if (!pane->isAvail())
			continue;

		CComparatorView *view = pane->pView;
		view->mProcessing = true;
	}
}

bool CComparatorDoc::CheckImgViewProcessing()
{
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	bool isProcessing = false;
	for (int i = 0; i < pMainFrm->mNumOfViews; i++) {
		ComparatorPane *pane = &mPane[i];
		CComparatorView *view = pane->pView;
		if (view->mProcessing)
			isProcessing = true;
	}

	return isProcessing;
}

bool CComparatorDoc::isFixedResolution()
{
	for (int i = 0; i < CComparatorDoc::IMG_VIEW_MAX; i++) {
		ComparatorPane* pane = &mPane[i];
		if (!pane)
			continue;
		FrmSrc* frmSrc = pane->frmSrc;
		if (frmSrc && frmSrc->isFixedResolution())
			return true;
	}

	return false;
}

bool CComparatorDoc::IsLpipsScanRunning() const
{
	return mLpipsScanThread != nullptr && mLpipsScanThread->isRunning();
}

void CComparatorDoc::StopLpipsScanThread()
{
	if (mLpipsScanThread) {
		mLpipsScanThread->requestExitAndWait();
		delete mLpipsScanThread;
		mLpipsScanThread = nullptr;
	}
}

void CComparatorDoc::SelectMetric(int metricIdx)
{
	CMainFrame* pMainFrm = static_cast<CMainFrame*>(AfxGetMainWnd());
	if (pMainFrm)
		pMainFrm->mMetricIdx = metricIdx;

	// Join both workers before switching a shared model/strategy. Keep the
	// per-pair sequence caches and continue an unfinished eager scan in place.
	StopAnalysis();

	if (ComparisonTargets().empty()) {
		mFrmState.Empty();
		UpdateAllViews(NULL);
		return;
	}
	mFileScanThread->ensureRunning();

	const qmetric_info* qminfo = &qmetric_info_table[metricIdx];
	if (qminfo->lazy) {
		for (int target:ComparisonTargets()) mFileScanThread->setPairError(target,_T(""),metricIdx);
		// Start the worker regardless of whether the base scan is still running:
		// it trails the base scanner, waiting per-frame for the eager parse, and
		// fills every frame in order. Run() sets the running flag synchronously.
		mLpipsScanThread = new LpipsScanThread(this, mFileScanThread, metricIdx);
		mLpipsScanThread->run();

		// The base scan's periodic refresh timer may already have stopped (or
		// never started for the lazy metric). Restart it so the graph and label
		// update as the worker fills frames; OnTimer stops it once both the base
		// scan and the lazy scan are done.
		if (pMainFrm)
			pMainFrm->SetTimer(CTI_ID_POS_INVALIDATE, VIT_INVALIDATE_DUR, NULL);
	}

	UpdateCurrentMetricState(metricIdx);
	if (pMainFrm) pMainFrm->SetTimer(CTI_ID_POS_INVALIDATE, VIT_INVALIDATE_DUR, NULL);

	if (mFrmsInfoView)
		mFrmsInfoView->Invalidate();
	UpdateAllViews(NULL);
}

bool CComparatorDoc::GetSelectionRect(int &l, int &t, int &r, int &b) const
{
	if (!mHasSelection || mW <= 0 || mH <= 0)
		return false;

	l = mSelStart.x < mSelCur.x ? mSelStart.x : mSelCur.x;
	t = mSelStart.y < mSelCur.y ? mSelStart.y : mSelCur.y;
	r = mSelStart.x > mSelCur.x ? mSelStart.x : mSelCur.x;
	b = mSelStart.y > mSelCur.y ? mSelStart.y : mSelCur.y;

	if (l < 0) l = 0;
	if (t < 0) t = 0;
	if (r > mW - 1) r = mW - 1;
	if (b > mH - 1) b = mH - 1;

	return (r >= l && b >= t);
}

// Publish each pair's latest request for the actual displayed source frames and
// committed ROI. Numerical work runs on owned snapshots, never on the UI thread.
void CComparatorDoc::UpdateCurrentMetricState(int metricIdx)
{
	const auto& metric = qmetric_info_table[metricIdx];
	int l=0,t=0,r=0,b=0;
	bool roi = GetSelectionRect(l,t,r,b);
	const auto targets = ComparisonTargets();
	if (targets.empty()) { mFrmState.Empty(); return; }
	const bool modelAvailable=metric.ml_id && LpipsEngine::getInstance().availableFor(metric.ml_id);
	CString signatures[3]; unsigned changedTargets=0;
	for (int target:targets) {
		auto& signature=signatures[target-1];
		signature.Format(_T("%u:%d:%d:%d:%d:%d:%d:%d:%d:%d:%ld:%ld"),
			mFileScanThread->pairGeneration(target),metricIdx,modelAvailable,mW,mH,roi,l,t,r,b,
			mPane[0].curFrameID,mPane[target].curFrameID);
		if (signature!=mCurrentSignature[target-1]) changedTargets|=1u<<target;
	}
	if (changedTargets) {
		auto request = std::make_shared<CurrentComparisonRequest>();
		request->id = ++mCurrentRequest;
		request->targets=changedTargets;
		request->metric = metricIdx; request->width=mW; request->height=mH;
		request->hasRoi=roi; request->l=l; request->t=t; request->r=r; request->b=b;
		for (int i=0;i<4;++i) if ((i==0 || (changedTargets&(1u<<i))) && mPane[i].isAvail() && mPane[i].curFrameID>=0) {
			const auto& source=mPane[i];
			auto snapshot=std::make_shared<ComparatorPane>();
			snapshot->curFrameID=source.curFrameID; snapshot->frames=source.frames;
			snapshot->colorSpace=source.colorSpace; snapshot->csLoadInfo=source.csLoadInfo;
			snapshot->csc2rgb888=source.csc2rgb888; snapshot->csc2yuv420=source.csc2yuv420;
			snapshot->origBufSize=snapshot->origSceneSize=source.origSceneSize;
			snapshot->origBuf=new BYTE[source.origSceneSize];
			memcpy(snapshot->origBuf,source.origBuf,source.origSceneSize);
			snapshot->rgbBufSize=source.rgbBufSize; snapshot->rgbBuf=new BYTE[source.rgbBufSize];
			memcpy(snapshot->rgbBuf,source.rgbBuf,source.rgbBufSize); snapshot->rgbValid=true;
			request->sources[i]=snapshot;
		}
		for (int target : targets) if (changedTargets&(1u<<target)) {
			mCurrentSignature[target-1]=signatures[target-1];
			mCurrentPairRequests[target-1]=request->id;
			request->strategies[target-1]=PairStrategy(target);
			mPairResults[target-1]=PairCurrentResult();
			mPairResults[target-1].request=request->id;
			mPairResults[target-1].state=_T("Computing");
			if (!request->sources[0] || !request->sources[target])
				mPairResults[target-1].state=_T("Unavailable: current frame could not be decoded");
		}
		mFileScanThread->queueCurrent(request);
		static_cast<CMainFrame*>(AfxGetMainWnd())->SetTimer(CTI_ID_POS_INVALIDATE,VIT_INVALIDATE_DUR,nullptr);
	}
	for (int target : targets) {
		mFileScanThread->copyCurrent(target,mCurrentPairRequests[target-1],mPairResults[target-1]);
		auto summary=mFileScanThread->summary(target);
		if (metric.lazy && mPairResults[target-1].state==_T("Preparing"))
			mPairResults[target-1].state=IsLpipsScanRunning() ? _T("Preparing model") : _T("Unavailable");
		if (metric.lazy && !IsLpipsScanRunning() && !summary.metricError[metricIdx].IsEmpty()
			&& mPairResults[target-1].full.IsEmpty()) mPairResults[target-1].state=summary.metricError[metricIdx];
	}
	mFrmState=mPairResults[0].full;
	if (!mPairResults[0].roi.IsEmpty()) mFrmState+=_T("   |   ")+mPairResults[0].roi;
	if (mFrmState.IsEmpty() && PairFrames(1)) mFrmState=mPairResults[0].state;
	mFileScanThread->ensureRunning();
}

// Recompute the current-frame metric line (which now includes the crop score)
// and repaint the readout. Called when the selection rectangle is committed or
// cleared so the crop figure tracks the selection (issue #74).
void CComparatorDoc::RefreshSelectionMetric()
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	if (!pMainFrm)
		return;

	UpdateCurrentMetricState(pMainFrm->mMetricIdx);
	if (mFrmInfoView)
		mFrmInfoView->Invalidate(FALSE);
}
