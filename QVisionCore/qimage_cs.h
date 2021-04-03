#ifndef __QIMAGE_CSC_H__
#define __QIMAGE_CSC_H__

#include <stdlib.h>

#include <QCommon.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum _QIMAGE_CS
{
	QIMAGE_CS_YUV = 0x10000000,
	QIMAGE_CS_YUV420,
	QIMAGE_CS_NV12,
	QIMAGE_CS_NV12_T256X16,
	QIMAGE_CS_NV21,
	QIMAGE_CS_YUV420P10,
	QIMAGE_CS_P010,

	QIMAGE_CS_RGB = 0x20000000,
	QIMAGE_CS_GRAYSCALE,
	QIMAGE_CS_RGB888,
	QIMAGE_CS_RGBA8888,
	QIMAGE_CS_BGR888,
	QIMAGE_CS_BGR565,
	QIMAGE_CS_RGBA1010102,
} QIMAGE_CS;

typedef void (*QIMAGE_CSC_FN) (qu8 *, qu8 *, qu8 *,
							   qu8 *, int, int, int);

/*
 * return a scene size of a frame
 * The each bufoff2 and bufoff3 is usually the offset of U, V Plane
 */
typedef int (*QIMAGE_CS_INFO_FN) (int w, int h, int *bufoff2, int *bufoff3);

struct qcsc_info
{
	const QIMAGE_CS cs;
	const char *name;
	const QIMAGE_CS_INFO_FN cs_load_info;
	const QIMAGE_CSC_FN csc2rgb888;
	const QIMAGE_CSC_FN csc2yuv420;
};

int qimage_yuv420_load_info(int w, int h, int *bufoff2, int *bufoff3);
int qimage_nv12_load_info(int w, int h, int *bufoff2, int *bufoff3);
int qimage_nv21_load_info(int w, int h, int *bufoff2, int *bufoff3);
int qimage_rgb888_load_info(int w, int h, int *bufoff2, int *bufoff3);
int qimage_rgba8888_load_info(int w, int h, int *bufoff2, int *bufoff3);
int qimage_bgr888_load_info(int w, int h, int *bufoff2, int *bufoff3);
int qimage_bgr565_load_info(int w, int h, int *bufoff2, int *bufoff3);
int qimage_rgba1010102_load_info(int w, int h, int* bufoff2, int* bufoff3);
int qimage_t256x16_load_info(int w, int h, int *bufoff2, int *bufoff3);
int qimage_grayscale_load_info(int w, int h, int *bufoff2, int *bufoff3);
int qimage_yuv420p10_load_info(int w, int h, int *buffoff2, int *bufoff3);
int qimage_p010_load_info(int w, int h, int *buffoff2, int *bufoff3);

void qimage_yuv420_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
							 int s_bgr, int w, int h);
void qimage_nv12_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
						   int s_bgr, int w, int h);
void qimage_nv21_to_bgr888(qu8 *bgr, qu8 *y, qu8 *v, qu8 *u,
						   int s_bgr, int w, int h);
void qimage_rgb888_to_bgr888(qu8 *bgr, qu8 *src_rgb, qu8 *n1, qu8 *n2,
							 int s_bgr, int w, int h);
void qimage_rgba8888_to_bgr888(qu8 *bgr, qu8 *src_argb, qu8 *n1, qu8 *n2,
							 int s_bgr, int w, int h);
void qimage_bgr888_to_bgr888(qu8 *bgr, qu8 *src_rgb, qu8 *n1, qu8 *n2,
							 int s_bgr, int w, int h);
void qimage_rgba1010102_to_bgr888(qu8* bgr, qu8* src_rgb, qu8* n1, qu8* n2,
	int s_bgr, int w, int h);
void qimage_bgr565_to_bgr888(qu8 *bgr, qu8 *src_rgb, qu8 *n1, qu8 *n2,
							 int s_bgr, int w, int h);
void qimage_t256x16_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
							  int s_bgr, int w, int h);
void qimage_grayscale_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
							 int s_bgr, int w, int h);
void qimage_yuv420p10_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
						   int s_bgr, int w, int h);
void qimage_p010_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
						   int s_bgr, int w, int h);

void qimage_nv12_to_420(qu8 *yuv, qu8 *y, qu8 *u, qu8 *v,
						int n1, int w, int h);
void qimage_nv21_to_420(qu8 *yuv, qu8 *y, qu8 *v, qu8 *u,
						int n1, int w, int h);

// from bgr888 to something
void qimage_bgr888_to_nv21(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
						   int s_bgr, int w, int h);
void qimage_bgr888_to_yuv420(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
							 int s_bgr, int w, int h);

static const struct qcsc_info qcsc_info_table[] =
{
	{
		QIMAGE_CS_YUV420,
		"yuv420",
		qimage_yuv420_load_info,
		qimage_yuv420_to_bgr888,
		NULL,
	},
	{
		QIMAGE_CS_NV12,
		"nv12",
		qimage_nv12_load_info,
		qimage_nv12_to_bgr888,
		qimage_nv12_to_420,
	},
	{
		QIMAGE_CS_NV12_T256X16,
		"nv12_t256x16",
		qimage_t256x16_load_info,
		qimage_t256x16_to_bgr888,
		NULL,
	},
	{
		QIMAGE_CS_NV21,
		"nv21",
		qimage_nv21_load_info,
		qimage_nv21_to_bgr888,
		qimage_nv21_to_420,
	},
	{
		QIMAGE_CS_YUV420P10,
		"yuv420p10",
		qimage_yuv420p10_load_info,
		qimage_yuv420p10_to_bgr888,
		NULL,
	},
	{
		QIMAGE_CS_P010,
		"p010",
		qimage_p010_load_info,
		qimage_p010_to_bgr888,
		NULL,
	},
	/* ... add more yuv colors */

	{
		QIMAGE_CS_GRAYSCALE,
		"grayscale",
		qimage_grayscale_load_info,
		qimage_grayscale_to_bgr888,
		NULL,
	},
	{
		QIMAGE_CS_RGB888,
		"rgb888",
		qimage_rgb888_load_info,
		qimage_rgb888_to_bgr888,
		NULL,
	},
	{
		QIMAGE_CS_RGBA8888,
		"rgba8888",
		qimage_rgba8888_load_info,
		qimage_rgba8888_to_bgr888,
		NULL,
	},
	{
		QIMAGE_CS_BGR888,
		"bgr888",
		qimage_bgr888_load_info,
		qimage_bgr888_to_bgr888,
		NULL,
	},
	{
		QIMAGE_CS_BGR565,
		"bgr565",
		qimage_bgr565_load_info,
		qimage_bgr565_to_bgr888,
		NULL,
	},
	{
		QIMAGE_CS_RGBA1010102,
		"rgba1010102",
		qimage_rgba1010102_load_info,
		qimage_rgba1010102_to_bgr888,
		NULL,
	},
	/* ... add more rgb colors */
};

#define QIMG_DEF_CS_IDX       0
#define QIMG_DST_RGB_BYTES    3
#define QPLANES               3 /* color plane, 3 for rgb and yuv */

#ifdef __cplusplus
}
#endif

#endif /* __QIMAGE_CSC_H__ */
