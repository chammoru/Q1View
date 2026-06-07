#ifndef Q1VIEW_VIEWERQT_MAINWINDOW_H
#define Q1VIEW_VIEWERQT_MAINWINDOW_H

#include "RawOpenDialog.h"

#include <QImage>
#include <QMainWindow>
#include <QPoint>
#include <QRect>
#include <QStringList>

class ImageView;
class QDragEnterEvent;
class QDropEvent;
class QAction;
class QActionGroup;
class QEvent;
class QKeyEvent;
class QLabel;
class QMenu;
class QMouseEvent;
class QResizeEvent;
class QRubberBand;
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
	bool openY4mFile(const QString &fileName);

private:
	void createActions();
	// Builds the MFC-style "control panel" menus (resolution / color space / FPS)
	// that sit on the menu bar between File and Edit.
	void createControlMenus();
	// Syncs the control menus' titles, radio checks, and enabled state to the
	// currently loaded image (raw vs. structured, Y4M, frame count).
	void refreshControlMenus();
	void updateMagnifyLabel();
	void applyResolution(int width, int height);
	void applyColorSpace(const QString &colorSpaceName);
	void applyFps(double fps);
	void promptCustomResolution();
	void promptCustomFps();
	QString helpText() const;
	void toggleHelpOverlay();
	void positionHelpOverlay();
	void adjustScrollBar(QScrollBar *scrollBar, double factor);
	void applyZoom(double factor, const QPoint *anchor = nullptr);
	void closeCurrentFile();
	QImage displayImage() const;
	void toggleSelectionMode();
	void clearSelection();
	QImage selectedImage() const;
	QRect imageRectFromViewport(const QPoint &a, const QPoint &b) const;
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
	ImageView *mImageView;
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
	// MFC-style control-panel menus shown on the menu bar (raw sources only).
	QMenu *mResolutionMenu;
	QMenu *mColorSpaceMenu;
	QMenu *mFpsMenu;
	QActionGroup *mResolutionGroup;
	QActionGroup *mColorSpaceGroup;
	QActionGroup *mFpsGroup;
	// Live magnification readout pinned to the menu bar's right corner.
	QLabel *mMagnifyLabel;
	// Translucent shortcut overlay drawn over the viewport (replaces the dialog).
	QLabel *mHelpOverlay;
	double mFps;
	QImage mImage;
	QString mCurrentFile;
	RawOpenOptions mRawOptions;
	QString mRawColorSpaceName;
	qint64 mRawFrameSize;
	int mRawWidth;
	int mRawHeight;
	int mRawFrameCount;
	int mCurrentFrame;
	// YUV4MPEG2 (.y4m) layout. When mIsY4m is true the raw frames are not packed
	// back to back: each is preceded by a "FRAME...\n" marker and the file opens
	// with a text header, so loadRawFrame seeks past both.
	bool mIsY4m;
	qint64 mY4mHeaderLen;
	int mY4mFrameMarkerLen;
	double mScaleFactor;
	bool mFitToWindow;
	bool mCurrentFileIsRaw;
	bool mIsPanning;
	bool mShowCoordinates;
	bool mYOnly;
	int mRotationQuarterTurns;
	QPoint mLastPanPoint;
	QPoint mCursorImagePoint;
	QRubberBand *mRubberBand;
	QAction *mSelectModeAction;
	bool mSelectionMode;
	bool mIsSelecting;
	QPoint mSelectionOrigin;
	QRect mSelectionRect;
};

#endif
