#include "StdAfx.h"
#include <math.h>

#include "QImageDiff.h"

using namespace std;

void QCalRLC(qu8 *src, qu8 *dst, int w, int h,
			  int stride, int px_w, list<RLC> *diffRLC)
{
	int margin = stride - w;
	bool preSame = true;
	long preCount = 0;

	for (int i = 0; i < h; i++) {
		qu32 line_sum_dxd = 0;
		bool curSame = true;
		for (int j = 0; j < w; j++) {
			long d = *src - *dst;

			src += px_w;
			dst += px_w;

			line_sum_dxd += d * d;
		}

		curSame = line_sum_dxd == 0;
		if (curSame == preSame) {
			preCount++;
		} else {
			if (preCount)
				diffRLC->push_back(RLC(preSame, preCount));

			preCount = 1;
			preSame = curSame;
		}

		src += 3 * margin;
		dst += 3 * margin;
	}

	if (preCount)
		diffRLC->push_back(RLC(preSame, preCount));
}
