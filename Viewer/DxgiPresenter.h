#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

// Presents a completed CPU-rendered BGRA frame through an HWND flip-model
// swap chain. All methods must be called on the UI thread.
class DxgiPresenter
{
public:
	DxgiPresenter();
	~DxgiPresenter();

	bool Present(HWND hwnd, const void *pixels, UINT width, UINT height, UINT rowPitch);
	void Reset();
	bool IsActive() const { return mSwapChain != nullptr; }

private:
	bool EnsureResources(HWND hwnd, UINT width, UINT height);
	bool CreateDevice();
	bool CreateSwapChain(HWND hwnd, UINT width, UINT height);
	bool Resize(UINT width, UINT height);
	bool CreateSizeDependentResources(UINT width, UINT height);
	bool Upload(const void *pixels, UINT rowPitch);
	bool Fail(const char *operation, HRESULT hr);

	Microsoft::WRL::ComPtr<ID3D11Device> mDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> mContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> mSwapChain;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> mUploadTexture;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> mBackBuffer;
	HWND mHwnd;
	UINT mWidth;
	UINT mHeight;
	bool mDisabled;
};
