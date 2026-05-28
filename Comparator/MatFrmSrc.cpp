#include "stdafx.h"
#include "MatFrmSrc.h"
#include "QImageStr.h"
#include "QCvUtil.h"
#include "ComparatorPane.h"

#include <algorithm>

using namespace cv;

static bool IsLikelyImageFile(const CString& path)
{
	int dot = path.ReverseFind(_T('.'));
	if (dot < 0)
		return false;

	CString ext = path.Mid(dot + 1);
	ext.MakeLower();

	// Match the set OpenCV's imread covers in the configurations Q1View ships
	// with. Used as a cheap pre-filter so the folder scan doesn't try to
	// imreadW() a directory full of mp4 / yuv / txt files.
	return ext == _T("bmp") || ext == _T("dib")
		|| ext == _T("jpg") || ext == _T("jpeg") || ext == _T("jpe")
		|| ext == _T("png")
		|| ext == _T("webp")
		|| ext == _T("pbm") || ext == _T("pgm") || ext == _T("ppm")
		|| ext == _T("sr") || ext == _T("ras")
		|| ext == _T("tif") || ext == _T("tiff")
		|| ext == _T("exr") || ext == _T("hdr") || ext == _T("pic");
}

MatFrmSrc::MatFrmSrc(SQPane *pane)
: FrmSrc(pane, true)
, mCurFrameID(0)
, mDstW(0)
, mDstH(0)
{}

MatFrmSrc::~MatFrmSrc()
{
	Release();
}

bool MatFrmSrc::Open(const CString& filePath, const struct qcsc_info* sortedCscInfo,
	int srcW, int srcH, int dstW, int dstH)
{
	UNREFERENCED_PARAMETER(sortedCscInfo);

	mOcvMat = q1::imreadW(filePath.GetString());
	if (mOcvMat.data == NULL)
		return false;

	mDstW = dstW;
	mDstH = dstH;
	if (mOcvMat.cols != dstW || mOcvMat.rows != dstH)
		resize(mOcvMat, mOcvMat, Size(dstW, dstH));

	// Treat the rest of the folder as part of the sequence so the timeline
	// and Left/Right shortcuts work like they do for video sources. Anchor
	// mCurFrameID to the file the user actually opened — not index 0 — so
	// navigation feels relative to "where I am" instead of jumping home.
	BuildFileList(filePath, srcW, srcH);

	return true;
}

void MatFrmSrc::BuildFileList(const CString& filePath, int srcW, int srcH)
{
	mFileList.clear();
	mCurFrameID = 0;

	// Take whichever separator appears later — drag-drop and some shell APIs
	// hand us mixed `C:\foo/bar/baz.png`, where only checking '\\' first would
	// peel back to `C:\` and scan the entire root.
	int slash = std::max(filePath.ReverseFind(_T('\\')),
	                     filePath.ReverseFind(_T('/')));
	if (slash < 0) {
		mFileList.push_back(filePath);
		return;
	}

	CString dir = filePath.Left(slash + 1);
	CString pattern = dir + _T("*");

	CFileFind finder;
	BOOL bWorking = finder.FindFile(pattern);
	while (bWorking) {
		bWorking = finder.FindNextFile();
		if (finder.IsDirectory())
			continue;

		CString candidatePath = finder.GetFilePath();
		if (!IsLikelyImageFile(candidatePath))
			continue;

		// Verify resolution via a real decode. q1::imreadW returns empty
		// data on anything imread can't handle, so this also drops files
		// whose extension we guessed wrong about.
		int w = 0, h = 0;
		if (GetResolution(candidatePath, &w, &h) && w == srcW && h == srcH)
			mFileList.push_back(candidatePath);
	}
	finder.Close();

	// Deterministic, case-insensitive lexical order so zero-padded names
	// like frame_001.png .. frame_100.png line up naturally. Mixed-pad
	// names (frame_2 / frame_10) won't sort numerically; that's a known
	// limitation users can avoid by zero-padding their output.
	std::sort(mFileList.begin(), mFileList.end(),
		[](const CString& a, const CString& b) { return a.CompareNoCase(b) < 0; });

	for (size_t i = 0; i < mFileList.size(); i++) {
		if (mFileList[i].CompareNoCase(filePath) == 0) {
			mCurFrameID = (long)i;
			return;
		}
	}

	// Shouldn't happen — we just imread'd filePath successfully — but if the
	// opened file somehow isn't in the scan results, fall back to inserting
	// it at the front so the sequence still has the right starting frame.
	mFileList.insert(mFileList.begin(), filePath);
	mCurFrameID = 0;
}

bool MatFrmSrc::LoadFrame(long id)
{
	if (id < 0 || id >= (long)mFileList.size())
		return false;

	Mat next = q1::imreadW(mFileList[id].GetString());
	if (next.data == NULL)
		return false;

	if (next.cols != mDstW || next.rows != mDstH)
		resize(next, next, Size(mDstW, mDstH));

	mOcvMat = next;
	return true;
}

int MatFrmSrc::GetFrameNum()
{
	return (int)std::max<size_t>(mFileList.size(), 1);
}

void MatFrmSrc::Release()
{
	if (IsAvailable())
		mOcvMat.release();
	mFileList.clear();
	mCurFrameID = 0;
}

bool MatFrmSrc::GetResolution(CString &pathName, int* w, int* h)
{
	Mat ocvMat = q1::imreadW(pathName.GetString());
	if (ocvMat.data == NULL)
		return false;

	*w = ocvMat.cols;
	*h = ocvMat.rows;
	ocvMat.release();

	return true;
}

const struct qcsc_info* MatFrmSrc::GetColorSpace(const CString &pathName,
	const struct qcsc_info* sortedCscInfo, bool doResize)
{
	UNREFERENCED_PARAMETER(doResize);

	Mat ocvMat = q1::imreadW(pathName.GetString());
	if (ocvMat.data == NULL)
		return NULL;

	ocvMat.release();
	return q1::image_find_cs(sortedCscInfo, "bgr888");
}

bool MatFrmSrc::IsAvailable()
{
	return mOcvMat.data != NULL;
}

bool MatFrmSrc::FillSceneBuf(BYTE* origBuf)
{
	if (mOcvMat.data == NULL)
		return false;

	memcpy(origBuf, mOcvMat.data, mPane->origSceneSize);

	// Auto-advance to the next file in the sequence so GetNextFrameID()
	// reports the frame that will be served by the *next* FillSceneBuf,
	// matching cv::VideoCapture::read()'s position semantics that the
	// timeline and NextScenes() rely on. Stays clamped at the last index
	// when there's no successor; NextScenes already guards against reading
	// past pane->frames - 1, so playback stops naturally.
	AdvanceTo(mCurFrameID + 1);

	return true;
}

long MatFrmSrc::GetNextFrameID()
{
	return mCurFrameID;
}

bool MatFrmSrc::SetNextFrameID(long frameID)
{
	// mOcvMat is in sync with mCurFrameID, so a no-op is genuinely a no-op
	// (used by SetScene → SetNextFrameID(curFrameID) repeats during play).
	if (frameID == mCurFrameID && mOcvMat.data != NULL)
		return true;

	return AdvanceTo(frameID);
}

bool MatFrmSrc::AdvanceTo(long startID)
{
	// Skip-forward on LoadFrame failure: if a file in the cached list went
	// missing or won't decode (deleted mid-session, permission flip, etc.),
	// step over it instead of pinning mCurFrameID. Without this, the no-op
	// shortcut in SetNextFrameID would lock the user out of advancing past
	// a bad frame — and a stuck cursor is worse than a one-frame jump.
	long id = startID;
	while (id < (long)mFileList.size()) {
		// Avoid re-imread'ing the frame we already hold — happens when a
		// skip-forward lands on the file that auto-advance pre-loaded.
		if (id == mCurFrameID && mOcvMat.data != NULL)
			return true;
		if (LoadFrame(id)) {
			mCurFrameID = id;
			return true;
		}
		id++;
	}

	// No loadable successor remains. Leave mOcvMat and mCurFrameID alone so
	// the last good frame keeps showing; NextScenes guards against advancing
	// past the last frame anyway.
	return false;
}

double MatFrmSrc::GetFps()
{
	return -1;
}
