#pragma once

#include <string>

namespace q1 {

// Reads the cardinal clockwise display rotation stored in an MP4/MOV video
// track's tkhd matrix. Returns false for files without a usable video matrix.
bool ReadVideoDisplayRotation(const std::wstring& path, int& clockwiseDegrees);

} // namespace q1
