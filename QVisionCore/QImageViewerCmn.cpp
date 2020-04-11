#include "QMath.h"
#include "QImageViewerCmn.h"

using namespace std;

int QDeterminDestPos(int lenCanvas, int lenDst, float &offset, float ratio)
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

float QGetFitRatio(float ratio, int w, int h, int wCanvas, int hCanvas)
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

float QGetBestFitRatio(int w, int h, int wCanvas, int hCanvas)
{
	float maxRatio = QMAX((float)wCanvas / w, (float)hCanvas / h);
	return QGetFitRatio(maxRatio,  w, h, wCanvas, hCanvas);
}

void investigatePixelBorder(qu16 *nOffsetBuf,
							int start, int end, int base, int nDst,
							int *gridDim, vector<int> *cellCounts,
							qu8 *nOffsetBorderFlag)
{
	const int GIRD_SIDE_NONE  = 0x0;
	const int GRID_SIDE_START = 0x1;
	const int GRID_SIDE_END   = 0x2;

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

void scaleUsingOffset(qu8 *src, int yStart, int yEnd, int xStart, int xEnd, int stride, int gap,
					  qu16 * nOffsetBuf, qu8 *dst)
{
	for (int y = yStart; y < yEnd; y++) {
		qu8 *src_y = src + nOffsetBuf[y] * stride;
		for (int x = xStart; x < xEnd; x++) {
			qu8 *src_x = src_y + nOffsetBuf[x];
			*dst++ = src_x[0];
			*dst++ = src_x[1];
			*dst++ = src_x[2];
		}
		dst += gap;
	}
}

void scaleUsingOffset(qu8 *src, int yStart, int yEnd, int xStart, int xEnd, int stride, int gap,
					  qu8 *nOffsetYBorderFlag, qu8 *nOffsetXBorderFlag,
					  qu16 * nOffsetBuf, qu8 *dst)
{
	for (int y = yStart; y < yEnd; y++) {
		qu8 *src_y = src + nOffsetBuf[y] * stride;
		for (int x = xStart; x < xEnd; x++) {
			qu8 *src_x = src_y + nOffsetBuf[x];
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

static int findNById(std::vector<P> &nList, N_ID id)
{
	for (int i = 0; i < nList.size(); i++) {
		if (nList[i].second == id)
			return i;
	}

	return -1;
}

static int findIdByN(std::vector<P> &nList, float n)
{
	for (int i = 0; i < nList.size(); i++) {
		if (nList[i].first == n)
			return i;
	}

	return -1;
}

float QGetNextN(float curN, float fitN, float nextD)
{
	vector<P> nList;
	nList.push_back(make_pair(curN, PREV_N));
	nList.push_back(make_pair(ZOOM_RATIO(nextD), NEXT_N));
	if (findIdByN(nList, fitN) < 0)
		nList.push_back(make_pair(fitN, FITT_N));
	float normNs[] = {1.f, 2.f, 3.f, 4.f};
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
