#ifndef __QIMAGE_UTIL_H__
#define __QIMAGE_UTIL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <QCommon.h>

void qimage_crop(qu8 *src, int sStride, qu8 *dst, int x0, int y0, int dStride, int y);

// JUST ADD HERE, IF YOU WANT ADDITIONAL RESOLUTION, THIS'S IT!
static const char *qfps_info_table[] = {
	"15.00fps",
	"24.00fps",
	"30.00fps",
	"60.00fps",

	"C&ustom...",
};

#ifdef __cplusplus
}
#endif

#endif /* __QIMAGE_UTIL_H__ */
