#include "PCH.h"

#include <windows.h>
#include <sddl.h> // ConvertStringSecurityDescriptorToSecurityDescriptorA

#include "NamedPipeServer.hpp"
#include <cstring>
#include <chrono>

namespace TrueGaze::Bridge
{

    namespace
    {

        constexpr uint32_t HCEP_MAGIC = 0x48434550; // "HCEP"
        constexpr uint32_t SKYR_MAGIC = 0x534B5952; // "SKYR"

        uint32_t ComputeCrc32(const uint8_t *data, size_t length) noexcept
        {
            uint32_t crc = 0xFFFFFFFF;
            for (size_t i = 0; i < length; ++i)
            {
                crc ^= data[i];
                for (int j = 0; j < 8; ++j)
                {
                    crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
                }
            }
            return ~crc;
        }

        uint64_t NowMicroseconds() noexcept
        {
            using namespace std::chrono;
            return static_cast<uint64_t>(
                duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count());
        }

        /// Builds a security descriptor granting access to the current user only.
        ///
        /// The pipe carries biometric data, so the default DACL — which typically permits
        /// any process in the same session — is not appropriate. See GOVERNANCE.md
        /// "Law 6 — Biometric data protection".
        ///
        /// On failure the caller falls back to the default DACL and logs, rather than
        /// refusing to start: a permissive pipe is preferable to a mod that cannot load,
        /// provided the fallback is reported.
        class UserOnlySecurityDescriptor
        {
        public:
            UserOnlySecurityDescriptor() noexcept
            {
                // "D:(A;;GA;;;OW)" grants GENERIC_ALL to the Owner Rights SID, i.e. the
                // user who created the object. An explicit DACL is used rather than
                // nullptr so the pipe does not silently inherit a permissive default.
                if (!ConvertStringSecurityDescriptorToSecurityDescriptorA(
                        "D:(A;;GA;;;OW)", SDDL_REVISION_1, &_descriptor, nullptr))
                {
                    _descriptor = nullptr;
                    return;
                }

                _attributes.nLength = sizeof(_attributes);
                _attributes.lpSecurityDescriptor = _descriptor;
                _attributes.bInheritHandle = FALSE;
                _valid = true;
            }

            ~UserOnlySecurityDescriptor() noexcept
            {
                if (_descriptor)
                {
                    LocalFree(_descriptor);
                }
            }

            UserOnlySecurityDescriptor(const UserOnlySecurityDescriptor &) = delete;
            UserOnlySecurityDescriptor &operator=(const UserOnlySecurityDescriptor &) = delete;

            [[nodiscard]] bool IsValid() const noexcept { return _valid; }

            [[nodiscard]] SECURITY_ATTRIBUTES *Attributes() noexcept
            {
                return _valid ? &_attributes : nullptr;
            }

        private:
            PSECURITY_DESCRIPTOR _descriptor{nullptr};
            SECURITY_ATTRIBUTES _attributes{};
            bool _valid{false};
        };

    } // namespace

    void NamedPipeServer::Start() noexcept
    {
        if (_isRunning.exchange(true))
        {
            return; // Already running
        }

        _workerThread = std::thread(&NamedPipeServer::WorkerLoop, this);
    }

    void NamedPipeServer::Stop() noexcept
    {
        if (!_isRunning.exchange(false))
        {
            return; // Already stopped
        }

        // Cancel pending I/O and close the handle to unblock the worker.
        // _pipeHandle is atomic, so this is safe even while the worker holds it.
        if (void *raw = _pipeHandle.exchange(nullptr))
        {
            HANDLE h = static_cast<HANDLE>(raw);
            if (h != INVALID_HANDLE_VALUE)
            {
                CancelIoEx(h, nullptr);
                CloseHandle(h);
            }
        }

        if (_workerThread.joinable())
        {
            _workerThread.join();
        }

        _isConnected.store(false, std::memory_order_relaxed);
    }

    void NamedPipeServer::WorkerLoop() noexcept
    {
        while (_isRunning.load(std::memory_order_relaxed))
        {
            UserOnlySecurityDescriptor security;
            if (!security.IsValid())
            {
                logger::warn("[TrueGaze] Pipe security descriptor unavailable; "
                             "falling back to the default DACL.");
            }

            HANDLE hPipe = CreateNamedPipeA(
                PIPE_NAME.data(),
                PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                1,    // max instances
                4096, // out buffer
                4096, // in buffer
                5000, // default timeout (ms)
                security.Attributes());

            if (hPipe == INVALID_HANDLE_VALUE)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                continue;
            }

            _pipeHandle.store(hPipe, std::memory_order_release);

            // ConnectNamedPipe returns FALSE with ERROR_PIPE_CONNECTED when a client
            // raced us to the handle. That is a success condition, not a failure.
            const BOOL connected =
                ConnectNamedPipe(hPipe, nullptr) ? TRUE
                                                 : (GetLastError() == ERROR_PIPE_CONNECTED);

            if (connected && _isRunning.load(std::memory_order_relaxed))
            {
                _isConnected.store(true, std::memory_order_release);
                logger::info("[TrueGaze] HCEP Desktop connected on \\\\.\\pipe\\TrueGazeBridge.");

                TrueGazeTelemetryPacket incoming{};

                while (_isRunning.load(std::memory_order_relaxed) && _isConnected.load(std::memory_order_relaxed))
                {

                    // Drain anything the game thread queued for the client.
                    DrainOutboundQueue(hPipe);

                    DWORD bytesAvail = 0;
                    if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &bytesAvail, nullptr))
                    {
                        const DWORD err = GetLastError();
                        if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED)
                        {
                            break; // Client disconnected
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                        continue;
                    }

                    // Never read without confirming a whole packet is available.
                    if (bytesAvail < sizeof(TrueGazeTelemetryPacket))
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2));
                        continue;
                    }

                    DWORD bytesRead = 0;
                    if (!ReadFile(hPipe, &incoming, sizeof(incoming), &bytesRead, nullptr) || bytesRead != sizeof(incoming))
                    {
                        continue;
                    }

                    if (incoming.magic != HCEP_MAGIC)
                    {
                        continue;
                    }

                    const uint32_t expectedCrc = ComputeCrc32(
                        reinterpret_cast<const uint8_t *>(&incoming),
                        sizeof(incoming) - sizeof(uint32_t));

                    if (incoming.crc32 != expectedCrc)
                    {
                        continue;
                    }

                    // --- Publish into the next slot -------------------------------
                    //
                    // Triple buffering. The writer advances to the slot after the one
                    // it last wrote. With three slots and a single reader, the writer's
                    // next slot can never be the slot the reader is copying from.
                    const uint32_t writeIdx =
                        (_writeIndex.load(std::memory_order_relaxed) + 1) % SLOT_COUNT;

                    _slots[writeIdx].packet = incoming;
                    _slots[writeIdx].receivedAtUs = NowMicroseconds();
                    _slots[writeIdx].sequence = incoming.sequenceId;

                    _writeIndex.store(writeIdx, std::memory_order_relaxed);

                    // Bump the epoch before publishing the index, so a reader that
                    // sampled the old epoch knows to retry.
                    _publishEpoch.fetch_add(1, std::memory_order_release);
                    _readyIndex.store(writeIdx, std::memory_order_release);
                }

                _isConnected.store(false, std::memory_order_release);
                logger::info("[TrueGaze] HCEP Desktop disconnected.");
            }

            if (void *raw = _pipeHandle.exchange(nullptr))
            {
                HANDLE h = static_cast<HANDLE>(raw);
                if (h != INVALID_HANDLE_VALUE)
                {
                    DisconnectNamedPipe(h);
                    CloseHandle(h);
                }
            }

            if (_isRunning.load(std::memory_order_relaxed))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
        }
    }

    void NamedPipeServer::DrainOutboundQueue(void *pipeHandle) noexcept
    {
        if (!pipeHandle)
        {
            return;
        }

        HANDLE hPipe = static_cast<HANDLE>(pipeHandle);

        uint32_t head = _feedbackHead.load(std::memory_order_relaxed);
        const uint32_t tail = _feedbackTail.load(std::memory_order_acquire);

        while (head != tail)
        {
            SkyrimFeedbackPacket packet = _feedbackRing[head];
            head = (head + 1) % FEEDBACK_RING_SIZE;

            packet.magic = SKYR_MAGIC;
            packet.crc32 = ComputeCrc32(
                reinterpret_cast<const uint8_t *>(&packet),
                sizeof(packet) - sizeof(uint32_t));

            DWORD written = 0;
            if (!WriteFile(hPipe, &packet, sizeof(packet), &written, nullptr) || written != sizeof(packet))
            {
                break; // Client went away; leave the remainder for the next connection
            }
        }

        _feedbackHead.store(head, std::memory_order_release);
    }

    bool NamedPipeServer::TryGetLatestTelemetry(TrueGazeTelemetryPacket &outPacket) noexcept
    {
        if (!_isConnected.load(std::memory_order_relaxed))
        {
            return false;
        }

        // Bounded retry: if the writer reuses the slot mid-copy we simply read again.
        // With three slots this should never iterate more than a handful of times.
        for (int attempt = 0; attempt < 8; ++attempt)
        {
            const uint32_t epochBefore = _publishEpoch.load(std::memory_order_acquire);
            const uint32_t index = _readyIndex.load(std::memory_order_acquire);

            if (index >= SLOT_COUNT)
            {
                return false; // Nothing published yet
            }

            const TelemetrySlot snapshot = _slots[index];

            const uint32_t epochAfter = _publishEpoch.load(std::memory_order_acquire);
            if (epochBefore != epochAfter)
            {
                continue; // A publish happened during the copy; retry
            }

            if (snapshot.packet.magic != HCEP_MAGIC)
            {
                return false;
            }

            // Reject stale telemetry. If HCEP Desktop stops sending (crashed, paused,
            // sensor unplugged) we must stop driving NPCs from frozen data.
            const uint64_t now = NowMicroseconds();
            if (now > snapshot.receivedAtUs && (now - snapshot.receivedAtUs) > TELEMETRY_TIMEOUT_US)
            {
                return false;
            }

            outPacket = snapshot.packet;
            return true;
        }

        return false;
    }

    void NamedPipeServer::SendFeedback(const SkyrimFeedbackPacket &feedback) noexcept
    {
        // Called on the game thread. Must never block and must never touch the pipe
        // handle — that belongs exclusively to the worker. Feedback is queued here and
        // transmitted by DrainOutboundQueue().
        const uint32_t tail = _feedbackTail.load(std::memory_order_relaxed);
        const uint32_t nextTail = (tail + 1) % FEEDBACK_RING_SIZE;

        // Full ring: drop the oldest entry rather than stalling the game thread.
        if (nextTail == _feedbackHead.load(std::memory_order_acquire))
        {
            const uint32_t newHead =
                (_feedbackHead.load(std::memory_order_relaxed) + 1) % FEEDBACK_RING_SIZE;
            _feedbackHead.store(newHead, std::memory_order_release);
        }

        _feedbackRing[tail] = feedback;
        _feedbackTail.store(nextTail, std::memory_order_release);
    }

} // namespace TrueGaze::Bridge
