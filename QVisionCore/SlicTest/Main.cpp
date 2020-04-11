#include "opencv2/highgui/highgui.hpp"

#include <QSlicWrapper.h>
#include <QSuperPixel.h>

using namespace std;
using namespace cv;

int main(int argc, char **argv)
{
	Mat label;
    Mat input = imread("opp_color_circle_256x256.png");
	imshow("input", input);

	int numlabels = qDoSlicSuperPixel(input, label, 100, 20);
	std::vector<cv::Point2d> centroid;
	qSpGetCentroid(label, numlabels, centroid);
	qDisplaySlicSuperPixel(input, label, &centroid);
	imshow("output", input);

    waitKey();

    return 0;
}
