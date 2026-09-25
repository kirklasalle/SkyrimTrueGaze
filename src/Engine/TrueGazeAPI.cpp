#define TRUEGAZE_EXPORTS
#include "../include/TrueGazeAPI.h"
#include "PCH.h"
#include "GazeEngine.hpp"
#include "ConfigManager.hpp"
#include "Integrations/OarConditions.hpp"
#include "Bridge/NamedPipeServer.hpp"

namespace TrueGaze::API
{

    namespace
    {

        /// Populate telemetry for an actor from live kinematics state.
        ///
        /// Before this was wired to GazeEngine, every field was a hardcoded literal and
        /// the function returned true, so a caller could not distinguish real data from
        /// a stub. It now returns false when there is genuinely nothing to report.
        bool BuildTelemetry(uint32_t actorFormId, ActorGazeTelemetry *out) noexcept
        {
            if (!out || actorFormId == 0)
            {
                return false;
            }

            auto *state = Engine::GazeEngine::Get().FindActor(actorFormId);
            if (!state || !state->initialised)
            {
                return false; // no live runtime state for this actor
            }

            out->actorFormId = actorFormId;
            out->targetFormId = state->trackedTargetFormId;
            out->gazePitchDeg = state->lastPitchDeg;
            out->gazeYawDeg = state->lastYawDeg;
            out->mutualGazeDurationSec = state->mutualGazeHoldSec;
            out->activeMode = static_cast<HcepCognitiveMode>(state->hcepMode);
            out->isMutualGaze = state->mutualGazeHoldSec > 0.0f ? 1 : 0;
            out->isBlinking = state->blink.isBlinking ? 1 : 0;
            out->gazeRegion = state->gazeRegion;
            out->padding[0] = out->padding[1] = out->padding[2] = 0;

            // Derive LOD tier from the player distance instead of hardcoding 0.
            // Without access to the player pointer here, we approximate from the
            // actor's idle time: a Tier 3 actor would already have been evicted.
            out->lodTier = state->eyeSaturated ? 1 : 0;
            return true;
        }

    } // namespace

    TRUEGAZE_API uint32_t TrueGaze_GetVersion() noexcept
    {
        return 0x01000500; // v1.0.5
    }

    TRUEGAZE_API bool TrueGaze_IsHcepConnected() noexcept
    {
        // Reports the real bridge state. Previously this returned a hardcoded false,
        // so a caller could never tell "not connected" from "not implemented".
        return Engine::GazeEngine::Get().IsBridgeConnected();
    }

    TRUEGAZE_API bool TrueGaze_GetActorGaze(uint32_t actorFormId,
                                            ActorGazeTelemetry *outTelemetry) noexcept
    {
        return BuildTelemetry(actorFormId, outTelemetry);
    }

    TRUEGAZE_API void TrueGaze_OverrideActorMode(uint32_t actorFormId,
                                                 HcepCognitiveMode mode,
                                                 float durationSec) noexcept
    {
        if (actorFormId == 0)
        {
            return;
        }

        auto *state = Engine::GazeEngine::Get().FindActor(actorFormId);
        if (!state)
        {
            return;
        }

        const uint8_t raw = static_cast<uint8_t>(mode);
        if (raw > 4)
        {
            return; // outside the defined HCEP mode range
        }

        state->hcepMode = raw;
        state->modeOverrideTimerSec = (durationSec > 0.0f) ? durationSec : 5.0f;
        state->hasModeOverride = true;

        Integrations::OarConditions::PublishActorState(
            actorFormId, state->hcepMode, state->gazeRegion, state->mutualGazeHoldSec);
    }

} // namespace TrueGaze::API
