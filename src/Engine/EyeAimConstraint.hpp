#pragma once

#include "PCH.h"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

namespace TrueGaze::Engine
{

    /// @brief Applies and withdraws gaze rotations on an actor's skeleton bones.
    ///
    /// ## How the rotation is applied
    ///
    /// The post-animation hook runs after the skeleton has been posed for the frame.
    /// Each bone's current local rotation already encodes the pose that the animation
    /// produced. A gaze deflection is therefore a *relative* rotation applied in bone
    /// local space, composed onto the existing local rotation:
    ///
    ///     newLocal.rotate = gazeRotation * oldLocal.rotate
    ///
    /// Rotating first composition-wise (gaze * existing) means the deflection is
    /// expressed in the bone's parent frame, which is the correct frame for a yaw
    /// deflection applied to a neck, head, or eye joint.
    ///
    /// ## Why the original rotation is cached
    ///
    /// The hook may run more than once per frame (multiple animation updates, or a
    /// second pass after a cell load). If the rotation were composed onto the already
    /// modified transform, the deflection would compound and the head would spin.
    /// `Apply` caches the pristine local rotation on first touch per frame and always
    /// composes from the cache. `Withdraw` restores it. This makes the operation
    /// idempotent within a frame.
    class EyeAimConstraint
    {
    public:
        /// Cache the bone's current local rotation as the frame baseline.
        /// Safe to call repeatedly; only the first call per frame takes effect.
        static void BeginFrame() noexcept;

        /// @brief Rotates a bone by the given degrees, relative to its animated pose.
        /// @return false if the bone is null or the rotation could not be applied.
        static bool Apply(uint32_t actorFormId, RE::NiAVObject *bone,
                          float yawDeg, float pitchDeg) noexcept;

        /// Restore only one actor's procedural pose before Skyrim animates that
        /// actor again. Other actors must remain posed until their own updates.
        static void WithdrawActor(uint32_t actorFormId) noexcept;

        /// @brief Restores every touched bone to its animated pose.
        static void Withdraw() noexcept;

        /// Number of bones currently holding an applied deflection. Diagnostic.
        [[nodiscard]] static uint32_t ActiveBoneCount() noexcept;
    };

} // namespace TrueGaze::Engine