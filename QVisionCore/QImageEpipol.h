// Copyright (C) 2014 Kyuwon Kim <chammoru@gmail.com>

#ifndef __QIMAGE_EPIPOL_
#define __QIMAGE_EPIPOL_

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/calib3d/calib3d.hpp>

cv::Mat qGetFundamentalMat(cv::Mat& image1, cv::Mat& image2);
void qGetEpilines(std::vector<cv::Point2f> &pts, bool isLeft,
				  cv::Mat &fundamental, std::vector<cv::Vec3f> &lines);
void qDecomposeEssentialMat(cv::Mat &E, cv::Mat &R1, cv::Mat &R2, cv::Mat &t);
void qTriangulatePoints(cv::Mat &projMatr1, cv::Mat &projMatr2,
						cv::Mat &projPoints1, cv::Mat &projPoints2,
						cv::Mat &points4D);
cv::Mat qGetProjectionMat(cv::Mat &R1, cv::Mat &R2, cv::Mat &t,
				   std::vector<cv::Point2d> &points1, std::vector<cv::Point2d> &points2);
cv::Mat qFindFundamentalMat8Point(cv::Mat &points1, cv::Mat &points2);
int qRun8Point(const cv::Mat &_m1, const cv::Mat &_m2, cv::Mat &F);

#endif /* __QIMAGE_EPIPOL_ */
