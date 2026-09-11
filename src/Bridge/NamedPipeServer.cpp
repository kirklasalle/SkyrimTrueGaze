#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "NamedPipeServer.hpp"
#include <cstring>

namespace TrueGaze::Bridge {

namespace {

constexpr uint32_t HCEP_MAGIC = 0x48434550; // "HCEP"
constexpr uint32_t SKYR_MAGIC = 0x534B5952; // "SKYR"

uint32_t ComputeCrc32(const uint8_t* data, size_t length) noexcept
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

} // namespace

void NamedPipeServer::Start() noexcept
{
    if (_isRunning.exchange(true)) {
        return; // Already running
    }

    _workerThread = std::thread(&NamedPipeServer::WorkerLoop, this);
}

void NamedPipeServer::Stop() noexcept
{
    if (!_isRunning.exchange(false)) {
        return; // Already stopped
    }

    // Cancel pending I/O and close pipe handle to unblock WorkerLoop
    if (_pipeHandle && _pipeHandle != INVALID_HANDLE_VALUE) {
        HANDLE h = static_cast<HANDLE>(_pipeHandle);
        CancelIoEx(h, nullptr);
        CloseHandle(h);
        _pipeHandle = nullptr;
    }

    if (_workerThread.joinable()) {
        _workerThread.join();
    }

    _isConnected.store(false, std::memory_order_relaxed);
}

void NamedPipeServer::WorkerLoop() noexcept
{
    while (_isRunning.load(std::memory_order_relaxed)) {
        // Create inbound/outbound duplex message pipe
        HANDLE hPipe = CreateNamedPipeA(
            PIPE_NAME.data(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            1,                  // Max instances
            4096,               // Out buffer size
            4096,               // In buffer size
            5000,               // Default timeout (ms)
            nullptr             // Security attributes
        );

        if (hPipe == INVALID_HANDLE_VALUE) {
            // Sleep and retry if pipe creation failed
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }

        _pipeHandle = hPipe;

        // Wait for incoming client connection (HCEP Desktop suite)
        BOOL connected = ConnectNamedPipe(hPipe, nullptr) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected && _isRunning.load(std::memory_order_relaxed)) {
            _isConnected.store(true, std::memory_order_release);

            TrueGazeTelemetryPacket incomingPacket{};
            DWORD bytesRead = 0;

            while (_isRunning.load(std::memory_order_relaxed) && _isConnected.load(std::memory_order_relaxed)) {
                DWORD bytesAvail = 0;
                BOOL peekOk = PeekNamedPipe(hPipe, nullptr, 0, nullptr, &bytesAvail, nullptr);
                if (!peekOk) {
                    DWORD err = GetLastError();
                    if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED) {
                        break; // Client disconnected
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }

                if (bytesAvail < sizeof(TrueGazeTelemetryPacket)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                    continue;
                }

                BOOL success = ReadFile(
                    hPipe,
                    &incomingPacket,
                    sizeof(TrueGazeTelemetryPacket),
                    &bytesRead,
                    nullptr
                );

                if (!success || bytesRead != sizeof(TrueGazeTelemetryPacket)) {
                    continue;
                }

                // Verify magic and protocol integrity
                if (incomingPacket.magic == HCEP_MAGIC) {
                    uint32_t expectedCrc = ComputeCrc32(
                        reinterpret_cast<const uint8_t*>(&incomingPacket),
                        sizeof(TrueGazeTelemetryPacket) - sizeof(uint32_t)
                    );

                    if (incomingPacket.crc32 == expectedCrc) {
                        // Write to inactive buffer slot
                        uint32_t currentIndex = _readIndex.load(std::memory_order_relaxed);
                        uint32_t writeIndex = 1 - currentIndex;

                        std::memcpy(&_packetBuffers[writeIndex], &incomingPacket, sizeof(TrueGazeTelemetryPacket));

                        // Atomic publication with release semantics
                        _readIndex.store(writeIndex, std::memory_order_release);
                    }
                }
            }

            _isConnected.store(false, std::memory_order_release);
        }

        if (_pipeHandle) {
            DisconnectNamedPipe(static_cast<HANDLE>(_pipeHandle));
            CloseHandle(static_cast<HANDLE>(_pipeHandle));
            _pipeHandle = nullptr;
        }

        // Brief backoff before reopening pipe
        if (_isRunning.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

bool NamedPipeServer::TryGetLatestTelemetry(TrueGazeTelemetryPacket& outPacket) noexcept
{
    if (!_isConnected.load(std::memory_order_relaxed)) {
        return false;
    }

    uint32_t index = _readIndex.load(std::memory_order_acquire);
    std::memcpy(&outPacket, &_packetBuffers[index], sizeof(TrueGazeTelemetryPacket));
    return outPacket.magic == HCEP_MAGIC;
}

void NamedPipeServer::SendFeedback(const SkyrimFeedbackPacket& feedback) noexcept
{
    if (!_isConnected.load(std::memory_order_relaxed) || !_pipeHandle) {
        return;
    }

    SkyrimFeedbackPacket packetToSend = feedback;
    packetToSend.magic = SKYR_MAGIC;
    packetToSend.crc32 = ComputeCrc32(
        reinterpret_cast<const uint8_t*>(&packetToSend),
        sizeof(SkyrimFeedbackPacket) - sizeof(uint32_t)
    );

    DWORD bytesWritten = 0;
    WriteFile(
        static_cast<HANDLE>(_pipeHandle),
        &packetToSend,
        sizeof(SkyrimFeedbackPacket),
        &bytesWritten,
        nullptr
    );
}

} // namespace TrueGaze::Bridge
