#ifndef Q1VIEW_VIEWERQT_IMAGEVIEW_H
#define Q1VIEW_VIEWERQT_IMAGEVIEW_H

#include <QImage>
#include <QRect>
#include <QSize>
#include <QWidget>

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
	QRect mSelection;
	bool mSelectionActive;
	bool mInterpolate;
};

#endif
