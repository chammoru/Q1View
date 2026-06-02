#pragma once

#include <atomic>
#include <vector>
#include <string>

class LpipsEngine
{
public:
	static LpipsEngine& getInstance();

	// Initializes the engine if not already initialized.
	// Returns true if initialization succeeded (model found & valid, DLL loaded).
	// Takes the path to the model file.
	bool init(const std::wstring& modelPath);

	bool available() const { return m_available.load(); }

	// Computes LPIPS distance between two RGB888 frames.
	// rgbA and rgbB are assumed to have the same dimensions.
	double distance(const unsigned char* rgbA, const unsigned char* rgbB, int w, int h, int stride);

private:
	LpipsEngine();
	~LpipsEngine();

	std::atomic<bool> m_available;
	std::atomic<bool> m_initialized;

	void* m_hDll;
#ifdef _WIN64
	const struct OrtApi* m_ortApi;
	struct OrtEnv* m_env;
	struct OrtSession* m_session;
	struct OrtMemoryInfo* m_memoryInfo;

	// Releases the ORT objects and unloads the DLL. Safe to call on a partially
	// initialized engine (used both by the destructor and to roll back a failed init).
	void teardown();
#endif
};
