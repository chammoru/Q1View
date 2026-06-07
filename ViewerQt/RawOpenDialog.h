#ifndef Q1VIEW_VIEWERQT_RAWOPENDIALOG_H
#define Q1VIEW_VIEWERQT_RAWOPENDIALOG_H

#include <QString>

class QWidget;

struct RawOpenOptions
{
	QString fileName;
	// Default to the viewer's display-area footprint, matching the MFC viewer's
	// VIEWER_DEF_W x VIEWER_DEF_H rather than a hard-coded 1080p.
	int width = 500;
	int height = 392;
	QString colorSpaceName = QStringLiteral("yuv420");
};

class RawOpenDialog
{
public:
	static bool getOptions(QWidget *parent, RawOpenOptions *options);
};

#endif
