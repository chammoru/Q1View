#include <string>
#include <iostream>
#include "opencv2/highgui/highgui.hpp"

#include "QColorName.h"

using namespace std;
using namespace cv;

int main( int argc, char** argv)
{
    q1::ColorName cn("../../ExtraData/ColorName/w2c_model.xml");
	argv[1] = "opp_color_circle.tif";

    Mat input = imread(argv[1]);
    Mat output = cn.getColorName(input);
	Mat actual = cn.cvt2Actual(output);
    imshow("output", actual);
    waitKey();

    return 0;
}
