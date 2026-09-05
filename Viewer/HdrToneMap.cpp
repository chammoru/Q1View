#include "HdrToneMap.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <thread>
#include <vector>

namespace {

constexpr int kCurveMaximum = 4096;
constexpr int kColorBits = 7;
constexpr int kColorLevels = 1 << kColorBits;
constexpr int kColorMask = kColorLevels - 1;

float Clamp(float value)
{
	return std::max(0.0f, std::min(value, 1.0f));
}

int CurveIndex(float value)
{
	return static_cast<int>(Clamp(value) * kCurveMaximum + 0.5f);
}

struct HlgToSdrLut
{
	std::array<float, kCurveMaximum + 1> hlgScene;
	std::array<float, kCurveMaximum + 1> hlgSceneGamma;
	std::array<float, kCurveMaximum + 1> ootfGammaScale;
	std::array<float, kCurveMaximum + 1> toneMappedLuma;
	std::array<float, kCurveMaximum + 1> gammaToLinear;
	std::array<std::uint8_t, kCurveMaximum + 1> linearToGamma8;
	std::vector<std::uint32_t> colors;

	HlgToSdrLut() : colors(size_t(kColorLevels) * kColorLevels * kColorLevels)
	{
		BuildCurves();
		const unsigned threadCount = std::min<unsigned>(
			std::max(1u, std::thread::hardware_concurrency()), kColorLevels);
		std::vector<std::thread> workers;
		workers.reserve(threadCount);
		for (unsigned thread = 0; thread < threadCount; ++thread) {
			workers.emplace_back([this, thread, threadCount] {
				for (int red = int(thread); red < kColorLevels; red += int(threadCount))
					BuildRedSlice(red);
			});
		}
		for (std::thread& worker : workers)
			worker.join();
	}

	void BuildCurves()
	{
		constexpr float a = 0.17883277f;
		constexpr float b = 0.28466892f;
		constexpr float c = 0.55991073f;
		const float rhoHdr = 1.0f + 32.0f * std::pow(0.1f, 1.0f / 2.4f);
		const float rhoSdr = 1.0f + 32.0f * std::pow(0.01f, 1.0f / 2.4f);
		const float logRhoHdr = std::log(rhoHdr);
		for (int i = 0; i <= kCurveMaximum; ++i) {
			const float value = float(i) / kCurveMaximum;
			const float scene = value <= 0.5f ? value * value / 3.0f :
				(std::exp((value - c) / a) + b) / 12.0f;
			hlgScene[i] = scene;
			hlgSceneGamma[i] = std::pow(scene, 1.0f / 2.4f);
			// At a 1,000-nit HLG reference display, gamma=1.2. After the
			// following 1/2.4 encoding the common OOTF multiplier is Y^(1/12).
			ootfGammaScale[i] = i == 0 ? 0.0f : std::pow(value, 1.0f / 12.0f);
			const float perceptual = std::log(1.0f + (rhoHdr - 1.0f) * value) /
				logRhoHdr;
			float compressed;
			if (perceptual <= 0.7399f)
				compressed = 1.0770f * perceptual;
			else if (perceptual < 0.9909f)
				compressed = -1.1510f * perceptual * perceptual +
					2.7811f * perceptual - 0.6302f;
			else
				compressed = 0.5f * perceptual + 0.5f;
			toneMappedLuma[i] = (std::pow(rhoSdr, compressed) - 1.0f) /
				(rhoSdr - 1.0f);
			gammaToLinear[i] = std::pow(value, 2.4f);
			linearToGamma8[i] = static_cast<std::uint8_t>(
				std::lround(std::pow(value, 1.0f / 2.4f) * 255.0f));
		}
	}

	std::uint32_t Convert(float b601, float g601, float r601) const
	{
		// The bundled OpenCV/libswscale produces full-range RGB using its
		// default BT.601 matrix. Recover Y'CbCr, then apply BT.2020 NCL.
		const float y = 0.2990f * r601 + 0.5870f * g601 + 0.1140f * b601;
		const float cb = (b601 - y) / 1.7720f;
		const float cr = (r601 - y) / 1.4020f;
		const float rHlg = Clamp(y + 1.4746f * cr);
		const float gHlg = Clamp(y - 0.164553f * cb - 0.571353f * cr);
		const float bHlg = Clamp(y + 1.8814f * cb);
		const int rIndex = CurveIndex(rHlg);
		const int gIndex = CurveIndex(gHlg);
		const int bIndex = CurveIndex(bHlg);
		const float sceneLuma = 0.2627f * hlgScene[rIndex] +
			0.6780f * hlgScene[gIndex] + 0.0593f * hlgScene[bIndex];
		const float ootfScale = ootfGammaScale[CurveIndex(sceneLuma)];
		const float rHdr = hlgSceneGamma[rIndex] * ootfScale;
		const float gHdr = hlgSceneGamma[gIndex] * ootfScale;
		const float bHdr = hlgSceneGamma[bIndex] * ootfScale;

		// ITU-R BT.2446 Method A: 1,000 cd/m2 HDR to 100 cd/m2 SDR.
		const float yHdr = Clamp(0.2627f * rHdr + 0.6780f * gHdr + 0.0593f * bHdr);
		const float ySdr = toneMappedLuma[CurveIndex(yHdr)];
		const float colorScale = yHdr > 1.0e-6f ? ySdr / (1.1f * yHdr) : 0.0f;
		const float cbSdr = colorScale * (bHdr - yHdr) / 1.8814f;
		const float crSdr = colorScale * (rHdr - yHdr) / 1.4746f;
		const float yAdjusted = ySdr - std::max(0.1f * crSdr, 0.0f);
		const float r2020Gamma = Clamp(yAdjusted + 1.4746f * crSdr);
		const float b2020Gamma = Clamp(yAdjusted + 1.8814f * cbSdr);
		const float g2020Gamma = Clamp((yAdjusted - 0.2627f * r2020Gamma -
			0.0593f * b2020Gamma) / 0.6780f);

		// The desktop surface is BT.709/sRGB, so convert primaries in linear
		// light after tone mapping and then encode with the Method A gamma.
		const float r2020 = gammaToLinear[CurveIndex(r2020Gamma)];
		const float g2020 = gammaToLinear[CurveIndex(g2020Gamma)];
		const float b2020 = gammaToLinear[CurveIndex(b2020Gamma)];
		// HLG reference white is 203 cd/m2 on the 1,000-nit reference display;
		// the legacy desktop surface represents 100 cd/m2 SDR. Scale in linear
		// light before encoding, matching the Windows video processor's SDR
		// adaptation instead of leaving diffuse whites elevated and washed out.
		constexpr float sdrWhiteScale = 100.0f / 203.0f;
		const float r709 = Clamp((1.660491f * r2020 - 0.587641f * g2020 - 0.072850f * b2020) * sdrWhiteScale);
		const float g709 = Clamp((-0.124550f * r2020 + 1.132899f * g2020 - 0.008349f * b2020) * sdrWhiteScale);
		const float b709 = Clamp((-0.018151f * r2020 - 0.100579f * g2020 + 1.118730f * b2020) * sdrWhiteScale);
		return std::uint32_t(linearToGamma8[CurveIndex(b709)]) |
			(std::uint32_t(linearToGamma8[CurveIndex(g709)]) << 8) |
			(std::uint32_t(linearToGamma8[CurveIndex(r709)]) << 16);
	}

	void BuildRedSlice(int red)
	{
		const float r = float(red == kColorMask ? 255 : red * 2) / 255.0f;
		for (int green = 0; green < kColorLevels; ++green) {
			const float g = float(green == kColorMask ? 255 : green * 2) / 255.0f;
			for (int blue = 0; blue < kColorLevels; ++blue) {
				const float b = float(blue == kColorMask ? 255 : blue * 2) / 255.0f;
				colors[(red << (kColorBits * 2)) | (green << kColorBits) | blue] =
					Convert(b, g, r);
			}
		}
	}
};

const HlgToSdrLut& ColorLut()
{
	static const HlgToSdrLut lut;
	return lut;
}

} // namespace

namespace q1 {

void PrepareOpenCvHlgToSdrLut()
{
	(void)ColorLut();
}

void ToneMapOpenCvHlgBgrRows(const std::uint8_t* source,
	int sourceStridePixels, std::uint8_t* destination,
	int destinationStridePixels, int width, int rowBegin, int rowEnd)
{
	if (!source || !destination || sourceStridePixels < width ||
		destinationStridePixels < width || width <= 0 || rowBegin < 0 ||
		rowEnd < rowBegin) {
		return;
	}
	const std::vector<std::uint32_t>& colors = ColorLut().colors;
	for (int row = rowBegin; row < rowEnd; ++row) {
		const std::uint8_t* input = source + size_t(row) * sourceStridePixels * 3;
		std::uint8_t* output = destination + size_t(row) * destinationStridePixels * 3;
		for (int column = 0; column < width; ++column) {
			const int index = ((input[2] >> 1) << (kColorBits * 2)) |
				((input[1] >> 1) << kColorBits) | (input[0] >> 1);
			const std::uint32_t color = colors[index];
			output[0] = static_cast<std::uint8_t>(color);
			output[1] = static_cast<std::uint8_t>(color >> 8);
			output[2] = static_cast<std::uint8_t>(color >> 16);
			input += 3;
			output += 3;
		}
	}
}

void ToneMapOpenCvHlgBgrRotateTiles(const std::uint8_t* source,
	int sourceWidth, int sourceHeight, std::uint8_t* destination,
	int destinationStridePixels, int clockwiseDegrees,
	int tileBegin, int tileEnd)
{
	if (!source || !destination || sourceWidth <= 0 || sourceHeight <= 0 ||
		destinationStridePixels <= 0 || tileBegin < 0 || tileEnd < tileBegin ||
		(clockwiseDegrees != 90 && clockwiseDegrees != 180 && clockwiseDegrees != 270)) {
		return;
	}
	const std::vector<std::uint32_t>& colors = ColorLut().colors;
	constexpr int tileSize = 8;
	const size_t sourceStride = size_t(sourceWidth) * 3;
	const size_t destinationStride = size_t(destinationStridePixels) * 3;
	if (clockwiseDegrees == 180) {
		for (int tileIndex = tileBegin; tileIndex < tileEnd; ++tileIndex) {
			const int xBegin = tileIndex * tileSize;
			const int xEnd = std::min(sourceWidth, xBegin + tileSize);
			for (int sourceY = 0; sourceY < sourceHeight; ++sourceY) {
				const std::uint8_t* input = source + size_t(sourceY) * sourceStride +
					size_t(xBegin) * 3;
				std::uint8_t* output = destination +
					size_t(sourceHeight - sourceY - 1) * destinationStride +
					size_t(sourceWidth - xBegin - 1) * 3;
				for (int sourceX = xBegin; sourceX < xEnd; ++sourceX,
					input += 3, output -= 3) {
					const int index = ((input[2] >> 1) << (kColorBits * 2)) |
						((input[1] >> 1) << kColorBits) | (input[0] >> 1);
					const std::uint32_t color = colors[index];
					output[0] = static_cast<std::uint8_t>(color);
					output[1] = static_cast<std::uint8_t>(color >> 8);
					output[2] = static_cast<std::uint8_t>(color >> 16);
				}
			}
		}
		return;
	}
	for (int tileIndex = tileBegin; tileIndex < tileEnd; ++tileIndex) {
		const int xBegin = tileIndex * tileSize;
		const int xEnd = std::min(sourceWidth, xBegin + tileSize);
		for (int yBegin = 0; yBegin < sourceHeight; yBegin += tileSize) {
			const int yEnd = std::min(sourceHeight, yBegin + tileSize);
			for (int sourceX = xBegin; sourceX < xEnd; ++sourceX) {
				const int outputY = clockwiseDegrees == 90 ? sourceX :
					sourceWidth - sourceX - 1;
				const int firstSourceY = clockwiseDegrees == 90 ? yEnd - 1 : yBegin;
				const int firstOutputX = clockwiseDegrees == 90 ? sourceHeight - yEnd :
					yBegin;
				const ptrdiff_t inputStep = clockwiseDegrees == 90 ?
					-ptrdiff_t(sourceStride) : ptrdiff_t(sourceStride);
				const std::uint8_t* input = source + size_t(firstSourceY) * sourceStride +
					size_t(sourceX) * 3;
				std::uint8_t* output = destination + size_t(outputY) * destinationStride +
					size_t(firstOutputX) * 3;
				for (int sourceY = yBegin; sourceY < yEnd; ++sourceY,
					input += inputStep, output += 3) {
					const int index = ((input[2] >> 1) << (kColorBits * 2)) |
						((input[1] >> 1) << kColorBits) | (input[0] >> 1);
					const std::uint32_t color = colors[index];
					output[0] = static_cast<std::uint8_t>(color);
					output[1] = static_cast<std::uint8_t>(color >> 8);
					output[2] = static_cast<std::uint8_t>(color >> 16);
				}
			}
		}
	}
}

} // namespace q1
