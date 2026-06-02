param(
    [string]$Version = "0.0.0-dev",

    [string]$OutputPath = ""
)

$ErrorActionPreference = "Stop"

function Split-Version {
    param([string]$Value)

    $clean = $Value.Trim()
    if ($clean.StartsWith("v")) {
        $clean = $clean.Substring(1)
    }

    if ($clean -match '^(\d+)\.(\d+)\.(\d+)(?:\.(\d+))?$') {
        return @{
            Major = [int]$Matches[1]
            Minor = [int]$Matches[2]
            Patch = [int]$Matches[3]
            Build = if ($Matches[4]) { [int]$Matches[4] } else { 0 }
            Display = "$($Matches[1]).$($Matches[2]).$($Matches[3])"
        }
    }

    if ($clean -match '^(\d+)\.(\d+)$') {
        return @{
            Major = [int]$Matches[1]
            Minor = [int]$Matches[2]
            Patch = 0
            Build = 0
            Display = "$($Matches[1]).$($Matches[2]).0"
        }
    }

    return @{
        Major = 0
        Minor = 0
        Patch = 0
        Build = 0
        Display = "0.0.0-dev"
    }
}

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $repoRoot "QCommon\inc\Q1ViewVersion.h"
}

$parsed = Split-Version -Value $Version
$msixVersion = "{0}.{1}.{2}.{3}" -f $parsed.Major, $parsed.Minor, $parsed.Patch, $parsed.Build

$content = @"
#pragma once

#define Q1VIEW_VERSION_MAJOR $($parsed.Major)
#define Q1VIEW_VERSION_MINOR $($parsed.Minor)
#define Q1VIEW_VERSION_PATCH $($parsed.Patch)
#define Q1VIEW_VERSION_BUILD $($parsed.Build)

#define Q1VIEW_VERSION_STRING "$($parsed.Display)"

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
"@

Set-Content -LiteralPath $OutputPath -Value $content -Encoding ascii
Write-Host "Wrote Q1View version $($parsed.Display) ($msixVersion) to $OutputPath"
