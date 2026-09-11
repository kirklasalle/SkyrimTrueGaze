#pragma once

#include "PCH.h"
#include <algorithm>

namespace TrueGaze::Engine {

/// @brief Controls anatomical strain distribution across NetImmerse NiNodes.
/// Hierarchy: NPC Spine2 (10%) -> NPC Neck (25%) -> NPC Head (65%) -> Eye Nodes (100% Saccade + VOR).
class BoneController
{
public:
    struct StrainDistribution
    {
        float spineYaw{ 0.0f };
        float neckYaw{ 0.0f };
        float neckPitch{ 0.0f };
        float headYaw{ 0.0f };
        float headPitch{ 0.0f };
        float eyeYaw{ 0.0f };
        float eyePitch{ 0.0f };
    };

    /// @brief Distributes a desired gaze delta across the skeletal hierarchy with physiological limits.
    static StrainDistribution CalculateHierarchyStrain(float totalYawDeg, float totalPitchDeg) noexcept
    {
        StrainDistribution dist;

        // Anatomical comfort clamping: Head cannot comfortably rotate past +/- 70 degrees
        float clampedYaw = std::clamp(totalYawDeg, -70.0f, 70.0f);
        float clampedPitch = std::clamp(totalPitchDeg, -45.0f, 50.0f);

        // 1. Spine2 absorbs 10% of total yaw (max +/- 12 deg)
        dist.spineYaw = std::clamp(clampedYaw * 0.10f, -12.0f, 12.0f);

        // 2. Neck absorbs 25% of total yaw and pitch (max +/- 20 deg)
        dist.neckYaw = std::clamp(clampedYaw * 0.25f, -20.0f, 20.0f);
        dist.neckPitch = std::clamp(clampedPitch * 0.25f, -15.0f, 15.0f);

        // 3. Head absorbs the remaining 65% of yaw and pitch (max +/- 45 deg)
        dist.headYaw = std::clamp(clampedYaw * 0.65f, -45.0f, 45.0f);
        dist.headPitch = std::clamp(clampedPitch * 0.75f, -35.0f, 35.0f);

        return dist;
    }
};

} // namespace TrueGaze::Engine
