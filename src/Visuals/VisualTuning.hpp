#pragma once

#include <cstdint>

namespace TrueGaze::Visuals
{

    /// @brief Immutable per-frame snapshot of the in-game visual configuration.
    ///
    /// Mirrors the role `Engine::GazeTuning` plays for the runtime engine: the renderer
    /// takes its constants as arguments rather than reading `ConfigManager` globals,
    /// so a key in `TrueGaze.ini` has exactly one path to the visuals.
    ///
    /// Like `GazeTuning`, this is rebuilt only when configuration changes
    /// (`GazeEngine::RefreshTuning`), never per frame.
    struct VisualTuning
    {
        // --- Master switches ---
        bool enableInGameVisuals{false};
        bool gazeRaysEnabled{false};

        /// 0 = Both (light emitter + branded geometry, when available)
        /// 1 = LightOnly (bare-bones; NiPointLight emitters only, no art required)
        /// 2 = GeometryOnly (suppress the light emitters)
        int rayRenderMode{0};

        // --- Beam appearance ---
        float gazeRayLengthMeters{10.0f};
        uint32_t gazeRayColour{0xC9A86A}; // 0xRRGGBB, TrueGaze gold
        float gazeRayOpacity{0.85f};

        /// Resource path of the visible beam model, relative to the Skyrim Data
        /// root. Configurable so a standalone non-Bethesda asset can be used without
        /// a rebuild. Primary target is the standalone TrueGaze mesh path.
        const char *beamModelPath{"meshes\\TrueGaze\\GazeBeam.nif"};
        /// Verified secondary vanilla fallback asset in the Dawnguard effects folder.
        const char *beamModelFallbackPath{"meshes\\dlc01\\effects\\fxsoulcairnbeam.nif"};

        // --- Which actors emit ---
        bool gazeRaysOnPlayer{true};
        bool gazeRaysOnNPCs{true};
        bool gazeRaysOnCreatures{true};

        // --- Attachment / origin ---
        bool gazeRaysAttachHead{true};
        bool gazeRaysTerminus{false};
        float pupilForwardOffsetCm{7.0f};
        float pupilUpOffsetCm{1.5f};
        float pupilGlowIntensity{0.5f};

        // --- Skyrim world constants ---
        static constexpr float kUnitsPerMeter = 70.0f;
        static constexpr float kCmPerMeter = 100.0f;

        /// True when the light-emitting path (NiPointLight) should run.
        [[nodiscard]] constexpr bool UseLightEmitters() const noexcept
        {
            return rayRenderMode == 0 || rayRenderMode == 1;
        }

        /// True when the branded geometry path should run. Degrades to nothing
        /// until the beam NIF assets exist (see VisualEffectsManager).
        [[nodiscard]] constexpr bool UseGeometry() const noexcept
        {
            return rayRenderMode == 0 || rayRenderMode == 2;
        }

        /// Beam length in Skyrim units.
        [[nodiscard]] constexpr float LengthUnits() const noexcept
        {
            return gazeRayLengthMeters * kUnitsPerMeter;
        }

        /// Pupil forward offset in Skyrim units.
        [[nodiscard]] constexpr float ForwardOffsetUnits() const noexcept
        {
            return pupilForwardOffsetCm / kCmPerMeter * kUnitsPerMeter;
        }

        /// Pupil up offset in Skyrim units.
        [[nodiscard]] constexpr float UpOffsetUnits() const noexcept
        {
            return pupilUpOffsetCm / kCmPerMeter * kUnitsPerMeter;
        }

        /// Normalised red channel, 0..1.
        [[nodiscard]] constexpr float ColourR() const noexcept
        {
            return static_cast<float>((gazeRayColour >> 16) & 0xFF) / 255.0f;
        }

        /// Normalised green channel, 0..1.
        [[nodiscard]] constexpr float ColourG() const noexcept
        {
            return static_cast<float>((gazeRayColour >> 8) & 0xFF) / 255.0f;
        }

        /// Normalised blue channel, 0..1.
        [[nodiscard]] constexpr float ColourB() const noexcept
        {
            return static_cast<float>(gazeRayColour & 0xFF) / 255.0f;
        }
    };

} // namespace TrueGaze::Visuals