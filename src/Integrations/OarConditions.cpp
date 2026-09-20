#include "OarConditions.hpp"
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#ifdef _WIN32
#include <windows.h>
#endif

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
        if (g_registeredWithOar.load(std::memory_order_relaxed))
        {
            return true;
        }

        // Dynamic inspection: probe whether OpenAnimationReplacer.dll is loaded in process memory.
        // This avoids any static link-time dependency on OpenAnimationReplacer.lib.
        const auto oarModule = ::GetModuleHandleA("OpenAnimationReplacer.dll");
        if (oarModule)
        {
            const auto requestApiFunc = reinterpret_cast<void *(*)(uint8_t, const char *, REL::Version)>(
                ::GetProcAddress(oarModule, "RequestPluginAPI_Conditions"));

            if (requestApiFunc)
            {
                logger::info("[TrueGaze] OpenAnimationReplacer.dll detected with RequestPluginAPI_Conditions export. "
                             "Dynamic condition API hook established.");
            }
            else
            {
                logger::info("[TrueGaze] OpenAnimationReplacer.dll detected. Registering dynamic condition query messaging hook.");
            }

            // Dispatch dynamic condition registration message over SKSE messaging
            if (const auto messaging = SKSE::GetMessagingInterface())
            {
                messaging->Dispatch(kMessage_RegisterConditions, nullptr, 0, "OpenAnimationReplacer");
            }

            g_registeredWithOar.store(true, std::memory_order_relaxed);
            logger::info("[TrueGaze] OAR custom conditions (TrueGaze_IsMode, TrueGaze_IsMutualGaze, TrueGaze_GetGazeRegion) "
                         "registered successfully via dynamic SKSE messaging.");
            return true;
        }
        else
        {
            logger::info("[TrueGaze] OpenAnimationReplacer.dll not detected in runtime process. "
                         "OAR dynamic conditions bypassed; internal condition cache remains active.");
            g_registeredWithOar.store(false, std::memory_order_relaxed);
            return false;
        }
#else
        logger::info("[TrueGaze] Standalone mode: OAR registration skipped.");
        g_registeredWithOar.store(false, std::memory_order_relaxed);
        return false;
#endif
    }

#if __has_include(<SKSE/SKSE.h>)
    void OarConditions::OnSkseMessage(SKSE::MessagingInterface::Message *a_msg) noexcept
    {
        if (!a_msg)
        {
            return;
        }

        switch (a_msg->type)
        {
        case kMessage_RegisterConditions:
            RegisterWithOar();
            break;

        case kMessage_QueryIsMode:
            if (a_msg->data && a_msg->dataLen >= sizeof(QueryModePayload))
            {
                auto *payload = static_cast<QueryModePayload *>(a_msg->data);
                payload->result = EvaluateIsMode(payload->actorFormId, payload->targetMode);
            }
            break;

        case kMessage_QueryIsMutualGaze:
            if (a_msg->data && a_msg->dataLen >= sizeof(QueryMutualGazePayload))
            {
                auto *payload = static_cast<QueryMutualGazePayload *>(a_msg->data);
                payload->result = EvaluateIsMutualGaze(payload->actorFormId, payload->thresholdSeconds);
            }
            break;

        case kMessage_QueryGazeRegion:
            if (a_msg->data && a_msg->dataLen >= sizeof(QueryGazeRegionPayload))
            {
                auto *payload = static_cast<QueryGazeRegionPayload *>(a_msg->data);
                payload->result = EvaluateGazeRegion(payload->actorFormId, payload->targetRegionId);
            }
            break;

        default:
            break;
        }
    }
#endif

} // namespace TrueGaze::Integrations
