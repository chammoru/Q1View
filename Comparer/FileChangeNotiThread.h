#pragma once

#include <SThread.h>
#include <SMutex.h>
#include "MainFrm.h"

// Watches one file on disk and, when it is modified, posts WM_RELOAD_PANE to
// the supplied frame with the pane index in WPARAM. One instance per pane.
class FileChangeNotiThread : public SThread
{
public:
	FileChangeNotiThread();
	virtual ~FileChangeNotiThread(void);
	SmpError fire(CMainFrame *pFrame, CString pathName, int paneIdx);

private:
	int requestExitAndWait() { // do not call from outside, setup() will do
		return SThread::requestExitAndWait();
	}

	virtual SmpError run(const char* name = 0,
		int priority = STHREAD_PRIORITY_DEFAULT,
		int stack = 0) {
		return SThread::run();
	}

	virtual bool threadLoop();
	void cleanUp();

	HANDLE mChangeHandle;
	HWND mNotifyHwnd;
	CString mDirName;
	CString mFileName;
	int mPaneIdx;
	SMutex mStateLock;
};
