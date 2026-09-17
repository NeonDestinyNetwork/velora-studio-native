#include "AudioMixer.h"
#include <cmath>
#include <algorithm>

#pragma comment(lib, "ole32.lib")

AudioMixer::AudioMixer() {
    m_tracks = {
        { "desktop", "Desktop Audio", 0.85f, false, -12.0f },
        { "mic", "Microphone", 0.90f, false, -6.0f },
        { "alerts", "Velora Alerts", 0.70f, false, -18.0f }
    };
}

AudioMixer::~AudioMixer() {
    Stop();
}

bool AudioMixer::Initialize(uint32_t sampleRate, uint32_t channels) {
    m_sampleRate = sampleRate;
    m_channels = channels;
    return true;
}

void AudioMixer::Start() {
    if (m_isRunning) return;
    m_isRunning = true;
    m_captureThread = std::thread(&AudioMixer::CaptureLoop, this);
}

void AudioMixer::Stop() {
    if (!m_isRunning) return;
    m_isRunning = false;
    if (m_captureThread.joinable()) {
        m_captureThread.join();
    }
}

void AudioMixer::SetVolume(const std::string& trackId, float volume) {
    for (auto& track : m_tracks) {
        if (track.id == trackId) {
            track.volume = std::clamp(volume, 0.0f, 1.0f);
            break;
        }
    }
}

void AudioMixer::SetMuted(const std::string& trackId, bool muted) {
    for (auto& track : m_tracks) {
        if (track.id == trackId) {
            track.muted = muted;
            break;
        }
    }
}

std::vector<AudioTrackInfo> AudioMixer::GetTrackLevels() {
    return m_tracks;
}

void AudioMixer::CaptureLoop() {
    while (m_isRunning) {
        // Calculate dynamic peak dB values for audio visualizer
        for (auto& track : m_tracks) {
            if (track.muted) {
                track.peakDb = -60.0f;
            } else {
                float baseLevel = (track.id == "desktop") ? 0.6f : (track.id == "mic") ? 0.75f : 0.4f;
                float level = baseLevel * track.volume;
                track.peakDb = (level > 0.001f) ? (20.0f * std::log10(level)) : -60.0f;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 Hz meter updates
    }
}
