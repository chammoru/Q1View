#include "QHeifUtil.h"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <cstring>
#include <cstdint>
#include <cwchar>
#include <cwctype>

#ifdef Q1VIEW_ENABLE_LIBHEIF
#include <libheif/heif.h>
#endif

namespace q1 {

static bool equalsBrand(const uchar *brand, const char *candidate)
{
	return std::memcmp(brand, candidate, 4) == 0;
}

static bool isHeifBrand(const uchar *brand)
{
	static const char *brands[] = {
		"heic", "heix", "hevc", "hevx",
		"heim", "heis", "hevm", "hevs",
		"mif1", "msf1", "avif", "avis",
	};

	for (size_t i = 0; i < sizeof(brands) / sizeof(brands[0]); ++i) {
		if (equalsBrand(brand, brands[i]))
			return true;
	}

	return false;
}

static bool endsWithNoCase(const std::wstring &text, const wchar_t *suffix)
{
	size_t suffixLen = std::wcslen(suffix);
	if (text.size() < suffixLen)
		return false;

	size_t offset = text.size() - suffixLen;
	for (size_t i = 0; i < suffixLen; ++i) {
		if (std::towlower(text[offset + i]) != std::towlower(suffix[i]))
			return false;
	}

	return true;
}

bool isHeifBuffer(const std::vector<uchar> &data)
{
	if (data.size() < 12)
		return false;

	if (std::memcmp(&data[4], "ftyp", 4) != 0)
		return false;

	if (isHeifBrand(&data[8]))
		return true;

	for (size_t offset = 16; offset + 4 <= data.size(); offset += 4) {
		if (isHeifBrand(&data[offset]))
			return true;
	}

	return false;
}

bool isHeifFileW(const std::wstring &filename)
{
	return endsWithNoCase(filename, L".heic")
		|| endsWithNoCase(filename, L".heif")
		|| endsWithNoCase(filename, L".hif")
		|| endsWithNoCase(filename, L".avif");
}

cv::Mat imdecodeHeif(const std::vector<uchar> &data, int flags)
{
	if (!isHeifBuffer(data))
		return cv::Mat();

#ifndef Q1VIEW_ENABLE_LIBHEIF
	(void)flags;
	return cv::Mat();
#else
	heif_context *context = heif_context_alloc();
	if (context == nullptr)
		return cv::Mat();

	cv::Mat result;
	heif_image_handle *handle = nullptr;
	heif_image *image = nullptr;
	int stride = 0;
	const uint8_t *plane = nullptr;

	heif_error error = heif_context_read_from_memory_without_copy(
		context, data.data(), data.size(), nullptr);
	if (error.code != heif_error_Ok)
		goto cleanupContext;

	error = heif_context_get_primary_image_handle(context, &handle);
	if (error.code != heif_error_Ok || handle == nullptr)
		goto cleanupContext;

	error = heif_decode_image(handle, &image, heif_colorspace_RGB,
		heif_chroma_interleaved_RGBA, nullptr);
	if (error.code != heif_error_Ok || image == nullptr)
		goto cleanupHandle;

	plane = heif_image_get_plane_readonly(image, heif_channel_interleaved, &stride);
	if (plane == nullptr || stride <= 0)
		goto cleanupImage;

	{
		const int width = heif_image_handle_get_width(handle);
		const int height = heif_image_handle_get_height(handle);
		const cv::Mat rgba(height, width, CV_8UC4,
			const_cast<uint8_t *>(plane), static_cast<size_t>(stride));

		if (flags == cv::IMREAD_GRAYSCALE) {
			cv::cvtColor(rgba, result, cv::COLOR_RGBA2GRAY);
		} else if (flags == cv::IMREAD_UNCHANGED
			&& heif_image_handle_has_alpha_channel(handle)) {
			cv::cvtColor(rgba, result, cv::COLOR_RGBA2BGRA);
		} else {
			cv::cvtColor(rgba, result, cv::COLOR_RGBA2BGR);
		}
	}

cleanupImage:
	if (image != nullptr)
		heif_image_release(image);
cleanupHandle:
	if (handle != nullptr)
		heif_image_handle_release(handle);
cleanupContext:
	heif_context_free(context);
	return result;
#endif
}

} // namespace q1
