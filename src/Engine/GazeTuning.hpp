#pragma once

#include <cstdint>

namespace TrueGaze::Engine
{

    /// @brief Immutable per-frame snapshot of the tunable parameters.
    ///
    /// The kinematics modules take their constants as arguments rather than reading
    /// globals. This struct is the single place where configuration is converted into
    /// those arguments, so that a value in TrueGaze.ini has exactly one path to the
    /// mathematics.
    ///
    /// Before this existed, ConfigManager parsed the INI and nothing consumed it:
    /// every module used its own compile-time default. See
    /// docs/AUDIT_REPORT_2026-09-11.md finding C-8.
    struct GazeTuning
    {
        // --- Kinematics ---
        float saccadeSpeedMult{1.0f};        // scales V_max
        float velocitySaturation{14.0f};     // c in the Main Sequence equation
        float microJitterAmp{0.35f};         // degrees
        float microJitterIntervalMin{0.2f};  // seconds; drives OU mean reversion
        float microJitterIntervalMax{0.45f}; // seconds
        float headTrackingSpeed{6.0f};       // head damping factor
        float maxComfortEyeAngle{35.0f};     // ocular yaw limit

        // --- Skeletal hierarchy strain shares ---
        float spine2YawWeight{0.10f};
        float neckYawWeight{0.25f};
        float neckPitchWeight{0.25f};
        float headYawWeight{0.65f};
        float headPitchWeight{0.75f};

        // --- Social ---
        bool enableGazeAversion{true};
        bool enableSocialTriangle{true};
        float triangleFixationDuration{0.35f};
        float mutualGazeThreshold{2.0f};

        // --- Crosshair sweet spot (player gaze) ---
        bool enableCrosshairGaze{true};
        float crosshairToleranceDeg{4.0f};
        float crosshairMaxRangeMeters{25.0f};
        float crosshairPointBlankMeters{1.5f};

        // --- LOD ---
        float tier1DistanceMeters{5.0f};
        float tier2DistanceMeters{15.0f};

        // --- Master switches ---
        bool enableTrueGaze{true};
        bool enableCreatures{true};
        bool debugGazeRays{false};

        /// Peak saccadic velocity after the user's speed multiplier is applied.
        [[nodiscard]] constexpr float EffectiveVMax(float baseVMax) const noexcept
        {
            return baseVMax * saccadeSpeedMult;
        }
    };

} // namespace TrueGaze::Engine