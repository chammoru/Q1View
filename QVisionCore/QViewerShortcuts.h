#ifndef Q1VIEW_QVISIONCORE_QVIEWERSHORTCUTS_H
#define Q1VIEW_QVISIONCORE_QVIEWERSHORTCUTS_H

// Single source of truth for the Viewer shortcut panel, shared by the MFC
// Viewer (CViewerView::DrawHelpMenu) and the Qt viewer (MainWindow's help
// overlay) so both render the same keys and descriptions from one place.
//
// Plain ASCII so it can feed both a CString (GDI DrawText) and a QString
// (QLabel). The visual style - white surface, soft border, dark text, Cascadia
// Mono at 14 px - is shared through the Q1UI_COLOR_SURFACE / Q1UI_COLOR_BORDER /
// Q1UI_COLOR_TEXT / Q1UI_FONT_MONO constants in QViewerCmn.h (the Qt side, which
// can't include that Windows-only header, mirrors the same hex values).

// Bitmask: which front-end a row applies to. The MFC and Qt feature sets differ,
// so each renderer shows only the rows tagged for it; the common rows live here
// exactly once.
enum {
	Q1VIEW_FE_MFC = 0x1,
	Q1VIEW_FE_QT  = 0x2,
	Q1VIEW_FE_ALL = Q1VIEW_FE_MFC | Q1VIEW_FE_QT
};

struct Q1ViewShortcutRow {
	const char *key;   // left column, e.g. "Ctrl + C"
	const char *desc;  // right column
	unsigned    fe;    // OR of Q1VIEW_FE_*
};

// Title drawn above the list.
#define Q1VIEW_SHORTCUTS_TITLE "Viewer shortcuts"
// Width of the key column, in monospace characters, used to align descriptions.
#define Q1VIEW_SHORTCUTS_KEY_WIDTH 15

// Display order. Rows tagged Q1VIEW_FE_MFC only appear in the MFC panel, QT only
// in the Qt overlay, ALL in both. The MFC-tagged rows are kept in their original
// order so the MFC panel reads exactly as before.
static const Q1ViewShortcutRow Q1VIEW_SHORTCUTS[] = {
	{ "?",            "Show or hide this panel",                      Q1VIEW_FE_ALL },
	{ "E",            "Toggle thumbnail browser (drawer)",            Q1VIEW_FE_MFC },
	{ "Drag & Drop",  "Open an image",                                Q1VIEW_FE_ALL },
	{ "Ctrl + O",     "Open an image",                                Q1VIEW_FE_QT  },
	{ "Mouse Wheel",  "Zoom in or out; high zoom shows pixel values", Q1VIEW_FE_ALL },
	{ "Left Drag",    "Pan the image",                                Q1VIEW_FE_QT  },
	{ "Return",       "Full screen",                                  Q1VIEW_FE_ALL },
	{ "H",            "Toggle hex pixel values",                      Q1VIEW_FE_ALL },
	{ "Y",            "Toggle Y-only view",                           Q1VIEW_FE_ALL },
	{ "V",            "Toggle RGB/source YUV pixel values",           Q1VIEW_FE_MFC },
	{ "R",            "Rotate 90 degrees clockwise",                  Q1VIEW_FE_ALL },
	{ "Page Up/Down", "Previous or next file",                        Q1VIEW_FE_ALL },
	{ "S",            "Selection capture mode",                       Q1VIEW_FE_ALL },
	{ "Esc",          "Clear selection",                              Q1VIEW_FE_QT  },
	{ "Ctrl + C",     "Capture view or selected region",             Q1VIEW_FE_ALL },
	{ "Ctrl + V",     "Paste image from clipboard",                   Q1VIEW_FE_ALL },
	{ "Ctrl+Alt+S",   "Save image or selection",                      Q1VIEW_FE_QT  },
	{ "Left/Right",   "Previous or next video frame",                 Q1VIEW_FE_ALL },
	{ "Home/End",     "First or last frame/file",                     Q1VIEW_FE_ALL },
	{ "Space",        "Play or stop video",                           Q1VIEW_FE_ALL },
	{ "Click bottom", "Seek to a video frame",                        Q1VIEW_FE_MFC },
	{ "C",            "Cursor coordinates",                           Q1VIEW_FE_ALL },
	{ "B",            "Selected box size",                            Q1VIEW_FE_MFC },
	{ "I",            "Interpolate pixels",                           Q1VIEW_FE_ALL },
	{ "N",            "Next color space",                             Q1VIEW_FE_MFC },
	{ "D",            "Next preset resolution (raw input)",           Q1VIEW_FE_MFC },
	{ "M",            "Mute or unmute video audio",                   Q1VIEW_FE_MFC }
};

#endif
