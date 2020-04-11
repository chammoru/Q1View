#include <iostream>
#include <vector>

#include "QColorName.h"
#include "opencv2/highgui/highgui.hpp"

using namespace std;
using namespace cv;

namespace q1 {

ColorName::ColorName(const string &model)
: mModelValid(false)
{
    FileStorage fs(model, FileStorage::READ);
    if (!fs.isOpened()) {
        cout << "Can not load the model file " << model << endl;
        return;
    }

    fs["matrix"] >> mW2c;
    const int number_of_colors = 11;
    if (mW2c.empty() || mW2c.cols != number_of_colors) {
        cout << "Model's file invalid , cols -> " << number_of_colors << endl;
        return;
    }

    /* compute the index of the max value each row */
    getMaxIndex();

    /* prepare the RGB value for each color */
    mColors.push_back(Vec3b(0x00, 0x00, 0x00)); // black   (0)
    mColors.push_back(Vec3b(0xff, 0x00, 0x00)); // blue    (1)
    mColors.push_back(Vec3b(0x40, 0x66, 0x80)); // brown   (2)
    mColors.push_back(Vec3b(0x80, 0x80, 0x80)); // grey    (3)
    mColors.push_back(Vec3b(0x00, 0xff, 0x00)); // green   (4)
    mColors.push_back(Vec3b(0x00, 0xcc, 0xff)); // orange  (5)
    mColors.push_back(Vec3b(0xff, 0x80, 0xff)); // pink    (6)
    mColors.push_back(Vec3b(0xff, 0x00, 0xff)); // purple  (7)
    mColors.push_back(Vec3b(0x00, 0x00, 0xff)); // red     (8)
    mColors.push_back(Vec3b(0xff, 0xff, 0xff)); // white   (9)
    mColors.push_back(Vec3b(0x00, 0xff, 0xff)); // yellow (10)

    /* set valid */
    mModelValid = true;
}

bool ColorName::getMaxIndex()
{
    if (mW2c.empty())
        return false;

	mW2cMax.resize(mW2c.rows);

    for(int i = 0; i < mW2c.rows; i++) {
        int maxIdx = 0;
        double maxValue = -1;
        for (int j = 0; j < mW2c.cols;j++) {
            if (mW2c.at<double>(i, j) > maxValue) {
                maxValue = mW2c.at<double>(i, j);
                maxIdx = j;
            }
        }
        mW2cMax[i] = maxIdx;
    }

    return true;
}

Mat ColorName::getColorName(const Mat &src) const
	/*  in: type == 0 -> only return the color's type in dst( value ranges from 0 to 10)
    type == 1 -> further map the color's type to the real color value( good to visualize the result)*/

{
    /* since the output value's range is [0, 32767] , we can not use opencv's LUT() function */
    Mat labelImage(src.size(), CV_8U);
    for (int i = 0; i < labelImage.rows; i++) {
        const uchar *row_ptr = src.ptr<uchar>(i);
        for (int j = 0; j < labelImage.cols;j++) {
            int idx = ((*row_ptr >> 3) << 10) +
				      ((*(row_ptr + 1) >> 3) << 5) +
				      (*(row_ptr + 2) >> 3);
            labelImage.at<uchar>(i, j) = mW2cMax[idx];
            row_ptr += 3; /* cross BGR 3 pixels */
        }
    }

    return labelImage;
}

/*  convert the colorName to actual color */
Mat ColorName::cvt2Actual(const Mat &src)
{
    Mat dst(src.size(), CV_8UC3);
    for (int i = 0; i < src.rows; i++) {
        for(int j = 0; j < src.cols; j++)
            dst.at<Vec3b>(i,j) = mColors[src.at<uchar>(i, j)];
    }
	return dst;
}

Mat ColorName::getSameCnPos(const Mat &src, int cn) const
{
	Mat flagImage(src.size(), CV_8U);
	for (int i = 0; i < flagImage.rows; i++) {
		const uchar *row_ptr = src.ptr<uchar>(i);
		for (int j = 0; j < flagImage.cols; j++) {
			int idx = ((*row_ptr >> 3) << 10) +
						((*(row_ptr + 1) >> 3) << 5) +
						(*(row_ptr + 2) >> 3);
			flagImage.at<uchar>(i, j) = mW2cMax[idx] == cn;
			row_ptr += 3; /* cross BGR 3 pixels */
		}
	}

	return flagImage;
}

}
