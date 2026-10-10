#pragma once

#include <vector>
#include <QCommon.h>
#include <opencv2/core/core.hpp>

#include "QImageScaling.h"
#include "qimage_presets.h"
#include "Q1UiAppearanceWin.h"

namespace q1 {

#define ZOOM_GAMMA         8.f
#define ZOOM_RATIO(_D)     exp((_D) / ZOOM_GAMMA)
#define ZOOM_DELTA(_N)     (ZOOM_GAMMA * log(_N))
#define ZOOM_MAX           100.f
#define ZOOM_GRID_START    16.f
#define ZOOM_TEXT_START    42.f

#define Q1UI_FONT_TEXT       _T("Segoe UI")
#define Q1UI_FONT_MONO       _T("Cascadia Mono")

#define Q1UI_COLOR_APP_BG        q1view::WindowsUiColorValue(q1view::WindowsUiColor::Window)
#define Q1UI_COLOR_SURFACE       q1view::WindowsUiColorValue(q1view::WindowsUiColor::Surface)
#define Q1UI_COLOR_SURFACE_ALT   q1view::WindowsUiColorValue(q1view::WindowsUiColor::SurfaceAlt)
#define Q1UI_COLOR_BORDER        q1view::WindowsUiColorValue(q1view::WindowsUiColor::Boundary)
#define Q1UI_COLOR_BORDER_SOFT   q1view::WindowsUiColorValue(q1view::WindowsUiColor::Border)
#define Q1UI_COLOR_TEXT          q1view::WindowsUiColorValue(q1view::WindowsUiColor::Text)
#define Q1UI_COLOR_TEXT_MUTED    q1view::WindowsUiColorValue(q1view::WindowsUiColor::Muted)
#define Q1UI_COLOR_ACCENT        q1view::WindowsUiColorValue(q1view::WindowsUiColor::Accent)
#define Q1UI_COLOR_ACCENT_TEXT   q1view::WindowsUiColorValue(q1view::WindowsUiColor::OnAccent)
#define Q1UI_COLOR_ACCENT_SOFT   q1view::WindowsUiColorValue(q1view::WindowsUiColor::Selection)
#define Q1UI_COLOR_WARNING       q1view::WindowsUiColorValue(q1view::WindowsUiColor::Warning)
#define Q1UI_COLOR_DANGER        q1view::WindowsUiColorValue(q1view::WindowsUiColor::Danger)
#define Q1UI_COLOR_SUCCESS       q1view::WindowsUiColorValue(q1view::WindowsUiColor::Success)
#define Q1UI_COLOR_OVERLAY       q1view::WindowsUiColorValue(q1view::WindowsUiColor::Overlay)
#define Q1UI_COLOR_OVERLAY_TEXT  q1view::WindowsUiColorValue(q1view::WindowsUiColor::OverlayText)
#define Q1UI_COLOR_CANVAS_BG     q1view::WindowsUiColorValue(q1view::WindowsUiColor::Canvas)

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
void ResizeArea(qu8* src, int h, int w, int hDst, int wDst,
	int dstStridePixels, qu8* dst);
void ResizeLinear(qu8* src, int h, int w, int hDst, int wDst,
	int dstStridePixels, qu8* dst);
void RotateBgr(const qu8* src, int h, int w, int clockwiseDegrees,
	int dstStridePixels, qu8* dst);

// resolution_info_table now lives in the dependency-free qimage_presets.h so the
// Qt viewer can share it; it is brought into q1 via the include above.

} // namespace q1
