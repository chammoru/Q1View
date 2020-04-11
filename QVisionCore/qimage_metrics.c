#include "qimage_metrics.h"

double PSNR(qu8 *src, qu8 *dst, int w, int h, int stride, int px_w)
{
	int i, j;
	qu32 sum_dxd = 0;
	double MSE, PMSE;
	const int MAXi = 255; 
	int margin = stride - w;

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			long d = *src - *dst;

			src += px_w;
			dst += px_w;

			sum_dxd += d * d;
		}

		src += 3 * margin;
		dst += 3 * margin;
	}

	MSE = (double)sum_dxd / (w * h);
	PMSE = MSE / (MAXi * MAXi);

	return -10 * log10(PMSE);
}

#define SIZE_N      8
#define WINDOW_SIZE (SIZE_N * SIZE_N)
#define K1          0.01f
#define K2          0.03f
#define L           255

static const double window_size_reciprocal = 1 / (double)WINDOW_SIZE;

static double ssim_average(qu8 *src, int real_w, int px_w)
{
	int total = 0;
	int i, j;
	double average;

	for (i = 0; i < SIZE_N; i++) {
		for (j = 0; j < SIZE_N; j++)
			total += src[j * px_w];

		src += real_w;
	}

	average = total * window_size_reciprocal;

	return average;
}

static double ssim_variance(qu8 *src, int real_w, double average, int px_w)
{
	int i, j;
	double t, sum_square = 0, variance;

	for (i = 0; i < SIZE_N; i++) {
		for (j = 0; j < SIZE_N; j++) {
			t = src[j * px_w] - average;
			sum_square += t * t;
		}
		src += real_w;
	}

	variance = sum_square * window_size_reciprocal;

	return variance;
}

static double ssim_covariance(qu8 *src, qu8 *dst, int real_w,
							  double s_average, double d_average, int px_w)
{
	int i, j;
	double total = 0, covariance;

	for (i = 0; i < SIZE_N; i++) {
		for (j = 0; j < SIZE_N; j++) {
			int idx = j * px_w;

			total += (src[idx] - s_average) * (dst[idx] - d_average);
		}

		src += real_w;
		dst += real_w;
	}

	covariance = total * window_size_reciprocal;

	return covariance;
}

static double ssim_window(qu8 *src, qu8 *dst, int real_w, int px_w)
{
	const double c1 = K1 * L * K1 * L;
	const double c2 = K2 * L * K2 * L;

	double u1 = ssim_average(src, real_w, px_w);
	double u2 = ssim_average(dst, real_w, px_w);

	double a1 = ssim_variance(src, real_w, u1, px_w);
	double a2 = ssim_variance(dst, real_w, u2, px_w);

	double covariance = ssim_covariance(src, dst, real_w, u1, u2, px_w);

	double ssim = ((2 * u1 * u2 + c1) * (2 * covariance + c2)) /
		((u1 * u1 + u2 * u2 + c1) * (a1 + a2 + c2));

	return ssim;
}


double SSIM(qu8 *src, qu8 *dst, int w, int h, int stride, int px_w)
{
	double mean_ssim, total_ssim = 0;
	int real_w = w * px_w;
	int i, j;
	int nW = w - SIZE_N + 1;
	int nH = h - SIZE_N + 1;

	// SIZE_N x SIZE_N linear sliding window
	// My implementation of SSIM is certainly inefficient,
	// but it gives values and is easy to understand.
	for (i = 0; i < nH; i++) {
		qu8 *src_t, *dst_t;
		int h_hop = real_w * i;

		src_t = src + h_hop;
		dst_t = dst + h_hop;

		for (j = 0; j < nW; j++, src_t += px_w, dst_t += px_w)
			total_ssim += ssim_window(src_t, dst_t, real_w, px_w);
	}

	mean_ssim = total_ssim / (nW * nH);

	return mean_ssim;
}
