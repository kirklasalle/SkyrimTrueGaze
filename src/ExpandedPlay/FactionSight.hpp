#pragma once

#include "ExpandedPlay/AwakeningRitual.hpp"
#include "ExpandedPlay/CombatAndCreatureSight.hpp"
#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace TrueGaze::ExpandedPlay
{
    /// @brief Bitmask of major Skyrim faction memberships & supernatural states (WP-11C).
    namespace FactionBits
    {
        inline constexpr uint32_t kNone             = 0u;
        inline constexpr uint32_t kDarkBrotherhood  = 1u << 0;
        inline constexpr uint32_t kThievesGuild     = 1u << 1;
        inline constexpr uint32_t kCompanions       = 1u << 2;
        inline constexpr uint32_t kCompanionsCircle = 1u << 3;
        inline constexpr uint32_t kCollegeWinterhold = 1u << 4;
        inline constexpr uint32_t kImperialLegion   = 1u << 5;
        inline constexpr uint32_t kStormcloaks      = 1u << 6;
        inline constexpr uint32_t kBardsCollege     = 1u << 7;
        inline constexpr uint32_t kVampire          = 1u << 8;
        inline constexpr uint32_t kDawnguard        = 1u << 9;
        inline constexpr uint32_t kVigilant         = 1u << 10;
    } // namespace FactionBits

    /// @brief Verified base Skyrim.esm Faction / Keyword FormIDs (zero DLC hard masters).
    namespace VanillaFactionIds
    {
        inline constexpr uint32_t kDarkBrotherhood      = 0x0001BDB3u;
        inline constexpr uint32_t kThievesGuild         = 0x00029DA9u;
        inline constexpr uint32_t kCompanions           = 0x00048362u;
        inline constexpr uint32_t kCompanionsCircle     = 0x000FF234u;
        inline constexpr uint32_t kPlayerWerewolf       = 0x00091822u;
        inline constexpr uint32_t kCollegeOfWinterhold  = 0x0001F259u;
        inline constexpr uint32_t kCWImperial           = 0x0002BF9Au;
        inline constexpr uint32_t kCWImperialNpc        = 0x0001C9FCu;
        inline constexpr uint32_t kCWSons               = 0x0002BF9Bu;
        inline constexpr uint32_t kBardsCollege         = 0x000C13C7u;
        inline constexpr uint32_t kBardsCollegeQuest    = 0x00053511u; // MS05
        inline constexpr uint32_t kVampirePC            = 0x000C4DE0u;
        inline constexpr uint32_t kVampireFaction       = 0x00027242u;
        inline constexpr uint32_t kVampireKeyword       = 0x000A82BBu;
        inline constexpr uint32_t kUndeadKeyword        = 0x00013796u;
        inline constexpr uint32_t kVigilantOfStendarr   = 0x000B3292u;
        inline constexpr uint32_t kJobPriest            = 0x0005159Au;
        inline constexpr uint32_t kJobInnkeeper         = 0x0005091Bu;
        inline constexpr uint32_t kJobMerchant          = 0x00051596u;
    } // namespace VanillaFactionIds

    struct FactionSightInput
    {
        uint32_t playerFactions{FactionBits::kNone};
        uint32_t observerFactions{FactionBits::kNone};
        AwakeningStage stage{AwakeningStage::Dormant};

        bool wearingDarkBrotherhoodGear{false};
        bool wearingThievesGuildGear{false};
        bool wearingCollegeRobes{false};
        bool wearingImperialArmor{false};
        bool wearingStormcloakArmor{false};
        bool wearingDawnguardOrVigilantGear{false};

        bool observerIsGuard{false};
        bool observerIsMerchant{false};
        bool observerIsPriestOrVigilant{false};
        bool observerIsUndeadOrVampire{false};
        bool observerIsNordTraditionalist{false};
        bool observerIsChild{false};

        bool inJarlCourtOrShop{false};
        bool isDaylight{true};
        bool playerSneaking{false};
        bool inCombat{false};
        uint8_t regionId{Region::LeftEye};
        Demeanor activeDemeanor{Demeanor::Courteous};
        float dwellSec{0.0f};
        float distanceMeters{3.0f};
        float playerDominanceAxis{0.0f}; // -100..+100 Signature dominance
    };

    struct FactionSightOutcome
    {
        TacticalSightBuffs tacticalBuffs{};
        Impressions socialDelta{};

        float guardGlanceRangeMeters{12.0f};
        float bribeCostMultiplier{1.0f};
        float barterSpeechMultiplier{1.0f};
        float coerceAuthorityMultiplier{1.0f};
        float bookStudyRateMultiplier{1.0f};
        float meleeCombatSightMultiplier{1.0f};
        float arcaneCombatSightMultiplier{1.0f};
        float vampireBlinkScale{1.0f};

        bool maskedByCivilianDemeanor{false};
        bool drawbackTriggered{false};
        bool strengthTriggered{false};
        bool suggestVoiceScrutiny{false};
        std::string whisper{};
        uint32_t whisperColorRgb{0xE6D8B8u};
    };

    /// @brief True when the player adopts a calm non-threatening social mask in civilian attire.
    [[nodiscard]] constexpr bool IsCalmSocialMask(Demeanor demeanor, bool wearingFactionUniform) noexcept
    {
        if (wearingFactionUniform)
        {
            return false;
        }
        return demeanor == Demeanor::Courteous || demeanor == Demeanor::Deferential ||
               demeanor == Demeanor::Warm;
    }

    /// @brief Pure SDK-free Faction Sight evaluator covering all 7 major Skyrim faction groups
    /// with both build-defining Sight strengths and authentic social drawbacks (WP-11C).
    [[nodiscard]] inline FactionSightOutcome EvaluateFactionSight(
        const FactionSightInput& in, std::string_view actorName = "They")
    {
        FactionSightOutcome out{};
        if (in.playerFactions == FactionBits::kNone || in.stage == AwakeningStage::Dormant ||
            in.observerIsChild)
        {
            return out;
        }

        const std::string_view who = actorName.empty() ? "They" : actorName;
        const bool isEyes = (in.regionId == Region::LeftEye || in.regionId == Region::RightEye);
        const bool isHandsOrBelt =
            (in.regionId == Region::LeftHand || in.regionId == Region::RightHand);
        const bool isTorso = (in.regionId == Region::Torso);

        // Passive faction tuning scalars (always active once Stage 1+ & faction member)
        if ((in.playerFactions & FactionBits::kThievesGuild) != 0u)
        {
            out.guardGlanceRangeMeters = 18.0f;
            out.bribeCostMultiplier = 0.80f;
        }
        if ((in.playerFactions & FactionBits::kCollegeWinterhold) != 0u)
        {
            out.bookStudyRateMultiplier = 1.20f;
            out.arcaneCombatSightMultiplier = 1.35f;
        }
        if ((in.playerFactions & FactionBits::kCompanions) != 0u)
        {
            out.meleeCombatSightMultiplier = 1.25f;
        }
        if ((in.playerFactions & FactionBits::kBardsCollege) != 0u)
        {
            out.barterSpeechMultiplier = 1.25f;
            out.coerceAuthorityMultiplier = 0.75f;
        }
        if ((in.playerFactions & (FactionBits::kImperialLegion | FactionBits::kStormcloaks)) != 0u &&
            in.activeDemeanor == Demeanor::Stern)
        {
            out.coerceAuthorityMultiplier = std::max(out.coerceAuthorityMultiplier, 1.20f);
        }
        if ((in.playerFactions & FactionBits::kVampire) != 0u)
        {
            out.vampireBlinkScale = 0.55f; // Eerie unblinking cadence
        }
        if ((in.playerFactions & (FactionBits::kDawnguard | FactionBits::kVigilant)) != 0u &&
            in.observerIsUndeadOrVampire)
        {
            out.arcaneCombatSightMultiplier = std::max(out.arcaneCombatSightMultiplier, 1.40f);
            out.meleeCombatSightMultiplier = std::max(out.meleeCombatSightMultiplier, 1.20f);
        }

        // --- 1. Dark Brotherhood: Mark Study Poise vs. Citizen Dread ---
        if ((in.playerFactions & FactionBits::kDarkBrotherhood) != 0u)
        {
            const bool dbPeer = (in.observerFactions & FactionBits::kDarkBrotherhood) != 0u;
            const bool masked = IsCalmSocialMask(in.activeDemeanor, in.wearingDarkBrotherhoodGear);
            if (masked)
            {
                out.maskedByCivilianDemeanor = true;
            }

            if (in.playerSneaking && !in.inCombat && (isTorso || isEyes) && in.dwellSec >= 1.8f &&
                in.distanceMeters <= 12.0f && !dbPeer)
            {
                out.tacticalBuffs.sneakBonus = std::max(out.tacticalBuffs.sneakBonus, 10.0f);
                out.tacticalBuffs.offenseBonus = std::max(out.tacticalBuffs.offenseBonus, 8.0f);
                out.strengthTriggered = true;
                out.whisper = std::format("Listener's Eye marks {}'s opening.", who);
                out.whisperColorRgb = 0xCC6688u;
            }
            else if (dbPeer && isEyes && in.dwellSec >= 1.5f &&
                     (in.activeDemeanor == Demeanor::Predatory ||
                      in.activeDemeanor == Demeanor::Stern))
            {
                out.socialDelta.respect += 1.2f;
                out.strengthTriggered = true;
                out.whisper = std::format("{} respects your cold stillness.", who);
                out.whisperColorRgb = 0xCC99AAu;
            }
            else if (!dbPeer && !in.playerSneaking && !in.inCombat && !masked &&
                     (in.wearingDarkBrotherhoodGear || in.playerDominanceAxis >= 45.0f) &&
                     isEyes && in.dwellSec >= 1.6f && in.distanceMeters <= 6.0f)
            {
                out.socialDelta.unease += 2.2f;
                out.socialDelta.fear += 1.8f;
                if (in.observerIsGuard)
                {
                    out.socialDelta.suspicion += 2.5f;
                }
                out.drawbackTriggered = true;
                out.suggestVoiceScrutiny = true;
                out.whisper = in.observerIsGuard
                                  ? std::format("{} narrows their eyes at your dark attire.", who)
                                  : std::format("{} shrinks back from the chill in your gaze.", who);
                out.whisperColorRgb = 0xFF7766u;
            }
        }

        // --- 2. Thieves Guild: Belt/Hand Sizing-Up Poise vs. Court/Shop Scrutiny ---
        if ((in.playerFactions & FactionBits::kThievesGuild) != 0u && !in.inCombat)
        {
            const bool tgPeer = (in.observerFactions & FactionBits::kThievesGuild) != 0u;
            const bool masked = IsCalmSocialMask(in.activeDemeanor, in.wearingThievesGuildGear);
            if (!tgPeer && isHandsOrBelt && in.dwellSec >= 0.7f && in.distanceMeters <= 4.5f)
            {
                out.tacticalBuffs.pickpocketBonus =
                    std::max(out.tacticalBuffs.pickpocketBonus, 12.0f);
                out.strengthTriggered = true;
                if (out.whisper.empty())
                {
                    out.whisper = std::format("Shadowmark Eye gauges {}'s pockets.", who);
                    out.whisperColorRgb = 0xFFD966u;
                }
            }
            if (!tgPeer && !masked && in.wearingThievesGuildGear &&
                (in.observerIsGuard || in.observerIsMerchant || in.inJarlCourtOrShop) &&
                (isHandsOrBelt || isEyes) && in.dwellSec >= 1.8f && in.distanceMeters <= 6.0f)
            {
                out.socialDelta.suspicion += 2.0f;
                out.socialDelta.unease += 1.2f;
                out.drawbackTriggered = true;
                out.suggestVoiceScrutiny = true;
                if (out.whisper.empty())
                {
                    out.whisper = std::format("{} eyes your Guild leathers warily.", who);
                    out.whisperColorRgb = 0xFFAA66u;
                }
            }
        }

        // --- 3. The Companions & Circle: Shield-Sibling Honor vs. Evasive / Beast-Blood Tell ---
        if ((in.playerFactions & FactionBits::kCompanions) != 0u && !in.inCombat)
        {
            const bool compPeer = (in.observerFactions & FactionBits::kCompanions) != 0u;
            if (compPeer && isEyes && in.dwellSec >= 1.5f)
            {
                if (in.activeDemeanor == Demeanor::Evasive)
                {
                    out.socialDelta.respect -= 1.2f;
                    out.socialDelta.unease += 1.0f;
                    out.drawbackTriggered = true;
                    if (out.whisper.empty())
                    {
                        out.whisper =
                            std::format("{} frowns at your shifting eyes—meet a Companion square.", who);
                        out.whisperColorRgb = 0xFF9966u;
                    }
                }
                else
                {
                    out.socialDelta.respect += 1.4f;
                    out.strengthTriggered = true;
                }
            }
            if ((in.playerFactions & FactionBits::kCompanionsCircle) != 0u && !compPeer &&
                isEyes && in.dwellSec >= 3.2f && in.distanceMeters <= 4.0f &&
                in.activeDemeanor != Demeanor::Deferential)
            {
                out.socialDelta.unease += 1.2f;
                out.drawbackTriggered = true;
                if (out.whisper.empty())
                {
                    out.whisper = std::format("{} senses something wild behind your eyes.", who);
                    out.whisperColorRgb = 0xFFB866u;
                }
            }
        }

        // --- 4. College of Winterhold: Arcane Ward vs. Nord Skepticism ---
        if ((in.playerFactions & FactionBits::kCollegeWinterhold) != 0u)
        {
            const bool collegePeer =
                (in.observerFactions & FactionBits::kCollegeWinterhold) != 0u;
            if (in.inCombat && in.dwellSec >= 0.4f)
            {
                out.tacticalBuffs.magickaRegenBonus =
                    std::max(out.tacticalBuffs.magickaRegenBonus, 15.0f);
                out.strengthTriggered = true;
            }
            else if (!collegePeer && in.wearingCollegeRobes && in.observerIsNordTraditionalist &&
                     isEyes && in.dwellSec >= 2.2f && in.distanceMeters <= 5.5f &&
                     in.activeDemeanor != Demeanor::Warm &&
                     in.activeDemeanor != Demeanor::Deferential)
            {
                out.socialDelta.unease += 1.3f;
                out.socialDelta.suspicion += 1.0f;
                out.drawbackTriggered = true;
                if (out.whisper.empty())
                {
                    out.whisper = std::format("{} eyes your Winterhold robes with Nord distrust.", who);
                    out.whisperColorRgb = 0xA6C8FFu;
                }
            }
        }

        // --- 5. Civil War (Imperial Legion vs. Stormcloaks) ---
        const bool playerImp = (in.playerFactions & FactionBits::kImperialLegion) != 0u;
        const bool playerSc  = (in.playerFactions & FactionBits::kStormcloaks) != 0u;
        const bool obsImp    = (in.observerFactions & FactionBits::kImperialLegion) != 0u;
        const bool obsSc     = (in.observerFactions & FactionBits::kStormcloaks) != 0u;
        if (!in.inCombat && isEyes && in.dwellSec >= 1.5f && in.distanceMeters <= 6.5f)
        {
            if ((playerImp && obsImp) || (playerSc && obsSc))
            {
                out.socialDelta.respect += 1.2f;
                out.strengthTriggered = true;
            }
            else if ((playerImp && obsSc && in.wearingImperialArmor) ||
                     (playerSc && obsImp && in.wearingStormcloakArmor))
            {
                out.socialDelta.suspicion += 2.4f;
                out.socialDelta.unease += 1.8f;
                out.barterSpeechMultiplier *= 0.85f;
                out.drawbackTriggered = true;
                out.suggestVoiceScrutiny = true;
                if (out.whisper.empty())
                {
                    out.whisper = std::format("{} glares coldly at your rival uniform.", who);
                    out.whisperColorRgb = 0xFF6666u;
                }
            }
        }

        // --- 6. Vampires vs. Vigilants / Priests / Dawnguard ---
        if ((in.playerFactions & FactionBits::kVampire) != 0u && !in.inCombat && isEyes &&
            in.dwellSec >= 1.6f && in.distanceMeters <= 5.5f)
        {
            const bool hunter =
                in.observerIsPriestOrVigilant ||
                (in.observerFactions & (FactionBits::kVigilant | FactionBits::kDawnguard)) != 0u;
            const bool masked =
                (in.activeDemeanor == Demeanor::Evasive || in.activeDemeanor == Demeanor::Deferential);
            if (hunter && in.isDaylight && !masked)
            {
                out.socialDelta.suspicion += 3.0f;
                out.socialDelta.unease += 2.0f;
                out.drawbackTriggered = true;
                out.suggestVoiceScrutiny = true;
                out.whisper = std::format("{} studies your pale, unblinking eyes in the sun!", who);
                out.whisperColorRgb = 0xFF5555u;
            }
            else if (!hunter && !in.isDaylight &&
                     (in.activeDemeanor == Demeanor::Warm ||
                      in.activeDemeanor == Demeanor::Predatory))
            {
                out.socialDelta.attraction += 1.5f;
                out.strengthTriggered = true;
            }
        }

        // --- 7. Dawnguard / Vigilant Scrutiny ---
        if ((in.playerFactions & (FactionBits::kDawnguard | FactionBits::kVigilant)) != 0u &&
            in.observerIsUndeadOrVampire && in.dwellSec >= 0.8f && in.distanceMeters <= 10.0f)
        {
            out.tacticalBuffs.resistMagicBonus =
                std::max(out.tacticalBuffs.resistMagicBonus, 12.0f);
            out.tacticalBuffs.armorBonus = std::max(out.tacticalBuffs.armorBonus, 15.0f);
            out.strengthTriggered = true;
            if (out.whisper.empty())
            {
                out.whisper = std::format("Hunter's Sight pierces {}'s dark aura.", who);
                out.whisperColorRgb = 0xFFD966u;
            }
        }

        return out;
    }
} // namespace TrueGaze::ExpandedPlay
