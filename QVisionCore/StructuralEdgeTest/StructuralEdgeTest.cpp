#include <stdio.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

#include <QCvUtil.h>
#include <QStructuralEdge.h>

using namespace std;
using namespace cv;

int main()
{
	Mat img = imread("../../DataSet/01_Edge/peppers.png");
	q1::StructuralEdge se;
	se.setup("../../ExtraData/SED_Dollar/model.bin");
	Mat edges = se.detectEdges(img);
#if 0 // save cvmat for verifying
	q1::matWrite("ref.cvmat", edges);
#else // verify now
	Mat ref;
	q1::matRead("ref.cvmat", ref);
	Mat diff = edges - ref;
	CV_Assert(countNonZero(diff) == 0);
#endif
	imshow("edges", edges);
	waitKey();

	return 0;
}
