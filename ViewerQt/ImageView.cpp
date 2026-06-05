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
	  mYOnly(false)
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

void ImageView::setSelection(const QRect &selectionInImage)
{
	if (mSelection == selectionInImage) {
		return;
	}
	mSelection = selectionInImage;
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

	painter.setRenderHint(QPainter::SmoothPixmapTransform, mScale < 1.0);
	painter.drawImage(dstRect, mImage, QRectF(srcRect));

	if (mScale >= kPixelOverlayMinScale) {
		paintPixelValues(painter, srcRect);
	}

	if (mSelection.isValid() && !mSelection.isEmpty()) {
		const QRectF sel(mSelection.x() * mScale, mSelection.y() * mScale,
			mSelection.width() * mScale, mSelection.height() * mScale);
		QPen pen(Qt::red);
		pen.setStyle(Qt::DashLine);
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

	// Value text, sized to fit the configured number of lines inside a cell.
	const int lineCount = mYOnly ? 1 : 3;
	QFont font = painter.font();
	// Start from a height-based estimate, then shrink until the value text fits
	// the cell. Font height/width ratios vary by platform, so measure to be sure
	// rather than trusting the estimate (otherwise the text can vanish entirely).
	int pixelSize = static_cast<int>(std::min(cell / (lineCount * 1.35), cell / 2.2));
	QFontMetrics metrics(font);
	while (pixelSize >= 6) {
		font.setPixelSize(pixelSize);
		metrics = QFontMetrics(font);
		if (metrics.height() * lineCount <= cell
			&& metrics.horizontalAdvance(QStringLiteral("FF")) <= cell) {
			break;
		}
		--pixelSize;
	}
	if (pixelSize < 6) {
		return; // cell too small for legible text; grid only
	}
	painter.setFont(font);

	for (int y = srcRect.top(); y <= srcRect.bottom(); ++y) {
		const QRgb *line = reinterpret_cast<const QRgb *>(mRgb.constScanLine(y));
		const int top = static_cast<int>(std::lround(y * cell));
		const int bottom = static_cast<int>(std::lround((y + 1) * cell));
		for (int x = srcRect.left(); x <= srcRect.right(); ++x) {
			const QRgb value = line[x];
			const int left = static_cast<int>(std::lround(x * cell));
			const int right = static_cast<int>(std::lround((x + 1) * cell));
			const QRect cellRect(left, top, right - left, bottom - top);

			const QString text = mYOnly
				? QString::asprintf("%02X", qRed(value))
				: QString::asprintf("%02X\n%02X\n%02X", qRed(value), qGreen(value), qBlue(value));

			painter.setPen(qGray(value) < 128 ? Qt::white : Qt::black);
			painter.drawText(cellRect, Qt::AlignCenter, text);
		}
	}
}
