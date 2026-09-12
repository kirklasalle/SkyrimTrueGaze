#pragma once

#include "PCH.h"
#include <cstdint>

namespace TrueGaze::Integrations
{

    /// @brief Native C++ Condition Interface for Open Animation Replacer (OAR).
    /// Allows animators to conditionally play custom body/gesture animations based on HCEP states.
    ///
    /// ## Correctness notes
    ///
    /// The original implementation logged `"Registered TrueGaze_IsMode, ... conditions
    /// with OAR."` while performing no registration at all, and read from a state cache
    /// that no code ever wrote to. The practical effect was that the 7-rule OAR package
    /// fired Rule 1 unconditionally and Rules 2-7 never fired. See
    /// docs/AUDIT_REPORT_2026-09-11.md finding C-5.
    ///
    /// Two rules are now enforced:
    ///   1. `RegisterWithOar()` reports what actually happened. It never logs success
    ///      for registration it did not perform.
    ///   2. The cache is written every tick by `PublishActorState`, which GazeEngine
    ///      calls after computing each actor's state.
    class OarConditions
    {
    public:
        enum class HcepMode : uint8_t
        {
            LOGIC = 0,
            AFFECT = 1,
            SPIRIT = 2,
            HEART = 3,
            THINK = 4
        };

        /// @brief Writes an actor's live gaze state into the condition cache.
        ///
        /// Called from the game thread every frame by GazeEngine. OAR reads the cache
        /// during its own evaluation tick, so this must stay cheap: one short
        /// shared-lock and one map insert.
        static void PublishActorState(uint32_t actorFormId,
                                      uint8_t hcepMode,
                                      uint8_t gazeRegion,
                                      float mutualGazeHoldSec) noexcept;

        /// @brief Drops all cached actor state. Call on cell change and game load.
        static void ClearCache() noexcept;

        /// @brief Number of actors currently published to the cache. Diagnostic.
        [[nodiscard]] static size_t CachedActorCount() noexcept;

        /// @brief Whether OAR accepted our condition registration.
        [[nodiscard]] static bool IsRegisteredWithOar() noexcept;

        /// @brief Evaluates whether the specified actor is currently in the requested HCEP mode.
        static bool EvaluateIsMode(uint32_t actorFormId, uint8_t targetMode) noexcept;

        /// @brief Evaluates whether mutual eye contact with the player has exceeded the duration threshold.
        static bool EvaluateIsMutualGaze(uint32_t actorFormId, float thresholdSeconds) noexcept;

        /// @brief Evaluates whether the actor's current gaze region matches the requested region ID (0-12).
        static bool EvaluateGazeRegion(uint32_t actorFormId, uint8_t targetRegionId) noexcept;

        /// @brief Registers TrueGaze custom conditions with Open Animation Replacer.
        /// @return true only if registration actually succeeded. OAR being absent is a
        ///         normal condition and is logged as information, not as success.
        static bool RegisterWithOar() noexcept;
    };

} // namespace TrueGaze::Integrations
