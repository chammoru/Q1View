#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qimage_cs.h"
#include "qimage_metrics.h"

static int failures = 0;

#define CHECK_TRUE(name, condition) do { \
	if (!(condition)) { \
		fprintf(stderr, "FAIL: %s\n", name); \
		failures++; \
	} \
} while (0)

typedef int (*LoadInfoFn)(int, int, int *, int *);

static void check_int(const char *name, int actual, int expected)
{
	if (actual != expected) {
		fprintf(stderr, "FAIL: %s expected %d, got %d\n",
			name, expected, actual);
		failures++;
	}
}

static void check_near(const char *name, double actual, double expected,
	double tolerance)
{
	if (fabs(actual - expected) > tolerance) {
		fprintf(stderr, "FAIL: %s expected %.12f, got %.12f\n",
			name, expected, actual);
		failures++;
	}
}

static void check_bytes(const char *name, const qu8 *actual,
	const qu8 *expected, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++) {
		if (actual[i] != expected[i]) {
			fprintf(stderr, "FAIL: %s byte %zu expected %u, got %u\n",
				name, i, expected[i], actual[i]);
			failures++;
			return;
		}
	}
}

static void check_layout(const char *name, LoadInfoFn load_info,
	int w, int h, int expected_size, int expected_off2, int expected_off3)
{
	char value_name[96];
	int off2 = -1;
	int off3 = -1;
	int size = load_info(w, h, &off2, &off3);

	snprintf(value_name, sizeof(value_name), "%s size", name);
	check_int(value_name, size, expected_size);
	snprintf(value_name, sizeof(value_name), "%s plane 2 offset", name);
	check_int(value_name, off2, expected_off2);
	snprintf(value_name, sizeof(value_name), "%s plane 3 offset", name);
	check_int(value_name, off3, expected_off3);
}

static void check_sample(const char *name, QIMAGE_SAMPLE_NATIVE_PIXEL_FN fn,
	const qu8 *src, int w, int h, int x, int y, QIMAGE_PIXEL_MODEL model,
	int bit_depth, int chroma_x, int chroma_y, int c0, int c1, int c2)
{
	QIMAGE_NATIVE_PIXEL_SAMPLE sample = { 0 };

	CHECK_TRUE(name, fn(src, w, h, x, y, &sample) != 0);
	check_int(name, sample.model, model);
	check_int(name, sample.bit_depth, bit_depth);
	check_int(name, sample.chroma_x, chroma_x);
	check_int(name, sample.chroma_y, chroma_y);
	check_int(name, sample.component[0], c0);
	check_int(name, sample.component[1], c1);
	check_int(name, sample.component[2], c2);
}

static void test_raw_layouts(void)
{
	check_layout("yuv420 odd layout", qimage_yuv420_load_info,
		3, 3, 17, 9, 13);
	check_layout("nv12 odd layout", qimage_nv12_load_info,
		3, 3, 17, 9, 0);
	check_layout("nv21 odd layout", qimage_nv21_load_info,
		3, 3, 17, 9, 0);
	check_layout("yuv420p10le odd layout", qimage_yuv420p10le_load_info,
		3, 3, 34, 18, 26);
	check_layout("p010 odd layout", qimage_p010_load_info,
		3, 3, 34, 18, 0);
	check_layout("yuv422p10le odd layout", qimage_yuv422p10le_load_info,
		3, 3, 42, 18, 30);
	check_layout("p210 odd layout", qimage_p210_load_info,
		3, 3, 42, 18, 0);
	check_layout("grayscale layout", qimage_grayscale_load_info,
		3, 3, 9, 0, 0);
	check_layout("rgb888 layout", qimage_rgb888_load_info,
		3, 3, 27, 0, 0);
	check_layout("bgr888 layout", qimage_bgr888_load_info,
		3, 3, 27, 0, 0);
	check_layout("rgba8888 layout", qimage_rgba8888_load_info,
		3, 3, 36, 0, 0);
	check_layout("bgr565 layout", qimage_bgr565_load_info,
		3, 3, 18, 0, 0);
	check_layout("rgba1010102 layout", qimage_rgba1010102_load_info,
		3, 3, 36, 0, 0);
	check_layout("abgr2101010 layout", qimage_abgr2101010_load_info,
		3, 3, 36, 0, 0);
	check_layout("rgb16u layout", qimage_rgb16u_load_info,
		3, 3, 54, 0, 0);
	check_layout("tiled nv12 layout", qimage_t256x16_load_info,
		257, 17, 98304, 65536, 0);
}

static void test_native_yuv8_samples(void)
{
	qu8 yuv420[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9,
		101, 102, 103, 104,
		201, 202, 203, 204
	};
	qu8 nv12[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9,
		101, 201, 102, 202, 103, 203, 104, 204
	};
	qu8 nv21[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 9,
		201, 101, 202, 102, 203, 103, 204, 104
	};
	QIMAGE_NATIVE_PIXEL_SAMPLE sample = { 0 };

	check_sample("yuv420 first pixel", qimage_yuv420_sample_native,
		yuv420, 3, 3, 0, 0, QIMAGE_PIXEL_MODEL_YUV, 8, 0, 0,
		1, 101, 201);
	check_sample("yuv420 shared chroma", qimage_yuv420_sample_native,
		yuv420, 3, 3, 1, 1, QIMAGE_PIXEL_MODEL_YUV, 8, 0, 0,
		5, 101, 201);
	check_sample("yuv420 odd edge", qimage_yuv420_sample_native,
		yuv420, 3, 3, 2, 2, QIMAGE_PIXEL_MODEL_YUV, 8, 1, 1,
		9, 104, 204);
	CHECK_TRUE("yuv420 rejects out of range",
		qimage_yuv420_sample_native(yuv420, 3, 3, 3, 0, &sample) == 0);

	check_sample("nv12 odd edge", qimage_nv12_sample_native,
		nv12, 3, 3, 2, 2, QIMAGE_PIXEL_MODEL_YUV, 8, 1, 1,
		9, 104, 204);
	check_sample("nv21 odd edge", qimage_nv21_sample_native,
		nv21, 3, 3, 2, 2, QIMAGE_PIXEL_MODEL_YUV, 8, 1, 1,
		9, 104, 204);
}

static void test_native_yuv10_samples(void)
{
	qu16 planar420[] = {
		10, 11, 12, 13, 14, 15, 16, 17, 18,
		600, 601, 602, 603,
		700, 701, 702, 703
	};
	qu16 p010[] = {
		10 << 6, 11 << 6, 12 << 6, 13 << 6, 14 << 6, 15 << 6,
		16 << 6, 17 << 6, 18 << 6,
		600 << 6, 700 << 6, 601 << 6, 701 << 6,
		602 << 6, 702 << 6, 603 << 6, 703 << 6
	};
	qu16 planar422[] = {
		10, 11, 12, 13, 14, 15,
		400, 401, 402, 403,
		500, 501, 502, 503
	};
	qu16 p210[] = {
		10 << 6, 11 << 6, 12 << 6, 13 << 6, 14 << 6, 15 << 6,
		400 << 6, 500 << 6, 401 << 6, 501 << 6,
		402 << 6, 502 << 6, 403 << 6, 503 << 6
	};

	check_sample("yuv420p10le odd edge", qimage_yuv420p10le_sample_native,
		(const qu8 *)planar420, 3, 3, 2, 2, QIMAGE_PIXEL_MODEL_YUV,
		10, 1, 1, 18, 603, 703);
	check_sample("p010 odd edge", qimage_p010_sample_native,
		(const qu8 *)p010, 3, 3, 2, 2, QIMAGE_PIXEL_MODEL_YUV,
		10, 1, 1, 18, 603, 703);
	check_sample("yuv422p10le second row", qimage_yuv422p10le_sample_native,
		(const qu8 *)planar422, 3, 2, 2, 1, QIMAGE_PIXEL_MODEL_YUV,
		10, 1, 1, 15, 403, 503);
	check_sample("p210 second row", qimage_p210_sample_native,
		(const qu8 *)p210, 3, 2, 2, 1, QIMAGE_PIXEL_MODEL_YUV,
		10, 1, 1, 15, 403, 503);
}

static void test_tiled_and_packed_native_samples(void)
{
	int size = qimage_t256x16_load_info(257, 17, NULL, NULL);
	qu8 *tiled = (qu8 *)calloc((size_t)size, 1);
	qu32 abgr = (9u << 20) | (8u << 10) | 7u;

	CHECK_TRUE("allocate tiled fixture", tiled != NULL);
	if (tiled != NULL) {
		tiled[36864] = 19;
		tiled[65536 + 6144] = 111;
		tiled[65536 + 6145] = 211;
		check_sample("tiled nv12 tile crossing", qimage_t256x16_sample_native,
			tiled, 257, 17, 256, 16, QIMAGE_PIXEL_MODEL_YUV,
			8, 128, 8, 19, 111, 211);
		free(tiled);
	}

	check_sample("abgr2101010 native rgb", qimage_abgr2101010_sample_native,
		(const qu8 *)&abgr, 1, 1, 0, 0, QIMAGE_PIXEL_MODEL_RGB,
		10, 0, 0, 7, 8, 9);
}

static void test_yuv_display_conversions(void)
{
	qu8 y[] = { 81, 145, 41, 210 };
	qu8 u[] = { 90 };
	qu8 v[] = { 240 };
	qu8 uv[] = { 90, 240 };
	qu8 vu[] = { 240, 90 };
	qu16 y10[] = { 81 << 2, 145 << 2, 41 << 2, 210 << 2 };
	qu16 u10[] = { 90 << 2 };
	qu16 v10[] = { 240 << 2 };
	qu16 p010[] = {
		81 << 8, 145 << 8, 41 << 8, 210 << 8, 90 << 8, 240 << 8
	};
	qu8 out[12];
	const qu8 expected[] = {
		0, 0, 255, 74, 74, 255, 0, 0, 208, 149, 150, 255
	};

	qimage_yuv420_to_bgr888(out, y, u, v, 2, 2, 2);
	check_bytes("yuv420 display conversion", out, expected, sizeof(expected));
	qimage_nv12_to_bgr888(out, y, uv, NULL, 2, 2, 2);
	check_bytes("nv12 display conversion", out, expected, sizeof(expected));
	qimage_nv21_to_bgr888(out, y, vu, NULL, 2, 2, 2);
	check_bytes("nv21 display conversion", out, expected, sizeof(expected));
	qimage_yuv420p10le_to_bgr888(out, (qu8 *)y10, (qu8 *)u10,
		(qu8 *)v10, 2, 2, 2);
	check_bytes("yuv420p10le display conversion", out, expected,
		sizeof(expected));
	qimage_p010_to_bgr888(out, (qu8 *)p010, (qu8 *)(p010 + 4),
		NULL, 2, 2, 2);
	check_bytes("p010 display conversion", out, expected, sizeof(expected));
}

static void test_rgb_display_conversions(void)
{
	qu8 out[6];
	qu8 rgb888[] = { 12, 34, 56 };
	qu8 rgba8888[] = { 12, 34, 56, 99 };
	qu8 bgr888[] = { 56, 34, 12 };
	qu8 bgr565[] = { 0x00, 0xf8 };
	qu32 rgba1010102 = (1023u << 22) | (512u << 2);
	qu32 abgr2101010 = (512u << 20) | 1023u;
	qu16 rgb16u[] = { 65535, 0, 32768 };
	qu8 gray[] = { 4, 200 };
	const qu8 bgr_expected[] = { 56, 34, 12 };
	const qu8 packed_expected[] = { 128, 0, 255 };
	const qu8 bgr565_expected[] = { 4, 2, 252 };
	const qu8 gray_expected[] = { 4, 4, 4, 200, 200, 200 };

	qimage_rgb888_to_bgr888(out, rgb888, NULL, NULL, 1, 1, 1);
	check_bytes("rgb888 display conversion", out, bgr_expected, 3);
	qimage_rgba8888_to_bgr888(out, rgba8888, NULL, NULL, 1, 1, 1);
	check_bytes("rgba8888 display conversion", out, bgr_expected, 3);
	qimage_bgr888_to_bgr888(out, bgr888, NULL, NULL, 1, 1, 1);
	check_bytes("bgr888 display conversion", out, bgr_expected, 3);
	qimage_bgr565_to_bgr888(out, bgr565, NULL, NULL, 1, 1, 1);
	check_bytes("bgr565 display conversion", out, bgr565_expected, 3);
	qimage_rgba1010102_to_bgr888(out, (qu8 *)&rgba1010102, NULL, NULL,
		1, 1, 1);
	check_bytes("rgba1010102 display conversion", out, packed_expected, 3);
	qimage_abgr2101010_to_bgr888(out, (qu8 *)&abgr2101010, NULL, NULL,
		1, 1, 1);
	check_bytes("abgr2101010 display conversion", out, packed_expected, 3);
	qimage_rgb16u_to_bgr888(out, (qu8 *)rgb16u, NULL, NULL, 1, 1, 1);
	check_bytes("rgb16u display conversion", out, packed_expected, 3);
	qimage_grayscale_to_bgr888(out, gray, NULL, NULL, 2, 2, 1);
	check_bytes("grayscale display conversion", out, gray_expected, 6);
}

static void test_metrics(void)
{
	qu8 psnr_src[] = { 0, 0, 255, 0, 0, 255 };
	qu8 psnr_dst[] = { 0, 10, 0, 0, 0, 0 };
	qu8 ssim_src[80];
	qu8 ssim_dst[80];
	qu8 zero[64] = { 0 };
	qu8 ten[64];
	int x;
	int y;

	check_near("PSNR ignores padded samples",
		qPSNR(psnr_src, psnr_dst, 2, 2, 3, 1),
		34.151403521959, 0.000000001);

	memset(ssim_src, 0, sizeof(ssim_src));
	memset(ssim_dst, 0, sizeof(ssim_dst));
	for (y = 0; y < 8; y++) {
		for (x = 0; x < 8; x++) {
			ssim_src[y * 10 + x] = (qu8)(y * 8 + x);
			ssim_dst[y * 10 + x] = ssim_src[y * 10 + x];
		}
		ssim_dst[y * 10 + 8] = 255;
		ssim_dst[y * 10 + 9] = 255;
	}
	check_near("SSIM ignores padded samples",
		qSSIM(ssim_src, ssim_dst, 8, 8, 10, 1), 1.0, 0.000000001);

	memset(ten, 10, sizeof(ten));
	check_near("SSIM constant reference",
		qSSIM(zero, ten, 8, 8, 8, 1),
		6.5025 / 106.5025, 0.000000001);

	{
		/* When the window slides from a bright region into a dark region the
		 * column sums decrease.  The sliding update
		 *   win_x += col_x[c + SIZE_N] - col_x[c]
		 * computed the difference in 32-bit unsigned, which wrapped to a large
		 * positive value and inflated win_x, corrupting SSIM (sometimes > 1).
		 * For identical src/dst the result must be exactly 1.0. */
		qu8 bright_dark[8 * 16];
		for (y = 0; y < 8; y++) {
			for (x = 0; x < 16; x++)
				bright_dark[y * 16 + x] = (x < 8) ? 255 : 0;
		}
		check_near("SSIM bright-to-dark identical images",
			qSSIM(bright_dark, bright_dark, 16, 8, 16, 1), 1.0, 0.000000001);
	}
}

int main(void)
{
	test_raw_layouts();
	test_native_yuv8_samples();
	test_native_yuv10_samples();
	test_tiled_and_packed_native_samples();
	test_yuv_display_conversions();
	test_rgb_display_conversions();
	test_metrics();

	if (failures != 0) {
		fprintf(stderr, "%d core regression test failure(s)\n", failures);
		return 1;
	}

	puts("Core regression tests passed.");
	return 0;
}
