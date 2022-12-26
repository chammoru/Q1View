// ViewerDoc.h : interface of the CViewerDoc class
//


#pragma once

#include <vector>

#include "SSafeCQ.h"
#include "SBufferPool.h"
#include "QOcv.h"

#include "qimage_cs.h"
#include "FileChangeNotiThread.h"

#define NUM_THREADS           3
// If the pool is too big, it will exhaust the memory too much
#define NUM_BUFFER_POOL       (NUM_THREADS * 3)
// good, if the queue size is larger than pool size or equal to it
#define NUM_BUFFER_QUEUE      (NUM_THREADS * 3)
#define VIEWER_DEF_W          500
#define VIEWER_DEF_H          392
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

	// For original buffer
	BYTE *mOrigBuf;
	size_t mOrigBufSize;
	size_t mOrigSceneSize;
	int mBufOffset2, mBufOffset3;

	// For Multithreading Play
	SBufferPool *mBufferPool;
	SSafeCQ<BufferInfo> *mBufferQueue;
	double mFps;

	// Various Color Formats
	QIMAGE_CS mColorSpace;
	QIMAGE_CSC_FN mCsc2Rgb888;
	QIMAGE_CS_INFO_FN mCsLoadInfo;
	QIMAGE_SET_PIXEL_STR_FN mCsSetPixelStr;
	struct qcsc_info *mSortedCscInfo;

	CString mPendingFile;
	// from OpenCV library
	std::vector<FrmSrc *> mFrmSrcs;
	FrmSrc *mFrmSrc;

	q1::ImageProcessor *mBgr888Processor;
	FileChangeNotiThread *mFileChangeNotiThread;

// Operations
public:
	bool QueueSource2View();
	int SeekScene(int frameID);
	int NextScene();
	int PrevScene();
	int FirstScene();
	int LastScene();
	void LoadSourceImage();
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
	void SetPixelString(int viewX, int viewY, int base, char* strBuf);
};
