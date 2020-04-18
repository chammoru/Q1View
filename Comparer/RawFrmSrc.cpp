#include "stdafx.h"
#include "RawFrmSrc.h"
#include "ComparerDoc.h"
#include "QImageStr.h"
#include "QDebug.h"
#include "QCommon.h"

RawFrmSrc::RawFrmSrc(SQPane *pane)
: FrmSrc(pane)
, mFileSize(0L)
{}

RawFrmSrc::~RawFrmSrc()
{
}

bool RawFrmSrc::Open(const CString& filePath)
{
	CFileException e;
	BOOL ok = mFile.Open(filePath,
		CFile::modeRead | CFile::shareDenyNone | CFile::typeBinary, &e);
	if (!ok) {
		e.ReportError();
		return false;
	}

	mFileSize = mFile.GetLength();

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
}

static CString parseLowerFileName(CString& pathName)
{
	CString fileName = pathName.Mid(pathName.ReverseFind('\\') + 1);
	fileName.MakeLower();

	return fileName;
}

bool RawFrmSrc::GetResolution(CString &pathName, int* w, int* h)
{
	char szFileName[MAX_PATH + 1];
	CString fileName = parseLowerFileName(pathName);
	strncpy_s(szFileName, CT2A(fileName), MAX_PATH);

	int error = qimage_parse_w_h(szFileName, w, h);
	if (error) {
		LOGWRN("width and height is not found");
		return false;
	}

	return true;
}

const struct qcsc_info* RawFrmSrc::GetColorSpace(CString &pathName,
		struct qcsc_info* sortedCscInfo)
{
	char szFileName[MAX_PATH + 1];
	CString fileName = parseLowerFileName(pathName);
	strncpy_s(szFileName, CT2A(fileName), MAX_PATH);

	return qimage_find_cs(sortedCscInfo, szFileName);
}

bool RawFrmSrc::IsAvailable()
{
	return mFile != CFile::hFileNull && mFileSize > 0;
}

void RawFrmSrc::FillSceneBuf(BYTE* origBuf, long curFrameID)
{
	if (curFrameID >= 0) {
		ULONGLONG pos = ULONGLONG(curFrameID) * mPane->origSceneSize;
		mFile.Seek(pos, CFile::begin);
	}

	UINT nRead = mFile.Read(origBuf, UINT(mPane->origSceneSize));
	if (nRead < mPane->origSceneSize) {
		LOGWRN("The remaining data is too short to be one frame");

		// initialize the remaining buffer
		memset(origBuf + nRead, 0, mPane->origSceneSize - nRead);
	}
}
