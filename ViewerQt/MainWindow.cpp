#include "MainWindow.h"

#include "RawOpenDialog.h"

#include <QAction>
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
#include <QPixmap>
#include <QResizeEvent>
#include <QScrollBar>
#include <QScrollArea>
#include <QSettings>
#include <QSize>
#include <QStatusBar>
#include <QTimer>
#include <QTransform>
#include <QUrl>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

#include "qimage_cs.h"

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent),
	  mImageLabel(new QLabel),
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
	  mRawFrameSize(0),
	  mRawWidth(0),
	  mRawHeight(0),
	  mRawFrameCount(0),
	  mCurrentFrame(0),
	  mScaleFactor(1.0),
	  mFitToWindow(true),
	  mCurrentFileIsRaw(false),
	  mIsPanning(false),
	  mShowCoordinates(false),
	  mYOnly(false),
	  mRotationQuarterTurns(0),
	  mCursorImagePoint(-1, -1)
{
	mImageLabel->setAlignment(Qt::AlignCenter);
	mImageLabel->setBackgroundRole(QPalette::Base);
	mImageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	mImageLabel->setScaledContents(false);
	mImageLabel->setAcceptDrops(false);
	mImageLabel->setMouseTracking(true);

	mScrollArea->setBackgroundRole(QPalette::Dark);
	mScrollArea->setWidget(mImageLabel);
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

	mPlayTimer->setInterval(33);
	connect(mPlayTimer, &QTimer::timeout, this, &MainWindow::nextFrameOrFile);

	loadSettings();
	createActions();
	updateZoomStatus();
	statusBar()->showMessage(tr("Ready"));
	setWindowTitle(tr("Q1View Qt"));
	resize(960, 640);
}

bool MainWindow::openFile(const QString &fileName)
{
	QImageReader reader(fileName);
	reader.setAutoTransform(true);

	const QImage image = reader.read();
	if (image.isNull()) {
		QMessageBox::warning(this, tr("Open image"),
			tr("Could not read %1:\n%2").arg(QFileInfo(fileName).fileName(), reader.errorString()));
		return false;
	}

	resetPlayback();
	mImage = image;
	mCurrentFile = fileName;
	mCurrentFileIsRaw = false;
	mRawFrameSize = 0;
	mRawFrameCount = 1;
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
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
		const QString fileName = QFileDialog::getOpenFileName(
			this,
			tr("Open image"),
			QString(),
			tr("Images (*.bmp *.gif *.jpeg *.jpg *.png *.ppm *.pgm *.pbm *.xbm *.xpm);;All files (*)"));

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
		const QPoint labelPoint = mImageLabel->mapFrom(mScrollArea->viewport(), *anchor);
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
	setWindowTitle(tr("Q1View Qt"));
	updateView();
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
	return QStringList()
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

	if (!file.seek(static_cast<qint64>(frameIndex) * mRawFrameSize)) {
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
	colorSpace->csc2rgb888(reinterpret_cast<qu8 *>(bgr.data()), base, plane2, plane3, strideBytes, mRawWidth, mRawHeight);

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
	if (reader.canRead()) {
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

	if (!displayImage().save(fileName)) {
		QMessageBox::warning(this, tr("Save image as"), tr("Could not save %1.").arg(QFileInfo(fileName).fileName()));
	}
}

void MainWindow::showHelp()
{
	QMessageBox::information(this, tr("Viewer shortcuts"),
		tr("Mouse wheel / trackpad: zoom\n"
		   "Left drag: pan image\n"
		   "Ctrl+O: open image\n"
		   "Ctrl+V: paste image from clipboard\n"
		   "Ctrl+C: copy image to clipboard\n"
		   "Ctrl+Alt+S: save current image\n"
		   "R: rotate 90 degrees clockwise\n"
		   "Y: toggle Y-only view\n"
		   "C: toggle cursor coordinates\n"
		   "Left/Right: previous or next raw frame\n"
		   "Page Up/Down: previous or next file\n"
		   "Home/End: first or last frame/file\n"
		   "Space: play or stop raw sequence\n"
		   "Return: full screen"));
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
	if (reader.canRead()) {
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
		QApplication::clipboard()->setImage(displayImage());
		statusBar()->showMessage(tr("Copied image to clipboard"), 2000);
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
	updateZoomStatus();
}

void MainWindow::updateView()
{
	const QImage shownImage = displayImage();
	if (shownImage.isNull()) {
		mImageLabel->clear();
		mImageLabel->resize(0, 0);
		updateZoomStatus();
		return;
	}

	if (mFitToWindow) {
		const QSize viewportSize = mScrollArea->viewport()->size();
		const double widthScale = static_cast<double>(viewportSize.width()) / static_cast<double>(shownImage.width());
		const double heightScale = static_cast<double>(viewportSize.height()) / static_cast<double>(shownImage.height());
		mScaleFactor = std::max(0.02, std::min(widthScale, heightScale));
	}

	const QSize scaledSize(
		std::max(1, static_cast<int>(shownImage.width() * mScaleFactor + 0.5)),
		std::max(1, static_cast<int>(shownImage.height() * mScaleFactor + 0.5)));
	const QPixmap pixmap = QPixmap::fromImage(shownImage).scaled(
		scaledSize,
		Qt::KeepAspectRatio,
		mScaleFactor >= 1.0 ? Qt::FastTransformation : Qt::SmoothTransformation);

	mImageLabel->setPixmap(pixmap);
	mImageLabel->resize(pixmap.size());
	updateZoomStatus();
}

void MainWindow::updateZoomStatus()
{
	const bool hasImage = !mImage.isNull();
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
	mRawColorSpaceName = colorSpaceName;
	mRawFrameSize = frameSize;
	mRawWidth = width;
	mRawHeight = height;
	mRawFrameCount = static_cast<int>(fileSize / frameSize);
	mCurrentFrame = 0;
	mRotationQuarterTurns = 0;
	mCursorImagePoint = QPoint(-1, -1);
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
			mIsPanning = true;
			mLastPanPoint = mouseEvent->pos();
			mScrollArea->viewport()->setCursor(Qt::ClosedHandCursor);
			mouseEvent->accept();
			return true;
		}
	}

	if (event->type() == QEvent::MouseMove && !mImage.isNull()) {
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
		const QPoint labelPoint = mImageLabel->mapFrom(mScrollArea->viewport(), mouseEvent->pos());
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
