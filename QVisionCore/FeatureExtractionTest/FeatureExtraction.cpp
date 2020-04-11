#include <stdio.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <QCommon.h>
#include <QFeatureExtraction.h>
#include <QDerivative.h>

using namespace std;
using namespace cv;

int main()
{
	Mat img = imread("../../DataSet/01_Edge/peppers.png");

	q1::FeatureExtractor fe;
	vector<Mat> chns, chnsReg, chnsSim;

	const int mImWidth = 32;
	Mat padded, padded3f;
	int h = img.rows;
	int w = img.cols;
	int p = mImWidth / 2;
	int t = p;
	int b = ROUNDUP_DWORD(h + mImWidth) - p - h;
	int l = p;
	int r = ROUNDUP_DWORD(w + mImWidth) - p - w;
	copyMakeBorder(img, padded, t, b, l, r, BORDER_REFLECT);

	padded.convertTo(padded3f, DataType<float>::type, 1/255.f);
	fe.getFeatures(padded3f, chns);
	fe.getSmoothFtrs(chns, chnsReg, chnsSim);

	return 0;
}
