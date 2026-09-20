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

        /// @brief Absolute path of the INI that actually drove the last Load(), or
        /// empty when compiled defaults were used. Phase S1 evidence baseline: lets a
        /// log reader confirm which configuration governed a session.
        [[nodiscard]] const std::string &LoadedPath() const noexcept { return _loadedPath; }

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

        // --- Visuals (in-game 3D representation of the solved gaze) ---
        //
        // These keys drive src/Visuals. The whole subsystem is off by default and
        // developer-oriented: nothing here may affect the simulation, the save game,
        // or a shipped build's appearance unless explicitly enabled.
        bool enableInGameVisuals{false};  // master switch for every in-game visual
        bool gazeRaysEnabled{false};      // laser-eye beams from the pupil
        int rayRenderMode{0};             // 0=Both(branded), 1=LightOnly(bare-bones), 2=GeometryOnly(branded)
        float gazeRayLengthMeters{10.0f}; // beam length; drives the light radius
        int gazeRayColour{0xC9A86A};      // 0xRRGGBB TrueGaze gold (alpha is separate)
        float gazeRayOpacity{0.85f};      // 0..1 emitter brightness
        bool gazeRaysOnPlayer{true};
        bool gazeRaysOnNPCs{true};
        bool gazeRaysOnCreatures{true};
        bool gazeRaysAttachHead{true};    // attach emitters under the head bone (vs actor root)
        bool gazeRaysTerminus{false};     // emit a second glow at the gaze terminus
        float pupilForwardOffsetCm{7.0f}; // pupil origin, forward from the head bone origin
        float pupilUpOffsetCm{1.5f};      // pupil origin, up from the head bone origin
        float pupilGlowIntensity{0.5f};   // pupil emitter brightness multiplier

        // --- Console commands (~) ---
        //
        // Registers the tg* commands so the game's own console can toggle TrueGaze
        // at runtime. Uses only the vanilla console: no Papyrus, no ESP, no MCM.
        //
        // DEFAULT OFF. Registration reclaims entries the engine already treats as dead
        // or empty, so it needs no count and cannot displace a working command - but it
        // does write into engine memory and has not yet been confirmed in a running
        // game. See src/Integrations/ConsoleCommands.cpp.
        bool enableConsoleCommands{false};

    private:
        ConfigManager() = default;

        bool _loaded{false};
        std::string _loadedPath{};
    };

} // namespace TrueGaze::Engine
