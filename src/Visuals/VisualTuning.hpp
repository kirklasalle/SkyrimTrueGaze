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
        bool gazeRaysEnabled{true};

        /// 0 = Both (light emitter + beam geometry)
        /// 1 = LightOnly (bare-bones; NiPointLight emitters only, no art required)
        /// 2 = GeometryOnly (suppress the light emitters)
        /// Default 0: both light emitters and beam geometry are rendered so the
        /// gaze laser is visually apparent. Mode 1 (light-only) creates only
        /// invisible point-glow lights that are lost in ambient Skyrim lighting.
        int rayRenderMode{0};

        // --- Beam appearance ---
        float gazeRayLengthMeters{2.5f};  // beam reach in metres (~175 Skyrim units)
        float gazeRayThicknessCm{0.2f};   // beam cross-section diameter; ~2mm hair-thin laser
        uint32_t gazeRayColour{0xC9A86A}; // 0xRRGGBB, TrueGaze gold
        float gazeRayOpacity{0.85f};

        /// Resource path of the visible beam model, relative to the Skyrim Data root.
        /// PRIMARY (Kirk directive 2026-10-01): the vanilla editor marker arrow —
        /// a PROVEN in-engine mesh that has rendered correctly every time it was
        /// used. Authored along +Y (handled by AlignBeamOrientation's standard
        /// forward path).
        const char* beamModelPath{"meshes\\marker_arrow.nif"};
        /// Fallback: branded laser beam (SSE-correct rewrite 2026-10-01).
        const char* beamModelFallbackPath{"meshes\\TrueGaze\\GazeBeam.nif"};

        // --- Which actors emit ---
        bool gazeRaysOnPlayer{true};
        bool gazeRaysOnNPCs{true};
        bool gazeRaysOnCreatures{true};

        // --- Attachment / origin ---
        bool gazeRaysAttachHead{true};
        bool gazeRaysTerminus{true};
        float pupilForwardOffsetCm{12.0f}; // ~12cm forward from head bone to eye socket
        float pupilUpOffsetCm{6.0f};       // ~6cm up from head bone to eye socket
        float pupilGlowIntensity{0.35f};

        // --- HCEP Floating Diagram Panel ---
        bool showHcepPanel{true};       // master switch for the HCEP diagram panel
        bool hcepPanelAllActors{true};  // true = Player + NPCs; false = Player only
        // Panel quad is authored 1 unit tall; scale 25 -> ~36 cm tall diagram,
        // readable at the ~35 cm forward offset. (Old default 5 = 7 cm: unreadable.)
        float hcepPanelScale{25.0f};           // panel scale factor in Skyrim units
        float hcepPanelForwardOffsetCm{70.0f}; // ~49.0 Skyrim units forward from head bone
        const char* hcepPanelModelPath{"meshes\\TrueGaze\\GazeRegionPanel.nif"};


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

        /// Beam cross-section radius in Skyrim units (diameter -> radius).
        [[nodiscard]] constexpr float ThicknessUnits() const noexcept
        {
            return (gazeRayThicknessCm * 0.5f / kCmPerMeter) * kUnitsPerMeter;
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

        /// HCEP panel forward offset in Skyrim units.
        [[nodiscard]] constexpr float HcepPanelForwardOffsetUnits() const noexcept
        {
            return hcepPanelForwardOffsetCm / kCmPerMeter * kUnitsPerMeter;
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