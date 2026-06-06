// Y4mReader.cpp : YUV4MPEG2 container parsing.

#include "Y4mReader.h"

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QList>

namespace q1y4m {

static const char kSignature[] = "YUV4MPEG2";

// Map a Y4M "C" color-space tag to a qcsc_info_table name plus the planar byte
// count of one frame. Only the formats this viewer can actually convert are
// returned; anything else yields an empty name so the caller can decline.
static QString mapColorSpace(const QByteArray &tag, int width, int height,
	int *frameDataSize)
{
	const int chromaW = (width + 1) / 2;
	const int chromaH = (height + 1) / 2;

	// Y4M defaults to 4:2:0 ("C420") when no C tag is present. The 8-bit 4:2:0
	// variants (420jpeg/420paldv/420mpeg2/420) differ only in chroma siting,
	// which the shared BT.601 converter treats identically.
	if (tag.isEmpty() || tag == "420" || tag == "420jpeg"
		|| tag == "420paldv" || tag == "420mpeg2") {
		*frameDataSize = width * height + 2 * chromaW * chromaH;
		return QStringLiteral("yuv420");
	}

	if (tag == "mono") {
		*frameDataSize = width * height;
		return QStringLiteral("grayscale");
	}

	// 4:2:2, 4:4:4, and the 10/16-bit variants have no converter here yet.
	*frameDataSize = 0;
	return QString();
}

bool isY4mFile(const QString &fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	const QByteArray head = file.read(static_cast<int>(sizeof(kSignature) - 1));
	return head == QByteArray(kSignature, sizeof(kSignature) - 1);
}

bool parseY4m(const QString &fileName, Y4mInfo *out)
{
	if (out == nullptr)
		return false;

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly))
		return false;

	const qint64 fileSize = file.size();

	// The header line plus the first FRAME marker are both small; this is more
	// than enough to cover them.
	const QByteArray chunk = file.read(1024);
	if (!chunk.startsWith(QByteArray(kSignature, sizeof(kSignature) - 1)))
		return false;

	const int headerEnd = chunk.indexOf('\n');
	if (headerEnd < 0)
		return false; // header line not terminated within the chunk

	Y4mInfo info;
	info.headerLength = headerEnd + 1;

	// Tokens are space-separated; the first ("YUV4MPEG2") is the signature.
	const QByteArray headerLine = chunk.left(headerEnd);
	const QList<QByteArray> tokens = headerLine.split(' ');
	QByteArray colorTag;
	for (const QByteArray &token : tokens) {
		if (token.size() < 2)
			continue;
		const char tag = token.at(0);
		const QByteArray value = token.mid(1);
		switch (tag) {
		case 'W':
			info.width = value.toInt();
			break;
		case 'H':
			info.height = value.toInt();
			break;
		case 'C':
			colorTag = value;
			break;
		case 'F': {
			const int colon = value.indexOf(':');
			if (colon > 0) {
				const double num = value.left(colon).toDouble();
				const double den = value.mid(colon + 1).toDouble();
				if (den > 0.0)
					info.fps = num / den;
			}
			break;
		}
		default:
			break; // I (interlace), A (aspect), X (comment), etc.
		}
	}

	if (info.width <= 0 || info.height <= 0)
		return false;

	info.colorSpaceName = mapColorSpace(colorTag, info.width, info.height,
		&info.frameDataSize);
	if (info.colorSpaceName.isEmpty() || info.frameDataSize <= 0)
		return false; // unsupported color space

	// The first frame marker begins right after the header line. Marker length
	// is constant across the file, so measure it once here.
	if (info.headerLength + 5 > chunk.size())
		return false;
	if (chunk.mid(static_cast<int>(info.headerLength), 5) != "FRAME")
		return false;
	const int markerEnd = chunk.indexOf('\n', static_cast<int>(info.headerLength));
	if (markerEnd < 0)
		return false;
	info.frameMarkerLength = markerEnd + 1 - static_cast<int>(info.headerLength);

	const qint64 perFrame = static_cast<qint64>(info.frameMarkerLength) + info.frameDataSize;
	info.frameCount = static_cast<int>((fileSize - info.headerLength) / perFrame);
	if (info.frameCount <= 0)
		return false;

	*out = info;
	return true;
}

} // namespace q1y4m
