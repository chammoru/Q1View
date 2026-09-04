#include <algorithm>
#include <chrono>
#include <cstdio>
#include <vector>

#include "QViewerCmn.h"

namespace {

int failures = 0;

#define CHECK_TRUE(name, condition) do { \
	if (!(condition)) { \
		std::fprintf(stderr, "FAIL: %s\n", name); \
		++failures; \
	} \
} while (0)

void TestCheckerboardAverage()
{
	const int srcW = 8;
	const int srcH = 8;
	const int srcStride = ROUNDUP_DWORD(srcW);
	const int dstW = 2;
	const int dstH = 2;
	const int dstStride = ROUNDUP_DWORD(dstW);
	std::vector<qu8> src(srcStride * srcH * 3, 0);
	std::vector<qu8> dst(dstStride * dstH * 3, 0);

	for (int y = 0; y < srcH; ++y) {
		for (int x = 0; x < srcW; ++x) {
			qu8 value = ((x + y) & 1) ? 255 : 0;
			for (int c = 0; c < 3; ++c)
				src[(y * srcStride + x) * 3 + c] = value;
		}
	}

	q1::ResizeArea(src.data(), srcH, srcW, dstH, dstW, dstStride,
		dst.data());
	for (int y = 0; y < dstH; ++y) {
		for (int x = 0; x < dstW; ++x) {
			for (int c = 0; c < 3; ++c) {
				int value = dst[(y * dstStride + x) * 3 + c];
				CHECK_TRUE("area checkerboard averages high frequency detail",
					value >= 127 && value <= 128);
			}
		}
	}
}

void TestDestinationStrideAndChannels()
{
	const int srcW = 6;
	const int srcH = 4;
	const int srcStride = ROUNDUP_DWORD(srcW);
	const int dstW = 3;
	const int dstH = 2;
	const int dstStride = 8;
	std::vector<qu8> src(srcStride * srcH * 3, 0);
	std::vector<qu8> dst(dstStride * dstH * 3, 77);

	for (int y = 0; y < srcH; ++y) {
		for (int x = 0; x < srcW; ++x) {
			src[(y * srcStride + x) * 3 + 0] = 12;
			src[(y * srcStride + x) * 3 + 1] = 34;
			src[(y * srcStride + x) * 3 + 2] = 56;
		}
	}

	q1::ResizeArea(src.data(), srcH, srcW, dstH, dstW, dstStride,
		dst.data());
	for (int y = 0; y < dstH; ++y) {
		for (int x = 0; x < dstW; ++x) {
			const qu8 *pixel = &dst[(y * dstStride + x) * 3];
			CHECK_TRUE("area resize preserves BGR channels",
				pixel[0] == 12 && pixel[1] == 34 && pixel[2] == 56);
		}
		for (int x = dstW * 3; x < dstStride * 3; ++x)
			CHECK_TRUE("area resize respects destination stride",
				dst[y * dstStride * 3 + x] == 77);
	}
}

double BenchmarkArea(int srcW, int srcH, int dstW, int dstH, int iterations)
{
	const int srcStride = ROUNDUP_DWORD(srcW);
	const int dstStride = ROUNDUP_DWORD(dstW);
	std::vector<qu8> src(static_cast<size_t>(srcStride) * srcH * 3);
	std::vector<qu8> dst(static_cast<size_t>(dstStride) * dstH * 3);
	for (size_t i = 0; i < src.size(); ++i)
		src[i] = static_cast<qu8>((i * 37u + i / 17u) & 0xffu);

	q1::ResizeArea(src.data(), srcH, srcW, dstH, dstW, dstStride,
		dst.data());
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < iterations; ++i)
		q1::ResizeArea(src.data(), srcH, srcW, dstH, dstW, dstStride,
			dst.data());
	const auto elapsed = std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - start).count();
	return elapsed / iterations;
}

} // namespace

int main()
{
	TestCheckerboardAverage();
	TestDestinationStrideAndChannels();

	const double fourK = BenchmarkArea(3840, 2160, 1920, 1080, 8);
	const double eightK = BenchmarkArea(7680, 4320, 1920, 1080, 3);
	std::printf("INTER_AREA 3840x2160 -> 1920x1080: %.3f ms/frame\n", fourK);
	std::printf("INTER_AREA 7680x4320 -> 1920x1080: %.3f ms/frame\n", eightK);
	CHECK_TRUE("4K static-image resize remains interactive", fourK < 250.0);
	CHECK_TRUE("8K static-image resize remains interactive", eightK < 750.0);

	if (failures != 0) {
		std::fprintf(stderr, "%d image scaling test(s) failed\n", failures);
		return 1;
	}
	std::puts("Image scaling tests passed");
	return 0;
}
