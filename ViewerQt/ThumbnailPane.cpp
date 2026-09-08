#include "ThumbnailPane.h"

#include "HeifReader.h"
#include "Q1Theme.h"

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
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMessageBox>
#include <QUrl>
#include <functional>
#ifdef Q_OS_WIN
#include "../QCommon/inc/QFileActionsWin.h"
#endif

namespace {
constexpr int kThumbSize = 96;   // decode size (scaled down for display)
constexpr int kIconDisplay = 48; // on-screen icon edge, matching the MFC pane
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
	// Compact rows (small thumbnail at left, name at right), matching the MFC
	// viewer's report-style drawer rather than a big icon grid.
	setViewMode(QListView::ListMode);
	setIconSize(QSize(kIconDisplay, kIconDisplay));
	setMovement(QListView::Static);
	setResizeMode(QListView::Adjust);
	setWrapping(false);
	setUniformItemSizes(true);
	setWordWrap(false);
	setTextElideMode(Qt::ElideRight);
	setSpacing(1);
	setSelectionMode(QAbstractItemView::SingleSelection);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	// Drawer colours mirror the MFC pane: a light surface, the accent-soft
	// selection highlight, and the shared text colour (from q1theme).
	setStyleSheet(QStringLiteral(
		"QListWidget { background:%1; border:none; outline:none; }"
		"QListWidget::item { color:%2; padding:2px 4px; }"
		"QListWidget::item:selected { background:%3; color:%2; }")
		.arg(q1theme::hex(q1theme::surfaceAlt()),
			q1theme::hex(q1theme::text()),
			q1theme::hex(q1theme::accentSoft())));

	// Decode thumbnails one at a time off the event loop, so opening a folder of
	// large images fills in progressively instead of blocking the UI.
	mDecodeTimer->setSingleShot(true);
	mDecodeTimer->setInterval(0);
	connect(mDecodeTimer, &QTimer::timeout, this, &ThumbnailPane::decodeNextThumb);
	connect(this, &QListWidget::itemActivated, this, &ThumbnailPane::onItemActivated);
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
	if (canGoToParent()) {
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
		if (it->data(kKindRole).toInt() != ParentDir
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
	if (kind == ParentDir) {
		goToParent();
	} else if (kind == SubDir) {
		navigateTo(path);
	} else {
		if (QFileInfo::exists(path)) emit fileActivated(path);
	}
}

bool ThumbnailPane::canGoToParent() const
{
#ifdef Q_OS_WIN
	return !q1view::ParentDirectory(mFolder.toStdWString()).empty();
#else
	return !mFolder.isEmpty() && !QDir(mFolder).isRoot();
#endif
}

bool ThumbnailPane::navigateTo(const QString &folder, const QString &select)
{
	if (!QFileInfo(folder).isDir() || !QDir(folder).isReadable()) return false;
	populate(QDir(folder).absolutePath(), select);
	return true;
}

bool ThumbnailPane::goToParent()
{
	if (!canGoToParent()) return false;
	QDir parent(mFolder);
	const QString child = mFolder;
	return parent.cdUp() && navigateTo(parent.absolutePath(), child);
}

void ThumbnailPane::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Backspace ||
		(event->key() == Qt::Key_Up && event->modifiers() == Qt::AltModifier)) {
		goToParent(); event->accept(); return;
	}
	QListWidget::keyPressEvent(event);
}

QMenu* ThumbnailPane::createContextMenu(QListWidgetItem *item)
{
	auto* menu = new QMenu(this);
	const int generation = mGeneration;
	const int kind = item ? item->data(kKindRole).toInt() : ParentDir;
	const QString path = item ? item->data(kPathRole).toString() : QString();
	// Capture values: nested menu events may repopulate the drawer.
	auto add = [this, menu, generation](const QString &label, bool enabled, std::function<bool()> operation) {
		QAction* action = menu->addAction(label);
		action->setEnabled(enabled);
		connect(action, &QAction::triggered, this, [this, generation, operation] {
			if (generation != mGeneration) return;
			if (!operation()) QMessageBox::information(this, tr("Thumbnail browser"),
				tr("The operation could not be completed. The item may be unavailable or the clipboard may be in use."));
		});
		return action;
	};
	const bool exists = QFileInfo::exists(path);
	if (!item || kind == ParentDir) return menu;
	add(kind == SubDir ? tr("Open folder") : tr("Open"), exists, [this, kind, path] {
		if (kind == SubDir) return navigateTo(path);
		if (!QFileInfo::exists(path)) return false;
		emit fileActivated(path); return true;
	});
#ifdef Q_OS_WIN
	add(tr("Show in File Explorer"), exists, [path] { return q1view::ShowInExplorer(path.toStdWString()); });
#else
	add(tr("Open containing folder"), exists, [path] {
		return QFileInfo::exists(path) && QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
	});
#endif
	menu->addSeparator();
	add(tr("Copy"), exists, [this, path] {
#ifdef Q_OS_WIN
		return q1view::ClipboardFile(reinterpret_cast<HWND>(winId()), path.toStdWString());
#else
		if (!QFileInfo::exists(path)) return false;
		auto* mime = new QMimeData;
		mime->setUrls({QUrl::fromLocalFile(path)});
		mime->setData("x-special/gnome-copied-files", "copy\n" + QUrl::fromLocalFile(path).toEncoded());
		QApplication::clipboard()->setMimeData(mime); return true;
#endif
	});
	auto* names = menu->addMenu(tr("Copy path/name"));
	auto copyText = [this, generation](const QString &text) {
		if (generation != mGeneration) return;
		QApplication::clipboard()->setText(text);
	};
	connect(names->addAction(tr("Copy full path")), &QAction::triggered, this, [copyText, path] { copyText(path); });
	if (kind == FileEntry)
		connect(names->addAction(tr("Copy file name")), &QAction::triggered, this, [copyText, path] { copyText(QFileInfo(path).fileName()); });
#ifdef Q_OS_WIN
	menu->addSeparator();
	add(tr("Properties"), exists, [this, path] {
		return q1view::ShowFileProperties(reinterpret_cast<HWND>(winId()), path.toStdWString());
	});
#endif
	return menu;
}

void ThumbnailPane::contextMenuEvent(QContextMenuEvent *event)
{
	QListWidgetItem* target = event->reason() == QContextMenuEvent::Keyboard ?
		currentItem() : itemAt(viewport()->mapFromGlobal(event->globalPos()));
	if (target) setCurrentItem(target);
	QMenu* menu = createContextMenu(target);
	if (!menu->isEmpty()) menu->exec(event->globalPos());
	delete menu;
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
