#ifndef __CVUTIL_H__
#define __CVUTIL_H__

#include <opencv2/core/core.hpp>

namespace q1 {

bool matWrite(const std::string &filename, const cv::Mat &M);
bool matRead(const std::string &filename, cv::Mat &M);

double calIoU(const cv::Vec4i &bb, const cv::Vec4i &bbgt);
void putTxtW(cv::Mat &img, const std::string &msg, const cv::Point &loc,
			  float scale, int oTick, int iTick);

#define QCV_BOX_LINE5 5
#define QCV_BOX_LINE3 3
#define QCV_BOX_LINE1 1

static inline cv::Rect vec4i2Rect(const cv::Vec4i &bb)
{
	return cv::Rect(cv::Point(bb[0], bb[1]), cv::Point(bb[2] + 1, bb[3] + 1));
}

static inline cv::Vec4i vec4ishift(const cv::Vec4i &bb, const cv::Point2i &off)
{
	return 	cv::Vec4i(bb[0] + off.x, bb[1] + off.y, bb[2] + off.x, bb[3] + off.y);
}

void fillBox(cv::Mat &img, const cv::Scalar &color, const cv::Rect &rt, bool blending);
void drawBox(cv::Mat &img, const cv::Scalar &color, const cv::Rect &rt,
			  int nLines = QCV_BOX_LINE5, bool blending = false);
void drawBox(cv::Mat &img, const cv::Scalar &color, const cv::Vec4i &bb,
			  int nLines = QCV_BOX_LINE5, bool blending = false);

// for transparent png
cv::Mat createTransImg(const cv::Mat &orig, const cv::Mat &mask);

/*!
 * From OpenCV (structued_edge_detection.cpp)
 * The function filters src with triangle filter with radius equal rad
 *
 * \param src : source image to filter
 * \param rad : radius of filtering kernel
 * \return filtering result
 */
void convTri(const cv::Mat &src, cv::Mat &dst, const float rad);
cv::Mat convTri(const cv::Mat &src, const float rad);

cv::Mat rotate_aligned3b_090(const cv::Mat &src);
cv::Mat rotate_aligned3b_180(const cv::Mat &src);
cv::Mat rotate_aligned3b_270(const cv::Mat &src);

} // namespace q1

#endif /* __CVUTIL_H__ */
