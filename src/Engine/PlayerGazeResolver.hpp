#pragma once

#include "PCH.h"
#include <cstdint>

namespace TrueGaze::Engine {

/// @brief Resolves the player's crosshair into a "who is the player looking at"
/// answer, with a natural sweet-spot radius around the target's face.
///
/// ## Why this exists
///
/// TrueGaze is a social-gaze engine: an NPC should look back at the player when
/// the player is looking at the NPC. The only ground truth for "where the player
/// is looking" in first/third person is the crosshair. Before this module the
/// engine never consulted it: NPCs looked at the player merely because the player
/// was nearby, and the player's own gaze was invisible to the world.
///
/// ## What "looking at the face" means
///
/// The sweet spot is not a fixed world-space sphere. It is an ANGULAR test: the
/// angle between the crosshair ray and the ray from the camera to the target's
/// FACE must be smaller than the face's own angular size plus a base tolerance.
/// That reproduces natural vision:
///
///   * Close NPC (2 m): a human head subtends ~10 deg, so the crosshair may sit
///     anywhere on the head and still count as "looking at the face".
///   * Distant NPC (20 m): the head subtends ~1 deg, so the crosshair must be
///     almost exactly on the face.
///
/// A fixed radius would be wrong at one end or the other. The angular model is
/// scale-free and matches how the crosshair itself behaves on screen.
///
/// ## Where the data comes from
///
/// `RE::CrosshairPickData` is the game's own crosshair pick result (the same data
/// the HUD uses to show the activation prompt). It is read, never written. When
/// the pick is empty (crosshair on a wall, sky, or nothing) the resolver reports
/// "no target" and the engine falls back to its existing salience rules.
class PlayerGazeResolver
{
public:
    /// @brief Result of resolving the player's crosshair.
    struct PlayerGaze
    {
        /// FormID of the actor under the crosshair, 0 when none.
        uint32_t targetFormId{ 0 };

        /// True when the crosshair sits within the face sweet spot.
        bool onFace{ false };

        /// Angular error between the crosshair ray and the camera->face ray,
        /// in degrees. 0 = dead centre on the face.
        float faceAngleDeg{ 0.0f };

        /// Angular radius the target's head subtends, in degrees. The sweet
        /// spot is faceAngleDeg <= headAngularRadiusDeg + baseToleranceDeg.
        float headAngularRadiusDeg{ 0.0f };

        /// World position of the target's face (head bone, or head-height
        /// fallback). This is where the NPC should look back FROM the player's
        /// point of view - i.e. the player's own face position for mutual gaze.
        float faceX{ 0.0f };
        float faceY{ 0.0f };
        float faceZ{ 0.0f };

        /// Distance from the camera to the face, in metres.
        float distanceMeters{ 0.0f };
    };

    /// @brief Tunable sweet-spot parameters, snapshotted from configuration.
    struct Params
    {
        /// Base angular tolerance added to the head's angular radius, degrees.
        /// This is the "generosity" of the sweet spot at any distance.
        float baseToleranceDeg{ 4.0f };

        /// Beyond this range the crosshair no longer confers social attention.
        float maxRangeMeters{ 25.0f };

        /// Below this range any crosshair contact counts as face contact
        /// (point-blank: the face fills the screen).
        float pointBlankMeters{ 1.5f };
    };

    /// @brief Resolves what the player is looking at via the crosshair.
    /// Game thread only (reads live pick + camera state).
    static PlayerGaze Resolve(const Params &params) noexcept;

    /// @brief True when the crosshair currently rests on the given actor's face.
    /// Convenience wrapper over Resolve() for the per-actor tick path.
    [[nodiscard]] static bool IsPlayerLookingAtFace(uint32_t actorFormId,
                                                    const Params &params) noexcept;
};

} // namespace TrueGaze::Engine
