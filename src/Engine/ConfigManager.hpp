#pragma once

#include <string>
#include <cstdint>

namespace TrueGaze::Engine
{

    /// @brief Loads, manages, and hot-reloads runtime settings from Data/SKSE/Plugins/TrueGaze.ini.
    class ConfigManager
    {
    public:
        static ConfigManager &GetSingleton() noexcept
        {
            static ConfigManager instance;
            return instance;
        }

        /// @brief Loads configuration values from TrueGaze.ini.
        /// Safe to call more than once; each call re-reads the file.
        void Load(const std::string &iniPath = "") noexcept;

        /// @brief True once Load() has completed, whether or not an INI was found.
        [[nodiscard]] bool IsLoaded() const noexcept { return _loaded; }

        /// @brief Clamps every value into its supported range, reporting any change.
        /// Called automatically at the end of Load().
        void Sanitise() noexcept;

        // --- General Settings ---
        bool enableTrueGaze{true};
        bool enableCreatures{true};

        // --- Kinematics ---
        float saccadeSpeedMult{1.0f};
        float velocitySaturation{14.0f};
        float microJitterAmp{0.35f};
        float headTrackingSpeed{6.0f};
        float maxComfortEyeAngle{35.0f};

        // --- Social ---
        bool enableGazeAversion{true};
        bool enableSocialTriangle{true};
        float triangleFixationDuration{0.35f};
        float mutualGazeThreshold{2.0f};

        // --- Bridge ---
        bool connectHcepBridge{true};
        std::string pipeName{R"(\\.\pipe\TrueGazeBridge)"};
        float autoReconnectIntervalSec{3.0f};

        // --- LOD ---
        float tier1DistanceMeters{5.0f};
        float tier2DistanceMeters{15.0f};

        // --- Debug ---
        bool debugGazeRays{false};
        int logLevel{2};

    private:
        ConfigManager() = default;

        bool _loaded{false};
    };

} // namespace TrueGaze::Engine
