#include "MainWindow.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QImageReader>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QScrollArea>
#include <QStatusBar>

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
