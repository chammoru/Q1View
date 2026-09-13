#pragma once

#define Q1VIEW_VERSION_MAJOR 0
#define Q1VIEW_VERSION_MINOR 0
#define Q1VIEW_VERSION_PATCH 0
#define Q1VIEW_VERSION_BUILD 0

#define Q1VIEW_VERSION_STRING "0.0.0-dev"

#ifndef RC_INVOKED

#include <afxstr.h>
#include <stdio.h>
#include <tchar.h>
#include <vector>

inline CString Q1ViewGetProductVersion()
{
	TCHAR modulePath[MAX_PATH] = {0};
	if (::GetModuleFileName(NULL, modulePath, MAX_PATH) == 0)
		return _T(Q1VIEW_VERSION_STRING);

	DWORD unused = 0;
	DWORD versionInfoSize = ::GetFileVersionInfoSize(modulePath, &unused);
	if (versionInfoSize == 0)
		return _T(Q1VIEW_VERSION_STRING);

	std::vector<BYTE> versionInfo(versionInfoSize);
	if (!::GetFileVersionInfo(modulePath, 0, versionInfoSize, &versionInfo[0]))
		return _T(Q1VIEW_VERSION_STRING);

	struct Translation
	{
		WORD language;
		WORD codePage;
	};

	Translation *translations = NULL;
	UINT translationBytes = 0;
	if (!::VerQueryValue(&versionInfo[0], _T("\\VarFileInfo\\Translation"),
			reinterpret_cast<LPVOID *>(&translations), &translationBytes) ||
			translationBytes < sizeof(Translation)) {
		return _T(Q1VIEW_VERSION_STRING);
	}

	TCHAR query[80] = {0};
	_stprintf_s(query, _T("\\StringFileInfo\\%04x%04x\\ProductVersion"),
		translations[0].language, translations[0].codePage);

	LPTSTR productVersion = NULL;
	UINT productVersionChars = 0;
	if (!::VerQueryValue(&versionInfo[0], query,
			reinterpret_cast<LPVOID *>(&productVersion), &productVersionChars) ||
			productVersion == NULL || productVersionChars == 0) {
		return _T(Q1VIEW_VERSION_STRING);
	}

	return CString(productVersion);
}

#endif
