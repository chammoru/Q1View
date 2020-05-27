#include <opencv2/imgproc/imgproc.hpp>

#include "QDerivative.h"

using namespace cv;

namespace q1 {

void fastGradient32Sx2(const Mat &bgr3u, Mat &Gx, Mat &Gy)
{
	CV_Assert(bgr3u.depth() == CV_8U);
	const int h = bgr3u.rows, w = bgr3u.cols;
	int cn = CV_MAT_CN(bgr3u.type());
	int dtype = CV_MAKE_TYPE(CV_32S, cn);
	int stride = w * cn;
	Gx.create(h, w, dtype);
	Gy.create(h, w, dtype);

	// Left/right most column Gx
	for (int y = 0; y < h; y++) {
		int *dstRowL = Gx.ptr<int>(y);
		int *dstRowR = Gx.ptr<int>(y) + (w - 1) * cn;
		const uchar *srcRowL = bgr3u.ptr<uchar>(y);
		const uchar *srcRowR = bgr3u.ptr<uchar>(y) + (w - 1) * cn;
		for (int c = 0; c < cn; c++) {
			dstRowL[c] = ((srcRowL + cn)[c] - (srcRowL     )[c]) * 2;
			dstRowR[c] = ((srcRowR     )[c] - (srcRowR - cn)[c]) * 2;
		}
	}
	// Find the gradient for inner regions
	for (int y = 0; y < h; y++) {
		int *dstRow = Gx.ptr<int>(y) + cn;
		const uchar *srcRow = bgr3u.ptr<uchar>(y) + cn;
		for (int j = 0; j < stride - 2 * cn; j++) {
			*dstRow++ = *(srcRow + cn) - *(srcRow - cn);
			srcRow++;
		}
	}

	// Top/bottom most column Gy
	int *dstRowT = Gy.ptr<int>(0);
	int *dstRowB = Gy.ptr<int>(h - 1);
	const uchar *srcRowT = bgr3u.ptr<uchar>(0);
	const uchar *srcRowB = bgr3u.ptr<uchar>(h - 1);
	for (int j = 0; j < stride; j++)	{
		*dstRowT++ = (*(srcRowT + stride) - *srcRowT) * 2;
		*dstRowB++ = (*srcRowB - *(srcRowB - stride)) * 2;
		srcRowT++;
		srcRowB++;
	}
	// Find the gradient for inner regions
	for (int y = 1; y < h-1; y++) {
		int *dstRow = Gy.ptr<int>(y);
		const uchar *srcRowU = bgr3u.ptr<uchar>(y - 1);
		const uchar *srcRowL = bgr3u.ptr<uchar>(y + 1);
		for (int j = 0; j < stride; j++) {
			*dstRow++ = *srcRowL++ - *srcRowU++;
		}
	}
}

void gradient(const Mat &src, Mat &Gx, Mat &Gy, int ddepth, int diffType)
{
	int sdepth = src.depth();
	int cn = CV_MAT_CN(src.type());
	int dtype = CV_MAKE_TYPE(ddepth, cn);
	CV_Assert(diffType == QCV_DIFF_CENTRAL || diffType == QCV_DIFF_FORWARD);
	CV_Assert(ddepth >= sdepth);
	Mat kernelX, kernelY;
	Point anchor;
	if (diffType == QCV_DIFF_CENTRAL) {
		kernelX = (Mat_<float>(1, 3) << -1.f, 0.f, 1.f);
		kernelY = (Mat_<float>(3, 1) << -1.f, 0.f, 1.f);
		anchor = Point(-1, -1);
	} else { // QCV_DIFF_FORWARD
		kernelX = (Mat_<float>(1, 2) << -1.f, 1.f);
		kernelY = (Mat_<float>(2, 1) << -1.f, 1.f);
		anchor = Point(0, 0);
	}

	Mat _src;
	if (sdepth == CV_32F && ddepth == CV_64F)
		src.convertTo(_src, CV_64F);
	else
		_src = src;

	filter2D(_src, Gx, ddepth, kernelX, anchor, 0, BORDER_REFLECT);
	filter2D(_src, Gy, ddepth, kernelY, anchor, 0, BORDER_REFLECT);

	if (diffType == QCV_DIFF_CENTRAL) {
		const int h = src.rows, w = src.cols;
		const Rect xCenter(1, 0, w - 2, h);
		Gx(xCenter) /= 2;
		const Rect yCenter(0, 1, w, h - 2);
		Gy(yCenter) /= 2;
	} 
}

Mat laplacian(const Mat &Gx, const Mat &Gy, int ddepth)
{
	CV_Assert(Gx.type() == Gy.type());
	int cn = CV_MAT_CN(Gx.type());
	int dtype = CV_MAKE_TYPE(ddepth, cn);
	Mat Gxx, Gyy;
	const Mat kernelX = (Mat_<float>(1, 2) << -1.f, 1.f);
	const Mat kernelY = (Mat_<float>(2, 1) << -1.f, 1.f);
	filter2D(Gx, Gxx, ddepth, kernelX, Point(-1, -1), 0, BORDER_REFLECT); // backward
	filter2D(Gy, Gyy, ddepth, kernelY, Point(-1, -1), 0, BORDER_REFLECT); // backward
	return Gxx + Gyy;
}

} // namespace q1
