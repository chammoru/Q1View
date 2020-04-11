#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <QCommon.h>

#include "QSlicWrapper.h"
#include "SLIC.h"

using namespace std;
using namespace cv;

void qDisplaySlicSuperPixel(const Mat &image, Mat &labelMap, vector<Point2d> *centroid)
{
	Mat imageRgba;
	cvtColor(image, imageRgba, CV_BGR2BGRA);
	if (centroid) {
		for (size_t i = 0; i < (*centroid).size(); i++) {
			circle(imageRgba, (*centroid)[i], 2,
				Scalar(0x00, 0xff, 0x00, 0xff), FILLED);
		}
	}
	SLIC slic;
	slic.DrawContoursAroundSegments(imageRgba.ptr<unsigned int>(0), labelMap.ptr<int>(),
		imageRgba.cols, imageRgba.rows, 0);

	namedWindow("SLIC");
	imshow("SLIC", imageRgba);
}

int qDoSlicSuperPixel(const Mat &image, Mat &labelMap, const int &k, const double &compactness, bool labInput)
{
	labelMap.create(image.size(), CV_32S);
	int numlabels = 0;

	SLIC slic;
	slic.DoSuperpixelSegmentation_ForGivenNumberOfSuperpixels(image.ptr<uchar>(),
		image.cols, image.rows, labelMap.ptr<int>(),
		numlabels, k, compactness, labInput);

	return numlabels;
}
