#include <opencv2/imgproc/imgproc.hpp>
#include <QCommon.h>
#include "QCvUtil.h"

using namespace std;
using namespace cv;

namespace q1 {

bool matWrite(const string &filename, const Mat &M)
{
	Mat _M = M.isContinuous() ? M : M.clone();
	FILE *file = fopen(filename.c_str(), "wb");
	if (file == NULL || _M.empty())
		return false;
	fwrite("Q1Mat", sizeof(char), 5, file);
	int headData[3] = {_M.cols, _M.rows, _M.type()};
	fwrite(headData, sizeof(int), 3, file);
	const uchar *data = _M.data;
	for (int i = 0; i < _M.rows; i++) { // gradual writing is safer
		fwrite(data, sizeof(uchar), _M.step, file);
		data += _M.step;
	}
	fclose(file);
	return true;
}

bool matRead(const string &filename, Mat &M)
{
	FILE *f = fopen(filename.c_str(), "rb");
	if (f == NULL)
		return false;
	char buf[8];
	size_t read = fread(buf, sizeof(char), 5, f);
	CV_Assert(read == 5);
	if (strncmp(buf, "Q1Mat", 5) != 0 && strncmp(buf, "CmMat", 5))	{
		printf("Invalidate cv::Mat data file %s\n", filename.c_str());
		return false;
	}
	int headData[3]; // Width, height, type
	read = fread(headData, sizeof(int), 3, f);
	CV_Assert(read == 3);
	M.create(headData[1], headData[0], headData[2]);
	uchar *data = M.data;
	for (int i = 0; i < M.rows; i++) {
		read = fread(data, sizeof(char), M.step, f);
		data += M.step;
	}
	fclose(f);
	return true;
}

double calIoU(const Vec4i &bb, const Vec4i &bbgt)
{
	int bi[4];
	bi[0] = MAX(bb[0], bbgt[0]);
	bi[1] = MAX(bb[1], bbgt[1]);
	bi[2] = MIN(bb[2], bbgt[2]);
	bi[3] = MIN(bb[3], bbgt[3]);

	double iw = bi[2] - bi[0] + 1;
	double ih = bi[3] - bi[1] + 1;

	if (iw <= 0 || ih <= 0)
		return 0;

	double ua = (bb[2] - bb[0] + 1) * (bb[3] - bb[1] + 1)
		+ (bbgt[2] - bbgt[0] + 1) * (bbgt[3] - bbgt[1] + 1)
		- iw * ih;

	return iw * ih / ua;
}

void putTxtW(Mat &img, const string &msg, const Point &loc,
					  float scale, int oTick, int iTick)
{
	putText(img, msg, loc, FONT_HERSHEY_SIMPLEX, scale, Scalar::all(0x00), oTick, LINE_AA);
	putText(img, msg, loc, FONT_HERSHEY_SIMPLEX, scale, Scalar::all(0xff), iTick, LINE_AA);
}

// for transparent png
Mat createTransImg(const Mat &orig, const Mat &mask)
{
	Mat alpha, trans;
	threshold(mask, alpha, 0, 255, THRESH_BINARY);

	Mat rgb[3];
	split(orig, rgb);
	Mat rgba[4] = {rgb[0], rgb[1], rgb[2], alpha};
	merge(rgba, 4, trans);
	return trans;
}

void fillBox(Mat &img, const Scalar &color, const Rect &rt, bool blending)
{
	Mat cloned;
	if (blending)
		cloned = img.clone();

	rectangle(img, rt, color, FILLED);

	if (blending)
		addWeighted(img, 0.5, cloned, 0.5, 0, img);
}

void drawBox(Mat &img, const Scalar &color, const Rect &rt, int nLines, bool blending)
{
	CV_Assert(nLines >= QCV_BOX_LINE1 || nLines <= QCV_BOX_LINE5);

	Mat cloned;
	if (blending)
		cloned = img.clone();

	switch (nLines) {
	case QCV_BOX_LINE5:
	{
		Rect rt5O(rt.x - 2, rt.y - 2, rt.width + 4, rt.height + 4);
		Rect rt5I(rt.x + 2, rt.y + 2, rt.width - 4, rt.height - 4);
		rectangle(img, rt5O, color, 1);
		rectangle(img, rt5I, color, 1);
		// fall through
	}
	case QCV_BOX_LINE3:
	{
		Rect rt3O(rt.x - 1, rt.y - 1, rt.width + 2, rt.height + 2);
		Rect rt3I(rt.x + 1, rt.y + 1, rt.width - 2, rt.height - 2);
		rectangle(img, rt3O, color, 1);
		rectangle(img, rt3I, color, 1);
		// fall through
	}
	case QCV_BOX_LINE1:
		rectangle(img, rt, color                   , 1);
		break;
	}

	if (blending)
		addWeighted(img, 0.5, cloned, 0.5, 0, img);
}

void drawBox(Mat &img, const Scalar &color, const Vec4i &bb, int nLines, bool blending)
{
	drawBox(img, color, vec4i2Rect(bb), nLines, blending);
}

/*!
 * From OpenCV (structued_edge_detection.cpp)
 * The function filters src with triangle filter with radius equal rad
 *
 * \param src : source image to filter
 * \param rad : radius of filtering kernel
 * \return filtering result
 */
void convTri(const Mat &src, Mat &dst, const float rad)
{
	if (rad == 0.f) {
		src.copyTo(dst);
	} else {
		vector<float> kernelXY;
		if (rad <= 1) {
			const float p = 12.0f / rad / (rad + 2) - 2;
			kernelXY.resize(3);
			kernelXY[0] = kernelXY[2] = 1 / (p + 2);
			kernelXY[1] = p / (p + 2);
		} else {
			int iRad = (int)rad;
			float nrml = QSQR(rad + 1.0f);
			kernelXY.resize(2*iRad + 1);
			for (int i = 0; i < rad; i++)
				kernelXY[i] = kernelXY[2*iRad - i] = (i + 1) / nrml;
			kernelXY[iRad] = (rad + 1) / nrml;
		}
		Mat _src = src.isContinuous() ? src : src.clone();
		sepFilter2D(_src, dst, -1, kernelXY, kernelXY, Point(-1, -1), 0, BORDER_REFLECT);
	}
}

Mat rotate_aligned3b_090(const Mat &src)
{
	if (src.empty())
		return Mat();
	int srcH = src.rows;
	int srcW = src.cols;
	Mat dst_aligned4(srcW, ROUNDUP_DWORD(srcH), src.type());

	for (int y = 0; y < srcH; y++) {
		for (int x = 0; x < srcW; x++) {
			dst_aligned4.at<Vec3b>(x, srcH - y - 1) = src.at<Vec3b>(y, x);
		}
	}
	return dst_aligned4;
}

Mat rotate_aligned3b_180(const Mat &src)
{
	if (src.empty())
		return Mat();
	int srcH = src.rows;
	int srcW = src.cols;
	Mat dst_aligned(srcH, ROUNDUP_DWORD(srcW), src.type());

	for (int y = 0; y < srcH; y++) {
		for (int x = 0; x < srcW; x++) {
			dst_aligned.at<Vec3b>(srcH - y - 1, srcW - x - 1) = src.at<Vec3b>(y, x);
		}
	}
	return dst_aligned;
}

Mat rotate_aligned3b_270(const Mat &src)
{
	if (src.empty())
		return Mat();
	int srcH = src.rows;
	int srcW = src.cols;
	Mat dst_aligned4(srcW, ROUNDUP_DWORD(srcH), src.type());

	for (int y = 0; y < srcH; y++) {
		for (int x = 0; x < srcW; x++) {
			dst_aligned4.at<Vec3b>(srcW - x - 1, y) = src.at<Vec3b>(y, x);
		}
	}
	return dst_aligned4;
}

}
