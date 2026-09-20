#include "OarConditions.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <atomic>

namespace TrueGaze::Integrations
{

    namespace
    {

        struct ActorGazeInfo
        {
            uint8_t mode{0};               // HCEP Mode 0..4
            float mutualGazeHoldSec{0.0f}; // Duration of continuous mutual gaze
            uint8_t regionId{0};           // Region ID 0..12
        };

        std::shared_mutex g_cacheMutex;
        std::unordered_map<uint32_t, ActorGazeInfo> g_actorGazeCache;

        /// Whether OAR accepted our registration. Set only by RegisterWithOar().
        std::atomic<bool> g_registeredWithOar{false};

    } // namespace

    // ---------------------------------------------------------------------------
    // State publishing — the half that was missing
    // ---------------------------------------------------------------------------

    void OarConditions::PublishActorState(uint32_t actorFormId,
                                          uint8_t hcepMode,
                                          uint8_t gazeRegion,
                                          float mutualGazeHoldSec) noexcept
    {
        if (actorFormId == 0)
        {
            return;
        }

        // Clamp to the documented ranges so a bad caller cannot poison the cache.
        const uint8_t mode = hcepMode > 4 ? 4 : hcepMode;
        const uint8_t region = gazeRegion > 12 ? 12 : gazeRegion;
        const float hold = mutualGazeHoldSec < 0.0f ? 0.0f : mutualGazeHoldSec;

        std::unique_lock lock(g_cacheMutex);
        auto &entry = g_actorGazeCache[actorFormId];
        entry.mode = mode;
        entry.regionId = region;
        entry.mutualGazeHoldSec = hold;
    }

    void OarConditions::ClearCache() noexcept
    {
        std::unique_lock lock(g_cacheMutex);
        g_actorGazeCache.clear();
    }

    size_t OarConditions::CachedActorCount() noexcept
    {
        std::shared_lock lock(g_cacheMutex);
        return g_actorGazeCache.size();
    }

    bool OarConditions::IsRegisteredWithOar() noexcept
    {
        return g_registeredWithOar.load(std::memory_order_relaxed);
    }

    // ---------------------------------------------------------------------------
    // Condition evaluation
    // ---------------------------------------------------------------------------

    bool OarConditions::EvaluateIsMode(uint32_t actorFormId, uint8_t targetMode) noexcept
    {
        std::shared_lock lock(g_cacheMutex);
        auto it = g_actorGazeCache.find(actorFormId);
        if (it != g_actorGazeCache.end())
        {
            return it->second.mode == targetMode;
        }
        // An actor with no published state is not in any specific mode. Returning
        // true for LOGIC here (as the original did) made Rule 1 fire unconditionally.
        return false;
    }

    bool OarConditions::EvaluateIsMutualGaze(uint32_t actorFormId, float thresholdSeconds) noexcept
    {
        std::shared_lock lock(g_cacheMutex);
        auto it = g_actorGazeCache.find(actorFormId);
        if (it != g_actorGazeCache.end())
        {
            return it->second.mutualGazeHoldSec >= thresholdSeconds;
        }
        return false;
    }

    bool OarConditions::EvaluateGazeRegion(uint32_t actorFormId, uint8_t targetRegionId) noexcept
    {
        std::shared_lock lock(g_cacheMutex);
        auto it = g_actorGazeCache.find(actorFormId);
        if (it != g_actorGazeCache.end())
        {
            return it->second.regionId == targetRegionId;
        }
        return false;
    }

    // ---------------------------------------------------------------------------
    // Registration
    // ---------------------------------------------------------------------------

    bool OarConditions::RegisterWithOar() noexcept
    {
#if __has_include(<SKSE/SKSE.h>)
        // OAR exposes custom conditions to plugins through its own API interface,
        // obtained over the SKSE messaging system. That interface is versioned and
        // its exact contract is not vendored in this repository, so registration is
        // NOT performed here.
        //
        // This is reported honestly rather than logged as success. The previous
        // implementation logged "Registered TrueGaze_IsMode, ... conditions with OAR."
        // while doing nothing, which is precisely the false-success reporting the
        // September 2026 audit identified as a Law 7 violation.
        //
        // What DOES work today: the condition cache is populated every frame by
        // PublishActorState(), so the evaluators above return correct answers. Any
        // consumer that can reach them — a future OAR binding or the public C API —
        // gets real data.
        logger::warn("[TrueGaze] OAR condition registration is NOT implemented. "
                     "Condition state is published and evaluators are live, but no "
                     "OAR API binding exists yet. OAR rules will not fire. "
                     "See GOVERNANCE.md and issue #6.");

        g_registeredWithOar.store(false, std::memory_order_relaxed);
        return false;
#else
        logger::info("[TrueGaze] Standalone mode: OAR registration skipped.");
        g_registeredWithOar.store(false, std::memory_order_relaxed);
        return false;
#endif
    }

} // namespace TrueGaze::Integrations
