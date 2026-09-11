#pragma once

#include "PCH.h"
#include "TelemetryPacket.h"
#include <atomic>
#include <thread>

namespace TrueGaze::Bridge {

/// @brief Asynchronous Windows Named Pipe Server for real-time HCEP Desktop telemetry.
/// Connects to D:\Projects\HCEP over \\.\pipe\TrueGazeBridge with zero game-thread blocking.
class NamedPipeServer
{
public:
    static constexpr std::string_view PIPE_NAME = R"(\\.\pipe\TrueGazeBridge)";

    NamedPipeServer() = default;
    ~NamedPipeServer() { Stop(); }

    /// @brief Starts the background worker thread listening for incoming telemetry packets.
    void Start() noexcept;

    /// @brief Stops the background worker and terminates pipe handles.
    void Stop() noexcept;

    /// @brief Checks if a client (HCEP Desktop suite) is currently connected.
    bool IsConnected() const noexcept { return _isConnected.load(std::memory_order_relaxed); }

    /// @brief Atomically fetches the latest valid telemetry packet. Takes < 10 nanoseconds.
    bool TryGetLatestTelemetry(TrueGazeTelemetryPacket& outPacket) noexcept;

    /// @brief Sends game feedback (target NPC FormID, mutual gaze state) back to HCEP Desktop.
    void SendFeedback(const SkyrimFeedbackPacket& feedback) noexcept;

private:
    void WorkerLoop() noexcept;

    std::atomic<bool> _isRunning{ false };
    std::atomic<bool> _isConnected{ false };
    std::atomic<uint32_t> _readIndex{ 0 };

    // Double-buffered lock-free storage
    TrueGazeTelemetryPacket _packetBuffers[2]{};

    std::thread _workerThread;
    void* _pipeHandle{ nullptr }; // Windows HANDLE
};

} // namespace TrueGaze::Bridge
