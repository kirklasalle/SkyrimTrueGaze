#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// ApiTelemetryCache.hpp — thread-safe hand-off between the game thread and the
// exported C API (include/TrueGazeAPI.h).
//
// AUDIT 2026-10-15 F-03: the exported functions are callable by third-party
// SKSE plugins from ANY thread (Papyrus VM threads, their own workers, ...).
// They previously called GazeEngine::FindActor(), which reads an
// unsynchronised std::unordered_map that the game thread mutates every frame
// (try_emplace / erase in TickActor and EndFrame) — a data race that can crash
// on rehash. TrueGaze_OverrideActorMode also WROTE runtime state cross-thread.
//
// The fix mirrors the proven OarConditions pattern:
//   * the game thread publishes an immutable snapshot per actor every tick
//     (GazeEngine::PublishState -> PublishTelemetry);
//   * API readers copy the snapshot under a shared lock;
//   * mode overrides are QUEUED under a mutex and applied by the game thread at
//     the start of that actor's next TickActor (TakePendingModeOverride).
//
// Implemented in src/Engine/TrueGazeAPI.cpp.
// ---------------------------------------------------------------------------

namespace TrueGaze::API::Internal
{
    /// Immutable copy of one actor's live gaze state, as of its last tick.
    struct TelemetrySnapshot
    {
        uint32_t targetFormId{0};
        float gazePitchDeg{0.0f};
        float gazeYawDeg{0.0f};
        float mutualGazeDurationSec{0.0f};
        uint8_t hcepMode{0};
        uint8_t gazeRegion{0};
        uint8_t lodTier{0};
        bool isBlinking{false};
    };

    /// Game thread only. Replaces the actor's snapshot and stamps it fresh.
    void PublishTelemetry(uint32_t actorFormId, const TelemetrySnapshot &snapshot) noexcept;

    /// Game thread only. Drops the actor's snapshot and any queued override
    /// (called when GazeEngine evicts the actor).
    void ForgetActor(uint32_t actorFormId) noexcept;

    /// Game thread only. Drops every snapshot and queued override (ResetAll).
    void ClearTelemetry() noexcept;

    /// Any thread. Copies the actor's snapshot if one was published within the
    /// staleness window; returns false otherwise.
    [[nodiscard]] bool TryGetTelemetry(uint32_t actorFormId, TelemetrySnapshot &out) noexcept;

    /// Any thread. Queues a mode override for an actor that currently has fresh
    /// telemetry. Returns false (and queues nothing) for unknown/stale actors or
    /// an out-of-range mode. A newer request for the same actor replaces an
    /// older, not-yet-applied one.
    bool QueueModeOverride(uint32_t actorFormId, uint8_t mode, float durationSec) noexcept;

    /// Game thread only. Removes and returns the actor's queued override, if any.
    [[nodiscard]] bool TakePendingModeOverride(uint32_t actorFormId, uint8_t &outMode,
                                               float &outDurationSec) noexcept;
} // namespace TrueGaze::API::Internal
