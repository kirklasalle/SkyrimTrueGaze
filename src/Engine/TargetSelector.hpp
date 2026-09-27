#pragma once

#include "PCH.h"
#include <cstdint>

namespace TrueGaze::Engine
{

    struct ActorGazeRuntime;

    /// @brief Target Salience and Spatial Priority Selector.
    /// Resolves the optimal 3D focus point for each active actor in the game world.
    class TargetSelector
    {
    public:
        enum class TargetPriority : uint8_t
        {
            None = 0,
            AmbientInterest = 1, // Torches, birds, landscape horizon
            NearbyActor = 2,     // Approaching friendly NPCs or player
            CombatTarget = 3,    // Hostile adversary in combat
            DialoguePartner = 4, // Active conversational partner
            CrosshairFocus = 5   // Player's crosshair is on this actor's face (Highest)
        };

        struct GazeTarget
        {
            uint32_t targetFormId{0};
            TargetPriority priority{TargetPriority::None};
            float worldX{0.0f};
            float worldY{0.0f};
            float worldZ{0.0f};
            float distanceMeters{0.0f};
            bool isPlayer{false};
        };

        /// @brief Resolves the highest-salience target for an actor within their visual cone.
        static GazeTarget ResolveTarget(uint32_t observerFormId,
                                        ActorGazeRuntime *state = nullptr,
                                        float deltaSeconds = 0.0f) noexcept;

        /// @brief Tunable crosshair sweet-spot parameters, snapshotted from config.
        /// Mirrors PlayerGazeResolver::Params; held here so ResolveTarget's caller
        /// passes one struct.
        struct CrosshairParams
        {
            bool enabled{true};
            float baseToleranceDeg{4.0f};
            float maxRangeMeters{25.0f};
            float pointBlankMeters{1.5f};
        };

        /// @brief Crosshair sweet-spot parameters for the current frame.
        /// Set by GazeEngine::RefreshTuning from configuration; read by ResolveTarget.
        /// Game thread only, like everything else in the engine.
        static CrosshairParams s_crosshair;

        /// @brief Eye-anchor parameters: where the true "eye point" sits relative to
        /// an actor's head bone.
        ///
        /// The head bone (`NPC Head [Head]`) origin sits at the base of the skull /
        /// top of the neck — roughly 5–8 cm below and behind the eyeballs. Aiming a
        /// gaze solve at the raw head-bone position therefore lands on the throat or
        /// nose bridge, never the eyes. These offsets lift and push the anchor onto
        /// the eyeline so "the eyes are the target" is literally true. Applied along
        /// the head bone's own world basis (forward = +Y, up = +Z of the bone), so
        /// they track seated, leaning and crouched poses correctly.
        struct EyeAnchorParams
        {
            float forwardCm{7.0f}; // toward the face, from the head bone origin
            float upCm{7.5f};      // up to eyeline, from the head bone origin
        };

        /// @brief Eye-anchor parameters for the current frame. Set by
        /// GazeEngine::RefreshTuning; read by ResolveTarget. Game thread only.
        static EyeAnchorParams s_eyeAnchor;
    };

} // namespace TrueGaze::Engine
