// ViewerDoc.h : interface of the CViewerDoc class
//


#pragma once

#include <vector>

#include "SSafeCQ.h"
#include "SBufferPool.h"
#include "QOcv.h"

#include "qimage_cs.h"
#include "qimage_presets.h" // shared VIEWER_DEF_W / VIEWER_DEF_H
#include "FileChangeNotiThread.h"

#define NUM_THREADS           3
// If the pool is too big, it will exhaust the memory too much
#define NUM_BUFFER_POOL       (NUM_THREADS * 3)
// good, if the queue size is larger than pool size or equal to it
#define NUM_BUFFER_QUEUE      (NUM_THREADS * 3)
// VIEWER_DEF_W / VIEWER_DEF_H come from qimage_presets.h (shared with the Qt viewer).
#define VIEWER_DEF_FPS        30.0

enum ID_MSG {
	MSG_QUIT = 0x7fff0000,
};

struct BufferInfo {
	BYTE *addr;
	int ID;
};

enum DocState {
	DOC_JUSTLOAD,
	DOC_NEWIMAGE,
	DOC_ADJUSTED,
};

enum QROTATION {
	QROT_000,
	QROT_090,
	QROT_180,
	QROT_270,
	QROT_MAX,
};

// How the frame should react when a source image is (re)loaded.
enum LoadLayout {
	// Fresh open (File>Open, drag-drop, command line, resolution/rotate):
	// size the frame to the image, as before.
	LOAD_RESIZE_WINDOW,
	// In-place reload of the same file: keep the frame size and the current
	// zoom/pan so an on-disk change does not disturb the view.
	LOAD_PRESERVE_VIEW,
	// Folder navigation (next/prev/first/last file): keep the current frame
	// size and fit the new image into the existing viewport instead of
	// resizing the window around every image (issue #69).
	LOAD_FIT_TO_WINDOW,
};

class FrmSrc;
class ImageProcessor;
class FileChangeNotiThread;

class CViewerDoc : public CDocument
{
protected: // create from serialization only
	CViewerDoc();
	DECLARE_DYNCREATE(CViewerDoc)

// Attributes
public:
	DocState mDocState;

	CString mPathName, mPurePathName, mPurePathRegex;
	long mCurFrameID;
	long mPlayFrameID;
	CFile mFile;
	long mFrames;

	// Basic Image information
	int mW, mH;

	QROTATION mRot;
	int mOrigW, mOrigH;

	// Original frame data before color conversion and rotation.
	BYTE *mOrigBuf;
	size_t mOrigBufSize;
	size_t mOrigSceneSize;
	int mBufOffset2, mBufOffset3;

	// Playback worker queue and buffer pool.
	SBufferPool *mBufferPool;
	SSafeCQ<BufferInfo> *mBufferQueue;
	double mFps;
	bool mHasTimingFps;

	// Active color-space conversion settings.
	QIMAGE_CS mColorSpace;
	QIMAGE_CSC_FN mCsc2Rgb888;
	QIMAGE_CS_INFO_FN mCsLoadInfo;
	QIMAGE_SAMPLE_NATIVE_PIXEL_FN mSampleNativePixel;
	struct qcsc_info *mSortedCscInfo;

	CString mPendingFile;
	// from OpenCV library
	std::vector<FrmSrc *> mFrmSrcs;
	FrmSrc *mFrmSrc;

	q1::ImageProcessor *mBgr888Processor;
	FileChangeNotiThread *mFileChangeNotiThread;
	LoadLayout mLoadLayout;

// Operations
public:
	bool QueueSource2View();
	bool RefreshNativePixelSource();
	int SeekScene(int frameID);
	int NextScene();
	int PrevScene();
	int FirstScene();
	int LastScene();
	void LoadSourceImage(LoadLayout layout = LOAD_RESIZE_WINDOW);
	BOOL ReloadDocument();
	void UpdateMenu();
	void Rotate90();

// Overrides
public:
	virtual BOOL OnNewDocument();
	virtual void Serialize(CArchive& ar);

// Implementation
public:
	virtual ~CViewerDoc();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
public:

	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	virtual BOOL DoSave(LPCTSTR lpszPathName, BOOL bReplace = TRUE);
	virtual void SetPathName(LPCTSTR lpszPathName, BOOL bAddToMRU = TRUE);
	bool GetNativePixelSample(int viewX, int viewY, QIMAGE_NATIVE_PIXEL_SAMPLE *sample);
	void NextCs();
};
