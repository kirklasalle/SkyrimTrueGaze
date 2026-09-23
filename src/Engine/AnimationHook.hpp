#pragma once

#include "PCH.h"
#include <cstdint>

namespace TrueGaze::Engine
{

    /// @brief Drives the gaze kinematics engine each frame and applies it to actor skeletons.
    ///
    /// Installed on Actor/Character/PlayerCharacter Update vtables. Each actor's
    /// previous procedural pose is restored before Skyrim animates it, then fresh
    /// gaze is applied after the update and retained through rendering.
    class AnimationHook
    {
    public:
        /// @brief Installs the per-frame gaze driver.
        static void Install() noexcept;

        /// @brief Evaluates and applies gaze kinematics for every eligible loaded actor.
        static void TickAllActors(float deltaSeconds) noexcept;

        /// @brief Releases modified bones and retires idle actors. Call after ticking.
        static void FrameEnd() noexcept;

        /// @brief Evaluates whether an actor should receive procedural gaze overrides.
        /// Returns false if the actor is dead, disabled, deleted, or has no 3D root.
#if __has_include(<RE/Skyrim.h>)
        static bool IsActorEligibleForGaze(RE::Actor *actor) noexcept;
#endif
        static bool IsActorEligibleForGaze(uint32_t actorFormId) noexcept;

        /// @brief Gets the current animation blend weight [0.0f, 1.0f] for an actor.
        /// Normal dialogue/idle = 1.0f; Heavy combat = 0.45f; Sleeping/Dead = 0.0f.
        static float GetActorGazeWeight(uint32_t actorFormId) noexcept;
    };

} // namespace TrueGaze::Engine
