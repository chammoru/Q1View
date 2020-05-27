// ViewerDoc.cpp : implementation of the CViewerDoc class
//

#include "stdafx.h"
#include "Viewer.h"
#include "MainFrm.h"

#include "ViewerDoc.h"
#include "ViewerView.h"

#include <cstring>
#include <QImageStr.h>
#include "QCommon.h"

#include "VidCapFrmSrc.h"
#include "MatFrmSrc.h"
#include "RawFrmSrc.h"
#include "ViewerCmn.h"

#include "QOcv.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using namespace std;

// CViewerDoc

IMPLEMENT_DYNCREATE(CViewerDoc, CDocument)

BEGIN_MESSAGE_MAP(CViewerDoc, CDocument)
END_MESSAGE_MAP()

// CViewerDoc construction/destruction

CViewerDoc::CViewerDoc()
: mDocState(DOC_JUSTLOAD)
, mPathName("")
, mCurFrameID(0L)
, mFrames(0)
, mW(VIEWER_DEF_W)
, mH(VIEWER_DEF_H)
, mRot(QROT_000)
, mOrigW(-1)
, mOrigH(-1)
, mOrigBuf(NULL)
, mOrigBufSize(0)
, mOrigSceneSize(0)
, mBufOffset2(0)
, mBufOffset3(0)
, mBufferPool(new SBufferPool(NUM_BUFFER_POOL))
, mBufferQueue(new SSafeCQ<BufferInfo>(NUM_BUFFER_QUEUE))
, mFps(VIEWER_DEF_FPS)
, mColorSpace(qcsc_info_table[QIMG_DEF_CS_IDX].cs)
, mCsc2Rgb888(qcsc_info_table[QIMG_DEF_CS_IDX].csc2rgb888)
, mCsLoadInfo(qcsc_info_table[QIMG_DEF_CS_IDX].cs_load_info)
, mFrmSrc(NULL)
, mBgr888Processor(NULL)
, mFileChangeNotiThread(new FileChangeNotiThread)
{
	// TODO: add one-time construction code here
	mSortedCscInfo =
		static_cast<struct qcsc_info *>(malloc(sizeof(qcsc_info_table)));
	ASSERT(mSortedCscInfo != NULL);

	q1::image_sort_cs(mSortedCscInfo);

	// ADD QIMAGEPROCESSOR HERE, IF NEEDED
	// mBgr888Processor = new SomeDetector;

	// ADD MORE FRAME SOURCES, IF NEEDED
	mFrmSrcs.push_back(new MatFrmSrc(this));
	mFrmSrcs.push_back(new VidCapFrmSrc(this));
	mFrmSrcs.push_back(new RawFrmSrc(this));
}

CViewerDoc::~CViewerDoc()
{
	for (auto it = std::begin(mFrmSrcs); it != std::end(mFrmSrcs); it++)
		delete *it;

	if (mBgr888Processor)
		delete mBgr888Processor;

	free(mSortedCscInfo);

	delete mBufferQueue;
	delete mBufferPool;

	if (mOrigBuf)
		delete [] mOrigBuf;

	delete mFileChangeNotiThread;
}

BOOL CViewerDoc::OnNewDocument()
{
	if (!CDocument::OnNewDocument())
		return FALSE;

	// TODO: add reinitialization code here
	// (SDI documents will reuse this document)
	mPathName.Empty();

	return TRUE;
}

void CViewerDoc::Rotate90()
{
	if (mOrigW > 0 && mOrigH > 0) {
		mW = mOrigW;
		mH = mOrigH;
	} else {
		mOrigW = mW;
		mOrigH = mH;
	}

	mRot = QROTATION((mRot + 1) % QROT_MAX);
	if (mRot == QROT_090 || mRot == QROT_270)
		QSWAP(mW, mH);

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	pMainFrm->RefreshView();
	pMainFrm->UpdateResolutionLabel(mW, mH);
	pMainFrm->CheckResolutionRadio(mW, mH);
}

bool CViewerDoc::QueueSource2View()
{
	int h = mH, w = mW;
	if (mRot == QROT_090 || mRot == QROT_270)
		QSWAP(h, w);

	BYTE *RGB = mBufferPool->checkout();
	if (RGB == NULL)
		return false;

	// mOrigBuf contains RGB images, not being supposed to be DWORD-aligned
	bool ok = mFrmSrc->LoadOrigBuf(this, mOrigBuf);
	if (!ok)
		return false;

	BufferInfo bi = PostProcess(mColorSpace, mBgr888Processor, w, h,
		mOrigBuf, RGB, mBufOffset2, mBufOffset3, mRot, mCsc2Rgb888, mCurFrameID);

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	CViewerView *pView = static_cast<CViewerView *>(pMainFrm->GetActiveView());
	SSafeCQ<BufferInfo> *newBuffer1Q = pView->mNewRgbBufferInfoQ;
	newBuffer1Q->push(bi);

	return true;
}

static inline void GetScreenSize(CMainFrame *pMainFrm, int &wScreen, int &hScreen)
{
	HMONITOR hmon = MonitorFromWindow(pMainFrm->m_hWnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFO mi;
	mi.cbSize = sizeof(MONITORINFO);
	BOOL OK = ::GetMonitorInfo(hmon, &mi);
	QASSERT(OK);

	RECT &rcMon = mi.rcMonitor;
	wScreen = rcMon.right - rcMon.left;
	hScreen = rcMon.bottom - rcMon.top;
}

static inline void GetBezelSize(CMainFrame *pMainFrm, int &wBezel, int &hBezel)
{
	CRect wndRect, cntRect;

	pMainFrm->GetWindowRect(wndRect);
	pMainFrm->GetClientRect(cntRect);

	wBezel = wndRect.Width() - cntRect.Width();
	hBezel = wndRect.Height() - cntRect.Height();
}

void CViewerDoc::LoadSourceImage()
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	CViewerView *pView = static_cast<CViewerView *>(pMainFrm->GetActiveView());

	mBufferPool->disable();
	mBufferQueue->destroy();

	pView->KillPlayTimer();
	pView->mSelRegions.clear();

	int w = mW, h = mH;
	if (mRot == QROT_090 || mRot == QROT_270)
		QSWAP(w, h);

	mOrigSceneSize = mCsLoadInfo(w, h, &mBufOffset2, &mBufOffset3);
	if (mOrigBufSize < mOrigSceneSize) {
		if (mOrigBuf)
			delete [] mOrigBuf;

		mOrigBufSize = mOrigSceneSize;
		mOrigBuf = new BYTE[mOrigBufSize];
	}

	mFrmSrc->SetFramePos(this, 0);
	mFrames = mFrmSrc->CalNumFrame(this);

	mCurFrameID = 0;

	// The width must be DWORD aligned.
	// Also, do ROUNDUP_DWORD even for height for the rotations of 90 and 270
	size_t rgbBufSize = ROUNDUP_DWORD(mW) * ROUNDUP_DWORD(mH) * QIMG_DST_RGB_BYTES;
	mBufferPool->setup(rgbBufSize);

	mBufferQueue->init();
	pView->Initialize(mFrames, ROUNDUP_DWORD(mW), mW, mH);

	QueueSource2View();

	int wScreen = 0;
	int hScreen = 0;
	GetScreenSize(pMainFrm, wScreen, hScreen);

	int wBezel = 0;
	int hBezel = 0;
	GetBezelSize(pMainFrm, wBezel, hBezel);

	if (mW + wBezel > wScreen || mH + hBezel + pView->mHProgress > hScreen) {
		if (!pMainFrm->IsZoomed()) {
			pMainFrm->SendMessage(WM_SYSCOMMAND, SC_MAXIMIZE,
				(LPARAM)pMainFrm->GetSafeHwnd());
		}

		CPoint pt;
		GetCursorPos(&pt);
		pView->ChangeZoom(-WHEEL_DELTA, pt);
	} else {
		// SetWindowPos, which is located in AdjustWindowSize(), should be invoked
		// after QueueSource2View and before OnDraw because QueueSource2View might
		// take some time and OnDraw funtion uses the current client size
		pView->AdjustWindowSize();
	}

	mDocState = DOC_NEWIMAGE;
}

// CViewerDoc serialization

void CViewerDoc::Serialize(CArchive& ar)
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

int CViewerDoc::SeekScene(int frameID)
{
	if (frameID >= mFrames || frameID < 0) {
		LOGWRN("out of bound");
		return -1;
	}

	mCurFrameID = frameID;
	
	if (!QueueSource2View())
		return -1;

	return mCurFrameID;
}

int CViewerDoc::NextScene()
{
	if (mCurFrameID >= mFrames - 1) {
		LOGWRN("already end");
		return -1;
	}

	mCurFrameID++;

	if (!QueueSource2View())
		return -1;

	return mCurFrameID;
}

int CViewerDoc::PrevScene()
{
	if (mCurFrameID == 0) {
		LOGWRN("already first");
		return -1;
	}

	mCurFrameID--;

	if (!QueueSource2View())
		return -1;

	return mCurFrameID;
}

int CViewerDoc::FirstScene()
{
	mCurFrameID = 0;

	if (!QueueSource2View())
		return -1;

	return mCurFrameID;
}

int CViewerDoc::LastScene()
{
	mCurFrameID = mFrames - 1;

	if (!QueueSource2View())
		return -1;

	return mCurFrameID;
}

// CViewerDoc diagnostics

#ifdef _DEBUG
void CViewerDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CViewerDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG

void CViewerDoc::UpdateMenu()
{
	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	CViewerView *pView = static_cast<CViewerView *>(pMainFrm->GetActiveView());

	pMainFrm->UpdateResolutionLabel(mW, mH);
	pMainFrm->CheckResolutionRadio(mW, mH);

	const char *cs_name = q1::image_find_cs_name(mSortedCscInfo, mColorSpace);
	if (cs_name) {
		CString str = CA2W(cs_name);
		pMainFrm->UpdateCsLabel(str.MakeUpper());
		pMainFrm->CheckCsRadio(mColorSpace);
	}

	pMainFrm->UpdateFpsLabel(mFps);
	pMainFrm->CheckFpsRadio(mFps);
	pMainFrm->UpdateMagnication(pView->mN, pView->mWDst, pView->mHDst);
}

// CViewerDoc commands
BOOL CViewerDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	if (mPathName != lpszPathName) {
		if (!CDocument::OnOpenDocument(lpszPathName))
			return FALSE;
	}

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	if (pMainFrm == NULL) {
		// take care of the open operation in CMainFrame::ActivateFrame()
		mPendingFile = lpszPathName;
		::CoInitialize(NULL); // TODO: Is this really right solution?
		return TRUE;
	}

	mPathName = lpszPathName;
	mPurePathName = mPathName.Left(mPathName.ReverseFind('\\') + 1);
	mPurePathRegex = mPurePathName + _T("*");
	mRot = QROT_000;

	if (mFrmSrc)
		mFrmSrc->Release();

	auto it = std::begin(mFrmSrcs);
	for (; it != std::end(mFrmSrcs); it++) {
		bool ok = (*it)->Open(mPathName);
		if (ok) {
			mFrmSrc = *it;
			break;
		}
	}
	ASSERT(it != std::end(mFrmSrcs));

	mFileChangeNotiThread->fire(pMainFrm, mPathName);

	mOrigW = mOrigH = -1;
	mFrmSrc->ConfigureDoc(this);

	LoadSourceImage();

	UpdateMenu();

	return TRUE;
}

static BOOL OpenCfileForWriting(const string &pathName, CFile &wFile)
{
	CFileException e;
	BOOL ok = wFile.Open(CString(pathName.c_str()),
		CFile::modeWrite | CFile::modeCreate | CFile::shareDenyNone | CFile::typeBinary, &e);
	if (!ok) {
		e.ReportError();
		return FALSE;
	}

	return TRUE;
}

static BOOL WriteBgr888(const string &pathName, const cv::Mat &roi)
{
	CFile wFile;
	BOOL ok = OpenCfileForWriting(pathName, wFile);
	if (!ok) {
		LOGERR("failed to open %s", pathName.c_str());
		return FALSE;
	}
	const uchar *buf = roi.ptr<uchar>(0);
	for (int i = 0; i < roi.rows; i++) {
		wFile.Write(buf, UINT(roi.cols * QIMG_DST_RGB_BYTES));
		buf += ROUNDUP_DWORD(roi.cols) * QIMG_DST_RGB_BYTES;
	}

	return TRUE;
}

static BOOL WriteNv21(const string &pathName, const cv::Mat &roi)
{
	int ySize = roi.cols * roi.rows;
	int vuSize = ROUNDUP_EVEN(roi.cols) * ((roi.rows + 1) >> 1);
	qu32 nv21Size = ySize + vuSize;
	vector<qu8> nv21(nv21Size);
	qimage_bgr888_to_nv21(roi.data, &nv21[0], NULL, &nv21[ySize], ROUNDUP_DWORD(roi.cols),
		roi.cols, roi.rows);
	CFile wFile;
	BOOL ok = OpenCfileForWriting(pathName, wFile);
	if (!ok) {
		LOGERR("failed to open %s", pathName.c_str());
		return FALSE;
	}
	wFile.Write(&nv21[0], nv21Size);
	return TRUE;
}

static BOOL WriteYuv420(const string &pathName, const cv::Mat &roi)
{
	int ySize = roi.cols * roi.rows;
	int chromaSize = ((roi.cols + 1) >> 1) * ((roi.rows + 1) >> 1);
	qu32 yuv420Size = ySize + (chromaSize << 1);
	vector<qu8> yuv420(yuv420Size);
	qimage_bgr888_to_yuv420(roi.data, &yuv420[0], &yuv420[ySize], &yuv420[ySize + chromaSize],
		ROUNDUP_DWORD(roi.cols), roi.cols, roi.rows);
	CFile wFile;
	BOOL ok = OpenCfileForWriting(pathName, wFile);
	if (!ok) {
		LOGERR("failed to open %s", pathName.c_str());
		return FALSE;
	}
	wFile.Write(&yuv420[0], yuv420Size);
	return TRUE;
}

// save compressed file using OpenCV
static BOOL OcvWrite(const string &pathName, const cv::Mat &roi)
{
	bool ok = cv::imwrite(pathName, roi);
	if (!ok)
		return FALSE;

	return TRUE;
}

typedef int (*write_func_t)(const string &pathName, const cv::Mat &roi);

struct ImageWriter
{
	const char *ext;
	const write_func_t wFunc;
};

BOOL CViewerDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
	if (mDocState == DOC_JUSTLOAD)
		return FALSE;

	CMainFrame *pMainFrm = static_cast<CMainFrame *>(AfxGetMainWnd());
	CViewerView *pView = static_cast<CViewerView *>(pMainFrm->GetActiveView());

	const string pathName = CT2A(CString(lpszPathName));
	const string ext = pathName.substr(pathName.find_last_of(".") + 1); // extension
	const ImageWriter imageWriters[] = {
		{"jpg",    OcvWrite},
		{"png",    OcvWrite},
		{"bmp",    OcvWrite},
		{"bgr888", WriteBgr888},
		{"yuv420", WriteYuv420},
		{"nv21",   WriteNv21},
	};
	int idx = -1;
	for (int i = 0; i < ARRAY_SIZE(imageWriters); i++) {
		if (_stricmp(imageWriters[i].ext, ext.c_str()) == 0) {
			idx = i;
			break;
		}
	}
	if (idx < 0 || idx >= ARRAY_SIZE(imageWriters)) {
		LOGERR("invalid index [%d]", idx);
		return FALSE;
	}
	cv::Mat roi = pView->GetRoiMat();
	if (!roi.data) {
		LOGWRN("source image doesn't exist");
		return FALSE;
	}

	return imageWriters[idx].wFunc(pathName, roi);
}

// @Override
// For "Save Copy As" most parts are from CDocument:DoSave()
BOOL CViewerDoc::DoSave(LPCTSTR lpszPathName, BOOL bReplace)
{
	CString newName = lpszPathName;
	if (newName.IsEmpty()) {
		CDocTemplate* pTemplate = GetDocTemplate();
		ASSERT(pTemplate != NULL);
		newName = m_strPathName;
		if (!AfxGetApp()->DoPromptFileName(newName,
				AFX_IDS_SAVEFILE, OFN_HIDEREADONLY | OFN_PATHMUSTEXIST, FALSE, pTemplate)) {
			return FALSE;       // don't even attempt to save
		}
	}

	if (!OnSaveDocument(newName)) {
		if (lpszPathName == NULL) {
			// be sure to delete the file
			TRY {
				CFile::Remove(newName);
			} CATCH_ALL(e) {
				TRACE(traceAppMsg, 0, "Warning: failed to delete file after failed SaveAs.\n");
				e->Delete();
			}
			END_CATCH_ALL
		}
		return FALSE;
	}

	return TRUE;        // success
}

void CViewerDoc::SetPathName(LPCTSTR lpszPathName, BOOL bAddToMRU)
{
	// From CDocument::SetPathName(lpszPathName, bAddToMRU);
	// store the path fully qualified
	TCHAR szFullPath[_MAX_PATH];
	ENSURE(lpszPathName);
	if ( _tcslen(lpszPathName) >= _MAX_PATH )
	{
		ASSERT(FALSE);
		// MFC requires paths with length < _MAX_PATH
		// No other way to handle the error from a void function
		AfxThrowFileException(CFileException::badPath);
	}

	DWORD retval = GetFullPathName(lpszPathName, _MAX_PATH, szFullPath, NULL);
	if( retval == 0 )
	{
		ASSERT(FALSE);
		// MFC requires paths with length < _MAX_PATH
		// No other way to handle the error from a void function
		AfxThrowFileException(CFileException::badPath);
	}

	m_strPathName = szFullPath;
	ASSERT(!m_strPathName.IsEmpty());       // must be set to something
	m_bEmbedded = FALSE;
	ASSERT_VALID(this);

	// set the document title based on path name
	SetTitle(PathFindFileName(szFullPath));

	// add it to the file MRU list
	if (bAddToMRU)
		AfxGetApp()->AddToRecentFileList(m_strPathName);

	ASSERT_VALID(this);
}
