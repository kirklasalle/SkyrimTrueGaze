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

    void ConfigManager::ApplyIni(const std::string &path) noexcept
    {
        // Read every managed value from a single INI, using the value already held in
        // memory as the default. This lets the function be called more than once to
        // *layer* files: a later call only overrides keys that the later file actually
        // contains, and leaves everything else untouched.
        const char *p = path.c_str();

        // General
        enableTrueGaze = ReadBool("General", "bEnableTrueGaze", enableTrueGaze, p);
        enableCreatures = ReadBool("General", "bEnableCreatures", enableCreatures, p);
        {
            char targetBuf[32]{0};
            GetPrivateProfileStringA("General", "sEngineTarget", engineTarget.c_str(), targetBuf, sizeof(targetBuf), p);
            engineTarget = targetBuf;
        }

        // Kinematics
        saccadeSpeedMult = ReadFloat("Kinematics", "fSaccadeSpeedMult", saccadeSpeedMult, p);
        velocitySaturation = ReadFloat("Kinematics", "fVelocitySaturation", velocitySaturation, p);
        microJitterAmp = ReadFloat("Kinematics", "fMicroJitterAmp", microJitterAmp, p);
        microJitterIntervalMin = ReadFloat("Kinematics", "fMicroJitterIntervalMin", microJitterIntervalMin, p);
        microJitterIntervalMax = ReadFloat("Kinematics", "fMicroJitterIntervalMax", microJitterIntervalMax, p);
        headTrackingSpeed = ReadFloat("Kinematics", "fHeadTrackingSpeed", headTrackingSpeed, p);
        maxComfortEyeAngle = ReadFloat("Kinematics", "fMaxComfortEyeAngle", maxComfortEyeAngle, p);
        headOnsetDelaySec = ReadFloat("Kinematics", "fHeadOnsetDelaySec", headOnsetDelaySec, p);

        // SkeletalHierarchy
        spine2YawWeight = ReadFloat("SkeletalHierarchy", "fSpine2YawWeight", spine2YawWeight, p);
        neckYawWeight = ReadFloat("SkeletalHierarchy", "fNeckYawWeight", neckYawWeight, p);
        neckPitchWeight = ReadFloat("SkeletalHierarchy", "fNeckPitchWeight", neckPitchWeight, p);
        headYawWeight = ReadFloat("SkeletalHierarchy", "fHeadYawWeight", headYawWeight, p);
        headPitchWeight = ReadFloat("SkeletalHierarchy", "fHeadPitchWeight", headPitchWeight, p);

        // Social
        enableGazeAversion = ReadBool("Social", "bEnableGazeAversion", enableGazeAversion, p);
        enableSocialTriangle = ReadBool("Social", "bEnableSocialTriangle", enableSocialTriangle, p);
        triangleFixationDuration = ReadFloat("Social", "fTriangleFixationDuration", triangleFixationDuration, p);
        mutualGazeThreshold = ReadFloat("Social", "fMutualGazeThreshold", mutualGazeThreshold, p);

        // Crosshair (player gaze sweet spot)
        enableCrosshairGaze = ReadBool("Crosshair", "bEnableCrosshairGaze", enableCrosshairGaze, p);
        crosshairToleranceDeg = ReadFloat("Crosshair", "fCrosshairToleranceDeg", crosshairToleranceDeg, p);
        crosshairMaxRangeMeters = ReadFloat("Crosshair", "fCrosshairMaxRangeMeters", crosshairMaxRangeMeters, p);
        crosshairPointBlankMeters = ReadFloat("Crosshair", "fCrosshairPointBlankMeters", crosshairPointBlankMeters, p);

        // Bridge
        connectHcepBridge = ReadBool("Bridge", "bConnectHcepBridge", connectHcepBridge, p);
        char pipeBuf[256]{0};
        GetPrivateProfileStringA("Bridge", "sPipeName", pipeName.c_str(), pipeBuf, sizeof(pipeBuf), p);
        pipeName = pipeBuf;
        autoReconnectIntervalSec = ReadFloat("Bridge", "fAutoReconnectIntervalSec", autoReconnectIntervalSec, p);
        // bLockFreeTelemetry intentionally not read: the key was removed (no-op).

        // LOD
        tier1DistanceMeters = ReadFloat("LOD", "fTier1DistanceMeters", tier1DistanceMeters, p);
        tier2DistanceMeters = ReadFloat("LOD", "fTier2DistanceMeters", tier2DistanceMeters, p);

        // Debug
        debugGazeRays = ReadBool("Debug", "bDebugGazeRays", debugGazeRays, p);
        logLevel = GetPrivateProfileIntA("Debug", "iLogLevel", logLevel, p);

        // Visuals (in-game 3D representation of the solved gaze)
        enableInGameVisuals = ReadBool("Visuals", "bEnableInGameVisuals", enableInGameVisuals, p);
        gazeRaysEnabled = ReadBool("Visuals", "bGazeRaysEnabled", gazeRaysEnabled, p);
        rayRenderMode = GetPrivateProfileIntA("Visuals", "iRayRenderMode", rayRenderMode, p);
        gazeRayLengthMeters = ReadFloat("Visuals", "fGazeRayLengthMeters", gazeRayLengthMeters, p);
        gazeRayThicknessCm = ReadFloat("Visuals", "fGazeRayThicknessCm", gazeRayThicknessCm, p);
        gazeRayColour = GetPrivateProfileIntA("Visuals", "iGazeRayColour", gazeRayColour, p);
        gazeRayOpacity = ReadFloat("Visuals", "fGazeRayOpacity", gazeRayOpacity, p);
        gazeRaysOnPlayer = ReadBool("Visuals", "bGazeRaysOnPlayer", gazeRaysOnPlayer, p);
        gazeRaysOnNPCs = ReadBool("Visuals", "bGazeRaysOnNPCs", gazeRaysOnNPCs, p);
        gazeRaysOnCreatures = ReadBool("Visuals", "bGazeRaysOnCreatures", gazeRaysOnCreatures, p);
        gazeRaysAttachHead = ReadBool("Visuals", "bGazeRaysAttachHead", gazeRaysAttachHead, p);
        gazeRaysTerminus = ReadBool("Visuals", "bGazeRaysTerminus", gazeRaysTerminus, p);
        pupilForwardOffsetCm = ReadFloat("Visuals", "fPupilForwardOffsetCm", pupilForwardOffsetCm, p);
        pupilUpOffsetCm = ReadFloat("Visuals", "fPupilUpOffsetCm", pupilUpOffsetCm, p);
        pupilGlowIntensity = ReadFloat("Visuals", "fPupilGlowIntensity", pupilGlowIntensity, p);
        showHcepPanel = ReadBool("Visuals", "bShowHcepPanel", showHcepPanel, p);
        hcepPanelAllActors = ReadBool("Visuals", "bHcepPanelAllActors", hcepPanelAllActors, p);
        hcepPanelScale = ReadFloat("Visuals", "fHcepPanelScale", hcepPanelScale, p);
        hcepPanelForwardOffsetCm = ReadFloat("Visuals", "fHcepPanelForwardOffsetCm", hcepPanelForwardOffsetCm, p);

        // Console commands
        enableConsoleCommands = ReadBool("Console", "bEnableConsoleCommands", enableConsoleCommands, p);
    }

    void ConfigManager::Load(const std::string &customPath) noexcept
    {
        // If a caller forces a specific file, honour it exactly and skip layering.
        if (!customPath.empty())
        {
            if (std::filesystem::exists(customPath))
            {
                ApplyIni(customPath);
                Sanitise();
                _loaded = true;
                _loadedPath = customPath;
                logger::info("[TrueGaze] Configuration loaded from '{}'.", customPath);
            }
            else
            {
                _loadedPath.clear();
                logger::info("[TrueGaze] Requested INI '{}' not found; using compiled defaults.", customPath);
                _loaded = true;
            }
            return;
        }

        // Single-layer load. The INI shipped in Data/SKSE/Plugins supplies the full
        // set of tuning keys.
        const std::array<std::string, 4> baseCandidates = {
            "Data/SKSE/Plugins/TrueGaze.ini",
            "SKSE/Plugins/TrueGaze.ini",
            "TrueGaze.ini",
            "config/TrueGaze.ini"};

        std::string basePath;
        for (const auto &candidate : baseCandidates)
        {
            std::error_code ec;
            if (std::filesystem::exists(candidate, ec))
            {
                basePath = std::filesystem::absolute(candidate, ec).string();
                break;
            }
        }

        if (basePath.empty())
        {
            // No INI found. Defaults stand. Reported at info level because a missing
            // INI is a normal, fully supported configuration — not an error.
            logger::info("[TrueGaze] No TrueGaze.ini found; using compiled defaults "
                         "(saccadeMult={:.2f}, jitter={:.2f}, headSpeed={:.2f}).",
                         saccadeSpeedMult, microJitterAmp, headTrackingSpeed);
            _loadedPath.clear();
            _loaded = true;
            return;
        }

        ApplyIni(basePath);

        Sanitise();

        _loaded = true;
        _loadedPath = basePath;

        logger::info("[TrueGaze] Configuration loaded (base='{}').",
                     basePath);
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
        microJitterIntervalMin = clampReport("fMicroJitterIntervalMin", microJitterIntervalMin, 0.05f, 2.0f);
        microJitterIntervalMax = clampReport("fMicroJitterIntervalMax", microJitterIntervalMax, 0.1f, 4.0f);
        headTrackingSpeed = clampReport("fHeadTrackingSpeed", headTrackingSpeed, 0.5f, 40.0f);
        maxComfortEyeAngle = clampReport("fMaxComfortEyeAngle", maxComfortEyeAngle, 5.0f, 45.0f);
        headOnsetDelaySec = clampReport("fHeadOnsetDelaySec", headOnsetDelaySec, 0.0f, 0.5f);
        spine2YawWeight = clampReport("fSpine2YawWeight", spine2YawWeight, 0.0f, 1.0f);
        neckYawWeight = clampReport("fNeckYawWeight", neckYawWeight, 0.0f, 1.0f);
        neckPitchWeight = clampReport("fNeckPitchWeight", neckPitchWeight, 0.0f, 1.0f);
        headYawWeight = clampReport("fHeadYawWeight", headYawWeight, 0.0f, 1.0f);
        headPitchWeight = clampReport("fHeadPitchWeight", headPitchWeight, 0.0f, 1.0f);
        triangleFixationDuration = clampReport("fTriangleFixationDuration", triangleFixationDuration, 0.05f, 2.0f);
        mutualGazeThreshold = clampReport("fMutualGazeThreshold", mutualGazeThreshold, 0.1f, 30.0f);
        crosshairToleranceDeg = clampReport("fCrosshairToleranceDeg", crosshairToleranceDeg, 0.0f, 30.0f);
        crosshairMaxRangeMeters = clampReport("fCrosshairMaxRangeMeters", crosshairMaxRangeMeters, 2.0f, 100.0f);
        crosshairPointBlankMeters = clampReport("fCrosshairPointBlankMeters", crosshairPointBlankMeters, 0.0f, 10.0f);
        autoReconnectIntervalSec = clampReport("fAutoReconnectIntervalSec", autoReconnectIntervalSec, 0.25f, 60.0f);
        tier1DistanceMeters = clampReport("fTier1DistanceMeters", tier1DistanceMeters, 1.0f, 50.0f);
        tier2DistanceMeters = clampReport("fTier2DistanceMeters", tier2DistanceMeters, 2.0f, 200.0f);

        // Jitter interval: max must exceed min, or the OU reversion-rate derivation
        // inverts and the drift statistics become meaningless.
        if (microJitterIntervalMax <= microJitterIntervalMin)
        {
            microJitterIntervalMax = microJitterIntervalMin + 0.05f;
            logger::warn("[TrueGaze] fMicroJitterIntervalMax must exceed fMicroJitterIntervalMin; "
                         "adjusted to {:.2f}.",
                         microJitterIntervalMax);
        }

        // Yaw strain shares should sum to 1.0. If the user's custom split does not,
        // renormalise so the head chain still covers the full deflection and the eye
        // residual stays meaningful.
        const float yawSum = spine2YawWeight + neckYawWeight + headYawWeight;
        if (yawSum <= 0.0f)
        {
            spine2YawWeight = 0.10f;
            neckYawWeight = 0.25f;
            headYawWeight = 0.65f;
            logger::warn("[TrueGaze] SkeletalHierarchy yaw weights sum to zero; restored defaults.");
        }
        else if (std::abs(yawSum - 1.0f) > 0.001f)
        {
            const float inv = 1.0f / yawSum;
            spine2YawWeight *= inv;
            neckYawWeight *= inv;
            headYawWeight *= inv;
            logger::warn("[TrueGaze] SkeletalHierarchy yaw weights summed to {:.3f}; renormalised to 1.0.",
                         yawSum);
        }

        // Engine target must be a known value; anything else falls back to Auto so a
        // typo cannot silently disable runtime selection.
        {
            static const char *kValidTargets[] = {"Auto", "SE", "AE", "VR"};
            bool valid = false;
            for (const char *t : kValidTargets)
            {
                if (_stricmp(engineTarget.c_str(), t) == 0)
                {
                    engineTarget = t; // canonical casing
                    valid = true;
                    break;
                }
            }
            if (!valid)
            {
                logger::warn("[TrueGaze] sEngineTarget '{}' is not one of Auto/SE/AE/VR; using Auto.",
                             engineTarget);
                engineTarget = "Auto";
            }
        }

        // Log level: documented 0-4 (Trace..Error). Out-of-range values would make
        // the sink filter silently swallow everything or nothing.
        if (logLevel < 0 || logLevel > 4)
        {
            logger::warn("[TrueGaze] iLogLevel {} out of range [0,4]; clamped.", logLevel);
            logLevel = std::clamp(logLevel, 0, 4);
        }

        // Tier 2 must be beyond tier 1, or the LOD tiers invert and actors near the
        // player silently fall through to the culled tier.
        if (tier2DistanceMeters <= tier1DistanceMeters)
        {
            tier2DistanceMeters = tier1DistanceMeters + 5.0f;
            logger::warn("[TrueGaze] fTier2DistanceMeters must exceed fTier1DistanceMeters; "
                         "adjusted to {:.1f}.",
                         tier2DistanceMeters);
        }

        // --- Visuals -----------------------------------------------------------
        // A beam shorter than the eye is a dot; an unbounded one reaches across
        // Whiterun. Both are configuration mistakes rather than preferences.
        rayRenderMode = std::clamp(rayRenderMode, 0, 2);
        gazeRayLengthMeters = clampReport("fGazeRayLengthMeters", gazeRayLengthMeters, 0.5f, 100.0f);
        gazeRayOpacity = clampReport("fGazeRayOpacity", gazeRayOpacity, 0.0f, 1.0f);
        pupilGlowIntensity = clampReport("fPupilGlowIntensity", pupilGlowIntensity, 0.0f, 5.0f);
        pupilForwardOffsetCm = clampReport("fPupilForwardOffsetCm", pupilForwardOffsetCm, 0.0f, 30.0f);
        pupilUpOffsetCm = clampReport("fPupilUpOffsetCm", pupilUpOffsetCm, -30.0f, 30.0f);
        hcepPanelScale = clampReport("fHcepPanelScale", hcepPanelScale, 0.5f, 30.0f);
        hcepPanelForwardOffsetCm = clampReport("fHcepPanelForwardOffsetCm", hcepPanelForwardOffsetCm, 5.0f, 200.0f);

        // Colour is stored as 0xRRGGBB. Mask off any stray high bits so the
        // per-channel extraction in the renderer is always in range.
        gazeRayColour &= 0x00FFFFFF;
    }

    void ConfigManager::Save(const std::string &customPath) noexcept
    {
        std::string path = customPath;
        if (path.empty())
        {
            // Prefer the standard plugin INI location when saving.
            const std::array<std::string, 3> candidates = {
                "Data/SKSE/Plugins/TrueGaze.ini",
                "SKSE/Plugins/TrueGaze.ini",
                "TrueGaze.ini"};

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

        if (path.empty())
        {
            path = "Data/SKSE/Plugins/TrueGaze.ini";
        }

        const char *p = path.c_str();

        auto WriteFloat = [&](const char *sec, const char *key, float val)
        {
            char buf[64]{0};
            snprintf(buf, sizeof(buf), "%.6f", val);
            WritePrivateProfileStringA(sec, key, buf, p);
        };

        auto WriteBool = [&](const char *sec, const char *key, bool val)
        {
            WritePrivateProfileStringA(sec, key, val ? "true" : "false", p);
        };

        WriteBool("General", "bEnableTrueGaze", enableTrueGaze);
        WriteBool("General", "bEnableCreatures", enableCreatures);
        WritePrivateProfileStringA("General", "sEngineTarget", engineTarget.c_str(), p);

        WriteFloat("Kinematics", "fSaccadeSpeedMult", saccadeSpeedMult);
        WriteFloat("Kinematics", "fVelocitySaturation", velocitySaturation);
        WriteFloat("Kinematics", "fMicroJitterAmp", microJitterAmp);
        WriteFloat("Kinematics", "fMicroJitterIntervalMin", microJitterIntervalMin);
        WriteFloat("Kinematics", "fMicroJitterIntervalMax", microJitterIntervalMax);
        WriteFloat("Kinematics", "fHeadTrackingSpeed", headTrackingSpeed);
        WriteFloat("Kinematics", "fMaxComfortEyeAngle", maxComfortEyeAngle);
        WriteFloat("Kinematics", "fHeadOnsetDelaySec", headOnsetDelaySec);

        WriteFloat("SkeletalHierarchy", "fSpine2YawWeight", spine2YawWeight);
        WriteFloat("SkeletalHierarchy", "fNeckYawWeight", neckYawWeight);
        WriteFloat("SkeletalHierarchy", "fNeckPitchWeight", neckPitchWeight);
        WriteFloat("SkeletalHierarchy", "fHeadYawWeight", headYawWeight);
        WriteFloat("SkeletalHierarchy", "fHeadPitchWeight", headPitchWeight);

        WriteBool("Social", "bEnableGazeAversion", enableGazeAversion);
        WriteBool("Social", "bEnableSocialTriangle", enableSocialTriangle);
        WriteFloat("Social", "fTriangleFixationDuration", triangleFixationDuration);
        WriteFloat("Social", "fMutualGazeThreshold", mutualGazeThreshold);

        WriteBool("Crosshair", "bEnableCrosshairGaze", enableCrosshairGaze);
        WriteFloat("Crosshair", "fCrosshairToleranceDeg", crosshairToleranceDeg);
        WriteFloat("Crosshair", "fCrosshairMaxRangeMeters", crosshairMaxRangeMeters);
        WriteFloat("Crosshair", "fCrosshairPointBlankMeters", crosshairPointBlankMeters);

        WriteBool("Bridge", "bConnectHcepBridge", connectHcepBridge);
        WritePrivateProfileStringA("Bridge", "sPipeName", pipeName.c_str(), p);
        WriteFloat("Bridge", "fAutoReconnectIntervalSec", autoReconnectIntervalSec);

        WriteFloat("LOD", "fTier1DistanceMeters", tier1DistanceMeters);
        WriteFloat("LOD", "fTier2DistanceMeters", tier2DistanceMeters);

        WriteBool("Debug", "bDebugGazeRays", debugGazeRays);
        {
            char lvl[16]{0};
            snprintf(lvl, sizeof(lvl), "%d", logLevel);
            WritePrivateProfileStringA("Debug", "iLogLevel", lvl, p);
        }

        // Visuals
        WriteBool("Visuals", "bEnableInGameVisuals", enableInGameVisuals);
        WriteBool("Visuals", "bGazeRaysEnabled", gazeRaysEnabled);
        {
            char mode[16]{0};
            snprintf(mode, sizeof(mode), "%d", rayRenderMode);
            WritePrivateProfileStringA("Visuals", "iRayRenderMode", mode, p);
        }
        WriteFloat("Visuals", "fGazeRayLengthMeters", gazeRayLengthMeters);
        WriteFloat("Visuals", "fGazeRayThicknessCm", gazeRayThicknessCm);
        {
            // 0xRRGGBB reads as decimal in the INI; keep it that way so the HTML
            // page and the engine agree on the representation.
            char col[24]{0};
            snprintf(col, sizeof(col), "%d", gazeRayColour & 0x00FFFFFF);
            WritePrivateProfileStringA("Visuals", "iGazeRayColour", col, p);
        }
        WriteFloat("Visuals", "fGazeRayOpacity", gazeRayOpacity);
        WriteBool("Visuals", "bGazeRaysOnPlayer", gazeRaysOnPlayer);
        WriteBool("Visuals", "bGazeRaysOnNPCs", gazeRaysOnNPCs);
        WriteBool("Visuals", "bGazeRaysOnCreatures", gazeRaysOnCreatures);
        WriteBool("Visuals", "bGazeRaysAttachHead", gazeRaysAttachHead);
        WriteBool("Visuals", "bGazeRaysTerminus", gazeRaysTerminus);
        WriteFloat("Visuals", "fPupilForwardOffsetCm", pupilForwardOffsetCm);
        WriteFloat("Visuals", "fPupilUpOffsetCm", pupilUpOffsetCm);
        WriteFloat("Visuals", "fPupilGlowIntensity", pupilGlowIntensity);
        WriteBool("Visuals", "bShowHcepPanel", showHcepPanel);
        WriteBool("Visuals", "bHcepPanelAllActors", hcepPanelAllActors);
        WriteFloat("Visuals", "fHcepPanelScale", hcepPanelScale);
        WriteFloat("Visuals", "fHcepPanelForwardOffsetCm", hcepPanelForwardOffsetCm);

        WriteBool("Console", "bEnableConsoleCommands", enableConsoleCommands);

        logger::info("[TrueGaze] Configuration saved to '{}'.", path);
    }

} // namespace TrueGaze::Engine
