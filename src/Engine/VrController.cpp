#include "VrController.hpp"
#include <cmath>

namespace TrueGaze::Engine {

bool VrController::IsSkyrimVr() noexcept
{
#if defined(SKYRIMVR)
    return true;
#elif __has_include(<RE/Skyrim.h>)
    return REL::Module::IsVR();
#else
    return false;
#endif
}

VrController::HmdPose VrController::GetHmdPose() noexcept
{
    HmdPose pose{};

#if __has_include(<RE/Skyrim.h>)
    if (!IsSkyrimVr()) return pose;

    auto* player = RE::PlayerCharacter::GetSingleton();
    if (!player) return pose;

    auto pos = player->GetPosition();
    pose.posX = pos.x;
    pose.posY = pos.y;
    pose.posZ = pos.z + 160.0f; // Eye-line approximate in Skyrim units
    pose.yawRad = player->GetAngleZ();
    pose.pitchRad = player->GetAngleX();
    pose.isValid = true;
#else
    pose.posX = 0.0f;
    pose.posY = 0.0f;
    pose.posZ = 160.0f;
    pose.isValid = false;
#endif

    return pose;
}

void VrController::GetVrGazeRay(float& outOriginX, float& outOriginY, float& outOriginZ,
                               float& outDirX, float& outDirY, float& outDirZ) noexcept
{
    HmdPose pose = GetHmdPose();
    outOriginX = pose.posX;
    outOriginY = pose.posY;
    outOriginZ = pose.posZ;

    // Standard spherical coordinates from pitch and yaw
    float cy = std::cos(pose.yawRad);
    float sy = std::sin(pose.yawRad);
    float cp = std::cos(pose.pitchRad);
    float sp = std::sin(pose.pitchRad);

    outDirX = sy * cp;
    outDirY = cy * cp;
    outDirZ = -sp;
}

} // namespace TrueGaze::Engine
