#pragma once

#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace TrueGaze::ExpandedPlay
{
    enum class ReticleGlyphShape : uint8_t
    {
        Circle = 0,
        Diamond = 1,
        Triangle = 2,
        Square = 3
    };

    /// @brief Maps HCEP-02 region ID onto RGB hex color with color-blind-safe palettes.
    /// Mode 0 = canonical HCEP-02 colors; 1 = Deuteranopia; 2 = Protanopia; 3 = Tritanopia.
    [[nodiscard]] constexpr uint32_t RegionColorRgb(uint8_t regionId,
                                                    int colorBlindMode = 0) noexcept
    {
        if (colorBlindMode == 1) // Deuteranopia-safe (Okabe-Ito inspired)
        {
            switch (regionId)
            {
            case 0:
                return 0x009E73u; // LeftEye: bluish green
            case 1:
                return 0xE69F00u; // RightEye: orange-gold
            case 2:
                return 0xCC79A7u; // Mouth: reddish purple
            case 3:
                return 0x56B4E9u; // Forehead: sky blue
            case 4:
                return 0xF0E442u; // Chin: bright yellow
            case 5:
                return 0xD55E00u; // Torso: vermilion
            case 8:
                return 0x0072B2u; // Ground: deep blue
            case 9:
                return 0x88CCEEu; // ULPeripheral
            case 10:
                return 0xAAEEFFu; // URPeripheral
            case 11:
                return 0xB8C2D4u; // LLPeripheral
            case 12:
                return 0x8B95A8u; // LRPeripheral
            default:
                return 0xE6D8B8u;
            }
        }
        if (colorBlindMode == 2) // Protanopia-safe
        {
            switch (regionId)
            {
            case 0:
                return 0x0072B2u; // LeftEye: blue
            case 1:
                return 0xF0E442u; // RightEye: yellow
            case 2:
                return 0x56B4E9u; // Mouth: sky
            case 3:
                return 0xCC79A7u; // Forehead: magenta-rose
            case 4:
                return 0xE69F00u; // Chin: amber
            case 5:
                return 0xD55E00u; // Torso: strong orange
            case 8:
                return 0x332288u; // Ground: indigo
            case 9:
                return 0x88CCEEu;
            case 10:
                return 0xBBEEFFu;
            case 11:
                return 0xB8C2D4u;
            case 12:
                return 0x8B95A8u;
            default:
                return 0xE6D8B8u;
            }
        }
        if (colorBlindMode == 3) // Tritanopia-safe
        {
            switch (regionId)
            {
            case 0:
                return 0x00B4A0u; // LeftEye: teal
            case 1:
                return 0xFF6E54u; // RightEye: coral
            case 2:
                return 0xFF99CCu; // Mouth: rose
            case 3:
                return 0xFFFFFFu; // Forehead: bright white
            case 4:
                return 0xFFD0A6u; // Chin: peach
            case 5:
                return 0xE62E4Du; // Torso: crimson
            case 8:
                return 0x3D6D99u; // Ground: slate blue
            case 9:
                return 0x66E0D8u;
            case 10:
                return 0x99F5EEu;
            case 11:
                return 0xB0B0C0u;
            case 12:
                return 0x787888u;
            default:
                return 0xE6D8B8u;
            }
        }

        // Mode 0: Canonical HCEP-02 palette
        switch (regionId)
        {
        case 0:
            return 0x33FF59u; // LeftEye (Green)
        case 1:
            return 0xFF9926u; // RightEye (Orange)
        case 2:
            return 0xFF59BFu; // Mouth (Pink)
        case 3:
            return 0xD966FFu; // Forehead (Purple)
        case 4:
            return 0xE6BF99u; // Chin (Warm Tan)
        case 5:
            return 0xFF2633u; // Torso (Crimson)
        case 8:
            return 0x2659E6u; // Ground (Blue)
        case 9:
            return 0x33CCFFu; // ULPeripheral (Cyan)
        case 10:
            return 0x66F2FFu; // URPeripheral (Bright Cyan)
        case 11:
            return 0xA6B3CCu; // LLPeripheral (Silver)
        case 12:
            return 0x8080A6u; // LRPeripheral (Slate)
        default:
            return 0xE6D8B8u;
        }
    }

    /// @brief Distinct accessibility vector marker shape per HCEP-02 region.
    [[nodiscard]] constexpr ReticleGlyphShape RegionGlyphShape(uint8_t regionId) noexcept
    {
        switch (regionId)
        {
        case 0: // LeftEye
        case 1: // RightEye
            return ReticleGlyphShape::Diamond;
        case 2: // Mouth
        case 4: // Chin
            return ReticleGlyphShape::Circle;
        case 3:  // Forehead
        case 9:  // ULPeripheral
        case 10: // URPeripheral
        case 11: // LLPeripheral
        case 12: // LRPeripheral
            return ReticleGlyphShape::Triangle;
        case 5: // Torso
        case 8: // Ground
            return ReticleGlyphShape::Square;
        default:
            return ReticleGlyphShape::Circle;
        }
    }

    struct DwellArcState
    {
        DwellBand band{DwellBand::None};
        float progress01{0.0f};
    };

    /// @brief Maps dwell duration onto [0..1] arc progress aligned with the 4 threshold ticks.
    [[nodiscard]] inline DwellArcState ComputeDwellArcState(float dwellSec,
                                                            bool rapidEscalation = false) noexcept
    {
        const DwellBand band = ClassifyDwellBand(dwellSec, rapidEscalation);
        if (band == DwellBand::None)
        {
            return {DwellBand::None, 0.0f};
        }

        const float t0 = 0.08f;
        const float t1 = rapidEscalation ? 0.45f : 0.60f;
        const float t2 = rapidEscalation ? 1.50f : 2.00f;
        const float t3 = rapidEscalation ? 3.00f : 4.50f;
        const float t4 = rapidEscalation ? 6.00f : 9.00f;

        if (dwellSec < t1)
        {
            return {band, 0.25f * ((dwellSec - t0) / (t1 - t0))};
        }
        if (dwellSec < t2)
        {
            return {band, 0.25f + 0.25f * ((dwellSec - t1) / (t2 - t1))};
        }
        if (dwellSec < t3)
        {
            return {band, 0.50f + 0.25f * ((dwellSec - t2) / (t3 - t2))};
        }
        if (dwellSec < t4)
        {
            return {band, 0.75f + 0.25f * ((dwellSec - t3) / (t4 - t3))};
        }
        return {DwellBand::Fixation, 1.0f};
    }

    /// @brief Tracks the single soft pulse emitted when mutual eye contact begins.
    struct ReciprocityPulseState
    {
        static constexpr float kDurationSec = 0.45f;

        bool wasMutual{false};
        float remainingSec{0.0f};

        /// @brief Advances pulse state; returns normalized pulse strength in [0..1].
        float Step(bool isMutualNow, float deltaSec) noexcept
        {
            const float dt = (std::isfinite(deltaSec) && deltaSec > 0.0f) ? deltaSec : 0.0f;

            if (isMutualNow && !wasMutual)
            {
                remainingSec = kDurationSec;
            }
            else
            {
                remainingSec = std::max(0.0f, remainingSec - dt);
            }
            wasMutual = isMutualNow;
            return std::clamp(remainingSec / kDurationSec, 0.0f, 1.0f);
        }
    };

    struct MagnetismOffset
    {
        float offsetX{0.0f};
        float offsetY{0.0f};
        bool active{false};
    };

    /// @brief Computes subtle 1st-person reticle attraction inside a 2-degree social-triangle cone.
    [[nodiscard]] inline MagnetismOffset
    ComputeSoftMagnetismOffset(float angleErrorDeg, float dirScreenX, float dirScreenY,
                               float strength01, bool weaponOrCombat) noexcept
    {
        if (weaponOrCombat || !std::isfinite(angleErrorDeg) || !std::isfinite(dirScreenX) ||
            !std::isfinite(dirScreenY) || !std::isfinite(strength01) || angleErrorDeg < 0.0f ||
            angleErrorDeg > 2.0f || strength01 <= 0.0f)
        {
            return {};
        }

        const float lenSq = dirScreenX * dirScreenX + dirScreenY * dirScreenY;
        if (lenSq <= 1e-12f)
        {
            return {};
        }

        const float invLen = 1.0f / std::sqrt(lenSq);
        const float clampedStrength = std::clamp(strength01, 0.0f, 1.0f);
        constexpr float kMaxPullPx = 16.0f;
        const float shiftPx = kMaxPullPx * clampedStrength * (angleErrorDeg / 2.0f);

        return {dirScreenX * invLen * shiftPx, dirScreenY * invLen * shiftPx, true};
    }

    /// @brief WP-11A Persuasion Phase-1 affinity halo color (0=Hate, 1=Dislike, 2=Like, 3=Love).
    [[nodiscard]] constexpr uint32_t PersuasionAffinityColorRgb(uint8_t affinityId,
                                                                bool guardGlance) noexcept
    {
        if (guardGlance)
        {
            return 0xFF5555u;
        }
        switch (affinityId)
        {
        case 3u:
            return 0x5CE68Au; // Love: warm emerald-gold resonance
        case 2u:
            return 0x7FD6C2u; // Like: soft warm teal
        case 1u:
            return 0xC996E6u; // Dislike: muted violet cue to pivot
        case 0u:
        default:
            return 0xFF4D4Du; // Hate: crisp crimson abort cue
        }
    }

    /// @brief Short HUD tag for the active real-time Oblivion persuasion vector.
    [[nodiscard]] constexpr const char* PersuasionVectorTag(uint8_t vectorId) noexcept
    {
        switch (vectorId)
        {
        case 1u:
            return "Admire";
        case 2u:
            return "Joke";
        case 3u:
            return "Boast";
        case 4u:
            return "Coerce";
        case 5u:
            return "Plea";
        case 6u:
            return "Sly Bribe";
        default:
            return "";
        }
    }

    struct ReticleHudSnapshot
    {
        bool enabled{false};
        bool isThirdPerson{false};
        bool showReticle{true};
        bool immersiveHideReticle{false};
        bool targetValid{false};
        uint8_t gazeRegionId{0};
        float dwellSec{0.0f};
        bool rapidEscalation{false};
        bool isMutualGaze{false};
        float opacity{0.22f};
        int colorBlindMode{0};
        float magnetismX{0.0f};
        float magnetismY{0.0f};
        Demeanor activeDemeanor{Demeanor::Courteous};
        DemeanorMode demeanorMode{DemeanorMode::Hybrid};
        bool demeanorPinned{false};
        float demeanorPinRemainingSec{0.0f};
        float frostVignetteAlpha{0.0f};

        // WP-11A Real-Time Persuasion Visual Feedback
        bool enablePersuasionVisuals{true};
        uint8_t persuasionVectorId{0};   // 0=None, 1=Admire..6=SlyBribe
        uint8_t persuasionAffinityId{2}; // 0=Hate, 1=Dislike, 2=Like, 3=Love
        bool persuasionProbeActive{false};
        bool persuasionGuardGlance{false};
        int8_t guardGlanceDirSign{0}; // -1=left, +1=right, 0=unknown
        bool slyBribeArmed{false};
    };

    /// @brief Procedural Scaleform HUD overlay for the 1st-person Gaze Reticle,
    /// 3rd-person Demeanor badge, Persuasion resonance ring, and Telltale-style whispers.
    class GazeReticle
    {
    public:
        static GazeReticle& Get() noexcept;

        void InstallUi() noexcept;
        void Reset() noexcept;
        void UpdateFrame(const ReticleHudSnapshot& snapshot) noexcept;
        void PushWhisper(std::string_view text, uint32_t colorRgb = 0xE6D8B8) noexcept;
        void TriggerAwakeningPulse(uint32_t colorRgb = 0xFFD966u) noexcept;
        void TriggerPersuasionBloom(uint32_t colorRgb = 0xFFD966u,
                                    const char* soundEditorId = nullptr) noexcept;
        void NotifyMilestone(std::string_view text, uint32_t colorRgb = 0xFFD966u,
                             const char* soundEditorId = nullptr,
                             bool triggerPulse = true) noexcept;
    };
} // namespace TrueGaze::ExpandedPlay
