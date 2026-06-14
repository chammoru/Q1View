#ifndef __QIMAGE_METRICS_H__
#define __QIMAGE_METRICS_H__

#include <math.h>
#include "QCommon.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef double (*QIMAGE_METRIC_FN) (qu8 *src, qu8 *dst,
								  int w, int h, int stride, int px_w);

enum qmetric_kind { QMETRIC_PER_PLANE, QMETRIC_WHOLE_IMAGE };

struct qmetric_info
{
	const char *name;
	const QIMAGE_METRIC_FN measure;
	enum qmetric_kind kind;
	int lazy;
	int plane_count;
	int higher_is_better;
	int clamp_to_max;
	double min_val;
	double max_val;
	double same_value;
	double same_epsilon;
	// Backend id for a lazy ML metric ("alex" / "vgg"), used to provision and
	// load the matching ONNX model. NULL for built-in (non-ML) metrics. LPIPS
	// values from different backbones are not numerically interchangeable, so
	// each backbone is its own metric entry with its own cache slot and label.
	const char *ml_id;
};

double qPSNR(qu8 *src, qu8 *dst, int w, int h, int stride, int px_w);
double qSSIM(qu8 *src, qu8 *dst, int w, int h, int stride, int px_w);

static const struct qmetric_info qmetric_info_table[] =
{
	{ "PSNR", qPSNR, QMETRIC_PER_PLANE, 0, 3, 1, 1, 0.0, 120.0, 120.0, 0.0, NULL },
	{ "SSIM", qSSIM, QMETRIC_PER_PLANE, 0, 3, 1, 1, -1.0, 1.0, 1.0, 0.0, NULL },
	{ "LPIPS-AlexNet", NULL, QMETRIC_WHOLE_IMAGE, 1, 1, 0, 0, 0.0, 1.0, 0.0, 1e-4, "alex" },
	{ "LPIPS-VGG",     NULL, QMETRIC_WHOLE_IMAGE, 1, 1, 0, 0, 0.0, 1.0, 0.0, 1e-4, "vgg"  }
};

#define METRIC_COUNT         ARRAY_SIZE(qmetric_info_table)

#define METRIC_PSNR_IDX      0
#define METRIC_START_IDX     METRIC_PSNR_IDX

#ifdef __cplusplus
}
#endif

#endif /* __QIMAGE_METRICS_H__ */
