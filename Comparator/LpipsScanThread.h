#pragma once

#include "SThread.h"

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
	CComparatorDoc* m_doc;
	FileScanThread* m_baseScanner;
	int m_metricIdx;
	HWND m_hMainWnd;
};
