#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ConfigManager.hpp"
#include <filesystem>
#include <cstdlib>

namespace TrueGaze::Engine {

namespace {

float ReadFloat(const char* section, const char* key, float defaultValue, const char* path) noexcept
{
    char buf[64]{ 0 };
    char defStr[64]{ 0 };
    snprintf(defStr, sizeof(defStr), "%f", defaultValue);

    GetPrivateProfileStringA(section, key, defStr, buf, sizeof(buf), path);
    char* end = nullptr;
    float val = std::strtof(buf, &end);
    return (end != buf) ? val : defaultValue;
}

bool ReadBool(const char* section, const char* key, bool defaultValue, const char* path) noexcept
{
    char buf[32]{ 0 };
    const char* defStr = defaultValue ? "true" : "false";

    GetPrivateProfileStringA(section, key, defStr, buf, sizeof(buf), path);
    return (_stricmp(buf, "true") == 0 || _stricmp(buf, "1") == 0);
}

} // namespace

void ConfigManager::Load(const std::string& customPath) noexcept
{
    std::string path = customPath;

    if (path.empty()) {
        const std::array<std::string, 3> candidates = {
            "Data/SKSE/Plugins/TrueGaze.ini",
            "SKSE/Plugins/TrueGaze.ini",
            "TrueGaze.ini"
        };

        for (const auto& candidate : candidates) {
            if (std::filesystem::exists(candidate)) {
                path = std::filesystem::absolute(candidate).string();
                break;
            }
        }
    }

    if (path.empty() || !std::filesystem::exists(path)) {
        return; // Fallback to compiled default values
    }

    const char* p = path.c_str();

    // General
    enableTrueGaze = ReadBool("General", "bEnableTrueGaze", enableTrueGaze, p);
    enableCreatures = ReadBool("General", "bEnableCreatures", enableCreatures, p);

    // Kinematics
    saccadeSpeedMult = ReadFloat("Kinematics", "fSaccadeSpeedMult", saccadeSpeedMult, p);
    velocitySaturation = ReadFloat("Kinematics", "fVelocitySaturation", velocitySaturation, p);
    microJitterAmp = ReadFloat("Kinematics", "fMicroJitterAmp", microJitterAmp, p);
    headTrackingSpeed = ReadFloat("Kinematics", "fHeadTrackingSpeed", headTrackingSpeed, p);
    maxComfortEyeAngle = ReadFloat("Kinematics", "fMaxComfortEyeAngle", maxComfortEyeAngle, p);

    // Social
    enableGazeAversion = ReadBool("Social", "bEnableGazeAversion", enableGazeAversion, p);
    enableSocialTriangle = ReadBool("Social", "bEnableSocialTriangle", enableSocialTriangle, p);
    triangleFixationDuration = ReadFloat("Social", "fTriangleFixationDuration", triangleFixationDuration, p);
    mutualGazeThreshold = ReadFloat("Social", "fMutualGazeThreshold", mutualGazeThreshold, p);

    // Bridge
    connectHcepBridge = ReadBool("Bridge", "bConnectHcepBridge", connectHcepBridge, p);
    char pipeBuf[256]{ 0 };
    GetPrivateProfileStringA("Bridge", "sPipeName", pipeName.c_str(), pipeBuf, sizeof(pipeBuf), p);
    pipeName = pipeBuf;
    autoReconnectIntervalSec = ReadFloat("Bridge", "fAutoReconnectIntervalSec", autoReconnectIntervalSec, p);

    // LOD
    tier1DistanceMeters = ReadFloat("LOD", "fTier1DistanceMeters", tier1DistanceMeters, p);
    tier2DistanceMeters = ReadFloat("LOD", "fTier2DistanceMeters", tier2DistanceMeters, p);

    // Debug
    debugGazeRays = ReadBool("Debug", "bDebugGazeRays", debugGazeRays, p);
    logLevel = GetPrivateProfileIntA("Debug", "iLogLevel", logLevel, p);
}

} // namespace TrueGaze::Engine
