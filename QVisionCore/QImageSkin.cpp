/*
 * Copyright (c) 2011. Philipp Wagner <bytefish[at]gmx[dot]de>.
 * Released to public domain under terms of the BSD Simplified license.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of the organization nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 *   See <http:www.opensource.org/licenses/bsd-license>
 */

// http://bytefish.de/blog/opencv/skin_color_thresholding/

#include "QImageSkin.h"

using namespace cv;

static inline bool R1(int R, int G, int B)
{
	bool e1 = (R > 95) && (G > 40) && (B > 20) &&
		((max(R, max(G, B)) - min(R, min(G, B))) > 15) &&
		(abs(R - G) > 15) && (R > G) && (R > B);
	bool e2 = (R > 220) && (G > 210) && (B > 170) &&
		(abs(R - G) <= 15) && (R > B) && (G > B);
	return e1 || e2;
}

static inline bool R2(uchar Y, uchar Cr, uchar Cb)
{
	bool e3 = Cr <= 1.5862 * Cb + 20;
	bool e4 = Cr >= 0.3448 * Cb + 76.2069;
	bool e5 = Cr >= -4.5652 * Cb + 234.5652;
	bool e6 = Cr <= -1.15 * Cb + 301.75;
	bool e7 = Cr <= -2.2857 * Cb + 432.85;
	return e3 && e4 && e5 && e6 && e7;
}

static inline bool R3(float H, float S, float V)
{
	return (H < 25) || (H > 230);
}

Mat qGetSkin(const Mat &src)
{
	// allocate the result matrix
	Mat dst = src.clone();

	const Vec3b cblack = Vec3b::all(0);

	Mat src_ycrcb, src_hsv;
	// OpenCV scales the YCrCb components, so that they
	// cover the whole value range of [0,255], so there's
	// no need to scale the values:
	cvtColor(src, src_ycrcb, COLOR_BGR2YCrCb);
	// OpenCV scales the Hue Channel to [0,180] for
	// 8bit images, so make sure we are operating on
	// the full spectrum from [0,360] by using floating
	// point precision:
	src.convertTo(src_hsv, CV_32FC3);
	cvtColor(src_hsv, src_hsv, COLOR_BGR2HSV);
	// Now scale the values between [0,255]:
	normalize(src_hsv, src_hsv, 0.0, 255.0, NORM_MINMAX, CV_32FC3);

	for (int i = 0; i < src.rows; i++) {
		for (int j = 0; j < src.cols; j++) {

			Vec3b pix_bgr = src.ptr<Vec3b>(i)[j];
			int B = pix_bgr.val[0];
			int G = pix_bgr.val[1];
			int R = pix_bgr.val[2];
			// apply rgb rule
			bool a = R1(R, G, B);

			Vec3b pix_ycrcb = src_ycrcb.ptr<Vec3b>(i)[j];
			uchar Y = pix_ycrcb.val[0];
			uchar Cr = pix_ycrcb.val[1];
			uchar Cb = pix_ycrcb.val[2];
			// apply ycrcb rule
			bool b = R2(Y, Cr, Cb);

			Vec3f pix_hsv = src_hsv.ptr<Vec3f>(i)[j];
			float H = pix_hsv.val[0];
			float S = pix_hsv.val[1];
			float V = pix_hsv.val[2];
			// apply hsv rule
			bool c = R3(H, S, V);

			if (!(a && b && c))
				dst.ptr<Vec3b>(i)[j] = cblack;
		}
	}

	return dst;
}

Mat qGetSkinBin(const Mat &src) // Binary
{
	// allocate the result matrix
	Mat dst(src.size(), CV_8U, Scalar(0));

	const uchar cwhite = 255;

	Mat src_ycrcb, src_hsv;
	// OpenCV scales the YCrCb components, so that they
	// cover the whole value range of [0,255], so there's
	// no need to scale the values:
	cvtColor(src, src_ycrcb, COLOR_BGR2YCrCb);
	// OpenCV scales the Hue Channel to [0,180] for
	// 8bit images, so make sure we are operating on
	// the full spectrum from [0,360] by using floating
	// point precision:
	src.convertTo(src_hsv, CV_32FC3);
	cvtColor(src_hsv, src_hsv, COLOR_BGR2HSV);
	// Now scale the values between [0,255]:
	normalize(src_hsv, src_hsv, 0.0, 255.0, NORM_MINMAX, CV_32FC3);

	for (int i = 0; i < src.rows; i++) {
		for (int j = 0; j < src.cols; j++) {

			Vec3b pix_bgr = src.ptr<Vec3b>(i)[j];
			int B = pix_bgr.val[0];
			int G = pix_bgr.val[1];
			int R = pix_bgr.val[2];
			// apply rgb rule
			bool a = R1(R, G, B);

			Vec3b pix_ycrcb = src_ycrcb.ptr<Vec3b>(i)[j];
			uchar Y = pix_ycrcb.val[0];
			uchar Cr = pix_ycrcb.val[1];
			uchar Cb = pix_ycrcb.val[2];
			// apply ycrcb rule
			bool b = R2(Y, Cr, Cb);

			Vec3f pix_hsv = src_hsv.ptr<Vec3f>(i)[j];
			float H = pix_hsv.val[0];
			float S = pix_hsv.val[1];
			float V = pix_hsv.val[2];
			// apply hsv rule
			bool c = R3(H, S, V);

			if (a && b && c)
				dst.ptr<uchar>(i)[j] = cwhite;
		}
	}

	return dst;
}
