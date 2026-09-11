#pragma once

#include "PCH.h"
#include <array>

namespace TrueGaze::Engine {

/// @brief Skyrim VR specific HMD Pose and Foveated Gaze Coordinator.
/// In Skyrim VR, the player's head orientation is driven directly by OpenVR / SteamVR 6DOF tracking.
/// VrController integrates the HMD transform and coordinates gaze vectors in 3D stereoscopic space.
class VrController
{
public:
    struct HmdPose
    {
        float posX{ 0.0f };
        float posY{ 0.0f };
        float posZ{ 0.0f };
        float yawRad{ 0.0f };
        float pitchRad{ 0.0f };
        float rollRad{ 0.0f };
        bool isValid{ false };
    };

    /// @brief Checks whether the plugin is executing inside Skyrim VR runtime.
    static bool IsSkyrimVr() noexcept;

    /// @brief Fetches the current 6DOF HMD head pose in world space coordinates.
    static HmdPose GetHmdPose() noexcept;

    /// @brief Projects the VR player's foveal gaze ray into the game world for salience intersection.
    static void GetVrGazeRay(float& outOriginX, float& outOriginY, float& outOriginZ,
                            float& outDirX, float& outDirY, float& outDirZ) noexcept;
};

} // namespace TrueGaze::Engine
