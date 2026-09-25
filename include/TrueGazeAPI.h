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
        uint32_t actorFormId;
        uint32_t targetFormId;
        float gazePitchDeg;
        float gazeYawDeg;
        float mutualGazeDurationSec;
        HcepCognitiveMode activeMode;
        uint8_t isMutualGaze;
        uint8_t isBlinking;
        uint8_t lodTier;
        uint8_t gazeRegion; ///< Classified gaze region (0..12), from HCEP-02 Enhanced Diagram.
        uint8_t padding[3]; ///< Reserved for ABI stability.
    };
#pragma pack(pop)

    /// @brief Returns the version integer (e.g. 0x01000000 -> v1.0.0).
    TRUEGAZE_API uint32_t TrueGaze_GetVersion() noexcept;

    /// @brief Returns whether TrueGaze is connected to the HCEP Desktop perception platform.
    TRUEGAZE_API bool TrueGaze_IsHcepConnected() noexcept;

    /// @brief Fetches real-time gaze telemetry for an actor in the game world.
    TRUEGAZE_API bool TrueGaze_GetActorGaze(uint32_t actorFormId, ActorGazeTelemetry *outTelemetry) noexcept;

    /// @brief Overrides an actor's cognitive mode (e.g., forcing THINK mode during specific dialogue scenes).
    TRUEGAZE_API void TrueGaze_OverrideActorMode(uint32_t actorFormId, HcepCognitiveMode mode, float durationSec) noexcept;

} // namespace TrueGaze::API
