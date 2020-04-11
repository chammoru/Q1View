#pragma once

#include "qimage_cs.h"

class CComparerViewC;

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

struct SQPane
{
	CFile file;
	cv::Mat mOcvMat;
	size_t origSceneSize;
	size_t origBufSize;
	BYTE *origBuf;
	QIMAGE_CS colorSpace;
	QIMAGE_CS_INFO_FN csLoadInfo;
	QIMAGE_CSC_FN csc2yuv420;
	BYTE *rgbBuf;
	BYTE *yuv420Buf;
	size_t rgbBufSize;
	size_t yuv420BufSize;
	QIMAGE_CSC_FN csc2rgb888;

	SQPane()
	: origSceneSize(0)
	, origBufSize(0)
	, origBuf(NULL)
	, colorSpace(qcsc_info_table[QIMG_DEF_CS_IDX].cs)
	, csLoadInfo(qcsc_info_table[QIMG_DEF_CS_IDX].cs_load_info)
	, csc2yuv420(NULL)
	, rgbBuf(NULL)
	, yuv420Buf(NULL)
	, rgbBufSize(0)
	, yuv420BufSize(0)
	, csc2rgb888(qcsc_info_table[QIMG_DEF_CS_IDX].csc2rgb888)
	{}

	virtual ~SQPane()
	{
		if (origBuf)
			delete [] origBuf;

		if (rgbBuf)
			delete [] rgbBuf;

		if (yuv420Buf)
			delete [] yuv420Buf;
	}

	void SetColorInfo(const struct qcsc_info * const ci)
	{
		colorSpace = ci->cs;
		csc2rgb888 = ci->csc2rgb888;
		csLoadInfo = ci->cs_load_info;

		if ((ci->cs & QIMAGE_CS_YUV) && ci->csc2yuv420)
			csc2yuv420 = ci->csc2yuv420;
		else
			csc2yuv420 = NULL;
	}

	inline bool isOcvMat() const { return mOcvMat.data != NULL; }
};

struct ComparerPane : public SQPane
{
	CString pathName;
	int bufOffset2, bufOffset3;
	CComparerViewC *pView;

	ULONGLONG fileSize;
	long frames;
	long curFrameID;

	virtual ~ComparerPane() {}

	ComparerPane()
	: pathName(_T(""))
	, bufOffset2(0)
	, bufOffset3(0)
	, pView(NULL)
	, fileSize(0L)
	, frames(0)
	, curFrameID(0)
	{}

	inline bool isAvail() const { return fileSize > 0 || mOcvMat.data; }
};
