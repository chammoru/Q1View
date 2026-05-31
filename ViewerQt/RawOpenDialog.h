#ifndef Q1VIEW_VIEWERQT_RAWOPENDIALOG_H
#define Q1VIEW_VIEWERQT_RAWOPENDIALOG_H

#include <QString>

class QWidget;

struct RawOpenOptions
{
	QString fileName;
	int width = 1920;
	int height = 1080;
	QString colorSpaceName = QStringLiteral("yuv420");
};

class RawOpenDialog
{
public:
	static bool getOptions(QWidget *parent, RawOpenOptions *options);
};

#endif
