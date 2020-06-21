#include "QMath.h"
#include "QViewerCmn.h"
#include "qimage_cs.h"

#include <opencv2/imgproc/imgproc.hpp>

using namespace std;

namespace q1 {

	int DeterminDestPos(int lenCanvas, int lenDst, float& offset, float ratio)
	{
		int posDst;
		int margin = lenCanvas - lenDst;

		if (lenCanvas >= lenDst) {
			posDst = margin >> 1;
			offset = 0.0f;
		} else {
			posDst = ROUND2I((float)margin / 2 + offset * ratio);

			if (posDst > 0) {
				posDst = 0;
				offset = -((float)margin / 2) / ratio;
			} else if (posDst < margin) {
				posDst = margin;
				offset = ((float)margin / 2) / ratio;
			}
		}

		return posDst;
	}

	float GetFitRatio(float ratio, int w, int h, int wCanvas, int hCanvas)
	{
		int wDst = ROUND2I(w * ratio);
		int hDst = ROUND2I(h * ratio);

		if (wDst > wCanvas || hDst > hCanvas) {
			// reduce the image size to less than its canvas size,
			// only if client area is small
			if (wDst * hCanvas > hDst * wCanvas)
				ratio = (float)wCanvas / w;
			else
				ratio = (float)hCanvas / h;
		}

		return ratio;
	}

	float GetBestFitRatio(int w, int h, int wCanvas, int hCanvas)
	{
		float maxRatio = QMAX((float)wCanvas / w, (float)hCanvas / h);
		return GetFitRatio(maxRatio, w, h, wCanvas, hCanvas);
	}

	void InvestigatePixelBorder(qu16* nOffsetBuf,
		int start, int end, int base, int nDst,
		int* gridDim, vector<int>* cellCounts,
		qu8* nOffsetBorderFlag)
	{
		const int GIRD_SIDE_NONE = 0x0;
		const int GRID_SIDE_START = 0x1;
		const int GRID_SIDE_END = 0x2;

		for (int i = start; i < end; i++) {
			qu16 cyOffset = nOffsetBuf[i];
			if (i == 0 || nOffsetBuf[i - 1] != cyOffset)
				nOffsetBorderFlag[i] = GRID_SIDE_START;
			else if (i == nDst - 1 || nOffsetBuf[i + 1] != cyOffset)
				nOffsetBorderFlag[i] = GRID_SIDE_END;
			else
				nOffsetBorderFlag[i] = GIRD_SIDE_NONE;
		}
		int gridStart = start; // refine the first point
		if (nOffsetBorderFlag[gridStart] == GRID_SIDE_END)
			gridStart--;
		while (gridStart - 1 >= 0 && nOffsetBuf[gridStart - 1] == nOffsetBuf[gridStart]) {
			nOffsetBorderFlag[gridStart] = GIRD_SIDE_NONE;
			gridStart--;
		}
		nOffsetBorderFlag[gridStart] = GRID_SIDE_START;
		*gridDim = gridStart - start + base;
		int gridEnd = end - 1; // refine the last point
		while (gridEnd + 1 < nDst && nOffsetBuf[gridEnd + 1] == nOffsetBuf[gridEnd]) {
			nOffsetBorderFlag[gridEnd] = GIRD_SIDE_NONE;
			gridEnd++;
		}
		nOffsetBorderFlag[gridEnd] = GRID_SIDE_END;
		// record the number of cells in each grid
		cellCounts->clear();
		int count;
		for (int i = gridStart; i <= gridEnd; i++) {
			if (nOffsetBorderFlag[i] == GRID_SIDE_START)
				count = 2; /* start + end */
			else if (nOffsetBorderFlag[i] == GRID_SIDE_END)
				cellCounts->push_back(count);
			else
				count++;
		}
	}

	void ScaleUsingOffset(qu8* src, int yStart, int yEnd, int xStart, int xEnd, int stride, int gap,
		qu16* nOffsetBuf, qu8* dst)
	{
		for (int y = yStart; y < yEnd; y++) {
			qu8* src_y = src + nOffsetBuf[y] * stride;
			for (int x = xStart; x < xEnd; x++) {
				qu8* src_x = src_y + nOffsetBuf[x];
				*dst++ = src_x[0];
				*dst++ = src_x[1];
				*dst++ = src_x[2];
			}
			dst += gap;
		}
	}

	void ScaleUsingOffset(qu8* src, int yStart, int yEnd, int xStart, int xEnd, int stride, int gap,
		qu8* nOffsetYBorderFlag, qu8* nOffsetXBorderFlag,
		qu16* nOffsetBuf, qu8* dst)
	{
		for (int y = yStart; y < yEnd; y++) {
			qu8* src_y = src + nOffsetBuf[y] * stride;
			for (int x = xStart; x < xEnd; x++) {
				qu8* src_x = src_y + nOffsetBuf[x];
				if (nOffsetYBorderFlag[y] || nOffsetXBorderFlag[x]) {
					*dst++ = (0x80 + src_x[0]) >> 1;
					*dst++ = (0x80 + src_x[1]) >> 1;
					*dst++ = (0x80 + src_x[2]) >> 1;
				} else {
					*dst++ = src_x[0];
					*dst++ = src_x[1];
					*dst++ = src_x[2];
				}
			}
			dst += gap;
		}
	}

	enum N_ID {
		FITT_N,
		NORM_N,
		PREV_N,
		NEXT_N,
	};

	typedef std::pair<float, N_ID> P;

	static int findNById(std::vector<P>& nList, N_ID id)
	{
		for (int i = 0; i < nList.size(); i++) {
			if (nList[i].second == id)
				return i;
		}

		return -1;
	}

	static int findIdByN(std::vector<P>& nList, float n)
	{
		for (int i = 0; i < nList.size(); i++) {
			if (nList[i].first == n)
				return i;
		}

		return -1;
	}

	float GetNextN(float curN, float fitN, float nextD)
	{
		vector<P> nList;
		nList.push_back(make_pair(curN, PREV_N));
		nList.push_back(make_pair(ZOOM_RATIO(nextD), NEXT_N));
		if (findIdByN(nList, fitN) < 0)
			nList.push_back(make_pair(fitN, FITT_N));
		float normNs[] = { 1.f, 2.f, 3.f, 4.f };
		for (int i = 0; i < ARRAY_SIZE(normNs); i++) {
			if (normNs[i] != curN) {
				nList.push_back(make_pair(normNs[i], NORM_N));
			}
		}
		sort(nList.begin(), nList.end());

		int prevIdx = findNById(nList, PREV_N);
		int nextIdx = findNById(nList, NEXT_N);
		float nextN;
		if (nextIdx < prevIdx) {
			nextN = nList[prevIdx - 1].first;
		} else {
			nextN = nList[prevIdx + 1].first;
		}

		return nextN;
	}

	void NearestNeighbor(qu8* src, int h, int w, int hDst, int wDst, int xDst, int yDst, float n,
		long xStart, long xEnd, long yStart, long yEnd, long gap, GridInfo& gi, qu16* nOffsetBuf,
		qu8* nOffsetYBorderFlag, qu8* nOffsetXBorderFlag, qu8* dst)
	{
		long xBase = xDst > 0 ? xDst : 0;
		long yBase = yDst > 0 ? yDst : 0;

		// Nearest Neighborhood
		if (n >= ZOOM_GRID_START) {
			// investigate y-axis pixel border
			InvestigatePixelBorder(nOffsetBuf, yStart, yEnd, yBase, hDst,
				&gi.y, &gi.Hs, nOffsetYBorderFlag);

			// investigate x-axis border
			InvestigatePixelBorder(nOffsetBuf, xStart, xEnd, xBase, wDst,
				&gi.x, &gi.Ws, nOffsetXBorderFlag);

			gi.pixelMap.create(int(gi.Hs.size()), int(gi.Ws.size()), CV_8UC3);
			for (int i = 0, y = yStart; i < gi.pixelMap.rows; y += gi.Hs[i], i++) {
				qu8* src_y = src + nOffsetBuf[y] * ROUNDUP_DWORD(w);
				for (int j = 0, x = xStart; j < gi.pixelMap.cols; x += gi.Ws[j], j++) {
					qu8* src_x = src_y + nOffsetBuf[x];
					gi.pixelMap.at<cv::Vec3b>(i, j) = cv::Vec3b(src_x);
				}
			}
			ScaleUsingOffset(src, yStart, yEnd, xStart, xEnd, ROUNDUP_DWORD(w), gap,
				nOffsetYBorderFlag, nOffsetXBorderFlag, nOffsetBuf, dst);
		} else {
			ScaleUsingOffset(src, yStart, yEnd, xStart, xEnd, ROUNDUP_DWORD(w), gap,
				nOffsetBuf, dst);
		}
	}

	void Interpolate(qu8* src, int h, int w, int wCanvas, long xStart, long xEnd,
		long yStart, long yEnd, qu16 *nOffsetBuf, qu8* dst)
	{
		int xLen = xEnd - xStart;
		int yLen = yEnd - yStart;
		cv::Mat patch;
		cv::Mat srcMat(h, w, CV_8UC3, src, ROUNDUP_DWORD(w) * QIMG_DST_RGB_BYTES);
		cv::Mat dstMat(yLen, xLen, CV_8UC3, dst, ROUNDUP_DWORD(wCanvas) * QIMG_DST_RGB_BYTES);
		cv::Size patchSize(
			(nOffsetBuf[xEnd - 1] - nOffsetBuf[xStart]) / QIMG_DST_RGB_BYTES + 1,
			(nOffsetBuf[yEnd - 1] - nOffsetBuf[yStart]) / QIMG_DST_RGB_BYTES + 1);
		float xSum = 0.f, ySum = 0.f;
		for (int x = xStart; x < xEnd; x++)
			xSum += nOffsetBuf[x];
		xSum /= xLen * QIMG_DST_RGB_BYTES;
		for (int y = yStart; y < yEnd; y++)
			ySum += nOffsetBuf[y];
		ySum /= yLen * QIMG_DST_RGB_BYTES;
		cv::Point2f center(xSum, ySum);
		cv::getRectSubPix(srcMat, patchSize, center, patch);
		cv::resize(patch, dstMat, dstMat.size(), 0, 0, cv::INTER_LINEAR);
	}
} // namespace q1
