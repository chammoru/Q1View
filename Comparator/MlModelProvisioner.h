#pragma once

#include <afxwin.h>
#include <string>

class MlModelProvisioner
{
public:
	// Returns the absolute path to the lpips_alex.onnx model.
	// If not found in the cache, it prompts the user to download it.
	// If the user accepts, downloads it to %LOCALAPPDATA%\Q1View\ml\v1\lpips_alex.onnx,
	// verifies the SHA-256 hash, and returns the path.
	// Returns an empty string on any failure, decline, or mismatch.
	static std::wstring provisionLpipsModel(HWND hParentWnd);
};
