#ifndef __QIMAGE_PRESETS_H__
#define __QIMAGE_PRESETS_H__

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
