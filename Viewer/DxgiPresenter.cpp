#include "stdafx.h"
#include "DxgiPresenter.h"

#include "QDebug.h"

#include <cstring>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

using Microsoft::WRL::ComPtr;

DxgiPresenter::DxgiPresenter()
	: mHwnd(NULL)
	, mWidth(0)
	, mHeight(0)
	, mDisabled(::GetEnvironmentVariableW(L"Q1VIEW_DISABLE_DXGI", NULL, 0) != 0)
{
	if (mDisabled)
		LOGINF("%s", "DXGI presenter disabled by Q1VIEW_DISABLE_DXGI");
}

DxgiPresenter::~DxgiPresenter()
{
	Reset();
}

void DxgiPresenter::Reset()
{
	if (mContext)
		mContext->ClearState();

	mBackBuffer.Reset();
	mUploadTexture.Reset();
	mSwapChain.Reset();
	mContext.Reset();
	mDevice.Reset();
	mHwnd = NULL;
	mWidth = 0;
	mHeight = 0;
}

bool DxgiPresenter::Fail(const char *operation, HRESULT hr)
{
	LOGWRN("DXGI presenter %s failed (0x%08lx); using GDI fallback",
		operation, static_cast<unsigned long>(hr));
	Reset();
	mDisabled = true;
	return false;
}

bool DxgiPresenter::CreateDevice()
{
	static const D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1,
		D3D_FEATURE_LEVEL_10_0,
	};

	D3D_FEATURE_LEVEL selectedLevel = D3D_FEATURE_LEVEL_10_0;
	HRESULT hr = ::D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
		D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, _countof(levels),
		D3D11_SDK_VERSION, &mDevice, &selectedLevel, &mContext);
	if (FAILED(hr)) {
		hr = ::D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_WARP, NULL,
			D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, _countof(levels),
			D3D11_SDK_VERSION, &mDevice, &selectedLevel, &mContext);
	}

	return SUCCEEDED(hr) || Fail("device creation", hr);
}

bool DxgiPresenter::CreateSwapChain(HWND hwnd, UINT width, UINT height)
{
	ComPtr<IDXGIDevice> dxgiDevice;
	HRESULT hr = mDevice.As(&dxgiDevice);
	if (FAILED(hr))
		return Fail("device query", hr);

	ComPtr<IDXGIAdapter> adapter;
	hr = dxgiDevice->GetAdapter(&adapter);
	if (FAILED(hr))
		return Fail("adapter query", hr);

	ComPtr<IDXGIFactory2> factory;
	hr = adapter->GetParent(IID_PPV_ARGS(&factory));
	if (FAILED(hr))
		return Fail("factory query", hr);

	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = 2;
	desc.Scaling = DXGI_SCALING_STRETCH;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

	hr = factory->CreateSwapChainForHwnd(mDevice.Get(), hwnd, &desc, NULL, NULL,
		&mSwapChain);
	if (FAILED(hr)) {
		mSwapChain.Reset();
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		hr = factory->CreateSwapChainForHwnd(mDevice.Get(), hwnd, &desc, NULL, NULL,
			&mSwapChain);
	}
	if (FAILED(hr))
		return Fail("swap-chain creation", hr);

	factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
	mHwnd = hwnd;
	return CreateSizeDependentResources(width, height);
}

bool DxgiPresenter::CreateSizeDependentResources(UINT width, UINT height)
{
	HRESULT hr = mSwapChain->GetBuffer(0, IID_PPV_ARGS(&mBackBuffer));
	if (FAILED(hr))
		return Fail("back-buffer acquisition", hr);

	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	hr = mDevice->CreateTexture2D(&desc, NULL, &mUploadTexture);
	if (FAILED(hr))
		return Fail("upload-texture creation", hr);

	mWidth = width;
	mHeight = height;
	return true;
}

bool DxgiPresenter::Resize(UINT width, UINT height)
{
	mContext->ClearState();
	mBackBuffer.Reset();
	mUploadTexture.Reset();

	HRESULT hr = mSwapChain->ResizeBuffers(0, width, height,
		DXGI_FORMAT_UNKNOWN, 0);
	if (FAILED(hr))
		return Fail("swap-chain resize", hr);

	return CreateSizeDependentResources(width, height);
}

bool DxgiPresenter::EnsureResources(HWND hwnd, UINT width, UINT height)
{
	if (mDisabled || hwnd == NULL || width == 0 || height == 0)
		return false;

	if (!mDevice && !CreateDevice())
		return false;

	if (!mSwapChain || mHwnd != hwnd) {
		if (mSwapChain)
			Reset();
		if (!mDevice && !CreateDevice())
			return false;
		if (!CreateSwapChain(hwnd, width, height))
			return false;
		LOGINF("DXGI flip-model presenter active (%ux%u)", width, height);
		return true;
	}

	if (mWidth != width || mHeight != height)
		return Resize(width, height);

	return true;
}

bool DxgiPresenter::Upload(const void *pixels, UINT rowPitch)
{
	D3D11_MAPPED_SUBRESOURCE mapped = {};
	HRESULT hr = mContext->Map(mUploadTexture.Get(), 0,
		D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(hr))
		return Fail("texture mapping", hr);

	const BYTE *src = static_cast<const BYTE *>(pixels);
	BYTE *dst = static_cast<BYTE *>(mapped.pData);
	const UINT copyBytes = mWidth * 4;
	for (UINT y = 0; y < mHeight; ++y)
		std::memcpy(dst + y * mapped.RowPitch, src + y * rowPitch, copyBytes);

	mContext->Unmap(mUploadTexture.Get(), 0);
	mContext->CopyResource(mBackBuffer.Get(), mUploadTexture.Get());
	return true;
}

bool DxgiPresenter::Present(HWND hwnd, const void *pixels, UINT width,
	UINT height, UINT rowPitch)
{
	if (pixels == NULL || rowPitch < width * 4 ||
		!EnsureResources(hwnd, width, height)) {
		return false;
	}

	if (!Upload(pixels, rowPitch))
		return false;

	HRESULT hr = mSwapChain->Present(1, 0);
	if (FAILED(hr))
		return Fail("present", hr);

	return true;
}
