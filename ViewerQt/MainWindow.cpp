#include "MainWindow.h"

#include "HeifReader.h"
#include "ImageView.h"
#include "RawOpenDialog.h"
#include "Y4mReader.h"
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
#include "VideoView.h"
#endif

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QGesture>
#include <QGestureEvent>
#include <QImageReader>
#include <QInputDialog>
#include <QIODevice>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPinchGesture>
#include <QPointF>
#include <QRect>
#include <QResizeEvent>
#include <QScopeGuard>
#include <QScreen>
#include <QScrollBar>
#include <QScrollArea>
#include <QSettings>
#include <QSize>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QTransform>
#include <QUrl>
#include <QVariant>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "qimage_cs.h"
#include "qimage_presets.h"
#include "qimage_util.h"
// Shared "WxH" / numeric label parsing, reused from the cross-platform core so
// the Qt viewer and the MFC front-ends interpret the preset tables identically.
#include "QImageStr.h"
// Shared shortcut-panel content (keys + descriptions), so this overlay and the
// MFC viewer's DrawHelpMenu render the same list from one place.
#include "QViewerShortcuts.h"

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX // keep std::min/std::max usable alongside <windows.h>
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <QFont>
#include <QFontMetrics>
#endif

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent),
	  mImageView(new ImageView),
	  mScrollArea(new QScrollArea),
	  mCentralStack(new QStackedWidget),
	  mVideoView(nullptr),
	  mShowingVideo(false),
	  mFileWatcher(nullptr),
	  mReloadTimer(new QTimer(this)),
	  mAutoReloadAction(nullptr),
	  mAutoReload(true),
	  mWatchedSize(-1),
	  mWatchedMtimeMs(-1),
	  mCurrentSourceOnDisk(false),
	  mSyncChannel(new SyncChannel(this)),
	  mSyncInputAction(nullptr),
	  mApplyingSync(false),
	  mLastViewStateBroadcastMs(0),
	  mPlayTimer(new QTimer(this)),
	  mSaveAsAction(nullptr),
	  mCopyAction(nullptr),
	  mPasteAction(nullptr),
	  mZoomInAction(nullptr),
	  mZoomOutAction(nullptr),
	  mActualSizeAction(nullptr),
	  mFitToWindowAction(nullptr),
	  mRotateAction(nullptr),
	  mYOnlyAction(nullptr),
	  mCoordinatesAction(nullptr),
	  mInterpolateAction(nullptr),
	  mPlayAction(nullptr),
	  mRecentMenu(nullptr),
	  mInterpolate(false),
	  mResolutionMenu(nullptr),
	  mColorSpaceMenu(nullptr),
	  mFpsMenu(nullptr),
	  mResolutionGroup(nullptr),
	  mColorSpaceGroup(nullptr),
	  mFpsGroup(nullptr),
	  mMagnifyLabel(nullptr),
	  mHelpOverlay(nullptr),
	  mFps(30.0),
	  mRawFrameSize(0),
	  mRawWidth(0),
	  mRawHeight(0),
	  mRawFrameCount(0),
	  mCurrentFrame(0),
	  mIsY4m(false),
	  mY4mHeaderLen(0),
	  mY4mFrameMarkerLen(0),
	  mScaleFactor(1.0),
	  mFitToWindow(true),
	  mCurrentFileIsRaw(false),
	  mIsPanning(false),
	  mShowCoordinates(false),
	  mYOnly(false),
	  mRotationQuarterTurns(0),
	  mCursorImagePoint(-1, -1),
	  mSelectModeAction(nullptr),
	  mSelectionMode(false),
	  mIsSelecting(false),
	  mActiveHandle(SelHandle::None)
{
	mImageView->setAcceptDrops(false);
	mImageView->setMouseTracking(true);
	// The overlay asks for the native source sample per visible cell; the callback
	// returns false for non-raw sources, so it falls back to the converted RGB.
	mImageView->setNativeSampler([this](int x, int y, QIMAGE_NATIVE_PIXEL_SAMPLE *s) {
		return nativeSampleAtDisplay(x, y, s);
	});

	mScrollArea->setBackgroundRole(QPalette::Dark);
	mScrollArea->setWidget(mImageView);
	mScrollArea->setWidgetResizable(false);
	mScrollArea->setAlignment(Qt::AlignCenter);
	mScrollArea->setAcceptDrops(false);
	mScrollArea->viewport()->setMouseTracking(true);
	mScrollArea->viewport()->setFocusPolicy(Qt::StrongFocus);
	mScrollArea->viewport()->grabGesture(Qt::PinchGesture);
	mScrollArea->viewport()->installEventFilter(this);
	// The image scroll area is page 0 of the central stack; the optional video
	// page is added lazily the first time a clip opens (see openVideoFile).
	mCentralStack->addWidget(mScrollArea);
	setCentralWidget(mCentralStack);
	setAcceptDrops(true);
	setFocusPolicy(Qt::StrongFocus);

	mPlayTimer->setInterval(static_cast<int>(1000.0 / mFps + 0.5));
	connect(mPlayTimer, &QTimer::timeout, this, &MainWindow::nextFrameOrFile);

	// File-change auto-refresh: coalesce the watcher's rapid-fire signals (editors
	// often write a file in several steps) into a single in-place reload.
	mFileWatcher = new QFileSystemWatcher(this);
	connect(mFileWatcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onWatchedPathChanged);
	connect(mFileWatcher, &QFileSystemWatcher::directoryChanged, this, &MainWindow::onWatchedPathChanged);
	mReloadTimer->setSingleShot(true);
	mReloadTimer->setInterval(180);
	connect(mReloadTimer, &QTimer::timeout, this, &MainWindow::reloadCurrentSource);

	// Multi-window Sync Input: apply mirrored actions from sibling instances.
	connect(mSyncChannel, &SyncChannel::received, this, &MainWindow::applySyncMessage);

	// Translucent shortcut overlay. Parented to the viewport so it stays pinned to
	// the visible area instead of scrolling with the image, mirroring the MFC
	// viewer's in-canvas help rather than a modal dialog.
	mHelpOverlay = new QLabel(mScrollArea->viewport());
	// Rich text so the explicit ink colour in helpText() survives the system
	// dark-mode palette; the Cascadia Mono family/size is set both on the widget
	// and inline (helpText) to match the MFC panel's lfHeight = 14.
	mHelpOverlay->setTextFormat(Qt::RichText);
	mHelpOverlay->setText(helpText());
	mHelpOverlay->setMargin(16);
	{
		QFont overlayFont(QStringLiteral("Cascadia Mono"));
		overlayFont.setStyleHint(QFont::Monospace);
		overlayFont.setPixelSize(14);
		mHelpOverlay->setFont(overlayFont);
	}
	// Match the MFC shortcuts panel (DrawHelpMenu): a white surface card with a
	// soft border. Background/border apply cleanly from the stylesheet; the text
	// colour is carried by the rich text above. Colours mirror
	// Q1UI_COLOR_SURFACE / Q1UI_COLOR_BORDER / Q1UI_COLOR_TEXT.
	mHelpOverlay->setStyleSheet(QStringLiteral(
		"background-color: #ffffff;"
		"border: 1px solid #d8e0ea;"
		"border-radius: 8px;"));
	mHelpOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	mHelpOverlay->hide();

	loadSettings();
	createActions();
	applyNativeMenuMetrics();
	refreshControlMenus();
	updateZoomStatus();
	statusBar()->showMessage(tr("Ready"));
	setWindowTitle(tr("Q1View Qt"));
	// Open at the same footprint as the MFC viewer, whose client area defaults to
	// VIEWER_DEF_W x VIEWER_DEF_H (500 x 392). The menu bar and status bar live
	// inside the Qt window, so size the central area to leave the viewport close
	// to that, keeping the on-screen window the same size as the MFC one.
	resize(500, 412);
}

void MainWindow::warn(const QString &title, const QString &text)
{
	if (mQuiet) {
		qWarning("%s: %s", qPrintable(title), qPrintable(text));
		return;
	}
	QMessageBox::warning(this, title, text);
}

bool MainWindow::openFile(const QString &fileName)
{
	// A playable video container goes to the Multimedia page rather than the
	// image decoders (only recognised when built with Qt6::Multimedia).
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
	if (VideoView::isVideoFile(fileName)) {
		return openVideoFile(fileName);
	}
#endif

	// A .y4m/.yuv may be a YUV4MPEG2 container rather than a still image; route
	// it to the Y4M loader, which parses the header and frame markers.
	if (q1y4m::isY4mFile(fileName)) {
		return openY4mFile(fileName);
	}

	showImagePage();
	QImageReader reader(fileName);
	reader.setAutoTransform(true);

	// QImageReader covers the formats Qt was built with. HEIF/HEIC/AVIF are not
	// among them, so fall back to libheif when Qt can't read a HEIF-family file.
	QImage image = reader.read();
	const bool isHeif = q1qt::isHeifFile(fileName);
	if (image.isNull() && isHeif) {
		image = q1qt::readHeif(fileName);
	}
	if (image.isNull()) {
		// Qt's "unsupported format" string is misleading for the libheif path,
		// so spell out why a HEIF-family file failed instead.
		QString detail;
		if (isHeif && !q1qt::heifSupported()) {
			detail = tr("HEIF/HEIC/AVIF support is not available in this build.");
		} else if (isHeif) {
			detail = tr("The HEIF/HEIC/AVIF file could not be decoded.");
		} else {
			detail = reader.errorString();
		}
		warn(tr("Open image"),
			tr("Could not read %1:\n%2").arg(QFileInfo(fileName).fileName(), detail));
		return false;
	}

	resetPlayback();
	mImage = image;
	mCurrentFile = fileName;
	mCurrentFileIsRaw = false;
	mRawSource.clear();
	mRawSampler = nullptr;
	mIsY4m = false;
	mRawFrameSize = 0;
	mRawFrameCount = 1;
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
	clearSelection();
	mFitToWindow = true;
	mCurrentSourceOnDisk = true;
	updateImage();
	resizeToImage();
	addToRecentFiles(mCurrentFile);
	watchCurrentFile();
	return true;
}

void MainWindow::createActions()
{
	QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

	QAction *openAction = fileMenu->addAction(tr("&Open..."));
	openAction->setShortcut(QKeySequence::Open);
	connect(openAction, &QAction::triggered, this, [this]() {
		const QString heifExtensions = q1qt::heifSupported()
			? QStringLiteral(" *.heic *.heif *.hif *.avif") : QString();
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
		const QString videoFilter = QStringLiteral(";;Videos (%1)")
			.arg(VideoView::nameFilters().join(QLatin1Char(' ')));
#else
		const QString videoFilter;
#endif
		const QString fileName = QFileDialog::getOpenFileName(
			this,
			tr("Open image"),
			QString(),
			tr("Images (*.bmp *.gif *.jpeg *.jpg *.png *.ppm *.pgm *.pbm *.xbm *.xpm%1)%2;;All files (*)")
				.arg(heifExtensions, videoFilter));

		if (!fileName.isEmpty()) {
			openFile(fileName);
		}
	});

	fileMenu->addSeparator();

	mSaveAsAction = fileMenu->addAction(tr("Save &As..."));
	mSaveAsAction->setShortcut(QKeySequence(tr("Ctrl+Alt+S")));
	connect(mSaveAsAction, &QAction::triggered, this, &MainWindow::saveImageAs);

	QAction *closeAction = fileMenu->addAction(tr("&Close"));
	closeAction->setShortcut(QKeySequence::Close);
	connect(closeAction, &QAction::triggered, this, &MainWindow::closeCurrentFile);

	fileMenu->addSeparator();

	QAction *openRawAction = fileMenu->addAction(tr("Open &Raw..."));
	connect(openRawAction, &QAction::triggered, this, [this]() {
		RawOpenOptions options = mRawOptions;
		if (!RawOpenDialog::getOptions(this, &options)) {
			return;
		}

		if (openRawFile(options.fileName, options.width, options.height, options.colorSpaceName)) {
			mRawOptions = options;
			saveRawSettings();
		}
	});

	fileMenu->addSeparator();

	mRecentMenu = fileMenu->addMenu(tr("Open &Recent"));
	updateRecentFilesMenu();

	fileMenu->addSeparator();

	QAction *exitAction = fileMenu->addAction(tr("E&xit"));
	exitAction->setShortcut(QKeySequence::Quit);
	connect(exitAction, &QAction::triggered, this, &QWidget::close);

	// Resolution / color space / FPS dropdowns sit right after File, matching the
	// MFC viewer's menu bar layout.
	createControlMenus();

	QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));

	mCopyAction = editMenu->addAction(tr("&Copy"));
	mCopyAction->setShortcut(QKeySequence::Copy);
	connect(mCopyAction, &QAction::triggered, this, &MainWindow::copyImageToClipboard);

	mPasteAction = editMenu->addAction(tr("&Paste"));
	mPasteAction->setShortcut(QKeySequence::Paste);
	connect(mPasteAction, &QAction::triggered, this, &MainWindow::openClipboardImage);

	QMenu *viewMenu = menuBar()->addMenu(tr("&View"));

	mZoomInAction = viewMenu->addAction(tr("Zoom &In"));
	mZoomInAction->setShortcuts(QKeySequence::ZoomIn);
	connect(mZoomInAction, &QAction::triggered, this, &MainWindow::zoomIn);

	mZoomOutAction = viewMenu->addAction(tr("Zoom &Out"));
	mZoomOutAction->setShortcuts(QKeySequence::ZoomOut);
	connect(mZoomOutAction, &QAction::triggered, this, &MainWindow::zoomOut);

	mActualSizeAction = viewMenu->addAction(tr("&Actual Size"));
	mActualSizeAction->setShortcut(QKeySequence(tr("Ctrl+0")));
	connect(mActualSizeAction, &QAction::triggered, this, &MainWindow::setActualSize);

	mFitToWindowAction = viewMenu->addAction(tr("&Fit to Window"));
	mFitToWindowAction->setShortcut(QKeySequence(tr("Ctrl+1")));
	mFitToWindowAction->setCheckable(true);
	mFitToWindowAction->setChecked(mFitToWindow);
	connect(mFitToWindowAction, &QAction::triggered, this, &MainWindow::fitImageToWindow);

	viewMenu->addSeparator();

	mRotateAction = viewMenu->addAction(tr("&Rotate Clockwise"));
	mRotateAction->setShortcut(QKeySequence(tr("R")));
	connect(mRotateAction, &QAction::triggered, this, &MainWindow::rotateClockwise);

	mYOnlyAction = viewMenu->addAction(tr("&Y-only View"));
	mYOnlyAction->setShortcut(QKeySequence(tr("Y")));
	mYOnlyAction->setCheckable(true);
	connect(mYOnlyAction, &QAction::triggered, this, &MainWindow::toggleYOnly);

	mHexValuesAction = viewMenu->addAction(tr("He&xadecimal Pixel Values"));
	mHexValuesAction->setShortcut(QKeySequence(tr("H")));
	mHexValuesAction->setCheckable(true);
	mHexValuesAction->setChecked(mHexMode);
	connect(mHexValuesAction, &QAction::triggered, this, &MainWindow::toggleHexValues);

	mSourceYuvAction = viewMenu->addAction(tr("Source &YUV Pixel Values"));
	mSourceYuvAction->setShortcut(QKeySequence(tr("V")));
	mSourceYuvAction->setCheckable(true);
	mSourceYuvAction->setChecked(mShowSourceValues);
	connect(mSourceYuvAction, &QAction::triggered, this, &MainWindow::toggleSourceValues);

	mCoordinatesAction = viewMenu->addAction(tr("Cursor &Coordinates"));
	mCoordinatesAction->setShortcut(QKeySequence(tr("C")));
	mCoordinatesAction->setCheckable(true);
	connect(mCoordinatesAction, &QAction::triggered, this, &MainWindow::toggleShowCoordinates);

	mInterpolateAction = viewMenu->addAction(tr("&Interpolate Pixels"));
	mInterpolateAction->setShortcut(QKeySequence(tr("I")));
	mInterpolateAction->setCheckable(true);
	mInterpolateAction->setChecked(mInterpolate);
	connect(mInterpolateAction, &QAction::triggered, this, &MainWindow::toggleInterpolate);

	mSelectModeAction = viewMenu->addAction(tr("&Selection Mode"));
	mSelectModeAction->setShortcut(QKeySequence(tr("S")));
	mSelectModeAction->setCheckable(true);
	connect(mSelectModeAction, &QAction::triggered, this, &MainWindow::toggleSelectionMode);

	viewMenu->addSeparator();

	mAutoReloadAction = viewMenu->addAction(tr("&Auto-Reload on Change"));
	mAutoReloadAction->setCheckable(true);
	mAutoReloadAction->setChecked(mAutoReload);
	connect(mAutoReloadAction, &QAction::triggered, this, &MainWindow::toggleAutoReload);

	viewMenu->addSeparator();

	QAction *fullScreenAction = viewMenu->addAction(tr("&Full Screen"));
	fullScreenAction->setShortcut(QKeySequence(tr("Return")));
	connect(fullScreenAction, &QAction::triggered, this, &MainWindow::toggleFullScreen);

	QMenu *navigateMenu = menuBar()->addMenu(tr("&Navigate"));

	QAction *previousAction = navigateMenu->addAction(tr("&Previous Frame/File"));
	previousAction->setShortcut(QKeySequence(tr("Left")));
	connect(previousAction, &QAction::triggered, this, &MainWindow::previousFrameOrFile);

	QAction *nextAction = navigateMenu->addAction(tr("&Next Frame/File"));
	nextAction->setShortcut(QKeySequence(tr("Right")));
	connect(nextAction, &QAction::triggered, this, &MainWindow::nextFrameOrFile);

	QAction *previousFileAction = navigateMenu->addAction(tr("Previous &File"));
	previousFileAction->setShortcut(QKeySequence(tr("PgUp")));
	connect(previousFileAction, &QAction::triggered, this, [this]() { openAdjacentFile(-1); });

	QAction *nextFileAction = navigateMenu->addAction(tr("Next F&ile"));
	nextFileAction->setShortcut(QKeySequence(tr("PgDown")));
	connect(nextFileAction, &QAction::triggered, this, [this]() { openAdjacentFile(1); });

	QAction *firstAction = navigateMenu->addAction(tr("&First Frame/File"));
	firstAction->setShortcut(QKeySequence(tr("Home")));
	connect(firstAction, &QAction::triggered, this, &MainWindow::firstFrameOrFile);

	QAction *lastAction = navigateMenu->addAction(tr("&Last Frame/File"));
	lastAction->setShortcut(QKeySequence(tr("End")));
	connect(lastAction, &QAction::triggered, this, &MainWindow::lastFrameOrFile);

	navigateMenu->addSeparator();

	mPlayAction = navigateMenu->addAction(tr("&Play / Stop"));
	mPlayAction->setShortcut(QKeySequence(tr("Space")));
	mPlayAction->setCheckable(true);
	connect(mPlayAction, &QAction::triggered, this, &MainWindow::togglePlayback);

	navigateMenu->addSeparator();

	mSyncInputAction = navigateMenu->addAction(tr("&Sync Input (multi-window)"));
	mSyncInputAction->setCheckable(true);
	connect(mSyncInputAction, &QAction::triggered, this, &MainWindow::toggleSyncInput);

	QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));

	QAction *shortcutsAction = helpMenu->addAction(tr("&Shortcuts"));
	shortcutsAction->setShortcut(QKeySequence(tr("?")));
	connect(shortcutsAction, &QAction::triggered, this, &MainWindow::showHelp);

	QAction *aboutAction = helpMenu->addAction(tr("&About Q1View Qt"));
	connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

	// Live magnification readout pinned to the menu bar's right corner, mirroring
	// the MFC viewer's right-justified "WxH (n.nnx)" item.
	mMagnifyLabel = new QLabel(menuBar());
	mMagnifyLabel->setContentsMargins(0, 0, 12, 0);
	menuBar()->setCornerWidget(mMagnifyLabel, Qt::TopRightCorner);
}

void MainWindow::createControlMenus()
{
	mResolutionMenu = menuBar()->addMenu(QStringLiteral("1920x1080"));
	mNextResolutionAction = mResolutionMenu->addAction(tr("Next Preset &Resolution"));
	mNextResolutionAction->setShortcut(QKeySequence(tr("D")));
	connect(mNextResolutionAction, &QAction::triggered, this, &MainWindow::cycleResolution);
	mResolutionMenu->addSeparator();
	mResolutionGroup = new QActionGroup(this);
	mResolutionGroup->setExclusive(true);
	for (size_t i = 0; i < ARRAY_SIZE(q1::resolution_info_table); ++i) {
		const char *entry = q1::resolution_info_table[i];
		const QString label = QString::fromLatin1(entry);
		int w = 0;
		int h = 0;
		// image_parse_w_h() returns 0 when it pulls a "WxH" out of the label.
		if (q1::image_parse_w_h(entry, &w, &h) != 0) {
			// The trailing "C&ustom..." sentinel.
			mResolutionMenu->addSeparator();
			QAction *customResolution = mResolutionMenu->addAction(label);
			connect(customResolution, &QAction::triggered, this, &MainWindow::promptCustomResolution);
			continue;
		}
		QAction *action = mResolutionMenu->addAction(label);
		action->setCheckable(true);
		action->setData(QSize(w, h));
		mResolutionGroup->addAction(action);
		connect(action, &QAction::triggered, this, [this, w, h]() { applyResolution(w, h); });
	}

	mColorSpaceMenu = menuBar()->addMenu(QStringLiteral("YUV420"));
	mNextColorSpaceAction = mColorSpaceMenu->addAction(tr("&Next Color Space"));
	mNextColorSpaceAction->setShortcut(QKeySequence(tr("N")));
	connect(mNextColorSpaceAction, &QAction::triggered, this, &MainWindow::cycleColorSpace);
	mColorSpaceMenu->addSeparator();
	mColorSpaceGroup = new QActionGroup(this);
	mColorSpaceGroup->setExclusive(true);
	for (size_t i = 0; i < ARRAY_SIZE(qcsc_info_table); ++i) {
		// Only color spaces with a raw loader/converter can be applied to a file,
		// matching the formats offered by the Open Raw dialog.
		if (!qcsc_info_table[i].cs_load_info || !qcsc_info_table[i].csc2rgb888) {
			continue;
		}
		const QString name = QString::fromLatin1(qcsc_info_table[i].name);
		QAction *action = mColorSpaceMenu->addAction(name.toUpper());
		action->setCheckable(true);
		action->setData(name);
		mColorSpaceGroup->addAction(action);
		connect(action, &QAction::triggered, this, [this, name]() { applyColorSpace(name); });
	}

	mFpsMenu = menuBar()->addMenu(QStringLiteral("30.00fps"));
	mFpsGroup = new QActionGroup(this);
	mFpsGroup->setExclusive(true);
	for (size_t i = 0; i < ARRAY_SIZE(qfps_info_table); ++i) {
		const QString entry = QString::fromLatin1(qfps_info_table[i]);
		if (!entry.endsWith(QStringLiteral("fps"))) {
			// The trailing "C&ustom..." sentinel.
			mFpsMenu->addSeparator();
			QAction *customFps = mFpsMenu->addAction(entry);
			connect(customFps, &QAction::triggered, this, &MainWindow::promptCustomFps);
			continue;
		}
		const double fps = entry.left(entry.size() - 3).toDouble();
		QAction *action = mFpsMenu->addAction(entry);
		action->setCheckable(true);
		action->setData(fps);
		mFpsGroup->addAction(action);
		connect(action, &QAction::triggered, this, [this, fps]() { applyFps(fps); });
	}
}

void MainWindow::applyNativeMenuMetrics()
{
#ifdef Q_OS_WIN
	// Use the exact font Win32 (and therefore the MFC viewer) draws menus with,
	// so the two viewers' menu bars use the same family and size.
	NONCLIENTMETRICSW ncm;
	ncm.cbSize = sizeof(ncm);
	if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0)) {
		const LOGFONTW &lf = ncm.lfMenuFont;
		QFont menuFont(QString::fromWCharArray(lf.lfFaceName));
		// lfHeight is a device-pixel em height at the system DPI; convert it back
		// to a DPI-independent point size so Qt's own scaling reproduces the same
		// physical size as the native menu.
		HDC hdc = GetDC(nullptr);
		const int dpi = hdc ? GetDeviceCaps(hdc, LOGPIXELSY) : 96;
		if (hdc) {
			ReleaseDC(nullptr, hdc);
		}
		if (lf.lfHeight < 0 && dpi > 0) {
			menuFont.setPointSizeF(-lf.lfHeight * 72.0 / dpi);
		}
		menuFont.setBold(lf.lfWeight >= FW_BOLD);
		menuFont.setItalic(lf.lfItalic != 0);

		menuBar()->setFont(menuFont);
		if (mMagnifyLabel) {
			mMagnifyLabel->setFont(menuFont);
		}
		// Qt's default menu bar is a few pixels taller than the Win32 menu; trim
		// the per-item vertical padding so the bar height tracks the font, the way
		// the MFC menu does. (A fixed height instead collapses items into the
		// overflow button, so style the padding and let the bar auto-size.)
		menuBar()->setStyleSheet(QStringLiteral(
			"QMenuBar { padding: 0px; }"
			"QMenuBar::item { padding: 2px 7px; }"));
	}
#endif
}

void MainWindow::refreshControlMenus()
{
	// Resolution and color space can only be reinterpreted for our own packed raw
	// sources; structured images carry their own dimensions, and Y4M defines them
	// in its container header.
	const bool rawTweakable = mCurrentFileIsRaw && !mIsY4m;
	const bool fpsTweakable = mCurrentFileIsRaw && mRawFrameCount > 1;

	// The N / D cycle shortcuts only apply to reinterpretable raw input.
	if (mNextColorSpaceAction) {
		mNextColorSpaceAction->setEnabled(rawTweakable);
	}
	if (mNextResolutionAction) {
		mNextResolutionAction->setEnabled(rawTweakable);
	}

	if (mResolutionMenu) {
		mResolutionMenu->menuAction()->setEnabled(rawTweakable);
		int w = mRawOptions.width;
		int h = mRawOptions.height;
		if (rawTweakable) {
			w = mRawWidth;
			h = mRawHeight;
		} else if (!mImage.isNull()) {
			// Structured images carry their own dimensions; show them (disabled).
			w = mImage.width();
			h = mImage.height();
		}
		const QString title = QStringLiteral("%1x%2").arg(w).arg(h);
		if (mResolutionMenu->title() != title) {
			mResolutionMenu->setTitle(title);
		}
		const QSize current(w, h);
		for (QAction *action : mResolutionGroup->actions()) {
			action->setChecked(action->data().toSize() == current);
		}
	}

	if (mColorSpaceMenu) {
		mColorSpaceMenu->menuAction()->setEnabled(rawTweakable);
		const QString name = rawTweakable ? mRawColorSpaceName : mRawOptions.colorSpaceName;
		const QString title = name.toUpper();
		if (mColorSpaceMenu->title() != title) {
			mColorSpaceMenu->setTitle(title);
		}
		for (QAction *action : mColorSpaceGroup->actions()) {
			action->setChecked(action->data().toString() == name);
		}
	}

	if (mFpsMenu) {
		mFpsMenu->menuAction()->setEnabled(fpsTweakable);
		const QString title = QStringLiteral("%1fps").arg(mFps, 0, 'f', 2);
		if (mFpsMenu->title() != title) {
			mFpsMenu->setTitle(title);
		}
		for (QAction *action : mFpsGroup->actions()) {
			action->setChecked(action->data().toDouble() == mFps);
		}
	}
}

void MainWindow::updateMagnifyLabel()
{
	if (!mMagnifyLabel) {
		return;
	}

	if (mImage.isNull()) {
		mMagnifyLabel->clear();
		return;
	}

	// mImageView is sized to the on-screen (scaled, rotated) pixmap, so its size is
	// exactly the destination dimensions the MFC readout reports.
	mMagnifyLabel->setText(QStringLiteral("%1x%2 (%3x)")
		.arg(mImageView->width())
		.arg(mImageView->height())
		.arg(mScaleFactor, 0, 'f', 2));
}

void MainWindow::applyResolution(int width, int height)
{
	if (width <= 0 || height <= 0) {
		refreshControlMenus();
		return;
	}

	if (mCurrentFileIsRaw && !mIsY4m && !mCurrentFile.isEmpty()
		&& (width != mRawWidth || height != mRawHeight)) {
		if (openRawFile(mCurrentFile, width, height, mRawColorSpaceName)) {
			mRawOptions.width = width;
			mRawOptions.height = height;
			saveRawSettings();
		}
	}
	refreshControlMenus();

	SyncMessage message;
	message.command = SyncMessage::Resolution;
	message.first = width;
	message.second = height;
	broadcastSync(message);
}

void MainWindow::applyColorSpace(const QString &colorSpaceName)
{
	if (mCurrentFileIsRaw && !mIsY4m && !mCurrentFile.isEmpty()
		&& colorSpaceName != mRawColorSpaceName) {
		if (openRawFile(mCurrentFile, mRawWidth, mRawHeight, colorSpaceName)) {
			mRawOptions.colorSpaceName = colorSpaceName;
			saveRawSettings();
		}
	}
	refreshControlMenus();

	SyncMessage message;
	message.command = SyncMessage::ColorSpace;
	message.text = colorSpaceName;
	broadcastSync(message);
}

void MainWindow::applyFps(double fps)
{
	if (fps > 0.0) {
		mFps = fps;
		mPlayTimer->setInterval(static_cast<int>(1000.0 / mFps + 0.5));
	}
	refreshControlMenus();

	SyncMessage message;
	message.command = SyncMessage::Fps;
	message.scalar = fps;
	broadcastSync(message);
}

void MainWindow::cycleResolution()
{
	// Only packed raw sources can be reinterpreted at another size (Y4M and
	// decoded images carry their own), mirroring CMainFrame::NextResolution.
	if (!mCurrentFileIsRaw || mIsY4m) {
		return;
	}
	int w = 0;
	int h = 0;
	q1::image_next_resolution(mRawWidth, mRawHeight, &w, &h);
	if (w > 0 && h > 0) {
		applyResolution(w, h);
	}
}

void MainWindow::cycleColorSpace()
{
	if (!mCurrentFileIsRaw || mIsY4m) {
		return;
	}
	const int count = static_cast<int>(ARRAY_SIZE(qcsc_info_table));
	int current = -1;
	for (int i = 0; i < count; ++i) {
		if (QString::fromLatin1(qcsc_info_table[i].name) == mRawColorSpaceName) {
			current = i;
			break;
		}
	}
	if (current < 0) {
		return;
	}
	// Advance to the next color space that has a raw loader (the same set the
	// menu offers), wrapping around; image_next_cs can land on entries without a
	// loader, so skip those rather than fail the reopen.
	for (int step = 1; step <= count; ++step) {
		const qcsc_info &c = qcsc_info_table[(current + step) % count];
		if (c.cs_load_info && c.csc2rgb888) {
			applyColorSpace(QString::fromLatin1(c.name));
			return;
		}
	}
}

void MainWindow::promptCustomResolution()
{
	bool ok = false;
	const int width = QInputDialog::getInt(this, tr("Custom resolution"), tr("Width"),
		mCurrentFileIsRaw ? mRawWidth : mRawOptions.width, 1, 32768, 1, &ok);
	if (!ok) {
		refreshControlMenus();
		return;
	}
	const int height = QInputDialog::getInt(this, tr("Custom resolution"), tr("Height"),
		mCurrentFileIsRaw ? mRawHeight : mRawOptions.height, 1, 32768, 1, &ok);
	if (!ok) {
		refreshControlMenus();
		return;
	}
	applyResolution(width, height);
}

void MainWindow::promptCustomFps()
{
	bool ok = false;
	const double fps = QInputDialog::getDouble(this, tr("Custom FPS"), tr("Frames per second"),
		mFps, 0.01, 1000.0, 2, &ok);
	if (ok) {
		applyFps(fps);
	} else {
		refreshControlMenus();
	}
}

QString MainWindow::helpText() const
{
	// Two-column "key  description" layout built from the shared shortcut table
	// (QViewerShortcuts.h), so this overlay and the MFC viewer's DrawHelpMenu
	// stay in sync. Rows tagged for the Qt front-end are shown; the monospace
	// font aligns the descriptions at the fixed key-column width.
	QString text = QString::fromLatin1(Q1VIEW_SHORTCUTS_TITLE) + QStringLiteral("\n\n");
	for (size_t i = 0; i < sizeof(Q1VIEW_SHORTCUTS) / sizeof(Q1VIEW_SHORTCUTS[0]); ++i) {
		const Q1ViewShortcutRow &row = Q1VIEW_SHORTCUTS[i];
		if (!(row.fe & Q1VIEW_FE_QT)) {
			continue;
		}
		text += QStringLiteral("%1%2\n")
			.arg(QString::fromLatin1(row.key), -Q1VIEW_SHORTCUTS_KEY_WIDTH)
			.arg(QString::fromLatin1(row.desc));
	}

	// Emit it as preformatted rich text with an explicit dark colour and the
	// Cascadia Mono family/size. A plain QLabel stylesheet `color` was being
	// overridden by the system dark-mode palette (white text on the white card,
	// i.e. invisible); an inline rich-text colour is honoured regardless of the
	// palette. <pre> keeps the monospace columns aligned. Colour mirrors
	// Q1UI_COLOR_TEXT, the same dark ink the MFC panel draws with.
	return QStringLiteral(
		"<pre style=\"margin:0; color:#1f2937; "
		"font-family:'Cascadia Mono'; font-size:14px;\">%1</pre>")
		.arg(text.trimmed().toHtmlEscaped());
}

void MainWindow::toggleHelpOverlay()
{
	if (!mHelpOverlay) {
		return;
	}

	if (mHelpOverlay->isVisible()) {
		mHelpOverlay->hide();
		return;
	}

	positionHelpOverlay();
	mHelpOverlay->show();
	mHelpOverlay->raise();
}

void MainWindow::positionHelpOverlay()
{
	if (!mHelpOverlay) {
		return;
	}

	mHelpOverlay->adjustSize();
	const QRect viewport = mScrollArea->viewport()->rect();
	QPoint topLeft = viewport.center() - mHelpOverlay->rect().center();
	// In a small window the panel can be wider/taller than the viewport; clamp to
	// the top-left so its content stays on-screen instead of spilling off the edge.
	topLeft.setX(std::max(0, topLeft.x()));
	topLeft.setY(std::max(0, topLeft.y()));
	mHelpOverlay->move(topLeft);
}

void MainWindow::adjustScrollBar(QScrollBar *scrollBar, double factor)
{
	scrollBar->setValue(static_cast<int>(factor * scrollBar->value() + ((factor - 1.0) * scrollBar->pageStep() / 2.0)));
}

void MainWindow::applyZoom(double factor, const QPoint *anchor)
{
	if (mImage.isNull()) {
		return;
	}

	const double oldScaleFactor = mScaleFactor;
	const double newScaleFactor = std::max(0.02, std::min(64.0, oldScaleFactor * factor));
	const double actualFactor = newScaleFactor / oldScaleFactor;
	if (actualFactor == 1.0) {
		return;
	}

	mFitToWindow = false;
	mScaleFactor = newScaleFactor;
	if (mFitToWindowAction) {
		mFitToWindowAction->setChecked(false);
	}

	QPointF imagePoint;
	if (anchor) {
		const QPoint labelPoint = mImageView->mapFrom(mScrollArea->viewport(), *anchor);
		imagePoint = QPointF(labelPoint.x() / oldScaleFactor, labelPoint.y() / oldScaleFactor);
	}
	updateView();

	if (anchor) {
		mScrollArea->horizontalScrollBar()->setValue(static_cast<int>(imagePoint.x() * newScaleFactor - anchor->x()));
		mScrollArea->verticalScrollBar()->setValue(static_cast<int>(imagePoint.y() * newScaleFactor - anchor->y()));
	} else {
		adjustScrollBar(mScrollArea->horizontalScrollBar(), actualFactor);
		adjustScrollBar(mScrollArea->verticalScrollBar(), actualFactor);
	}

	broadcastViewState(true);
}

void MainWindow::closeCurrentFile()
{
	resetPlayback();
	showImagePage();
	mImage = QImage();
	mCurrentFile.clear();
	mCurrentFileIsRaw = false;
	mCurrentSourceOnDisk = false;
	mRawFrameSize = 0;
	mRawFrameCount = 0;
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
	clearSelection();
	setWindowTitle(tr("Q1View Qt"));
	updateView();
	refreshControlMenus();
	watchCurrentFile();
}

QImage MainWindow::displayImage() const
{
	if (mImage.isNull()) {
		return QImage();
	}

	QImage image = mYOnly
		? mImage.convertToFormat(QImage::Format_Grayscale8).convertToFormat(QImage::Format_RGB888)
		: mImage;
	const int turns = ((mRotationQuarterTurns % 4) + 4) % 4;
	if (turns != 0) {
		QTransform transform;
		transform.rotate(90 * turns);
		image = image.transformed(transform);
	}
	return image;
}

QStringList MainWindow::imageNameFilters() const
{
	QStringList filters;
	filters
		<< QStringLiteral("*.bmp")
		<< QStringLiteral("*.gif")
		<< QStringLiteral("*.jpeg")
		<< QStringLiteral("*.jpg")
		<< QStringLiteral("*.png")
		<< QStringLiteral("*.ppm")
		<< QStringLiteral("*.pgm")
		<< QStringLiteral("*.pbm")
		<< QStringLiteral("*.xbm")
		<< QStringLiteral("*.xpm")
		<< QStringLiteral("*.bgr888")
		<< QStringLiteral("*.rgb888")
		<< QStringLiteral("*.rgba8888")
		<< QStringLiteral("*.yuv420")
		<< QStringLiteral("*.nv12")
		<< QStringLiteral("*.nv21")
		<< QStringLiteral("*.y4m");

	// Only advertise HEIF-family extensions for navigation when this build can
	// actually decode them; otherwise stepping onto one would just fail.
	if (q1qt::heifSupported()) {
		filters
			<< QStringLiteral("*.heic")
			<< QStringLiteral("*.heif")
			<< QStringLiteral("*.hif")
			<< QStringLiteral("*.avif");
	}

	// Likewise, only step onto video files when this build can play them.
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
	filters << VideoView::nameFilters();
#endif

	return filters;
}

bool MainWindow::loadRawFrame(int frameIndex)
{
	if (!mCurrentFileIsRaw || mRawFrameSize <= 0 || frameIndex < 0 || frameIndex >= mRawFrameCount) {
		return false;
	}

	const qcsc_info *colorSpace = nullptr;
	for (size_t i = 0; i < ARRAY_SIZE(qcsc_info_table); ++i) {
		if (QString::fromLatin1(qcsc_info_table[i].name) == mRawColorSpaceName) {
			colorSpace = &qcsc_info_table[i];
			break;
		}
	}

	if (!colorSpace || !colorSpace->cs_load_info || !colorSpace->csc2rgb888) {
		warn(tr("Open raw image"), tr("Unsupported color space: %1").arg(mRawColorSpaceName));
		return false;
	}

	QFile file(mCurrentFile);
	if (!file.open(QIODevice::ReadOnly)) {
		warn(tr("Open raw image"), tr("Could not open %1").arg(QFileInfo(mCurrentFile).fileName()));
		return false;
	}

	// Raw files pack frames back to back. Y4M prepends a text header and a
	// per-frame "FRAME...\n" marker, so each frame's pixels sit at
	// header + index*(marker + data) + marker.
	const qint64 framePos = mIsY4m
		? mY4mHeaderLen + static_cast<qint64>(frameIndex) * (mY4mFrameMarkerLen + mRawFrameSize) + mY4mFrameMarkerLen
		: static_cast<qint64>(frameIndex) * mRawFrameSize;
	if (!file.seek(framePos)) {
		return false;
	}

	QByteArray raw = file.read(mRawFrameSize);
	if (raw.size() < mRawFrameSize) {
		return false;
	}

	int offset2 = 0;
	int offset3 = 0;
	colorSpace->cs_load_info(mRawWidth, mRawHeight, &offset2, &offset3);

	const int stridePixels = ROUNDUP_DWORD(mRawWidth);
	const int strideBytes = stridePixels * QIMG_DST_RGB_BYTES;
	QByteArray bgr;
	bgr.resize(strideBytes * mRawHeight);

	qu8 *base = reinterpret_cast<qu8 *>(raw.data());
	qu8 *plane2 = offset2 > 0 ? base + offset2 : nullptr;
	qu8 *plane3 = offset3 > 0 ? base + offset3 : nullptr;
	// csc2rgb888 takes the destination stride in *pixels* (it multiplies the
	// row gap by the byte depth itself), so pass stridePixels, not strideBytes.
	// Passing bytes overruns the bgr buffer threefold and corrupts the heap.
	colorSpace->csc2rgb888(reinterpret_cast<qu8 *>(bgr.data()), base, plane2, plane3, stridePixels, mRawWidth, mRawHeight);

	mImage = QImage(
		reinterpret_cast<const uchar *>(bgr.constData()),
		mRawWidth,
		mRawHeight,
		strideBytes,
		QImage::Format_BGR888).copy();
	// Retain the source bytes + sampler so the overlay can read native Y/U/V.
	mRawSource = raw;
	mRawSampler = colorSpace->sample_native_pixel;
	mCurrentFrame = frameIndex;
	updateImage();
	return true;
}

void MainWindow::openAdjacentFile(int direction, bool boundaryOnly)
{
	if (mCurrentFile.isEmpty()) {
		return;
	}

	const QFileInfo currentInfo(mCurrentFile);
	if (!currentInfo.exists() || !currentInfo.isFile()) {
		return;
	}
	QDir dir = currentInfo.dir();
	const QFileInfoList files = dir.entryInfoList(imageNameFilters(), QDir::Files, QDir::Name | QDir::IgnoreCase);
	if (files.isEmpty()) {
		return;
	}

	int currentIndex = -1;
	for (int i = 0; i < files.size(); ++i) {
		if (files[i].absoluteFilePath() == currentInfo.absoluteFilePath()) {
			currentIndex = i;
			break;
		}
	}
	if (currentIndex < 0) {
		return;
	}

	const int nextIndex = boundaryOnly ? (direction < 0 ? 0 : files.size() - 1) : currentIndex + direction;
	if (nextIndex < 0 || nextIndex >= files.size() || nextIndex == currentIndex) {
		return;
	}

	const QString nextFile = files[nextIndex].absoluteFilePath();

	// Folder navigation must not resize the window around each image (issue
	// #69): keep the current frame and let the openers fit the new image into
	// the existing viewport. Restore the flag on every exit path so a later
	// explicit open still sizes the window to its image.
	const bool prevKeep = mKeepWindowOnLoad;
	mKeepWindowOnLoad = true;
	const auto restoreKeep = qScopeGuard([this, prevKeep]() { mKeepWindowOnLoad = prevKeep; });

#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
	if (VideoView::isVideoFile(nextFile)) {
		openFile(nextFile);
		return;
	}
#endif
	QImageReader reader(nextFile);
	// Y4M containers go through openFile() (which sniffs and routes them to the
	// Y4M loader); decoding them as packed raw would fold the header into pixels.
	if (q1y4m::isY4mFile(nextFile) || reader.canRead()
		|| (q1qt::heifSupported() && q1qt::isHeifFile(nextFile))) {
		openFile(nextFile);
		return;
	}

	if (mCurrentFileIsRaw && mRawWidth > 0 && mRawHeight > 0 && !mRawColorSpaceName.isEmpty()) {
		openRawFile(nextFile, mRawWidth, mRawHeight, mRawColorSpaceName);
	} else {
		openDroppedFile(nextFile);
	}
}

void MainWindow::openClipboardImage()
{
	const QImage image = QApplication::clipboard()->image();
	if (image.isNull()) {
		QMessageBox::information(this, tr("Paste"), tr("The clipboard does not contain an image."));
		return;
	}

	resetPlayback();
	showImagePage();
	clearSelection();
	mImage = image;
	mCurrentFile = tr("Clipboard");
	mCurrentFileIsRaw = false;
	mCurrentSourceOnDisk = false;
	mRawFrameCount = 1;
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
	mFitToWindow = true;
	updateImage();
	resizeToImage();
	// A pasted image has no file on disk to watch.
	watchCurrentFile();
}

void MainWindow::fitImageToWindow()
{
	if (mImage.isNull()) {
		return;
	}

	mFitToWindow = true;
	if (mFitToWindowAction) {
		mFitToWindowAction->setChecked(true);
	}
	updateView();
	broadcastViewState(true);
}

void MainWindow::setActualSize()
{
	if (mImage.isNull()) {
		return;
	}

	mFitToWindow = false;
	mScaleFactor = 1.0;
	if (mFitToWindowAction) {
		mFitToWindowAction->setChecked(false);
	}
	updateView();
	broadcastViewState(true);
}

void MainWindow::loadSettings()
{
	// The default raw resolution comes from RawOpenOptions (the MFC viewer's
	// 500x392 display-area footprint); a saved value still takes precedence.
	QSettings settings;
	mRawOptions.width = settings.value(QStringLiteral("raw/width"), mRawOptions.width).toInt();
	mRawOptions.height = settings.value(QStringLiteral("raw/height"), mRawOptions.height).toInt();
	mRawOptions.colorSpaceName = settings.value(QStringLiteral("raw/colorSpace"), mRawOptions.colorSpaceName).toString();
	mRawOptions.fileName = settings.value(QStringLiteral("raw/fileName")).toString();
	mAutoReload = settings.value(QStringLiteral("autoReload"), true).toBool();

	loadRecentFiles();
}

void MainWindow::resetPlayback()
{
	mPlayTimer->stop();
	if (mPlayAction) {
		mPlayAction->setChecked(false);
	}
}

void MainWindow::saveRawSettings() const
{
	QSettings settings;
	settings.setValue(QStringLiteral("raw/width"), mRawOptions.width);
	settings.setValue(QStringLiteral("raw/height"), mRawOptions.height);
	settings.setValue(QStringLiteral("raw/colorSpace"), mRawOptions.colorSpaceName);
	settings.setValue(QStringLiteral("raw/fileName"), mRawOptions.fileName);
}

void MainWindow::saveImageAs()
{
	if (mImage.isNull()) {
		return;
	}

	const QString fileName = QFileDialog::getSaveFileName(
		this,
		tr("Save image as"),
		QFileInfo(mCurrentFile).completeBaseName(),
		tr("PNG image (*.png);;JPEG image (*.jpg *.jpeg);;Bitmap image (*.bmp);;All files (*)"));
	if (fileName.isEmpty()) {
		return;
	}

	if (!selectedImage().save(fileName)) {
		QMessageBox::warning(this, tr("Save image as"), tr("Could not save %1.").arg(QFileInfo(fileName).fileName()));
	}
}

void MainWindow::showHelp()
{
	toggleHelpOverlay();
}

QPoint MainWindow::sourcePointFromDisplayPoint(const QPoint &point) const
{
	if (mImage.isNull()) {
		return QPoint(-1, -1);
	}

	const int turns = ((mRotationQuarterTurns % 4) + 4) % 4;
	switch (turns) {
	case 1:
		return QPoint(point.y(), mImage.height() - point.x() - 1);
	case 2:
		return QPoint(mImage.width() - point.x() - 1, mImage.height() - point.y() - 1);
	case 3:
		return QPoint(mImage.width() - point.y() - 1, point.x());
	default:
		return point;
	}
}

void MainWindow::openDroppedFile(const QString &fileName)
{
	// A dropped video plays on the Multimedia page (openFile routes it there).
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
	if (VideoView::isVideoFile(fileName)) {
		openFile(fileName);
		return;
	}
#endif

	// A dropped .y4m is a YUV4MPEG2 container, not headerless raw; parse it via the
	// Y4M loader instead of prompting with the raw dialog.
	if (q1y4m::isY4mFile(fileName)) {
		openY4mFile(fileName);
		return;
	}

	QImageReader reader(fileName);
	if (reader.canRead() || (q1qt::heifSupported() && q1qt::isHeifFile(fileName))) {
		openFile(fileName);
		return;
	}

	RawOpenOptions options = mRawOptions;
	options.fileName = fileName;
	if (!RawOpenDialog::getOptions(this, &options)) {
		return;
	}

	if (openRawFile(options.fileName, options.width, options.height, options.colorSpaceName)) {
		mRawOptions = options;
		saveRawSettings();
	}
}

void MainWindow::copyImageToClipboard()
{
	if (!mImage.isNull()) {
		const bool hasSelection = mSelectionRect.isValid() && !mSelectionRect.isEmpty();
		QApplication::clipboard()->setImage(selectedImage());
		statusBar()->showMessage(hasSelection
			? tr("Copied selection to clipboard")
			: tr("Copied image to clipboard"), 2000);
	}
}

void MainWindow::firstFrameOrFile()
{
	if (mCurrentFileIsRaw && mRawFrameCount > 1) {
		loadRawFrame(0);
		SyncMessage message;
		message.command = SyncMessage::SeekFrame;
		message.first = mCurrentFrame;
		broadcastSync(message);
	} else {
		openAdjacentFile(-1, true);
		SyncMessage message;
		message.command = SyncMessage::FirstFile;
		broadcastSync(message);
	}
}

void MainWindow::lastFrameOrFile()
{
	if (mCurrentFileIsRaw && mRawFrameCount > 1) {
		loadRawFrame(mRawFrameCount - 1);
		SyncMessage message;
		message.command = SyncMessage::SeekFrame;
		message.first = mCurrentFrame;
		broadcastSync(message);
	} else {
		openAdjacentFile(1, true);
		SyncMessage message;
		message.command = SyncMessage::LastFile;
		broadcastSync(message);
	}
}

void MainWindow::nextFrameOrFile()
{
	// nextFrameOrFile is also the playback-timer slot; while the timer drives it
	// each instance advances on its own clock, so don't mirror per-frame steps.
	const bool fromTimer = mPlayTimer->isActive();
	if (mCurrentFileIsRaw && mRawFrameCount > 1) {
		if (mCurrentFrame >= mRawFrameCount - 1) {
			resetPlayback();
			return;
		}
		loadRawFrame(mCurrentFrame + 1);
		if (!fromTimer) {
			SyncMessage message;
			message.command = SyncMessage::SeekFrame;
			message.first = mCurrentFrame;
			broadcastSync(message);
		}
	} else {
		openAdjacentFile(1);
		SyncMessage message;
		message.command = SyncMessage::NextFile;
		broadcastSync(message);
	}
}

void MainWindow::previousFrameOrFile()
{
	if (mCurrentFileIsRaw && mRawFrameCount > 1) {
		loadRawFrame(std::max(0, mCurrentFrame - 1));
		SyncMessage message;
		message.command = SyncMessage::SeekFrame;
		message.first = mCurrentFrame;
		broadcastSync(message);
	} else {
		openAdjacentFile(-1);
		SyncMessage message;
		message.command = SyncMessage::PreviousFile;
		broadcastSync(message);
	}
}

void MainWindow::rotateClockwise()
{
	if (mImage.isNull()) {
		return;
	}

	mRotationQuarterTurns = (mRotationQuarterTurns + 1) % 4;
	clearSelection();
	mCursorImagePoint = QPoint(-1, -1);
	mFitToWindow = true;
	updateImage();

	SyncMessage message;
	message.command = SyncMessage::Rotate;
	broadcastSync(message);
}

void MainWindow::toggleFullScreen()
{
	if (isFullScreen()) {
		showNormal();
	} else {
		showFullScreen();
	}
}

void MainWindow::togglePlayback()
{
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
	// On the video page, Space / Play-Stop drive the media player through the
	// same path as the transport button; its playbackToggled() signal mirrors
	// the play/pause *state* to sibling windows (video position is not synced).
	if (mShowingVideo && mVideoView) {
		mVideoView->togglePlay();
		return;
	}
#endif

	if (!mCurrentFileIsRaw || mRawFrameCount <= 1) {
		resetPlayback();
		return;
	}

	if (mPlayTimer->isActive()) {
		resetPlayback();
		SyncMessage stop;
		stop.command = SyncMessage::Playback;
		stop.first = 0;
		broadcastSync(stop);
		return;
	}

	if (mCurrentFrame >= mRawFrameCount - 1) {
		loadRawFrame(0);
	}
	if (mPlayAction) {
		mPlayAction->setChecked(true);
	}
	mPlayTimer->start();

	SyncMessage play;
	play.command = SyncMessage::Playback;
	play.first = 1;
	broadcastSync(play);
}

void MainWindow::toggleShowCoordinates()
{
	mShowCoordinates = !mShowCoordinates;
	if (mCoordinatesAction) {
		mCoordinatesAction->setChecked(mShowCoordinates);
	}
	updateZoomStatus();

	SyncMessage message;
	message.command = SyncMessage::DisplayOptions;
	message.first = static_cast<qint32>(displayOptionBits());
	broadcastSync(message);
}

void MainWindow::toggleYOnly()
{
	mYOnly = !mYOnly;
	if (mYOnlyAction) {
		mYOnlyAction->setChecked(mYOnly);
	}
	updateView();

	SyncMessage message;
	message.command = SyncMessage::DisplayOptions;
	message.first = static_cast<qint32>(displayOptionBits());
	broadcastSync(message);
}

void MainWindow::toggleHexValues()
{
	mHexMode = !mHexMode;
	if (mHexValuesAction) {
		mHexValuesAction->setChecked(mHexMode);
	}
	updateView();

	SyncMessage message;
	message.command = SyncMessage::DisplayOptions;
	message.first = static_cast<qint32>(displayOptionBits());
	broadcastSync(message);
}

void MainWindow::toggleSourceValues()
{
	mShowSourceValues = !mShowSourceValues;
	if (mSourceYuvAction) {
		mSourceYuvAction->setChecked(mShowSourceValues);
	}
	updateView();

	SyncMessage message;
	message.command = SyncMessage::DisplayOptions;
	message.first = static_cast<qint32>(displayOptionBits());
	broadcastSync(message);
}

bool MainWindow::nativeSampleAtDisplay(int displayX, int displayY,
	QIMAGE_NATIVE_PIXEL_SAMPLE *sample) const
{
	if (!mCurrentFileIsRaw || !mRawSampler || mRawSource.isEmpty()
		|| mRawWidth <= 0 || mRawHeight <= 0) {
		return false;
	}

	// The displayed image is the source rotated clockwise by `turns` quarter
	// turns; map the display pixel back to source coordinates, mirroring the MFC
	// viewer's CViewerDoc::GetNativePixelSample. The sampler always works on the
	// unrotated source layout (mRawWidth x mRawHeight).
	const int srcW = mRawWidth;
	const int srcH = mRawHeight;
	const int turns = ((mRotationQuarterTurns % 4) + 4) % 4;
	int sx = displayX;
	int sy = displayY;
	switch (turns) {
	case 1: sx = displayY;             sy = srcH - 1 - displayX; break;
	case 2: sx = srcW - 1 - displayX;  sy = srcH - 1 - displayY; break;
	case 3: sx = srcW - 1 - displayY;  sy = displayX;            break;
	}
	if (sx < 0 || sy < 0 || sx >= srcW || sy >= srcH) {
		return false;
	}

	return mRawSampler(reinterpret_cast<const qu8 *>(mRawSource.constData()),
		srcW, srcH, sx, sy, sample) != 0;
}

void MainWindow::toggleInterpolate()
{
	mInterpolate = !mInterpolate;
	if (mInterpolateAction) {
		mInterpolateAction->setChecked(mInterpolate);
	}
	updateView();
	statusBar()->showMessage(
		mInterpolate ? tr("Interpolation on") : tr("Interpolation off"), 1500);

	SyncMessage message;
	message.command = SyncMessage::DisplayOptions;
	message.first = static_cast<qint32>(displayOptionBits());
	broadcastSync(message);
}

void MainWindow::showAbout()
{
	QMessageBox::about(this, tr("About Q1View Qt"),
		tr("<b>Q1View Qt</b><br>Cross-platform image / raw-frame viewer.<br>"
		   "A Qt port of the Q1View MFC viewer."));
}

void MainWindow::resizeToImage()
{
	const QImage shown = displayImage();
	if (shown.isNull() || isFullScreen() || isMaximized()) {
		return;
	}

	// Folder navigation keeps the current window size and only refits the new
	// image into the existing viewport, so the frame no longer jumps around
	// when stepping through differently sized images (issue #69). A fresh open
	// still sizes the window to its image below.
	if (!mKeepWindowOnLoad) {
		// Grow the window so the viewport shows the image near 1:1, but never smaller
		// than the default footprint nor larger than the available screen. Mirrors the
		// MFC viewer, which sizes its frame to max(default, image) around the image.
		QRect available(0, 0, 1920, 1080);
		if (QScreen *scr = screen()) {
			available = scr->availableGeometry();
		}

		// sizeHint() is valid even before the window is shown (unlike height()).
		const int chromeH = menuBar()->sizeHint().height() + statusBar()->sizeHint().height();
		// frameGeometry() includes the OS title bar / borders; the difference from the
		// widget size is the chrome the screen budget must also leave room for.
		const int frameExtraW = std::max(0, frameGeometry().width() - width());
		const int frameExtraH = std::max(0, frameGeometry().height() - height());
		const int maxContentW = std::max(500, available.width() - frameExtraW);
		const int maxContentH = std::max(412, available.height() - frameExtraH);

		const int contentW = std::clamp(shown.width(), 500, maxContentW);
		const int contentH = std::clamp(shown.height() + chromeH, 412, maxContentH);
		resize(contentW, contentH);
	}

	// Refit once the window has actually been laid out. A queued call runs after
	// show()/the resize is applied, when the viewport finally reports its real
	// size; the synchronous value is stale when a file is opened from the command
	// line before show() (the viewport still holds Qt's default size).
	if (mFitToWindow) {
		QTimer::singleShot(0, this, [this]() {
			if (mFitToWindow && !mImage.isNull()) {
				updateView();
			}
		});
	}
}

void MainWindow::loadRecentFiles()
{
	QSettings settings;
	mRecentFiles = settings.value(QStringLiteral("recentFiles")).toStringList();
}

void MainWindow::saveRecentFiles() const
{
	QSettings settings;
	settings.setValue(QStringLiteral("recentFiles"), mRecentFiles);
}

void MainWindow::addToRecentFiles(const QString &fileName)
{
	if (fileName.isEmpty()) {
		return;
	}
	const QString path = QFileInfo(fileName).absoluteFilePath();
	mRecentFiles.removeAll(path);
	mRecentFiles.prepend(path);
	const int kMaxRecent = 10;
	while (mRecentFiles.size() > kMaxRecent) {
		mRecentFiles.removeLast();
	}
	saveRecentFiles();
	updateRecentFilesMenu();
}

void MainWindow::updateRecentFilesMenu()
{
	if (!mRecentMenu) {
		return;
	}
	mRecentMenu->clear();

	if (mRecentFiles.isEmpty()) {
		QAction *empty = mRecentMenu->addAction(tr("(No recent files)"));
		empty->setEnabled(false);
		return;
	}

	int index = 1;
	for (const QString &path : mRecentFiles) {
		// "&1 name" gives Alt-mnemonics for the first nine entries, like MFC's MRU.
		QAction *action = mRecentMenu->addAction(
			tr("&%1  %2").arg(index++).arg(QFileInfo(path).fileName()));
		action->setData(path);
		action->setToolTip(path);
		connect(action, &QAction::triggered, this, [this, path]() { openFile(path); });
	}
	mRecentMenu->addSeparator();
	QAction *clear = mRecentMenu->addAction(tr("&Clear Recent Files"));
	connect(clear, &QAction::triggered, this, [this]() {
		mRecentFiles.clear();
		saveRecentFiles();
		updateRecentFilesMenu();
	});
}

void MainWindow::updateImage()
{
	updateView();

	const QFileInfo info(mCurrentFile);
	setWindowTitle(tr("%1 - Q1View Qt").arg(info.fileName()));
	refreshControlMenus();
	updateZoomStatus();
}

void MainWindow::updateView()
{
	const QImage shownImage = displayImage();
	if (shownImage.isNull()) {
		mImageView->setImage(QImage(), 1.0);
		updateZoomStatus();
		return;
	}

	if (mFitToWindow) {
		const QSize viewportSize = mScrollArea->viewport()->size();
		const double widthScale = static_cast<double>(viewportSize.width()) / static_cast<double>(shownImage.width());
		const double heightScale = static_cast<double>(viewportSize.height()) / static_cast<double>(shownImage.height());
		// Shrink large images to fit, but never upscale past 1:1: small images
		// stay crisp at 100% rather than being blurrily enlarged to fill the
		// window. Matches the MFC viewer's fit and common inspection-oriented
		// viewers, and keeps folder navigation steady (issue #69).
		mScaleFactor = std::clamp(std::min(widthScale, heightScale), 0.02, 1.0);
	}

	mImageView->setYOnly(mYOnly);
	mImageView->setHexMode(mHexMode);
	mImageView->setShowSourceValues(mShowSourceValues);
	mImageView->setInterpolate(mInterpolate);
	mImageView->setSelection(mSelectionRect);
	mImageView->setImage(shownImage, mScaleFactor);
	updateZoomStatus();
}

void MainWindow::toggleSelectionMode()
{
	mSelectionMode = !mSelectionMode;
	if (mSelectModeAction) {
		mSelectModeAction->setChecked(mSelectionMode);
	}

	if (mSelectionMode) {
		mScrollArea->viewport()->setCursor(Qt::CrossCursor);
		statusBar()->showMessage(
			tr("Selection mode on: drag to select; drag an edge/corner to resize, "
			   "inside to move; Copy/Save As then act on it"), 5000);
	} else {
		mIsSelecting = false;
		mActiveHandle = SelHandle::None;
		mScrollArea->viewport()->unsetCursor();
		statusBar()->showMessage(tr("Selection mode off"), 2000);
	}
}

void MainWindow::clearSelection()
{
	mIsSelecting = false;
	mActiveHandle = SelHandle::None;
	mSelectionRect = QRect();
	mImageView->setSelection(QRect());
}

QImage MainWindow::selectedImage() const
{
	const QImage shown = displayImage();
	if (shown.isNull() || !mSelectionRect.isValid() || mSelectionRect.isEmpty()) {
		return shown;
	}

	const QRect r = mSelectionRect.intersected(QRect(QPoint(0, 0), shown.size()));
	if (r.isEmpty()) {
		return shown;
	}
	return shown.copy(r);
}

QRect MainWindow::imageRectFromViewport(const QPoint &a, const QPoint &b) const
{
	const QImage shown = displayImage();
	if (shown.isNull() || mScaleFactor <= 0.0) {
		return QRect();
	}

	QWidget *viewport = mScrollArea->viewport();
	const QPoint la = mImageView->mapFrom(viewport, a);
	const QPoint lb = mImageView->mapFrom(viewport, b);

	const int ax = static_cast<int>(la.x() / mScaleFactor);
	const int ay = static_cast<int>(la.y() / mScaleFactor);
	const int bx = static_cast<int>(lb.x() / mScaleFactor);
	const int by = static_cast<int>(lb.y() / mScaleFactor);

	const int x0 = std::clamp(std::min(ax, bx), 0, shown.width());
	const int y0 = std::clamp(std::min(ay, by), 0, shown.height());
	const int x1 = std::clamp(std::max(ax, bx), 0, shown.width());
	const int y1 = std::clamp(std::max(ay, by), 0, shown.height());
	return QRect(x0, y0, x1 - x0, y1 - y0);
}

QRect MainWindow::selectionViewportRect() const
{
	if (!mSelectionRect.isValid() || mSelectionRect.isEmpty() || mScaleFactor <= 0.0) {
		return QRect();
	}

	// The selection is stored in display-image pixels; the on-screen rectangle
	// is that scaled by the zoom factor and offset by the scroll position.
	const int x = static_cast<int>(mSelectionRect.x() * mScaleFactor);
	const int y = static_cast<int>(mSelectionRect.y() * mScaleFactor);
	const int w = static_cast<int>(mSelectionRect.width() * mScaleFactor);
	const int h = static_cast<int>(mSelectionRect.height() * mScaleFactor);
	const QPoint topLeft = mImageView->mapTo(mScrollArea->viewport(), QPoint(x, y));
	return QRect(topLeft, QSize(w, h));
}

MainWindow::SelHandle MainWindow::selectionHandleAt(const QPoint &viewportPos) const
{
	const QRect r = selectionViewportRect();
	if (r.isNull() || r.isEmpty()) {
		return SelHandle::None;
	}

	// Grab tolerance, in viewport pixels, around each edge and corner.
	const int margin = 6;
	if (!r.adjusted(-margin, -margin, margin, margin).contains(viewportPos)) {
		return SelHandle::None;
	}

	const bool nearLeft = qAbs(viewportPos.x() - r.left()) <= margin;
	const bool nearRight = qAbs(viewportPos.x() - r.right()) <= margin;
	const bool nearTop = qAbs(viewportPos.y() - r.top()) <= margin;
	const bool nearBottom = qAbs(viewportPos.y() - r.bottom()) <= margin;

	if (nearTop && nearLeft) return SelHandle::TopLeft;
	if (nearTop && nearRight) return SelHandle::TopRight;
	if (nearBottom && nearLeft) return SelHandle::BottomLeft;
	if (nearBottom && nearRight) return SelHandle::BottomRight;
	if (nearLeft) return SelHandle::Left;
	if (nearRight) return SelHandle::Right;
	if (nearTop) return SelHandle::Top;
	if (nearBottom) return SelHandle::Bottom;
	return SelHandle::Move;
}

QPoint MainWindow::imagePointFromViewport(const QPoint &viewportPos) const
{
	if (mScaleFactor <= 0.0) {
		return QPoint();
	}
	const QPoint local = mImageView->mapFrom(mScrollArea->viewport(), viewportPos);
	return QPoint(static_cast<int>(std::floor(local.x() / mScaleFactor)),
		static_cast<int>(std::floor(local.y() / mScaleFactor)));
}

QRect MainWindow::resizedSelection(SelHandle handle, const QPoint &imageDelta) const
{
	const QImage shown = displayImage();
	if (shown.isNull()) {
		return mDragStartRect;
	}

	// Half-open [x0, x1) x [y0, y1) image coordinates, so dragging an edge past
	// its opposite simply flips the rectangle (re-normalized at the end).
	int x0 = mDragStartRect.left();
	int y0 = mDragStartRect.top();
	int x1 = mDragStartRect.left() + mDragStartRect.width();
	int y1 = mDragStartRect.top() + mDragStartRect.height();
	const int dx = imageDelta.x();
	const int dy = imageDelta.y();

	switch (handle) {
	case SelHandle::Move:
		// Slide the rectangle but keep it whole and inside the image bounds.
		x0 += dx; x1 += dx; y0 += dy; y1 += dy;
		if (x0 < 0) { x1 -= x0; x0 = 0; }
		if (y0 < 0) { y1 -= y0; y0 = 0; }
		if (x1 > shown.width()) { x0 -= (x1 - shown.width()); x1 = shown.width(); }
		if (y1 > shown.height()) { y0 -= (y1 - shown.height()); y1 = shown.height(); }
		x0 = std::max(0, x0);
		y0 = std::max(0, y0);
		break;
	case SelHandle::Left:        x0 += dx; break;
	case SelHandle::Right:       x1 += dx; break;
	case SelHandle::Top:         y0 += dy; break;
	case SelHandle::Bottom:      y1 += dy; break;
	case SelHandle::TopLeft:     x0 += dx; y0 += dy; break;
	case SelHandle::TopRight:    x1 += dx; y0 += dy; break;
	case SelHandle::BottomLeft:  x0 += dx; y1 += dy; break;
	case SelHandle::BottomRight: x1 += dx; y1 += dy; break;
	case SelHandle::None:        return mDragStartRect;
	}

	x0 = std::clamp(x0, 0, shown.width());
	x1 = std::clamp(x1, 0, shown.width());
	y0 = std::clamp(y0, 0, shown.height());
	y1 = std::clamp(y1, 0, shown.height());

	const int xmin = std::min(x0, x1);
	const int ymin = std::min(y0, y1);
	return QRect(xmin, ymin, std::max(x0, x1) - xmin, std::max(y0, y1) - ymin);
}

void MainWindow::updateSelectionCursor(const QPoint &viewportPos)
{
	if (!mSelectionMode) {
		return;
	}

	Qt::CursorShape shape = Qt::CrossCursor;
	switch (selectionHandleAt(viewportPos)) {
	case SelHandle::Left:
	case SelHandle::Right:       shape = Qt::SizeHorCursor; break;
	case SelHandle::Top:
	case SelHandle::Bottom:      shape = Qt::SizeVerCursor; break;
	case SelHandle::TopLeft:
	case SelHandle::BottomRight: shape = Qt::SizeFDiagCursor; break;
	case SelHandle::TopRight:
	case SelHandle::BottomLeft:  shape = Qt::SizeBDiagCursor; break;
	case SelHandle::Move:        shape = Qt::SizeAllCursor; break;
	case SelHandle::None:        shape = Qt::CrossCursor; break;
	}
	mScrollArea->viewport()->setCursor(shape);
}

void MainWindow::updateZoomStatus()
{
	const bool hasImage = !mImage.isNull();
	updateMagnifyLabel();
	if (mSaveAsAction) {
		mSaveAsAction->setEnabled(hasImage);
	}
	if (mCopyAction) {
		mCopyAction->setEnabled(hasImage);
	}
	if (mZoomInAction) {
		mZoomInAction->setEnabled(hasImage);
	}
	if (mZoomOutAction) {
		mZoomOutAction->setEnabled(hasImage);
	}
	if (mActualSizeAction) {
		mActualSizeAction->setEnabled(hasImage);
	}
	if (mFitToWindowAction) {
		mFitToWindowAction->setEnabled(hasImage);
		mFitToWindowAction->setChecked(mFitToWindow);
	}
	if (mRotateAction) {
		mRotateAction->setEnabled(hasImage);
	}
	if (mYOnlyAction) {
		mYOnlyAction->setEnabled(hasImage);
		mYOnlyAction->setChecked(mYOnly);
	}
	if (mHexValuesAction) {
		mHexValuesAction->setEnabled(hasImage);
		mHexValuesAction->setChecked(mHexMode);
	}
	if (mSourceYuvAction) {
		mSourceYuvAction->setEnabled(hasImage);
		mSourceYuvAction->setChecked(mShowSourceValues);
	}
	if (mCoordinatesAction) {
		mCoordinatesAction->setEnabled(hasImage);
		mCoordinatesAction->setChecked(mShowCoordinates);
	}
	if (mSelectModeAction) {
		mSelectModeAction->setEnabled(hasImage);
		mSelectModeAction->setChecked(mSelectionMode);
	}
	if (mPlayAction) {
		mPlayAction->setEnabled(mCurrentFileIsRaw && mRawFrameCount > 1);
		mPlayAction->setChecked(mPlayTimer->isActive());
	}

	if (!hasImage) {
		statusBar()->showMessage(tr("Ready"));
		return;
	}

	QString message = tr("%1 x %2   %3%")
		.arg(mImage.width())
		.arg(mImage.height())
		.arg(static_cast<int>(mScaleFactor * 100.0 + 0.5));
	if (mCurrentFileIsRaw && mRawFrameCount > 1) {
		message += tr("   frame %1/%2").arg(mCurrentFrame + 1).arg(mRawFrameCount);
	}
	if (mShowCoordinates
		&& mCursorImagePoint.x() >= 0
		&& mCursorImagePoint.y() >= 0
		&& mCursorImagePoint.x() < mImage.width()
		&& mCursorImagePoint.y() < mImage.height()) {
		const QColor color = mImage.pixelColor(mCursorImagePoint);
		message += tr("   x:%1 y:%2   RGB:%3,%4,%5")
			.arg(mCursorImagePoint.x())
			.arg(mCursorImagePoint.y())
			.arg(color.red())
			.arg(color.green())
			.arg(color.blue());
	}
	statusBar()->showMessage(message);
}

void MainWindow::zoomIn()
{
	applyZoom(1.25);
}

void MainWindow::zoomOut()
{
	applyZoom(0.8);
}

bool MainWindow::openRawFile(const QString &fileName, int width, int height, const QString &colorSpaceName)
{
	const qcsc_info *colorSpace = nullptr;
	for (size_t i = 0; i < ARRAY_SIZE(qcsc_info_table); ++i) {
		if (QString::fromLatin1(qcsc_info_table[i].name) == colorSpaceName) {
			colorSpace = &qcsc_info_table[i];
			break;
		}
	}

	if (!colorSpace || !colorSpace->cs_load_info || !colorSpace->csc2rgb888) {
		warn(tr("Open raw image"), tr("Unsupported color space: %1").arg(colorSpaceName));
		return false;
	}

	int offset2 = 0;
	int offset3 = 0;
	const int frameSize = colorSpace->cs_load_info(width, height, &offset2, &offset3);
	if (frameSize <= 0) {
		warn(tr("Open raw image"), tr("Invalid raw image size."));
		return false;
	}

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		warn(tr("Open raw image"), tr("Could not open %1").arg(QFileInfo(fileName).fileName()));
		return false;
	}

	const qint64 fileSize = file.size();
	if (fileSize < frameSize) {
		warn(tr("Open raw image"),
			tr("File is too small for %1 %2x%3.\nExpected at least %4 bytes, got %5 bytes.")
				.arg(colorSpaceName)
				.arg(width)
				.arg(height)
				.arg(frameSize)
				.arg(fileSize));
		return false;
	}

	resetPlayback();
	showImagePage();
	mCurrentFile = fileName;
	mCurrentFileIsRaw = true;
	mCurrentSourceOnDisk = true;
	mIsY4m = false;
	mRawColorSpaceName = colorSpaceName;
	mRawFrameSize = frameSize;
	mRawWidth = width;
	mRawHeight = height;
	mRawFrameCount = static_cast<int>(fileSize / frameSize);
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
	clearSelection();
	mFitToWindow = true;
	const bool ok = loadRawFrame(0);
	if (ok) {
		resizeToImage();
		watchCurrentFile();
	}
	return ok;
}

bool MainWindow::openY4mFile(const QString &fileName)
{
	q1y4m::Y4mInfo info;
	if (!q1y4m::parseY4m(fileName, &info)) {
		warn(tr("Open Y4M"),
			tr("%1 is not a supported YUV4MPEG2 file.").arg(QFileInfo(fileName).fileName()));
		return false;
	}

	resetPlayback();
	showImagePage();
	mCurrentFile = fileName;
	mCurrentFileIsRaw = true;
	mCurrentSourceOnDisk = true;
	mIsY4m = true;
	mY4mHeaderLen = info.headerLength;
	mY4mFrameMarkerLen = info.frameMarkerLength;
	mRawColorSpaceName = info.colorSpaceName;
	mRawFrameSize = info.frameDataSize;
	mRawWidth = info.width;
	mRawHeight = info.height;
	mRawFrameCount = info.frameCount;
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
	// Honour the Y4M header frame rate (F<num>:<den>); the FPS menu/readout and the
	// playback timer follow it. A missing/invalid rate keeps the current FPS.
	if (info.fps > 0.0) {
		mFps = info.fps;
		mPlayTimer->setInterval(static_cast<int>(1000.0 / mFps + 0.5));
	}
	clearSelection();
	mFitToWindow = true;
	const bool ok = loadRawFrame(0);
	if (ok) {
		resizeToImage();
		addToRecentFiles(mCurrentFile);
		watchCurrentFile();
	}
	return ok;
}

bool MainWindow::openVideoFile(const QString &fileName)
{
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
	if (!mVideoView) {
		mVideoView = new VideoView;
		mCentralStack->addWidget(mVideoView);
		// Media loads asynchronously, so the success/failure of a clip is only
		// known later: add it to the recent list once it really loads, and on an
		// error surface the message and don't leave a dud in the recents.
		connect(mVideoView, &VideoView::loaded, this, [this]() {
			if (mShowingVideo) {
				addToRecentFiles(mCurrentFile);
			}
		});
		// The transport bar's own play/pause button toggles the player directly;
		// mirror that press to sibling windows too (Space/menu broadcast via
		// togglePlayback()).
		connect(mVideoView, &VideoView::playbackToggled, this, [this](bool playing) {
			SyncMessage message;
			message.command = SyncMessage::Playback;
			message.first = playing ? 1 : 0;
			broadcastSync(message);
		});
		connect(mVideoView, &VideoView::errorOccurred, this, [this](const QString &message) {
			if (!mShowingVideo) {
				return;
			}
			const QString shown = QFileInfo(mCurrentFile).fileName();
			QMessageBox::warning(this, tr("Open video"),
				tr("Could not play %1:\n%2").arg(shown, message));
			// A hard decode/resource error means the clip didn't really open
			// (it was never added to recents). Reset to a clean empty view rather
			// than leaving the dud as the current/watched source on a blank page.
			closeCurrentFile();
		});
	}

	resetPlayback();
	// Clearing the image state disables the image-only actions while the video
	// page is showing, and lets refreshControlMenus()/updateZoomStatus() treat
	// "no still image" correctly.
	mImage = QImage();
	mCurrentFile = fileName;
	mCurrentFileIsRaw = false;
	mCurrentSourceOnDisk = true;
	mIsY4m = false;
	mRawFrameSize = 0;
	mRawFrameCount = 0;
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
	clearSelection();

	mShowingVideo = true;
	mCentralStack->setCurrentWidget(mVideoView);
	// The synchronous return is best-effort; real decode errors arrive later via
	// errorOccurred, and recent-files is gated on the async loaded signal above.
	const bool ok = mVideoView->open(fileName);

	setWindowTitle(tr("%1 - Q1View Qt").arg(QFileInfo(fileName).fileName()));
	watchCurrentFile();
	refreshControlMenus();
	updateZoomStatus();
	return ok;
#else
	Q_UNUSED(fileName);
	return false;
#endif
}

void MainWindow::showImagePage()
{
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
	if (mShowingVideo && mVideoView) {
		mVideoView->stop();
	}
#endif
	mShowingVideo = false;
	mCentralStack->setCurrentWidget(mScrollArea);
}

void MainWindow::watchCurrentFile()
{
	if (!mFileWatcher) {
		return;
	}

	// Drop the previous watch first; QFileSystemWatcher keeps paths until removed.
	const QStringList watchedFiles = mFileWatcher->files();
	if (!watchedFiles.isEmpty()) {
		mFileWatcher->removePaths(watchedFiles);
	}
	const QStringList watchedDirs = mFileWatcher->directories();
	if (!watchedDirs.isEmpty()) {
		mFileWatcher->removePaths(watchedDirs);
	}

	mWatchedSize = -1;
	mWatchedMtimeMs = -1;
	// Skip non-file sources (the synthetic "Clipboard") so we never watch the
	// working directory by accident.
	if (!mAutoReload || !mCurrentSourceOnDisk || mCurrentFile.isEmpty()) {
		return;
	}

	const QFileInfo info(mCurrentFile);
	// Always watch the directory when it exists, even if the file is momentarily
	// gone mid-save: an atomic "write temp + rename" briefly deletes the target,
	// and the directory watch is what catches it reappearing (the Windows
	// FileChangeNotiThread likewise watches the containing directory handle).
	const QString dir = info.absolutePath();
	if (!dir.isEmpty() && QFileInfo::exists(dir)) {
		mFileWatcher->addPath(dir);
	}

	if (info.exists() && info.isFile()) {
		mFileWatcher->addPath(info.absoluteFilePath());
		mWatchedSize = info.size();
		mWatchedMtimeMs = info.lastModified().toMSecsSinceEpoch();
	}
}

void MainWindow::onWatchedPathChanged(const QString &path)
{
	Q_UNUSED(path);
	if (!mAutoReload || mCurrentFile.isEmpty()) {
		return;
	}

	const QFileInfo info(mCurrentFile);
	if (!info.exists() || !info.isFile()) {
		// Vanished mid-save (atomic replace); let reloadCurrentSource() re-arm.
		mReloadTimer->start();
		return;
	}

	// Ignore directoryChanged noise from unrelated siblings: only react when our
	// file's own bytes actually changed.
	const qint64 size = info.size();
	const qint64 mtime = info.lastModified().toMSecsSinceEpoch();
	if (size == mWatchedSize && mtime == mWatchedMtimeMs) {
		return;
	}
	mWatchedSize = size;
	mWatchedMtimeMs = mtime;

	// Coalesce the burst of signals a single save produces; reloadCurrentSource()
	// re-arms the watch afterwards (the file may have been replaced).
	mReloadTimer->start();
}

void MainWindow::reloadCurrentSource()
{
	if (mCurrentFile.isEmpty()) {
		return;
	}
	const QFileInfo info(mCurrentFile);
	if (!info.exists() || !info.isFile()) {
		// Missing mid-save (atomic replace); stay armed for the recreated file.
		watchCurrentFile();
		return;
	}

#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
	if (mShowingVideo) {
		if (mVideoView) {
			mVideoView->open(mCurrentFile);
		}
		watchCurrentFile();
		return;
	}
#endif

	// Preserve the on-screen view so an in-place edit does not jump the image.
	const double savedScale = mScaleFactor;
	const bool savedFit = mFitToWindow;
	const int savedH = mScrollArea->horizontalScrollBar()->value();
	const int savedV = mScrollArea->verticalScrollBar()->value();
	const QRect savedSelection = mSelectionRect;
	const int savedRotation = mRotationQuarterTurns;
	const int savedFrame = mCurrentFrame;

	bool ok = false;
	if (mCurrentFileIsRaw && mIsY4m) {
		q1y4m::Y4mInfo y4m;
		if (q1y4m::parseY4m(mCurrentFile, &y4m)) {
			mY4mHeaderLen = y4m.headerLength;
			mY4mFrameMarkerLen = y4m.frameMarkerLength;
			mRawColorSpaceName = y4m.colorSpaceName;
			mRawFrameSize = y4m.frameDataSize;
			mRawWidth = y4m.width;
			mRawHeight = y4m.height;
			mRawFrameCount = y4m.frameCount;
			ok = loadRawFrame(std::clamp(savedFrame, 0, std::max(0, mRawFrameCount - 1)));
		}
	} else if (mCurrentFileIsRaw) {
		const qint64 fileSize = info.size();
		if (mRawFrameSize > 0 && fileSize >= mRawFrameSize) {
			mRawFrameCount = static_cast<int>(fileSize / mRawFrameSize);
			ok = loadRawFrame(std::clamp(savedFrame, 0, std::max(0, mRawFrameCount - 1)));
		}
	} else {
		QImageReader reader(mCurrentFile);
		reader.setAutoTransform(true);
		QImage image = reader.read();
		if (image.isNull() && q1qt::isHeifFile(mCurrentFile)) {
			image = q1qt::readHeif(mCurrentFile);
		}
		if (!image.isNull()) {
			mImage = image;
			ok = true;
		}
	}

	if (!ok) {
		// A partial write can momentarily fail to decode; keep the current image
		// and stay armed for the next change.
		watchCurrentFile();
		return;
	}

	// Restore the saved view over whatever the loaders reset.
	mRotationQuarterTurns = savedRotation;
	mSelectionRect = savedSelection;
	mFitToWindow = savedFit;
	if (!savedFit) {
		mScaleFactor = savedScale;
	}
	updateView();
	if (!savedFit) {
		mScrollArea->horizontalScrollBar()->setValue(savedH);
		mScrollArea->verticalScrollBar()->setValue(savedV);
	}
	refreshControlMenus();
	updateZoomStatus();
	statusBar()->showMessage(tr("Reloaded %1").arg(info.fileName()), 1500);
	watchCurrentFile();
}

void MainWindow::toggleAutoReload()
{
	mAutoReload = !mAutoReload;
	if (mAutoReloadAction) {
		mAutoReloadAction->setChecked(mAutoReload);
	}
	QSettings settings;
	settings.setValue(QStringLiteral("autoReload"), mAutoReload);

	if (mAutoReload) {
		watchCurrentFile();
		statusBar()->showMessage(tr("Auto-reload on change: on"), 1500);
	} else {
		mReloadTimer->stop();
		watchCurrentFile(); // with mAutoReload false this clears the watch
		statusBar()->showMessage(tr("Auto-reload on change: off"), 1500);
	}
}

void MainWindow::toggleSyncInput()
{
	const bool enable = !mSyncChannel->isEnabled();
	if (enable && !mSyncChannel->setEnabled(true)) {
		QMessageBox::warning(this, tr("Sync Input"),
			tr("Could not set up the cross-window sync channel."));
		if (mSyncInputAction) {
			mSyncInputAction->setChecked(false);
		}
		return;
	}
	if (!enable) {
		mSyncChannel->setEnabled(false);
	}
	if (mSyncInputAction) {
		mSyncInputAction->setChecked(mSyncChannel->isEnabled());
	}
	statusBar()->showMessage(mSyncChannel->isEnabled()
		? tr("Sync Input: on (mirroring to other Q1View Qt windows)")
		: tr("Sync Input: off"), 2000);
}

void MainWindow::broadcastSync(const SyncMessage &message)
{
	if (mApplyingSync || !mSyncChannel->isEnabled()) {
		return;
	}
	mSyncChannel->send(message);
}

void MainWindow::broadcastViewState(bool force)
{
	if (mApplyingSync || !mSyncChannel->isEnabled() || mImage.isNull()) {
		return;
	}
	// Throttle continuous pan/zoom so a drag doesn't flood the channel, matching
	// the MFC viewer's ~16 ms guard on VIEWER_SYNC_VIEW_STATE.
	const qint64 now = QDateTime::currentMSecsSinceEpoch();
	if (!force && now - mLastViewStateBroadcastMs < 30) {
		return;
	}
	mLastViewStateBroadcastMs = now;

	SyncMessage message;
	message.command = SyncMessage::ViewState;
	message.scalar = mScaleFactor;
	message.x = mScrollArea->horizontalScrollBar()->value();
	message.y = mScrollArea->verticalScrollBar()->value();
	mSyncChannel->send(message);
}

quint32 MainWindow::displayOptionBits() const
{
	quint32 bits = 0;
	if (mYOnly) {
		bits |= SyncMessage::YOnly;
	}
	if (mInterpolate) {
		bits |= SyncMessage::Interpolate;
	}
	if (mShowCoordinates) {
		bits |= SyncMessage::Coordinates;
	}
	if (mHexMode) {
		bits |= SyncMessage::HexPixel;
	}
	if (mShowSourceValues) {
		bits |= SyncMessage::SourceYuv;
	}
	return bits;
}

void MainWindow::applySyncMessage(const SyncMessage &message)
{
	if (!mSyncChannel->isEnabled()) {
		return;
	}

	// Re-entrancy guard: the local actions invoked below would otherwise call
	// broadcastSync() again and ping-pong between instances.
	mApplyingSync = true;
	switch (message.command) {
	case SyncMessage::SeekFrame:
		if (mCurrentFileIsRaw && mRawFrameCount > 1) {
			loadRawFrame(std::clamp(message.first, 0, mRawFrameCount - 1));
		}
		break;
	case SyncMessage::FirstFile:
		firstFrameOrFile();
		break;
	case SyncMessage::LastFile:
		lastFrameOrFile();
		break;
	case SyncMessage::PreviousFile:
		previousFrameOrFile();
		break;
	case SyncMessage::NextFile:
		nextFrameOrFile();
		break;
	case SyncMessage::ViewState:
		if (!mImage.isNull()) {
			mFitToWindow = false;
			if (mFitToWindowAction) {
				mFitToWindowAction->setChecked(false);
			}
			mScaleFactor = std::max(0.02, std::min(64.0, message.scalar));
			updateView();
			mScrollArea->horizontalScrollBar()->setValue(static_cast<int>(message.x));
			mScrollArea->verticalScrollBar()->setValue(static_cast<int>(message.y));
			updateZoomStatus();
		}
		break;
	case SyncMessage::Rotate:
		rotateClockwise();
		break;
	case SyncMessage::Playback:
#ifdef Q1VIEW_ENABLE_QT_MULTIMEDIA
		if (mShowingVideo && mVideoView) {
			if ((message.first != 0) != mVideoView->isPlaying()) {
				if (message.first != 0) {
					mVideoView->play();
				} else {
					mVideoView->pause();
				}
			}
			break;
		}
#endif
		if ((message.first != 0) != mPlayTimer->isActive()) {
			togglePlayback();
		}
		break;
	case SyncMessage::DisplayOptions: {
		const quint32 bits = static_cast<quint32>(message.first);
		if (((bits & SyncMessage::YOnly) != 0) != mYOnly) {
			toggleYOnly();
		}
		if (((bits & SyncMessage::Interpolate) != 0) != mInterpolate) {
			toggleInterpolate();
		}
		if (((bits & SyncMessage::Coordinates) != 0) != mShowCoordinates) {
			toggleShowCoordinates();
		}
		if (((bits & SyncMessage::HexPixel) != 0) != mHexMode) {
			toggleHexValues();
		}
		if (((bits & SyncMessage::SourceYuv) != 0) != mShowSourceValues) {
			toggleSourceValues();
		}
		break;
	}
	case SyncMessage::ColorSpace:
		applyColorSpace(message.text);
		break;
	case SyncMessage::Resolution:
		applyResolution(message.first, message.second);
		break;
	case SyncMessage::Fps:
		applyFps(message.scalar);
		break;
	default:
		break;
	}
	mApplyingSync = false;
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasUrls()) {
		for (const QUrl &url : event->mimeData()->urls()) {
			if (url.isLocalFile()) {
				event->acceptProposedAction();
				return;
			}
		}
	}

	event->ignore();
}

bool MainWindow::eventFilter(QObject *object, QEvent *event)
{
	if (object != mScrollArea->viewport()) {
		return QMainWindow::eventFilter(object, event);
	}

	if (event->type() == QEvent::Resize && mHelpOverlay && mHelpOverlay->isVisible()) {
		positionHelpOverlay();
	}

	if (event->type() == QEvent::Wheel && !mImage.isNull()) {
		QWheelEvent *wheelEvent = static_cast<QWheelEvent *>(event);
		double factor = 1.0;
		if (!wheelEvent->pixelDelta().isNull()) {
			factor = std::pow(1.01, wheelEvent->pixelDelta().y());
		} else if (!wheelEvent->angleDelta().isNull()) {
			factor = std::pow(1.25, wheelEvent->angleDelta().y() / 120.0);
		}
		if (factor != 1.0) {
			const QPoint anchor = wheelEvent->position().toPoint();
			applyZoom(factor, &anchor);
			wheelEvent->accept();
			return true;
		}
	}

	if (event->type() == QEvent::Gesture && !mImage.isNull()) {
		QGestureEvent *gestureEvent = static_cast<QGestureEvent *>(event);
		QGesture *gesture = gestureEvent->gesture(Qt::PinchGesture);
		if (gesture) {
			QPinchGesture *pinch = static_cast<QPinchGesture *>(gesture);
			if (pinch->lastScaleFactor() > 0.0) {
				const QPoint anchor = pinch->centerPoint().toPoint();
				applyZoom(pinch->scaleFactor() / pinch->lastScaleFactor(), &anchor);
				gestureEvent->accept(pinch);
				return true;
			}
		}
	}

	if (event->type() == QEvent::MouseButtonPress && !mImage.isNull()) {
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
		if (mouseEvent->button() == Qt::LeftButton) {
			if (mSelectionMode) {
				const SelHandle handle = selectionHandleAt(mouseEvent->pos());
				if (handle != SelHandle::None) {
					// Grab the existing selection to resize (edge/corner) or move
					// it (interior) rather than starting a fresh rubber band.
					mActiveHandle = handle;
					mDragStartRect = mSelectionRect;
					mDragStartImagePoint = imagePointFromViewport(mouseEvent->pos());
					mouseEvent->accept();
					return true;
				}
				mIsSelecting = true;
				mSelectionOrigin = mouseEvent->pos();
				mSelectionRect = QRect();
				mImageView->setSelection(mSelectionRect, true);
				mouseEvent->accept();
				return true;
			}
			mIsPanning = true;
			mLastPanPoint = mouseEvent->pos();
			mScrollArea->viewport()->setCursor(Qt::ClosedHandCursor);
			mouseEvent->accept();
			return true;
		}
	}

	if (event->type() == QEvent::MouseMove && !mImage.isNull()) {
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
		const QPoint labelPoint = mImageView->mapFrom(mScrollArea->viewport(), mouseEvent->pos());
		const QPoint displayPoint(
			static_cast<int>(labelPoint.x() / mScaleFactor),
			static_cast<int>(labelPoint.y() / mScaleFactor));
		const QImage shownImage = displayImage();
		if (displayPoint.x() >= 0
			&& displayPoint.y() >= 0
			&& displayPoint.x() < shownImage.width()
			&& displayPoint.y() < shownImage.height()) {
			mCursorImagePoint = sourcePointFromDisplayPoint(displayPoint);
		} else {
			mCursorImagePoint = QPoint(-1, -1);
		}

		if (mActiveHandle != SelHandle::None) {
			const QPoint delta = imagePointFromViewport(mouseEvent->pos()) - mDragStartImagePoint;
			mSelectionRect = resizedSelection(mActiveHandle, delta);
			mImageView->setSelection(mSelectionRect);
			statusBar()->showMessage(tr("Selection  %1 x %2  at (%3, %4)")
				.arg(mSelectionRect.width()).arg(mSelectionRect.height())
				.arg(mSelectionRect.x()).arg(mSelectionRect.y()));
			mouseEvent->accept();
			return true;
		}

		if (mIsSelecting) {
			mSelectionRect = imageRectFromViewport(mSelectionOrigin, mouseEvent->pos());
			mImageView->setSelection(mSelectionRect, true);
			statusBar()->showMessage(tr("Selection  %1 x %2  at (%3, %4)")
				.arg(mSelectionRect.width()).arg(mSelectionRect.height())
				.arg(mSelectionRect.x()).arg(mSelectionRect.y()));
			mouseEvent->accept();
			return true;
		}

		if (mIsPanning) {
			const QPoint delta = mouseEvent->pos() - mLastPanPoint;
			mScrollArea->horizontalScrollBar()->setValue(mScrollArea->horizontalScrollBar()->value() - delta.x());
			mScrollArea->verticalScrollBar()->setValue(mScrollArea->verticalScrollBar()->value() - delta.y());
			mLastPanPoint = mouseEvent->pos();
			broadcastViewState(false);
			mouseEvent->accept();
			return true;
		}

		// Not dragging: in selection mode, reflect what a press here would grab.
		updateSelectionCursor(mouseEvent->pos());
		updateZoomStatus();
	}

	if (event->type() == QEvent::Leave) {
		mCursorImagePoint = QPoint(-1, -1);
		updateZoomStatus();
	}

	if (event->type() == QEvent::MouseButtonRelease) {
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
		if (mouseEvent->button() == Qt::LeftButton && mActiveHandle != SelHandle::None) {
			mActiveHandle = SelHandle::None;
			updateView();
			updateZoomStatus();
			updateSelectionCursor(mouseEvent->pos());
			mouseEvent->accept();
			return true;
		}
		if (mouseEvent->button() == Qt::LeftButton && mIsSelecting) {
			mIsSelecting = false;
			mSelectionRect = imageRectFromViewport(mSelectionOrigin, mouseEvent->pos());
			updateView();  // redraw the committed selection in the success colour
			updateZoomStatus();
			mouseEvent->accept();
			return true;
		}
		if (mouseEvent->button() == Qt::LeftButton && mIsPanning) {
			mIsPanning = false;
			mScrollArea->viewport()->unsetCursor();
			broadcastViewState(true);
			mouseEvent->accept();
			return true;
		}
	}

	if (event->type() == QEvent::ContextMenu) {
		QContextMenuEvent *contextEvent = static_cast<QContextMenuEvent *>(event);
		QMenu menu(this);
		menu.addAction(mZoomInAction);
		menu.addAction(mZoomOutAction);
		menu.addAction(mActualSizeAction);
		menu.addAction(mFitToWindowAction);
		menu.addSeparator();
		menu.addAction(mRotateAction);
		menu.addAction(mYOnlyAction);
		menu.addAction(mCoordinatesAction);
		menu.addAction(mSelectModeAction);
		menu.addSeparator();
		menu.addAction(mCopyAction);
		menu.addAction(mSaveAsAction);
		menu.addSeparator();
		menu.addAction(mAutoReloadAction);
		menu.addAction(mSyncInputAction);
		menu.exec(contextEvent->globalPos());
		contextEvent->accept();
		return true;
	}

	return QMainWindow::eventFilter(object, event);
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_Return:
	case Qt::Key_Enter:
		toggleFullScreen();
		event->accept();
		return;
	case Qt::Key_Slash:
	case Qt::Key_Question:
		showHelp();
		event->accept();
		return;
	case Qt::Key_Left:
		previousFrameOrFile();
		event->accept();
		return;
	case Qt::Key_Right:
		nextFrameOrFile();
		event->accept();
		return;
	case Qt::Key_Home:
		firstFrameOrFile();
		event->accept();
		return;
	case Qt::Key_End:
		lastFrameOrFile();
		event->accept();
		return;
	case Qt::Key_PageUp:
		openAdjacentFile(-1);
		event->accept();
		return;
	case Qt::Key_PageDown:
		openAdjacentFile(1);
		event->accept();
		return;
	case Qt::Key_Space:
		togglePlayback();
		event->accept();
		return;
	case Qt::Key_Escape:
		if (mSelectionRect.isValid() && !mSelectionRect.isEmpty()) {
			clearSelection();
			updateView();
			updateZoomStatus();
			event->accept();
			return;
		}
		break;
	default:
		break;
	}

	QMainWindow::keyPressEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
	QMainWindow::resizeEvent(event);
	if (mFitToWindow && !mImage.isNull()) {
		updateView();
	}
}

void MainWindow::dropEvent(QDropEvent *event)
{
	if (!event->mimeData()->hasUrls()) {
		event->ignore();
		return;
	}

	for (const QUrl &url : event->mimeData()->urls()) {
		if (url.isLocalFile()) {
			openDroppedFile(url.toLocalFile());
			event->acceptProposedAction();
			return;
		}
	}

	event->ignore();
}
