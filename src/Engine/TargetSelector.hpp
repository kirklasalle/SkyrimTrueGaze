#pragma once

#include "PCH.h"
#include <cstdint>

namespace TrueGaze::Engine {

/// @brief Target Salience and Spatial Priority Selector.
/// Resolves the optimal 3D focus point for each active actor in the game world.
class TargetSelector
{
public:
    enum class TargetPriority : uint8_t
    {
        None            = 0,
        AmbientInterest = 1, // Torches, birds, landscape horizon
        NearbyActor     = 2, // Approaching friendly NPCs or player
        CombatTarget    = 3, // Hostile adversary in combat
        DialoguePartner = 4  // Active conversational partner (Highest)
    };

    struct GazeTarget
    {
        uint32_t targetFormId{ 0 };
        TargetPriority priority{ TargetPriority::None };
        float worldX{ 0.0f };
        float worldY{ 0.0f };
        float worldZ{ 0.0f };
        float distanceMeters{ 0.0f };
        bool isPlayer{ false };
    };

    /// @brief Resolves the highest-salience target for an actor within their visual cone.
    static GazeTarget ResolveTarget(uint32_t observerFormId) noexcept;
};

} // namespace TrueGaze::Engine
