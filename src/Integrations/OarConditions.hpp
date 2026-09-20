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

        /// @brief Custom message types for SKSE dynamic messaging interface.
        enum MessageType : uint32_t
        {
            kMessage_RegisterConditions = 0x54473031, // 'TG01' - Dynamic condition registration
            kMessage_QueryIsMode        = 0x54473032, // 'TG02' - Query if actor is in HCEP mode
            kMessage_QueryIsMutualGaze  = 0x54473033, // 'TG03' - Query if actor holds mutual gaze
            kMessage_QueryGazeRegion    = 0x54473034  // 'TG04' - Query actor's gaze region
        };

        struct QueryModePayload
        {
            uint32_t actorFormId{0};
            uint8_t targetMode{0};
            bool result{false};
        };

        struct QueryMutualGazePayload
        {
            uint32_t actorFormId{0};
            float thresholdSeconds{0.0f};
            bool result{false};
        };

        struct QueryGazeRegionPayload
        {
            uint32_t actorFormId{0};
            uint8_t targetRegionId{0};
            bool result{false};
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

        /// @brief Registers TrueGaze custom conditions dynamically with Open Animation Replacer via SKSE messaging.
        /// @return true if OAR is detected and dynamic condition hook is established.
        static bool RegisterWithOar() noexcept;

#if __has_include(<SKSE/SKSE.h>)
        /// @brief Handles incoming dynamic messages from SKSE and external condition evaluators.
        static void OnSkseMessage(SKSE::MessagingInterface::Message *a_msg) noexcept;
#endif
    };

} // namespace TrueGaze::Integrations
