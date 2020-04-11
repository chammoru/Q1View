#pragma once

#include <opencv2/core/core.hpp>

namespace q1 {

#define QCV_DIFF_CENTRAL 1
#define QCV_DIFF_FORWARD 2

void gradient(const cv::Mat &src, cv::Mat &Gx, cv::Mat &Gy, int ddepth, int diffType);
cv::Mat laplacian(const cv::Mat &Gx, const cv::Mat &Gy, int ddepth);

// fast gradient from CV_8UCn -> CV_16SCn
void fastGradient32Sx2(const cv::Mat &bgr3u, cv::Mat &Gx, cv::Mat &Gy);

}
