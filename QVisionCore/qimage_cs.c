
#include <string.h>

#include "qimage_cs.h"
#include "QDebug.h"

// Converting Between YUV and RGB
// https://msdn.microsoft.com/en-us/library/ms893078.aspx

#define SAT_S32_TO_U8(x) \
	(unsigned char)(((x) & (~255)) ? (((-x) >> 15) & 0xFF) : (x))

int qimage_yuv420_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	int chroma_size = ((w + 1) >> 1) * ((h + 1) >> 1);
	int scene_size = w * h + (chroma_size << 1);

	if (bufoff2)
		*bufoff2 = w * h;

	if (bufoff3)
		*bufoff3 = chroma_size + w * h;

	return scene_size;
}

void qimage_yuv420_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
							 int s_bgr, int w, int h)
{
	int i, j;
	int c, d, e;
	int r, g, b;
	qu8 *cy, *cu, *cv;
	int gap;

	gap  = (s_bgr - ROUNDUP_EVEN(w)) * 3;

	for (i = 0; i < h; i++)
	{
		cy = y + i * w;
		cu = u + (i >> 1) * (w >> 1);
		cv = v + (i >> 1) * (w >> 1);

		for (j = 0; j < w; j += 2)
		{
			c = (*cy++ - 16) * 298;
			d = *cu++ - 128;
			e = *cv++ - 128;

			r = c           + 409 * e + 128;
			g = c - 100 * d - 208 * e + 128;
			b = c + 516 * d           + 128;

			*bgr++ = SAT_S32_TO_U8(b >> 8);
			*bgr++ = SAT_S32_TO_U8(g >> 8);
			*bgr++ = SAT_S32_TO_U8(r >> 8);

			c = (*cy++ - 16) * 298 - c;

			r += c;
			g += c;
			b += c;

			*bgr++ = SAT_S32_TO_U8(b >> 8);
			*bgr++ = SAT_S32_TO_U8(g >> 8);
			*bgr++ = SAT_S32_TO_U8(r >> 8);
		}

		bgr += gap;
	}
}

int qimage_nv12_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	int chroma_size = ((w + 1) >> 1) * ((h + 1) >> 1);
	int scene_size = w * h + (chroma_size << 1);

	if (bufoff2)
		*bufoff2 =  w * h;

	if (bufoff3)
		*bufoff3 = 0;

	return scene_size;
}

void qimage_nv12_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
						   int s_bgr, int w, int h)
{
	int i, j;
	int c, d, e;
	int r, g, b;
	qu8 *cy, *cu, *cv;
	int gap;

	gap = (s_bgr - ROUNDUP_EVEN(w)) * 3;

	for (i = 0; i < h; i++)
	{
		cy = y + i * w;
		cu = u + (i >> 1) * w;
		cv = cu + 1;

		for (j = 0; j < w; j += 2) // assume even width
		{
			c = *cy++ - 16;
			d = *cu - 128;
			e = *cv - 128;

			cu += 2;
			cv += 2;

			b = (298 * c + 516 * d           + 128) >> 8;
			g = (298 * c - 100 * d - 208 * e + 128) >> 8;
			r = (298 * c           + 409 * e + 128) >> 8;

			*bgr++ = SAT_S32_TO_U8(b);
			*bgr++ = SAT_S32_TO_U8(g);
			*bgr++ = SAT_S32_TO_U8(r);

			c = *cy++ - 16;

			b = (298 * c + 516 * d           + 128) >> 8;
			g = (298 * c - 100 * d - 208 * e + 128) >> 8;
			r = (298 * c           + 409 * e + 128) >> 8;

			*bgr++ = SAT_S32_TO_U8(b);
			*bgr++ = SAT_S32_TO_U8(g);
			*bgr++ = SAT_S32_TO_U8(r);
		}

		bgr += gap;
	}
}

void qimage_nv12_to_420(qu8 *yuv, qu8 *y, qu8 *u, qu8 *v,
						int n1, int w, int h)
{
	size_t i;
	qu8 *d_u, *d_v;

	size_t y_size = w * h;
	size_t u_size = ((w + 1) >> 1) * ((h + 1) >> 1);

	d_u = yuv + y_size;
	d_v = d_u + u_size;

	memcpy(yuv, y, y_size);

	for (i = 0; i < u_size; i++) {
		*d_u++ = *u++;
		*d_v++ = *u++;
	}
}

int qimage_nv21_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	int chroma_size = ((w + 1) >> 1) * ((h + 1) >> 1);
	int scene_size = w * h + (chroma_size << 1);

	if (bufoff2)
		*bufoff2 =  w * h;

	if (bufoff3)
		*bufoff3 = 0;

	return scene_size;
}

void qimage_nv21_to_bgr888(qu8 *bgr, qu8 *y, qu8 *v, qu8 *u,
						   int s_bgr, int w, int h)
{
	int i, j;
	int c, d, e;
	int r, g, b;
	qu8 *cy, *cu, *cv;
	int gap;

	gap = (s_bgr - ROUNDUP_EVEN(w)) * 3;

	for (i = 0; i < h; i++)
	{
		cy = y + i * w;
		cv = v + (i >> 1) * w;
		cu = cv + 1;

		for (j = 0; j < w; j += 2) // assume even width
		{
			c = *cy++ - 16;
			d = *cu - 128;
			e = *cv - 128;

			cu += 2;
			cv += 2;

			b = (298 * c + 516 * d           + 128) >> 8;
			g = (298 * c - 100 * d - 208 * e + 128) >> 8;
			r = (298 * c           + 409 * e + 128) >> 8;

			*bgr++ = SAT_S32_TO_U8(b);
			*bgr++ = SAT_S32_TO_U8(g);
			*bgr++ = SAT_S32_TO_U8(r);

			c = *cy++ - 16;

			b = (298 * c + 516 * d           + 128) >> 8;
			g = (298 * c - 100 * d - 208 * e + 128) >> 8;
			r = (298 * c           + 409 * e + 128) >> 8;

			*bgr++ = SAT_S32_TO_U8(b);
			*bgr++ = SAT_S32_TO_U8(g);
			*bgr++ = SAT_S32_TO_U8(r);
		}

		bgr += gap;
	}
}

void qimage_nv21_to_420(qu8 *yuv, qu8 *y, qu8 *v, qu8 *u,
						int n1, int w, int h)
{
	size_t i;
	qu8 *d_u, *d_v;

	size_t y_size = w * h;
	size_t u_size = ((w + 1) >> 1) * ((h + 1) >> 1);

	d_u = yuv + y_size;
	d_v = d_u + u_size;

	memcpy(yuv, y, y_size);

	for (i = 0; i < u_size; i++) {
		*d_v++ = *v++;
		*d_u++ = *v++;
	}
}

int qimage_rgb888_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	int scene_size = w * h * 3;

	if (bufoff2)
		*bufoff2 = 0;

	if (bufoff3)
		*bufoff3 = 0;

	return scene_size;
}

void qimage_rgb888_to_bgr888(qu8 *bgr, qu8 *src_rgb, qu8 *n1, qu8 *n2,
							 int s_bgr, int w, int h)
{
	int i, j;
	int gap;

	gap = (s_bgr - w) * 3;

	for (i = 0; i < h; i++)
	{
		for (j = 0; j < w; j++)
		{
			*bgr++ = src_rgb[2];
			*bgr++ = src_rgb[1];
			*bgr++ = src_rgb[0];

			src_rgb += 3;
		}

		bgr += gap;
	}
}

int qimage_rgba8888_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	int scene_size = w * h * 4;

	if (bufoff2)
		*bufoff2 = 0;

	if (bufoff3)
		*bufoff3 = 0;

	return scene_size;
}

void qimage_rgba8888_to_bgr888(qu8 *bgr, qu8 *src_argb, qu8 *n1, qu8 *n2,
							 int s_bgr, int w, int h)
{
	int i, j;
	int gap;

	gap = (s_bgr - w) * 3;

	for (i = 0; i < h; i++)
	{
		for (j = 0; j < w; j++)
		{
			*bgr++ = src_argb[2];
			*bgr++ = src_argb[1];
			*bgr++ = src_argb[0];

			src_argb += 4;
		}

		bgr += gap;
	}
}

int qimage_bgr888_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	return qimage_rgb888_load_info(w, h, bufoff2, bufoff2);
}

void qimage_bgr888_to_bgr888(qu8 *bgr, qu8 *src_bgr, qu8 *n1, qu8 *n2,
							 int s_bgr, int w, int h)
{
	int i, j;
	int gap;

	gap = (s_bgr - w) * 3;

	for (i = 0; i < h; i++)
	{
		for (j = 0; j < w; j++)
		{
			*bgr++ = src_bgr[0];
			*bgr++ = src_bgr[1];
			*bgr++ = src_bgr[2];

			src_bgr += 3;
		}

		bgr += gap;
	}
}

int qimage_bgr565_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	int scene_size = w * h * 2;

	if (bufoff2)
		*bufoff2 = 0;

	if (bufoff3)
		*bufoff3 = 0;

	return scene_size;
}

void qimage_bgr565_to_bgr888(qu8 *bgr, qu8 *src_rgb, qu8 *n1, qu8 *n2,
							 int s_bgr, int w, int h)
{
	int i, j;
	int gap;

	gap = (s_bgr - w) * 3;

	for (i = 0; i < h; i++)
	{
		for (j = 0; j < w; j++)
		{
			int bgr565 = (src_rgb[1] << 8) | src_rgb[0];

			qu8 b = bgr565 & 0x1F;
			qu8 g = (bgr565 >> 5) & 0x3F;
			qu8 r = (bgr565 >> 11) & 0x1f;

			*bgr++ = (b << 3) | (1 << 2); // fill unknowns with averages
			*bgr++ = (g << 2) | (1 << 1);
			*bgr++ = (r << 3) | (1 << 2);

			src_rgb += 2;
		}

		bgr += gap;
	}
}

int qimage_rgba1010102_load_info(int w, int h, int* bufoff2, int* bufoff3)
{
	int scene_size = w * h * 4;

	if (bufoff2)
		*bufoff2 = 0;

	if (bufoff3)
		*bufoff3 = 0;

	return scene_size;
}

// https://developer.apple.com/documentation/accelerate/1533003-vimageconvert_rgba1010102toargb8
void qimage_rgba1010102_to_bgr888(qu8* bgr, qu8* src_rgb, qu8* n1, qu8* n2,
	int s_bgr, int w, int h)
{
	int i, j;
	int gap;

	gap = (s_bgr - w) * 3;
	const int RGB101010RangeMin = 0;
	const int RGB101010RangeMax = 0x3ff; // 1023
	const int range10 = RGB101010RangeMax - RGB101010RangeMin;
	const int range10_half = range10 >> 1;

	for (i = 0; i < h; i++)
	{
		for (j = 0; j < w; j++)
		{
			qu32 rgb1010102 = ((qu32 *)src_rgb)[0];

			int R10 = (rgb1010102 >> 22) & 0x3ff;
			int G10 = (rgb1010102 >> 12) & 0x3ff;
			int B10 = (rgb1010102 >>  2) & 0x3ff;

			R10 = ((R10 - RGB101010RangeMin) * UCHAR_MAX + range10_half) / range10;
			G10 = ((G10 - RGB101010RangeMin) * UCHAR_MAX + range10_half) / range10;
			B10 = ((B10 - RGB101010RangeMin) * UCHAR_MAX + range10_half) / range10;

			*bgr++ = SAT_S32_TO_U8(B10);
			*bgr++ = SAT_S32_TO_U8(G10);
			*bgr++ = SAT_S32_TO_U8(R10);

			src_rgb += 4;
		}

		bgr += gap;
	}
}

int qimage_rgb16u_load_info(int w, int h, int* bufoff2, int* bufoff3)
{
	int scene_size = w * h * 6;

	if (bufoff2)
		*bufoff2 = 0;

	if (bufoff3)
		*bufoff3 = 0;

	return scene_size;
}

// https://developer.apple.com/documentation/accelerate/1642517-vimageconvert_argb16utoargb21010
void qimage_rgb16u_to_bgr888(qu8* bgr, qu8* src_rgb, qu8* n1, qu8* n2,
	int s_bgr, int w, int h)
{
	int i, j;
	int gap;

	gap = (s_bgr - w) * 3;
	const int RGB16RangeMin = 0;
	const int RGB16RangeMax = 0xffff; // 65535
	const int range16 = RGB16RangeMax - RGB16RangeMin;
	const int range16_half = range16 >> 1;

	for (i = 0; i < h; i++)
	{
		for (j = 0; j < w; j++)
		{
			int R16 = ((qu16*)src_rgb)[0];
			int G16 = ((qu16*)src_rgb)[2];
			int B16 = ((qu16*)src_rgb)[4];

			R16 = ((R16 - RGB16RangeMin) * UCHAR_MAX + range16_half) / range16;
			G16 = ((G16 - RGB16RangeMin) * UCHAR_MAX + range16_half) / range16;
			B16 = ((B16 - RGB16RangeMin) * UCHAR_MAX + range16_half) / range16;

			*bgr++ = SAT_S32_TO_U8(B16);
			*bgr++ = SAT_S32_TO_U8(G16);
			*bgr++ = SAT_S32_TO_U8(R16);

			src_rgb += 6;
		}

		bgr += gap;
	}
}

int qimage_t256x16_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	int w_aligned = 2048;
	int h_aligned = (h + 0x1f) & ~0x1f;
	int chroma_size = ((w_aligned + 1) >> 1) * ((h_aligned + 1) >> 1);
	int scene_size = w_aligned * h_aligned + (chroma_size << 1);

	if (bufoff2)
		*bufoff2 = w_aligned * h_aligned;

	if (bufoff3)
		*bufoff3 = 0;

	return scene_size;
}

void qimage_t256x16_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
							  int s_bgr, int w, int h)
{
	int i, j, k, l;
	int c, d, e;
	int r, g, b;
	int h_aligned;

	int tile_h, tile_w, remain_h, remain_w;

	qu8 *y_t, *u_t;
	qu8 *cu, *cv, *cy;
	qu8 *bgr_t, *dst;

	const long bytes_per_pixel_rgb = 3;

	h_aligned = (h + 0x1f) & ~0x1f;

	for (i = 0; i < h_aligned; i += 16)
	{
		for (j = 0; j < 2048; j += 256)
		{
			y_t = y + (j << 4) + (i << 11);

			if (((i >> 4) & 0x1) == 0)
			{
				u_t = u + (j << 4) + (i << 10);
			}
			else
			{
				u_t = u + (j << 4) + 2048 + ((i - 16) << 10);
			}

			bgr_t = bgr + (j + i * s_bgr) * bytes_per_pixel_rgb;

			remain_h = h - i;
			if (remain_h >= 16)
				tile_h = 16;
			else
				tile_h = remain_h;

			for (k = 0; k < tile_h; k++)
			{
				dst = bgr_t + k * s_bgr * bytes_per_pixel_rgb;
				cy = y_t + (k << 8);
				cu = u_t + ((k & ~0x1) << 7);
				cv = cu + 1;

				remain_w = w - j;
				if (remain_w >= 256)
					tile_w = 256;
				else if (remain_w >= 0)
					tile_w = remain_w;
				else
					tile_w = 0;

				for (l = 0; l < tile_w; l += 2)
				{
					c = *cy++ - 16;
					d = *cu - 128;
					e = *cv - 128;

					cu += 2;
					cv += 2;

					b = (298 * c + 516 * d           + 128) >> 8;
					g = (298 * c - 100 * d - 208 * e + 128) >> 8;
					r = (298 * c           + 409 * e + 128) >> 8;

					*dst++ = SAT_S32_TO_U8(b);
					*dst++ = SAT_S32_TO_U8(g);
					*dst++ = SAT_S32_TO_U8(r);

					c = *cy++ - 16;

					b = (298 * c + 516 * d           + 128) >> 8;
					g = (298 * c - 100 * d - 208 * e + 128) >> 8;
					r = (298 * c           + 409 * e + 128) >> 8;

					*dst++ = SAT_S32_TO_U8(b);
					*dst++ = SAT_S32_TO_U8(g);
					*dst++ = SAT_S32_TO_U8(r);
				}
			}
		}
	}
}

int qimage_grayscale_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	int scene_size = w * h;

	return scene_size;
}

void qimage_grayscale_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
							 int s_bgr, int w, int h)
{
	int i, j;
	int c;
	qu8 *cy;
	int gap;

	gap  = (s_bgr - w) * 3;

	for (i = 0; i < h; i++)
	{
		cy = y + i * w;

		for (j = 0; j < w; j ++)
		{
			c = *cy++;

			*bgr++ = c;
			*bgr++ = c;
			*bgr++ = c;
		}

		bgr += gap;
	}
}

int qimage_yuv420p10_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	int chroma_size = (((w + 1) >> 1) * ((h + 1) >> 1)) << 1;
	int luma_size = (w * h) << 1;
	int scene_size = luma_size + (chroma_size << 1);

	if (bufoff2)
		*bufoff2 = luma_size;

	if (bufoff3)
		*bufoff3 = chroma_size + luma_size;

	return scene_size;
}

void qimage_yuv420p10_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
								int s_bgr, int w, int h)
{
	int i, j;
	int c, d, e;
	int r, g, b;
	qu16 *y2, *u2, *v2;
	qu16 *cy, *cu, *cv;
	qu8 y3, u3, v3;
	int gap;

	y2 = (qu16 *)y;
	u2 = (qu16 *)u;
	v2 = (qu16 *)v;

	gap = (s_bgr - ROUNDUP_EVEN(w)) * 3;

	for (i = 0; i < h; i++)
	{
		cy = y2 + i * w;
		cu = u2 + (i >> 1) * (w >> 1);
		cv = v2 + (i >> 1) * (w >> 1);

		for (j = 0; j < w; j += 2)
		{
			y3 = *cy++ >> 2;
			u3 = *cu++ >> 2;
			v3 = *cv++ >> 2;

			c = (y3 - 16) * 298;
			d = u3 - 128;
			e = v3 - 128;

			r = c           + 409 * e + 128;
			g = c - 100 * d - 208 * e + 128;
			b = c + 516 * d           + 128;

			*bgr++ = SAT_S32_TO_U8(b >> 8);
			*bgr++ = SAT_S32_TO_U8(g >> 8);
			*bgr++ = SAT_S32_TO_U8(r >> 8);

			y3 = *cy++ >> 2;

			c = (y3 - 16) * 298 - c;

			r += c;
			g += c;
			b += c;

			*bgr++ = SAT_S32_TO_U8(b >> 8);
			*bgr++ = SAT_S32_TO_U8(g >> 8);
			*bgr++ = SAT_S32_TO_U8(r >> 8);
		}

		bgr += gap;
	}
}

int qimage_p010_load_info(int w, int h, int *bufoff2, int *bufoff3)
{
	int chroma_size = (((w + 1) >> 1) * ((h + 1) >> 1)) << 1;
	int luma_size = (w * h) << 1;
	int scene_size = luma_size + (chroma_size << 1);

	if (bufoff2)
		*bufoff2 = luma_size;

	if (bufoff3)
		*bufoff3 = 0;

	return scene_size;
}

void qimage_p010_to_bgr888(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
						   int s_bgr, int w, int h)
{
	int i, j;
	int c, d, e;
	int r, g, b;
	qu16 *y2, *u2;
	qu16 *cy, *cu, *cv;
	qu8 y3, u3, v3;
	int gap;

	y2 = (qu16 *)y;
	u2 = (qu16 *)u;

	gap = (s_bgr - ROUNDUP_EVEN(w)) * 3;

	for (i = 0; i < h; i++)
	{
		cy = y2 + i * w;
		cu = u2 + (i >> 1) * w;
		cv = cu + 1;

		for (j = 0; j < w; j += 2) // assume even width
		{
			y3 = *cy++ >> 8;
			u3 = *cu >> 8;
			v3 = *cv >> 8;

			c = (y3 - 16) * 298;
			d = u3 - 128;
			e = v3 - 128;

			cu += 2;
			cv += 2;

			b = c + 516 * d           + 128;
			g = c - 100 * d - 208 * e + 128;
			r = c           + 409 * e + 128;

			*bgr++ = SAT_S32_TO_U8(b >> 8);
			*bgr++ = SAT_S32_TO_U8(g >> 8);
			*bgr++ = SAT_S32_TO_U8(r >> 8);

			y3 = *cy++ >> 8;

			c = (y3 - 16) * 298 - c;

			b += c;
			g += c;
			r += c;

			*bgr++ = SAT_S32_TO_U8(b >> 8);
			*bgr++ = SAT_S32_TO_U8(g >> 8);
			*bgr++ = SAT_S32_TO_U8(r >> 8);
		}

		bgr += gap;
	}
}

void qimage_bgr888_to_nv21(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
						   int s_bgr, int w, int h)
{
	int i, j;
	int t;
	int r, g, b;
	int gap;

	gap = (s_bgr - w) * QIMG_DST_RGB_BYTES;

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			b = *bgr++;
			g = *bgr++;
			r = *bgr++;

			t    = ( 66 * r + 129 * g +  25 * b + 128) >> 8;
			*y++ = SAT_S32_TO_U8(t + 16);
			if ((i & 0x1) == 0 && (j & 0x1) == 0) {
				t    = (112 * r -  94 * g -  18 * b + 128) >> 8;
				*v++ = SAT_S32_TO_U8(t + 128); // note: v is first
				t    = (-38 * r -  74 * g + 112 * b + 128) >> 8;
				*v++ = SAT_S32_TO_U8(t + 128);
			}
		}
		bgr += gap;
	}
}

void qimage_bgr888_to_yuv420(qu8 *bgr, qu8 *y, qu8 *u, qu8 *v,
						  int s_bgr, int w, int h)
{
	int i, j;
	int t;
	int r, g, b;
	int gap;

	gap = (s_bgr - w) * QIMG_DST_RGB_BYTES;

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			b = *bgr++;
			g = *bgr++;
			r = *bgr++;

			t    = ( 66 * r + 129 * g +  25 * b + 128) >> 8;
			*y++ = SAT_S32_TO_U8(t + 16);
			if ((i & 0x1) == 0 && (j & 0x1) == 0) {
				t    = (-38 * r -  74 * g + 112 * b + 128) >> 8;
				*u++ = SAT_S32_TO_U8(t + 128);
				t    = (112 * r -  94 * g -  18 * b + 128) >> 8;
				*v++ = SAT_S32_TO_U8(t + 128);
			}
		}
		bgr += gap;
	}
}
