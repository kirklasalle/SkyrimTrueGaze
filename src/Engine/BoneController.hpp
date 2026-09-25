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

        // Share of the head-chain deflection absorbed at each joint. These are the
        // compiled fallbacks; the live values come from GazeTuning (TrueGaze.ini
        // [SkeletalHierarchy]) and are passed into CalculateHierarchyStrain.
        static constexpr float SPINE_YAW_SHARE = 0.035f;
        static constexpr float NECK_YAW_SHARE = 0.070f;
        static constexpr float NECK_PITCH_SHARE = 0.070f;
        static constexpr float HEAD_YAW_SHARE = 0.245f;
        static constexpr float HEAD_PITCH_SHARE = 0.245f;

        struct StrainWeights
        {
            float spineYaw{SPINE_YAW_SHARE};
            float neckYaw{NECK_YAW_SHARE};
            float neckPitch{NECK_PITCH_SHARE};
            float headYaw{HEAD_YAW_SHARE};
            float headPitch{HEAD_PITCH_SHARE};
        };

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
        /// @param totalYawDeg       Desired gaze yaw relative to the actor's forward.
        /// @param totalPitchDeg     Desired gaze pitch relative to the actor's forward.
        /// @param eyeYawLimit       Ocular yaw limit (configurable).
        /// @param eyePitchLimit     Ocular pitch limit (configurable).
        /// @param weights           Strain shares from configuration (defaults = compiled).
        /// @param headEngageThresh  Head engagement threshold (degrees). Below this angle
        ///                          the head chain is zeroed and only eyes move.
        static StrainDistribution CalculateHierarchyStrain(
            float totalYawDeg,
            float totalPitchDeg,
            float eyeYawLimit = EYE_YAW_LIMIT,
            float eyePitchLimit = EYE_PITCH_LIMIT,
            const StrainWeights &weights = StrainWeights{},
            float headEngageThresh = 0.0f) noexcept
        {
            StrainDistribution dist;

            // The head chain cannot comfortably exceed its envelope. The body is
            // expected to step and turn beyond this (see the >70 deg navigation rule).
            float clampedYaw = std::clamp(totalYawDeg, -CHAIN_YAW_LIMIT, CHAIN_YAW_LIMIT);
            float clampedPitch = std::clamp(totalPitchDeg,
                                            -CHAIN_PITCH_LIMIT_DOWN,
                                            CHAIN_PITCH_LIMIT_UP);

            // HEAD ENGAGEMENT THRESHOLD: for small gaze shifts, the head stays still
            // and only the eyes move. This eliminates robotic micro-head-turns during
            // social triangle cycling at close range. Human heads don't visibly move
            // for tiny 2-5 degree gaze shifts — only the eyes do.
            const float totalMag = std::sqrt(clampedYaw * clampedYaw + clampedPitch * clampedPitch);
            const bool headSuppressed = (headEngageThresh > 0.0f && totalMag < headEngageThresh);

            if (headSuppressed)
            {
                // Eyes carry the entire deflection; head chain stays at zero.
                dist.eyeYaw = std::clamp(clampedYaw, -eyeYawLimit, eyeYawLimit);
                dist.eyePitch = std::clamp(clampedPitch, -eyePitchLimit, eyePitchLimit);
                return dist;
            }

            // --- Head chain: spine -> neck -> head (yaw), neck -> head (pitch) ---
            dist.spineYaw = std::clamp(clampedYaw * weights.spineYaw,
                                       -SPINE_YAW_LIMIT, SPINE_YAW_LIMIT);

            dist.neckYaw = std::clamp(clampedYaw * weights.neckYaw,
                                      -NECK_YAW_LIMIT, NECK_YAW_LIMIT);
            dist.neckPitch = std::clamp(clampedPitch * weights.neckPitch,
                                        -NECK_PITCH_LIMIT, NECK_PITCH_LIMIT);

            dist.headYaw = std::clamp(clampedYaw * weights.headYaw,
                                      -HEAD_YAW_LIMIT, HEAD_YAW_LIMIT);
            dist.headPitch = std::clamp(clampedPitch * weights.headPitch,
                                        -HEAD_PITCH_LIMIT, HEAD_PITCH_LIMIT);

            // --- Eyes: the residual the head chain did not cover ---
            float residualYaw = clampedYaw - dist.HeadChainYaw();
            float residualPitch = clampedPitch - dist.HeadChainPitch();

            dist.eyeYaw = std::clamp(residualYaw, -eyeYawLimit, eyeYawLimit);
            dist.eyePitch = std::clamp(residualPitch, -eyePitchLimit, eyePitchLimit);

            return dist;
        }

        /// @brief CGA (Cognitive Gaze Aversion) eye-dominant strain distribution.
        ///
        /// During THINK mode gaze aversion, the deflection should be carried almost
        /// entirely by the eyes with minimal head involvement. This prevents the
        /// grotesque neck-twist seen when CGA's ±18° peripheral offsets route through
        /// the normal head chain at full weight.
        ///
        /// @param cgaHeadFraction  0.0 = pure eye aversion, 1.0 = normal head chain.
        ///                         Recommended: 0.05–0.15 for subtle head drift.
        static StrainDistribution CalculateCgaStrain(
            float totalYawDeg,
            float totalPitchDeg,
            float eyeYawLimit = EYE_YAW_LIMIT,
            float eyePitchLimit = EYE_PITCH_LIMIT,
            float cgaHeadFraction = 0.08f) noexcept
        {
            StrainDistribution dist;

            float clampedYaw = std::clamp(totalYawDeg, -CHAIN_YAW_LIMIT, CHAIN_YAW_LIMIT);
            float clampedPitch = std::clamp(totalPitchDeg,
                                            -CHAIN_PITCH_LIMIT_DOWN,
                                            CHAIN_PITCH_LIMIT_UP);

            // Eyes take the lion's share of the aversion deflection.
            // The head chain gets only a tiny fraction for organic subtlety.
            const float headYaw = clampedYaw * cgaHeadFraction;
            const float headPitch = clampedPitch * cgaHeadFraction;

            // Distribute the small head fraction across the chain naturally.
            dist.spineYaw = std::clamp(headYaw * 0.15f, -SPINE_YAW_LIMIT, SPINE_YAW_LIMIT);
            dist.neckYaw = std::clamp(headYaw * 0.35f, -NECK_YAW_LIMIT, NECK_YAW_LIMIT);
            dist.neckPitch = std::clamp(headPitch * 0.35f, -NECK_PITCH_LIMIT, NECK_PITCH_LIMIT);
            dist.headYaw = std::clamp(headYaw * 0.50f, -HEAD_YAW_LIMIT, HEAD_YAW_LIMIT);
            dist.headPitch = std::clamp(headPitch * 0.65f, -HEAD_PITCH_LIMIT, HEAD_PITCH_LIMIT);

            // Eyes get the full deflection minus whatever tiny head contribution there was.
            float residualYaw = clampedYaw - dist.HeadChainYaw();
            float residualPitch = clampedPitch - dist.HeadChainPitch();

            dist.eyeYaw = std::clamp(residualYaw, -eyeYawLimit, eyeYawLimit);
            dist.eyePitch = std::clamp(residualPitch, -eyePitchLimit, eyePitchLimit);

            return dist;
        }
    };

} // namespace TrueGaze::Engine
