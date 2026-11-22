#pragma once

#include "ExpandedPlay/AwakeningRitual.hpp"
#include "ExpandedPlay/DemeanorController.hpp"
#include "ExpandedPlay/ExpandedPlayTypes.hpp"
#include "ExpandedPlay/InterpretationEngine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace TrueGaze::ExpandedPlay
{
    /// @brief Semantic classification of what an actor holds in a hand (or has raised).
    enum class HeldItemThreat : uint8_t
    {
        None              = 0, // Empty or sheathed
        UtilityOrPeaceful = 1, // Torch, open Canticle book, Healing/Candlelight/utility spell
        UnarmedFists      = 2, // Bare fists raised
        ShieldOnly        = 3, // Defensive shield / ward
        MeleeWeapon       = 4, // Sword, axe, mace, dagger, greatsword, battleaxe, warhammer
        RangedWeapon      = 5, // Bow, crossbow, offensive staff
        HostileSpell      = 6  // Destruction or hostile spell equipped/charging
    };

    struct ActorHeldPosture
    {
        bool handsRaised{false};
        HeldItemThreat rightHand{HeldItemThreat::None};
        HeldItemThreat leftHand{HeldItemThreat::None};

        [[nodiscard]] constexpr bool IsLethalBrandish() const noexcept
        {
            if (!handsRaised)
            {
                return false;
            }
            const auto isLethal = [](HeldItemThreat t) noexcept
            {
                return t == HeldItemThreat::MeleeWeapon || t == HeldItemThreat::RangedWeapon ||
                       t == HeldItemThreat::HostileSpell;
            };
            return isLethal(rightHand) || isLethal(leftHand);
        }

        [[nodiscard]] constexpr bool IsPeacefulOrUtility() const noexcept
        {
            if (!handsRaised)
            {
                return true;
            }
            return !IsLethalBrandish() && rightHand != HeldItemThreat::UnarmedFists &&
                   leftHand != HeldItemThreat::UnarmedFists;
        }

        [[nodiscard]] constexpr bool HasShield() const noexcept
        {
            return leftHand == HeldItemThreat::ShieldOnly || rightHand == HeldItemThreat::ShieldOnly;
        }

        [[nodiscard]] constexpr bool HasHostileSpell() const noexcept
        {
            return handsRaised && (rightHand == HeldItemThreat::HostileSpell ||
                                   leftHand == HeldItemThreat::HostileSpell);
        }
    };

    /// @brief Reversible transient tactical bonuses granted by Combat Sight & Creature Sight.
    /// Every delta is tracked symmetrically through ModifierLedger and stripped on expiry/save.
    struct TacticalSightBuffs
    {
        float blockBonus{0.0f};
        float resistMagicBonus{0.0f};
        float offenseBonus{0.0f};
        float armorBonus{0.0f};
        float staminaRegenBonus{0.0f};
        float sneakBonus{0.0f};
        float durationSec{0.0f};

        [[nodiscard]] constexpr bool HasAny() const noexcept
        {
            return blockBonus > 0.01f || resistMagicBonus > 0.01f || offenseBonus > 0.01f ||
                   armorBonus > 0.01f || staminaRegenBonus > 0.01f || sneakBonus > 0.01f;
        }
    };

    enum class CombatSightKind : uint8_t
    {
        None             = 0,
        WeaponWindup     = 1,
        SpellCharge      = 2,
        ShieldOpening    = 3,
        FootworkLunge    = 4,
        PreCombatBrandish = 5
    };

    struct CombatSightInput
    {
        AwakeningStage stage{AwakeningStage::Dormant};
        bool targetInCombat{false};
        bool targetHostile{false};
        uint8_t regionId{Region::LeftEye};
        float dwellSec{0.0f};
        float distanceMeters{3.0f};
        ActorHeldPosture targetPosture{};
        bool targetAttacking{false};
        bool targetCasting{false};
        bool targetBlocking{false};
        float targetSpeedUnitsPerSec{0.0f};
        float playerPresenceMax{20.0f};
    };

    struct CombatSightResult
    {
        bool triggered{false};
        CombatSightKind kind{CombatSightKind::None};
        TacticalSightBuffs buffs{};
        std::string whisper{};
        uint32_t colorRgb{0xFFD966u};
    };

    /// @brief Evaluates Stage 2+ Combat Sight when reading an adversary's weapon hand, shield,
    ///        casting hand, or footwork — plus pre-combat brandishing awareness.
    [[nodiscard]] inline CombatSightResult EvaluateCombatSight(
        const CombatSightInput& in, std::string_view targetName)
    {
        CombatSightResult out{};
        if (in.stage < AwakeningStage::SteadySight || !std::isfinite(in.dwellSec) ||
            !std::isfinite(in.distanceMeters) || in.distanceMeters > 14.0f)
        {
            return out;
        }

        const float stageScalar =
            (in.stage >= AwakeningStage::CanticleAwakened)
                ? 1.35f
                : ((in.stage >= AwakeningStage::LivingGlyph) ? 1.10f : 0.70f);
        const std::string_view name = targetName.empty() ? std::string_view("Foe") : targetName;

        // Pre-combat brandishing read (non-combat NPC drawing lethal steel near player)
        if (!in.targetInCombat)
        {
            if (in.targetPosture.IsLethalBrandish() && in.dwellSec >= 0.45f &&
                in.distanceMeters <= 8.0f)
            {
                out.triggered = true;
                out.kind = CombatSightKind::PreCombatBrandish;
                out.buffs.blockBonus = 6.0f * stageScalar;
                out.buffs.durationSec = 3.0f;
                out.colorRgb = 0xFFCC66u;
                out.whisper = std::format(
                    "{}'s hand tightens on steel - hold steady or lower your gaze.", name);
            }
            return out;
        }

        if (!in.targetHostile || in.dwellSec < 0.25f)
        {
            return out;
        }

        const bool lookingAtWeaponSide =
            (in.regionId == Region::RightHand || in.regionId == Region::Torso ||
             in.regionId == Region::LeftEye || in.regionId == Region::RightEye);
        const bool lookingAtShieldSide =
            (in.regionId == Region::LeftHand || in.regionId == Region::Torso);
        const bool lookingAtFeet =
            (in.regionId == Region::Ground || in.regionId == Region::Chin ||
             in.regionId == Region::Torso);

        // 1. Spell charge read (highest priority when caster charges hostile magic)
        if ((in.targetCasting || in.targetPosture.HasHostileSpell()) && lookingAtWeaponSide &&
            in.dwellSec >= 0.30f)
        {
            out.triggered = true;
            out.kind = CombatSightKind::SpellCharge;
            out.buffs.resistMagicBonus = std::clamp(12.0f * stageScalar, 8.0f, 18.0f);
            out.buffs.durationSec = 3.5f;
            out.colorRgb = 0x66F2FFu;
            out.whisper = std::format("Spell cadence read on {} - ward braced!", name);
            return out;
        }

        // 2. Weapon / bow strike wind-up read
        if (in.targetAttacking && lookingAtWeaponSide)
        {
            out.triggered = true;
            out.kind = CombatSightKind::WeaponWindup;
            out.buffs.blockBonus = std::clamp(12.0f * stageScalar, 8.0f, 18.0f);
            out.buffs.armorBonus = std::clamp(24.0f * stageScalar, 15.0f, 36.0f);
            out.buffs.durationSec = 3.5f;
            out.colorRgb = 0xFFD966u;
            out.whisper = std::format("Strike telegraph read on {} - guard braced!", name);
            return out;
        }

        // 3. Shield / off-hand guard rhythm read
        if ((in.targetBlocking || in.targetPosture.HasShield()) && lookingAtShieldSide &&
            in.dwellSec >= 0.40f)
        {
            out.triggered = true;
            out.kind = CombatSightKind::ShieldOpening;
            out.buffs.offenseBonus = std::clamp(10.0f * stageScalar, 6.0f, 15.0f);
            out.buffs.durationSec = 3.5f;
            out.colorRgb = 0xFF9926u;
            out.whisper = std::format("Shield rhythm read on {} - opening spotted!", name);
            return out;
        }

        // 4. Footwork / lunge stride anticipation read
        if (in.targetSpeedUnitsPerSec >= 165.0f && lookingAtFeet && in.dwellSec >= 0.35f)
        {
            out.triggered = true;
            out.kind = CombatSightKind::FootworkLunge;
            out.buffs.armorBonus = std::clamp(32.0f * stageScalar, 20.0f, 50.0f);
            out.buffs.staminaRegenBonus = std::clamp(15.0f * stageScalar, 10.0f, 22.0f);
            out.buffs.durationSec = 3.5f;
            out.colorRgb = 0x33FF59u;
            out.whisper = std::format("Footwork read on {} - stride anticipated!", name);
            return out;
        }

        return out;
    }

    enum class CreatureArchetype : uint8_t
    {
        Humanoid      = 0,
        Prey          = 1,
        Predator      = 2,
        Dragon        = 3,
        OtherCreature = 4
    };

    /// @brief Classifies a Skyrim race name / EditorID into CreatureArchetype (pure ASCII).
    [[nodiscard]] inline CreatureArchetype ClassifyCreatureArchetype(std::string_view raceText,
                                                                     bool isHumanoid) noexcept
    {
        if (isHumanoid)
        {
            return CreatureArchetype::Humanoid;
        }
        if (raceText.empty())
        {
            return CreatureArchetype::OtherCreature;
        }
        const auto has = [raceText](std::string_view needle) noexcept
        {
            return AwakeningDetail::ContainsICase(raceText, needle);
        };

        if (has("Dragon") && !has("DragonPriest"))
        {
            return CreatureArchetype::Dragon;
        }
        if (has("Deer") || has("Elk") || has("Fox") || has("Hare") || has("Rabbit") ||
            has("Goat") || has("Cow") || has("Chicken") || has("Horse") || has("Mammoth"))
        {
            return CreatureArchetype::Prey;
        }
        if (has("Wolf") || has("Sabre") || has("Bear") || has("Troll") || has("Spider") ||
            has("Chaurus") || has("Skeever") || has("Horker") || has("Mudcrab") ||
            has("Werewolf") || has("Hound") || has("Dog") || has("Spriggan") || has("Giant"))
        {
            return CreatureArchetype::Predator;
        }
        return CreatureArchetype::OtherCreature;
    }

    struct CreatureSightInput
    {
        AwakeningStage stage{AwakeningStage::Dormant};
        CreatureArchetype archetype{CreatureArchetype::Humanoid};
        bool playerLethalBrandish{false};
        Demeanor activeDemeanor{Demeanor::Courteous};
        uint8_t regionId{Region::LeftEye};
        float dwellSec{0.0f};
        float distanceMeters{5.0f};
        bool alreadyAwardedMilestone{false};
    };

    struct CreatureSightResult
    {
        bool triggered{false};
        float presenceAward{0.0f};
        bool markMilestone{false};
        TacticalSightBuffs buffs{};
        std::string whisper{};
        uint32_t colorRgb{0xA6D8FFu};
    };

    /// @brief Evaluates Creature Sight: Prey Gentle Sight, Predator Kyne's Stillness, Dragon Dov's Weight.
    [[nodiscard]] inline CreatureSightResult EvaluateCreatureSight(
        const CreatureSightInput& in, std::string_view creatureName)
    {
        CreatureSightResult out{};
        if (in.archetype == CreatureArchetype::Humanoid || in.stage < AwakeningStage::FirstGlimmer ||
            !std::isfinite(in.dwellSec) || !std::isfinite(in.distanceMeters))
        {
            return out;
        }

        const std::string_view name =
            creatureName.empty() ? std::string_view("Creature") : creatureName;

        // 1. Prey (Deer, Elk, Fox, Horse): Gentle / averted unarmed gaze quiets your step
        if (in.archetype == CreatureArchetype::Prey)
        {
            const bool gentleGaze =
                !in.playerLethalBrandish &&
                (in.activeDemeanor == Demeanor::Deferential ||
                 in.activeDemeanor == Demeanor::Courteous || in.regionId == Region::Ground ||
                 in.regionId == Region::Chin);
            if (gentleGaze && in.dwellSec >= 1.2f && in.distanceMeters <= 16.0f)
            {
                out.triggered = true;
                out.buffs.sneakBonus = 12.0f;
                out.buffs.durationSec = 4.0f;
                out.colorRgb = 0x33FF59u;
                out.whisper = std::format(
                    "Gentle Sight - your quiet gaze does not startle the {}.", name);
            }
            return out;
        }

        if (in.stage < AwakeningStage::SteadySight)
        {
            return out;
        }

        // 2. Predator (Wolf, Bear, Sabre Cat, Troll): Kyne's Stillness when unarmed & steady
        if (in.archetype == CreatureArchetype::Predator)
        {
            if (!in.playerLethalBrandish && in.dwellSec >= 1.5f && in.distanceMeters >= 2.0f &&
                in.distanceMeters <= 12.0f)
            {
                out.triggered = true;
                out.buffs.armorBonus = 30.0f;
                out.buffs.staminaRegenBonus = 15.0f;
                out.buffs.durationSec = 4.0f;
                out.colorRgb = 0xFFD966u;
                if (!in.alreadyAwardedMilestone)
                {
                    out.presenceAward = 0.15f;
                    out.markMilestone = true;
                    out.whisper = std::format(
                        "Kyne's Stillness - you hold the {}'s eyes steady (+0.15 Presence).", name);
                }
                else
                {
                    out.whisper = std::format(
                        "Kyne's Stillness - your eyes stay steady on the {}.", name);
                }
            }
            return out;
        }

        // 3. Dragon (Dov): Holding unbroken eye contact tests Presence & braces Magic Ward
        if (in.archetype == CreatureArchetype::Dragon)
        {
            if (in.dwellSec >= 2.0f && in.distanceMeters <= 25.0f)
            {
                out.triggered = true;
                out.buffs.resistMagicBonus = 18.0f;
                out.buffs.durationSec = 5.0f;
                out.colorRgb = 0xFFD966u;
                if (!in.alreadyAwardedMilestone)
                {
                    out.presenceAward = 0.50f;
                    out.markMilestone = true;
                    out.whisper = std::format(
                        "Dov's Weight - you meet {}'s gaze without flinching (+0.50 Presence, "
                        "Ward braced)!",
                        name);
                }
                else
                {
                    out.whisper =
                        std::format("Dov's Weight - steady against {}'s gaze (Ward braced).", name);
                }
            }
            return out;
        }

        return out;
    }
} // namespace TrueGaze::ExpandedPlay
