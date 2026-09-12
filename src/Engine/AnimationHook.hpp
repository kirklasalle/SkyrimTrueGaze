#pragma once

#include "PCH.h"
#include <cstdint>

namespace TrueGaze::Engine
{

    /// @brief Drives the gaze simulation each frame and applies it to actor skeletons.
    ///
    /// Installed on the main update loop rather than on an animation function. See
    /// the rationale in AnimationHook.cpp — briefly, a vtable hook on a stable index
    /// cannot silently fail to bind the way an unverified address relocation can.
    class AnimationHook
    {
    public:
        /// @brief Installs the per-frame gaze driver.
        static void Install() noexcept;

        /// @brief Simulates and applies gaze for every eligible loaded actor.
        static void TickAllActors(float deltaSeconds) noexcept;

        /// @brief Releases modified bones and retires idle actors. Call after ticking.
        static void FrameEnd() noexcept;

        /// @brief Evaluates whether an actor should receive procedural gaze overrides.
        /// Returns false if the actor is dead, paralyzed, sleeping, or ragdolled.
        static bool IsActorEligibleForGaze(uint32_t actorFormId) noexcept;

        /// @brief Gets the current animation blend weight [0.0f, 1.0f] for an actor.
        /// Normal dialogue/idle = 1.0f; Heavy combat = 0.45f; Sleeping/Dead = 0.0f.
        static float GetActorGazeWeight(uint32_t actorFormId) noexcept;
    };

} // namespace TrueGaze::Engine
