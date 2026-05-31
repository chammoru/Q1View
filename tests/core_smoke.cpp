#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#include "SMutex.h"
#include "qimage_cs.h"
#include "qimage_metrics.h"
#include "qimage_util.h"

int main()
{
	int off2 = 0;
	int off3 = 0;
	const int w = 4;
	const int h = 4;
	const int frameSize = qimage_yuv420_load_info(w, h, &off2, &off3);

	if (frameSize <= 0 || off2 <= 0 || off3 <= off2) {
		std::cerr << "invalid yuv420 layout\n";
		return 1;
	}

	std::vector<qu8> yuv(static_cast<size_t>(frameSize), 128);
	std::vector<qu8> bgr(static_cast<size_t>(ROUNDUP_DWORD(w) * h * QIMG_DST_RGB_BYTES), 0);

	qimage_yuv420_to_bgr888(
		bgr.data(),
		yuv.data(),
		yuv.data() + off2,
		yuv.data() + off3,
		ROUNDUP_DWORD(w) * QIMG_DST_RGB_BYTES,
		w,
		h);

	std::vector<qu8> bgrCopy = bgr;
	const double psnr = qPSNR(
		bgr.data(),
		bgrCopy.data(),
		w,
		h,
		ROUNDUP_DWORD(w),
		QIMG_DST_RGB_BYTES);

	if (!std::isinf(psnr) && (!std::isfinite(psnr) || psnr < 100.0)) {
		std::cerr << "unexpected PSNR: " << psnr << "\n";
		return 1;
	}

	std::vector<qu8> crop(static_cast<size_t>(w * QIMG_DST_RGB_BYTES), 0);
	qimage_crop(
		bgr.data(),
		ROUNDUP_DWORD(w) * QIMG_DST_RGB_BYTES,
		crop.data(),
		0,
		0,
		w * QIMG_DST_RGB_BYTES,
		1);

	SMutex mutex;
	if (mutex.lock() != 0) {
		std::cerr << "mutex lock failed\n";
		return 1;
	}
	mutex.unlock();

	std::cout << "q1view core smoke test passed\n";
	return 0;
}
