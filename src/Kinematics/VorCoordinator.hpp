#pragma once

#include <cmath>
#include <algorithm>

namespace TrueGaze::Kinematics {

/// @brief Vestibulo-Ocular Reflex (VOR) and Head-Eye Decoupling Coordinator.
/// In biological systems, eyes move first (low inertia, ~25ms); head follows (high inertia, ~250ms).
/// As the head turns, the eyes counter-rotate at equal and opposite velocity to maintain foveal lock.
class VorCoordinator
{
public:
    struct VorState
    {
        // Target in world/actor-relative degrees
        float targetYaw{ 0.0f };
        float targetPitch{ 0.0f };

        // Head bone current angles
        float headYaw{ 0.0f };
        float headPitch{ 0.0f };

        // Eye bone local angles (relative to head)
        float eyeLocalYaw{ 0.0f };
        float eyeLocalPitch{ 0.0f };

        // Tuning parameters
        float headTrackingSpeed{ 6.0f };  // Damping factor for head following
        float eyeMaxAngle{ 35.0f };       // Maximum comfortable eye angle before forced head turn
    };

    /// @brief Computes one frame of coupled Head-Eye kinematics with VOR counter-rotation.
    static void Update(VorState& state, float deltaSeconds) noexcept
    {
        if (deltaSeconds <= 0.0f) return;

        // 1. Calculate ideal head trajectory (smooth exponential approach to target)
        float headErrorYaw = state.targetYaw - state.headYaw;
        float headErrorPitch = state.targetPitch - state.headPitch;

        float prevHeadYaw = state.headYaw;
        float prevHeadPitch = state.headPitch;

        // Damped head following
        float alpha = 1.0f - std::exp(-state.headTrackingSpeed * deltaSeconds);
        state.headYaw += headErrorYaw * alpha;
        state.headPitch += headErrorPitch * alpha;

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
