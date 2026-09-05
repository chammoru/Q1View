#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include "VideoColorInfo.h"

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using Microsoft::WRL::ComPtr;

namespace {

class ComScope
{
public:
	ComScope() : mResult(::CoInitializeEx(nullptr, COINIT_MULTITHREADED)),
		mUninitialize(SUCCEEDED(mResult)) {}
	~ComScope() { if (mUninitialize) ::CoUninitialize(); }
	bool Available() const { return SUCCEEDED(mResult) || mResult == RPC_E_CHANGED_MODE; }
private:
	HRESULT mResult;
	bool mUninitialize;
};

} // namespace

namespace q1 {

bool IsBt2020HlgVideo(const std::wstring& path)
{
	ComScope com;
	if (!com.Available() || FAILED(::MFStartup(MF_VERSION)))
		return false;

	ComPtr<IMFSourceReader> reader;
	HRESULT hr = ::MFCreateSourceReaderFromURL(path.c_str(), nullptr, &reader);
	ComPtr<IMFMediaType> type;
	if (SUCCEEDED(hr))
		hr = reader->GetNativeMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &type);

	UINT32 transfer = MFVideoTransFunc_Unknown;
	UINT32 primaries = MFVideoPrimaries_Unknown;
	UINT32 matrix = MFVideoTransferMatrix_Unknown;
	UINT32 range = MFNominalRange_Unknown;
	const bool matches = SUCCEEDED(hr) &&
		SUCCEEDED(type->GetUINT32(MF_MT_TRANSFER_FUNCTION, &transfer)) &&
		SUCCEEDED(type->GetUINT32(MF_MT_VIDEO_PRIMARIES, &primaries)) &&
		SUCCEEDED(type->GetUINT32(MF_MT_YUV_MATRIX, &matrix)) &&
		SUCCEEDED(type->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &range)) &&
		transfer == MFVideoTransFunc_HLG &&
		primaries == MFVideoPrimaries_BT2020 &&
		matrix == MFVideoTransferMatrix_BT2020_10 &&
		range == MFNominalRange_16_235;

	reader.Reset();
	::MFShutdown();
	return matches;
}

} // namespace q1
