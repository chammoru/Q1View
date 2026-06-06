// HeifReader.h : HEIF/HEIC/AVIF decode for the Qt viewer.
//
// QImageReader only covers the formats Qt was built with (PNG/JPG/BMP/...), so
// HEIF-family images are decoded here directly through libheif into a QImage.
// This mirrors the Windows product's QHeifUtil (which targets cv::Mat), but
// stays OpenCV-free so the Qt viewer keeps its lean dependency set.
//
// When the build is configured without libheif, readHeif() is a no-op that
// returns a null QImage and heifSupported() reports false, so callers never
// have to special-case the unconfigured build.

#pragma once

#include <QImage>
#include <QString>

namespace q1qt {

// True when this build was compiled with libheif (Q1VIEW_ENABLE_LIBHEIF).
bool heifSupported();

// True when the file name's extension is a known HEIF-family container. This is
// a cheap name check only; it does not open or sniff the file.
bool isHeifFile(const QString &fileName);

// Decode the primary image of a HEIF/HEIC/AVIF file into a QImage. Returns a
// null QImage on any failure, on a non-HEIF buffer, or when built without
// libheif.
QImage readHeif(const QString &fileName);

} // namespace q1qt
