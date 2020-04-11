#pragma once

#include <set>
#include <vector>
#include <opencv2/core/core.hpp>

static const int qDx8[] = {-1, -1, 0, 1, 1, 1, 0, -1};
static const int qDy8[] = {0, -1, -1, -1, 0, 1, 1, 1};

static const int qDx4[] = {-1, 0, 1, 0};
static const int qDy4[] = {0, -1, 0, 1};

void qSpSetLabel(cv::Mat &seg, cv::Point startPt, int nl /*new label*/);
void qSpGet4NgbsBi(const cv::Mat &labelMap, std::vector<std::set<int> > &neighbors);
void qSpGet4NgbsUni(const cv::Mat &labelMap, std::vector<std::set<int> > &neighbors);
void qSpGet8NgbsBi(const cv::Mat &labelMap, std::vector<std::set<int> > &neighbors);
void qSpGet8NgbsUni(const cv::Mat &labelMap, std::vector<std::set<int> > &neighbors);
int qSpNormLabel(cv::Mat &seg, size_t maxLabel);
int qSpSegment(const cv::Mat &lab, cv::Mat &labelMap, int nLabels, float c, int min_size);
void qSp2Box(const cv::Mat &labelMap, size_t nLabels, int w, int h, std::vector<cv::Vec4i> &boxes);
void qSpGetCentroid(const cv::Mat &labelMap, size_t nLabels, std::vector<cv::Point2d> &centroid);
