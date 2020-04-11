#pragma once

#include "FrmSrc.h"

#include "CscThread.h"

class RawFrmSrc : public FrmSrc {
public:
	RawFrmSrc(CViewerDoc *pDoc)
	: FrmSrc(false), mNextFrame(-1)
	{
		for (int i = 0; i < NUM_THREADS; i++) {
			mCscRequesters[i] = new CscThread(pDoc->mBufferPool, pDoc->mBufferQueue,
				&pDoc->mPlayFrameID, pDoc->mBgr888Processor);
		}
	}

	virtual ~RawFrmSrc()
	{
		for (int i = 0; i < NUM_THREADS; i++)
			delete mCscRequesters[i];
	}

	virtual inline bool Open(CString &filePath)
	{
		CFileException e;
		BOOL ok = mFile.Open(filePath,
			CFile::modeRead | CFile::shareDenyNone | CFile::typeBinary, &e);
		if (!ok) {
			e.ReportError();
			return false;
		}

		mNextFrame = 0;

		mFilePath = filePath;

		return true;
	}

	virtual void ConfigureDoc(CViewerDoc *pDoc)
	{
		int ret;
		CString fileName = mFilePath.Mid(mFilePath.ReverseFind('\\') + 1);
		fileName.MakeLower();
		char szFileName[MAX_PATH + 1];
		strncpy_s(szFileName, CT2A(fileName), MAX_PATH);

		ret = qimage_parse_w_h(szFileName, &pDoc->mW, &pDoc->mH);
		if (ret) {
			LOGWRN("width or height is not found");
		}

		const struct qcsc_info * const ci =
			qimage_find_cs(pDoc->mSortedCscInfo, szFileName);
		if (ci == NULL) {
			LOGWRN("color space is not found");
		} else  {
			// Since I just changed the menu title and id when It clicked,
			// Whole new setting might be necessary
			pDoc->mColorSpace = ci->cs;
			pDoc->mCsc2Rgb888 = ci->csc2rgb888;
			pDoc->mCsLoadInfo = ci->cs_load_info;
		}

		qimage_parse_arg(szFileName, &pDoc->mFps, "fps");
	}

	virtual bool LoadOrigBuf(CViewerDoc *pDoc, BYTE *buf)
	{
		if (mFile.m_hFile == CFile::hFileNull) {
			LOGERR("File is not available");
			return false;
		}

		if (pDoc->mCurFrameID != mNextFrame) {
			ULONGLONG pos = ULONGLONG(pDoc->mCurFrameID) * pDoc->mOrigSceneSize;
			mFile.Seek(pos, CFile::begin);
		}

		UINT nRead = mFile.Read(buf, UINT(pDoc->mOrigSceneSize));
		if (nRead < pDoc->mOrigSceneSize) {
			LOGWRN("The remaining data is too short to be one frame");

			// initialize the remaining buffer
			memset(buf + nRead, 0, pDoc->mOrigSceneSize - nRead);
		}

		mNextFrame = pDoc->mCurFrameID + 1;

		return true;
	}

	virtual long CalNumFrame(CViewerDoc *pDoc)
	{
		mFrames = long(mFile.GetLength() / pDoc->mOrigSceneSize);
		return mFrames;
	}

	virtual bool SetFramePos(CViewerDoc *pDoc, long id)
	{
		if (id >= mFrames)
			return false;

		mNextFrame = id;

		ULONGLONG pos = ULONGLONG(id) * pDoc->mOrigSceneSize;
		mFile.Seek(pos, CFile::begin);

		return true;
	}

	virtual bool Play(CViewerDoc *pDoc)
	{
		int i = 0;

		for (; i < NUM_THREADS; i++) {
			bool ok = mCscRequesters[i]->setup(pDoc);
			ASSERT(ok);
			if (!ok) {
				LOGERR("mCscRequesters[%d] setup: failed", i);
				i--;
				goto PlayFail;
			}

			mCscRequesters[i]->run();
		}

		return true;

PlayFail:
		for (; i >= 0; i--) {
			mCscRequesters[i]->requestExitAndWait();
			mCscRequesters[i]->deinit();
		}

		return false;
	}

	virtual inline void Stop()
	{
		for (int i = 0; i < NUM_THREADS; i++) {
			mCscRequesters[i]->requestExitAndWait();
			mCscRequesters[i]->deinit();
		}
	}

	virtual inline void Release()
	{
		if (mFile.m_hFile != CFile::hFileNull)
			mFile.Close();
		mNextFrame = -1;
	}

private:
	CFile mFile;
	long mNextFrame;
	int mFrames;
	CString mFilePath;
	CscThread *mCscRequesters[NUM_THREADS];
};
