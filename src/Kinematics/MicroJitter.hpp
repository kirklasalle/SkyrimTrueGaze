#pragma once

#include <cmath>
#include <random>

namespace TrueGaze::Kinematics {

/// @brief Organic Fixation Jitter (Micro-Saccades & Physiological Nystagmus).
/// Generates continuous bounded Brownian motion across the ocular plane to eliminate "frozen stare".
class MicroJitter
{
public:
    struct JitterState
    {
        float currentYawOffset{ 0.0f };
        float currentPitchOffset{ 0.0f };
        float targetYawOffset{ 0.0f };
        float targetPitchOffset{ 0.0f };
        float timerSec{ 0.0f };
        float nextIntervalSec{ 0.25f }; // 2 to 4 Hz micro-adjustments
        float maxAmplitudeDeg{ 0.35f }; // Subtle sub-degree drift
    };

    /// @brief Steps the micro-jitter simulation using a random-walk generator.
    static void Update(JitterState& state, float deltaSeconds) noexcept
    {
        if (deltaSeconds <= 0.0f) return;

        state.timerSec += deltaSeconds;

        if (state.timerSec >= state.nextIntervalSec) {
            state.timerSec = 0.0f;

            // Generate new small target drift
            static thread_local std::mt19937 rng{ 1337 };
            std::uniform_real_distribution<float> dist(-state.maxAmplitudeDeg, state.maxAmplitudeDeg);
            std::uniform_real_distribution<float> intervalDist(0.2f, 0.45f);

            state.targetYawOffset = dist(rng);
            state.targetPitchOffset = dist(rng);
            state.nextIntervalSec = intervalDist(rng);
        }

        // Smoothly interpolate toward micro-target
        float speed = 8.0f;
        float alpha = 1.0f - std::exp(-speed * deltaSeconds);
        state.currentYawOffset += (state.targetYawOffset - state.currentYawOffset) * alpha;
        state.currentPitchOffset += (state.targetPitchOffset - state.currentPitchOffset) * alpha;
    }
};

} // namespace TrueGaze::Kinematics
