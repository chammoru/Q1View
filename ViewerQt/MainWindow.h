#ifndef Q1VIEW_VIEWERQT_MAINWINDOW_H
#define Q1VIEW_VIEWERQT_MAINWINDOW_H

#include "RawOpenDialog.h"
#include "SyncChannel.h"

#include "qimage_cs.h"

#include <QByteArray>
#include <QImage>
#include <QMainWindow>
#include <QPoint>
#include <QRect>
#include <QStringList>

class ImageView;
class ThumbnailPane;
class VideoView;
class QDockWidget;
class QCloseEvent;
class QDragEnterEvent;
class QDropEvent;
class QAction;
class QActionGroup;
class QEvent;
class QFileSystemWatcher;
class QKeyEvent;
class QLabel;
class QMenu;
class QMouseEvent;
class QResizeEvent;
class QScrollBar;
class QScrollArea;
class QSlider;
class QStackedWidget;
class QTimer;
class QWheelEvent;

class MainWindow : public QMainWindow
{
public:
	explicit MainWindow(QWidget *parent = nullptr);

	bool openFile(const QString &fileName);
	bool openRawFile(const QString &fileName, int width, int height, const QString &colorSpaceName);
	bool openY4mFile(const QString &fileName);

	// Quiet mode: report open/decode failures via qWarning() instead of a modal
	// QMessageBox. The headless --selftest smoke check sets this so a failing
	// fixture exits immediately rather than blocking on a dialog nobody can
	// dismiss (see ViewerQt/main.cpp).
	void setQuiet(bool quiet) { mQuiet = quiet; }

private:
	// Which part of an existing selection a press or hover lands on, so the
	// rectangle can be grabbed and resized by an edge/corner or moved by its
	// interior (mirroring the MFC viewer's resizable selection).
	enum class SelHandle {
		None, Move,
		Left, Right, Top, Bottom,
		TopLeft, TopRight, BottomLeft, BottomRight
	};

	// Open/decode failure notice: a modal QMessageBox normally, or a qWarning()
	// when mQuiet is set (headless --selftest). Keeps the smoke check from
	// hanging on an undismissable dialog.
	void warn(const QString &title, const QString &text);

	void createActions();
	// Builds the MFC-style "control panel" menus (resolution / color space / FPS)
	// that sit on the menu bar between File and Edit.
	void createControlMenus();
	// Matches the menu bar's font and height to the native Windows menu so the Qt
	// viewer's menu bar lines up with the MFC viewer's (no-op off Windows).
	void applyNativeMenuMetrics();
	// Syncs the control menus' titles, radio checks, and enabled state to the
	// currently loaded image (raw vs. structured, Y4M, frame count).
	void refreshControlMenus();
	void updateMagnifyLabel();
	void applyResolution(int width, int height);
	void applyColorSpace(const QString &colorSpaceName);
	void applyFps(double fps);
	// Advance to the next preset resolution / color space for raw input, like the
	// MFC viewer's 'D' and 'N' shortcuts. No-ops for non-raw (fixed-size) sources.
	void cycleResolution();
	void cycleColorSpace();
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
	// Selection resize/move support: map the selection to screen space, hit-test a
	// viewport point against its edges/corners, convert a viewport point to image
	// coordinates, and produce the resized rectangle for an in-progress drag.
	QRect selectionViewportRect() const;
	SelHandle selectionHandleAt(const QPoint &viewportPos) const;
	QPoint imagePointFromViewport(const QPoint &viewportPos) const;
	QRect resizedSelection(SelHandle handle, const QPoint &imageDelta) const;
	void updateSelectionCursor(const QPoint &viewportPos);
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
	void toggleHexValues();
	void toggleSourceValues();
	void toggleInterpolate();
	void showAbout();
	// Maps a display-space pixel to the raw source sample (handling rotation),
	// mirroring the MFC viewer's CViewerDoc::GetNativePixelSample. Returns false
	// for non-raw sources, so the overlay falls back to the converted RGB.
	bool nativeSampleAtDisplay(int displayX, int displayY, QIMAGE_NATIVE_PIXEL_SAMPLE *sample) const;
	// Recent-files (MRU) list, persisted in QSettings and shown under File, like
	// the MFC viewer's recent-file menu.
	void addToRecentFiles(const QString &fileName);
	void updateRecentFilesMenu();
	void loadRecentFiles();
	void saveRecentFiles() const;
	// Grows the window to show a freshly opened image near its natural size,
	// clamped between the default footprint and the available screen, mirroring
	// the MFC viewer which resizes its frame around the image.
	void resizeToImage();
	void updateImage();
	void updateView();
	void updateZoomStatus();
	// Refresh / act on the multi-frame raw seek bar (visibility, range, readout).
	void updateSeekBar();
	void seekToFrame(int frame);
	void zoomIn();
	void zoomOut();

	// --- Video playback (Qt6::Multimedia) ---------------------------------
	// openVideoFile swaps the central area to the video page and starts the
	// clip; showImagePage swaps back and stops any playing video. Both are
	// no-ops in a build without Qt6::Multimedia (mVideoView stays null).
	bool openVideoFile(const QString &fileName);
	void showImagePage();

	// --- File-change auto-refresh -----------------------------------------
	// Watch mCurrentFile (and its directory, so atomic saves are caught) and
	// reload the active frame in place when the bytes on disk change, keeping
	// zoom, pan, selection, rotation, and frame index. Mirrors the Windows
	// FileChangeNotiThread -> WM_RELOAD path.
	void watchCurrentFile();
	void onWatchedPathChanged(const QString &path);
	void reloadCurrentSource();
	void toggleAutoReload();

	// --- Multi-window Sync Input ------------------------------------------
	// Cross-platform replacement for the WM_COPYDATA broadcast: mirror the
	// listed actions to sibling ViewerQt instances and apply theirs.
	void toggleSyncInput();
	void broadcastSync(const SyncMessage &message);
	void broadcastViewState(bool force = false);
	quint32 displayOptionBits() const;
	void applySyncMessage(const SyncMessage &message);

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;
	bool eventFilter(QObject *object, QEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;
	void resizeEvent(QResizeEvent *event) override;
	void closeEvent(QCloseEvent *event) override;

private:
	ImageView *mImageView;
	// Side thumbnail/folder drawer (toggled with E), persisted across sessions.
	ThumbnailPane *mThumbPane = nullptr;
	QDockWidget *mThumbDock = nullptr;
	QAction *mToggleDrawerAction = nullptr;
	int mDrawerWidth = DRAWER_DEF_W;
	QScrollArea *mScrollArea;
	// Image page = scroll area + the raw/sequence seek bar below it. The seek bar
	// (frame slider + frame/time readout) shows only for multi-frame raw sources,
	// mirroring the MFC viewer's bottom progress bar; video files use VideoView's
	// own transport instead.
	QWidget *mImagePage = nullptr;
	QWidget *mSeekContainer = nullptr;
	QSlider *mSeekBar = nullptr;
	QLabel *mSeekLabel = nullptr;
	bool mSeekBarUpdating = false;
	// Hosts the image scroll area and (when built with Qt6::Multimedia) the
	// video page; the active page swaps as image/raw vs. video files open.
	QStackedWidget *mCentralStack;
	VideoView *mVideoView;
	bool mShowingVideo;
	// File-change auto-refresh state.
	QFileSystemWatcher *mFileWatcher;
	QTimer *mReloadTimer;
	QAction *mAutoReloadAction;
	bool mAutoReload;
	// Size/mtime snapshot of the watched file, so a directoryChanged signal from
	// an unrelated sibling file doesn't trigger a needless reload (mirrors the
	// Windows watcher, which compares the changed file name to the watched one).
	qint64 mWatchedSize;
	qint64 mWatchedMtimeMs;
	// True when the current source is a real on-disk file (not the synthetic
	// "Clipboard" source). Lets watchCurrentFile() keep the *directory* watch
	// alive even while the file is briefly absent during an atomic save, without
	// accidentally watching the working directory for a pasted image.
	bool mCurrentSourceOnDisk;
	// Multi-window Sync Input state.
	SyncChannel *mSyncChannel;
	QAction *mSyncInputAction;
	// True while applying an incoming sync message, so the resulting local
	// actions don't echo straight back out and loop between instances.
	bool mApplyingSync;
	// Tick of the last view-state broadcast, to throttle pan/zoom mirroring.
	qint64 mLastViewStateBroadcastMs;
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
	QAction *mHexValuesAction = nullptr;
	QAction *mSourceYuvAction = nullptr;
	QAction *mCoordinatesAction;
	QAction *mInterpolateAction;
	QAction *mPlayAction;
	QMenu *mRecentMenu;
	QStringList mRecentFiles;
	bool mInterpolate;
	// MFC-style control-panel menus shown on the menu bar (raw sources only).
	QMenu *mResolutionMenu;
	QMenu *mColorSpaceMenu;
	QMenu *mFpsMenu;
	QActionGroup *mResolutionGroup;
	QActionGroup *mColorSpaceGroup;
	QActionGroup *mFpsGroup;
	QAction *mNextResolutionAction = nullptr;
	QAction *mNextColorSpaceAction = nullptr;
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
	// Retained bytes of the current raw frame plus its color space's native-pixel
	// sampler, so the overlay can read source Y/U/V values. Empty/null for non-raw
	// sources (decoded images, clipboard, video).
	QByteArray mRawSource;
	QIMAGE_SAMPLE_NATIVE_PIXEL_FN mRawSampler = nullptr;
	// YUV4MPEG2 (.y4m) layout. When mIsY4m is true the raw frames are not packed
	// back to back: each is preceded by a "FRAME...\n" marker and the file opens
	// with a text header, so loadRawFrame seeks past both.
	bool mIsY4m;
	// True when the frame rate is a real timing source (Y4M header, an fps= tag in
	// a raw filename, or a user-chosen FPS), so the seek bar shows playback time;
	// a plain raw sequence at the default rate shows only the frame counter, like
	// the MFC viewer's mHasTimingFps.
	bool mHasTimingFps = false;
	qint64 mY4mHeaderLen;
	int mY4mFrameMarkerLen;
	double mScaleFactor;
	bool mFitToWindow;
	// True only while folder navigation (next/prev/first/last file) loads an
	// image: resizeToImage() then keeps the current window size and just fits
	// the new image into the existing viewport, instead of resizing the frame
	// around every image (issue #69).
	bool mKeepWindowOnLoad = false;
	// True once the window has been sized to an image this session; subsequent
	// loads then keep the current window instead of resizing to each image.
	bool mWindowSized = false;
	bool mCurrentFileIsRaw;
	bool mIsPanning;
	bool mShowCoordinates;
	bool mYOnly;
	// Per-pixel value overlay format. Decimal by default, like the MFC viewer's
	// 'H' toggle; mirrored to ImageView and over Sync Input.
	bool mHexMode = false;
	// Show the native source components (Y/U/V for raw YUV) in the overlay instead
	// of the converted RGB. On by default, like the MFC viewer's 'V' toggle; only
	// affects raw YUV sources (a no-op for RGB/decoded images).
	bool mShowSourceValues = true;
	int mRotationQuarterTurns;
	QPoint mLastPanPoint;
	QPoint mCursorImagePoint;
	QAction *mSelectModeAction;
	bool mSelectionMode;
	bool mIsSelecting;
	QPoint mSelectionOrigin;
	QRect mSelectionRect;
	// Active resize/move of an existing selection. SelHandle::None means no such
	// drag is in progress (a fresh rubber-band drag is tracked by mIsSelecting).
	SelHandle mActiveHandle;
	QRect mDragStartRect;
	QPoint mDragStartImagePoint;
	// True under the headless --selftest smoke check: route warn() to qWarning()
	// instead of a modal dialog.
	bool mQuiet = false;
};

#endif
