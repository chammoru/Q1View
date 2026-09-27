#include "stdafx.h"
#include "FileChangeNotiThread.h"
#include <QDebug.h>

FileChangeNotiThread::FileChangeNotiThread()
: mChangeHandle(INVALID_HANDLE_VALUE)
, mNotifyHwnd(NULL)
, mDirName("")
, mFileName("")
{
}

FileChangeNotiThread::~FileChangeNotiThread(void)
{
	cleanUp();
}

void FileChangeNotiThread::cleanUp()
{
	++mGeneration;
	requestExit();

	HANDLE changeHandle = INVALID_HANDLE_VALUE;
	{
		SMutex::Autolock lock(mStateLock);
		changeHandle = mChangeHandle;
		mChangeHandle = INVALID_HANDLE_VALUE;
	}

	if (changeHandle != INVALID_HANDLE_VALUE) {
		::CancelIoEx(changeHandle, NULL);
		::CloseHandle(changeHandle);
	}

	requestExitAndWait();
}

void FileChangeNotiThread::Stop()
{
	cleanUp();
	SMutex::Autolock lock(mStateLock);
	mDirName.Empty();
	mFileName.Empty();
	mNotifyHwnd = nullptr;
}

SmpError FileChangeNotiThread::fire(CMainFrame *pFrame, CString pathName)
{
	Stop();
	CString newDirName = pathName.Left(pathName.ReverseFind('\\') + 1);
	CString newFileName = pathName.Mid(pathName.ReverseFind('\\') + 1);
	HWND notifyHwnd = pFrame != NULL ? pFrame->GetSafeHwnd() : NULL;

	HANDLE changeHandle = ::CreateFile(newDirName, FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (changeHandle == INVALID_HANDLE_VALUE) {
		LOGERR("mChangeHandle is invalid");
		return SMP_INVALID_RESOURCE;
	}

	{
		SMutex::Autolock lock(mStateLock);
		mNotifyHwnd = notifyHwnd;
		mDirName = newDirName;
		mFileName = newFileName;
		mChangeHandle = changeHandle;
	}

	SmpError err = run();
	if (err != SMP_OK) {
		{
			SMutex::Autolock lock(mStateLock);
			changeHandle = mChangeHandle;
			mChangeHandle = INVALID_HANDLE_VALUE;
		}
		::CloseHandle(changeHandle);
		LOGWRN("failed to run FileChangeNotiThread [%d]", err);
		return err;
	}
	return SMP_OK;
}

bool FileChangeNotiThread::threadLoop()
{
	HANDLE changeHandle = INVALID_HANDLE_VALUE;
	unsigned generation;
	HWND notifyHwnd;
	CString watchedFileName;
	{
		SMutex::Autolock lock(mStateLock);
		changeHandle = mChangeHandle;
		generation = mGeneration.load();
		notifyHwnd = mNotifyHwnd;
		watchedFileName = mFileName;
	}
	if (changeHandle == INVALID_HANDLE_VALUE)
		return false;

	DWORD dwBytesReturned = 0;
	const DWORD dwBuffLength = 4096;
	BYTE buffer[dwBuffLength];
	BOOL ok = ::ReadDirectoryChangesW(changeHandle, buffer, dwBuffLength, FALSE,
		FILE_NOTIFY_CHANGE_LAST_WRITE, &dwBytesReturned, NULL, NULL);
	if (!ok || exitPending())
		return false;

	DWORD dwNextEntryOffset = 0;
	PFILE_NOTIFY_INFORMATION pfni;
	do {
		pfni = PFILE_NOTIFY_INFORMATION(buffer + dwNextEntryOffset);
		switch (pfni->Action) {
		case FILE_ACTION_MODIFIED:
		{
			CString changedFileName(pfni->FileName,
				static_cast<int>(pfni->FileNameLength / sizeof(WCHAR)));
			if (watchedFileName == changedFileName && notifyHwnd != NULL)
				::PostMessage(notifyHwnd, WM_RELOAD, generation, 1);
			break;
		}
		default:
			LOGERR("Unhandled: System Error Code [%d]", GetLastError());
			break;
		}
		dwNextEntryOffset += pfni->NextEntryOffset;
	} while (pfni && pfni->NextEntryOffset != 0);

	return true;
}
