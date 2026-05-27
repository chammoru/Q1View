#include "qimage_metrics.h"

#include <stdlib.h>

#if defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__) || defined(__i386__)
#include <emmintrin.h>
#define QM_HAS_SSE2 1
#else
#define QM_HAS_SSE2 0
#endif

#if QM_HAS_SSE2
static qu64 psnr_row_sse2(qu8 *a, qu8 *b, int n)
{
	qu64 sum;
	qu32 lane[4];
	__m128i acc = _mm_setzero_si128();
	__m128i zero = _mm_setzero_si128();
	int j = 0;

	for (; j + 16 <= n; j += 16) {
		__m128i va = _mm_loadu_si128((const __m128i *)(a + j));
		__m128i vb = _mm_loadu_si128((const __m128i *)(b + j));
		__m128i a_lo = _mm_unpacklo_epi8(va, zero);
		__m128i a_hi = _mm_unpackhi_epi8(va, zero);
		__m128i b_lo = _mm_unpacklo_epi8(vb, zero);
		__m128i b_hi = _mm_unpackhi_epi8(vb, zero);
		__m128i d_lo = _mm_sub_epi16(a_lo, b_lo);
		__m128i d_hi = _mm_sub_epi16(a_hi, b_hi);
		// _mm_madd_epi16(x, x) widens int16 to int32 and pairs them.
		// Per-row max: (W/16) * 8 lanes * (2 * 255^2) ~ 4.16M * (W/16);
		// for W <= 8192 this is well under 2^31, so 32-bit accum is safe.
		acc = _mm_add_epi32(acc, _mm_add_epi32(
			_mm_madd_epi16(d_lo, d_lo),
			_mm_madd_epi16(d_hi, d_hi)));
	}

	_mm_storeu_si128((__m128i *)lane, acc);
	sum = (qu64)lane[0] + lane[1] + lane[2] + lane[3];

	for (; j < n; j++) {
		long d = a[j] - b[j];
		sum += (qu64)(d * d);
	}
	return sum;
}
#endif

double qPSNR(qu8 *src, qu8 *dst, int w, int h, int stride, int px_w)
{
	int i, j;
	qu64 sum_dxd = 0;
	double MSE, PMSE;
	const int MAXi = 255;

#if QM_HAS_SSE2
	if (px_w == 1) {
		for (i = 0; i < h; i++) {
			sum_dxd += psnr_row_sse2(src, dst, w);
			src += stride;
			dst += stride;
		}
		MSE = (double)sum_dxd / ((double)w * h);
		PMSE = MSE / (MAXi * MAXi);
		return -10 * log10(PMSE);
	}
#endif

	{
		int margin = (stride - w) * px_w;

		for (i = 0; i < h; i++) {
			for (j = 0; j < w; j++) {
				long d = *src - *dst;

				src += px_w;
				dst += px_w;

				sum_dxd += (qu64)(d * d);
			}

			src += margin;
			dst += margin;
		}
	}

	MSE = (double)sum_dxd / ((double)w * h);
	PMSE = MSE / (MAXi * MAXi);

	return -10 * log10(PMSE);
}

#define SIZE_N      8
#define WINDOW_SIZE (SIZE_N * SIZE_N)
#define K1          0.01f
#define K2          0.03f
#define L           255

static const double window_size_reciprocal = 1 / (double)WINDOW_SIZE;

// Integral-image (box-filter) reformulation of the naive sliding-window SSIM.
// The previous implementation rescanned each 8x8 window five times, making
// it O(W*H*64); this version keeps running column sums of s, d, s*s, d*d, s*d
// over the last SIZE_N rows and slides them horizontally to evaluate each
// window in O(1), yielding O(W*H) overall. Algebraically the variance and
// covariance use Sx^2/N - mu^2 instead of Sigma(x - mu)^2 / N: mathematically
// identical, may differ from the old code in the last few ULPs of double
// rounding.
double qSSIM(qu8 *src, qu8 *dst, int w, int h, int stride, int px_w)
{
	const double c1 = K1 * L * K1 * L;
	const double c2 = K2 * L * K2 * L;

	int real_w = stride * px_w;
	int nW = w - SIZE_N + 1;
	int nH = h - SIZE_N + 1;
	int r, c, j;

	qu32 *col_s, *col_d, *col_ss, *col_dd, *col_sd;
	double total_ssim = 0.0;

	if (nW <= 0 || nH <= 0)
		return 0.0;

	col_s  = (qu32 *)malloc(sizeof(qu32) * w);
	col_d  = (qu32 *)malloc(sizeof(qu32) * w);
	col_ss = (qu32 *)malloc(sizeof(qu32) * w);
	col_dd = (qu32 *)malloc(sizeof(qu32) * w);
	col_sd = (qu32 *)malloc(sizeof(qu32) * w);
	if (!col_s || !col_d || !col_ss || !col_dd || !col_sd) {
		free(col_s); free(col_d); free(col_ss); free(col_dd); free(col_sd);
		return 0.0;
	}

	// Seed column sums over rows [0, SIZE_N).
	for (j = 0; j < w; j++) {
		col_s[j] = 0; col_d[j] = 0;
		col_ss[j] = 0; col_dd[j] = 0;
		col_sd[j] = 0;
	}
	for (r = 0; r < SIZE_N; r++) {
		qu8 *sp = src + r * real_w;
		qu8 *dp = dst + r * real_w;
		for (j = 0; j < w; j++) {
			qu32 s = sp[j * px_w];
			qu32 d = dp[j * px_w];
			col_s[j]  += s;
			col_d[j]  += d;
			col_ss[j] += s * s;
			col_dd[j] += d * d;
			col_sd[j] += s * d;
		}
	}

	for (r = 0; r < nH; r++) {
		qu64 win_s = 0, win_d = 0, win_ss = 0, win_dd = 0, win_sd = 0;

		for (j = 0; j < SIZE_N; j++) {
			win_s  += col_s[j];
			win_d  += col_d[j];
			win_ss += col_ss[j];
			win_dd += col_dd[j];
			win_sd += col_sd[j];
		}

		for (c = 0; c < nW; c++) {
			double u1 = (double)win_s  * window_size_reciprocal;
			double u2 = (double)win_d  * window_size_reciprocal;
			double a1 = (double)win_ss * window_size_reciprocal - u1 * u1;
			double a2 = (double)win_dd * window_size_reciprocal - u2 * u2;
			double cov = (double)win_sd * window_size_reciprocal - u1 * u2;

			double ssim = ((2 * u1 * u2 + c1) * (2 * cov + c2)) /
				((u1 * u1 + u2 * u2 + c1) * (a1 + a2 + c2));
			total_ssim += ssim;

			if (c + SIZE_N < w) {
				win_s  = win_s  + col_s[c + SIZE_N]  - col_s[c];
				win_d  = win_d  + col_d[c + SIZE_N]  - col_d[c];
				win_ss = win_ss + col_ss[c + SIZE_N] - col_ss[c];
				win_dd = win_dd + col_dd[c + SIZE_N] - col_dd[c];
				win_sd = win_sd + col_sd[c + SIZE_N] - col_sd[c];
			}
		}

		if (r + 1 < nH) {
			qu8 *sp_old = src + r * real_w;
			qu8 *dp_old = dst + r * real_w;
			qu8 *sp_new = src + (r + SIZE_N) * real_w;
			qu8 *dp_new = dst + (r + SIZE_N) * real_w;
			for (j = 0; j < w; j++) {
				qu32 s_old = sp_old[j * px_w];
				qu32 d_old = dp_old[j * px_w];
				qu32 s_new = sp_new[j * px_w];
				qu32 d_new = dp_new[j * px_w];
				col_s[j]  += s_new       - s_old;
				col_d[j]  += d_new       - d_old;
				col_ss[j] += s_new * s_new - s_old * s_old;
				col_dd[j] += d_new * d_new - d_old * d_old;
				col_sd[j] += s_new * d_new - s_old * d_old;
			}
		}
	}

	free(col_s); free(col_d); free(col_ss); free(col_dd); free(col_sd);

	return total_ssim / (nW * nH);
}
