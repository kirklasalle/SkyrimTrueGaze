#pragma once

#include <string>
#include <cstdint>

namespace TrueGaze::Engine {

/// @brief Loads, manages, and hot-reloads runtime settings from Data/SKSE/Plugins/TrueGaze.ini.
class ConfigManager
{
public:
    static ConfigManager& GetSingleton() noexcept
    {
        static ConfigManager instance;
        return instance;
    }

    /// @brief Loads configuration values from TrueGaze.ini.
    void Load(const std::string& iniPath = "") noexcept;

    // --- General Settings ---
    bool enableTrueGaze{ true };
    bool enableCreatures{ true };

    // --- Kinematics ---
    float saccadeSpeedMult{ 1.0f };
    float velocitySaturation{ 14.0f };
    float microJitterAmp{ 0.35f };
    float headTrackingSpeed{ 6.0f };
    float maxComfortEyeAngle{ 35.0f };

    // --- Social ---
    bool enableGazeAversion{ true };
    bool enableSocialTriangle{ true };
    float triangleFixationDuration{ 0.35f };
    float mutualGazeThreshold{ 2.0f };

    // --- Bridge ---
    bool connectHcepBridge{ true };
    std::string pipeName{ R"(\\.\pipe\TrueGazeBridge)" };
    float autoReconnectIntervalSec{ 3.0f };

    // --- LOD ---
    float tier1DistanceMeters{ 5.0f };
    float tier2DistanceMeters{ 15.0f };

    // --- Debug ---
    bool debugGazeRays{ false };
    int logLevel{ 2 };

private:
    ConfigManager() = default;
};

} // namespace TrueGaze::Engine
