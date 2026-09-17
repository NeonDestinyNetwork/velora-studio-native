#pragma once

#include "HardwareEncoder.h"
#include <string>
#include <vector>
#include <memory>
#include <deque>
#include <mutex>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <cstdint>

enum class OutputHealth {
    Healthy,     // < 250 ms buffer
    Warning,     // 250 - 750 ms buffer
    Congested,   // 750 - 1500 ms buffer
    Recovering,  // > 1500 ms (Purged to wait for next IDR keyframe)
    Disconnected
};

struct OutputStats {
    uint64_t bytesSent = 0;
    uint32_t framesSent = 0;
    uint32_t framesDropped = 0;
    uint32_t queuedDurationMs = 0;
    float currentBitrateKbps = 0.0f;
    OutputHealth health = OutputHealth::Healthy;
    bool isConnected = false;
};

#include <functional>

using KeyframeRequestCallback = std::function<void()>;

// Abstract Base Interface for all Output Destinations
class IOutput {
public:
    virtual ~IOutput() = default;
    virtual bool Connect() = 0;
    virtual void PushPacket(std::shared_ptr<EncodedPacket> packet) = 0;
    virtual void Disconnect() = 0;
    virtual OutputStats GetStats() const = 0;
    virtual std::string GetId() const = 0;
    virtual std::string GetName() const = 0;
    virtual void SetKeyframeRequestCallback(KeyframeRequestCallback callback) {}
};

// Threaded, GOP-Aware RTMP Output Worker with Protected A/V Lip-Sync
class RTMPOutput : public IOutput {
public:
    RTMPOutput(const std::string& id, const std::string& name, const std::string& rtmpUrl, const std::string& streamKey, uint32_t maxBufferMs = 1500);
    ~RTMPOutput() override;

    bool Connect() override;
    void PushPacket(std::shared_ptr<EncodedPacket> packet) override;
    void Disconnect() override;
    OutputStats GetStats() const override;
    std::string GetId() const override { return m_id; }
    std::string GetName() const override { return m_name; }
    void SetKeyframeRequestCallback(KeyframeRequestCallback callback) override { m_keyframeRequestCb = callback; }

private:
    void WorkerLoop();
    void PurgeToNextIDR();
    uint32_t CalculateQueuedDurationMs_Locked() const;

    std::string m_id;
    std::string m_name;
    std::string m_rtmpUrl;
    std::string m_streamKey;
    uint32_t m_maxBufferMs = 1500;

    std::atomic<bool> m_isRunning{ false };
    std::atomic<bool> m_isRecovering{ false }; // Waiting for clean IDR after congestion
    std::thread m_workerThread;
    KeyframeRequestCallback m_keyframeRequestCb = nullptr;

    // Single unified timestamp-ordered A/V queue for strict lip-sync preservation
    std::deque<std::shared_ptr<EncodedPacket>> m_packetQueue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_cv;

    // Statistics
    std::atomic<uint64_t> m_bytesSent{ 0 };
    std::atomic<uint32_t> m_framesSent{ 0 };
    std::atomic<uint32_t> m_framesDropped{ 0 };
    std::atomic<bool> m_isConnected{ false };
};

// Threaded Local Recording Output (MP4 / MKV Muxer)
class RecordingOutput : public IOutput {
public:
    RecordingOutput(const std::wstring& filePath, uint32_t maxBufferMs = 3000);
    ~RecordingOutput() override;

    bool Connect() override;
    void PushPacket(std::shared_ptr<EncodedPacket> packet) override;
    void Disconnect() override;
    OutputStats GetStats() const override;
    std::string GetId() const override { return "local-recording"; }
    std::string GetName() const override { return "Local Recording"; }

private:
    void WorkerLoop();

    std::wstring m_filePath;
    uint32_t m_maxBufferMs = 3000;

    std::atomic<bool> m_isRunning{ false };
    std::thread m_workerThread;
    std::deque<std::shared_ptr<EncodedPacket>> m_packetQueue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_cv;

    std::atomic<uint64_t> m_bytesWritten{ 0 };
    std::atomic<uint32_t> m_framesWritten{ 0 };
    std::atomic<bool> m_isWriting{ false };
};

// Central Output Router (Single-Encode Distribution with Output Isolation)
class OutputRouter {
public:
    OutputRouter();
    ~OutputRouter();

    void SetKeyframeRequestCallback(KeyframeRequestCallback callback);
    void RegisterOutput(std::shared_ptr<IOutput> output);
    void UnregisterOutput(const std::string& outputId);
    void ClearOutputs();

    // Broadcast unified A/V packets across all outputs
    void DispatchPacket(const EncodedPacket& packet);

    std::vector<OutputStats> GetAllStats() const;
    size_t GetActiveOutputCount() const;

private:
    std::vector<std::shared_ptr<IOutput>> m_outputs;
    mutable std::mutex m_outputsMutex;
    KeyframeRequestCallback m_keyframeRequestCb = nullptr;
};
