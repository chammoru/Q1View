#pragma once
#include <mfidl.h>
#include <mfreadwrite.h>
#include <xaudio2.h>
#include <wrl/client.h>
#include <atomic>
#include <string>
#include <thread>
#include <array>
#include <vector>

// Plays the audio track of a video file alongside the video rendering loop.
// Open() must succeed before Play() is called. If the file has no audio track,
// Open() returns false and all other methods become no-ops.
//
// Thread safety: Open/Close/Play/Pause are called from the UI thread only.
// The internal feed thread submits buffers to XAudio2.
class AudioPlayer {
public:
    AudioPlayer();
    ~AudioPlayer();

    // Open the audio track of |filePath|. Returns false if there is no audio
    // track or if the format is unsupported. Re-opening the same path is cheap.
    bool Open(const wchar_t* filePath);
    void Close();
    bool IsOpen() const { return mHasAudio; }

    // Start (or restart) playback from |startTimeSec|. Stops any in-progress
    // playback first.
    void Play(double startTimeSec);
    // Pause playback. Call Play() to resume from a new position.
    void Pause();

private:
    bool SeekReader(double timeSec);
    void FeedLoop();

    Microsoft::WRL::ComPtr<IMFSourceReader> mReader;
    Microsoft::WRL::ComPtr<IXAudio2>        mXAudio2;
    IXAudio2MasteringVoice*                 mMasterVoice = nullptr;
    IXAudio2SourceVoice*                    mSourceVoice = nullptr;
    WAVEFORMATEX                            mWfx = {};

    std::wstring      mOpenedPath;
    bool              mHasAudio = false;

    std::thread       mFeedThread;
    std::atomic<bool> mFeedRunning{ false };

    // Rotating PCM buffer pool: at most kBufCount buffers in flight at once.
    static constexpr int kBufCount = 4;
    std::array<std::vector<BYTE>, kBufCount> mBufs;
    int mBufIdx = 0;
};
