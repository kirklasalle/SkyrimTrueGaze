#pragma once

#include <cmath>
#include <random>
#include <cstdint>

namespace TrueGaze::Kinematics
{

    /// @brief Organic Fixation Jitter — micro-saccades and physiological nystagmus.
    ///
    /// Human gaze is never perfectly still during fixation. The eyes drift, tremble,
    /// and make small corrective micro-saccades several times a second. Without this,
    /// a rendered character reads as synthetic or dead.
    ///
    /// ## Why an Ornstein-Uhlenbeck process
    ///
    /// The original implementation resampled an independent uniform target every
    /// ~250 ms and exponentially approached it. That is *white noise through a
    /// low-pass filter*: mean-reverting, but not a random walk. It produces an
    /// obviously periodic "breathing" drift that repeats at the resample interval.
    ///
    /// Real ocular drift is a bounded Brownian motion. The correct model is the
    /// Ornstein-Uhlenbeck process — the continuous-time analogue of an AR(1):
    ///
    ///     dX = -theta * X * dt + sigma * dW
    ///
    /// which performs Brownian motion (a genuine random walk; `dW` accumulates) while
    /// being pulled back toward fixation at rate `theta`. It has a well-defined
    /// stationary distribution:
    ///
    ///     X ~ N(0, sigma^2 / (2 * theta))
    ///
    /// so the drift amplitude is a *statistical* quantity rather than a hard clamp,
    /// which is how real drift behaves. Occasional excursions beyond the nominal
    /// sigma are expected and correct.
    ///
    /// ## Seeding
    ///
    /// The RNG is seeded per actor from the FormID via Init(), so two NPCs never share
    /// a drift sequence and a given NPC drifts identically across sessions. The
    /// original code used one fixed seed (1337) for every actor, so every character in
    /// the world jittered in lockstep — a visible and specific tell. See
    /// docs/AUDIT_REPORT_2026-09-11.md section 5.3.
    class MicroJitter
    {
    public:
        struct JitterState
        {
            float currentYawOffset{0.0f};
            float currentPitchOffset{0.0f};

            /// Standard deviation of the stationary distribution, in degrees.
            float amplitudeDeg{0.35f};

            /// Mean reversion rate (1/s). Governs how quickly drift is corrected.
            float reversionRate{6.0f};

            /// Leftover time for fixed-step integration.
            float accumulatorSec{0.0f};

            /// Per-actor RNG state, seeded by Init().
            std::mt19937 rng{0u};
            bool seeded{false};
        };

        /// @brief Volatility giving `amplitudeDeg` as the stationary standard deviation.
        /// Inverts sigma = amplitude * sqrt(2 * theta).
        [[nodiscard]] static float VolatilityFor(float amplitudeDeg, float reversionRate) noexcept
        {
            return amplitudeDeg * std::sqrt(2.0f * reversionRate);
        }

        /// @brief Seeds the drift generator for a specific actor.
        /// @param seed Typically the actor's FormID, so drift is stable and unique.
        static void Init(JitterState &state, uint32_t seed, float amplitudeDeg) noexcept
        {
            state.rng.seed(seed != 0u ? seed : 0x9E3779B9u);
            state.seeded = true;
            state.currentYawOffset = 0.0f;
            state.currentPitchOffset = 0.0f;
            state.amplitudeDeg = amplitudeDeg;
            state.accumulatorSec = 0.0f;
        }

        /// @brief Advances the drift by one frame.
        ///
        /// Integrated at a fixed 120 Hz sub-step so the drift statistics do not depend
        /// on frame rate. A frame-rate-dependent random walk is a subtle but real
        /// defect: at 240 FPS the walk would accumulate twice as many steps as at
        /// 120 FPS and (absent this) would drift measurably further.
        static void Update(JitterState &state, float deltaSeconds) noexcept
        {
            if (deltaSeconds <= 0.0f)
                return;

            if (!state.seeded)
            {
                // Defensive: an unseeded state must still behave deterministically.
                Init(state, 0x9E3779B9u, state.amplitudeDeg);
            }

            constexpr float kFixedStep = 1.0f / 120.0f;
            constexpr float kMaxCatchUp = 0.25f; // never integrate more than 250 ms at once

            state.accumulatorSec += deltaSeconds;

            if (state.accumulatorSec > kMaxCatchUp)
            {
                // The game stalled (load screen, alt-tab). Relax toward the mean rather
                // than integrating a huge dt, which would fling the gaze across the face.
                state.accumulatorSec = 0.0f;
                state.currentYawOffset *= 0.5f;
                state.currentPitchOffset *= 0.5f;
                return;
            }

            const float theta = state.reversionRate;
            const float sigma = VolatilityFor(state.amplitudeDeg, theta);

            // Exact discretisation of the OU process over one fixed step.
            const float decay = std::exp(-theta * kFixedStep);
            const float noiseScale = sigma * std::sqrt((1.0f - decay * decay) / (2.0f * theta));

            std::normal_distribution<float> dist(0.0f, 1.0f);

            while (state.accumulatorSec >= kFixedStep)
            {
                state.accumulatorSec -= kFixedStep;

                state.currentYawOffset =
                    state.currentYawOffset * decay + noiseScale * dist(state.rng);
                state.currentPitchOffset =
                    state.currentPitchOffset * decay + noiseScale * dist(state.rng);
            }
        }

        /// @brief Current drift magnitude in degrees. Diagnostic.
        [[nodiscard]] static float Magnitude(const JitterState &state) noexcept
        {
            return std::sqrt(state.currentYawOffset * state.currentYawOffset + state.currentPitchOffset * state.currentPitchOffset);
        }
    };

} // namespace TrueGaze::Kinematics
