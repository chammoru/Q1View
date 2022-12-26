#pragma once

#include "FrmSrc.h"
#include "ViewerDoc.h"

#include <opencv2/core/core.hpp>

#include "FileFinderThread.h"

class MatFrmSrc : public FrmSrc {
public:
	MatFrmSrc(CViewerDoc *pDoc)
	: FrmSrc(true)
	{
		mFileFinderThread = new FileFinderThread(pDoc->mBufferPool, pDoc->mBufferQueue,
			&pDoc->mPlayFrameID, pDoc->mBgr888Processor);
	}

	virtual ~MatFrmSrc()
	{
		Release();
		delete mFileFinderThread;
	}

	virtual inline bool Open(CString &filePath)
	{
		cv::String str = CT2A(filePath.GetString());
		mOcvMat = cv::imread(str);
		return mOcvMat.data != NULL;
	}

	virtual inline void ConfigureDoc(CViewerDoc *pDoc)
	{
		pDoc->mW = mOcvMat.cols;
		pDoc->mH = mOcvMat.rows;

		pDoc->mColorSpace = QIMAGE_CS_BGR888;
		pDoc->mCsc2Rgb888 = qimage_bgr888_to_bgr888;
		pDoc->mCsLoadInfo = qimage_rgb888_load_info;
		pDoc->mCsSetPixelStr = nullptr;
	}

	virtual inline bool LoadOrigBuf(CViewerDoc *pDoc, BYTE *buf)
	{
		size_t matSize = mOcvMat.total() * mOcvMat.elemSize();
		memcpy(buf, mOcvMat.data, matSize);
		if (matSize < pDoc->mOrigSceneSize)
			memset(buf + matSize, 0, pDoc->mOrigSceneSize - matSize);

		return true;
	}

	virtual long CalNumFrame(CViewerDoc *pDoc) { return 1; }

	virtual inline bool SetFramePos(CViewerDoc *pDoc, long id)
	{
		long count = id - 1;
		if (count <= 0)
			return false;

		CFileFind finder;
		BOOL bWorking = finder.FindFile(pDoc->mPurePathName);

		bool found = false;
		while (bWorking) {
			bWorking = finder.FindNextFile();

			if (finder.IsDirectory())
				continue;

			if (pDoc->mPathName == finder.GetFilePath()) {
				found = true;
				break;
			}
		}

		if (!found)
			return false;

		for (int i = 0; i < count && bWorking; i++) {
			bWorking = finder.FindNextFile();

			if (finder.IsDirectory())
				i--;
		}

		CString newPath = finder.GetFilePath();
		Open(newPath);
		pDoc->mPathName = newPath;
		pDoc->SetPathName(newPath, TRUE);

		finder.Close();

		return true;
	}

	virtual inline bool Play(CViewerDoc *pDoc)
	{
		bool ok = mFileFinderThread->setup(pDoc);
		if (!ok)
			return false;
		mFileFinderThread->run();

		return true;
	}

	virtual inline void Stop()
	{
		mFileFinderThread->requestExitAndWait();
	}

	virtual inline void Release()
	{
		mOcvMat.release();
	}

private:
	cv::Mat mOcvMat;
	FileFinderThread *mFileFinderThread;
};
