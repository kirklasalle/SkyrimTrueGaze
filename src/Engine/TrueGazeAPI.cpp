#define TRUEGAZE_EXPORTS
#include "../include/TrueGazeAPI.h"
#include "ApiTelemetryCache.hpp"
#include "Bridge/NamedPipeServer.hpp"
#include "ConfigManager.hpp"
#include "GazeEngine.hpp"
#include "Integrations/OarConditions.hpp"
#include "LodManager.hpp"
#include "PCH.h"
#include "TrueGazeVersion.h" // generated from project(VERSION) - audit F-07

#include <chrono>
#include <cmath>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Thread-safe telemetry / override hand-off (audit 2026-10-15 F-03).
// See ApiTelemetryCache.hpp for the rationale. The exported functions below
// NEVER touch GazeEngine's per-actor map; they only read/write these caches.
// ---------------------------------------------------------------------------
namespace TrueGaze::API::Internal
{
    namespace
    {
        using Clock = std::chrono::steady_clock;

        /// Same freshness window as the OAR condition cache, so the C API and OAR
        /// agree on when an actor's state stops being "known".
        constexpr float kStaleAfterSec = Integrations::OarConditions::STALE_AFTER_SEC;

        /// Upper bound on queued overrides. Overrides are only accepted for actors
        /// with fresh telemetry (themselves capped by MAX_TRACKED_ACTORS), so this
        /// is purely a defence against a misbehaving caller.
        constexpr std::size_t kMaxPendingOverrides = 256;

        struct Entry
        {
            TelemetrySnapshot snapshot{};
            Clock::time_point updatedAt{};
        };

        struct PendingOverride
        {
            uint8_t mode{0};
            float durationSec{0.0f};
        };

        std::shared_mutex g_telemetryMutex;
        std::unordered_map<uint32_t, Entry> g_telemetry;

        std::mutex g_overrideMutex;
        std::unordered_map<uint32_t, PendingOverride> g_pendingOverrides;

        [[nodiscard]] bool IsFresh(const Entry &e, Clock::time_point now) noexcept
        {
            return (now - e.updatedAt) <= std::chrono::duration<float>(kStaleAfterSec);
        }
    } // namespace

    void PublishTelemetry(uint32_t actorFormId, const TelemetrySnapshot &snapshot) noexcept
    {
        if (actorFormId == 0)
        {
            return;
        }
        const auto now = Clock::now();
        try
        {
            std::unique_lock lock(g_telemetryMutex);
            auto &e = g_telemetry[actorFormId];
            e.snapshot = snapshot;
            e.updatedAt = now;
        }
        catch (...)
        {
            // Allocation failure on first insert: telemetry is best-effort.
        }
    }

    void ForgetActor(uint32_t actorFormId) noexcept
    {
        {
            std::unique_lock lock(g_telemetryMutex);
            g_telemetry.erase(actorFormId);
        }
        {
            std::lock_guard lock(g_overrideMutex);
            g_pendingOverrides.erase(actorFormId);
        }
    }

    void ClearTelemetry() noexcept
    {
        {
            std::unique_lock lock(g_telemetryMutex);
            g_telemetry.clear();
        }
        {
            std::lock_guard lock(g_overrideMutex);
            g_pendingOverrides.clear();
        }
    }

    bool TryGetTelemetry(uint32_t actorFormId, TelemetrySnapshot &out) noexcept
    {
        const auto now = Clock::now();
        std::shared_lock lock(g_telemetryMutex);
        const auto it = g_telemetry.find(actorFormId);
        if (it == g_telemetry.end() || !IsFresh(it->second, now))
        {
            return false;
        }
        out = it->second.snapshot;
        return true;
    }

    bool QueueModeOverride(uint32_t actorFormId, uint8_t mode, float durationSec) noexcept
    {
        if (actorFormId == 0 || mode > 4)
        {
            return false;
        }

        // Only actors TrueGaze is actively processing can be overridden — the same
        // contract as before, when an actor without runtime state was ignored.
        TelemetrySnapshot unused{};
        if (!TryGetTelemetry(actorFormId, unused))
        {
            return false;
        }

        // Reject NaN/inf/negative durations up front; fall back to the documented
        // 5 s default exactly as the previous implementation did for <= 0.
        const float duration = (std::isfinite(durationSec) && durationSec > 0.0f) ? durationSec : 5.0f;

        try
        {
            std::lock_guard lock(g_overrideMutex);
            if (g_pendingOverrides.size() >= kMaxPendingOverrides &&
                g_pendingOverrides.find(actorFormId) == g_pendingOverrides.end())
            {
                return false;
            }
            g_pendingOverrides[actorFormId] = PendingOverride{mode, duration};
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool TakePendingModeOverride(uint32_t actorFormId, uint8_t &outMode, float &outDurationSec) noexcept
    {
        std::lock_guard lock(g_overrideMutex);
        if (g_pendingOverrides.empty())
        {
            return false; // fast path: the overwhelmingly common case
        }
        const auto it = g_pendingOverrides.find(actorFormId);
        if (it == g_pendingOverrides.end())
        {
            return false;
        }
        outMode = it->second.mode;
        outDurationSec = it->second.durationSec;
        g_pendingOverrides.erase(it);
        return true;
    }
} // namespace TrueGaze::API::Internal

namespace TrueGaze::API
{

    namespace
    {

        /// Populate telemetry for an actor from live kinematics state.
        ///
        /// Before this was wired to GazeEngine, every field was a hardcoded literal and
        /// the function returned true, so a caller could not distinguish real data from
        /// a stub. It now returns false when there is genuinely nothing to report.
        ///
        /// Audit 2026-10-15 F-03: reads the game-thread-published snapshot instead of
        /// GazeEngine's live map, so it is safe from any thread. The LOD tier is now
        /// computed on the game thread at publish time (it needs a form lookup and
        /// the actor's position, neither of which is safe to touch from here).
        bool BuildTelemetry(uint32_t actorFormId, ActorGazeTelemetry* out) noexcept
        {
            if (!out || actorFormId == 0)
            {
                return false;
            }

            Internal::TelemetrySnapshot snap{};
            if (!Internal::TryGetTelemetry(actorFormId, snap))
            {
                return false; // no live (fresh) runtime state for this actor
            }

            out->actorFormId = actorFormId;
            out->targetFormId = snap.targetFormId;
            out->gazePitchDeg = snap.gazePitchDeg;
            out->gazeYawDeg = snap.gazeYawDeg;
            out->mutualGazeDurationSec = snap.mutualGazeDurationSec;
            out->activeMode = static_cast<HcepCognitiveMode>(snap.hcepMode);
            out->isMutualGaze = snap.mutualGazeDurationSec > 0.0f ? 1 : 0;
            out->isBlinking = snap.isBlinking ? 1 : 0;
            out->gazeRegion = snap.gazeRegion;
            out->padding[0] = out->padding[1] = out->padding[2] = 0;

            // R14 E7.7 — real LOD tier from the same classifier the runtime uses,
            // replacing the placeholder that reported eye saturation as a tier.
            // A Tier 3 actor is culled before TickActor, so live state can only
            // ever be Tier 1 or Tier 2 — which is exactly what this reports.
            out->lodTier = snap.lodTier;
            return true;
        }

    } // namespace

    TRUEGAZE_API uint32_t TrueGaze_GetVersion() noexcept
    {
        // Audit 2026-10-15 F-07: previously a literal 0x01000700 ("v1.0.7") while
        // the plugin declared 1.0.8 to SKSE. Now generated from project(VERSION).
        return TRUEGAZE_VERSION_PACKED;
    }

    TRUEGAZE_API bool TrueGaze_IsHcepConnected() noexcept
    {
        // Reports the real bridge state. Previously this returned a hardcoded false,
        // so a caller could never tell "not connected" from "not implemented".
        return Engine::GazeEngine::Get().IsBridgeConnected();
    }

    TRUEGAZE_API bool TrueGaze_GetActorGaze(uint32_t actorFormId,
                                            ActorGazeTelemetry* outTelemetry) noexcept
    {
        return BuildTelemetry(actorFormId, outTelemetry);
    }

    TRUEGAZE_API void TrueGaze_OverrideActorMode(uint32_t actorFormId, HcepCognitiveMode mode,
                                                 float durationSec) noexcept
    {
        // Audit 2026-10-15 F-03: the override is queued and applied by the game
        // thread at the start of the actor's next tick (normally within one frame),
        // which then publishes the new mode to OAR / telemetry as usual. Writing
        // ActorGazeRuntime from the caller's thread was a data race.
        (void)Internal::QueueModeOverride(actorFormId, static_cast<uint8_t>(mode), durationSec);
    }

} // namespace TrueGaze::API
