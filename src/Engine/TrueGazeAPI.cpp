#define TRUEGAZE_EXPORTS
#include "../include/TrueGazeAPI.h"
#include "Bridge/NamedPipeServer.hpp"
#include "ConfigManager.hpp"
#include "GazeEngine.hpp"
#include "Integrations/OarConditions.hpp"
#include "LodManager.hpp"
#include "PCH.h"

namespace TrueGaze::API
{

    namespace
    {

        /// Player distance in metres for the actor, or 0 when unavailable.
        /// Thin wrapper over GazeEngine's own tier-distance helper so the API
        /// and the runtime agree on the same number.
        float DistanceForActor(uint32_t actorFormId) noexcept
        {
#if __has_include(<RE/Skyrim.h>)
            auto* actor = RE::TESForm::LookupByID<RE::Actor>(actorFormId);
            return actor ? Engine::GazeEngine::DistanceMetersForTier(actor) : 0.0f;
#else
            (void)actorFormId;
            return 0.0f;
#endif
        }

        /// Populate telemetry for an actor from live kinematics state.
        ///
        /// Before this was wired to GazeEngine, every field was a hardcoded literal and
        /// the function returned true, so a caller could not distinguish real data from
        /// a stub. It now returns false when there is genuinely nothing to report.
        bool BuildTelemetry(uint32_t actorFormId, ActorGazeTelemetry* out) noexcept
        {
            if (!out || actorFormId == 0)
            {
                return false;
            }

            auto* state = Engine::GazeEngine::Get().FindActor(actorFormId);
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

            // R14 E7.7 — real LOD tier from the same classifier the runtime uses,
            // replacing the placeholder that reported eye saturation as a tier.
            // A Tier 3 actor is culled before TickActor, so live state can only
            // ever be Tier 1 or Tier 2 — which is exactly what this reports.
            out->lodTier =
                static_cast<uint8_t>(Engine::LodManager::GetLodTier(DistanceForActor(actorFormId)));
            return true;
        }

    } // namespace

    TRUEGAZE_API uint32_t TrueGaze_GetVersion() noexcept
    {
        return 0x01000600; // v1.0.6
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
        if (actorFormId == 0)
        {
            return;
        }

        auto* state = Engine::GazeEngine::Get().FindActor(actorFormId);
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

        Integrations::OarConditions::PublishActorState(actorFormId, state->hcepMode,
                                                       state->gazeRegion, state->mutualGazeHoldSec);
    }

} // namespace TrueGaze::API
