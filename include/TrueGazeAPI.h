#pragma once

#include <cstdint>

#ifdef TRUEGAZE_EXPORTS
#define TRUEGAZE_API extern "C" __declspec(dllexport)
#else
#define TRUEGAZE_API extern "C" __declspec(dllimport)
#endif

/// @brief Public C/C++ API for Kirk LaSalle's TrueGaze™ Biomechanical Engine.
/// Third-party SKSE mod authors can include this header to interface directly with TrueGaze.
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

    /// @brief Returns the version integer (e.g. 0x01000000 -> v1.0.0).
    TRUEGAZE_API uint32_t TrueGaze_GetVersion() noexcept;

    /// @brief Returns whether TrueGaze is connected to the HCEP Desktop perception platform.
    TRUEGAZE_API bool TrueGaze_IsHcepConnected() noexcept;

    /// @brief Fetches real-time gaze telemetry for an actor in the game world.
    TRUEGAZE_API bool TrueGaze_GetActorGaze(uint32_t actorFormId,
                                            ActorGazeTelemetry* outTelemetry) noexcept;

    /// @brief Overrides an actor's cognitive mode (e.g., forcing THINK mode during specific
    /// dialogue scenes).
    TRUEGAZE_API void TrueGaze_OverrideActorMode(uint32_t actorFormId, HcepCognitiveMode mode,
                                                 float durationSec) noexcept;

} // namespace TrueGaze::API
