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
    ///
    /// ## Data minimisation and connection audit (issue #7)
    ///
    ///   * `trackedPersonId` is an identity linkage that no gameplay code consumes. It
    ///     is **zeroed on receipt**, after CRC and semantic validation and before the
    ///     packet is published to the game thread, unless the operator opts in with
    ///     `SetRetainTrackedPersonId(true)` (INI: `[Bridge] bRetainTrackedPersonId`).
    ///   * Every client connection is logged with the client's process ID, session ID
    ///     and executable file name (no full path, so the user profile is kept out
    ///     of shared logs), plus whether the user-only ACL was in force. On disconnect,
    ///     the log records how many frames were accepted and how many were rejected.
    ///     Packet contents are never logged.
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

        /// Destructor. Runs Stop() with JoinPolicy::Join, but becomes a no-op
        /// if the server was abandoned at process exit (Stop(Abandon)) —
        /// destroying state the orphaned worker may still touch would be the
        /// exact use-after-free the R14 E1.1 fix exists to prevent. In that
        /// case the object leaks deliberately at process teardown.
        ~NamedPipeServer()
        {
            if (!_abandoned.load(std::memory_order_relaxed))
            {
                Stop(JoinPolicy::Join);
            }
            // Abandoned: the worker may still be running; destroying anything
            // it touches would be a use-after-free. The OS reclaims everything
            // at process exit. This is the documented leak-for-safety trade.
        }

        NamedPipeServer(const NamedPipeServer &) = delete;
        NamedPipeServer &operator=(const NamedPipeServer &) = delete;

        /// How Stop() should handle the worker thread.
        enum class JoinPolicy : uint8_t
        {
            /// Normal shutdown: join the worker unconditionally (bounded by the
            /// shutdown event + CancelIoEx) and release all handles. Use for
            /// session teardown, repeated start/stop, and tests.
            Join,

            /// Process-exit shutdown: do NOT join (loader-lock deadlock risk) and
            /// do NOT destroy any state. Flags the object as abandoned so the
            /// destructor is a no-op; the worker terminates with the process.
            /// This replaces the old 250 ms wait + detach, which risked a
            /// detached worker dereferencing destroyed memory (audit C-1).
            Abandon
        };

        /// @brief Starts the background worker thread listening for telemetry packets.
        /// @param pipeName Full pipe path; defaults to the canonical TrueGaze pipe.
        /// @param reconnectIntervalSec Worker retry delay when no client is connected.
        void Start(const char *pipeName = PIPE_NAME.data(),
                   float reconnectIntervalSec = 3.0f) noexcept;

        /// @brief Stops the background worker and terminates pipe handles.
        /// @param policy Join = graceful, blocking, for normal shutdown.
        ///               Abandon = process-exit path; the object is left intact
        ///               for the orphaned worker and the destructor no-ops.
        void Stop(JoinPolicy policy = JoinPolicy::Join) noexcept;

        /// @brief True if an HCEP Desktop client is currently connected.
        bool IsConnected() const noexcept { return _isConnected.load(std::memory_order_relaxed); }

        /// @brief Atomically fetches the latest valid telemetry packet.
        /// @return false if no client is connected, no packet has arrived, or the
        ///         newest packet is older than TELEMETRY_TIMEOUT_US.
        bool TryGetLatestTelemetry(TrueGazeTelemetryPacket &outPacket) noexcept;

        /// @brief Queues game feedback for the worker to transmit. Never blocks.
        void SendFeedback(const SkyrimFeedbackPacket &feedback) noexcept;

        /// @brief Whether `trackedPersonId` is kept on received packets.
        ///
        /// Default false: the field is zeroed at the pipe boundary so the identity
        /// linkage never reaches the rest of the plugin. May be called at any time;
        /// takes effect from the next received frame.
        void SetRetainTrackedPersonId(bool retain) noexcept
        {
            _retainTrackedPersonId.store(retain, std::memory_order_relaxed);
        }

        [[nodiscard]] bool RetainsTrackedPersonId() const noexcept
        {
            return _retainTrackedPersonId.load(std::memory_order_relaxed);
        }

    private:
        void WorkerLoop() noexcept;
        void DrainOutboundQueue(void *pipeHandle) noexcept;

        /// Event reused across DrainOutboundQueue calls (R14 E1.3 / audit C-3).
        /// Created once on Start, closed on Stop — the old code created and
        /// destroyed an event every 2 ms while a client was connected.
        HANDLE _drainEvent{nullptr};

        // --- Configuration (set once by Start, read by the worker) ---
        std::string _pipeName{PIPE_NAME.data()};
        float _reconnectIntervalSec{3.0f};

        // --- Connection state ---
        std::atomic<bool> _isRunning{false};
        std::atomic<bool> _isConnected{false};

        /// Data-minimisation switch (issue #7). False = zero trackedPersonId on receipt.
        std::atomic<bool> _retainTrackedPersonId{false};

        /// Owned exclusively by the worker thread. Reset to null on teardown.
        /// Declared atomic so Stop() can safely close it while the worker holds it.
        std::atomic<void *> _pipeHandle{nullptr};

        // --- Telemetry triple buffer (worker writes, game thread reads) ---
        //
        // ## HAPPENS-BEFORE ARGUMENT (R14 E1.2 — audit finding C-2)
        //
        // Single producer (the worker), single reader (the game thread), three
        // slots, one epoch counter. The proof:
        //
        //   WRITER (worker thread):
        //     1. Plain store of the full payload into _slots[w]
        //        (packet, receivedAtUs, sequence — all in one assignment).
        //     2. _writeIndex.store(w+1, relaxed)          [private bookkeeping]
        //     3. _publishEpoch.fetch_add(1, release)     [EPOCH BUMP]
        //     4. _readyIndex.store(w, release)           [PUBLISH]
        //
        //   READER (game thread):
        //     a. _publishEpoch.load(acquire)             [E0]
        //     b. _readyIndex.load(acquire)               [INDEX]
        //     c. Plain copy of _slots[index]
        //     d. _publishEpoch.load(acquire); if changed -> retry
        //
        //   Ordering: the payload store (1) is sequenced-before the epoch
        //   release (3), which is sequenced-before the readyIndex release (4).
        //   The reader's acquire on _readyIndex (step a) therefore
        //   synchronises-with the writer's release (step 4), so EVERY payload
        //   field stored in step 1 is visible to the reader's load in step c.
        //   This is the standard release/acquire publication pattern; no
        //   payload field requires an atomic type.
        //
        //   THE 3-SLOT INVARIANT (single reader):
        //   The writer always publishes to (lastWritten + 1) % 3. For the
        //   writer to overwrite the slot the reader is currently copying, the
        //   writer would have to complete TWO publications while the reader
        //   holds one copy. The epoch retry (steps epochBefore/epochAfter
        //   around the copy in TryGetLatestTelemetry) makes any such overlap
        //   detectable: the copy is discarded and retried. A torn read is
        //   therefore never OBSERVED, even though it can transiently occur —
        //   this is the seqlock-style validation, not a hope.
        //
        //   Worst-case retry count is 2 (third publication would need the
        //   writer to lap the reader twice within one memcpy of ~64 bytes);
        //   TryGetLatestTelemetry bounds attempts at 8 and fails honestly
        //   rather than returning a possibly-torn packet.
        //
        //   STRESS EVIDENCE: tests/HcepBridgeClientMock.cpp contains a
        //   10 kHz publisher / asserting reader consistency stress test
        //   (E1.2) that validates this argument empirically across millions
        //   of publications. See also TELEMETRY_TIMEOUT_US for the freshness
        //   contract.
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

        /// Set by Stop(JoinPolicy::Abandon) at process exit: the worker is
        /// deliberately left alive and the destructor must never destroy this
        /// object's state. See Stop() for the ownership rationale.
        std::atomic<bool> _abandoned{false};

        std::thread _workerThread;
    };

} // namespace TrueGaze::Bridge
