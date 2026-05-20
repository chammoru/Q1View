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

#define Q1UI_FONT_TEXT       _T("Segoe UI")
#define Q1UI_FONT_MONO       _T("Cascadia Mono")

#define Q1UI_COLOR_APP_BG        RGB(0xf3, 0xf6, 0xfb)
#define Q1UI_COLOR_SURFACE       RGB(0xff, 0xff, 0xff)
#define Q1UI_COLOR_SURFACE_ALT   RGB(0xf8, 0xfa, 0xfd)
#define Q1UI_COLOR_BORDER        RGB(0xd8, 0xe0, 0xea)
#define Q1UI_COLOR_BORDER_SOFT   RGB(0xea, 0xef, 0xf5)
#define Q1UI_COLOR_TEXT          RGB(0x1f, 0x29, 0x37)
#define Q1UI_COLOR_TEXT_MUTED    RGB(0x60, 0x6b, 0x7a)
#define Q1UI_COLOR_ACCENT        RGB(0x25, 0x66, 0xd9)
#define Q1UI_COLOR_ACCENT_SOFT   RGB(0xdb, 0xe7, 0xff)
#define Q1UI_COLOR_WARNING       RGB(0xf5, 0x9e, 0x0b)
#define Q1UI_COLOR_DANGER        RGB(0xdc, 0x26, 0x26)
#define Q1UI_COLOR_SUCCESS       RGB(0x16, 0x9b, 0x62)
#define Q1UI_COLOR_OVERLAY       RGB(0x20, 0x2a, 0x36)
#define Q1UI_COLOR_OVERLAY_TEXT  RGB(0xf8, 0xfa, 0xfc)
#define Q1UI_COLOR_CANVAS_BG     RGB(0xf7, 0xf9, 0xfc)

#define COLOR_PIXEL_TEXT   RGB(0xe8, 0xee, 0xf7)

#define QIMG_MAX_LENGTH    10000

struct GridInfo
{
	int y, x;
	std::vector<int> Hs;
	std::vector<int> Ws;
	cv::Mat pixelMap;
	cv::Mat pixelCoordMap;
};

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
void NearestNeighbor(qu8* src, int h, int w, int hDst, int wDst, int xDst, int yDst, float n,
	long xStart, long xEnd, long yStart, long yEnd, long gap, GridInfo& gi, qu16* nOffsetBuf,
	qu8* nOffsetYBorderFlag, qu8* nOffsetXBorderFlag, qu8* dst);
void Interpolate(qu8* src, int h, int w, int wCanvas, long xStart, long xEnd,
	long yStart, long yEnd, qu16* nOffsetBuf, qu8* dst);

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

} // namespace q1
