#ifndef Q1VIEW_QCOMMON_QIMAGESCALING_H
#define Q1VIEW_QCOMMON_QIMAGESCALING_H

namespace q1 {

enum class ImageScalingMode
{
	Auto = 0,
	Smooth = 1,
	PixelExact = 2,
};

enum class ImageScalingFilter
{
	Nearest,
	Bilinear,
	Area,
};

// Auto keeps inspection and timed-source performance predictable: single still
// images are smoothed while shrinking, using area sampling only for reductions
// of 2x or more. Moderate reductions use bilinear sampling so thin document
// strokes do not lose contrast. Multi-frame/video sources retain nearest
// sampling unless Smooth is selected explicitly.
inline ImageScalingFilter ResolveImageScalingFilter(ImageScalingMode mode,
	double scale, bool singleStillImage, bool areaSamplingAvailable)
{
	if (mode == ImageScalingMode::Smooth)
		return ImageScalingFilter::Bilinear;
	if (mode == ImageScalingMode::PixelExact)
		return ImageScalingFilter::Nearest;
	if (!singleStillImage || scale >= 1.0)
		return ImageScalingFilter::Nearest;
	if (areaSamplingAvailable && scale <= 0.5)
		return ImageScalingFilter::Area;
	return ImageScalingFilter::Bilinear;
}

inline ImageScalingMode NextImageScalingMode(ImageScalingMode mode)
{
	switch (mode) {
	case ImageScalingMode::Auto:
		return ImageScalingMode::Smooth;
	case ImageScalingMode::Smooth:
		return ImageScalingMode::PixelExact;
	default:
		return ImageScalingMode::Auto;
	}
}

} // namespace q1

#endif
