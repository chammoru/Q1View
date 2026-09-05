#pragma once

#include <cstdint>

namespace q1 {

void PrepareOpenCvHlgToSdrLut();

// Corrects OpenCV 4.3's 8-bit BT.601 interpretation of a studio-range
// BT.2020 HLG frame into tone-mapped SDR BT.709 BGR. Source and destination
// may be the same buffer; disjoint row ranges may be processed concurrently.
void ToneMapOpenCvHlgBgrRows(const std::uint8_t* source,
	int sourceStridePixels, std::uint8_t* destination,
	int destinationStridePixels, int width, int rowBegin, int rowEnd);

// Fuses tone mapping and cardinal rotation so portrait HDR playback does not
// write and reread a full intermediate corrected frame. One tile index covers
// eight adjacent source columns; callers may process tile ranges concurrently.
void ToneMapOpenCvHlgBgrRotateTiles(const std::uint8_t* source,
	int sourceWidth, int sourceHeight, std::uint8_t* destination,
	int destinationStridePixels, int clockwiseDegrees,
	int tileBegin, int tileEnd);

} // namespace q1
