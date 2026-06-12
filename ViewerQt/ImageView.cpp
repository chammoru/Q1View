#include "ImageView.h"

#include <QColor>
#include <QFont>
#include <QFontMetrics>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QString>

#include <algorithm>
#include <cmath>

namespace {

// Below this zoom the per-pixel labels are illegible, so only the image is drawn.
constexpr double kPixelOverlayMinScale = 16.0;

} // namespace

ImageView::ImageView(QWidget *parent)
	: QWidget(parent),
	  mScale(1.0),
	  mYOnly(false),
	  mHexMode(false),
	  mShowSourceValues(false),
	  mSelectionActive(false),
	  mInterpolate(false)
{
	setMouseTracking(true);
	setAttribute(Qt::WA_OpaquePaintEvent, true);
}

void ImageView::setImage(const QImage &shownImage, double scale)
{
	mImage = shownImage;
	mRgb = shownImage.isNull() ? QImage() : shownImage.convertToFormat(QImage::Format_RGB32);
	mScale = scale > 0.0 ? scale : 1.0;
	applyGeometry();
	update();
}

void ImageView::setYOnly(bool yOnly)
{
	mYOnly = yOnly;
}

void ImageView::setHexMode(bool hex)
{
	if (mHexMode == hex) {
		return;
	}
	mHexMode = hex;
	update();
}

void ImageView::setNativeSampler(NativeSampleFn sampler)
{
	mNativeSampler = std::move(sampler);
	update();
}

void ImageView::setShowSourceValues(bool on)
{
	if (mShowSourceValues == on) {
		return;
	}
	mShowSourceValues = on;
	update();
}

void ImageView::setInterpolate(bool on)
{
	if (mInterpolate == on) {
		return;
	}
	mInterpolate = on;
	update();
}

void ImageView::setSelection(const QRect &selectionInImage, bool active)
{
	if (mSelection == selectionInImage && mSelectionActive == active) {
		return;
	}
	mSelection = selectionInImage;
	mSelectionActive = active;
	update();
}

void ImageView::applyGeometry()
{
	if (mImage.isNull()) {
		resize(0, 0);
		return;
	}
	const int w = std::max(1, static_cast<int>(mImage.width() * mScale + 0.5));
	const int h = std::max(1, static_cast<int>(mImage.height() * mScale + 0.5));
	resize(w, h);
}

void ImageView::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	const QRect dirty = event->rect();
	painter.fillRect(dirty, palette().color(QPalette::Base));

	if (mImage.isNull()) {
		return;
	}

	// Source pixel range covering the exposed (dirty) widget rectangle.
	const int w = mImage.width();
	const int h = mImage.height();
	int sx0 = std::clamp(static_cast<int>(std::floor(dirty.left() / mScale)), 0, w);
	int sy0 = std::clamp(static_cast<int>(std::floor(dirty.top() / mScale)), 0, h);
	int sx1 = std::clamp(static_cast<int>(std::ceil((dirty.right() + 1) / mScale)), 0, w);
	int sy1 = std::clamp(static_cast<int>(std::ceil((dirty.bottom() + 1) / mScale)), 0, h);
	if (sx1 <= sx0 || sy1 <= sy0) {
		return;
	}

	const QRect srcRect(sx0, sy0, sx1 - sx0, sy1 - sy0);
	const QRectF dstRect(sx0 * mScale, sy0 * mScale,
		srcRect.width() * mScale, srcRect.height() * mScale);

	// Smooth when shrinking, and when the user has turned on interpolation; the
	// default magnified look is nearest-neighbour (a crisp pixel grid).
	painter.setRenderHint(QPainter::SmoothPixmapTransform, mInterpolate || mScale < 1.0);
	painter.drawImage(dstRect, mImage, QRectF(srcRect));

	// The per-pixel value grid is a nearest-neighbour aid; hide it while smoothly
	// interpolating so the magnified image stays clean.
	if (!mInterpolate && mScale >= kPixelOverlayMinScale) {
		paintPixelValues(painter, srcRect);
	}

	if (mSelection.isValid() && !mSelection.isEmpty()) {
		const QRectF sel(mSelection.x() * mScale, mSelection.y() * mScale,
			mSelection.width() * mScale, mSelection.height() * mScale);
		// Solid 1px outline matching the MFC viewer: Q1UI_COLOR_WARNING (amber)
		// while dragging out a new selection, Q1UI_COLOR_SUCCESS (green) once set.
		const QColor color = mSelectionActive ? QColor(0xf5, 0x9e, 0x0b)
			: QColor(0x16, 0x9b, 0x62);
		QPen pen(color);
		pen.setStyle(Qt::SolidLine);
		pen.setCosmetic(true);
		painter.setRenderHint(QPainter::Antialiasing, false);
		painter.setPen(pen);
		painter.setBrush(Qt::NoBrush);
		painter.drawRect(sel);
	}
}

void ImageView::paintPixelValues(QPainter &painter, const QRect &srcRect) const
{
	if (mRgb.isNull()) {
		return;
	}

	const double cell = mScale;

	// Faint grid around each visible pixel cell.
	painter.setRenderHint(QPainter::Antialiasing, false);
	painter.setPen(QColor(0, 0, 0, 64));
	const int gridTop = static_cast<int>(std::lround(srcRect.top() * cell));
	const int gridBottom = static_cast<int>(std::lround((srcRect.bottom() + 1) * cell));
	const int gridLeft = static_cast<int>(std::lround(srcRect.left() * cell));
	const int gridRight = static_cast<int>(std::lround((srcRect.right() + 1) * cell));
	for (int x = srcRect.left(); x <= srcRect.right() + 1; ++x) {
		const int px = static_cast<int>(std::lround(x * cell));
		painter.drawLine(px, gridTop, px, gridBottom);
	}
	for (int y = srcRect.top(); y <= srcRect.bottom() + 1; ++y) {
		const int py = static_cast<int>(std::lround(y * cell));
		painter.drawLine(gridLeft, py, gridRight, py);
	}

	// Decide the overlay mode once: the image has a single source format, so probe
	// the first visible pixel. With "source values" on and a raw YUV source, show
	// the native Y/U/V components (matching the MFC viewer's 'V'); otherwise show
	// the converted RGB. Digit count follows the sample bit depth, like the MFC
	// overlay (8-bit -> 2 hex / 3 dec, 10-bit -> 3 hex / 4 dec).
	bool sourceMode = false;
	int digits = mHexMode ? 2 : 3;
	if (mShowSourceValues && mNativeSampler) {
		QIMAGE_NATIVE_PIXEL_SAMPLE probe = {};
		if (mNativeSampler(srcRect.left(), srcRect.top(), &probe)
			&& probe.model == QIMAGE_PIXEL_MODEL_YUV) {
			sourceMode = true;
			digits = mHexMode ? (probe.bit_depth + 3) / 4 : (probe.bit_depth > 8 ? 4 : 3);
		}
	}

	// Value text, sized to fit the configured number of lines inside a cell.
	const int lineCount = (sourceMode || !mYOnly) ? 3 : 1;
	QFont font = painter.font();
	// Start from a height-based estimate, then shrink until the value text fits
	// the cell. Font height/width ratios vary by platform, so measure to be sure
	// rather than trusting the estimate (otherwise the text can vanish entirely).
	int pixelSize = static_cast<int>(std::min(cell / (lineCount * 1.35), cell / 2.2));
	// Size the font against the widest label the current mode/depth will draw.
	const QString widthRef(digits, mHexMode ? QLatin1Char('F') : QLatin1Char('0'));
	QFontMetrics metrics(font);
	while (pixelSize >= 6) {
		font.setPixelSize(pixelSize);
		metrics = QFontMetrics(font);
		if (metrics.height() * lineCount <= cell
			&& metrics.horizontalAdvance(widthRef) <= cell) {
			break;
		}
		--pixelSize;
	}
	if (pixelSize < 6) {
		return; // cell too small for legible text; grid only
	}
	painter.setFont(font);

	const char *fmt1 = mHexMode ? "%0*X" : "%0*d";
	for (int y = srcRect.top(); y <= srcRect.bottom(); ++y) {
		const QRgb *line = reinterpret_cast<const QRgb *>(mRgb.constScanLine(y));
		const int top = static_cast<int>(std::lround(y * cell));
		const int bottom = static_cast<int>(std::lround((y + 1) * cell));
		for (int x = srcRect.left(); x <= srcRect.right(); ++x) {
			const QRgb value = line[x];
			const int left = static_cast<int>(std::lround(x * cell));
			const int right = static_cast<int>(std::lround((x + 1) * cell));
			const QRect cellRect(left, top, right - left, bottom - top);

			QString text;
			QIMAGE_NATIVE_PIXEL_SAMPLE s = {};
			if (sourceMode && mNativeSampler(x, y, &s)
				&& s.model == QIMAGE_PIXEL_MODEL_YUV) {
				text = QString::asprintf(fmt1, digits, s.component[0])
					+ QLatin1Char('\n') + QString::asprintf(fmt1, digits, s.component[1])
					+ QLatin1Char('\n') + QString::asprintf(fmt1, digits, s.component[2]);
			} else if (mYOnly) {
				text = QString::asprintf(fmt1, digits, qRed(value));
			} else {
				text = QString::asprintf(fmt1, digits, qRed(value))
					+ QLatin1Char('\n') + QString::asprintf(fmt1, digits, qGreen(value))
					+ QLatin1Char('\n') + QString::asprintf(fmt1, digits, qBlue(value));
			}

			// Contrast against the actual displayed pixel colour.
			painter.setPen(qGray(value) < 128 ? Qt::white : Qt::black);
			painter.drawText(cellRect, Qt::AlignCenter, text);
		}
	}
}
