#pragma once

#include <cstdint>

/// @file TrueGazeUE5.h
/// @brief Unreal Engine 5 AnimNode and LiveLink Bridge Specification for TrueGaze.
/// Grounded in Kirk LaSalle's Human Communication Eye Protocol (HCEP).
///
/// In Unreal Engine 5, TrueGaze can be consumed either:
/// 1. As an AnimNode (`FAnimNode_TrueGazeOculomotor`) modifying eye and head bones in the AnimGraph.
/// 2. As a LiveLink Source receiving 64-byte `TrueGazeTelemetryPacket` frames over Named Pipe / UDP.

namespace TrueGaze::UE5 {

struct FTrueGazeBoneTransforms
{
    // Euler angles in degrees (Roll, Pitch, Yaw)
    float Spine2Pitch{ 0.0f };
    float Spine2Yaw{ 0.0f };
    float NeckPitch{ 0.0f };
    float NeckYaw{ 0.0f };
    float HeadPitch{ 0.0f };
    float HeadYaw{ 0.0f };
    float LeftEyePitch{ 0.0f };
    float LeftEyeYaw{ 0.0f };
    float RightEyePitch{ 0.0f };
    float RightEyeYaw{ 0.0f };
    float EyelidBlinkWeight{ 0.0f }; // [0.0f, 1.0f]
};

/// @brief Evaluates biological gaze transformations for an Unreal Engine skeletal mesh.
class FTrueGazeEvaluator
{
public:
    static FTrueGazeBoneTransforms EvaluateGaze(float TargetYawDeg, float TargetPitchDeg, float DeltaSeconds) noexcept;
};

} // namespace TrueGaze::UE5
