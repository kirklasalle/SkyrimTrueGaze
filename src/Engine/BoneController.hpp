#pragma once

#include "PCH.h"
#include <algorithm>

namespace TrueGaze::Engine
{

    /// @brief Controls anatomical strain distribution across NetImmerse NiNodes.
    ///
    /// Hierarchy: Spine2 (10%) -> Neck (25%) -> Head (65%) -> Eye nodes (residual).
    ///
    /// The 10/25/65 split describes how the HEAD CHAIN shares the burden of turning
    /// toward a target. It sums to 100% of the head's contribution. The eye nodes are
    /// NOT a fourth share of that same 100% — they carry the *residual*, i.e. whatever
    /// the head chain did not achieve:
    ///
    ///     head_chain   = spine + neck + head
    ///     eye_residual = target - head_chain      (clamped to the ocular range)
    ///
    /// This is what makes "eyes lead, head follows" work. When the head has fully
    /// adopted the target the residual is zero; when the target lies beyond the head's
    /// comfortable range the eyes strain toward their limit. Assigning the eyes a
    /// fraction of the total instead (as the original implementation did) left them at
    /// zero and produced exactly the whole-body robotic turning TrueGaze exists to
    /// eliminate.
    class BoneController
    {
    public:
        // Anatomical limits (degrees). Yaw is left/right, pitch is up/down.
        static constexpr float SPINE_YAW_LIMIT = 12.0f;
        static constexpr float NECK_YAW_LIMIT = 20.0f;
        static constexpr float NECK_PITCH_LIMIT = 15.0f;
        static constexpr float HEAD_YAW_LIMIT = 45.0f;
        static constexpr float HEAD_PITCH_LIMIT = 35.0f;

        /// Comfortable envelope of the whole head chain before the body should step.
        static constexpr float CHAIN_YAW_LIMIT = 70.0f;
        static constexpr float CHAIN_PITCH_LIMIT_UP = 50.0f;
        static constexpr float CHAIN_PITCH_LIMIT_DOWN = 45.0f;

        /// Ocular range. Beyond this the eyes cannot compensate and the head must move.
        static constexpr float EYE_YAW_LIMIT = 35.0f;
        static constexpr float EYE_PITCH_LIMIT = 25.0f;

        // Share of the head-chain deflection absorbed at each joint.
        static constexpr float SPINE_YAW_SHARE = 0.10f;
        static constexpr float NECK_YAW_SHARE = 0.25f;
        static constexpr float NECK_PITCH_SHARE = 0.25f;
        static constexpr float HEAD_YAW_SHARE = 0.65f;
        static constexpr float HEAD_PITCH_SHARE = 0.75f;

        struct StrainDistribution
        {
            float spineYaw{0.0f};
            float neckYaw{0.0f};
            float neckPitch{0.0f};
            float headYaw{0.0f};
            float headPitch{0.0f};
            float eyeYaw{0.0f};   // residual, head-relative
            float eyePitch{0.0f}; // residual, head-relative

            /// Total yaw adopted by the head chain (excludes the eyes).
            [[nodiscard]] constexpr float HeadChainYaw() const noexcept
            {
                return spineYaw + neckYaw + headYaw;
            }

            /// Total pitch adopted by the head chain (excludes the eyes).
            [[nodiscard]] constexpr float HeadChainPitch() const noexcept
            {
                return neckPitch + headPitch;
            }

            /// True when the eyes are at their limit and cannot reach the target.
            [[nodiscard]] bool IsEyeSaturated() const noexcept
            {
                return std::abs(eyeYaw) >= EYE_YAW_LIMIT - 0.001f || std::abs(eyePitch) >= EYE_PITCH_LIMIT - 0.001f;
            }
        };

        /// @brief Distributes a desired gaze deflection across the skeletal hierarchy.
        /// @param totalYawDeg   Desired gaze yaw relative to the actor's forward.
        /// @param totalPitchDeg Desired gaze pitch relative to the actor's forward.
        /// @param eyeYawLimit   Ocular yaw limit (configurable).
        /// @param eyePitchLimit Ocular pitch limit (configurable).
        static StrainDistribution CalculateHierarchyStrain(
            float totalYawDeg,
            float totalPitchDeg,
            float eyeYawLimit = EYE_YAW_LIMIT,
            float eyePitchLimit = EYE_PITCH_LIMIT) noexcept
        {
            StrainDistribution dist;

            // The head chain cannot comfortably exceed its envelope. The body is
            // expected to step and turn beyond this (see the >70 deg navigation rule).
            float clampedYaw = std::clamp(totalYawDeg, -CHAIN_YAW_LIMIT, CHAIN_YAW_LIMIT);
            float clampedPitch = std::clamp(totalPitchDeg,
                                            -CHAIN_PITCH_LIMIT_DOWN,
                                            CHAIN_PITCH_LIMIT_UP);

            // --- Head chain: spine 10% -> neck 25% -> head 65% (yaw) ---
            dist.spineYaw = std::clamp(clampedYaw * SPINE_YAW_SHARE,
                                       -SPINE_YAW_LIMIT, SPINE_YAW_LIMIT);

            dist.neckYaw = std::clamp(clampedYaw * NECK_YAW_SHARE,
                                      -NECK_YAW_LIMIT, NECK_YAW_LIMIT);
            dist.neckPitch = std::clamp(clampedPitch * NECK_PITCH_SHARE,
                                        -NECK_PITCH_LIMIT, NECK_PITCH_LIMIT);

            dist.headYaw = std::clamp(clampedYaw * HEAD_YAW_SHARE,
                                      -HEAD_YAW_LIMIT, HEAD_YAW_LIMIT);
            dist.headPitch = std::clamp(clampedPitch * HEAD_PITCH_SHARE,
                                        -HEAD_PITCH_LIMIT, HEAD_PITCH_LIMIT);

            // --- Eyes: the residual the head chain did not cover ---
            float residualYaw = clampedYaw - dist.HeadChainYaw();
            float residualPitch = clampedPitch - dist.HeadChainPitch();

            dist.eyeYaw = std::clamp(residualYaw, -eyeYawLimit, eyeYawLimit);
            dist.eyePitch = std::clamp(residualPitch, -eyePitchLimit, eyePitchLimit);

            return dist;
        }
    };

} // namespace TrueGaze::Engine
