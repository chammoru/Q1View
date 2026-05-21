#ifndef Q1VIEW_VIEWERQT_MAINWINDOW_H
#define Q1VIEW_VIEWERQT_MAINWINDOW_H

#include <QImage>
#include <QMainWindow>

class QLabel;
class QScrollArea;

class MainWindow : public QMainWindow
{
public:
	explicit MainWindow(QWidget *parent = nullptr);

	bool openFile(const QString &fileName);
	bool openRawFile(const QString &fileName, int width, int height, const QString &colorSpaceName);

private:
	void createActions();
	void updateImage();

	QLabel *mImageLabel;
	QScrollArea *mScrollArea;
	QImage mImage;
	QString mCurrentFile;
};

#endif
