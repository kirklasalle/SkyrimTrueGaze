#pragma once

#include "PCH.h"
#include <cstdint>

namespace TrueGaze::Engine {

/// @brief Hook for post-Havok animation evaluation (RE::Actor::ModifyAnimationUpdateData).
/// Injects additive biological gaze rotations on top of existing Havok animations without conflicts.
class AnimationHook
{
public:
    /// @brief Installs the function hook into Skyrim's actor animation update loop.
    static void Install() noexcept;

    /// @brief Evaluates whether an actor should receive procedural gaze overrides.
    /// Returns false if the actor is dead, paralyzed, sleeping, or ragdolled.
    static bool IsActorEligibleForGaze(uint32_t actorFormId) noexcept;

    /// @brief Gets the current animation blend weight [0.0f, 1.0f] for an actor.
    /// Normal dialogue/idle = 1.0f; Heavy combat = 0.35f; Sleeping/Dead = 0.0f.
    static float GetActorGazeWeight(uint32_t actorFormId) noexcept;
};

} // namespace TrueGaze::Engine
