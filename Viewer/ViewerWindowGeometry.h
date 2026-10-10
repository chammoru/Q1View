#pragma once

#include "../QVisionCore/Q1UiFontWin.h"

namespace q1view {

// Windows startup viewing area, in DIPs; not an image/RAW resolution preset.
constexpr int ViewerDefaultWindowWidthDip = 800;
constexpr int ViewerDefaultWindowHeightDip = 600;

inline SIZE ViewerDefaultContentSize(UINT dpi, int normalMenuWidth, SIZE available)
{
	const int wantedWidth = (std::max)(WindowsUiPixels(ViewerDefaultWindowWidthDip, dpi),
		normalMenuWidth + WindowsUiPixels(24, dpi));
	return {
		(std::max)(1L, (std::min)(LONG(wantedWidth), available.cx)),
		(std::max)(1L, (std::min)(LONG(WindowsUiPixels(ViewerDefaultWindowHeightDip, dpi)), available.cy))
	};
}

} // namespace q1view
