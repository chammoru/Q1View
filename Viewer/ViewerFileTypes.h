// ViewerFileTypes.h : shared file-extension groups used by the Viewer UI.

#pragma once

namespace q1view {

static const LPCTSTR kViewerImageExts[] = {
	_T("bmp"), _T("jpg"), _T("jpeg"), _T("png"), _T("tif"), _T("tiff"),
	_T("webp"), _T("heic"), _T("heif"), _T("hif"), _T("avif")
};

static const LPCTSTR kViewerVideoExts[] = {
	_T("mp4"), _T("m4v"), _T("mov"), _T("avi"), _T("mkv"), _T("webm"),
	_T("wmv"), _T("mpg"), _T("mpeg"), _T("flv"), _T("3gp"), _T("ts"), _T("m2ts")
};

static const LPCTSTR kViewerYuvExts[] = {
	_T("yuv")
};

static const LPCTSTR kViewerRgbExts[] = {
	_T("rgb")
};

inline bool IsOneOfExts(const CString &ext, const LPCTSTR *exts, int count)
{
	for (int i = 0; i < count; i++)
		if (ext.CompareNoCase(exts[i]) == 0)
			return true;
	return false;
}

inline bool IsViewerImageExt(const CString &ext)
{
	return IsOneOfExts(ext, kViewerImageExts, _countof(kViewerImageExts));
}

inline bool IsViewerVideoExt(const CString &ext)
{
	return IsOneOfExts(ext, kViewerVideoExts, _countof(kViewerVideoExts));
}

inline bool IsViewerThumbnailableExt(const CString &ext)
{
	return IsViewerImageExt(ext) || IsViewerVideoExt(ext);
}

inline CString ExtensionWildcards(const LPCTSTR *exts, int count)
{
	CString s;
	for (int i = 0; i < count; i++) {
		if (i > 0)
			s += _T(";");
		s += _T("*.");
		s += exts[i];
	}
	return s;
}

inline CString OpenFileFilter()
{
	CString image = ExtensionWildcards(kViewerImageExts, _countof(kViewerImageExts));
	CString yuv = ExtensionWildcards(kViewerYuvExts, _countof(kViewerYuvExts));
	CString rgb = ExtensionWildcards(kViewerRgbExts, _countof(kViewerRgbExts));

	CString filter;
	filter.Format(_T("All Files (*.*)|*.*|Image Files (%s)|%s|YUV Files (%s)|%s|RGB Files (%s)|%s|"),
		(LPCTSTR)image, (LPCTSTR)image,
		(LPCTSTR)yuv, (LPCTSTR)yuv,
		(LPCTSTR)rgb, (LPCTSTR)rgb);
	return filter;
}

} // namespace q1view
