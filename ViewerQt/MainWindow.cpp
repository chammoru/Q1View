#include "MainWindow.h"

#include "HeifReader.h"
#include "ImageView.h"
#include "RawOpenDialog.h"
#include "Y4mReader.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
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
#include <QRubberBand>
#include <QScrollBar>
#include <QScrollArea>
#include <QSettings>
#include <QSize>
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

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent),
	  mImageView(new ImageView),
	  mScrollArea(new QScrollArea),
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
	  mPlayAction(nullptr),
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
	  mRubberBand(nullptr),
	  mSelectModeAction(nullptr),
	  mSelectionMode(false),
	  mIsSelecting(false)
{
	mImageView->setAcceptDrops(false);
	mImageView->setMouseTracking(true);

	mScrollArea->setBackgroundRole(QPalette::Dark);
	mScrollArea->setWidget(mImageView);
	mScrollArea->setWidgetResizable(false);
	mScrollArea->setAlignment(Qt::AlignCenter);
	mScrollArea->setAcceptDrops(false);
	mScrollArea->viewport()->setMouseTracking(true);
	mScrollArea->viewport()->setFocusPolicy(Qt::StrongFocus);
	mScrollArea->viewport()->grabGesture(Qt::PinchGesture);
	mScrollArea->viewport()->installEventFilter(this);
	setCentralWidget(mScrollArea);
	setAcceptDrops(true);
	setFocusPolicy(Qt::StrongFocus);

	mPlayTimer->setInterval(static_cast<int>(1000.0 / mFps + 0.5));
	connect(mPlayTimer, &QTimer::timeout, this, &MainWindow::nextFrameOrFile);

	// Translucent shortcut overlay. Parented to the viewport so it stays pinned to
	// the visible area instead of scrolling with the image, mirroring the MFC
	// viewer's in-canvas help rather than a modal dialog.
	mHelpOverlay = new QLabel(mScrollArea->viewport());
	mHelpOverlay->setText(helpText());
	mHelpOverlay->setTextFormat(Qt::PlainText);
	mHelpOverlay->setMargin(16);
	{
		QFont overlayFont(QStringLiteral("Cascadia Mono"));
		overlayFont.setStyleHint(QFont::Monospace);
		overlayFont.setPointSize(10);
		mHelpOverlay->setFont(overlayFont);
	}
	// Colors mirror Q1UI_COLOR_OVERLAY / Q1UI_COLOR_OVERLAY_TEXT in QViewerCmn.h.
	mHelpOverlay->setStyleSheet(QStringLiteral(
		"background-color: rgba(32, 42, 54, 235);"
		"color: #f8fafc;"
		"border-radius: 8px;"));
	mHelpOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
	mHelpOverlay->hide();

	loadSettings();
	createActions();
	refreshControlMenus();
	updateZoomStatus();
	statusBar()->showMessage(tr("Ready"));
	setWindowTitle(tr("Q1View Qt"));
	resize(960, 640);
}

bool MainWindow::openFile(const QString &fileName)
{
	// A .y4m/.yuv may be a YUV4MPEG2 container rather than a still image; route
	// it to the Y4M loader, which parses the header and frame markers.
	if (q1y4m::isY4mFile(fileName)) {
		return openY4mFile(fileName);
	}

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
		QMessageBox::warning(this, tr("Open image"),
			tr("Could not read %1:\n%2").arg(QFileInfo(fileName).fileName(), detail));
		return false;
	}

	resetPlayback();
	mImage = image;
	mCurrentFile = fileName;
	mCurrentFileIsRaw = false;
	mIsY4m = false;
	mRawFrameSize = 0;
	mRawFrameCount = 1;
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
	clearSelection();
	mFitToWindow = true;
	updateImage();
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
		const QString fileName = QFileDialog::getOpenFileName(
			this,
			tr("Open image"),
			QString(),
			tr("Images (*.bmp *.gif *.jpeg *.jpg *.png *.ppm *.pgm *.pbm *.xbm *.xpm%1);;All files (*)")
				.arg(heifExtensions));

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

	mCoordinatesAction = viewMenu->addAction(tr("Cursor &Coordinates"));
	mCoordinatesAction->setShortcut(QKeySequence(tr("C")));
	mCoordinatesAction->setCheckable(true);
	connect(mCoordinatesAction, &QAction::triggered, this, &MainWindow::toggleShowCoordinates);

	mSelectModeAction = viewMenu->addAction(tr("&Selection Mode"));
	mSelectModeAction->setShortcut(QKeySequence(tr("S")));
	mSelectModeAction->setCheckable(true);
	connect(mSelectModeAction, &QAction::triggered, this, &MainWindow::toggleSelectionMode);

	viewMenu->addSeparator();

	QAction *fullScreenAction = viewMenu->addAction(tr("&Full Screen"));
	fullScreenAction->setShortcut(QKeySequence(tr("Return")));
	connect(fullScreenAction, &QAction::triggered, this, &MainWindow::toggleFullScreen);

	QAction *helpAction = viewMenu->addAction(tr("&Help"));
	helpAction->setShortcut(QKeySequence(tr("?")));
	connect(helpAction, &QAction::triggered, this, &MainWindow::showHelp);

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

	// Live magnification readout pinned to the menu bar's right corner, mirroring
	// the MFC viewer's right-justified "WxH (n.nnx)" item.
	mMagnifyLabel = new QLabel(menuBar());
	mMagnifyLabel->setContentsMargins(0, 0, 12, 0);
	menuBar()->setCornerWidget(mMagnifyLabel, Qt::TopRightCorner);
}

void MainWindow::createControlMenus()
{
	mResolutionMenu = menuBar()->addMenu(QStringLiteral("1920x1080"));
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

void MainWindow::refreshControlMenus()
{
	// Resolution and color space can only be reinterpreted for our own packed raw
	// sources; structured images carry their own dimensions, and Y4M defines them
	// in its container header.
	const bool rawTweakable = mCurrentFileIsRaw && !mIsY4m;
	const bool fpsTweakable = mCurrentFileIsRaw && mRawFrameCount > 1;

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
}

void MainWindow::applyFps(double fps)
{
	if (fps > 0.0) {
		mFps = fps;
		mPlayTimer->setInterval(static_cast<int>(1000.0 / mFps + 0.5));
	}
	refreshControlMenus();
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
	return tr("Mouse wheel / trackpad: zoom\n"
		"Left drag: pan image\n"
		"Ctrl+O: open image\n"
		"Ctrl+V: paste image from clipboard\n"
		"Ctrl+C: copy image or selection to clipboard\n"
		"Ctrl+Alt+S: save current image or selection\n"
		"R: rotate 90 degrees clockwise\n"
		"Y: toggle Y-only view\n"
		"C: toggle cursor coordinates\n"
		"S: selection mode (drag to select a region)\n"
		"Esc: clear selection\n"
		"Zoom in past 16x: per-pixel value overlay\n"
		"Left/Right: previous or next raw frame\n"
		"Page Up/Down: previous or next file\n"
		"Home/End: first or last frame/file\n"
		"Space: play or stop raw sequence\n"
		"Return: full screen\n"
		"?: toggle this help");
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
	mHelpOverlay->move(viewport.center() - mHelpOverlay->rect().center());
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
}

void MainWindow::closeCurrentFile()
{
	resetPlayback();
	mImage = QImage();
	mCurrentFile.clear();
	mCurrentFileIsRaw = false;
	mRawFrameSize = 0;
	mRawFrameCount = 0;
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
	clearSelection();
	setWindowTitle(tr("Q1View Qt"));
	updateView();
	refreshControlMenus();
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
		<< QStringLiteral("*.nv21");

	// Only advertise HEIF-family extensions for navigation when this build can
	// actually decode them; otherwise stepping onto one would just fail.
	if (q1qt::heifSupported()) {
		filters
			<< QStringLiteral("*.heic")
			<< QStringLiteral("*.heif")
			<< QStringLiteral("*.hif")
			<< QStringLiteral("*.avif");
	}

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
		QMessageBox::warning(this, tr("Open raw image"), tr("Unsupported color space: %1").arg(mRawColorSpaceName));
		return false;
	}

	QFile file(mCurrentFile);
	if (!file.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(this, tr("Open raw image"), tr("Could not open %1").arg(QFileInfo(mCurrentFile).fileName()));
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
	QImageReader reader(nextFile);
	if (reader.canRead() || (q1qt::heifSupported() && q1qt::isHeifFile(nextFile))) {
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
	clearSelection();
	mImage = image;
	mCurrentFile = tr("Clipboard");
	mCurrentFileIsRaw = false;
	mRawFrameCount = 1;
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
	mFitToWindow = true;
	updateImage();
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
}

void MainWindow::loadSettings()
{
	QSettings settings;
	mRawOptions.width = settings.value(QStringLiteral("raw/width"), mRawOptions.width).toInt();
	mRawOptions.height = settings.value(QStringLiteral("raw/height"), mRawOptions.height).toInt();
	mRawOptions.colorSpaceName = settings.value(QStringLiteral("raw/colorSpace"), mRawOptions.colorSpaceName).toString();
	mRawOptions.fileName = settings.value(QStringLiteral("raw/fileName")).toString();
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
	} else {
		openAdjacentFile(-1, true);
	}
}

void MainWindow::lastFrameOrFile()
{
	if (mCurrentFileIsRaw && mRawFrameCount > 1) {
		loadRawFrame(mRawFrameCount - 1);
	} else {
		openAdjacentFile(1, true);
	}
}

void MainWindow::nextFrameOrFile()
{
	if (mCurrentFileIsRaw && mRawFrameCount > 1) {
		if (mCurrentFrame >= mRawFrameCount - 1) {
			resetPlayback();
			return;
		}
		loadRawFrame(mCurrentFrame + 1);
	} else {
		openAdjacentFile(1);
	}
}

void MainWindow::previousFrameOrFile()
{
	if (mCurrentFileIsRaw && mRawFrameCount > 1) {
		loadRawFrame(std::max(0, mCurrentFrame - 1));
	} else {
		openAdjacentFile(-1);
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
	if (!mCurrentFileIsRaw || mRawFrameCount <= 1) {
		resetPlayback();
		return;
	}

	if (mPlayTimer->isActive()) {
		resetPlayback();
		return;
	}

	if (mCurrentFrame >= mRawFrameCount - 1) {
		loadRawFrame(0);
	}
	if (mPlayAction) {
		mPlayAction->setChecked(true);
	}
	mPlayTimer->start();
}

void MainWindow::toggleShowCoordinates()
{
	mShowCoordinates = !mShowCoordinates;
	if (mCoordinatesAction) {
		mCoordinatesAction->setChecked(mShowCoordinates);
	}
	updateZoomStatus();
}

void MainWindow::toggleYOnly()
{
	mYOnly = !mYOnly;
	if (mYOnlyAction) {
		mYOnlyAction->setChecked(mYOnly);
	}
	updateView();
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
		mScaleFactor = std::max(0.02, std::min(widthScale, heightScale));
	}

	mImageView->setYOnly(mYOnly);
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
			tr("Selection mode on: drag to select a region; Copy/Save As then act on it"), 4000);
	} else {
		mIsSelecting = false;
		if (mRubberBand) {
			mRubberBand->hide();
		}
		mScrollArea->viewport()->unsetCursor();
		statusBar()->showMessage(tr("Selection mode off"), 2000);
	}
}

void MainWindow::clearSelection()
{
	mIsSelecting = false;
	mSelectionRect = QRect();
	if (mRubberBand) {
		mRubberBand->hide();
	}
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
		QMessageBox::warning(this, tr("Open raw image"), tr("Unsupported color space: %1").arg(colorSpaceName));
		return false;
	}

	int offset2 = 0;
	int offset3 = 0;
	const int frameSize = colorSpace->cs_load_info(width, height, &offset2, &offset3);
	if (frameSize <= 0) {
		QMessageBox::warning(this, tr("Open raw image"), tr("Invalid raw image size."));
		return false;
	}

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly)) {
		QMessageBox::warning(this, tr("Open raw image"), tr("Could not open %1").arg(QFileInfo(fileName).fileName()));
		return false;
	}

	const qint64 fileSize = file.size();
	if (fileSize < frameSize) {
		QMessageBox::warning(this, tr("Open raw image"),
			tr("File is too small for %1 %2x%3.\nExpected at least %4 bytes, got %5 bytes.")
				.arg(colorSpaceName)
				.arg(width)
				.arg(height)
				.arg(frameSize)
				.arg(fileSize));
		return false;
	}

	resetPlayback();
	mCurrentFile = fileName;
	mCurrentFileIsRaw = true;
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
	return loadRawFrame(0);
}

bool MainWindow::openY4mFile(const QString &fileName)
{
	q1y4m::Y4mInfo info;
	if (!q1y4m::parseY4m(fileName, &info)) {
		QMessageBox::warning(this, tr("Open Y4M"),
			tr("%1 is not a supported YUV4MPEG2 file.").arg(QFileInfo(fileName).fileName()));
		return false;
	}

	resetPlayback();
	mCurrentFile = fileName;
	mCurrentFileIsRaw = true;
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
	clearSelection();
	mFitToWindow = true;
	return loadRawFrame(0);
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
				mIsSelecting = true;
				mSelectionOrigin = mouseEvent->pos();
				if (!mRubberBand) {
					mRubberBand = new QRubberBand(QRubberBand::Rectangle, mScrollArea->viewport());
				}
				mRubberBand->setGeometry(QRect(mSelectionOrigin, QSize()));
				mRubberBand->show();
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

		if (mIsSelecting && mRubberBand) {
			mRubberBand->setGeometry(QRect(mSelectionOrigin, mouseEvent->pos()).normalized());
			const QRect sel = imageRectFromViewport(mSelectionOrigin, mouseEvent->pos());
			statusBar()->showMessage(tr("Selection  %1 x %2  at (%3, %4)")
				.arg(sel.width()).arg(sel.height()).arg(sel.x()).arg(sel.y()));
			mouseEvent->accept();
			return true;
		}

		if (mIsPanning) {
			const QPoint delta = mouseEvent->pos() - mLastPanPoint;
			mScrollArea->horizontalScrollBar()->setValue(mScrollArea->horizontalScrollBar()->value() - delta.x());
			mScrollArea->verticalScrollBar()->setValue(mScrollArea->verticalScrollBar()->value() - delta.y());
			mLastPanPoint = mouseEvent->pos();
			mouseEvent->accept();
			return true;
		}

		updateZoomStatus();
	}

	if (event->type() == QEvent::Leave) {
		mCursorImagePoint = QPoint(-1, -1);
		updateZoomStatus();
	}

	if (event->type() == QEvent::MouseButtonRelease) {
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
		if (mouseEvent->button() == Qt::LeftButton && mIsSelecting) {
			mIsSelecting = false;
			if (mRubberBand) {
				mRubberBand->hide();
			}
			mSelectionRect = imageRectFromViewport(mSelectionOrigin, mouseEvent->pos());
			updateView();
			updateZoomStatus();
			mouseEvent->accept();
			return true;
		}
		if (mouseEvent->button() == Qt::LeftButton && mIsPanning) {
			mIsPanning = false;
			mScrollArea->viewport()->unsetCursor();
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
