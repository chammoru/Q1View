#pragma once

#include "stdafx.h"
#include "ViewerDoc.h"
#include <QOcv.h>
#include <QCvUtil.h>

static inline BufferInfo PostProcess(
	int colorSpace, q1::ImageProcessor *bgr888Processor,
	int w, int h,
	BYTE *src, BYTE *dst,
	int bufOffset2, int bufOffset3,
	QROTATION rot, QIMAGE_CSC_FN csc2Rgb888,
	long frameID)
{
	if (colorSpace == QIMAGE_CS_BGR888 && bgr888Processor) {
		cv::Mat src(h, w, CV_8UC3, src, w * QIMG_DST_RGB_BYTES);
		cv::Mat dst(h, w, CV_8UC3, dst, ROUNDUP_DWORD(w) * QIMG_DST_RGB_BYTES);
		bgr888Processor->process(src, dst);
	} else {
		BYTE *y = src;
		BYTE *u = src + bufOffset2;
		BYTE *v = src + bufOffset3;
		csc2Rgb888(dst, y, u, v, ROUNDUP_DWORD(w), w, h);
	}

	cv::Mat rgb(h, w, CV_8UC3, dst, ROUNDUP_DWORD(w) * QIMG_DST_RGB_BYTES);
	cv::Mat rotated;
	switch (rot) {
	case QROT_000:
		break;
	case QROT_090:
		rotated = q1::rotate_aligned3b_090(rgb);
		memcpy(dst, rotated.ptr(), ROUNDUP_DWORD(h) * w * QIMG_DST_RGB_BYTES);
		break;
	case QROT_180:
		rotated = q1::rotate_aligned3b_180(rgb);
		memcpy(dst, rotated.ptr(), h * ROUNDUP_DWORD(w) * QIMG_DST_RGB_BYTES);
		break;
	case QROT_270:
		rotated = q1::rotate_aligned3b_270(rgb);
		memcpy(dst, rotated.ptr(), ROUNDUP_DWORD(h) * w * QIMG_DST_RGB_BYTES);
		break;
	}

	BufferInfo bi;
	bi.ID = frameID;
	bi.addr = dst;

	return bi;
}
