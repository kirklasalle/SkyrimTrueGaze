#pragma once

#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace TrueGaze::ExpandedPlay
{
    /// HCEP-02 region IDs (mirror of Engine::GazeRegion; kept local so this header stays
    /// dependency-free and testable in standalone builds).
    namespace Region
    {
        inline constexpr uint8_t LeftEye   = 0;
        inline constexpr uint8_t RightEye  = 1;
        inline constexpr uint8_t Mouth     = 2;
        inline constexpr uint8_t Forehead  = 3;
        inline constexpr uint8_t Chin      = 4;
        inline constexpr uint8_t Torso     = 5;
        inline constexpr uint8_t RightHand = 6;
        inline constexpr uint8_t LeftHand  = 7;
        inline constexpr uint8_t Ground    = 8;
        inline constexpr uint8_t ULPeriph  = 9;
        inline constexpr uint8_t URPeriph  = 10;
        inline constexpr uint8_t LLPeriph  = 11;
        inline constexpr uint8_t LRPeriph  = 12;
    } // namespace Region

    /// Lore-flavoured observer culture used for cultural multipliers (§11.1).
    enum class ObserverCulture : uint8_t
    {
        Generic  = 0,
        Nord     = 1,
        Imperial = 2,
        Breton   = 3,
        Redguard = 4,
        Altmer   = 5,
        Bosmer   = 6,
        Dunmer   = 7,
        Orc      = 8,
        Khajiit  = 9,
        Argonian = 10
    };

    struct InterpretationContext
    {
        bool isChild{false};
        bool matureReactionsEnabled{false};
        RelationshipTier tier{RelationshipTier::Stranger};
        ObserverCulture culture{ObserverCulture::Generic};

        bool isMutual{false};               // observer is looking back
        bool firstMutualWithHistory{false}; // first mutual gaze this session with a known NPC
        bool compassionContext{false};      // observer wounded, grieving, or begging
        bool emblemContext{false};          // guild emblem / shared faction insignia on torso
        bool playerWeaponDrawn{false};
        bool playerIsThane{false};
        bool playerMakingClaim{false};      // speaking a persuade / lie line right now
        bool regionSpecific{true};          // false beyond 8 m (from PerceptionGate)

        float perceptionIntensity{1.0f};    // from PerceptionGate::Evaluate
        float facialBondScale{1.0f};        // from PerceptionGate::Evaluate
        float demeanorEfficacy{1.0f};       // from BuildDemeanorProfile
    };

    struct InterpretationResult
    {
        Interpretation interpretation{Interpretation::None};
        float strength{0.0f}; // [0..1]
        DwellBand band{DwellBand::None};
        bool allowSocialBump{false}; // may feed ledger / progression
    };

    [[nodiscard]] constexpr bool IsFaceRegion(uint8_t region) noexcept
    {
        return region == Region::LeftEye || region == Region::RightEye ||
               region == Region::Forehead || region == Region::Chin;
    }

    [[nodiscard]] constexpr bool IsBodyRegion(uint8_t region) noexcept
    {
        return region == Region::Torso || region == Region::Mouth || region == Region::Chin ||
               region == Region::RightHand || region == Region::LeftHand;
    }

    [[nodiscard]] constexpr bool IsIntimateTier(RelationshipTier tier) noexcept
    {
        return tier == RelationshipTier::Spouse || tier == RelationshipTier::Partner;
    }

    [[nodiscard]] constexpr bool IsCloseTier(RelationshipTier tier) noexcept
    {
        return tier == RelationshipTier::Friend || tier == RelationshipTier::Confidant ||
               tier == RelationshipTier::Follower || IsIntimateTier(tier);
    }

    [[nodiscard]] constexpr bool IsHostileTier(RelationshipTier tier) noexcept
    {
        return tier == RelationshipTier::Rival || tier == RelationshipTier::Enemy;
    }

    [[nodiscard]] constexpr float BandBaseStrength(DwellBand band) noexcept
    {
        switch (band)
        {
        case DwellBand::Glance: return 0.20f;
        case DwellBand::Look: return 0.40f;
        case DwellBand::Gaze: return 0.60f;
        case DwellBand::Stare: return 0.80f;
        case DwellBand::Fixation: return 1.00f;
        case DwellBand::None:
        default: return 0.0f;
        }
    }

    /// @brief Deterministic region x dwell x relationship x context -> meaning evaluator.
    class InterpretationEngine
    {
    public:
        [[nodiscard]] static InterpretationResult EvaluateLook(uint8_t region, float dwellSec,
                                                               const InterpretationContext& ctx) noexcept
        {
            // ---- HARD SAFETY GATE (§11.3) — runs before any branch ----------------
            // Children: body / mouth / chin regions never produce any interpretation.
            if (ctx.isChild && IsBodyRegion(region))
            {
                return {Interpretation::None, 0.0f, ClassifyDwellBand(dwellSec), false};
            }

            if (!ctx.regionSpecific &&
                (region == Region::Torso || region == Region::Mouth ||
                 region == Region::LeftHand || region == Region::RightHand))
            {
                return {Interpretation::None, 0.0f, ClassifyDwellBand(dwellSec), false};
            }

            const bool compassionOrEmblem = ctx.compassionContext || ctx.emblemContext;
            const bool rapid = region == Region::Torso && ctx.matureReactionsEnabled &&
                               !ctx.isChild && !compassionOrEmblem;
            const DwellBand band = ClassifyDwellBand(dwellSec, rapid);

            InterpretationResult result{};
            result.band = band;
            if (band == DwellBand::None)
            {
                return result;
            }

            Interpretation interp = Classify(region, band, ctx);
            float cultural = 1.0f;
            interp = ApplyCulture(interp, region, band, ctx, cultural);

            // ---- HARD SAFETY GATE (structural second guard) -----------------------
            interp = EnforceSafety(interp, ctx);

            result.interpretation = interp;
            if (interp == Interpretation::None)
            {
                return result;
            }

            float strength = BandBaseStrength(band) * cultural * Sanitise01(ctx.perceptionIntensity) *
                             SanitiseEfficacy(ctx.demeanorEfficacy);
            if (interp == Interpretation::Warmth || interp == Interpretation::Attraction ||
                interp == Interpretation::Recognition)
            {
                strength *= Sanitise01(ctx.facialBondScale);
            }
            result.strength = std::clamp(strength, 0.0f, 1.0f);
            result.allowSocialBump = result.strength > 0.05f;
            return result;
        }

        /// @brief Final filter; exposed for tests and any external caller.
        [[nodiscard]] static constexpr Interpretation EnforceSafety(Interpretation interp,
                                                                    const InterpretationContext& ctx) noexcept
        {
            if (ctx.isChild &&
                (interp == Interpretation::Attraction || interp == Interpretation::IntrusionLeer))
            {
                return Interpretation::None;
            }
            if (interp == Interpretation::IntrusionLeer &&
                (!ctx.matureReactionsEnabled || IsIntimateTier(ctx.tier)))
            {
                return Interpretation::None;
            }
            return interp;
        }

    private:
        [[nodiscard]] static float Sanitise01(float v) noexcept
        {
            return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
        }

        [[nodiscard]] static float SanitiseEfficacy(float v) noexcept
        {
            return std::isfinite(v) ? std::clamp(v, 0.0f, 1.0f) : 1.0f;
        }

        [[nodiscard]] static Interpretation Classify(uint8_t region, DwellBand band,
                                                     const InterpretationContext& ctx) noexcept
        {
            const bool atLeastLook = band >= DwellBand::Look;
            const bool atLeastGaze = band >= DwellBand::Gaze;
            const bool atLeastStare = band >= DwellBand::Stare;

            // Peripheral aversions modulate rather than create meaning.
            if (region == Region::LRPeriph)
            {
                if (ctx.playerMakingClaim && atLeastLook)
                {
                    return Interpretation::Deceit;
                }
                return (!ctx.playerMakingClaim && atLeastGaze) ? Interpretation::Suspicion
                                                               : Interpretation::None;
            }
            if (region == Region::LLPeriph)
            {
                return (atLeastLook && (IsHostileTier(ctx.tier) ||
                                        ctx.tier == RelationshipTier::Stranger))
                           ? Interpretation::Contempt
                           : Interpretation::None;
            }
            if (region == Region::ULPeriph || region == Region::URPeriph)
            {
                return Interpretation::None;
            }

            if (region == Region::Ground ||
                (region == Region::Chin && ctx.tier == RelationshipTier::Superior))
            {
                return (atLeastLook && (ctx.tier == RelationshipTier::Superior))
                           ? Interpretation::Deference
                           : Interpretation::None;
            }

            if (region == Region::RightHand || region == Region::LeftHand)
            {
                return atLeastGaze ? Interpretation::Suspicion : Interpretation::None;
            }

            if (region == Region::Torso)
            {
                if (ctx.emblemContext)
                {
                    return atLeastLook ? Interpretation::Recognition : Interpretation::None;
                }
                if (ctx.compassionContext)
                {
                    return Interpretation::Compassion;
                }
                if (IsIntimateTier(ctx.tier))
                {
                    return atLeastGaze ? Interpretation::Attraction : Interpretation::None;
                }
                if (ctx.matureReactionsEnabled && atLeastStare)
                {
                    return Interpretation::IntrusionLeer;
                }
                return Interpretation::None;
            }

            if (region == Region::Mouth)
            {
                if (IsIntimateTier(ctx.tier) && atLeastGaze)
                {
                    return Interpretation::Attraction;
                }
                if (IsCloseTier(ctx.tier) && atLeastGaze)
                {
                    return Interpretation::Warmth;
                }
                if (IsHostileTier(ctx.tier) && atLeastGaze)
                {
                    return Interpretation::Contempt;
                }
                return Interpretation::None;
            }

            // Face regions: eyes, forehead, chin
            if (!IsFaceRegion(region))
            {
                return Interpretation::None;
            }

            if (ctx.compassionContext && atLeastLook)
            {
                return Interpretation::Compassion;
            }
            if (ctx.firstMutualWithHistory && ctx.isMutual && atLeastLook)
            {
                return Interpretation::Recognition;
            }
            if (band == DwellBand::Glance)
            {
                return Interpretation::None; // acknowledgement only
            }

            const bool superiorImpertinence =
                ctx.tier == RelationshipTier::Superior && !ctx.playerIsThane;

            if (band == DwellBand::Look)
            {
                return Interpretation::Respect;
            }

            if (band == DwellBand::Gaze)
            {
                if (superiorImpertinence)
                {
                    return Interpretation::Challenge;
                }
                if (IsHostileTier(ctx.tier))
                {
                    return Interpretation::Challenge;
                }
                if (IsIntimateTier(ctx.tier) && ctx.isMutual)
                {
                    return Interpretation::Attraction;
                }
                if (IsCloseTier(ctx.tier) && ctx.isMutual)
                {
                    return Interpretation::Warmth;
                }
                return Interpretation::Respect;
            }

            // Stare / Fixation
            if (IsIntimateTier(ctx.tier))
            {
                return Interpretation::Attraction; // "You're staring again." (fond)
            }
            if (IsCloseTier(ctx.tier) && !ctx.playerWeaponDrawn)
            {
                return Interpretation::Warmth; // playful "Something on my face?"
            }
            return Interpretation::Challenge;
        }

        [[nodiscard]] static Interpretation ApplyCulture(Interpretation interp, uint8_t region,
                                                         DwellBand band,
                                                         const InterpretationContext& ctx,
                                                         float& outMultiplier) noexcept
        {
            outMultiplier = 1.0f;
            const bool face = IsFaceRegion(region);

            switch (ctx.culture)
            {
            case ObserverCulture::Nord:
                if (face && interp == Interpretation::Respect)
                {
                    outMultiplier = 1.15f; // candour valued
                }
                break;
            case ObserverCulture::Altmer:
                if (face && band >= DwellBand::Gaze && !ctx.playerIsThane &&
                    (interp == Interpretation::Respect))
                {
                    interp = Interpretation::Challenge; // impertinence
                    outMultiplier = 1.10f;
                }
                break;
            case ObserverCulture::Orc:
                if (face && band >= DwellBand::Stare && !IsIntimateTier(ctx.tier))
                {
                    interp = Interpretation::Challenge;
                }
                if (interp == Interpretation::Challenge)
                {
                    outMultiplier = 1.20f;
                }
                break;
            case ObserverCulture::Redguard:
                if (interp == Interpretation::Challenge)
                {
                    outMultiplier = 1.10f;
                }
                break;
            case ObserverCulture::Khajiit:
                if (face && interp == Interpretation::Challenge &&
                    ctx.tier == RelationshipTier::Stranger && !ctx.playerWeaponDrawn)
                {
                    interp = Interpretation::Respect; // long gazes read friendly
                }
                break;
            case ObserverCulture::Dunmer:
                if (interp == Interpretation::Suspicion)
                {
                    outMultiplier = 1.20f;
                }
                break;
            case ObserverCulture::Argonian:
                if (face)
                {
                    outMultiplier = 0.60f; // reads posture more than eyes
                }
                break;
            case ObserverCulture::Breton:
                if (interp == Interpretation::Attraction || interp == Interpretation::Warmth)
                {
                    outMultiplier = 1.10f;
                }
                break;
            case ObserverCulture::Imperial:
            case ObserverCulture::Bosmer:
            case ObserverCulture::Generic:
            default:
                break;
            }
            return interp;
        }
    };
} // namespace TrueGaze::ExpandedPlay
