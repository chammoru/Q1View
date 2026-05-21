#include "MainWindow.h"

#include "RawOpenDialog.h"

#include <QAction>
#include <QByteArray>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QIODevice>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QScrollArea>
#include <QStatusBar>

#include "qimage_cs.h"

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent),
	  mImageLabel(new QLabel),
	  mScrollArea(new QScrollArea)
{
	mImageLabel->setAlignment(Qt::AlignCenter);
	mImageLabel->setBackgroundRole(QPalette::Base);
	mImageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
	mImageLabel->setScaledContents(false);

	mScrollArea->setBackgroundRole(QPalette::Dark);
	mScrollArea->setWidget(mImageLabel);
	mScrollArea->setWidgetResizable(true);
	setCentralWidget(mScrollArea);

	createActions();
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

	mImage = image;
	mCurrentFile = fileName;
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

	QAction *openRawAction = fileMenu->addAction(tr("Open &Raw..."));
	connect(openRawAction, &QAction::triggered, this, [this]() {
		RawOpenOptions options;
		if (!RawOpenDialog::getOptions(this, &options)) {
			return;
		}

		openRawFile(options.fileName, options.width, options.height, options.colorSpaceName);
	});

	fileMenu->addSeparator();

	QAction *exitAction = fileMenu->addAction(tr("E&xit"));
	exitAction->setShortcut(QKeySequence::Quit);
	connect(exitAction, &QAction::triggered, this, &QWidget::close);
}

void MainWindow::updateImage()
{
	mImageLabel->setPixmap(QPixmap::fromImage(mImage));
	mImageLabel->adjustSize();

	const QFileInfo info(mCurrentFile);
	setWindowTitle(tr("%1 - Q1View Qt").arg(info.fileName()));
	statusBar()->showMessage(tr("%1 x %2").arg(mImage.width()).arg(mImage.height()));
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

	const QByteArray raw = file.readAll();
	if (raw.size() < frameSize) {
		QMessageBox::warning(this, tr("Open raw image"),
			tr("File is too small for %1 %2x%3.\nExpected at least %4 bytes, got %5 bytes.")
				.arg(colorSpaceName)
				.arg(width)
				.arg(height)
				.arg(frameSize)
				.arg(raw.size()));
		return false;
	}

	const int stridePixels = ROUNDUP_DWORD(width);
	const int strideBytes = stridePixels * QIMG_DST_RGB_BYTES;
	QByteArray bgr;
	bgr.resize(strideBytes * height);

	qu8 *base = reinterpret_cast<qu8 *>(const_cast<char *>(raw.constData()));
	qu8 *plane2 = offset2 > 0 ? base + offset2 : nullptr;
	qu8 *plane3 = offset3 > 0 ? base + offset3 : nullptr;
	colorSpace->csc2rgb888(reinterpret_cast<qu8 *>(bgr.data()), base, plane2, plane3, strideBytes, width, height);

	mImage = QImage(
		reinterpret_cast<const uchar *>(bgr.constData()),
		width,
		height,
		strideBytes,
		QImage::Format_BGR888).copy();
	mCurrentFile = fileName;
	updateImage();
	statusBar()->showMessage(tr("%1 %2x%3").arg(colorSpaceName).arg(width).arg(height));
	return true;
}
