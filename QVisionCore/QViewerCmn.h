#pragma once

#include <vector>
#include <QCommon.h>
#include <opencv2/core/core.hpp>

namespace q1 {

#define ZOOM_GAMMA         4.f
#define ZOOM_RATIO(_D)     exp((_D) / ZOOM_GAMMA)
#define ZOOM_DELTA(_N)     (ZOOM_GAMMA * log(_N))
#define ZOOM_MAX           100.f
#define ZOOM_GRID_START    16.f
#define ZOOM_TEXT_START    42.f

#define COLOR_PIXEL_TEXT   RGB(0xe0, 0xe0, 0xe0)

#define QIMG_MAX_LENGTH    10000

int DeterminDestPos(int lenCanvas, int lenDst, float &offset, float ratio);
float GetFitRatio(float ratio, int w, int h, int wCanvas, int hCanvas);
float GetBestFitRatio(int w, int h, int wCanvas, int hCanvas);
void InvestigatePixelBorder(qu16 *nOffsetBuf,
							int start, int end, int base, int nDst,
							int *gridDim, std::vector<int> *cellCounts,
							qu8 *nOffsetBorderFlag);
void ScaleUsingOffset(qu8 *src, int yStart, int yEnd, int xStart, int xEnd, int stride, int gap,
					  qu16 * nOffsetBuf, qu8 *dst);
void ScaleUsingOffset(qu8 *src, int yStart, int yEnd, int xStart, int xEnd, int stride, int gap,
					  qu8 *nOffsetYBorderFlag, qu8 *nOffsetXBorderFlag,
					  qu16 * nOffsetBuf, qu8 *dst);
float GetNextN(float curN, float fitN, float nextD);

// JUST ADD HERE, IF YOU WANT MORE RESOLUTION, THIS'S IT!
static const char *resolution_info_table[] = {
	"176x144 (QCIF)",
	"320x240 (QVGA)",
	"352x288 (CIF)",
	"640x480 (VGA)",
	"720x480",
	"800x600 (SVGA)",
	"960x540",
	"1024x600 (WSVGA)",
	"1024x768 (XGA)",
	"1280x720 (HD)",
	"1920x1080 (FHD)",
	"2560x1600 (WQXGA)",
	"3840x2160 (4K UHD)",
	"4096x2160 (Cinema UHD)",

	"C&ustom...",
};

struct GridInfo
{
	int y, x;
	std::vector<int> Hs;
	std::vector<int> Ws;
	cv::Mat pixelMap;
};

} // namespace q1
