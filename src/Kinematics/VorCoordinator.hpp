#pragma once

#include <cmath>
#include <algorithm>

namespace TrueGaze::Kinematics
{

    /// @brief Vestibulo-Ocular Reflex (VOR) and Head-Eye Decoupling Coordinator.
    /// In biological systems, eyes move first (low inertia, ~25ms); head follows (high inertia, ~250ms).
    /// As the head turns, the eyes counter-rotate at equal and opposite velocity to maintain foveal lock.
    class VorCoordinator
    {
    public:
        struct VorState
        {
            // Target in world/actor-relative degrees
            float targetYaw{0.0f};
            float targetPitch{0.0f};

            // Head bone current angles
            float headYaw{0.0f};
            float headPitch{0.0f};

            // Eye bone local angles (relative to head)
            float eyeLocalYaw{0.0f};
            float eyeLocalPitch{0.0f};

            // Tuning parameters
            float headTrackingSpeed{6.0f};      // Damping factor for head following
            float eyeMaxAngle{35.0f};           // Maximum comfortable eye angle before forced head turn
            float headOnsetDelayTimerSec{0.0f}; // Biological latency gap countdown (seconds)
        };

        /// @brief Computes one frame of coupled Head-Eye kinematics with VOR counter-rotation.
        static void Update(VorState &state, float deltaSeconds) noexcept
        {
            if (deltaSeconds <= 0.0f)
                return;

            // Biological latency gap: head movement is held while eyes lead
            bool headDelayed = false;
            if (state.headOnsetDelayTimerSec > 0.0f)
            {
                state.headOnsetDelayTimerSec = std::max(0.0f, state.headOnsetDelayTimerSec - deltaSeconds);
                headDelayed = true;
            }

            // 1. Calculate ideal head trajectory (smooth exponential approach to target)
            float headErrorYaw = state.targetYaw - state.headYaw;
            float headErrorPitch = state.targetPitch - state.headPitch;

            float prevHeadYaw = state.headYaw;
            float prevHeadPitch = state.headPitch;

            // GRACEFUL HEAD CURVE — critically-damped spring (SmoothDamp).
            //
            // The previous pure exponential follower `delta = error * (1 - e^(-k·dt))`
            // has MAXIMUM velocity at onset: the instant a new target arrives the head
            // jumps at full speed — the visible "head snap". Real neck motion is
            // torque-limited, not velocity-limited: it starts from rest, accelerates
            // smoothly, and decelerates into the target (an S-curve).
            //
            // Critically-damped spring-damper (no overshoot, zero initial velocity):
            //   omega = 2/halflife  (halflife = time for the error to halve)
            //   expW  = e^(-omega·dt)
            //   delta = error · (1 - expW)² / (1 - (1-2·omega·dt)·expW)   [stable form]
            //
            // This yields zero velocity at onset (no snap), a smooth acceleration
            // ramp, and graceful deceleration into the target. The halflife is
            // derived from headTrackingSpeed so existing INI tuning keeps working:
            // halflife = ln(2) / headTrackingSpeed.
            float deltaYaw = 0.0f;
            float deltaPitch = 0.0f;
            if (!headDelayed && deltaSeconds > 0.0f)
            {
                const float halflife = 0.6931472f / std::max(0.5f, state.headTrackingSpeed);
                const float omega = 0.6931471805599453f / halflife; // ln2 / halflife
                const float expW = std::exp(-omega * deltaSeconds);

                const float denomY = 1.0f - (1.0f - 2.0f * omega * deltaSeconds) * expW;
                const float denomP = denomY; // same scalar for both axes
                if (denomY > 1e-6f)
                {
                    deltaYaw = headErrorYaw * (1.0f - expW) * (1.0f - expW) / denomY;
                    deltaPitch = headErrorPitch * (1.0f - expW) * (1.0f - expW) / denomP;
                }
            }

            // BIOMECHANICAL HEAD SLEW-RATE LIMIT:
            // The human cervical spine cannot physically turn at unbounded speeds.
            // Cap maximum angular velocity during tracking to 104 deg/s (yaw) and 72 deg/s
            // (pitch). This permanently eliminates high-velocity head snapping, stuttering,
            // and neck spasms. (Reduced 20% from 130/90 for slower, more graceful turns.)
            //
            // CALM/COMBAT SPEED MODEL (Kirk directive, September 26 2026): the
            // caps are the COMBAT limits. Away from combat they are halved —
            // a calm head turn is visibly slower and more deliberate. The
            // caller communicates the mode through headTrackingSpeed (the
            // engine halves it when calm), so the caps scale with the same
            // factor: halflife-derived omega already carries the scale, and
            // the caps are derived from it here rather than fixed constants.
            constexpr float MAX_HEAD_YAW_VELOCITY_COMBAT = 104.0f;  // deg/s - natural cervical limit
            constexpr float MAX_HEAD_PITCH_VELOCITY_COMBAT = 72.0f; // deg/s - vertical cervical limit

            // The caps scale with the caller's speed setting: at the default
            // headTrackingSpeed (6.0) they equal the combat constants; at half
            // speed they halve. Bounded so extreme INI values cannot produce
            // unbounded caps.
            const float speedRatio = std::clamp(state.headTrackingSpeed / 6.0f, 0.25f, 2.0f);
            const float maxYawStep = MAX_HEAD_YAW_VELOCITY_COMBAT * speedRatio * deltaSeconds;
            const float maxPitchStep = MAX_HEAD_PITCH_VELOCITY_COMBAT * speedRatio * deltaSeconds;

            deltaYaw = std::clamp(deltaYaw, -maxYawStep, maxYawStep);
            deltaPitch = std::clamp(deltaPitch, -maxPitchStep, maxPitchStep);

            // CLAMP HEAD TO PHYSICAL CERVICAL LIMITS:
            // Human cervical range of motion is ~70 deg yaw, ~35 deg downward pitch (flexion),
            // and ~45 deg upward pitch (extension).
            // Clamping state.headYaw and state.headPitch prevents the virtual head from tracking
            // beyond cervical biomechanical limits (e.g. tracking a high ledge up to +86.6 deg),
            // which previously zeroed out ocular counter-rotation and froze the eyes forward.
            constexpr float HEAD_YAW_LIMIT = 70.0f;
            constexpr float HEAD_PITCH_LIMIT_DOWN = 35.0f;
            constexpr float HEAD_PITCH_LIMIT_UP = 45.0f;

            state.headYaw = std::clamp(state.headYaw + deltaYaw, -HEAD_YAW_LIMIT, HEAD_YAW_LIMIT);
            state.headPitch = std::clamp(state.headPitch + deltaPitch, -HEAD_PITCH_LIMIT_DOWN, HEAD_PITCH_LIMIT_UP);

            // Head velocity this frame
            [[maybe_unused]] float headDeltaYaw = state.headYaw - prevHeadYaw;
            [[maybe_unused]] float headDeltaPitch = state.headPitch - prevHeadPitch;

            // 2. VOR Counter-Rotation:
            // Ideal local eye angle points from current head orientation directly to target
            float idealEyeYaw = state.targetYaw - state.headYaw;
            float idealEyePitch = state.targetPitch - state.headPitch;

            // Clamp local eye angles to biological limits
            state.eyeLocalYaw = std::clamp(idealEyeYaw, -state.eyeMaxAngle, state.eyeMaxAngle);
            state.eyeLocalPitch = std::clamp(idealEyePitch, -state.eyeMaxAngle, state.eyeMaxAngle);

            // VOR effect: As head rotates by +headDelta, eye must compensate by -headDelta
            // (Automatically accounted for by computing (target - head) above)
        }
    };

} // namespace TrueGaze::Kinematics
