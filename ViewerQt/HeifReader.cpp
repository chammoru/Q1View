// HeifReader.cpp : libheif-backed HEIF/HEIC/AVIF decode for the Qt viewer.

#include "HeifReader.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QLatin1String>

#ifdef Q1VIEW_ENABLE_LIBHEIF
#include <libheif/heif.h>
#include <cstddef>
#include <cstdint>
#include <cstring>
#endif

namespace q1qt {

bool heifSupported()
{
#ifdef Q1VIEW_ENABLE_LIBHEIF
	return true;
#else
	return false;
#endif
}

bool isHeifFile(const QString &fileName)
{
	const QString suffix = QFileInfo(fileName).suffix().toLower();
	return suffix == QLatin1String("heic")
		|| suffix == QLatin1String("heif")
		|| suffix == QLatin1String("hif")
		|| suffix == QLatin1String("avif");
}

#ifndef Q1VIEW_ENABLE_LIBHEIF

QImage readHeif(const QString & /*fileName*/)
{
	return QImage();
}

#else

// The compatible-brand list as the Windows QHeifUtil helper: covers HEIC's HEVC
// brands plus the ISO base ("mif1"/"msf1") and AVIF brands.
static bool isHeifBrand(const char *brand)
{
	static const char *const brands[] = {
		"heic", "heix", "hevc", "hevx",
		"heim", "heis", "hevm", "hevs",
		"mif1", "msf1", "avif", "avis",
	};

	for (size_t i = 0; i < sizeof(brands) / sizeof(brands[0]); ++i) {
		if (std::memcmp(brand, brands[i], 4) == 0)
			return true;
	}

	return false;
}

// Sniff the ISO-BMFF "ftyp" box for a HEIF-family brand. Guards libheif against
// being handed an unrelated file that merely carries a HEIF extension.
static bool isHeifBuffer(const QByteArray &data)
{
	if (data.size() < 12)
		return false;

	const char *bytes = data.constData();
	if (std::memcmp(bytes + 4, "ftyp", 4) != 0)
		return false;

	if (isHeifBrand(bytes + 8))
		return true;

	for (int offset = 16; offset + 4 <= data.size(); offset += 4) {
		if (isHeifBrand(bytes + offset))
			return true;
	}

	return false;
}

QImage readHeif(const QString &fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return QImage();

	const QByteArray bytes = file.readAll();
	if (!isHeifBuffer(bytes))
		return QImage();

	heif_context *context = heif_context_alloc();
	if (context == nullptr)
		return QImage();

	QImage result;
	heif_image_handle *handle = nullptr;
	heif_image *image = nullptr;
	int stride = 0;
	const uint8_t *plane = nullptr;

	heif_error error = heif_context_read_from_memory_without_copy(
		context, bytes.constData(), static_cast<size_t>(bytes.size()), nullptr);
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
		if (width > 0 && height > 0) {
			// libheif RGBA byte order matches QImage::Format_RGBA8888 (R,G,B,A).
			// Copy row by row so the QImage owns its pixels and stays valid after
			// the libheif image is released, and so a padded stride is handled.
			QImage rgba(width, height, QImage::Format_RGBA8888);
			const size_t rowBytes = static_cast<size_t>(width) * 4;
			for (int y = 0; y < height; ++y) {
				std::memcpy(rgba.scanLine(y),
					plane + static_cast<size_t>(y) * static_cast<size_t>(stride),
					rowBytes);
			}
			result = rgba;
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
}

#endif // Q1VIEW_ENABLE_LIBHEIF

} // namespace q1qt
