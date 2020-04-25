#include "stdafx.h"
#include "MatFrmSrc.h"
#include "QImageStr.h"
#include "ComparerPane.h"

using namespace cv;

MatFrmSrc::MatFrmSrc(SQPane *pane) : FrmSrc(pane) {}

MatFrmSrc::~MatFrmSrc()
{
	Release();
}

bool MatFrmSrc::Open(const CString& filePath)
{
	String str = CT2A(filePath.GetString());
	mOcvMat = imread(str);
	return mOcvMat.data != NULL;
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

const struct qcsc_info* MatFrmSrc::GetColorSpace(CString &pathName,
		struct qcsc_info* sortedCscInfo)
{
	String str = CT2A(pathName.GetString());
	Mat ocvMat = imread(str);
	if (ocvMat.data == NULL)
		return NULL;

	ocvMat.release();
	return qimage_find_cs(sortedCscInfo, "bgr888");
}

bool MatFrmSrc::IsAvailable()
{
	return mOcvMat.data != NULL;
}

bool MatFrmSrc::FillSceneBuf(BYTE* origBuf, long frameID)
{
	memcpy(origBuf, mOcvMat.data, mPane->origSceneSize);

	return true;
}
