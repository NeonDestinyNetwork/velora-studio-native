#pragma once

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#else
// Linux cross-platform forward declarations
struct ID3D11Device;
struct ID3D11Texture2D;
#endif

#include <string>
#include <vector>
#include <memory>
#include <cstdint>
#include <chrono>
#include <mutex>
#include <atomic>

enum class MediaType {
    Video,
    Audio
};

enum class CodecType {
    H264,
    HEVC,
    AV1,
    AAC,
    Opus
};

struct VideoEncodeProfile {
    CodecType codec = CodecType::H264;
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t fps = 60;
    uint32_t bitrateBps = 6000000;
    float keyframeIntervalSec = 2.0f;
    uint32_t bFrames = 0;
};

struct AudioEncodeProfile {
    CodecType codec = CodecType::AAC;
    uint32_t sampleRate = 48000;
    uint32_t channels = 2;
    uint32_t bitrateBps = 160000;
};

struct EncoderConfig {
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t fps = 60;
    uint32_t bitrateKbps = 6000;
    uint32_t keyframeIntervalSec = 2; // Strict 2.0s GOP
    uint32_t bFrames = 0;             // 0 B-frames for real-time ingest
    CodecType codec = CodecType::H264;
};

struct VideoCodecConfig {
    CodecType codec = CodecType::H264;
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t fps = 60;
    std::string profile = "High";
    std::string level = "4.2";
};

struct AudioCodecConfig {
    CodecType codec = CodecType::Opus;
    uint32_t sampleRate = 48000;
    uint32_t channels = 2;
    uint32_t payloadType = 111; // Standard dynamic payload type for Opus
    uint32_t clockRate = 48000;
};

struct EncodedPacket {
    std::shared_ptr<const std::vector<uint8_t>> data;
    int64_t pts = 0;      // Presentation timestamp in canonical microseconds
    int64_t dts = 0;      // Decode timestamp in canonical microseconds
    int64_t duration = 0; // Duration in canonical microseconds (e.g. 16666 us for 60fps video)
    bool isKeyframe = false;
    MediaType type = MediaType::Video;
    CodecType codec = CodecType::H264;
};

struct GopTelemetry {
    float configuredGopSec = 2.0f;
    float observedGopSec = 2.0f;
    uint32_t bFrames = 0;
    uint32_t totalKeyframes = 0;
    uint32_t forcedRecoveryKeyframes = 0;
    bool isCompliant = true;
};

class HardwareEncoder {
public:
    HardwareEncoder();
    ~HardwareEncoder();

    bool Initialize(ID3D11Device* device, const EncoderConfig& config);
    bool EncodeFrame(ID3D11Texture2D* texture, int64_t timestampUs, std::vector<EncodedPacket>& outPackets);
    void RequestKeyframe();
    void Flush();
    GopTelemetry GetGopTelemetry() const;

private:
    ID3D11Device* m_device = nullptr;
    EncoderConfig m_config;
    bool m_isInitialized = false;
    int64_t m_frameIndex = 0;
    std::atomic<bool> m_forceKeyframe{ false };
    std::chrono::steady_clock::time_point m_lastKeyframeRequestTime;
    mutable std::mutex m_keyframeMutex;

    // Measured GOP Telemetry Tracking
    int64_t m_lastKeyframePtsUs = 0;
    float m_observedGopSec = 2.0f;
    uint32_t m_totalKeyframes = 0;
    uint32_t m_forcedRecoveryKeyframes = 0;
};
