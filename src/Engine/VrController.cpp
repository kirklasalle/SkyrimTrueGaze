#include "VrController.hpp"
#include "Engine/GazeAnchors.hpp"
#include <algorithm>
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

    // Prefer Skyrim VR's live-tracked 6DOF HMD node (VR_NODE_DATA::HmdNode) when
    // available; this reflects true headset position, pitch, yaw, and roll in world
    // space rather than the flat body orientation.
    if (auto* vrData = player->GetVRNodeData(); vrData && vrData->HmdNode)
    {
        const auto& world = vrData->HmdNode->world;
        const auto& t = world.translate;
        const auto& r = world.rotate;
        if (std::isfinite(t.x) && std::isfinite(t.y) && std::isfinite(t.z))
        {
            // Skyrim NetImmerse forward axis is column 1 (+Y local).
            const float fx = r.entry[0][1];
            const float fy = r.entry[1][1];
            const float fz = r.entry[2][1];
            const float fwdLenSq = fx * fx + fy * fy + fz * fz;
            if (std::isfinite(fwdLenSq) && fwdLenSq > 1e-4f)
            {
                const float invLen = 1.0f / std::sqrt(fwdLenSq);
                const float nx = fx * invLen;
                const float ny = fy * invLen;
                const float nz = std::clamp(fz * invLen, -1.0f, 1.0f);

                pose.posX = t.x;
                pose.posY = t.y;
                pose.posZ = t.z;
                // Consistent with GetVrGazeRay: dir = (sin(yaw)*cos(pitch), cos(yaw)*cos(pitch), -sin(pitch))
                pose.yawRad = std::atan2(nx, ny);
                pose.pitchRad = -std::asin(nz);
                pose.rollRad = std::atan2(r.entry[2][0], r.entry[2][2]);
                pose.isValid = true;
                return pose;
            }
        }
    }

    // Fallback during cell transitions / before HmdNode initializes: use canonical
    // eye-height constant from GazeAnchors (125.0 Skyrim units) rather than stale 160.
    auto pos = player->GetPosition();
    pose.posX = pos.x;
    pose.posY = pos.y;
    pose.posZ = pos.z + TrueGaze::Anchors::kEyeHeightUnits;
    pose.yawRad = player->GetAngleZ();
    pose.pitchRad = player->GetAngleX();
    pose.isValid = true;
#else
    pose.posX = 0.0f;
    pose.posY = 0.0f;
    pose.posZ = TrueGaze::Anchors::kEyeHeightUnits;
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
