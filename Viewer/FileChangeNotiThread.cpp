#include "stdafx.h"
#include "FileChangeNotiThread.h"
#include <QDebug.h>

FileChangeNotiThread::FileChangeNotiThread()
: mChangeHandle(INVALID_HANDLE_VALUE)
, mPathName("")
{
}

FileChangeNotiThread::~FileChangeNotiThread(void)
{
	cleanUp();
}

void FileChangeNotiThread::cleanUp()
{
	requestExit();
	if (mChangeHandle != INVALID_HANDLE_VALUE) {
		::CancelIoEx(mChangeHandle, NULL);
		::CloseHandle(mChangeHandle);
		mChangeHandle = INVALID_HANDLE_VALUE;
	}
	requestExitAndWait();
}

SmpError FileChangeNotiThread::fire(CMainFrame *pFrame, CString pathName)
{
	CString oldDirName = mPathName.Left(mPathName.ReverseFind('\\') + 1);
	CString newDirName = pathName.Left(pathName.ReverseFind('\\') + 1);

	mFrame = pFrame;
	mPathName = pathName;

	if (oldDirName != newDirName) {
		cleanUp();

		mChangeHandle =	::CreateFile(newDirName, FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
			NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
		if (mChangeHandle == INVALID_HANDLE_VALUE) {
			LOGERR("mChangeHandle is invalid");
			return SMP_INVALID_RESOURCE;
		}
		SmpError err = run();
		if (err != SMP_OK) {
			LOGWRN("failed to run FileChangeNotiThread [%d]", err);
			return err;
		}
	}

	return SMP_OK;
}

bool FileChangeNotiThread::threadLoop()
{
	DWORD dwBytesReturned = 0;
	const DWORD dwBuffLength = 4096;
	BYTE buffer[dwBuffLength];
	::ReadDirectoryChangesW(mChangeHandle, buffer, dwBuffLength, FALSE,
		FILE_NOTIFY_CHANGE_LAST_WRITE, &dwBytesReturned, NULL, NULL);
	if (exitPending() || mChangeHandle == INVALID_HANDLE_VALUE)
		return false;
	DWORD dwNextEntryOffset = 0;
	PFILE_NOTIFY_INFORMATION pfni;
	WCHAR wchFileName[_MAX_PATH + 1];
	CString fileName;
	do {
		pfni = PFILE_NOTIFY_INFORMATION(buffer + dwNextEntryOffset);
		switch(pfni->Action) {
		case FILE_ACTION_MODIFIED:
			memcpy(wchFileName, pfni->FileName, pfni->FileNameLength);
			wchFileName[pfni->FileNameLength / sizeof(WCHAR)] = L'\0';
			fileName = mPathName.Mid(mPathName.ReverseFind('\\') + 1);
			if (fileName == wchFileName)
				mFrame->PostMessage(WM_RELOAD);
			break;
		default:
			LOGERR("Unhandled: System Error Code [%d]", GetLastError());
			break;
		}
		dwNextEntryOffset += pfni->NextEntryOffset;
	} while (pfni && pfni->NextEntryOffset != 0);

	return true;
}
