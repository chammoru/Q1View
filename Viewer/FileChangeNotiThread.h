#pragma once

#include <SThread.h>
#include "MainFrm.h"

class FileChangeNotiThread : public SThread
{
public:
	FileChangeNotiThread();
	virtual ~FileChangeNotiThread(void);
	SmpError fire(CMainFrame *pFrame, CString pathName);

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
	CMainFrame *mFrame;
	CString mPathName;
};
