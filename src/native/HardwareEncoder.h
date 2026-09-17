#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdint>

enum class MediaType {
    Video,
    Audio
};

struct EncoderConfig {
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t fps = 60;
    uint32_t bitrateKbps = 6000;
    uint32_t keyframeIntervalSec = 2; // Strict 2.0s GOP
    uint32_t bFrames = 0;             // 0 B-frames for real-time ingest
    std::string codec = "h264";
};

struct EncodedPacket {
    std::shared_ptr<const std::vector<uint8_t>> data;
    int64_t pts = 0;      // Presentation timestamp in microseconds
    int64_t dts = 0;      // Decode timestamp in microseconds
    int64_t duration = 0; // Duration in microseconds (e.g. 16666 us for 60fps video)
    bool isKeyframe = false;
    MediaType type = MediaType::Video;
};

#include <chrono>
#include <mutex>
#include <atomic>

class HardwareEncoder {
public:
    HardwareEncoder();
    ~HardwareEncoder();

    bool Initialize(ID3D11Device* device, const EncoderConfig& config);
    bool EncodeFrame(ID3D11Texture2D* texture, int64_t timestampUs, std::vector<EncodedPacket>& outPackets);
    void RequestKeyframe();
    void Flush();

private:
    ID3D11Device* m_device = nullptr;
    EncoderConfig m_config;
    bool m_isInitialized = false;
    int64_t m_frameIndex = 0;
    std::atomic<bool> m_forceKeyframe{ false };
    std::chrono::steady_clock::time_point m_lastKeyframeRequestTime;
    mutable std::mutex m_keyframeMutex;
};
