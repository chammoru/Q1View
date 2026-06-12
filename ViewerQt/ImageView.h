#ifndef Q1VIEW_VIEWERQT_IMAGEVIEW_H
#define Q1VIEW_VIEWERQT_IMAGEVIEW_H

#include <QImage>
#include <QRect>
#include <QSize>
#include <QWidget>

#include <functional>

#include "qimage_cs.h"

class QPaintEvent;

// Image surface hosted inside the scroll area. Instead of building one full
// scaled pixmap (which becomes huge at high zoom), it repaints only the region
// exposed by each paint event, scaling straight from the source image. Painting
// cost is therefore bounded by the viewport, so arbitrarily large images can be
// zoomed without allocating a giant buffer. At high zoom it overlays each
// pixel's value, drawn only for the cells currently visible.
class ImageView : public QWidget
{
public:
	explicit ImageView(QWidget *parent = nullptr);

	// shownImage is already in display orientation (rotation / Y-only applied).
	void setImage(const QImage &shownImage, double scale);
	void setYOnly(bool yOnly);
	// Per-pixel value overlay format: hexadecimal when true, zero-padded decimal
	// otherwise. Matches the MFC viewer's 'H' toggle (which defaults to decimal).
	void setHexMode(bool hex);
	// Optional native (source) pixel sampler. Given display-space pixel coords it
	// fills the original component sample (e.g. source Y/U/V for raw YUV); returns
	// false to fall back to the converted RGB. With "source values" on, the overlay
	// shows the native Y/U/V instead of RGB, matching the MFC viewer's 'V' toggle.
	using NativeSampleFn = std::function<bool(int, int, QIMAGE_NATIVE_PIXEL_SAMPLE *)>;
	void setNativeSampler(NativeSampleFn sampler);
	void setShowSourceValues(bool on);
	// When true, the image is drawn with smooth (bilinear) interpolation even when
	// magnified, instead of the default nearest-neighbour "pixel grid" look.
	void setInterpolate(bool on);
	bool interpolate() const { return mInterpolate; }
	// image coords; empty = none. active == true while the user is dragging out a
	// fresh selection (drawn in the "warning" colour, like the MFC viewer); once
	// committed it is drawn in the "success" colour.
	void setSelection(const QRect &selectionInImage, bool active = false);

	QSize imageSize() const { return mImage.size(); }
	double scale() const { return mScale; }
	bool hasImage() const { return !mImage.isNull(); }

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	void applyGeometry();
	void paintPixelValues(QPainter &painter, const QRect &srcRect) const;

	QImage mImage;   // display image
	QImage mRgb;     // RGB32 copy for fast per-pixel overlay access
	double mScale;
	bool mYOnly;
	bool mHexMode;
	bool mShowSourceValues;
	NativeSampleFn mNativeSampler;
	QRect mSelection;
	bool mSelectionActive;
	bool mInterpolate;
};

#endif
