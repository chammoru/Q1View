#ifndef __QIMAGE_PRESETS_H__
#define __QIMAGE_PRESETS_H__

// Default viewer footprint, shared by the MFC and Qt front-ends so both open at
// the same fixed size on a fresh launch and assume the same dimensions for a raw
// image of unknown size. 640x480 (VGA) is a standard codec/image resolution and
// one of the resolution_info_table presets below, so the resolution menu shows a
// real, checked preset on launch instead of a non-standard size (issue #77).
#define VIEWER_DEF_W 640
#define VIEWER_DEF_H 480

// Thumbnail drawer width (logical px), shared by the MFC and Qt viewers so the
// drawer opens at the same default size and clamps to the same min/max when the
// user drags the divider. The drawer always lives inside the fixed window: it
// takes its width from the image area rather than growing the window (issue #76).
#define DRAWER_DEF_W 220
#define DRAWER_MIN_W 120
#define DRAWER_MAX_W 480

namespace q1 {

// Preset resolutions -- the single source of truth for the resolution menus in
// the MFC Viewer/Comparator and the cross-platform Qt viewer. Menus are
// generated directly from this table, so add new presets here only. The trailing
// "Custom..." entry is a UI sentinel (it has no WxH to parse); each front-end
// maps it to a custom-size prompt.
//
// This header is deliberately dependency-free (no OpenCV / Windows headers) so it
// can be included from the portable Qt build as well as QViewerCmn.h.
static const char *resolution_info_table[] = {
	"176x144 (QCIF)",
	"320x240 (QVGA)",
	"352x288 (CIF)",
	"640x480 (VGA)",
	"720x480",
	"800x600 (SVGA)",
	"960x540",
	"1024x600 (WSVGA)",
	"1024x768 (XGA)",
	"1280x720 (HD)",
	"1920x1080 (FHD)",
	"2560x1600 (WQXGA)",
	"3840x2160 (4K UHD)",
	"4096x2160 (Cinema UHD)",

	"C&ustom...",
};

} // namespace q1

#endif /* __QIMAGE_PRESETS_H__ */
