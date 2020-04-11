#include "QRotate.h"

using namespace cv;

namespace q1 {

Mat rotate(const Mat &src, int degree)
{
	Mat dst;
	int srcH = src.rows;
	int srcW = src.cols;
	Mat temp;
	switch (degree) {
	case QCV_ROT_090:
		dst.create(srcW, srcH, src.type());
		for (int i = 0; i < srcH; i++)
			transpose(src.row(i), dst.col(srcH - i - 1));
		break;
	case QCV_ROT_180:
		flip(src, dst, -1);
		break;
	case QCV_ROT_270:
        transpose(src, dst);
        flip(dst, dst, 0);
		break;
	default:
		CV_Error(Error::StsNotImplemented, "never get here");
	}
	return dst;
}

}
