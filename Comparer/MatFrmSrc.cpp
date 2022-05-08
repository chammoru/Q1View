#include "stdafx.h"
#include "MatFrmSrc.h"
#include "QImageStr.h"
#include "ComparerPane.h"

using namespace cv;

MatFrmSrc::MatFrmSrc(SQPane *pane) : FrmSrc(pane, true) {}

MatFrmSrc::~MatFrmSrc()
{
	Release();
}

bool MatFrmSrc::Open(const CString& filePath, const struct qcsc_info* sortedCscInfo,
	int srcW, int srcH, int dstW, int dstH)
{
	String str = CT2A(filePath.GetString());
	mOcvMat = imread(str);
	bool success = mOcvMat.data != NULL;
	if (success && (mOcvMat.cols != dstW || mOcvMat.rows != dstH)) {
		resize(mOcvMat, mOcvMat, Size(dstW, dstH));
	}
	return success;
}

int MatFrmSrc::GetFrameNum()
{
	return 1;
}

void MatFrmSrc::Release()
{
	if (IsAvailable())
		mOcvMat.release();
}

bool MatFrmSrc::GetResolution(CString &pathName, int* w, int* h)
{
	String str = CT2A(pathName.GetString());
	Mat ocvMat = imread(str);
	if (ocvMat.data == NULL)
		return false;

	*w = ocvMat.cols;
	*h = ocvMat.rows;
	ocvMat.release();

	return true;
}

const struct qcsc_info* MatFrmSrc::GetColorSpace(const CString &pathName,
		const struct qcsc_info* sortedCscInfo, bool doReisze)
{
	String str = CT2A(pathName.GetString());
	Mat ocvMat = imread(str);
	if (ocvMat.data == NULL)
		return NULL;

	ocvMat.release();
	return q1::image_find_cs(sortedCscInfo, "bgr888");
}

bool MatFrmSrc::IsAvailable()
{
	return mOcvMat.data != NULL;
}

bool MatFrmSrc::FillSceneBuf(BYTE* origBuf)
{
	memcpy(origBuf, mOcvMat.data, mPane->origSceneSize);

	return true;
}

long MatFrmSrc::GetNextFrameID()
{
	return 0L;
}

bool MatFrmSrc::SetNextFrameID(long frameID)
{
	if (frameID != 0)
		return false;
	return true;
}

double MatFrmSrc::GetFps()
{
	return -1;
}
