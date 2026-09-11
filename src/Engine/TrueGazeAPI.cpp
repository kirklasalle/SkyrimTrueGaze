#define TRUEGAZE_EXPORTS
#include "../include/TrueGazeAPI.h"
#include "PCH.h"
#include "Bridge/NamedPipeServer.hpp"

namespace TrueGaze::API {

TRUEGAZE_API uint32_t TrueGaze_GetVersion() noexcept
{
    return 0x01000000; // v1.0.0
}

TRUEGAZE_API bool TrueGaze_IsHcepConnected() noexcept
{
    // Check pipe server status if active
    return false;
}

TRUEGAZE_API bool TrueGaze_GetActorGaze(uint32_t actorFormId, ActorGazeTelemetry* outTelemetry) noexcept
{
    if (!outTelemetry || actorFormId == 0) return false;

    outTelemetry->actorFormId = actorFormId;
    outTelemetry->targetFormId = 0x14; // Default player FormID
    outTelemetry->gazePitchDeg = 0.0f;
    outTelemetry->gazeYawDeg = 0.0f;
    outTelemetry->mutualGazeDurationSec = 0.0f;
    outTelemetry->activeMode = HcepCognitiveMode::LOGIC;
    outTelemetry->isMutualGaze = 0;
    outTelemetry->isBlinking = 0;
    outTelemetry->lodTier = 0;
    return true;
}

TRUEGAZE_API void TrueGaze_OverrideActorMode(uint32_t /*actorFormId*/, HcepCognitiveMode /*mode*/, float /*durationSec*/) noexcept
{
    // Mode override logic for dialogue scripting
}

} // namespace TrueGaze::API
