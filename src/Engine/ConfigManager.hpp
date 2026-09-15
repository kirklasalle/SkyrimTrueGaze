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

        /// @brief Saves current configuration values to TrueGaze.ini.
        void Save(const std::string &iniPath = "") noexcept;

        /// @brief True once Load() has completed, whether or not an INI was found.
        [[nodiscard]] bool IsLoaded() const noexcept { return _loaded; }

        /// @brief Clamps every value into its supported range, reporting any change.
        /// Called automatically at the end of Load().
        void Sanitise() noexcept;

        /// @brief Applies every managed key from a single INI on top of the current
        /// in-memory state, using the existing value as each key's default. Enables
        /// Single-pass INI load from Data/SKSE/Plugins/TrueGaze.ini. Does not clamp.
        void ApplyIni(const std::string &iniPath) noexcept;

        // --- General Settings ---
        bool enableTrueGaze{true};
        bool enableCreatures{true};

        // --- Kinematics ---
        float saccadeSpeedMult{1.0f};
        float velocitySaturation{14.0f};
        float microJitterAmp{0.35f};
        float microJitterIntervalMin{0.2f};  // mean time between micro-corrections, lower bound (s)
        float microJitterIntervalMax{0.45f}; // upper bound (s); 1/mean drives OU mean reversion
        float headTrackingSpeed{6.0f};
        float maxComfortEyeAngle{35.0f};

        // --- Skeletal hierarchy strain shares (yaw sums to 1.0; pitch: neck+head) ---
        float spine2YawWeight{0.10f};
        float neckYawWeight{0.25f};
        float neckPitchWeight{0.25f};
        float headYawWeight{0.65f};
        float headPitchWeight{0.75f};

        // --- General ---
        std::string engineTarget{"Auto"}; // Auto | SE | AE | VR (validated at load)

        // --- Social ---
        bool enableGazeAversion{true};
        bool enableSocialTriangle{true};
        float triangleFixationDuration{0.35f};
        float mutualGazeThreshold{2.0f};

        // --- Crosshair sweet spot (player gaze) ---
        bool enableCrosshairGaze{true};
        float crosshairToleranceDeg{4.0f};
        float crosshairMaxRangeMeters{25.0f};
        float crosshairPointBlankMeters{1.5f};

        // --- Bridge ---
        bool connectHcepBridge{true};
        std::string pipeName{R"(\\.\pipe\TrueGazeBridge)"};
        float autoReconnectIntervalSec{3.0f};
        // NOTE: bLockFreeTelemetry was removed. The triple-buffered IPC is the only
        // implementation; there is no lock-based fallback to switch to, so the key
        // was a no-op. See CHANGELOG 2026-09-14.

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
