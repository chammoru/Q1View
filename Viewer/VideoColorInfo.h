#pragma once

#include <string>

namespace q1 {

// Returns true only for the precise HDR format whose OpenCV 4.3 conversion
// needs Q1View's corrective display transform: studio-range BT.2020 NCL HLG.
bool IsBt2020HlgVideo(const std::wstring& path);

} // namespace q1
