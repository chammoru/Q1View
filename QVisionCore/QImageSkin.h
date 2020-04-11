#ifndef __QIMAGE_SKIN__
#define __QIMAGE_SKIN__

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>

cv::Mat qGetSkin(const cv::Mat &src);
cv::Mat qGetSkinBin(const cv::Mat &src); // Binary

#endif /* __QIMAGE_SKIN__ */
