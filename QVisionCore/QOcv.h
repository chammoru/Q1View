// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#ifndef __QIMAGE_CV__
#define __QIMAGE_CV__

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

namespace q1 {

#define NUM_ORIENT_BIN 16

typedef cv::Vec<float, NUM_ORIENT_BIN> VecBNf; // Bin #

class ImageProcessor {
public:
	ImageProcessor() {}
	virtual ~ImageProcessor() {}
	virtual bool process(cv::Mat &src, cv::Mat &dst) = 0;
};

class SomeDetector : public q1::ImageProcessor
{
	bool process(cv::Mat &src, cv::Mat &dst);
};

} // namespace q1

#endif /* __QIMAGE_CV__ */
