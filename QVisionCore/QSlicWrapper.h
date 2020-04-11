#pragma once

#include <vector>
#include <opencv2/core/core.hpp>
#include <QSuperPixel.h>

void qDisplaySlicSuperPixel(const cv::Mat &image, cv::Mat &labelMap, std::vector<cv::Point2d> *centroid);
int qDoSlicSuperPixel(const cv::Mat &image, cv::Mat &labelMap, const int &k,
					  const double &compactness, bool labInput = false);
