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
        float eyePursuitSpeed{12.0f};        // smooth ocular pursuit rate (1/s); glide not snap
        float maxComfortEyeAngle{35.0f};     // ocular yaw limit
        float headOnsetDelaySec{0.12f};      // biological latency gap (seconds head lags eyes)

        // --- Skeletal hierarchy strain shares ---
        float spine2YawWeight{0.035f};
        float neckYawWeight{0.070f};
        float neckPitchWeight{0.070f};
        float headYawWeight{0.245f};
        float headPitchWeight{0.245f};

        // --- Skeletal hierarchy: head engagement threshold ---
        // For gaze shifts below this angle (degrees), ONLY the eyes move.
        // The head chain contribution is zeroed, preventing robotic micro-head-turns
        // during social triangle cycling at close range. Human heads don't visibly
        // turn for tiny 2-5 degree shifts. Set 0 to disable.
        float headEngageThresholdDeg{8.0f};

        // --- Social ---
        bool enableGazeAversion{true}; // CGA ON by default (HCEP-02 enhanced diagram)
        bool enableSocialTriangle{true};
        float triangleFixationDuration{0.35f};
        float mutualGazeThreshold{2.0f};
        // 0 = classic fixed orbit (LeftEye->RightEye->Mouth), 1 = free organic wandering.
        // At 0.6 the path still favours eye-to-eye transitions but lateral jumps and
        // same-point re-fixations destroy every predictable loop.
        float trianglePathRandomness{0.6f};

        // --- CGA Timing (dialogue-synced return) ---
        // Fraction of normal head chain engagement during CGA aversion (0=eyes-only).
        float cgaHeadInvolvement{0.08f};
        // When true, CGA aversion ends when dialogue begins (attention capture).
        bool dialogueSyncCgaReturn{true};
        // Per-actor random offset +/- seconds around dialogue onset for organic variation.
        float cgaDialogueOffsetSec{2.0f};

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