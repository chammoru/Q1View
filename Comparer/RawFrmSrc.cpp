#include "stdafx.h"
#include "RawFrmSrc.h"
#include "QImageStr.h"
#include "ComparerPane.h"
#include "QDebug.h"
#include "QCommon.h"
#include "ComparerUtil.h"

using namespace cv;

RawFrmSrc::RawFrmSrc(SQPane *pane)
: FrmSrc(pane, false)
, mFileSize(0L)
, mOrigSceneSize(0L)
, mResizeSceneSize(0L)
, mResizeBuf(nullptr)
{}

RawFrmSrc::~RawFrmSrc()
{
	Release();
}

bool RawFrmSrc::Open(const CString& filePath, const struct qcsc_info* sortedCscInfo,
	int srcW, int srcH, int dstW, int dstH)
{
	CFileException e;
	BOOL ok = mFile.Open(filePath,
		CFile::modeRead | CFile::shareDenyNone | CFile::typeBinary, &e);
	if (!ok) {
		e.ReportError();
		return false;
	}

	mFileSize = mFile.GetLength();
	mSrcW = srcW;
	mSrcH = srcH;
	mDstW = dstW;
	mDstH = dstH;

	mSrcCscInfo = GetColorSpace(filePath, sortedCscInfo, false);
	mOrigSceneSize = mSrcCscInfo->cs_load_info(mSrcW, mSrcH, nullptr, nullptr);

	if ((mSrcW != mDstW || mSrcH != mDstH) && mOrigSceneSize > mResizeSceneSize) {
		if (mResizeBuf != nullptr)
			delete[] mResizeBuf;

		mResizeSceneSize = mOrigSceneSize;
		mResizeBuf = new BYTE[mResizeSceneSize];
	}

	return true;
}

int RawFrmSrc::GetFrameNum()
{
	return MAX(long(mFileSize / mPane->origSceneSize), 1);
}

void RawFrmSrc::Release()
{
	if (IsAvailable())
		mFile.Close();
	mFileSize = 0;
	mOrigSceneSize = 0;
	mResizeSceneSize = 0;
	delete[] mResizeBuf;
	mResizeBuf = nullptr;
}

static CString parseLowerFileName(const CString& pathName)
{
	CString fileName = getBaseName(pathName);
	fileName.MakeLower();

	return fileName;
}

bool RawFrmSrc::GetResolution(CString &pathName, int* w, int* h)
{
	char szFileName[MAX_PATH + 1];
	CString fileName = parseLowerFileName(pathName);
	strncpy_s(szFileName, CT2A(fileName), MAX_PATH);

	int error = q1::image_parse_w_h(szFileName, w, h);
	if (error) {
		LOGWRN("width and height is not found");
		return false;
	}

	return true;
}

const struct qcsc_info* RawFrmSrc::GetColorSpace(const CString &pathName,
		const struct qcsc_info* sortedCscInfo, bool doReisze)
{
	char szFileName[MAX_PATH + 1];
	CString fileName = parseLowerFileName(pathName);
	strncpy_s(szFileName, CT2A(fileName), MAX_PATH);

	if (doReisze) {
		return q1::image_find_cs(sortedCscInfo, "bgr888");
	} else {
		return q1::image_find_cs(sortedCscInfo, szFileName);
	}
}

bool RawFrmSrc::IsAvailable()
{
	return mFile != CFile::hFileNull && mFileSize > 0;
}

static void readFromFile(CFile &file, BYTE *Buf, size_t size) {
	UINT nRead = file.Read(Buf, UINT(size));
	if (nRead < size) {
		LOGWRN("The remaining data is too short to be one frame");

		// initialize the remaining buffer
		memset(Buf + nRead, 0, size - nRead);
	}
}

bool RawFrmSrc::FillSceneBuf(BYTE* origBuf)
{
	if (mSrcW != mDstW || mSrcH != mDstH) {
		LOGWRN("resize");
		readFromFile(mFile, mResizeBuf, mResizeSceneSize);

		Mat matSrcTemp(mSrcH, mSrcW, CV_8UC3);
		int bufOffset2 = 0, bufOffset3 = 0;
		mSrcCscInfo->cs_load_info(mSrcW, mSrcH, &bufOffset2, &bufOffset3);
		mSrcCscInfo->csc2rgb888(matSrcTemp.ptr(),
			mResizeBuf,
			mResizeBuf + bufOffset2,
			mResizeBuf + bufOffset3, ROUNDUP_DWORD(mSrcW), mSrcW, mSrcH);

		Mat matDstTemp(mDstH, mDstW, CV_8UC3, origBuf, mDstW * (size_t)QIMG_DST_RGB_BYTES);
		resize(matSrcTemp, matDstTemp, Size(mDstW, mDstH));
		matSrcTemp.release();
		memcpy(origBuf, matDstTemp.ptr(), matDstTemp.total() * matDstTemp.elemSize());
		matDstTemp.release();
	} else {
		LOGWRN("normal");
		readFromFile(mFile, origBuf, mOrigSceneSize);
	}

	return true;
}

long RawFrmSrc::GetNextFrameID()
{
	return long(mFile.GetPosition() / mOrigSceneSize);
}

bool RawFrmSrc::SetNextFrameID(long frameID)
{
	try {
		ULONGLONG pos = ULONGLONG(frameID) * mOrigSceneSize;
		mFile.Seek(pos, CFile::begin);
	} catch (CFileException *e) {
		LOGWRN("Failed to Seek to frameID %ld", frameID);
		e->Delete();
		return false;
	}

	return true;
}

double RawFrmSrc::GetFps()
{
	double fps = - 1;
	CString fileName = mFile.GetFileName();
	fileName.MakeLower();
	char szFileName[MAX_PATH + 1];
	strncpy_s(szFileName, CT2A(fileName), MAX_PATH);
	q1::image_parse_arg(szFileName, &fps, "fps");

	return fps;
}
