#pragma once

#include "PCH.h"
#include <cstdint>

namespace TrueGaze::Integrations {

/// @brief Native C++ Condition Interface for Open Animation Replacer (OAR).
/// Allows animators to conditionally play custom body/gesture animations based on HCEP states.
class OarConditions
{
public:
    enum class HcepMode : uint8_t
    {
        LOGIC  = 0,
        AFFECT = 1,
        SPIRIT = 2,
        HEART  = 3,
        THINK  = 4
    };

    /// @brief Evaluates whether the specified actor is currently in the requested HCEP mode.
    static bool EvaluateIsMode(uint32_t actorFormId, uint8_t targetMode) noexcept;

    /// @brief Evaluates whether mutual eye contact with the player has exceeded the duration threshold.
    static bool EvaluateIsMutualGaze(uint32_t actorFormId, float thresholdSeconds) noexcept;

    /// @brief Evaluates whether the actor's current gaze region matches the requested region ID (0-12).
    static bool EvaluateGazeRegion(uint32_t actorFormId, uint8_t targetRegionId) noexcept;

    /// @brief Registers all TrueGaze custom conditions with Open Animation Replacer via SKSE messaging.
    static bool RegisterWithOar() noexcept;
};

} // namespace TrueGaze::Integrations
