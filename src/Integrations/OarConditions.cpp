#include "OarConditions.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#ifdef _WIN32
#include <windows.h>
#endif

#if __has_include(<SKSE/SKSE.h>)
#include "OarCustomConditions.hpp"
#endif

namespace TrueGaze::Integrations
{

    namespace
    {

        using Clock = std::chrono::steady_clock;

        struct ActorGazeInfo
        {
            uint8_t mode{0};               // HCEP Mode 0..4
            float mutualGazeHoldSec{0.0f}; // Duration of continuous mutual gaze
            uint8_t regionId{0};           // Region ID 0..12
            Clock::time_point updatedAt{}; // Last PublishActorState for this actor
        };

        std::shared_mutex g_cacheMutex;
        std::unordered_map<uint32_t, ActorGazeInfo> g_actorGazeCache;

        /// Whether OAR accepted our registration. Set only by RegisterWithOar().
        std::atomic<bool> g_registeredWithOar{false};

        [[nodiscard]] bool IsFresh(const ActorGazeInfo &entry, Clock::time_point now) noexcept
        {
            return (now - entry.updatedAt) <=
                   std::chrono::duration<float>(OarConditions::STALE_AFTER_SEC);
        }

    } // namespace

    // ---------------------------------------------------------------------------
    // State publishing
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
        const auto now = Clock::now();

        std::unique_lock lock(g_cacheMutex);
        auto &entry = g_actorGazeCache[actorFormId];
        entry.mode = mode;
        entry.regionId = region;
        entry.mutualGazeHoldSec = hold;
        entry.updatedAt = now;
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

    bool OarConditions::TryGetActorState(uint32_t actorFormId, ActorState &out) noexcept
    {
        const auto now = Clock::now();

        std::shared_lock lock(g_cacheMutex);
        const auto it = g_actorGazeCache.find(actorFormId);
        if (it == g_actorGazeCache.end() || !IsFresh(it->second, now))
        {
            // No state, or the actor has left TrueGaze's processing set and its
            // last state is no longer true. Either way, "unknown".
            return false;
        }

        out.hcepMode = it->second.mode;
        out.gazeRegion = it->second.regionId;
        out.mutualGazeHoldSec = it->second.mutualGazeHoldSec;
        return true;
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
        // An actor with no fresh published state is not in any specific mode.
        // Returning true for LOGIC here (as the original did) made Rule 1 fire
        // unconditionally.
        ActorState state;
        return TryGetActorState(actorFormId, state) && state.hcepMode == targetMode;
    }

    bool OarConditions::EvaluateIsMutualGaze(uint32_t actorFormId, float thresholdSeconds) noexcept
    {
        ActorState state;
        return TryGetActorState(actorFormId, state) && state.mutualGazeHoldSec >= thresholdSeconds;
    }

    bool OarConditions::EvaluateGazeRegion(uint32_t actorFormId, uint8_t targetRegionId) noexcept
    {
        ActorState state;
        return TryGetActorState(actorFormId, state) && state.gazeRegion == targetRegionId;
    }

    // ---------------------------------------------------------------------------
    // Registration
    // ---------------------------------------------------------------------------

    bool OarConditions::RegisterWithOar() noexcept
    {
#if __has_include(<SKSE/SKSE.h>)
        if (g_registeredWithOar.load(std::memory_order_relaxed))
        {
            return true;
        }

        // OAR is optional. Probe first, so its absence is reported as such rather
        // than as an API failure.
        if (!::GetModuleHandleA("OpenAnimationReplacer.dll"))
        {
            logger::info("[TrueGaze] Open Animation Replacer is not installed; TrueGaze OAR "
                         "conditions were not registered. Papyrus and C API condition queries "
                         "are unaffected.");
            return false;
        }

        bool ok = false;
        try
        {
            ok = OarCustomConditions::RegisterAll();
        }
        catch (const std::exception &e)
        {
            logger::error("[TrueGaze] OAR condition registration threw: {}", e.what());
            ok = false;
        }
        catch (...)
        {
            logger::error("[TrueGaze] OAR condition registration threw an unknown exception.");
            ok = false;
        }

        g_registeredWithOar.store(ok, std::memory_order_relaxed);
        return ok;
#else
        logger::info("[TrueGaze] Standalone mode: OAR registration skipped.");
        g_registeredWithOar.store(false, std::memory_order_relaxed);
        return false;
#endif
    }

} // namespace TrueGaze::Integrations
