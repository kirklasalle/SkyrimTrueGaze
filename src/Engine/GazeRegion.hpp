#pragma once

// ---------------------------------------------------------------------------
// GazeRegion — single source of truth for the 13 HCEP-02 gaze regions.
//
// Shared by GazeEngine (classification of the solved eye angles), the
// renderer (panel colours), the Phase 2 panel hit detector, and the tests, so
// the engine's notion of "where the eyes are looking" and the panel's notion
// can never drift apart.
//
// Spec: SPEC_Region_Map.md (approved 2026-10-03).
// ---------------------------------------------------------------------------

#include <cmath>
#include <cstdint>

namespace TrueGaze::Engine
{

    enum class GazeRegion : uint8_t
    {
        LeftEye = 0,
        RightEye = 1,
        Mouth = 2,
        Forehead = 3,
        Chin = 4,
        Torso = 5,
        RightHand = 6, // reserved: not produced by the angular classifier
        LeftHand = 7,  // reserved: not produced by the angular classifier
        Ground = 8,
        UpperLeftPeripheral = 9,   // CGA: positivity / hope
        UpperRightPeripheral = 10, // CGA: memory / constructive thought
        LowerLeftPeripheral = 11,  // CGA: tiredness / negativity / sadness
        LowerRightPeripheral = 12, // CGA: shyness / fear / deception
        Count = 13
    };

    [[nodiscard]] constexpr const char* GazeRegionName(uint8_t region) noexcept
    {
        switch (region)
        {
        case 0: return "LeftEye";
        case 1: return "RightEye";
        case 2: return "Mouth";
        case 3: return "Forehead";
        case 4: return "Chin";
        case 5: return "Torso";
        case 6: return "RightHand";
        case 7: return "LeftHand";
        case 8: return "Ground";
        case 9: return "ULPeripheral";
        case 10: return "URPeripheral";
        case 11: return "LLPeripheral";
        case 12: return "LRPeripheral";
        default: return "Unknown";
        }
    }

    /// Map a gaze deflection (degrees; +yaw = right, +pitch = up) onto a region.
    ///
    /// Behaviour is byte-for-byte identical to the classifier that previously
    /// lived in GazeEngine.cpp (moved here 2026-10-03). Known issue, pending
    /// Kirk's decision (see PLAN_Verification_and_Testing.md): the Torso band
    /// (-6 <= pitch < -3, |yaw| < 5) is tested before Mouth, so a central gaze
    /// just below the eyes classifies as Torso and Mouth is only reachable
    /// off-centre (|yaw| >= 5). Parity is preserved here deliberately so moving
    /// the code changes nothing in-game.
    [[nodiscard]] inline uint8_t ClassifyGazeRegion(float yawDeg, float pitchDeg) noexcept
    {
        // CGA aversion quadrants take priority: they are the cognitively
        // meaningful peripheral regions from the HCEP-02 enhanced diagram.
        if (pitchDeg > 12.0f)
        {
            if (yawDeg < -5.0f)
                return 9; // Upper-left peripheral (positivity, hope)
            if (yawDeg > 5.0f)
                return 10; // Upper-right peripheral (memory, constructive thought)
            return 3;      // Forehead / Third-Eye zone
        }

        if (pitchDeg < -12.0f)
        {
            if (yawDeg < -5.0f)
                return 11; // Lower-left peripheral (tiredness, negativity, sadness)
            if (yawDeg > 5.0f)
                return 12; // Lower-right peripheral (shyness, fear, deception)
            return 8;      // Floor / ground (shame, submission)
        }

        // Below face but within ~12 deg pitch: chin or torso zone
        if (pitchDeg < -6.0f)
        {
            return 4; // Chin
        }
        if (pitchDeg < -3.0f && std::abs(yawDeg) < 5.0f)
        {
            return 5; // Torso / chest (empathic resonance)
        }

        // Within the face -> social triangle vertices.
        if (std::abs(pitchDeg) <= 3.0f)
        {
            if (yawDeg < -1.0f)
                return 0; // Left eye
            if (yawDeg > 1.0f)
                return 1; // Right eye
            return 0;
        }

        if (pitchDeg < 0.0f)
            return 2; // Mouth / lips
        return 3;     // Forehead / Third-Eye
    }

} // namespace TrueGaze::Engine
