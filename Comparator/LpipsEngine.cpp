#include "stdafx.h"
#include "LpipsEngine.h"
#include <windows.h>
#include <vector>
#include <algorithm>

// ONNX Runtime ships only an x64 package here, and the C-API is reached through
// dynamic loading. Everything that touches ORT/OpenCV lives under _WIN64; a
// 32-bit build still compiles but reports available()==false (graceful degrade).
#ifdef _WIN64
#include <onnxruntime_c_api.h>
#include <opencv2/opencv.hpp>
#endif

LpipsEngine& LpipsEngine::getInstance()
{
	static LpipsEngine instance;
	return instance;
}

LpipsEngine::LpipsEngine()
	: m_available(false)
	, m_initialized(false)
	, m_hDll(nullptr)
#ifdef _WIN64
	, m_ortApi(nullptr)
	, m_env(nullptr)
	, m_session(nullptr)
	, m_memoryInfo(nullptr)
#endif
{
}

#ifdef _WIN64
LpipsEngine::~LpipsEngine()
{
	teardown();
}

void LpipsEngine::teardown()
{
	if (m_ortApi)
	{
		if (m_memoryInfo) { m_ortApi->ReleaseMemoryInfo(m_memoryInfo); m_memoryInfo = nullptr; }
		if (m_session) { m_ortApi->ReleaseSession(m_session); m_session = nullptr; }
		if (m_env) { m_ortApi->ReleaseEnv(m_env); m_env = nullptr; }
	}
	m_ortApi = nullptr;
	if (m_hDll)
	{
		FreeLibrary((HMODULE)m_hDll);
		m_hDll = nullptr;
	}
}

bool LpipsEngine::init(const std::wstring& modelPath)
{
	// A previous attempt already settled the outcome. Only a *successful* init
	// flips m_initialized, so a transient failure (e.g. the model had not been
	// downloaded yet) can be retried the next time LPIPS is selected.
	if (m_initialized)
		return m_available;

	// Load onnxruntime.dll from the executable's directory (bundled runtime).
	wchar_t exePath[MAX_PATH];
	GetModuleFileNameW(NULL, exePath, MAX_PATH);
	std::wstring exeDir = exePath;
	size_t slash = exeDir.find_last_of(L"\\/");
	if (slash != std::wstring::npos)
		exeDir = exeDir.substr(0, slash + 1);

	std::wstring dllPath = exeDir + L"onnxruntime.dll";
	m_hDll = LoadLibraryExW(dllPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
	if (!m_hDll)
		return false;

	const OrtApiBase* (ORT_API_CALL * OrtGetApiBaseFn)() =
		(const OrtApiBase* (ORT_API_CALL*)())GetProcAddress((HMODULE)m_hDll, "OrtGetApiBase");

	if (!OrtGetApiBaseFn)
	{
		teardown();
		return false;
	}

	const OrtApiBase* apiBase = OrtGetApiBaseFn();
	if (!apiBase)
	{
		teardown();
		return false;
	}

	m_ortApi = apiBase->GetApi(ORT_API_VERSION);
	if (!m_ortApi)
	{
		teardown();
		return false;
	}

	OrtStatus* status = m_ortApi->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "LpipsEngine", &m_env);
	if (status)
	{
		m_ortApi->ReleaseStatus(status);
		teardown();
		return false;
	}

	OrtSessionOptions* sessionOptions = nullptr;
	m_ortApi->CreateSessionOptions(&sessionOptions);
	m_ortApi->SetIntraOpNumThreads(sessionOptions, 1);
	m_ortApi->SetSessionGraphOptimizationLevel(sessionOptions, ORT_ENABLE_ALL);

	status = m_ortApi->CreateSession(m_env, modelPath.c_str(), sessionOptions, &m_session);
	m_ortApi->ReleaseSessionOptions(sessionOptions);

	if (status)
	{
		m_ortApi->ReleaseStatus(status);
		teardown();
		return false;
	}

	m_ortApi->CreateCpuMemoryInfo(OrtArenaAllocator, OrtMemTypeDefault, &m_memoryInfo);

	m_available = true;
	m_initialized = true;
	return true;
}

static void PreprocessImage(const unsigned char* bgr, int w, int h, int stride, std::vector<float>& outNCHW, int& targetW, int& targetH)
{
	cv::Mat src(h, w, CV_8UC3, (void*)bgr, stride);
	cv::Mat rgb;
	cv::cvtColor(src, rgb, cv::COLOR_BGR2RGB);

	int long_side = std::max(w, h);
	int short_side = std::min(w, h);
	float scale = 1.0f;
	if (long_side > 1024) scale = 1024.0f / long_side;
	else if (short_side < 64) scale = 64.0f / short_side;

	cv::Mat resized;
	if (scale != 1.0f) {
		cv::resize(rgb, resized, cv::Size(), scale, scale, cv::INTER_LINEAR);
	} else {
		resized = rgb;
	}
	
	targetW = resized.cols;
	targetH = resized.rows;

	// Normalize to [-1, 1] and HWC -> NCHW
	outNCHW.resize(1 * 3 * targetW * targetH);
	cv::Mat floatMat;
	resized.convertTo(floatMat, CV_32FC3, 1.0 / 127.5, -1.0);

	std::vector<cv::Mat> channels(3);
	cv::split(floatMat, channels);

	size_t planeSize = targetW * targetH;
	memcpy(outNCHW.data(), channels[0].data, planeSize * sizeof(float));
	memcpy(outNCHW.data() + planeSize, channels[1].data, planeSize * sizeof(float));
	memcpy(outNCHW.data() + 2 * planeSize, channels[2].data, planeSize * sizeof(float));
}

double LpipsEngine::distance(const unsigned char* rgbA, const unsigned char* rgbB, int w, int h, int stride)
{
	if (!m_available || !m_session)
		return 0.0;

	int targetW = 0, targetH = 0;
	std::vector<float> nchwA, nchwB;
	PreprocessImage(rgbA, w, h, stride, nchwA, targetW, targetH);
	PreprocessImage(rgbB, w, h, stride, nchwB, targetW, targetH);

	int64_t inputShape[] = { 1, 3, targetH, targetW };
	
	OrtValue* inputTensors[2] = { nullptr, nullptr };
	m_ortApi->CreateTensorWithDataAsOrtValue(m_memoryInfo, nchwA.data(), nchwA.size() * sizeof(float), inputShape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &inputTensors[0]);
	m_ortApi->CreateTensorWithDataAsOrtValue(m_memoryInfo, nchwB.data(), nchwB.size() * sizeof(float), inputShape, 4, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &inputTensors[1]);

	// Must match the names baked into the exported graph by
	// tools/lpips/export_lpips_onnx.py (input_names / output_names).
	const char* inputNames[] = { "img0", "img1" };
	const char* outputNames[] = { "dist" };
	OrtValue* outputTensor = nullptr;

	OrtStatus* status = m_ortApi->Run(m_session, nullptr, inputNames, (const OrtValue* const*)inputTensors, 2, outputNames, 1, &outputTensor);
	
	double dist = 0.0;
	if (!status && outputTensor)
	{
		float* outData = nullptr;
		m_ortApi->GetTensorMutableData(outputTensor, (void**)&outData);
		if (outData)
			dist = (double)outData[0];
	}

	if (status) {
		const char* msg = m_ortApi->GetErrorMessage(status);
		char err[512];
		snprintf(err, sizeof(err), "ONNX Run failed: %s\n", msg ? msg : "Unknown");
		OutputDebugStringA(err);
		m_ortApi->ReleaseStatus(status);
	}
	if (outputTensor) m_ortApi->ReleaseValue(outputTensor);
	if (inputTensors[0]) m_ortApi->ReleaseValue(inputTensors[0]);
	if (inputTensors[1]) m_ortApi->ReleaseValue(inputTensors[1]);

	return dist;
}
#else
LpipsEngine::~LpipsEngine() {}
bool LpipsEngine::init(const std::wstring& modelPath) { return false; }
double LpipsEngine::distance(const unsigned char* rgbA, const unsigned char* rgbB, int w, int h, int stride) { return 0.0; }
#endif
