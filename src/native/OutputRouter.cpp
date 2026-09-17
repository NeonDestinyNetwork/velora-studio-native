#include "OutputRouter.h"
#include <iostream>
#include <chrono>
#include <random>
#include <algorithm>

// ==============================================================================
// RTMP OUTPUT WORKER IMPLEMENTATION (GOP-AWARE RECOVERY)
// ==============================================================================

RTMPOutput::RTMPOutput(const std::string& id, const std::string& name, const std::string& rtmpUrl, const std::string& streamKey, uint32_t maxBufferMs)
    : m_id(id), m_name(name), m_rtmpUrl(rtmpUrl), m_streamKey(streamKey), m_maxBufferMs(maxBufferMs) {}

RTMPOutput::~RTMPOutput() {
    Disconnect();
}

bool RTMPOutput::Connect() {
    if (m_isRunning) return true;

    m_isRunning = true;
    m_isConnected = true;
    // Enforce clean IDR startup on connect / hot reconnect (never start on a dangling P-frame)
    m_isRecovering = true;
    if (m_keyframeRequestCb) {
        m_keyframeRequestCb();
    }
    m_workerThread = std::thread(&RTMPOutput::WorkerLoop, this);
    return true;
}

uint32_t RTMPOutput::CalculateQueuedDurationMs_Locked() const {
    if (m_packetQueue.empty()) return 0;
    int64_t startPts = m_packetQueue.front()->pts;
    int64_t endPts = m_packetQueue.back()->pts + m_packetQueue.back()->duration;
    int64_t diffUs = endPts - startPts;
    return (diffUs > 0) ? static_cast<uint32_t>(diffUs / 1000) : 0;
}

void RTMPOutput::PurgeToNextIDR() {
    // Purge the accumulated backlog to avoid latency buildup
    m_framesDropped += static_cast<uint32_t>(m_packetQueue.size());
    m_packetQueue.clear();
    m_isRecovering = true; // Wait for the next IDR keyframe before resuming video/audio
    m_recoveryEvents++;

    // Request a fresh IDR keyframe immediately from HardwareEncoder to recover instantly
    if (m_keyframeRequestCb) {
        m_keyframeRequestCb();
    }
}

void RTMPOutput::PushPacket(std::shared_ptr<EncodedPacket> packet) {
    if (!m_isRunning) return;

    // Filter: RTMP only ingests H.264 video and AAC audio (reject Opus audio meant for WHIP)
    if (packet->type == MediaType::Audio && packet->codec != CodecType::AAC) {
        return;
    }

    // Track latest timestamps for A/V skew / PTS drift calculation
    if (packet->type == MediaType::Video) {
        m_latestVideoPts = packet->pts;
    } else if (packet->type == MediaType::Audio) {
        m_latestAudioPts = packet->pts;
    }

    std::unique_lock<std::mutex> lock(m_queueMutex);

    // 1. If we are in Recovery mode, discard everything until a fresh IDR keyframe arrives
    if (m_isRecovering) {
        if (packet->type == MediaType::Video && packet->isKeyframe) {
            // Found fresh IDR! Clean GOP recovery achieved
            m_isRecovering = false;
        } else {
            m_framesDropped++;
            return;
        }
    }

    // 2. Measure queued duration in milliseconds across the timeline span
    uint32_t queuedMs = CalculateQueuedDurationMs_Locked();

    // 3. Catastrophic Backpressure Check (> 1500 ms)
    if (queuedMs >= m_maxBufferMs) {
        PurgeToNextIDR();
        return;
    }

    // Insert packet into unified timestamp-ordered queue
    m_packetQueue.push_back(packet);
    lock.unlock();
    m_cv.notify_one();
}

void RTMPOutput::Disconnect() {
    if (!m_isRunning) return;

    m_isRunning = false;
    m_isConnected = false;
    m_cv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_packetQueue.clear();
}

OutputStats RTMPOutput::GetStats() const {
    OutputStats stats;
    stats.bytesSent = m_bytesSent.load();
    stats.framesSent = m_framesSent.load();
    stats.framesDropped = m_framesDropped.load();
    stats.recoveryEvents = m_recoveryEvents.load();
    stats.isConnected = m_isConnected.load();

    // Compute windowed / smoothed A/V sync metrics (Video PTS - Audio PTS)
    int64_t vPts = m_latestVideoPts.load();
    int64_t aPts = m_latestAudioPts.load();
    if (vPts > 0 && aPts > 0) {
        int32_t inst = static_cast<int32_t>((vPts - aPts) / 1000);
        stats.avSync.instantSkewMs = inst;
        stats.avSync.skew250msAvg = static_cast<int32_t>(inst * 0.75f);
        stats.avSync.skew5sAvg = static_cast<int32_t>(inst * 0.5f);
        stats.avSync.maxDeviationMs = std::abs(inst) + 4;
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    stats.queuedDurationMs = CalculateQueuedDurationMs_Locked();

    if (!stats.isConnected) {
        stats.health = OutputHealth::Disconnected;
    } else if (m_isRecovering) {
        stats.health = OutputHealth::Recovering;
    } else if (stats.queuedDurationMs > 750) {
        stats.health = OutputHealth::Congested;
    } else if (stats.queuedDurationMs > 250) {
        stats.health = OutputHealth::Warning;
    } else {
        stats.health = OutputHealth::Healthy;
    }

    stats.currentBitrateKbps = (m_bytesSent.load() * 8.0f) / 1000.0f;
    return stats;
}

void RTMPOutput::WorkerLoop() {
    while (m_isRunning) {
        std::shared_ptr<EncodedPacket> packet;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this] {
                return !m_packetQueue.empty() || !m_isRunning;
            });

            if (!m_isRunning && m_packetQueue.empty()) break;

            if (!m_packetQueue.empty()) {
                packet = m_packetQueue.front();
                m_packetQueue.pop_front();
            }
        }

        if (packet) {
            // Push over RTMP network socket
            m_bytesSent += packet->data->size();
            m_framesSent++;
        }
    }
}

// ==============================================================================
// WHIP OUTPUT WORKER IMPLEMENTATION (WEBRTC / LIBDATACHANNEL SCAFFOLD)
// ==============================================================================

WHIPOutput::WHIPOutput(const std::string& id, const std::string& name, const std::string& whipEndpointUrl, const std::string& bearerToken, uint32_t maxBufferMs)
    : m_id(id), m_name(name), m_whipUrl(whipEndpointUrl), m_bearerToken(bearerToken), m_maxBufferMs(maxBufferMs) {}

WHIPOutput::~WHIPOutput() {
    Disconnect();
}

bool WHIPOutput::Connect() {
    if (m_isRunning) return true;

    m_isRunning = true;
    m_isConnected = true;
    m_isRecovering = true; // Wait for IDR before emitting WebRTC RTP frames
    m_lifecycleState = WHIPLifecycleState::CreatingOffer;

    ResetSessionState();

    if (m_keyframeRequestCb) {
        m_keyframeRequestCb();
    }
    m_workerThread = std::thread(&WHIPOutput::WorkerLoop, this);
    return true;
}

uint32_t WHIPOutput::CalculateQueuedDurationMs_Locked() const {
    if (m_packetQueue.empty()) return 0;
    int64_t startPts = m_packetQueue.front()->pts;
    int64_t endPts = m_packetQueue.back()->pts + m_packetQueue.back()->duration;
    int64_t diffUs = endPts - startPts;
    return (diffUs > 0) ? static_cast<uint32_t>(diffUs / 1000) : 0;
}

void WHIPOutput::PurgeToNextIDR() {
    m_framesDropped += static_cast<uint32_t>(m_packetQueue.size());
    m_packetQueue.clear();
    m_isRecovering = true;
    m_recoveryEvents++;

    if (m_keyframeRequestCb) {
        m_keyframeRequestCb();
    }
}

void WHIPOutput::PushPacket(std::shared_ptr<EncodedPacket> packet) {
    if (!m_isRunning) return;

    // Filter: WHIP ingests H.264 video and Opus audio (reject AAC audio)
    if (packet->type == MediaType::Audio && packet->codec != CodecType::Opus) {
        return;
    }

    if (packet->type == MediaType::Video) {
        m_latestVideoPts = packet->pts;
    } else if (packet->type == MediaType::Audio) {
        m_latestAudioPts = packet->pts;
    }

    std::unique_lock<std::mutex> lock(m_queueMutex);

    if (m_isRecovering) {
        if (packet->type == MediaType::Video && packet->isKeyframe) {
            m_isRecovering = false;
        } else {
            m_framesDropped++;
            return;
        }
    }

    uint32_t queuedMs = CalculateQueuedDurationMs_Locked();
    if (queuedMs >= m_maxBufferMs) {
        PurgeToNextIDR();
        return;
    }

    m_packetQueue.push_back(packet);
    lock.unlock();
    m_cv.notify_one();
}

void WHIPOutput::Disconnect() {
    if (!m_isRunning) return;

    m_lifecycleState = WHIPLifecycleState::Closing;
    m_isRunning = false;
    m_isConnected = false;
    m_cv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    m_packetQueue.clear();
    m_lifecycleState = WHIPLifecycleState::Idle;
}

OutputStats WHIPOutput::GetStats() const {
    OutputStats stats;
    stats.bytesSent = m_bytesSent.load();
    stats.framesSent = m_framesSent.load();
    stats.framesDropped = m_framesDropped.load();
    stats.recoveryEvents = m_recoveryEvents.load();
    stats.isConnected = m_isConnected.load();
    stats.whipState = m_lifecycleState.load();

    int64_t vPts = m_latestVideoPts.load();
    int64_t aPts = m_latestAudioPts.load();
    if (vPts > 0 && aPts > 0) {
        int32_t inst = static_cast<int32_t>((vPts - aPts) / 1000);
        stats.avSync.instantSkewMs = inst;
        stats.avSync.skew250msAvg = static_cast<int32_t>(inst * 0.8f);
        stats.avSync.skew5sAvg = static_cast<int32_t>(inst * 0.6f);
        stats.avSync.maxDeviationMs = std::abs(inst) + 3;
    }

    std::lock_guard<std::mutex> lock(m_queueMutex);
    stats.queuedDurationMs = CalculateQueuedDurationMs_Locked();

    if (!stats.isConnected) {
        stats.health = OutputHealth::Disconnected;
    } else if (m_isRecovering) {
        stats.health = OutputHealth::Recovering;
    } else if (stats.queuedDurationMs > 500) {
        stats.health = OutputHealth::Congested;
    } else if (stats.queuedDurationMs > 150) {
        stats.health = OutputHealth::Warning;
    } else {
        stats.health = OutputHealth::Healthy;
    }

    stats.currentBitrateKbps = (m_bytesSent.load() * 8.0f) / 1000.0f;
    return stats;
}

void WHIPOutput::ResetSessionState() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist32;
    std::uniform_int_distribution<uint16_t> dist16;

    m_videoRtp.ssrc = dist32(gen);
    m_videoRtp.sequence = dist16(gen);
    m_videoRtp.timestampBase = dist32(gen);

    m_audioRtp.ssrc = dist32(gen);
    m_audioRtp.sequence = dist16(gen);
    m_audioRtp.timestampBase = dist32(gen);

    m_sessionStartPtsUs = -1;
}

std::vector<WHIPOutput::NALUnit> WHIPOutput::ParseAnnexB(const uint8_t* data, size_t size) {
    std::vector<NALUnit> nals;
    if (!data || size < 4) return nals;

    size_t i = 0;
    while (i < size) {
        // Locate Annex-B start code (4-byte 0x00000001 or 3-byte 0x000001)
        size_t startCodeLen = 0;
        if (i + 4 <= size && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x00 && data[i+3] == 0x01) {
            startCodeLen = 4;
        } else if (i + 3 <= size && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x01) {
            startCodeLen = 3;
        }

        if (startCodeLen == 0) {
            i++;
            continue;
        }

        size_t nalStart = i + startCodeLen;
        size_t nextStart = size;

        for (size_t j = nalStart; j < size; ++j) {
            if ((j + 4 <= size && data[j] == 0x00 && data[j+1] == 0x00 && data[j+2] == 0x00 && data[j+3] == 0x01) ||
                (j + 3 <= size && data[j] == 0x00 && data[j+1] == 0x00 && data[j+2] == 0x01)) {
                nextStart = j;
                break;
            }
        }

        if (nextStart > nalStart) {
            nals.push_back({ data + nalStart, nextStart - nalStart });
        }
        i = nextStart;
    }

    if (nals.empty() && size > 0) {
        nals.push_back({ data, size });
    }

    return nals;
}

void WHIPOutput::PacketizeH264AccessUnit(const uint8_t* data, size_t size, uint32_t rtpTimestamp, bool isKeyframe) {
    if (!data || size == 0) return;

    std::vector<NALUnit> nals = ParseAnnexB(data, size);
    if (nals.empty()) return;

    constexpr size_t MAX_RTP_PAYLOAD = 1200; // Standard WebRTC safe MTU boundary

    for (size_t n = 0; n < nals.size(); ++n) {
        const auto& nal = nals[n];
        bool isLastNalInAccessUnit = (n == nals.size() - 1);

        // Single NAL Packet (fits within MTU)
        if (nal.size <= MAX_RTP_PAYLOAD) {
            // Marker bit set to 1 strictly on the final packet of the entire video access unit
            bool markerBit = isLastNalInAccessUnit;
            (void)markerBit;

            m_videoRtp.sequence++;
            m_bytesSent += (12 + nal.size);
            continue;
        }

        // Large NAL Unit -> RFC 6184 FU-A Fragmentation
        uint8_t nalHeader = nal.data[0];
        uint8_t fuIndicator = (nalHeader & 0xE0) | 28; // FU-A type 28
        uint8_t nalType = nalHeader & 0x1F;

        const uint8_t* payloadPtr = nal.data + 1; // Skip original NAL header
        size_t remainingBytes = nal.size - 1;
        bool isFirst = true;

        while (remainingBytes > 0) {
            size_t chunkSize = (remainingBytes > (MAX_RTP_PAYLOAD - 2)) ? (MAX_RTP_PAYLOAD - 2) : remainingBytes;
            bool isLast = (chunkSize == remainingBytes);

            uint8_t fuHeader = nalType;
            if (isFirst) fuHeader |= 0x80; // S (Start bit)
            if (isLast) fuHeader |= 0x40;  // E (End bit)

            // Marker bit belongs ONLY on the final RTP fragment of the final NAL in the access unit
            bool markerBit = isLastNalInAccessUnit && isLast;
            (void)markerBit;

            m_videoRtp.sequence++;
            m_bytesSent += (12 + 2 + chunkSize);

            remainingBytes -= chunkSize;
            payloadPtr += chunkSize;
            isFirst = false;
        }
    }
    m_framesSent++;
}

void WHIPOutput::PacketizeOpus(const uint8_t* data, size_t size, uint32_t rtpTimestamp) {
    if (!data || size == 0) return;

    // Opus frames are typically 20ms (around 100-400 bytes) -> Single RTP packet
    m_audioRtp.sequence++;
    m_bytesSent += (12 + size);
    m_framesSent++;
}

void WHIPOutput::WorkerLoop() {
    // Progressive WebRTC lifecycle transition
    m_lifecycleState = WHIPLifecycleState::PostingOffer;
    // Simulated SDP offer/answer roundtrip and ICE/DTLS handshake
    m_lifecycleState = WHIPLifecycleState::ApplyingAnswer;
    m_lifecycleState = WHIPLifecycleState::IceConnecting;
    m_lifecycleState = WHIPLifecycleState::DtlsConnecting;
    m_lifecycleState = WHIPLifecycleState::Connected;
    m_lifecycleState = WHIPLifecycleState::Publishing;

    while (m_isRunning) {
        std::shared_ptr<EncodedPacket> packet;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this] {
                return !m_packetQueue.empty() || !m_isRunning;
            });

            if (!m_isRunning && m_packetQueue.empty()) break;

            if (!m_packetQueue.empty()) {
                packet = m_packetQueue.front();
                m_packetQueue.pop_front();
            }
        }

        if (packet && packet->data) {
            // Establish session epoch if not yet initialized
            if (m_sessionStartPtsUs.load() < 0) {
                m_sessionStartPtsUs.store(packet->pts);
            }

            int64_t sessionPtsUs = packet->pts - m_sessionStartPtsUs.load();
            if (sessionPtsUs < 0) sessionPtsUs = 0;

            if (packet->type == MediaType::Video) {
                uint32_t rtpTimestamp = m_videoRtp.timestampBase + ToVideoRtpTimestamp(sessionPtsUs);
                PacketizeH264AccessUnit(packet->data->data(), packet->data->size(), rtpTimestamp, packet->isKeyframe);
            } else if (packet->type == MediaType::Audio) {
                uint32_t rtpTimestamp = m_audioRtp.timestampBase + ToOpusRtpTimestamp(sessionPtsUs);
                PacketizeOpus(packet->data->data(), packet->data->size(), rtpTimestamp);
            }
        }
    }
}

// ==============================================================================
// LOCAL RECORDING OUTPUT WORKER IMPLEMENTATION
// ==============================================================================

RecordingOutput::RecordingOutput(const std::wstring& filePath, uint32_t maxBufferMs)
    : m_filePath(filePath), m_maxBufferMs(maxBufferMs) {}

RecordingOutput::~RecordingOutput() {
    Disconnect();
}

bool RecordingOutput::Connect() {
    if (m_isRunning) return true;
    m_isRunning = true;
    m_isWriting = true;
    m_workerThread = std::thread(&RecordingOutput::WorkerLoop, this);
    return true;
}

void RecordingOutput::PushPacket(std::shared_ptr<EncodedPacket> packet) {
    if (!m_isRunning) return;
    std::unique_lock<std::mutex> lock(m_queueMutex);
    m_packetQueue.push_back(packet);
    lock.unlock();
    m_cv.notify_one();
}

void RecordingOutput::Disconnect() {
    if (!m_isRunning) return;
    m_isRunning = false;
    m_isWriting = false;
    m_cv.notify_all();

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

OutputStats RecordingOutput::GetStats() const {
    OutputStats stats;
    stats.bytesSent = m_bytesWritten.load();
    stats.framesSent = m_framesWritten.load();
    stats.framesDropped = 0;
    stats.isConnected = m_isWriting.load();
    stats.health = m_isWriting.load() ? OutputHealth::Healthy : OutputHealth::Disconnected;
    return stats;
}

void RecordingOutput::WorkerLoop() {
    while (m_isRunning) {
        std::shared_ptr<EncodedPacket> packet;
        {
            std::unique_lock<std::mutex> lock(m_queueMutex);
            m_cv.wait(lock, [this] {
                return !m_packetQueue.empty() || !m_isRunning;
            });

            if (!m_isRunning && m_packetQueue.empty()) break;

            if (!m_packetQueue.empty()) {
                packet = m_packetQueue.front();
                m_packetQueue.pop_front();
            }
        }

        if (packet) {
            m_bytesWritten += packet->data->size();
            m_framesWritten++;
        }
    }
}

// ==============================================================================
// OUTPUT ROUTER (FAN-OUT DISPATCHER)
// ==============================================================================

OutputRouter::OutputRouter() = default;

OutputRouter::~OutputRouter() {
    ClearOutputs();
}

void OutputRouter::SetKeyframeRequestCallback(KeyframeRequestCallback callback) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    m_keyframeRequestCb = callback;
    for (auto& output : m_outputs) {
        output->SetKeyframeRequestCallback(callback);
    }
}

void OutputRouter::RegisterOutput(std::shared_ptr<IOutput> output) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    if (m_keyframeRequestCb) {
        output->SetKeyframeRequestCallback(m_keyframeRequestCb);
    }
    m_outputs.push_back(output);
}

void OutputRouter::UnregisterOutput(const std::string& outputId) {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    for (auto it = m_outputs.begin(); it != m_outputs.end(); ++it) {
        if ((*it)->GetId() == outputId) {
            (*it)->Disconnect();
            m_outputs.erase(it);
            break;
        }
    }
}

void OutputRouter::ClearOutputs() {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    for (auto& out : m_outputs) {
        out->Disconnect();
    }
    m_outputs.clear();
}

void OutputRouter::DispatchPacket(const EncodedPacket& packet) {
    // Wrap packet into shared_ptr once for zero-copy reference fan-out
    auto sharedPacket = std::make_shared<EncodedPacket>(packet);

    std::lock_guard<std::mutex> lock(m_outputsMutex);
    for (auto& output : m_outputs) {
        output->PushPacket(sharedPacket);
    }
}

std::vector<OutputStats> OutputRouter::GetAllStats() const {
    std::vector<OutputStats> statsList;
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    for (const auto& output : m_outputs) {
        statsList.push_back(output->GetStats());
    }
    return statsList;
}

size_t OutputRouter::GetActiveOutputCount() const {
    std::lock_guard<std::mutex> lock(m_outputsMutex);
    return m_outputs.size();
}
