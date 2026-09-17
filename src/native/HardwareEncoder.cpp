#include "HardwareEncoder.h"
#include <iostream>

HardwareEncoder::HardwareEncoder() = default;

HardwareEncoder::~HardwareEncoder() = default;

bool HardwareEncoder::Initialize(ID3D11Device* device, const EncoderConfig& config) {
    m_device = device;
    m_config = config;
    m_isInitialized = true;
    m_frameIndex = 0;
    m_forceKeyframe = true; // First frame must be an IDR
    m_lastKeyframeRequestTime = std::chrono::steady_clock::now();
    return true;
}

void HardwareEncoder::RequestKeyframe() {
    std::lock_guard<std::mutex> lock(m_keyframeMutex);
    auto now = std::chrono::steady_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastKeyframeRequestTime).count();
    
    // Coalesce IDR requests across multiple outputs within a 500ms window to prevent keyframe storms
    if (!m_forceKeyframe.load() && elapsedMs >= 500) {
        m_forceKeyframe.store(true);
        m_lastKeyframeRequestTime = now;
    }
}

bool HardwareEncoder::EncodeFrame(ID3D11Texture2D* texture, int64_t timestampUs, std::vector<EncodedPacket>& outPackets) {
    if (!m_isInitialized || !texture) return false;

    uint32_t gopFrameInterval = m_config.fps * m_config.keyframeIntervalSec;
    bool isKeyframe = m_forceKeyframe || ((m_frameIndex % gopFrameInterval) == 0);
    m_forceKeyframe = false;

    EncodedPacket packet;
    packet.pts = timestampUs;
    packet.dts = timestampUs;
    packet.duration = 1000000 / m_config.fps; // 16,666 us at 60 FPS
    packet.isKeyframe = isKeyframe;
    packet.type = MediaType::Video;

    // Simulated compressed bitstream payload
    auto buffer = std::make_shared<std::vector<uint8_t>>(isKeyframe ? 32768 : 8192, 0xAA);
    packet.data = buffer;

    outPackets.push_back(std::move(packet));
    m_frameIndex++;

    return true;
}

void HardwareEncoder::Flush() {
    m_frameIndex = 0;
    m_forceKeyframe = true;
}
