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

            // Damped head following (inhibited during biological latency delay)
            float alpha = headDelayed ? 0.0f : (1.0f - std::exp(-state.headTrackingSpeed * deltaSeconds));
            float deltaYaw = headErrorYaw * alpha;
            float deltaPitch = headErrorPitch * alpha;

            // BIOMECHANICAL HEAD SLEW-RATE LIMIT:
            // The human cervical spine cannot physically turn at unbounded speeds.
            // Cap maximum angular velocity during tracking to 104 deg/s (yaw) and 72 deg/s
            // (pitch). This permanently eliminates high-velocity head snapping, stuttering,
            // and neck spasms. (Reduced 20% from 130/90 for slower, more graceful turns.)
            constexpr float MAX_HEAD_YAW_VELOCITY = 104.0f;  // deg/s - natural cervical limit
            constexpr float MAX_HEAD_PITCH_VELOCITY = 72.0f; // deg/s - vertical cervical limit

            const float maxYawStep = MAX_HEAD_YAW_VELOCITY * deltaSeconds;
            const float maxPitchStep = MAX_HEAD_PITCH_VELOCITY * deltaSeconds;

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
