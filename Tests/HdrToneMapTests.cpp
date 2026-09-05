#include "../Viewer/HdrToneMap.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

static void Require(bool condition, const char* text)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", text);
		std::exit(1);
	}
}

static std::vector<std::uint8_t> ToneMap(const std::vector<std::uint8_t>& source,
	int width, int height)
{
	std::vector<std::uint8_t> result(source.size());
	q1::ToneMapOpenCvHlgBgrRows(source.data(), width, result.data(), width,
		width, 0, height);
	return result;
}

static void TestNeutralRamp()
{
	std::vector<std::uint8_t> source;
	for (int value : {0, 32, 64, 96, 128, 160, 192, 224, 255}) {
		source.push_back(static_cast<std::uint8_t>(value));
		source.push_back(static_cast<std::uint8_t>(value));
		source.push_back(static_cast<std::uint8_t>(value));
	}
	auto result = ToneMap(source, 9, 1);
	int previous = -1;
	for (int i = 0; i < 9; ++i) {
		const int blue = result[i * 3];
		const int green = result[i * 3 + 1];
		const int red = result[i * 3 + 2];
		Require(std::abs(blue - green) <= 1 && std::abs(green - red) <= 1,
			"neutral HLG samples remain neutral");
		Require(blue >= previous, "neutral HLG ramp remains monotonic");
		previous = blue;
	}
	Require(result[0] == 0 && result[1] == 0 && result[2] == 0,
		"HLG black remains SDR black");
}

static void TestInPlace()
{
	std::vector<std::uint8_t> source(37 * 19 * 3);
	for (size_t i = 0; i < source.size(); ++i)
		source[i] = static_cast<std::uint8_t>((i * 73 + 19) & 255);
	const auto expected = ToneMap(source, 37, 19);
	q1::ToneMapOpenCvHlgBgrRows(source.data(), 37, source.data(), 37, 37, 0, 19);
	Require(source == expected, "HLG LUT supports exact in-place conversion");
}

static void CheckRotation(int degrees)
{
	const int width = 16;
	const int height = 12;
	std::vector<std::uint8_t> source(width * height * 3);
	for (size_t i = 0; i < source.size(); ++i)
		source[i] = static_cast<std::uint8_t>((i * 29 + i / 7) & 255);
	const auto corrected = ToneMap(source, width, height);
	const int outputWidth = degrees == 180 ? width : height;
	const int outputHeight = degrees == 180 ? height : width;
	std::vector<std::uint8_t> actual(outputWidth * outputHeight * 3, 0);
	q1::ToneMapOpenCvHlgBgrRotateTiles(source.data(), width, height,
		actual.data(), outputWidth, degrees, 0, (width + 7) / 8);
	std::vector<std::uint8_t> expected(actual.size(), 0);
	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			int outputX = 0;
			int outputY = 0;
			if (degrees == 90) { outputX = height - y - 1; outputY = x; }
			else if (degrees == 180) { outputX = width - x - 1; outputY = height - y - 1; }
			else { outputX = y; outputY = width - x - 1; }
			std::memcpy(&expected[(outputY * outputWidth + outputX) * 3],
				&corrected[(y * width + x) * 3], 3);
		}
	}
	Require(actual == expected, "fused HLG tone mapping preserves cardinal rotation mapping");
}

int main()
{
	q1::PrepareOpenCvHlgToSdrLut();
	TestNeutralRamp();
	TestInPlace();
	CheckRotation(90);
	CheckRotation(180);
	CheckRotation(270);
	std::puts("HDR tone-map tests passed");
	return 0;
}
