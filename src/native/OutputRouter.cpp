#include "OutputRouter.h"
#include <iostream>
#include <chrono>

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

    // Request a fresh IDR keyframe immediately from HardwareEncoder to recover instantly
    if (m_keyframeRequestCb) {
        m_keyframeRequestCb();
    }
}

void RTMPOutput::PushPacket(std::shared_ptr<EncodedPacket> packet) {
    if (!m_isRunning) return;

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

    // 2. Measure queued duration in milliseconds
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
    stats.isConnected = m_isConnected.load();

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
