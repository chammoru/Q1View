#include "stdafx.h"
#include "AudioPlayer.h"
#include <mfapi.h>

#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

using Microsoft::WRL::ComPtr;

AudioPlayer::AudioPlayer()
{
    MFStartup(MF_VERSION, MFSTARTUP_LITE);
}

AudioPlayer::~AudioPlayer()
{
    Close();
    MFShutdown();
}

bool AudioPlayer::Open(const wchar_t* filePath)
{
    if (mOpenedPath == filePath && mHasAudio)
        return true;

    Close();
    mOpenedPath = filePath;

    ComPtr<IMFSourceReader> reader;
    if (FAILED(MFCreateSourceReaderFromURL(filePath, nullptr, &reader)))
        return false;

    // Select only the first audio stream; reject files with no audio.
    reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
    if (FAILED(reader->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE)))
        return false;

    // Request decoded PCM output.
    ComPtr<IMFMediaType> pType;
    MFCreateMediaType(&pType);
    pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    if (FAILED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pType.Get())))
        return false;

    // Read back the actual output format.
    ComPtr<IMFMediaType> pOut;
    if (FAILED(reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pOut)))
        return false;

    UINT32 channels = 0, sampleRate = 0, bitsPerSample = 0;
    pOut->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels);
    pOut->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &sampleRate);
    pOut->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bitsPerSample);
    if (!channels || !sampleRate || !bitsPerSample)
        return false;

    WAVEFORMATEX wfx = {};
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = static_cast<WORD>(channels);
    wfx.nSamplesPerSec  = sampleRate;
    wfx.wBitsPerSample  = static_cast<WORD>(bitsPerSample);
    wfx.nBlockAlign     = static_cast<WORD>(channels * bitsPerSample / 8);
    wfx.nAvgBytesPerSec = sampleRate * wfx.nBlockAlign;

    // Size each buffer for ~100ms of audio.
    const DWORD bytesPerBuf = (sampleRate / 10) * wfx.nBlockAlign;
    for (auto& buf : mBufs)
        buf.resize(bytesPerBuf);

    ComPtr<IXAudio2> xa;
    if (FAILED(XAudio2Create(&xa)))
        return false;

    IXAudio2MasteringVoice* master = nullptr;
    if (FAILED(xa->CreateMasteringVoice(&master)))
        return false;

    IXAudio2SourceVoice* src = nullptr;
    if (FAILED(xa->CreateSourceVoice(&src, &wfx))) {
        master->DestroyVoice();
        return false;
    }

    mReader      = reader;
    mXAudio2     = xa;
    mMasterVoice = master;
    mSourceVoice = src;
    mWfx         = wfx;
    mHasAudio    = true;
    ApplyVolume();
    return true;
}

void AudioPlayer::SetVolume(float volume)
{
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;
    mVolume = volume;
    ApplyVolume();
}

void AudioPlayer::SetMuted(bool muted)
{
    mMuted = muted;
    ApplyVolume();
}

void AudioPlayer::ApplyVolume()
{
    if (mSourceVoice)
        mSourceVoice->SetVolume(mMuted ? 0.0f : mVolume);
}

void AudioPlayer::Close()
{
    Pause();
    if (mSourceVoice) { mSourceVoice->FlushSourceBuffers(); mSourceVoice->DestroyVoice(); mSourceVoice = nullptr; }
    if (mMasterVoice) { mMasterVoice->DestroyVoice(); mMasterVoice = nullptr; }
    mXAudio2.Reset();
    mReader.Reset();
    mHasAudio = false;
    mOpenedPath.clear();
}

bool AudioPlayer::SeekReader(double timeSec)
{
    PROPVARIANT var = {};
    var.vt = VT_I8;
    var.hVal.QuadPart = static_cast<LONGLONG>(timeSec * 1e7); // 100-ns units
    HRESULT hr = mReader->SetCurrentPosition(GUID_NULL, var);
    return SUCCEEDED(hr);
}

void AudioPlayer::Play(double startTimeSec)
{
    if (!mHasAudio)
        return;

    Pause(); // stop any existing feed thread and drain the voice

    if (mSourceVoice)
        mSourceVoice->FlushSourceBuffers();

    if (!SeekReader(startTimeSec))
        return;

    mBufIdx      = 0;
    mFeedRunning = true;
    mFeedThread  = std::thread([this] { FeedLoop(); });
    mSourceVoice->Start(0);
}

void AudioPlayer::Pause()
{
    if (!mFeedRunning)
        return;

    mFeedRunning = false;
    if (mFeedThread.joinable())
        mFeedThread.join();

    if (mSourceVoice)
        mSourceVoice->Stop(0);
}

void AudioPlayer::FeedLoop()
{
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    while (mFeedRunning) {
        // Throttle: wait until XAudio2 has consumed at least one buffer.
        for (;;) {
            XAUDIO2_VOICE_STATE state;
            mSourceVoice->GetState(&state, XAUDIO2_VOICE_NOSAMPLESPLAYED);
            if (state.BuffersQueued < kBufCount)
                break;
            if (!mFeedRunning)
                goto done;
            Sleep(1);
        }

        DWORD flags = 0;
        ComPtr<IMFSample> pSample;
        HRESULT hr = mReader->ReadSample(
            MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0,
            nullptr, &flags, nullptr, &pSample);

        if (FAILED(hr) || (flags & MF_SOURCE_READERF_ENDOFSTREAM) || !pSample)
            break;

        ComPtr<IMFMediaBuffer> pBuf;
        pSample->ConvertToContiguousBuffer(&pBuf);
        BYTE* pData = nullptr;
        DWORD cbLen = 0;
        pBuf->Lock(&pData, nullptr, &cbLen);

        auto& slot = mBufs[mBufIdx % kBufCount];
        if (slot.size() < cbLen)
            slot.resize(cbLen);
        memcpy(slot.data(), pData, cbLen);
        pBuf->Unlock();

        XAUDIO2_BUFFER xb = {};
        xb.AudioBytes = cbLen;
        xb.pAudioData = slot.data();
        mSourceVoice->SubmitSourceBuffer(&xb);
        mBufIdx = (mBufIdx + 1) % kBufCount;
    }

done:
    CoUninitialize();
}
