#pragma once

#include <opencv2/core/core.hpp>

namespace q1 {

#define QCV_ROT_090 1
#define QCV_ROT_180 2
#define QCV_ROT_270 3

cv::Mat rotate(const cv::Mat &img, int degree);

} // namespace q1
