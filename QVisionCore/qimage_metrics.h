#ifndef __QIMAGE_METRICS_H__
#define __QIMAGE_METRICS_H__

#include <math.h>
#include "QCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef double (*QIMAGE_METRIC_FN) (qu8 *src, qu8 *dst,
								  int w, int h, int stride, int px_w);

struct qmetric_info
{
	const char *name;
	const QIMAGE_METRIC_FN measure;
	const double min_val;
	const double max_val;
};

double qPSNR(qu8 *src, qu8 *dst, int w, int h, int stride, int px_w);
double qSSIM(qu8 *src, qu8 *dst, int w, int h, int stride, int px_w);

static const struct qmetric_info qmetric_info_table[] =
{
	{ "PSNR", qPSNR,   0.0f, 120.0f },

	{ "SSIM", qSSIM,  -1.0f, 1.0f },

	/* ... add more metric info */
};

#define METRIC_COUNT         ARRAY_SIZE(qmetric_info_table)

#define METRIC_PSNR_IDX      0

#ifdef __cplusplus
}
#endif

#endif /* __QIMAGE_METRICS_H__ */
