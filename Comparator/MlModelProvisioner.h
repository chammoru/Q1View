#pragma once

#include <afxwin.h>
#include <string>

class MlModelProvisioner
{
public:
	// Returns the absolute path to the LPIPS ONNX model for the given backend
	// (mlId is "alex" or "vgg"). If not found in the dev path or the per-user
	// cache, it prompts the user to download it, fetches it to
	// %LOCALAPPDATA%\Q1View\ml\v1\lpips_<backend>.onnx, verifies the SHA-256
	// hash, and returns the path. Returns an empty string on any failure,
	// decline, mismatch, or an unknown backend id.
	static std::wstring provisionLpipsModel(HWND hParentWnd, const char* mlId);
};
