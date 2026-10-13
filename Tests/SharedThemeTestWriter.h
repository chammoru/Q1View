#pragma once
#include <string>

// A separate process writes the isolated shared key and broadcasts the real
// production notification. The receiving app must update without WM_COMMAND,
// activation, a manual repaint, or changing the user's production preferences.
inline bool WriteSharedThemeFromChild(const wchar_t* registryRoot, unsigned theme)
{
	wchar_t executable[MAX_PATH] = {};
	if (!GetFullPathNameW(L"Tests\\bin\\x64\\Release\\WindowsUiTypographyTests.exe", MAX_PATH, executable, nullptr)) return false;
	std::wstring command = L"\"" + std::wstring(executable) + L"\" --shared-theme-write \"" + registryRoot + L"\" " + std::to_wstring(theme);
	STARTUPINFOW startup = {}; startup.cb = sizeof(startup);
	PROCESS_INFORMATION process = {};
	if (!CreateProcessW(executable, &command[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return false;
	const DWORD wait = WaitForSingleObject(process.hProcess, 5000);
	DWORD result = 1;
	if (wait == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &result);
	else { TerminateProcess(process.hProcess, 2); WaitForSingleObject(process.hProcess, 1000); }
	CloseHandle(process.hThread); CloseHandle(process.hProcess);
	return wait == WAIT_OBJECT_0 && result == 0;
}
