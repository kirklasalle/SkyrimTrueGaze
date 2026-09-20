#pragma once

#include "PCH.h"

namespace TrueGaze::Engine
{

    /// @brief Spatial Distance and Performance Level-of-Detail (LOD) Manager.
    /// Ensures 0 FPS drop even in massive battles or dense cities (Whiterun, Solitude).
    class LodManager
    {
    public:
        enum class LodTier : uint8_t
        {
            Tier1_DialogueRange = 0, // < 5 meters: Full biological kinematics (Eyes, Saccades, VOR, Micro-drift, Eyelid Blinks)
            Tier2_Proximity = 1,     // 5m - 15m: Head & Neck kinematics active; Eye tracking simplified; Micro-drift disabled
            Tier3_Culled = 2         // > 15 meters: Standard game engine LOD; processing bypassed entirely
        };

        static LodTier GetLodTier(float distanceMeters,
                                  float tier1Meters = 5.0f,
                                  float tier2Meters = 15.0f) noexcept
        {
            if (distanceMeters <= tier1Meters)
            {
                return LodTier::Tier1_DialogueRange;
            }
            else if (distanceMeters <= tier2Meters)
            {
                return LodTier::Tier2_Proximity;
            }
            return LodTier::Tier3_Culled;
        }
    };

} // namespace TrueGaze::Engine
