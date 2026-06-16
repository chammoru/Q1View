#ifndef Q1VIEW_VIEWERQT_RAWOPENDIALOG_H
#define Q1VIEW_VIEWERQT_RAWOPENDIALOG_H

#include <QString>

#include "qimage_presets.h" // shared VIEWER_DEF_W / VIEWER_DEF_H

class QWidget;

struct RawOpenOptions
{
	QString fileName;
	// Default to the viewer's display-area footprint, matching the MFC viewer's
	// VIEWER_DEF_W x VIEWER_DEF_H rather than a hard-coded 1080p.
	int width = VIEWER_DEF_W;
	int height = VIEWER_DEF_H;
	QString colorSpaceName = QStringLiteral("yuv420");
};

class RawOpenDialog
{
public:
	static bool getOptions(QWidget *parent, RawOpenOptions *options);
};

#endif
