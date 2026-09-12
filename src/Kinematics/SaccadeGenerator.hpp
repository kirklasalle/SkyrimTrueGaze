#pragma once

#include <cmath>
#include <numbers>
#include <algorithm>

namespace TrueGaze::Kinematics
{

    /// @brief Biological Saccade Generator implementing the empirical Main Sequence equations.
    /// Grounded in Kirk LaSalle's Human Communication Eye Protocol (HCEP).
    ///
    /// The Main Sequence describes how saccadic peak velocity and duration scale with
    /// amplitude (Bahill, Clark & Stark, 1975; Baloh et al., 1975):
    ///
    ///     V_peak(A) = V_max * (1 - e^(-A/c))
    ///     D(A)      = D0 + d * A
    ///
    /// V_peak is not merely decorative: Update() integrates a velocity profile whose
    /// PEAK equals V_peak(A) and whose INTEGRAL over [0, D] equals A. That is what
    /// makes a large saccade visibly ballistic rather than a constant-speed sweep.
    class SaccadeGenerator
    {
    public:
        // Empirical biological constants
        static constexpr float DEFAULT_VMAX = 750.0f;      // Peak asymptotic velocity (deg/s)
        static constexpr float DEFAULT_C = 14.0f;          // Velocity saturation constant (deg)
        static constexpr float BASE_DURATION_SEC = 0.022f; // D0 base duration (~22ms)
        static constexpr float DURATION_SLOPE = 0.0025f;   // d duration factor (~2.5ms per deg)

        /// Below this amplitude the eyes simply settle rather than launching a saccade.
        static constexpr float FIXATION_THRESHOLD_DEG = 0.5f;

        // --- Velocity profile (normalised skewed Gaussian) -----------------------
        //
        // Saccades accelerate faster than they decelerate, so the velocity profile is
        // skewed rather than a symmetric bell. Modelled as a Gaussian in *normalised
        // time* with mean mu and standard deviation sigma.
        //
        //     q(t) = exp( -0.5 * ((t - mu) / sigma)^2 )
        //
        // q_max = 1 (attained exactly at t = mu). The profile's area over [0,1] is
        //
        //     Zq = integral[0,1] q(t) dt = 0.536782   (sigma = 0.22, mu = 0.45)
        //
        // PROFILE_WINDOW = 1/Zq normalises the profile to unit area, so that the
        // saccade travels exactly the full amplitude over the full duration.
        //
        // Scaling by amplitude and duration, the implied peak velocity is
        //     (amplitude / duration) * PROFILE_WINDOW
        // which asymptotes to 745 deg/s at small amplitudes — the empirical
        // asymptote is ~750 deg/s (Bahill, Clark & Stark, 1975), a 0.7% agreement.
        //
        // These two constants being exactly consistent is what keeps the visual
        // shape and the stated V_peak from drifting apart. Verified in
        // tests/KinematicsTests.cpp::TestMainSequenceFidelity.
        static constexpr float PROFILE_MU = 0.45f;
        static constexpr float PROFILE_SIGMA = 0.22f;
        static constexpr float PROFILE_AREA = 0.536782f;
        static constexpr float PROFILE_WINDOW = 1.0f / PROFILE_AREA;

        struct SaccadeState
        {
            bool isBallistic{false};
            float amplitudeDeg{0.0f};
            float totalDurationSec{0.0f};
            float elapsedSec{0.0f};
            float startYaw{0.0f};
            float startPitch{0.0f};
            float targetYaw{0.0f};
            float targetPitch{0.0f};
            float currentYaw{0.0f};
            float currentPitch{0.0f};

            /// Peak velocity used for this saccade (deg/s). Diagnostic.
            float peakVelocityDegPerSec{0.0f};

            /// Profile parameter for the active saccade.
            float vMax{DEFAULT_VMAX};
            float saturationC{DEFAULT_C};
        };

        /// @brief Calculates the peak saccadic velocity for a given angular amplitude.
        static float CalculatePeakVelocity(float amplitudeDeg,
                                           float vMax = DEFAULT_VMAX,
                                           float c = DEFAULT_C) noexcept
        {
            if (amplitudeDeg <= 0.0f)
                return 0.0f;
            if (c <= 0.0f)
                return vMax;
            return vMax * (1.0f - std::exp(-amplitudeDeg / c));
        }

        /// @brief Calculates total ballistic duration based on Main Sequence linear relationship.
        static float CalculateDuration(float amplitudeDeg) noexcept
        {
            return BASE_DURATION_SEC + (DURATION_SLOPE * std::abs(amplitudeDeg));
        }

        /// @brief Normalised velocity profile at normalised time t.
        ///
        /// A skewed Gaussian peaking at t = PROFILE_MU. Returns a value in (0, 1],
        /// reaching exactly 1.0 at t = mu.
        static float VelocityProfile(float t) noexcept
        {
            constexpr float invTwoSigmaSq = 1.0f / (2.0f * PROFILE_SIGMA * PROFILE_SIGMA);
            const float d = t - PROFILE_MU;
            return std::exp(-d * d * invTwoSigmaSq);
        }

        /// @brief Fraction of the saccade distance completed at normalised time t.
        ///
        /// The normalised integral of the velocity profile, so that progress reaches
        /// exactly 1.0 at t = 1 and the profile's peak velocity corresponds to V_peak.
        /// Sampled by the trapezoidal rule. Cheap, stable, and monotonic.
        static float ProgressAt(float t) noexcept
        {
            if (t <= 0.0f)
                return 0.0f;
            if (t >= 1.0f)
                return 1.0f;

            constexpr int kSteps = 32;
            const float dt = t / static_cast<float>(kSteps);

            float sum = VelocityProfile(0.0f) + VelocityProfile(t);
            for (int i = 1; i < kSteps; ++i)
            {
                sum += 2.0f * VelocityProfile(static_cast<float>(i) * dt);
            }

            // Trapezoidal integral over [0, t], normalised by the full-profile area.
            const float integral = (dt * 0.5f) * sum;
            return std::clamp(integral / PROFILE_AREA, 0.0f, 1.0f);
        }

        /// @brief Triggers a new ballistic saccade toward a target gaze angle.
        static void TriggerSaccade(SaccadeState &state, float targetYaw, float targetPitch,
                                   float vMax = DEFAULT_VMAX, float c = DEFAULT_C) noexcept
        {
            float dy = targetYaw - state.currentYaw;
            float dp = targetPitch - state.currentPitch;
            float amplitude = std::sqrt(dy * dy + dp * dp);

            state.vMax = vMax;
            state.saturationC = c;

            if (amplitude < FIXATION_THRESHOLD_DEG)
            {
                state.currentYaw = targetYaw;
                state.currentPitch = targetPitch;
                state.isBallistic = false;
                state.amplitudeDeg = amplitude;
                state.peakVelocityDegPerSec = 0.0f;
                return;
            }

            state.isBallistic = true;
            state.amplitudeDeg = amplitude;
            state.totalDurationSec = CalculateDuration(amplitude);
            state.peakVelocityDegPerSec = CalculatePeakVelocity(amplitude, vMax, c);
            state.elapsedSec = 0.0f;
            state.startYaw = state.currentYaw;
            state.startPitch = state.currentPitch;
            state.targetYaw = targetYaw;
            state.targetPitch = targetPitch;
        }

        /// @brief Steps the saccade along its Main Sequence velocity profile.
        static void Update(SaccadeState &state, float deltaSeconds) noexcept
        {
            if (!state.isBallistic)
                return;

            if (deltaSeconds <= 0.0f)
                return;

            state.elapsedSec += deltaSeconds;

            if (state.elapsedSec >= state.totalDurationSec || state.totalDurationSec <= 0.0f)
            {
                state.currentYaw = state.targetYaw;
                state.currentPitch = state.targetPitch;
                state.isBallistic = false;
                return;
            }

            float t = std::clamp(state.elapsedSec / state.totalDurationSec, 0.0f, 1.0f);
            float progress = ProgressAt(t);

            state.currentYaw = state.startYaw + (state.targetYaw - state.startYaw) * progress;
            state.currentPitch = state.startPitch + (state.targetPitch - state.startPitch) * progress;
        }

        /// @brief Current angular velocity implied by the active profile (deg/s). Diagnostic.
        [[nodiscard]] static float CurrentVelocity(const SaccadeState &state) noexcept
        {
            if (!state.isBallistic || state.totalDurationSec <= 0.0f)
                return 0.0f;

            const float t = std::clamp(state.elapsedSec / state.totalDurationSec, 0.0f, 1.0f);
            const float amplitudeRate =
                state.amplitudeDeg / state.totalDurationSec * PROFILE_WINDOW;
            return VelocityProfile(t) * amplitudeRate;
        }
    };

} // namespace TrueGaze::Kinematics
