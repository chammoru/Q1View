
#include <string.h>

#include "qimage_util.h"

void qimage_crop(qu8 *src, int sStride, qu8 *dst, int x0, int y0, int dStride, int y)
{
	int i;
	qu8 *src_t = src + y0 * sStride + x0;
	qu8 *dst_t = dst;

	for (i = 0; i < y; i++) {
		memcpy(dst_t, src_t, dStride);
		src_t += sStride;
		dst_t += dStride;
	}
}
