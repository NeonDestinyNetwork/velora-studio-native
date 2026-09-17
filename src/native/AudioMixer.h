#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <wrl/client.h>
#include <string>
#include <vector>
#include <atomic>
#include <thread>

using Microsoft::WRL::ComPtr;

struct AudioTrackInfo {
    std::string id;
    std::string name;
    float volume = 1.0f;
    bool muted = false;
    float peakDb = -60.0f;
};

class AudioMixer {
public:
    AudioMixer();
    ~AudioMixer();

    bool Initialize(uint32_t sampleRate = 48000, uint32_t channels = 2);
    void Start();
    void Stop();

    void SetVolume(const std::string& trackId, float volume);
    void SetMuted(const std::string& trackId, bool muted);

    std::vector<AudioTrackInfo> GetTrackLevels();

private:
    void CaptureLoop();

    uint32_t m_sampleRate = 48000;
    uint32_t m_channels = 2;
    std::atomic<bool> m_isRunning{ false };
    std::thread m_captureThread;

    std::vector<AudioTrackInfo> m_tracks;

    ComPtr<IAudioClient> m_audioClient;
    ComPtr<IAudioCaptureClient> m_captureClient;
};
