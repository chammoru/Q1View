#pragma once

#include <opencv2/core/core.hpp>

#include <string>
#include <vector>

namespace q1 {

bool isHeifBuffer(const std::vector<uchar> &data);
bool isHeifFileW(const std::wstring &filename);
cv::Mat imdecodeHeif(const std::vector<uchar> &data, int flags);

} // namespace q1
