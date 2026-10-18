#pragma once

#include "FrmSrc.h"

#include "CscThread.h"
#include "QHeifUtil.h"
#include "Y4mReaderWin.h"

#include <cmath>

class RawFrmSrc : public FrmSrc {
public:
	RawFrmSrc(CViewerDoc *pDoc)
	: FrmSrc(false), mNextFrame(-1), mFrames(0)
	{
		for (int i = 0; i < NUM_THREADS; i++) {
			mCscRequesters[i] = new CscThread(pDoc->mBufferPool, pDoc->mBufferQueue,
				&pDoc->mPlayFrameID, pDoc->mBgr888Processor);
		}
	}

	virtual ~RawFrmSrc()
	{
		Release();
		for (int i = 0; i < NUM_THREADS; i++)
			delete mCscRequesters[i];
	}

	virtual inline bool Open(CString &filePath)
	{
		if (q1::isHeifFileW(filePath.GetString())) {
			LOGWRN("HEIF file was not decoded; skip raw fallback");
			return false;
		}

		CFileException e;
		BOOL ok = mFile.Open(filePath,
			CFile::modeRead | CFile::shareDenyNone | CFile::typeBinary, &e);
		if (!ok) {
			e.ReportError();
			return false;
		}

		mNextFrame = 0;

		mFilePath = filePath;
		mY4m = {};
		if (q1view::HasY4mSignature(mFile) && !q1view::ParseY4m(mFile, mY4m)) {
			mFile.Close(); return false;
		}

		return true;
	}

	virtual inline void ConfigureDoc(CViewerDoc *pDoc)
	{
		if (!mY4m.layout.offsets.empty()) {
			pDoc->mW = mY4m.width; pDoc->mH = mY4m.height;
			for (const auto& info : qcsc_info_table) if (info.cs == mY4m.colorSpace) {
				pDoc->mColorSpace = info.cs; pDoc->mCsc2Rgb888 = info.csc2rgb888;
				pDoc->mCsLoadInfo = info.cs_load_info; pDoc->mSampleNativePixel = info.sample_native_pixel;
				break;
			}
			pDoc->mHasTimingFps = mY4m.fps > 0;
			if (pDoc->mHasTimingFps) pDoc->mFps = mY4m.fps;
			return;
		}
		int ret;
		CString fileName = mFilePath.Mid(mFilePath.ReverseFind('\\') + 1);
		fileName.MakeLower();
		char szFileName[MAX_PATH + 1];
		strncpy_s(szFileName, CT2A(fileName), MAX_PATH);

		ret = q1::image_parse_w_h(szFileName, &pDoc->mW, &pDoc->mH);
		if (ret) {
			LOGWRN("width or height is not found");
		}

		const struct qcsc_info * const ci =
			q1::image_find_cs(pDoc->mSortedCscInfo, szFileName);
		if (ci == NULL) {
			LOGWRN("color space is not found");
		} else  {
			// Since I just changed the menu title and id when It clicked,
			// Whole new setting might be necessary
			pDoc->mColorSpace = ci->cs;
			pDoc->mCsc2Rgb888 = ci->csc2rgb888;
			pDoc->mCsLoadInfo = ci->cs_load_info;
			pDoc->mSampleNativePixel = ci->sample_native_pixel;
		}

		double fps = -1.0;
		if (q1::image_parse_arg(szFileName, &fps, "fps") == 0 &&
				fps > 0.0 && std::isfinite(fps)) {
			pDoc->mFps = fps;
			pDoc->mHasTimingFps = true;
		} else {
			pDoc->mHasTimingFps = false;
		}
	}

	virtual inline bool LoadOrigBuf(CViewerDoc *pDoc, BYTE *buf)
	{
		if (RawLayout()) return mY4m.layout.Read(mFile, pDoc->mCurFrameID, buf, pDoc->mOrigSceneSize);
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

	virtual inline long CalNumFrame(CViewerDoc *pDoc)
	{
		mFrames = RawLayout() ? long(mY4m.layout.offsets.size()) : long(mFile.GetLength() / pDoc->mOrigSceneSize);
		return mFrames;
	}

	virtual inline bool SetFramePos(CViewerDoc *pDoc, long id)
	{
		if (id < 0 || id >= mFrames)
			return false;
		if (RawLayout()) { mNextFrame = id; return true; }

		mNextFrame = id;

		ULONGLONG pos = ULONGLONG(id) * pDoc->mOrigSceneSize;
		mFile.Seek(pos, CFile::begin);

		return true;
	}

	virtual inline bool Play(CViewerDoc *pDoc)
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
		mY4m = {};
	}
	const q1view::RawFrameLayout* RawLayout() const override {
		return mY4m.layout.offsets.empty() ? nullptr : &mY4m.layout;
	}

private:
	CFile mFile;
	q1view::Y4mInfo mY4m;
	long mNextFrame;
	int mFrames;
	CString mFilePath;
	CscThread *mCscRequesters[NUM_THREADS];
};
