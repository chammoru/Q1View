#include <algorithm>
#include <chrono>
#include <cstdio>
#include <utility>
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

void TestLinearDestinationStrideAndChannels()
{
	const int srcW = 6;
	const int srcH = 4;
	const int srcStride = ROUNDUP_DWORD(srcW);
	const int dstW = 4;
	const int dstH = 3;
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

	q1::ResizeLinear(src.data(), srcH, srcW, dstH, dstW, dstStride,
		dst.data());
	for (int y = 0; y < dstH; ++y) {
		for (int x = 0; x < dstW; ++x) {
			const qu8 *pixel = &dst[(y * dstStride + x) * 3];
			CHECK_TRUE("linear resize preserves BGR channels",
				pixel[0] == 12 && pixel[1] == 34 && pixel[2] == 56);
		}
		for (int x = dstW * 3; x < dstStride * 3; ++x)
			CHECK_TRUE("linear resize respects destination stride",
				dst[y * dstStride * 3 + x] == 77);
	}
}

void TestScalingModePolicy()
{
	using q1::ImageScalingFilter;
	using q1::ImageScalingMode;

	CHECK_TRUE("Auto uses bilinear for moderate still-image reduction",
		q1::ResolveImageScalingFilter(ImageScalingMode::Auto, 0.70, true, true) ==
		ImageScalingFilter::Bilinear);
	CHECK_TRUE("Auto uses area for a 2x still-image reduction",
		q1::ResolveImageScalingFilter(ImageScalingMode::Auto, 0.50, true, true) ==
		ImageScalingFilter::Area);
	CHECK_TRUE("Auto uses area for strong still-image reduction",
		q1::ResolveImageScalingFilter(ImageScalingMode::Auto, 0.25, true, true) ==
		ImageScalingFilter::Area);
	CHECK_TRUE("Auto uses nearest at native size",
		q1::ResolveImageScalingFilter(ImageScalingMode::Auto, 1.0, true, true) ==
		ImageScalingFilter::Nearest);
	CHECK_TRUE("Auto preserves timed-source playback cost",
		q1::ResolveImageScalingFilter(ImageScalingMode::Auto, 0.25, false, true) ==
		ImageScalingFilter::Nearest);
	CHECK_TRUE("Qt Auto uses bilinear when area sampling is unavailable",
		q1::ResolveImageScalingFilter(ImageScalingMode::Auto, 0.25, true, false) ==
		ImageScalingFilter::Bilinear);
	CHECK_TRUE("Smooth is bilinear at high zoom",
		q1::ResolveImageScalingFilter(ImageScalingMode::Smooth, 48.0, true, true) ==
		ImageScalingFilter::Bilinear);
	CHECK_TRUE("Pixel Exact is nearest while shrinking",
		q1::ResolveImageScalingFilter(ImageScalingMode::PixelExact, 0.25, true, true) ==
		ImageScalingFilter::Nearest);

	ImageScalingMode mode = ImageScalingMode::Auto;
	mode = q1::NextImageScalingMode(mode);
	CHECK_TRUE("Auto cycles to Smooth", mode == ImageScalingMode::Smooth);
	mode = q1::NextImageScalingMode(mode);
	CHECK_TRUE("Smooth cycles to Pixel Exact", mode == ImageScalingMode::PixelExact);
	mode = q1::NextImageScalingMode(mode);
	CHECK_TRUE("Pixel Exact cycles to Auto", mode == ImageScalingMode::Auto);
}

void TestBgrRotation()
{
	const int w = 3, h = 2;
	std::vector<qu8> source(w * h * 3, 0);
	for (int i = 0; i < w * h; ++i) {
		source[i * 3] = static_cast<qu8>(i + 1);
		source[i * 3 + 1] = static_cast<qu8>(i + 21);
		source[i * 3 + 2] = static_cast<qu8>(i + 41);
	}
	auto check = [&](int degrees, int outputW, int outputH,
		const std::vector<int>& expected) {
		const int stride = ROUNDUP_DWORD(outputW);
		std::vector<qu8> destination(stride * outputH * 3, 77);
		q1::RotateBgr(source.data(), h, w, degrees, stride, destination.data());
		for (int y = 0; y < outputH; ++y) {
			for (int x = 0; x < outputW; ++x) {
				const int sourceValue = expected[y * outputW + x];
				const qu8* pixel = &destination[(y * stride + x) * 3];
				CHECK_TRUE("BGR rotation preserves direction and channels",
					pixel[0] == sourceValue && pixel[1] == sourceValue + 20 &&
					pixel[2] == sourceValue + 40);
			}
			for (int x = outputW * 3; x < stride * 3; ++x)
				CHECK_TRUE("BGR rotation respects destination stride",
					destination[y * stride * 3 + x] == 77);
		}
	};
	check(90, 2, 3, {4, 1, 5, 2, 6, 3});
	check(180, 3, 2, {6, 5, 4, 3, 2, 1});
	check(270, 2, 3, {3, 6, 2, 5, 1, 4});

	// Exercise complete and partial cache tiles in both orientations. The small
	// exact-value fixture above cannot cross the optimized path's tile boundary.
	for (const auto dimensions : { std::pair<int, int>(17, 10), std::pair<int, int>(10, 17) }) {
		const int tiledW = dimensions.first;
		const int tiledH = dimensions.second;
		std::vector<qu8> tiledSource(size_t(tiledW) * tiledH * 3);
		for (int y = 0; y < tiledH; ++y) {
			for (int x = 0; x < tiledW; ++x) {
				qu8* pixel = &tiledSource[(size_t(y) * tiledW + x) * 3];
				pixel[0] = static_cast<qu8>((x + y * 19) & 255);
				pixel[1] = static_cast<qu8>((x * 7 + y * 3) & 255);
				pixel[2] = static_cast<qu8>((x * 5 + y * 11) & 255);
			}
		}
		for (const int degrees : { 90, 270 }) {
			const int outputW = tiledH;
			const int outputH = tiledW;
			const int stride = ROUNDUP_DWORD(outputW);
			std::vector<qu8> destination(size_t(stride) * outputH * 3, 77);
			q1::RotateBgr(tiledSource.data(), tiledH, tiledW, degrees, stride, destination.data());
			for (int outputY = 0; outputY < outputH; ++outputY) {
				for (int outputX = 0; outputX < outputW; ++outputX) {
					const int sourceX = degrees == 90 ? outputY : tiledW - outputY - 1;
					const int sourceY = degrees == 90 ? tiledH - outputX - 1 : outputX;
					const qu8* actual = &destination[(size_t(outputY) * stride + outputX) * 3];
					const qu8* expected = &tiledSource[(size_t(sourceY) * tiledW + sourceX) * 3];
					CHECK_TRUE("tiled BGR rotation matches coordinate transform",
						actual[0] == expected[0] && actual[1] == expected[1] && actual[2] == expected[2]);
				}
			}
		}
	}
}

double BenchmarkRotation(int iterations)
{
	const int w = 1920, h = 1080, outputStride = ROUNDUP_DWORD(h);
	std::vector<qu8> source(size_t(w) * h * 3, 123);
	std::vector<qu8> destination(size_t(outputStride) * w * 3);
	q1::RotateBgr(source.data(), h, w, 90, outputStride, destination.data());
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < iterations; ++i)
		q1::RotateBgr(source.data(), h, w, 90, outputStride, destination.data());
	return std::chrono::duration<double, std::milli>(
		std::chrono::steady_clock::now() - start).count() / iterations;
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

double BenchmarkLinear(int srcW, int srcH, int dstW, int dstH, int iterations)
{
	const int srcStride = ROUNDUP_DWORD(srcW);
	const int dstStride = ROUNDUP_DWORD(dstW);
	std::vector<qu8> src(static_cast<size_t>(srcStride) * srcH * 3);
	std::vector<qu8> dst(static_cast<size_t>(dstStride) * dstH * 3);
	for (size_t i = 0; i < src.size(); ++i)
		src[i] = static_cast<qu8>((i * 37u + i / 17u) & 0xffu);

	q1::ResizeLinear(src.data(), srcH, srcW, dstH, dstW, dstStride,
		dst.data());
	const auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < iterations; ++i)
		q1::ResizeLinear(src.data(), srcH, srcW, dstH, dstW, dstStride,
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
	TestLinearDestinationStrideAndChannels();
	TestScalingModePolicy();
	TestBgrRotation();

	const double fourK = BenchmarkArea(3840, 2160, 1920, 1080, 8);
	const double eightK = BenchmarkArea(7680, 4320, 1920, 1080, 3);
	const double eightKLinear = BenchmarkLinear(7680, 4320, 1920, 1080, 3);
	const double moderateDocument = BenchmarkLinear(1075, 1484, 752, 1039, 20);
	const double fullHdRotation = BenchmarkRotation(30);
	std::printf("INTER_AREA 3840x2160 -> 1920x1080: %.3f ms/frame\n", fourK);
	std::printf("INTER_AREA 7680x4320 -> 1920x1080: %.3f ms/frame\n", eightK);
	std::printf("INTER_LINEAR 7680x4320 -> 1920x1080: %.3f ms/frame\n", eightKLinear);
	std::printf("INTER_LINEAR 1075x1484 -> 752x1039: %.3f ms/frame\n", moderateDocument);
	std::printf("ROTATE_90 1920x1080: %.3f ms/frame\n", fullHdRotation);
	CHECK_TRUE("4K static-image resize remains interactive", fourK < 250.0);
	CHECK_TRUE("8K static-image resize remains interactive", eightK < 750.0);
	CHECK_TRUE("8K Smooth resize remains interactive", eightKLinear < 250.0);
	CHECK_TRUE("moderate document resize remains interactive", moderateDocument < 50.0);
	CHECK_TRUE("Full HD video rotation remains real-time capable", fullHdRotation < 16.0);

	if (failures != 0) {
		std::fprintf(stderr, "%d image scaling test(s) failed\n", failures);
		return 1;
	}
	std::puts("Image scaling tests passed");
	return 0;
}
