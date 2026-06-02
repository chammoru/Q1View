#include "stdafx.h"
#include "MlModelProvisioner.h"
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <shobjidl.h>   // IProgressDialog
#include <bcrypt.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <iomanip>

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")

#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

static const wchar_t* LPIPS_URL = L"https://github.com/chammoru/Q1View/releases/download/models-lpips-v1/lpips_alex.onnx";
static const wchar_t* LPIPS_EXPECTED_SHA256 = L"6742F1268D6E3D577A91ABC7B8D21AA34F2F94072809046656E1EDF968166006";
static const wchar_t* LPIPS_FILE_NAME = L"lpips_alex.onnx";
static const wchar_t* LPIPS_VERSION_DIR = L"v1";

static std::wstring ComputeSHA256(const std::wstring& filePath)
{
	std::ifstream file(filePath.c_str(), std::ios::binary);
	if (!file.is_open()) return L"";

	BCRYPT_ALG_HANDLE hAlg = NULL;
	BCRYPT_HASH_HANDLE hHash = NULL;
	DWORD cbHashObject = 0;
	DWORD cbData = 0;
	DWORD cbHash = 0;
	std::vector<BYTE> pbHashObject;
	std::vector<BYTE> pbHash;
	std::wstring hashResult = L"";

	if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0))) goto Cleanup;
	if (!NT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD), &cbData, 0))) goto Cleanup;
	if (!NT_SUCCESS(BCryptGetProperty(hAlg, BCRYPT_HASH_LENGTH, (PBYTE)&cbHash, sizeof(DWORD), &cbData, 0))) goto Cleanup;
	
	pbHashObject.resize(cbHashObject);
	pbHash.resize(cbHash);

	if (!NT_SUCCESS(BCryptCreateHash(hAlg, &hHash, pbHashObject.data(), cbHashObject, NULL, 0, 0))) goto Cleanup;

	char buffer[8192];
	while (file.read(buffer, sizeof(buffer)))
	{
		if (!NT_SUCCESS(BCryptHashData(hHash, (PBYTE)buffer, (ULONG)file.gcount(), 0))) goto Cleanup;
	}
	if (file.gcount() > 0)
	{
		if (!NT_SUCCESS(BCryptHashData(hHash, (PBYTE)buffer, (ULONG)file.gcount(), 0))) goto Cleanup;
	}

	if (!NT_SUCCESS(BCryptFinishHash(hHash, pbHash.data(), cbHash, 0))) goto Cleanup;

	{
		std::wstringstream ss;
		for (size_t i = 0; i < pbHash.size(); ++i)
		{
			ss << std::hex << std::setw(2) << std::setfill(L'0') << std::uppercase << (int)pbHash[i];
		}
		hashResult = ss.str();
	}

Cleanup:
	if (hHash) BCryptDestroyHash(hHash);
	if (hAlg) BCryptCloseAlgorithmProvider(hAlg, 0);
	return hashResult;
}

// Downloads |url| to |destPath|. If |ppd| is non-null, drives its progress bar
// from the Content-Length and honors the dialog's Cancel button. Sets *cancelled
// to true if the user cancelled (so the caller can degrade silently).
static bool DownloadFileWinHttp(const std::wstring& url, const std::wstring& destPath,
	IProgressDialog* ppd, bool* cancelled)
{
	// All locals are declared up front so the goto-based cleanup below does not
	// cross any initialization (MSVC C2362).
	bool success = false;
	if (cancelled) *cancelled = false;
	HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
	DWORD dwOption = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
	DWORD statusCode = 0;
	DWORD dwSize = sizeof(statusCode);
	ULONGLONG total = 0;
	ULONGLONG downloaded = 0;

	URL_COMPONENTS urlComp = { 0 };
	urlComp.dwStructSize = sizeof(urlComp);

	wchar_t hostName[256] = { 0 };
	wchar_t urlPath[1024] = { 0 };
	urlComp.lpszHostName = hostName;
	urlComp.dwHostNameLength = _countof(hostName);
	urlComp.lpszUrlPath = urlPath;
	urlComp.dwUrlPathLength = _countof(urlPath);

	if (!WinHttpCrackUrl(url.c_str(), (DWORD)url.length(), 0, &urlComp))
		return false;

	hSession = WinHttpOpen(L"Q1View/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!hSession) goto Cleanup;

	// Follow GitHub's redirect to the release asset CDN.
	WinHttpSetOption(hSession, WINHTTP_OPTION_REDIRECT_POLICY, &dwOption, sizeof(dwOption));

	hConnect = WinHttpConnect(hSession, hostName, urlComp.nPort, 0);
	if (!hConnect) goto Cleanup;

	hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
		(urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
	if (!hRequest) goto Cleanup;

	if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) goto Cleanup;
	if (!WinHttpReceiveResponse(hRequest, NULL)) goto Cleanup;

	WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &dwSize, WINHTTP_NO_HEADER_INDEX);
	if (statusCode != 200) goto Cleanup;

	{
		// Total size for the progress bar (best effort; may be absent).
		wchar_t lenBuf[32] = { 0 };
		DWORD lenSize = sizeof(lenBuf);
		if (WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
			lenBuf, &lenSize, WINHTTP_NO_HEADER_INDEX))
			total = _wcstoui64(lenBuf, NULL, 10);
	}

	{
		std::ofstream outFile(destPath.c_str(), std::ios::binary);
		if (!outFile.is_open()) goto Cleanup;

		DWORD dwBytesAvailable = 0;
		DWORD dwBytesRead = 0;
		std::vector<char> buffer(65536);

		while (WinHttpQueryDataAvailable(hRequest, &dwBytesAvailable) && dwBytesAvailable > 0)
		{
			if (ppd && ppd->HasUserCancelled()) {
				if (cancelled) *cancelled = true;
				goto Cleanup;
			}

			if (dwBytesAvailable > buffer.size()) buffer.resize(dwBytesAvailable);
			if (WinHttpReadData(hRequest, buffer.data(), dwBytesAvailable, &dwBytesRead) && dwBytesRead > 0)
			{
				outFile.write(buffer.data(), dwBytesRead);
				downloaded += dwBytesRead;
				if (ppd)
					ppd->SetProgress64(downloaded, total ? total : downloaded + 1);
			}
			else break;
		}
		success = outFile.good();
	}

Cleanup:
	if (hRequest) WinHttpCloseHandle(hRequest);
	if (hConnect) WinHttpCloseHandle(hConnect);
	if (hSession) WinHttpCloseHandle(hSession);
	return success;
}

std::wstring MlModelProvisioner::provisionLpipsModel(HWND hParentWnd)
{
	wchar_t localAppData[MAX_PATH];
	if (!SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, localAppData)))
		return L"";

	std::wstring cacheDir = std::wstring(localAppData) + L"\\Q1View\\ml\\" + LPIPS_VERSION_DIR;
	std::wstring cachePath = cacheDir + L"\\" + LPIPS_FILE_NAME;

	// Check local dev path first (in models/ adjacent to the executable, or higher up)
	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(NULL, exePath, MAX_PATH);
	std::wstring exeDir = exePath;
	size_t slash = exeDir.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
		exeDir = exeDir.substr(0, slash + 1);

	const wchar_t* devPaths[] = {
		L"models\\lpips_alex.onnx",
		L"..\\models\\lpips_alex.onnx",
		L"..\\..\\models\\lpips_alex.onnx",
		L"..\\..\\..\\models\\lpips_alex.onnx"
	};

	for (int i = 0; i < 4; i++) {
		std::wstring testPath = exeDir + devPaths[i];
		if (GetFileAttributesW(testPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
			std::wstring hash = ComputeSHA256(testPath);
			if (hash == LPIPS_EXPECTED_SHA256)
				return testPath;
		}
	}

	// Check if already exists and valid
	if (GetFileAttributesW(cachePath.c_str()) != INVALID_FILE_ATTRIBUTES)
	{
		std::wstring hash = ComputeSHA256(cachePath);
		if (hash == LPIPS_EXPECTED_SHA256)
			return cachePath;
		// Invalid hash, delete it
		DeleteFileW(cachePath.c_str());
	}

	// Prompt user
	int resp = ::MessageBoxW(hParentWnd,
		L"LPIPS requires a one-time 4.7 MB download. Download now?",
		L"Q1View - Model Download Required",
		MB_YESNO | MB_ICONQUESTION
	);
	if (resp != IDYES)
		return L"";

	// Create directories
	SHCreateDirectoryExW(NULL, cacheDir.c_str(), NULL);

	std::wstring tempPath = cachePath + L".tmp";

	// Show the standard Windows progress dialog (own UI thread; driven from here
	// via SetProgress64). This runs on the LPIPS worker thread, so initialize COM
	// for it. CoCreateInstance failing just means we download without a bar.
	HRESULT hrCo = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	bool comInited = SUCCEEDED(hrCo);

	IProgressDialog* ppd = NULL;
	if (SUCCEEDED(CoCreateInstance(CLSID_ProgressDialog, NULL, CLSCTX_INPROC_SERVER,
		IID_PPV_ARGS(&ppd))))
	{
		ppd->SetTitle(L"Q1View - Downloading LPIPS model");
		ppd->SetLine(1, L"lpips_alex.onnx (one-time, ~4.7 MB)", FALSE, NULL);
		ppd->SetLine(2, L"Downloading the perceptual-metric model...", FALSE, NULL);
		ppd->StartProgressDialog(hParentWnd, NULL, PROGDLG_NORMAL | PROGDLG_AUTOTIME, NULL);
	}

	bool cancelled = false;
	bool dlOk = DownloadFileWinHttp(LPIPS_URL, tempPath, ppd, &cancelled);

	if (ppd)
	{
		ppd->StopProgressDialog();
		ppd->Release();
		ppd = NULL;
	}
	if (comInited)
		CoUninitialize();

	if (cancelled)
	{
		// User cancelled: degrade silently (metric stays unavailable).
		DeleteFileW(tempPath.c_str());
		return L"";
	}

	if (!dlOk)
	{
		::MessageBoxW(hParentWnd, L"Failed to download the LPIPS model. Please check your connection.", L"Error", MB_OK | MB_ICONERROR);
		DeleteFileW(tempPath.c_str());
		return L"";
	}

	std::wstring downloadedHash = ComputeSHA256(tempPath);
	if (downloadedHash != LPIPS_EXPECTED_SHA256)
	{
		::MessageBoxW(hParentWnd, L"Model validation failed (SHA-256 mismatch).", L"Error", MB_OK | MB_ICONERROR);
		DeleteFileW(tempPath.c_str());
		return L"";
	}

	// Atomic move
	if (!MoveFileExW(tempPath.c_str(), cachePath.c_str(), MOVEFILE_REPLACE_EXISTING))
	{
		::MessageBoxW(hParentWnd, L"Failed to save the downloaded model to cache.", L"Error", MB_OK | MB_ICONERROR);
		DeleteFileW(tempPath.c_str());
		return L"";
	}

	return cachePath;
}
