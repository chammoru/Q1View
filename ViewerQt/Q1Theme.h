#ifndef Q1VIEW_VIEWERQT_Q1THEME_H
#define Q1VIEW_VIEWERQT_Q1THEME_H

#include <QColor>
#include <QString>

// Shared Q1View UI palette for the Qt viewer. These mirror the MFC viewer's
// Q1UI_COLOR_* values in QViewerCmn.h, which is Windows/OpenCV-only and cannot be
// included here. Keeping the canonical copy in one place stops the same hex from
// being scattered (and drifting) across widgets and stylesheets.
namespace q1theme {

inline QColor canvasBg()   { return QColor(0xf7, 0xf9, 0xfc); } // Q1UI_COLOR_CANVAS_BG
inline QColor surface()    { return QColor(0xff, 0xff, 0xff); } // Q1UI_COLOR_SURFACE
inline QColor surfaceAlt() { return QColor(0xf8, 0xfa, 0xfd); } // Q1UI_COLOR_SURFACE_ALT
inline QColor border()     { return QColor(0xd8, 0xe0, 0xea); } // Q1UI_COLOR_BORDER
inline QColor text()       { return QColor(0x1f, 0x29, 0x37); } // Q1UI_COLOR_TEXT
inline QColor accentSoft() { return QColor(0xdb, 0xe7, 0xff); } // Q1UI_COLOR_ACCENT_SOFT
inline QColor warning()    { return QColor(0xf5, 0x9e, 0x0b); } // Q1UI_COLOR_WARNING
inline QColor success()    { return QColor(0x16, 0x9b, 0x62); } // Q1UI_COLOR_SUCCESS
inline QColor pixelText()  { return QColor(0xe8, 0xee, 0xf7); } // COLOR_PIXEL_TEXT

// "#rrggbb" for stylesheet strings, derived from the same QColor so the painted
// and stylesheet colours never drift apart.
inline QString hex(const QColor &c) { return c.name(QColor::HexRgb); }

} // namespace q1theme

#endif // Q1VIEW_VIEWERQT_Q1THEME_H
