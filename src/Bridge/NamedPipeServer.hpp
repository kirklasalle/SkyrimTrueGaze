#pragma once

#include "PCH.h"
#include "TelemetryPacket.h"
#include <atomic>
#include <array>
#include <thread>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace TrueGaze::Bridge
{

    /// @brief Asynchronous Windows Named Pipe server for real-time HCEP Desktop telemetry.
    ///
    /// ## Concurrency model
    ///
    /// One background worker thread owns the pipe. The game thread only ever reads the
    /// latest published telemetry packet, and only ever enqueues outbound feedback.
    ///
    /// The original implementation claimed "lock-free double buffering" but was not
    /// lock-free: it stored plain 64-byte PODs in a two-element array and published an
    /// atomic *index*. The index was ordered, the payload was not, so with two buffers
    /// the writer could overwrite the slot the reader was mid-`memcpy` on. `_pipeHandle`
    /// was additionally written by the worker and read by the game thread with no
    /// synchronisation at all. See docs/AUDIT_REPORT_2026-09-11.md finding C-7.
    ///
    /// This implementation fixes both:
    ///
    ///   * **Triple buffering.** Three slots guarantee the writer can never select the
    ///     slot the reader is currently consuming. The writer publishes with a
    ///     release store to `_readyIndex`; the reader acquires it. This is the standard
    ///     single-producer/single-consumer triple-buffer and needs no locks.
    ///
    ///   * **Atomic pipe handle.** `_pipeHandle` is a `std::atomic<void*>`. The game
    ///     thread no longer touches the handle directly; `SendFeedback` pushes onto a
    ///     small lock-free ring that the worker drains. The handle is therefore only
    ///     ever dereferenced on the thread that owns it.
    ///
    /// ## Security
    ///
    /// The pipe is created with an explicit security descriptor granting access to the
    /// current user only, instead of the default DACL. The payload carries biometric
    /// data (gaze vector, head pose, blink state, cognitive classification), so
    /// same-session processes should not be able to subscribe. See GOVERNANCE.md
    /// "Law 6 — Biometric data protection".
    class NamedPipeServer
    {
    public:
        static constexpr std::string_view PIPE_NAME = R"(\\.\pipe\TrueGazeBridge)";

        /// Outbound feedback ring capacity. Sized for several seconds at 60 Hz; a
        /// full ring drops the oldest entry rather than blocking the game thread.
        static constexpr uint32_t FEEDBACK_RING_SIZE = 256;

        /// Telemetry older than this is treated as stale and TryGetLatestTelemetry fails.
        static constexpr uint64_t TELEMETRY_TIMEOUT_US = 500'000; // 500 ms

        NamedPipeServer() = default;
        ~NamedPipeServer() { Stop(); }

        NamedPipeServer(const NamedPipeServer &) = delete;
        NamedPipeServer &operator=(const NamedPipeServer &) = delete;

        /// @brief Starts the background worker thread listening for telemetry packets.
        /// @param pipeName Full pipe path; defaults to the canonical TrueGaze pipe.
        /// @param reconnectIntervalSec Worker retry delay when no client is connected.
        void Start(const char *pipeName = PIPE_NAME.data(),
                   float reconnectIntervalSec = 3.0f) noexcept;

        /// @brief Stops the background worker and terminates pipe handles.
        void Stop() noexcept;

        /// @brief True if an HCEP Desktop client is currently connected.
        bool IsConnected() const noexcept { return _isConnected.load(std::memory_order_relaxed); }

        /// @brief Atomically fetches the latest valid telemetry packet.
        /// @return false if no client is connected, no packet has arrived, or the
        ///         newest packet is older than TELEMETRY_TIMEOUT_US.
        bool TryGetLatestTelemetry(TrueGazeTelemetryPacket &outPacket) noexcept;

        /// @brief Queues game feedback for the worker to transmit. Never blocks.
        void SendFeedback(const SkyrimFeedbackPacket &feedback) noexcept;

    private:
        void WorkerLoop() noexcept;
        void DrainOutboundQueue(void *pipeHandle) noexcept;

        // --- Configuration (set once by Start, read by the worker) ---
        std::string _pipeName{PIPE_NAME.data()};
        float _reconnectIntervalSec{3.0f};

        // --- Connection state ---
        std::atomic<bool> _isRunning{false};
        std::atomic<bool> _isConnected{false};

        /// Owned exclusively by the worker thread. Reset to null on teardown.
        /// Declared atomic so Stop() can safely close it while the worker holds it.
        std::atomic<void *> _pipeHandle{nullptr};

        // --- Telemetry triple buffer (worker writes, game thread reads) ---
        static constexpr uint32_t SLOT_COUNT = 3;

        struct TelemetrySlot
        {
            TrueGazeTelemetryPacket packet{};
            uint64_t receivedAtUs{0};
            uint32_t sequence{0};
        };

        std::array<TelemetrySlot, SLOT_COUNT> _slots{};

        /// Index of the most recently published slot. Starts at SLOT_COUNT meaning
        /// "nothing published yet", so the reader never mistakes slot 0's zeroed
        /// contents for real data.
        std::atomic<uint32_t> _readyIndex{SLOT_COUNT};
        std::atomic<uint32_t> _writeIndex{0};

        /// Bumped every publish. Lets the reader detect that a slot was reused while
        /// it was copying, and retry.
        std::atomic<uint32_t> _publishEpoch{0};

        // --- Outbound feedback ring (game thread writes, worker drains) ---
        std::array<SkyrimFeedbackPacket, FEEDBACK_RING_SIZE> _feedbackRing{};
        std::atomic<uint32_t> _feedbackHead{0}; // consumer (worker)
        std::atomic<uint32_t> _feedbackTail{0}; // producer (game thread)

        // --- Overlapped I/O & Shutdown Control ---
        HANDLE _shutdownEvent{nullptr};
        OVERLAPPED _connectOverlapped{};

        std::thread _workerThread;
    };

} // namespace TrueGaze::Bridge
