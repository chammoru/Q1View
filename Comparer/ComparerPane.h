#pragma once

#include "qimage_cs.h"

class CComparerView;

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <QDebug.h>

#include "FrmSrc.h"
#include "VidCapFrmSrc.h"
#include "MatFrmSrc.h"
#include "RawFrmSrc.h"

struct SQPane
{
	size_t origSceneSize;
	size_t origBufSize;
	BYTE *origBuf;
	QIMAGE_CS colorSpace;
	QIMAGE_CS_INFO_FN csLoadInfo;
	QIMAGE_CSC_FN csc2yuv420;
	BYTE *rgbBuf;
	BYTE *yuv420Buf;
	size_t rgbBufSize;
	size_t yuv420BufSize;
	QIMAGE_CSC_FN csc2rgb888;
	long curFrameID;

	std::vector<FrmSrc*> frmSrcs;
	FrmSrc* frmSrc;

	inline SQPane()
	: origSceneSize(0)
	, origBufSize(0)
	, origBuf(NULL)
	, colorSpace(qcsc_info_table[QIMG_DEF_CS_IDX].cs)
	, csLoadInfo(qcsc_info_table[QIMG_DEF_CS_IDX].cs_load_info)
	, csc2yuv420(NULL)
	, rgbBuf(NULL)
	, yuv420Buf(NULL)
	, rgbBufSize(0)
	, yuv420BufSize(0)
	, csc2rgb888(qcsc_info_table[QIMG_DEF_CS_IDX].csc2rgb888)
	, curFrameID(-1L)
	, frmSrc(NULL)
	{
		// ADD MORE FRAME SOURCES, IF NEEDED
		frmSrcs.push_back(new MatFrmSrc(this));
		frmSrcs.push_back(new VidCapFrmSrc(this));
		frmSrcs.push_back(new RawFrmSrc(this));
	}

	inline virtual ~SQPane()
	{
		closeFrmSrcs();

		for (auto it = std::begin(frmSrcs); it != std::end(frmSrcs); it++)
			delete* it;

		if (origBuf)
			delete [] origBuf;

		if (rgbBuf)
			delete [] rgbBuf;

		if (yuv420Buf)
			delete [] yuv420Buf;
	}

	inline void SetColorInfo(const struct qcsc_info * const ci)
	{
		colorSpace = ci->cs;
		csc2rgb888 = ci->csc2rgb888;
		csLoadInfo = ci->cs_load_info;

		if ((ci->cs & QIMAGE_CS_YUV) && ci->csc2yuv420)
			csc2yuv420 = ci->csc2yuv420;
		else
			csc2yuv420 = NULL;
	}

	void OpenFrmSrc(const CString &pathName)
	{
		closeFrmSrcs();

		auto it = std::begin(frmSrcs);
		for (; it != std::end(frmSrcs); it++) {
			bool ok = (*it)->Open(pathName);
			if (ok) {
				frmSrc = *it;
				break;
			}
		}
		ASSERT(it != std::end(frmSrcs));
	}

	void closeFrmSrcs() {
		for (auto it = std::begin(frmSrcs); it != std::end(frmSrcs); it++) {
			if ((*it)->IsAvailable())
				(*it)->Release();
		}
		frmSrc = NULL;
	}

	inline bool GetResolution(CString &pathName, int* w, int* h) {
		for (auto it = std::begin(frmSrcs); it != std::end(frmSrcs); it++) {
			bool ok = (*it)->GetResolution(pathName, w, h);
			if (ok) {
				return true;
			}
		}

		return false;
	}

	inline const struct qcsc_info* GetColorSpace(CString &pathName,
			struct qcsc_info* sortedCscInfo) {
		for (auto it = std::begin(frmSrcs); it != std::end(frmSrcs); it++) {
			const qcsc_info* ci = (*it)->GetColorSpace(pathName, sortedCscInfo);
			if (ci != NULL)
				return ci;
		}

		return NULL;
	}

	inline bool isAvail() const
	{
		return frmSrc && frmSrc->IsAvailable();
	}

	inline bool FillSceneBuf(BYTE* origBuf) {
		long candidateCurFrameID = frmSrc->GetNextFrameID();
		bool ok = frmSrc->FillSceneBuf(origBuf);
		if (ok)
			curFrameID = candidateCurFrameID;
		return ok;
	}
};

struct ComparerPane : public SQPane
{
	CString pathName;
	int bufOffset2, bufOffset3;
	CComparerView *pView;

	long frames;

	inline virtual ~ComparerPane()
	{
	}

	inline ComparerPane()
	: pathName(_T(""))
	, bufOffset2(0)
	, bufOffset3(0)
	, pView(NULL)
	, frames(0)
	{
	}

	void OpenFrmSrc()
	{
		SQPane::OpenFrmSrc(pathName);
		frames = QMAX(frmSrc->GetFrameNum(), 1);
	}

	inline void Release()
	{
		closeFrmSrcs();
		frames = 0;
		pathName.Empty();
	}

	inline long GetNextFrameID()
	{
		if (frmSrc)
			return frmSrc->GetNextFrameID();

		return 0L;
	}

	inline bool SetNextFrameID(long nextFrameID)
	{
		if (frmSrc) {
			if (nextFrameID < frames) {
				return frmSrc->SetNextFrameID(nextFrameID);
			} else {
				frmSrc->SetNextFrameID(curFrameID); // repeat current frame
			}
		}

		return false;
	}

	inline double GetFps()
	{
		return frmSrc->GetFps();
	}
};
