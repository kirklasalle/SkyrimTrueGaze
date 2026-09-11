#pragma once

#include <cmath>
#include <numbers>
#include <algorithm>

namespace TrueGaze::Kinematics {

/// @brief Biological Saccade Generator implementing the empirical Main Sequence equations.
/// Grounded in Kirk LaSalle's Human Communication Eye Protocol (HCEP).
class SaccadeGenerator
{
public:
    // Empirical biological constants (Bahill, Clark & Stark, 1975; Baloh et al., 1975)
    static constexpr float DEFAULT_VMAX = 750.0f;       // Peak asymptotic velocity (deg/s)
    static constexpr float DEFAULT_C = 14.0f;           // Velocity saturation constant (deg)
    static constexpr float BASE_DURATION_SEC = 0.022f;  // D0 base duration (~22ms)
    static constexpr float DURATION_SLOPE = 0.0025f;    // d duration factor (~2.5ms per deg)

    struct SaccadeState
    {
        bool isBallistic{ false };
        float amplitudeDeg{ 0.0f };
        float totalDurationSec{ 0.0f };
        float elapsedSec{ 0.0f };
        float startYaw{ 0.0f };
        float startPitch{ 0.0f };
        float targetYaw{ 0.0f };
        float targetPitch{ 0.0f };
        float currentYaw{ 0.0f };
        float currentPitch{ 0.0f };
    };

    /// @brief Calculates the peak saccadic velocity for a given angular amplitude.
    static float CalculatePeakVelocity(float amplitudeDeg, float vMax = DEFAULT_VMAX, float c = DEFAULT_C) noexcept
    {
        if (amplitudeDeg <= 0.0f) return 0.0f;
        return vMax * (1.0f - std::exp(-amplitudeDeg / c));
    }

    /// @brief Calculates total ballistic duration based on Main Sequence linear relationship.
    static float CalculateDuration(float amplitudeDeg) noexcept
    {
        return BASE_DURATION_SEC + (DURATION_SLOPE * std::abs(amplitudeDeg));
    }

    /// @brief Triggers a new ballistic saccade toward a target gaze angle.
    static void TriggerSaccade(SaccadeState& state, float targetYaw, float targetPitch) noexcept
    {
        float dy = targetYaw - state.currentYaw;
        float dp = targetPitch - state.currentPitch;
        float amplitude = std::sqrt(dy * dy + dp * dp);

        if (amplitude < 0.5f) { // Threshold for micro-fixation
            state.currentYaw = targetYaw;
            state.currentPitch = targetPitch;
            state.isBallistic = false;
            return;
        }

        state.isBallistic = true;
        state.amplitudeDeg = amplitude;
        state.totalDurationSec = CalculateDuration(amplitude);
        state.elapsedSec = 0.0f;
        state.startYaw = state.currentYaw;
        state.startPitch = state.currentPitch;
        state.targetYaw = targetYaw;
        state.targetPitch = targetPitch;
    }

    /// @brief Steps the saccade animation using a smoothed bell-curve velocity profile.
    static void Update(SaccadeState& state, float deltaSeconds) noexcept
    {
        if (!state.isBallistic) return;

        state.elapsedSec += deltaSeconds;
        if (state.elapsedSec >= state.totalDurationSec) {
            state.currentYaw = state.targetYaw;
            state.currentPitch = state.targetPitch;
            state.isBallistic = false;
            return;
        }

        // Normalized progress [0, 1]
        float t = std::clamp(state.elapsedSec / state.totalDurationSec, 0.0f, 1.0f);

        // Smoothstep / minimum-jerk interpolation for biological eye trajectory
        float smoothT = t * t * (3.0f - 2.0f * t);

        state.currentYaw = state.startYaw + (state.targetYaw - state.startYaw) * smoothT;
        state.currentPitch = state.startPitch + (state.targetPitch - state.startPitch) * smoothT;
    }
};

} // namespace TrueGaze::Kinematics
