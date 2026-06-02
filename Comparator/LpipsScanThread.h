#pragma once

#include "SThread.h"
#include "qimage_cs.h"

class CComparatorDoc;
class FileScanThread;

class LpipsScanThread : public SThread
{
public:
	LpipsScanThread(CComparatorDoc* doc, FileScanThread* baseScanner, int metricIdx);
	virtual ~LpipsScanThread();

	// Exposes SThread's protected running flag so the Doc/MainFrame can tell
	// whether the lazy scan is still in progress.
	bool isRunning() const { return SThread::isRunning(); }

protected:
	virtual bool threadLoop() override;

private:
	struct PaneSnapshot
	{
		CString pathName;
		int srcW;
		int srcH;
		size_t origSceneSize;
		QIMAGE_CS colorSpace;
		QIMAGE_CS_INFO_FN csLoadInfo;
		QIMAGE_CSC_FN csc2yuv420;
		QIMAGE_CSC_FN csc2rgb888;
	};

	CComparatorDoc* m_doc;
	FileScanThread* m_baseScanner;
	int m_metricIdx;
	HWND m_hMainWnd;
	int m_w;
	int m_h;
	int m_frames;
	PaneSnapshot m_pane[2];
};
