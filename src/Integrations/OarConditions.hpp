#pragma once

#include "PCH.h"
#include <cstdint>

namespace TrueGaze::Integrations
{

    /// @brief TrueGaze's condition state and its registration with Open Animation Replacer (OAR).
    /// Lets animators play body/gesture animations conditionally, based on HCEP state.
    ///
    /// ## Correctness notes
    ///
    /// The original implementation logged `"Registered TrueGaze_IsMode, ... conditions
    /// with OAR."` while performing no registration at all, and read from a state cache
    /// that no code ever wrote to. The practical effect was that the 7-rule OAR package
    /// fired Rule 1 unconditionally and Rules 2-7 never fired. See
    /// docs/AUDIT_REPORT_2026-09-11.md finding C-5.
    ///
    /// A later revision replaced this with an invented SKSE "registration message"
    /// sent to OpenAnimationReplacer. OAR does not listen for that message, so it
    /// again logged success for work that did not happen (issue #6). It has been
    /// removed.
    ///
    /// Rules now enforced:
    ///   1. Registration uses OAR's published Conditions API (`RequestPluginAPI_Conditions`,
    ///      interface V3), vendored unmodified in extern/OpenAnimationReplacer-API.
    ///      Implementation: OarCustomConditions.cpp.
    ///   2. `RegisterWithOar()` reports what actually happened. It returns true and logs
    ///      success only if OAR accepted every condition.
    ///   3. GazeEngine writes the cache every tick through `PublishActorState`. An entry
    ///      that has not been refreshed for `STALE_AFTER_SEC` counts as unknown, so an
    ///      NPC who leaves TrueGaze's processing range cannot keep matching an old mode.
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

        /// Cache entries older than this are treated as "no state" by every evaluator.
        static constexpr float STALE_AFTER_SEC = 2.0f;

        /// Snapshot of one actor's published state.
        struct ActorState
        {
            uint8_t hcepMode{0};
            uint8_t gazeRegion{0};
            float mutualGazeHoldSec{0.0f};
        };

        /// @brief Writes an actor's live gaze state into the condition cache.
        ///
        /// Called from the game thread every frame by GazeEngine. OAR reads the cache
        /// during its own evaluation (possibly off the main thread), so this must stay
        /// cheap: one short exclusive lock and one map insert.
        static void PublishActorState(uint32_t actorFormId,
                                      uint8_t hcepMode,
                                      uint8_t gazeRegion,
                                      float mutualGazeHoldSec) noexcept;

        /// @brief Drops all cached actor state. Call on cell change and game load.
        static void ClearCache() noexcept;

        /// @brief Number of actors currently published to the cache. Diagnostic.
        [[nodiscard]] static size_t CachedActorCount() noexcept;

        /// @brief Fetches an actor's fresh published state.
        /// @return false if the actor has no state, or its state is stale.
        [[nodiscard]] static bool TryGetActorState(uint32_t actorFormId, ActorState &out) noexcept;

        /// @brief Whether OAR accepted every TrueGaze condition.
        [[nodiscard]] static bool IsRegisteredWithOar() noexcept;

        /// @brief Evaluates whether the specified actor is currently in the requested HCEP mode.
        static bool EvaluateIsMode(uint32_t actorFormId, uint8_t targetMode) noexcept;

        /// @brief Evaluates whether mutual eye contact with the player has exceeded the duration threshold.
        static bool EvaluateIsMutualGaze(uint32_t actorFormId, float thresholdSeconds) noexcept;

        /// @brief Evaluates whether the actor's current gaze region matches the requested region ID (0-12).
        static bool EvaluateGazeRegion(uint32_t actorFormId, uint8_t targetRegionId) noexcept;

        /// @brief Registers TrueGaze_IsMode, TrueGaze_IsMutualGaze and TrueGaze_GetGazeRegion
        /// as OAR custom conditions.
        ///
        /// Call during SKSE `kPostLoad`. After that point OAR has built its condition-factory
        /// map and later registrations have no effect. Idempotent once it has succeeded.
        /// @return true only if OAR accepted all three conditions.
        static bool RegisterWithOar() noexcept;
    };

} // namespace TrueGaze::Integrations
