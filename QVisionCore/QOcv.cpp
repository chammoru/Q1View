// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#include "QDebug.h"
#include "QTickMeter.h"
#include "QOcv.h"
#include "QDerivative.h"

#include <opencv2/objdetect/objdetect.hpp>

namespace q1 {

bool SomeDetector::process(cv::Mat &src, cv::Mat &dst)
{
	src.copyTo(dst);

	char buf[256];
	cv::Point pt(10, 20);
	sprintf(buf, "Hello world");
	cv::putText(dst, buf, pt, cv::FONT_HERSHEY_SIMPLEX, 4.0, cv::Scalar::all(0x00));

	return true;
}

} // namespace q1