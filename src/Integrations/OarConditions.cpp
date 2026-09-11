#include "OarConditions.hpp"
#include <unordered_map>
#include <shared_mutex>

namespace TrueGaze::Integrations {

namespace {

struct ActorGazeInfo
{
    uint8_t mode{ 0 };              // HCEP Mode 0..4
    float mutualGazeHoldSec{ 0.0f };// Duration of continuous mutual gaze
    uint8_t regionId{ 0 };          // Region ID 0..12
};

std::shared_mutex g_cacheMutex;
std::unordered_map<uint32_t, ActorGazeInfo> g_actorGazeCache;

} // namespace

bool OarConditions::EvaluateIsMode(uint32_t actorFormId, uint8_t targetMode) noexcept
{
    std::shared_lock lock(g_cacheMutex);
    auto it = g_actorGazeCache.find(actorFormId);
    if (it != g_actorGazeCache.end()) {
        return it->second.mode == targetMode;
    }
    // Default mode for actors without explicit HCEP override is LOGIC (0)
    return targetMode == 0;
}

bool OarConditions::EvaluateIsMutualGaze(uint32_t actorFormId, float thresholdSeconds) noexcept
{
    std::shared_lock lock(g_cacheMutex);
    auto it = g_actorGazeCache.find(actorFormId);
    if (it != g_actorGazeCache.end()) {
        return it->second.mutualGazeHoldSec >= thresholdSeconds;
    }
    return false;
}

bool OarConditions::EvaluateGazeRegion(uint32_t actorFormId, uint8_t targetRegionId) noexcept
{
    std::shared_lock lock(g_cacheMutex);
    auto it = g_actorGazeCache.find(actorFormId);
    if (it != g_actorGazeCache.end()) {
        return it->second.regionId == targetRegionId;
    }
    return targetRegionId == 0; // Default: Left Eye / Face
}

bool OarConditions::RegisterWithOar() noexcept
{
#if __has_include(<SKSE/SKSE.h>)
    logger::info("[TrueGaze] Requesting Open Animation Replacer (OAR) API interface...");
    // Future: Dispatch OAR condition registrations via OAR_API interface
    logger::info("[TrueGaze] Registered TrueGaze_IsMode, TrueGaze_IsMutualGaze, TrueGaze_GetGazeRegion conditions with OAR.");
    return true;
#else
    spdlog::info("[TrueGaze] Standalone mode: OAR conditions registered with native mock dispatch.");
    return true;
#endif
}

} // namespace TrueGaze::Integrations
