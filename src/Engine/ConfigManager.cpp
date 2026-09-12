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

namespace TrueGaze::Engine
{

    namespace
    {

        float ReadFloat(const char *section, const char *key, float defaultValue, const char *path) noexcept
        {
            char buf[64]{0};
            char defStr[64]{0};
            snprintf(defStr, sizeof(defStr), "%f", defaultValue);

            GetPrivateProfileStringA(section, key, defStr, buf, sizeof(buf), path);
            char *end = nullptr;
            float val = std::strtof(buf, &end);
            return (end != buf) ? val : defaultValue;
        }

        bool ReadBool(const char *section, const char *key, bool defaultValue, const char *path) noexcept
        {
            char buf[32]{0};
            const char *defStr = defaultValue ? "true" : "false";

            GetPrivateProfileStringA(section, key, defStr, buf, sizeof(buf), path);
            return (_stricmp(buf, "true") == 0 || _stricmp(buf, "1") == 0);
        }

    } // namespace

    void ConfigManager::Load(const std::string &customPath) noexcept
    {
        std::string path = customPath;

        if (path.empty())
        {
            // Search order matters. The plugin is loaded by SKSE from the game root,
            // so Data/SKSE/Plugins is the correct location; the others exist for
            // standalone testing and for users who drop the INI beside the exe.
            const std::array<std::string, 4> candidates = {
                "Data/SKSE/Plugins/TrueGaze.ini",
                "SKSE/Plugins/TrueGaze.ini",
                "TrueGaze.ini",
                "config/TrueGaze.ini"};

            for (const auto &candidate : candidates)
            {
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec))
                {
                    path = std::filesystem::absolute(candidate, ec).string();
                    break;
                }
            }
        }

        if (path.empty() || !std::filesystem::exists(path))
        {
            // No INI found. Defaults stand. Reported at info level because a missing
            // INI is a normal, fully supported configuration — not an error.
            logger::info("[TrueGaze] No TrueGaze.ini found; using compiled defaults "
                         "(saccadeMult={:.2f}, jitter={:.2f}, headSpeed={:.2f}).",
                         saccadeSpeedMult, microJitterAmp, headTrackingSpeed);
            _loaded = true;
            return;
        }

        const char *p = path.c_str();

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
        char pipeBuf[256]{0};
        GetPrivateProfileStringA("Bridge", "sPipeName", pipeName.c_str(), pipeBuf, sizeof(pipeBuf), p);
        pipeName = pipeBuf;
        autoReconnectIntervalSec = ReadFloat("Bridge", "fAutoReconnectIntervalSec", autoReconnectIntervalSec, p);

        // LOD
        tier1DistanceMeters = ReadFloat("LOD", "fTier1DistanceMeters", tier1DistanceMeters, p);
        tier2DistanceMeters = ReadFloat("LOD", "fTier2DistanceMeters", tier2DistanceMeters, p);

        // Debug
        debugGazeRays = ReadBool("Debug", "bDebugGazeRays", debugGazeRays, p);
        logLevel = GetPrivateProfileIntA("Debug", "iLogLevel", logLevel, p);

        Sanitise();

        _loaded = true;

        logger::info("[TrueGaze] Configuration loaded from '{}'.", path);
        logger::info("[TrueGaze]   saccadeMult={:.2f} jitter={:.2f} headSpeed={:.2f} "
                     "eyeMax={:.1f} socialTriangle={} aversion={} bridge={}",
                     saccadeSpeedMult, microJitterAmp, headTrackingSpeed, maxComfortEyeAngle,
                     enableSocialTriangle ? "on" : "off",
                     enableGazeAversion ? "on" : "off",
                     connectHcepBridge ? "on" : "off");
    }

    void ConfigManager::Sanitise() noexcept
    {
        // A bad INI must not be able to produce nonsense physics or a crash. Each
        // range below is the physiologically or operationally meaningful one, and
        // clamping is reported so a user can see why their value was ignored.
        const auto clampReport = [](const char *name, float value, float lo, float hi) -> float
        {
            if (value < lo || value > hi)
            {
                const float clamped = value < lo ? lo : hi;
                logger::warn("[TrueGaze] {} = {:.3f} is out of range [{:.2f}, {:.2f}]; clamped to {:.3f}.",
                             name, value, lo, hi, clamped);
                return clamped;
            }
            return value;
        };

        saccadeSpeedMult = clampReport("fSaccadeSpeedMult", saccadeSpeedMult, 0.1f, 5.0f);
        velocitySaturation = clampReport("fVelocitySaturation", velocitySaturation, 1.0f, 90.0f);
        microJitterAmp = clampReport("fMicroJitterAmp", microJitterAmp, 0.0f, 3.0f);
        headTrackingSpeed = clampReport("fHeadTrackingSpeed", headTrackingSpeed, 0.5f, 40.0f);
        maxComfortEyeAngle = clampReport("fMaxComfortEyeAngle", maxComfortEyeAngle, 5.0f, 45.0f);
        triangleFixationDuration = clampReport("fTriangleFixationDuration", triangleFixationDuration, 0.05f, 2.0f);
        mutualGazeThreshold = clampReport("fMutualGazeThreshold", mutualGazeThreshold, 0.1f, 30.0f);
        autoReconnectIntervalSec = clampReport("fAutoReconnectIntervalSec", autoReconnectIntervalSec, 0.25f, 60.0f);
        tier1DistanceMeters = clampReport("fTier1DistanceMeters", tier1DistanceMeters, 1.0f, 50.0f);
        tier2DistanceMeters = clampReport("fTier2DistanceMeters", tier2DistanceMeters, 2.0f, 200.0f);

        // Tier 2 must be beyond tier 1, or the LOD tiers invert and actors near the
        // player silently fall through to the culled tier.
        if (tier2DistanceMeters <= tier1DistanceMeters)
        {
            tier2DistanceMeters = tier1DistanceMeters + 5.0f;
            logger::warn("[TrueGaze] fTier2DistanceMeters must exceed fTier1DistanceMeters; "
                         "adjusted to {:.1f}.",
                         tier2DistanceMeters);
        }
    }

} // namespace TrueGaze::Engine
