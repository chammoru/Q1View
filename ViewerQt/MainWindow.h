#ifndef Q1VIEW_VIEWERQT_MAINWINDOW_H
#define Q1VIEW_VIEWERQT_MAINWINDOW_H

#include "RawOpenDialog.h"

#include <QImage>
#include <QMainWindow>

class QDragEnterEvent;
class QDropEvent;
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
	void loadSettings();
	void saveRawSettings() const;
	void openDroppedFile(const QString &fileName);
	void updateImage();

protected:
	void dragEnterEvent(QDragEnterEvent *event) override;
	void dropEvent(QDropEvent *event) override;

private:
	QLabel *mImageLabel;
	QScrollArea *mScrollArea;
	QImage mImage;
	QString mCurrentFile;
	RawOpenOptions mRawOptions;
};

#endif
