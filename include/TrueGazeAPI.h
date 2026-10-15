#pragma once

#include <cstdint>

#ifdef TRUEGAZE_EXPORTS
#define TRUEGAZE_API extern "C" __declspec(dllexport)
#else
#define TRUEGAZE_API extern "C" __declspec(dllimport)
#endif

/// @brief Public C/C++ API for Kirk LaSalle's TrueGaze™ Biomechanical Engine.
/// Third-party SKSE mod authors can include this header to interface directly with TrueGaze.
///
/// THREADING (audit 2026-10-15 F-03): every function below is safe to call from
/// any thread. Telemetry is a snapshot the game thread publishes once per actor
/// tick (so it may lag the live pose by up to one frame), and an actor that has
/// not been ticked for ~2 s reports as unavailable. Mode overrides are queued and
/// applied by the game thread on the actor's next tick.
namespace TrueGaze::API
{

    enum class HcepCognitiveMode : uint8_t
    {
        LOGIC = 0,  // Analytical, structured, direct facial fixation
        AFFECT = 1, // Empathetic dialogue, social triangle eye-mouth scanning
        SPIRIT = 2, // Sustained deep mutual eye contact
        HEART = 3,  // Lower face empathic resonance
        THINK = 4   // Cognitive gaze aversion (defocused, processing)
    };

#pragma pack(push, 1)
    struct ActorGazeTelemetry
    {
        uint32_t actorFormId;  ///< FormID of the observed actor (0 = invalid).
        uint32_t targetFormId; ///< FormID of the actor's current gaze target (0 = ambient/vacant).
        float gazePitchDeg;    ///< Actor-relative pitch deflection in degrees (positive = up).
        float gazeYawDeg;      ///< Actor-relative yaw deflection in degrees (positive = right).
        float mutualGazeDurationSec;  ///< Continuous seconds of mutual eye contact with the player.
        HcepCognitiveMode activeMode; ///< Live HCEP cognitive mode (LOGIC..THINK).
        uint8_t isMutualGaze;         ///< 1 while mutualGazeDurationSec > 0.
        uint8_t isBlinking;           ///< 1 while the eyelid blink controller has the eyes closed.
        uint8_t lodTier;              ///< Runtime LOD tier: 0 = Tier1 (<5 m, full kinematics),
                                      ///< 1 = Tier2 (5-15 m, simplified). Tier3 actors are culled
                                      ///< before ticking and never appear in live telemetry.
        uint8_t gazeRegion; ///< Classified gaze region (0..12), from HCEP-02 Enhanced Diagram.
        uint8_t padding[3]; ///< Reserved for ABI stability.
    };
#pragma pack(pop)

    /// @brief Returns the packed plugin version 0xMMmmpp00 (e.g. 0x01000800 -> v1.0.8).
    TRUEGAZE_API uint32_t TrueGaze_GetVersion() noexcept;

    /// @brief Returns whether TrueGaze is connected to the HCEP Desktop perception platform.
    TRUEGAZE_API bool TrueGaze_IsHcepConnected() noexcept;

    /// @brief Fetches real-time gaze telemetry for an actor in the game world.
    /// @return false if the actor is not currently processed by TrueGaze (out of
    ///         range, culled, evicted) or @p outTelemetry is null.
    TRUEGAZE_API bool TrueGaze_GetActorGaze(uint32_t actorFormId,
                                            ActorGazeTelemetry* outTelemetry) noexcept;

    /// @brief Overrides an actor's cognitive mode (e.g., forcing THINK mode during specific
    /// dialogue scenes).
    /// The request is ignored if the actor is not currently processed by TrueGaze or
    /// @p mode is out of range. @p durationSec <= 0 (or non-finite) means 5 seconds.
    /// Applied on the actor's next game-thread tick; a newer call replaces an older
    /// request that has not yet been applied.
    TRUEGAZE_API void TrueGaze_OverrideActorMode(uint32_t actorFormId, HcepCognitiveMode mode,
                                                 float durationSec) noexcept;

} // namespace TrueGaze::API
