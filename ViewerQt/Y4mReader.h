// Y4mReader.h : YUV4MPEG2 (.y4m) container parsing for the Qt viewer.
//
// A .y4m / Y4M file is *not* raw planar YUV: it begins with a text header line
//
//     YUV4MPEG2 W<w> H<h> F<num>:<den> I<i> A<n>:<d> C<colorspace> ...\n
//
// and every frame is preceded by a "FRAME[ params]\n" marker. Decoding the
// pixels as if the file were headerless raw YUV folds those bytes into the
// image and shifts every frame, so the container has to be parsed first.
//
// This reader only extracts the layout (dimensions, color space, where the
// frames live). Pixel conversion still goes through the shared core's
// qcsc_info_table, exactly like the raw path.

#pragma once

#include <QString>
#include <QtGlobal>

namespace q1y4m {

struct Y4mInfo {
	int width = 0;
	int height = 0;
	// A qcsc_info_table color-space name (e.g. "yuv420"), empty if the Y4M
	// color space is one this viewer cannot convert.
	QString colorSpaceName;
	double fps = 0.0;            // 0 when unspecified
	qint64 headerLength = 0;     // bytes before the first FRAME marker
	int frameMarkerLength = 0;   // length of each "FRAME...\n" marker
	int frameDataSize = 0;       // pixel bytes per frame
	int frameCount = 0;
};

// True when the file starts with the "YUV4MPEG2" signature.
bool isY4mFile(const QString &fileName);

// Parse the header and first frame marker of a Y4M file. Returns false if the
// file is not Y4M, is malformed, or uses a color space without a converter.
bool parseY4m(const QString &fileName, Y4mInfo *out);

} // namespace q1y4m
