#include "ThumbnailPane.h"

#include "HeifReader.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>
#include <QStyle>
#include <QTimer>

namespace {
constexpr int kThumbSize = 96;
constexpr int kKindRole = Qt::UserRole;       // EntryKind
constexpr int kPathRole = Qt::UserRole + 1;   // QString target/file path
} // namespace

ThumbnailPane::ThumbnailPane(QWidget *parent)
	: QListWidget(parent),
	  mThumb(kThumbSize),
	  mGeneration(0),
	  mPendingPos(0),
	  mDecodeTimer(new QTimer(this))
{
	setViewMode(QListView::IconMode);
	setIconSize(QSize(mThumb, mThumb));
	setGridSize(QSize(mThumb + 24, mThumb + 34));
	setResizeMode(QListView::Adjust);
	setMovement(QListView::Static);
	setUniformItemSizes(true);
	setWordWrap(true);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

	// Decode thumbnails one at a time off the event loop, so opening a folder of
	// large images fills in progressively instead of blocking the UI.
	mDecodeTimer->setSingleShot(true);
	mDecodeTimer->setInterval(0);
	connect(mDecodeTimer, &QTimer::timeout, this, &ThumbnailPane::decodeNextThumb);
	connect(this, &QListWidget::itemActivated, this, &ThumbnailPane::onItemActivated);
	connect(this, &QListWidget::itemDoubleClicked, this, &ThumbnailPane::onItemActivated);
}

void ThumbnailPane::setCurrentFile(const QString &path)
{
	if (path.isEmpty()) {
		mFolder.clear();
		mGeneration++;
		mDecodeTimer->stop();
		clear();
		return;
	}

	const QFileInfo info(path);
	const QString folder = info.absolutePath();
	if (folder != mFolder) {
		populate(folder, info.absoluteFilePath());
	} else {
		selectPath(info.absoluteFilePath());
	}
}

void ThumbnailPane::populate(const QString &folder, const QString &currentPath)
{
	mDecodeTimer->stop();
	clear();
	mPendingRows.clear();
	mPendingPos = 0;
	mFolder = folder;
	const unsigned generation = ++mGeneration;
	Q_UNUSED(generation);

	QDir dir(folder);
	const QIcon dirIcon = style()->standardIcon(QStyle::SP_DirIcon);

	// ".." parent entry, unless this is a filesystem root.
	if (!QDir(folder).isRoot()) {
		QDir up(folder);
		if (up.cdUp()) {
			QListWidgetItem *item = new QListWidgetItem(dirIcon, QStringLiteral(".."), this);
			item->setData(kKindRole, ParentDir);
			item->setData(kPathRole, up.absolutePath());
		}
	}

	// Sub-folders, then files, each sorted by name (case-insensitive).
	const QFileInfoList dirs = dir.entryInfoList(
		QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
	for (const QFileInfo &sub : dirs) {
		QListWidgetItem *item = new QListWidgetItem(dirIcon, sub.fileName(), this);
		item->setData(kKindRole, SubDir);
		item->setData(kPathRole, sub.absoluteFilePath());
	}

	const QFileInfoList files = dir.entryInfoList(
		mNameFilters, QDir::Files, QDir::Name | QDir::IgnoreCase);
	for (const QFileInfo &file : files) {
		QListWidgetItem *item = new QListWidgetItem(file.fileName(), this);
		item->setData(kKindRole, FileEntry);
		item->setData(kPathRole, file.absoluteFilePath());
		item->setToolTip(file.fileName());
		if (isThumbnailable(file.absoluteFilePath())) {
			// Placeholder until the real thumbnail is decoded below.
			item->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
			mPendingRows.append(row(item));
		} else {
			// Raw formats: a labeled badge, like the MFC pane's placeholder.
			item->setIcon(placeholderIcon(file.suffix().toUpper()));
		}
	}

	selectPath(currentPath);

	if (!mPendingRows.isEmpty()) {
		mDecodeTimer->start();
	}
}

void ThumbnailPane::selectPath(const QString &path)
{
	const QString target = QFileInfo(path).absoluteFilePath();
	for (int i = 0; i < count(); ++i) {
		QListWidgetItem *it = item(i);
		if (it->data(kKindRole).toInt() == FileEntry
			&& it->data(kPathRole).toString() == target) {
			setCurrentItem(it);
			scrollToItem(it);
			return;
		}
	}
	setCurrentItem(nullptr);
}

void ThumbnailPane::onItemActivated(QListWidgetItem *item)
{
	if (!item) {
		return;
	}
	const int kind = item->data(kKindRole).toInt();
	const QString path = item->data(kPathRole).toString();
	if (kind == ParentDir || kind == SubDir) {
		populate(path, QString());
	} else {
		emit fileActivated(path);
	}
}

void ThumbnailPane::decodeNextThumb()
{
	if (mPendingPos >= mPendingRows.size()) {
		return;
	}
	const unsigned generation = static_cast<unsigned>(mGeneration);
	const int r = mPendingRows[mPendingPos++];
	QListWidgetItem *it = item(r);
	if (it) {
		const QString path = it->data(kPathRole).toString();
		const QSize bound(mThumb, mThumb);
		QImage img;
		QImageReader reader(path);
		reader.setAutoTransform(true);
		const QSize full = reader.size();
		if (full.isValid()) {
			QSize scaled = full.scaled(bound, Qt::KeepAspectRatio);
			reader.setScaledSize(scaled.isEmpty() ? bound : scaled);
		}
		img = reader.read();
		if (img.isNull() && q1qt::isHeifFile(path)) {
			const QImage heif = q1qt::readHeif(path);
			if (!heif.isNull()) {
				img = heif.scaled(bound, Qt::KeepAspectRatio, Qt::SmoothTransformation);
			}
		}
		// A folder change between scheduling and now bumps the generation; drop
		// the stale result rather than painting it onto a recycled row.
		if (!img.isNull() && generation == static_cast<unsigned>(mGeneration)) {
			it->setIcon(QPixmap::fromImage(img));
		}
	}
	if (mPendingPos < mPendingRows.size()) {
		mDecodeTimer->start();
	}
}

QIcon ThumbnailPane::placeholderIcon(const QString &label) const
{
	QPixmap pm(mThumb, mThumb);
	pm.fill(QColor(0x3a, 0x3f, 0x4b));
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing, true);
	p.setPen(QColor(0x5a, 0x60, 0x6e));
	p.drawRect(0, 0, mThumb - 1, mThumb - 1);
	QFont f = p.font();
	f.setBold(true);
	f.setPixelSize(label.size() > 5 ? 14 : 18);
	p.setFont(f);
	p.setPen(QColor(0xe5, 0xe7, 0xeb));
	p.drawText(pm.rect(), Qt::AlignCenter, label);
	p.end();
	return QIcon(pm);
}

bool ThumbnailPane::isThumbnailable(const QString &path) const
{
	if (QImageReader(path).canRead()) {
		return true;
	}
	return q1qt::isHeifFile(path);
}
