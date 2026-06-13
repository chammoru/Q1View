#ifndef Q1VIEW_VIEWERQT_THUMBNAILPANE_H
#define Q1VIEW_VIEWERQT_THUMBNAILPANE_H

#include <QIcon>
#include <QListWidget>
#include <QString>
#include <QStringList>
#include <QVector>

class QTimer;

// Side "drawer" folder explorer, the cross-platform analogue of the MFC viewer's
// CThumbnailPane. It lists the supported image/raw files in the current
// document's folder (plus a ".." entry and sub-folders). Encoded images get an
// incrementally decoded thumbnail; raw formats, whose pixel layout can't be
// guessed, get a labeled placeholder. Double-clicking / Entering a folder
// navigates into it; doing so on a file emits fileActivated() to open it.
class ThumbnailPane : public QListWidget
{
	Q_OBJECT

public:
	explicit ThumbnailPane(QWidget *parent = nullptr);

	// Name filters (e.g. "*.png") of the files to list; supplied by the owner so
	// the HEIF/video sets stay defined in one place (MainWindow::imageNameFilters).
	void setNameFilters(const QStringList &filters) { mNameFilters = filters; }

	// Show the folder containing `path`, repopulating only when the folder
	// changes, and select the matching item. An empty path clears the list.
	void setCurrentFile(const QString &path);

signals:
	// A file row was activated (double-click / Enter); the owner should open it.
	void fileActivated(const QString &path);

private slots:
	void onItemActivated(QListWidgetItem *item);
	void decodeNextThumb();

private:
	enum EntryKind { ParentDir, SubDir, FileEntry };

	void populate(const QString &folder, const QString &currentPath);
	void selectPath(const QString &path);
	QIcon placeholderIcon(const QString &label) const;
	bool isThumbnailable(const QString &path) const;

	QStringList mNameFilters;
	QString mFolder;          // folder currently listed (absolute, no trailing /)
	int mThumb;               // icon edge size in px
	int mGeneration;          // bumped on each populate to drop stale decode work
	QVector<int> mPendingRows; // file rows still awaiting a decoded thumbnail
	int mPendingPos;
	QTimer *mDecodeTimer;
};

#endif // Q1VIEW_VIEWERQT_THUMBNAILPANE_H
