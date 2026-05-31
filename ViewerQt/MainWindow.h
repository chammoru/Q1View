#ifndef Q1VIEW_VIEWERQT_MAINWINDOW_H
#define Q1VIEW_VIEWERQT_MAINWINDOW_H

#include "RawOpenDialog.h"

#include <QImage>
#include <QMainWindow>
#include <QPoint>
#include <QStringList>

class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QAction;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QResizeEvent;
class QScrollBar;
class QScrollArea;
class QTimer;
class QWheelEvent;

class MainWindow : public QMainWindow
{
public:
	explicit MainWindow(QWidget *parent = nullptr);

	bool openFile(const QString &fileName);
	bool openRawFile(const QString &fileName, int width, int height, const QString &colorSpaceName);

private:
	void createActions();
	void adjustScrollBar(QScrollBar *scrollBar, double factor);
	void applyZoom(double factor, const QPoint *anchor = nullptr);
	void closeCurrentFile();
	QImage displayImage() const;
	QStringList imageNameFilters() const;
	bool loadRawFrame(int frameIndex);
	void openAdjacentFile(int direction, bool boundaryOnly = false);
	void openClipboardImage();
	void fitImageToWindow();
	void setActualSize();
	void loadSettings();
	void resetPlayback();
	void saveRawSettings() const;
	void saveImageAs();
	void showHelp();
	QPoint sourcePointFromDisplayPoint(const QPoint &point) const;
	void openDroppedFile(const QString &fileName);
	void copyImageToClipboard();
	void firstFrameOrFile();
	void lastFrameOrFile();
	void nextFrameOrFile();
	void previousFrameOrFile();
	void rotateClockwise();
	void toggleFullScreen();
	void togglePlayback();
	void toggleShowCoordinates();
	void toggleYOnly();
	void updateImage();
	void updateView();
	void updateZoomStatus();
	void zoomIn();
	void zoomOut();

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	bool eventFilter(QObject *object, QEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;

private:
	QLabel *mImageLabel;
	QScrollArea *mScrollArea;
	QTimer *mPlayTimer;
	QAction *mSaveAsAction;
	QAction *mCopyAction;
	QAction *mPasteAction;
	QAction *mZoomInAction;
	QAction *mZoomOutAction;
	QAction *mActualSizeAction;
	QAction *mFitToWindowAction;
	QAction *mRotateAction;
	QAction *mYOnlyAction;
	QAction *mCoordinatesAction;
	QAction *mPlayAction;
	QImage mImage;
	QString mCurrentFile;
	RawOpenOptions mRawOptions;
	QString mRawColorSpaceName;
	qint64 mRawFrameSize;
	int mRawWidth;
	int mRawHeight;
	int mRawFrameCount;
	int mCurrentFrame;
	double mScaleFactor;
	bool mFitToWindow;
	bool mCurrentFileIsRaw;
	bool mIsPanning;
	bool mShowCoordinates;
	bool mYOnly;
	int mRotationQuarterTurns;
	QPoint mLastPanPoint;
	QPoint mCursorImagePoint;
};

#endif
