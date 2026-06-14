#pragma once

#include <atomic>
#include <mutex>
#include <vector>
#include <string>

class LpipsEngine
{
public:
	static LpipsEngine& getInstance();

	// Loads the ONNX model at |modelPath| for backend |backendId| ("alex"/"vgg").
	// The engine holds a single session; selecting a different backend reloads it.
	// Returns true once the requested backend's model is loaded and ready.
	// Thread-safe: serialized against distance() so a reload never frees the
	// session out from under an in-flight inference.
	bool init(const std::wstring& modelPath, const std::string& backendId);

	// True once any model is loaded. Prefer availableFor() when the caller needs
	// a specific backend, since the loaded model may lag a just-changed selection.
	bool available() const { return m_available.load(); }

	// True only when the currently loaded model is the requested backend's.
	bool availableFor(const std::string& backendId) const;

	// Computes LPIPS distance between two RGB888 frames.
	// rgbA and rgbB are assumed to have the same dimensions.
	double distance(const unsigned char* rgbA, const unsigned char* rgbB, int w, int h, int stride);

private:
	LpipsEngine();
	~LpipsEngine();

	std::atomic<bool> m_available;
	std::atomic<bool> m_initialized;

	// Serializes init()/distance() so a backend switch (which releases and
	// recreates the ORT session) can never race an in-flight inference.
	mutable std::mutex m_mutex;
	// The model/backend currently loaded, so init() can detect a backend switch
	// and availableFor() can answer which backbone is live. Guarded by m_mutex.
	std::wstring m_modelPath;
	std::string m_backendId;

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
