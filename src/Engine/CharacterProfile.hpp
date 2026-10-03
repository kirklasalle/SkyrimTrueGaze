#pragma once

#include "PCH.h"
#include <array>
#include <cstdint>

namespace TrueGaze::Engine
{

    /// @brief Character Gaze Profile — temperament-driven gaze characterisation.
    ///
    /// TrueGaze already solves HOW eyes move (biomechanics). This module solves
    /// WHO is looking: every AI entity in Skyrim carries Bethesda's own
    /// characterization (Confidence, Aggression, relationship ranks, factions,
    /// race, combat state), and the GazeProfile projects that onto the HCEP-02
    /// Enhanced Diagram so a cowardly merchant, a foolhardy guard, a lover, and
    /// a wolf each LOOK like themselves.
    ///
    /// THE SPECTRUM PRINCIPLE: no NPC is a "type". Every entity is a point in
    /// (Confidence x Aggression x Relationship x StoryState) space, and the
    /// profile is that point projected onto the diagram's regions as behavioural
    /// weights.
    ///
    /// Psychology grounding:
    /// - Gaze avoidance correlates with low confidence / shyness (Shackelford et al.,
    ///   Personality & Individual Differences, 1996).
    /// - Affective eye contact has robust attentional/emotional effects (Frontiers
    ///   in Psychology 2018, "Affective Eye Contact: An Integrative Review").
    /// - Personality shapes gaze patterns (SAGE QJEP 2025, 116-participant study).
    /// - Gaze aversion in conversation = cognitive-load management (PMC8188832).
    ///
    /// DESIGN CONTRACTS:
    /// - Pure function, no state, no SDK dependency in the math (SDK adapter below).
    /// - DEFAULT PARITY: a default-constructed profile multiplies every parameter
    ///   by exactly 1.0 — the engine behaves bit-for-bit as before until a profile
    ///   is active. Additive only.
    /// - No ESP, no Papyrus, no MCM. INI keys only.
    ///
    /// R15 CATEGORY LAYER (2026-09-27, Kirk LaSalle directive): every entity
    /// class in Skyrim — player, NPC types, races, creatures, animals, other —
    /// gazes like ITSELF. Categories are a multiplier bundle layered on top of
    /// the temperament axes; a neutral bundle (all 1.0) is exact parity.
    class CharacterProfile
    {
    public:
        /// @brief Entity category, derived at gather time from safe base-form
        /// reads (race FormID, actor-base keywords, Actor virtuals). The category
        /// selects a multiplier bundle applied AFTER the temperament axes.
        enum class GazeCategory : std::uint8_t
        {
            Player = 0,
            HumanoidNPC,
            Guard,
            Child,
            Vampire,
            Werewolf,
            Khajiit,
            Argonian,
            Elf,
            Orc,
            OtherHumanoid,
            Creature_Predator,
            Creature_Prey,
            Creature_Dragon,
            Undead,
            Daedra,
            Construct,
            OtherCreature,
            Count
        };

        /// Raw temperament inputs, gathered by the SDK adapter. All values are
        /// already normalised to the ranges Skyrim itself uses.
        struct TemperamentInput
        {
            /// Confidence AV 0-3: 0=Cowardly, 1=Cautious, 2=Brave, 3=Foolhardy.
            float confidence{2.0f};
            /// Aggression AV 0-3: 0=Unaggressive .. 3=Frenzied.
            float aggression{1.0f};
            /// Assistance AV 0-3 (helps nobody/friends/allies/anyone).
            float assistance{1.0f};
            /// Relationship rank to the observer-of-interest (usually the player),
            /// -4 (Archnemesis) .. +4 (Lover).
            float relationshipRank{0.0f};
            /// True for guards / authority figures.
            bool isGuard{false};
            /// True for children.
            bool isChild{false};
            /// True for humanoids (false = creature: simplified profile).
            bool isHumanoid{true};
            /// True while in combat.
            bool inCombat{false};
            /// R15: entity category (race / archetype / creature class).
            GazeCategory category{GazeCategory::HumanoidNPC};
            /// R15 C2.5: player play-style bias, gathered from equipped state.
            /// 0 = none/neutral, >0 = stealth-leaning, <0 = heavy-combat-leaning.
            /// Only meaningful for the Player category; 0 elsewhere = parity.
            float playStyleBias{0.0f};
        };

        /// @brief A per-category multiplier bundle. Every field multiplies onto
        /// the temperament-derived profile; a default bundle (all 1.0, triangle
        /// untouched) is exact parity. Compiled defaults express the stereotype
        /// the category implies; the INI [Profiles] section can override the
        /// high-signal fields per category.
        struct CategoryProfileBundle
        {
            float aversionRateMult{1.0f};
            float aversionDwellMult{1.0f};
            float mutualGazeThresholdMult{1.0f};
            float fixationScaleMult{1.0f};
            float pathRandomnessMult{1.0f};
            float modeBiasAffect{0.0f};
            /// -1 = leave the temperament decision untouched (parity);
            /// 0/1 = force triangle off/on (creatures force off).
            int triangleMode{-1};
        };

        /// @brief Compiled default bundle for a category (the stereotype).
        ///        All-neutral for HumanoidNPC/OtherHumanoid = parity.
        static CategoryProfileBundle DefaultBundleFor(GazeCategory c) noexcept
        {
            CategoryProfileBundle b{};
            switch (c)
            {
            case GazeCategory::Guard:
                // Steady, unflinching authority attention.
                b.aversionRateMult = 0.6f;
                b.fixationScaleMult = 1.25f;
                b.modeBiasAffect = -0.2f;
                break;
            case GazeCategory::Child:
                // Quick, curious scanning; little sustained lock.
                b.fixationScaleMult = 0.6f;
                b.aversionRateMult = 1.3f;
                b.mutualGazeThresholdMult = 0.7f;
                b.pathRandomnessMult = 1.4f;
                break;
            case GazeCategory::Vampire:
                // Intense, unblinking predatory lock.
                b.aversionRateMult = 0.2f;
                b.fixationScaleMult = 1.5f;
                b.mutualGazeThresholdMult = 1.4f;
                b.modeBiasAffect = -0.4f;
                break;
            case GazeCategory::Werewolf:
                // Feral, restless — brief hard locks, frequent shifts.
                b.fixationScaleMult = 0.7f;
                b.aversionRateMult = 1.2f;
                b.pathRandomnessMult = 1.5f;
                b.modeBiasAffect = -0.3f;
                break;
            case GazeCategory::Khajiit:
                // Feline darting: quick curious glances, playful scanning.
                b.fixationScaleMult = 0.8f;
                b.pathRandomnessMult = 1.3f;
                b.aversionRateMult = 1.15f;
                break;
            case GazeCategory::Argonian:
                // Reptilian stillness: long steady holds, slow deliberate shifts.
                b.fixationScaleMult = 1.3f;
                b.aversionRateMult = 0.7f;
                b.pathRandomnessMult = 0.7f;
                break;
            case GazeCategory::Elf:
                // Aldmeri poise: measured, composed, slightly aloof.
                b.fixationScaleMult = 1.15f;
                b.aversionRateMult = 0.8f;
                b.modeBiasAffect = -0.1f;
                break;
            case GazeCategory::Orc:
                // Direct, confrontational, little aversion.
                b.aversionRateMult = 0.5f;
                b.fixationScaleMult = 1.2f;
                b.modeBiasAffect = -0.3f;
                break;
            case GazeCategory::Creature_Predator:
                // Stalking attention: fixation-dominant, no social triangle.
                b.triangleMode = 0;
                b.fixationScaleMult = 1.4f;
                b.aversionRateMult = 0.3f;
                b.aversionDwellMult = 0.6f;
                b.mutualGazeThresholdMult = 0.5f;
                break;
            case GazeCategory::Creature_Prey:
                // Skittish: frequent wary glances, short dwells.
                b.triangleMode = 0;
                b.fixationScaleMult = 0.6f;
                b.aversionRateMult = 1.6f;
                b.aversionDwellMult = 0.5f;
                break;
            case GazeCategory::Creature_Dragon:
                // Ancient, imperious, unblinking.
                b.triangleMode = 0;
                b.fixationScaleMult = 1.8f;
                b.aversionRateMult = 0.15f;
                b.mutualGazeThresholdMult = 1.5f;
                break;
            case GazeCategory::Undead:
                // Hollow, unfocused — long vacant stares, no social warmth.
                b.fixationScaleMult = 1.6f;
                b.aversionRateMult = 0.4f;
                b.modeBiasAffect = -0.5f;
                break;
            case GazeCategory::Daedra:
                // Otherworldly menace: hard lock, cold appraisal.
                b.aversionRateMult = 0.25f;
                b.fixationScaleMult = 1.4f;
                b.modeBiasAffect = -0.5f;
                break;
            case GazeCategory::Construct:
                // Dwarven automata: mechanical fixation, zero social softness.
                b.triangleMode = 0;
                b.fixationScaleMult = 2.0f;
                b.aversionRateMult = 0.1f;
                b.modeBiasAffect = -0.6f;
                break;
            case GazeCategory::Player:
            case GazeCategory::HumanoidNPC:
            case GazeCategory::OtherHumanoid:
            case GazeCategory::OtherCreature:
            default:
                // Neutral: the temperament axes carry the whole profile.
                break;
            }
            return b;
        }

        // (ApplyBundle is declared after GazeProfile below — it needs the type.)

        /// The resolved gaze profile. Every field is a MULTIPLIER on the engine's
        /// existing base parameters (from TrueGaze.ini), so 1.0 = unchanged.
        struct GazeProfile
        {
            /// Multiplier on CGA aversion frequency (how often gaze breaks away).
            float aversionRateMult{1.0f};
            /// Multiplier on CGA aversion dwell time (how long each aversion lasts).
            float aversionDwellMult{1.0f};
            /// Multiplier on the mutual-gaze hold threshold (seconds of eye contact
            /// before intimacy reactions). >1 = holds eye contact longer.
            float mutualGazeThresholdMult{1.0f};
            /// Multiplier on social-triangle fixation durations.
            float fixationScaleMult{1.0f};
            /// Multiplier on triangle path randomness (clamped 0..1 after scaling).
            float pathRandomnessMult{1.0f};
            /// HCEP mode bias: -1 = toward LOGIC (0), +1 = toward AFFECT (1).
            /// Applied as a nudge, never an override of dialogue/combat state.
            float modeBiasAffect{0.0f};
            /// Vertex selection weights for the social triangle / extended diagram.
            /// Index = SocialTriangle::Vertex (0..8). 1.0 = neutral.
            /// Examples: shy NPC raises LowerRightAversion (shyness/deception);
            /// lover raises Chest (HEART resonance); scholar raises ThirdEye.
            std::array<float, 9> vertexWeights{1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                                               1.0f, 1.0f, 1.0f, 1.0f};
            /// False for creatures: no social triangle, fixation + head-dominant
            /// tracking instead (a wolf does not scan eyes/mouth/third-eye).
            bool triangleEnabled{true};
        };

        /// @brief Applies a category bundle onto a profile (multiplicative).
        /// R15: called from Classify AFTER the temperament axes and combat state,
        /// BEFORE clamps. A neutral bundle is a no-op = exact pre-R15 parity.
        static void ApplyBundle(GazeProfile& p, const CategoryProfileBundle& b) noexcept
        {
            p.aversionRateMult *= b.aversionRateMult;
            p.aversionDwellMult *= b.aversionDwellMult;
            p.mutualGazeThresholdMult *= b.mutualGazeThresholdMult;
            p.fixationScaleMult *= b.fixationScaleMult;
            p.pathRandomnessMult *= b.pathRandomnessMult;
            p.modeBiasAffect += b.modeBiasAffect;
            if (b.triangleMode == 0)
            {
                p.triangleEnabled = false;
            }
            else if (b.triangleMode == 1)
            {
                p.triangleEnabled = true;
            }
            // triangleMode == -1: leave the temperament decision untouched.
        }

        /// @brief Classifies temperament into a gaze profile. Pure, no SDK calls.
        /// @param in Raw temperament inputs (from the SDK adapter below).
        /// @param profileEnabled Master switch (INI [CharacterProfile] bEnableCharacterProfiles).
        /// @param categoryBundle R15: per-category multiplier bundle (INI [Profiles]);
        ///        the default argument (all-neutral) is exact pre-R15 parity.
        static GazeProfile Classify(const TemperamentInput& in, bool profileEnabled = true,
                                    const CategoryProfileBundle& categoryBundle = {}) noexcept
        {
            GazeProfile p{};

            if (!profileEnabled)
            {
                return p; // default parity: all 1.0 multipliers
            }

            // --- Creatures: simplified profile ---------------------------------
            // A wolf does not run the social triangle. Fixation-dominant tracking
            // with slightly longer dwells reads as animal attention.
            // R15: the creature-class CATEGORY bundle (Predator/Prey/Dragon/Construct)
            // now layers on top; OtherCreature keeps the legacy simplified profile.
            if (!in.isHumanoid)
            {
                p.triangleEnabled = false;
                p.fixationScaleMult = 1.3f;
                p.aversionRateMult = 0.4f; // animals hold a stare; they don't avert socially
                p.aversionDwellMult = 0.6f;
                p.mutualGazeThresholdMult = 0.5f; // a staring wolf IS a signal

                ApplyBundle(p, categoryBundle);
                ClampProfile(p);
                return p;
            }

            // --- Confidence axis (0-3) -----------------------------------------
            // Low confidence -> more aversion, shorter mutual gaze, faster triangle
            // (Shackelford 1996: gaze avoidance correlates with shyness/low confidence).
            // Normalise to -1..+1 around Cautious/Brave midpoint (1.5).
            const float conf = (in.confidence - 1.5f) / 1.5f; // -1..+1

            p.aversionRateMult *= (1.0f - 0.45f * conf);  // cowardly: 1.45x, foolhardy: 0.55x
            p.aversionDwellMult *= (1.0f - 0.30f * conf); // cowardly dwells longer in aversion
            p.mutualGazeThresholdMult *= (1.0f + 0.50f * conf); // foolhardy holds eye contact
            // Cowardly (conf<0) darts quicker -> SHORTER fixations; foolhardy
            // (conf>0) holds longer. (The old sign was inverted: it gave
            // cowardly 1.2x LONGER fixations, contradicting the comment,
            // the psychology, and the KinematicsTests confidence ordering.)
            p.fixationScaleMult *= (1.0f + 0.20f * conf);

            // Shyness signature on the diagram: low confidence raises the
            // Lower-Right aversion region (shyness/fear/deception per HCEP-02).
            if (conf < -0.2f)
            {
                p.vertexWeights[8] *= (1.0f + 0.8f * (-conf)); // LowerRightAversion
            }

            // --- Aggression axis (0-3) -----------------------------------------
            // High aggression -> harder eye-lock, less aversion, LOGIC bias.
            const float aggr = (in.aggression - 1.5f) / 1.5f; // -1..+1

            p.aversionRateMult *= (1.0f - 0.35f * aggr);
            p.modeBiasAffect -= 0.35f * aggr; // aggressive entities read colder/analytical

            // --- Relationship axis (-4..+4) -----------------------------------
            // Lovers/confidants: HEART lean, long mutual gaze, Chest-region visits.
            // Enemies: LOGIC lock, minimal triangle softness.
            const float rel = in.relationshipRank / 4.0f; // -1..+1

            if (rel > 0.25f)
            {
                p.mutualGazeThresholdMult *= (1.0f + 0.6f * rel); // lovers hold eyes far longer
                p.fixationScaleMult *= (1.0f + 0.35f * rel);      // slower, warmer scanning
                p.modeBiasAffect += 0.5f * rel;                   // toward AFFECT/HEART
                p.vertexWeights[4] *= (1.0f + 0.9f * rel);        // Chest (heart resonance)
            }
            else if (rel < -0.25f)
            {
                p.mutualGazeThresholdMult *=
                    (1.0f + 0.4f * (-rel));                   // enemies stare too — hostile lock
                p.modeBiasAffect -= 0.4f * (-rel);            // toward LOGIC
                p.aversionRateMult *= (1.0f - 0.3f * (-rel)); // enemies don't look away
            }

            // --- Guard archetype ------------------------------------------------
            // Authority reads as steady, unflinching attention.
            if (in.isGuard)
            {
                p.aversionRateMult *= 0.6f;
                p.fixationScaleMult *= 1.25f;
                p.modeBiasAffect -= 0.2f;
            }

            // --- Child archetype -------------------------------------------------
            // Children: quick, curious scanning; wide saccades; little sustained lock.
            if (in.isChild)
            {
                p.fixationScaleMult *= 0.6f;
                p.aversionRateMult *= 1.3f;
                p.mutualGazeThresholdMult *= 0.7f;
                p.pathRandomnessMult = 1.4f; // clamp downstream
            }

            // --- Combat state ----------------------------------------------------
            // In combat everyone locks: LOGIC, minimal aversion, hard fixation.
            if (in.inCombat)
            {
                p.aversionRateMult *= 0.25f;
                p.fixationScaleMult *= 1.4f;
                p.modeBiasAffect -= 0.6f;
            }

            // --- R15 CATEGORY LAYER -------------------------------------------------
            // The category bundle (race / archetype / creature-class stereotype)
            // applies AFTER the temperament axes and combat state, BEFORE clamps.
            // A neutral bundle is a no-op = exact pre-R15 parity.
            ApplyBundle(p, categoryBundle);

            // --- R15 C2.5: Player play-style bias -----------------------------------
            // Stealth-leaning (>0): wary, watchful — more aversion, shorter fixations.
            // Heavy-combat-leaning (<0): steady, direct — longer holds, less aversion.
            // Lightweight and observational; 0 (the default) is parity.
            if (in.category == GazeCategory::Player && in.playStyleBias != 0.0f)
            {
                const float stealth = in.playStyleBias; // -1..+1
                p.aversionRateMult *= (1.0f + 0.2f * stealth);
                p.fixationScaleMult *= (1.0f - 0.1f * stealth);
            }

            ClampProfile(p);
            return p;
        }

        /// @brief Clamp all profile multipliers to sane ranges so no combination
        ///        can produce degenerate behaviour (0 = never, >3 = tripled).
        static void ClampProfile(GazeProfile& p) noexcept
        {
            p.aversionRateMult = Clamp(p.aversionRateMult, 0.1f, 3.0f);
            p.aversionDwellMult = Clamp(p.aversionDwellMult, 0.1f, 3.0f);
            p.mutualGazeThresholdMult = Clamp(p.mutualGazeThresholdMult, 0.2f, 3.0f);
            p.fixationScaleMult = Clamp(p.fixationScaleMult, 0.3f, 3.0f);
            p.pathRandomnessMult = Clamp(p.pathRandomnessMult, 0.0f, 2.0f);
            p.modeBiasAffect = Clamp(p.modeBiasAffect, -1.0f, 1.0f);
            for (auto& w : p.vertexWeights)
            {
                w = Clamp(w, 0.1f, 3.0f);
            }
        }

        /// @brief Default profile — exact parity with pre-profile engine behaviour.
        static GazeProfile Default() noexcept { return GazeProfile{}; }

    private:
        static float Clamp(float v, float lo, float hi) noexcept
        {
            return v < lo ? lo : (v > hi ? hi : v);
        }
    };

} // namespace TrueGaze::Engine

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>

namespace TrueGaze::Engine
{

    /// @brief SDK adapter: gathers TemperamentInput from a live RE::Actor.
    ///
    /// All reads are null-guarded; any failure falls back to the neutral default
    /// input (Confidence 2, Aggression 1 — the most common NPC template), which
    /// yields a near-parity profile. Safe to call on any actor including the
    /// player and creatures.
    inline float GatherPlayerPlayStyle(const RE::Actor* a_player) noexcept;

    inline CharacterProfile::GazeCategory ClassifyCategory(const RE::Actor* a_actor,
                                                           bool isHumanoid) noexcept;

    inline CharacterProfile::TemperamentInput
    GatherTemperament(const RE::Actor* a_actor, const RE::Actor* a_observer) noexcept
    {
        CharacterProfile::TemperamentInput in{};

        if (!a_actor)
        {
            return in;
        }

        // --- Actor values (Confidence / Aggression / Assistance) ---------------
        // READ FROM THE BASE FORM, NOT THE ACTOR. Actor::GetBaseActorValue
        // dispatches into engine code that consults the actor's AI process —
        // which can be mid-initialisation during save/new-game load (the game
        // crashed on the first eligible actor tick, 2026-09-25). TESActorBase
        // also inherits ActorValueOwner, and Confidence/Aggression/Assistance
        // are static per-NPC data (the NPC record's AI Data tab) — so the base
        // form is both the SAFE read path and the semantically correct one.
        if (const auto* base = a_actor->GetActorBase())
        {
            in.confidence = base->GetBaseActorValue(RE::ActorValue::kConfidence);
            in.aggression = base->GetBaseActorValue(RE::ActorValue::kAggression);
            in.assistance = base->GetBaseActorValue(RE::ActorValue::kAssistance);
        }

        // --- Relationship rank to the observer (usually the player) ------------
        // R15 C1.3 (2026-09-27): RE-ENABLED via the proven-safe read path.
        //
        // HISTORY: the SDK's BGSRelationship::GetRelationship is a raw static
        // REL::Relocation call that crashed the game on first actor tick
        // (2026-09-25, actor 000BB971), so this input was hard-disabled
        // (relationshipRank = 0). Since then, R14 E7.6 shipped
        // GazeEngine::RelationshipRankForActor, which calls the SAME relocation
        // through a full null-guard chain (base form -> relationships array ->
        // GetRelationship) and has run safely in the feedback packet path. This
        // adapter now follows that exact pattern. If any guard fails, the input
        // degrades to neutral (0) — never a crash.
        if (const auto* base = a_actor->GetActorBase())
        {
            if (base->relationships)
            {
                const auto* observer = a_observer;
                if (observer)
                {
                    if (const auto* observerBase = observer->GetActorBase())
                    {
                        // GetRelationship takes non-const TESNPC*; the call is a
                        // pure data-side read (the R14 E7.6 proven path), so the
                        // const_cast is safe by contract.
                        auto* npc1 = const_cast<RE::TESNPC*>(base->As<RE::TESNPC>());
                        auto* npc2 = const_cast<RE::TESNPC*>(observerBase->As<RE::TESNPC>());
                        if (npc1 && npc2)
                        {
                            if (const auto* rel = RE::BGSRelationship::GetRelationship(npc1, npc2))
                            {
                                // Map Skyrim's 0..8 relationship ladder onto -4..+4
                                // (0=Lover -> +4 ... 4=Acquaintance -> 0 ...
                                //  8=Archnemesis -> -4).
                                const auto level = rel->level.get();
                                in.relationshipRank =
                                    static_cast<float>(4 - static_cast<std::int32_t>(level));
                            }
                        }
                    }
                }
            }
        }

        // --- Archetype flags ----------------------------------------------------
        // IsGuard/IsChild/IsHumanoid/IsInCombat are Actor virtuals with SDK
        // default implementations — proven safe on the live actor (already used
        // in the eligibility log line and the HCEP mode block in prior builds).
        in.isGuard = a_actor->IsGuard();
        in.isChild = a_actor->IsChild();
        in.isHumanoid = a_actor->IsHumanoid();
        in.inCombat = a_actor->IsInCombat();

        // --- R15 C1.2: Category gathering ---------------------------------------
        // All reads are BASE-FORM (TESActorBase / TESRace data + keyword arrays):
        // no actor-process dispatch, no relocations — the 2026-09-25 crash
        // lesson applied as a standing rule. Any failure falls back to the
        // neutral category (HumanoidNPC / OtherCreature) = parity.
        in.category = ClassifyCategory(a_actor, in.isHumanoid);

        // --- R15 C2.5: Player play-style bias ------------------------------------
        // Lightweight and observational: read the equipped weapon/spell state
        // from the player's base form data. Stealth-leaning (dagger/bow/illusion)
        // biases wary watchfulness; heavy arms bias steady directness. 0 = parity.
        if (a_actor->IsPlayerRef())
        {
            in.category = CharacterProfile::GazeCategory::Player;
            in.playStyleBias = GatherPlayerPlayStyle(a_actor);
        }

        return in;
    }

    /// @brief R15 C1.2: derives the GazeCategory from safe base-form reads.
    ///
    /// Priority: player flag (handled by the caller) > supernatural keywords >
    /// race match > archetype flags > humanoid/creature fallback. Keyword reads
    /// are BGSKeywordForm::HasKeyword — pure data-side array scans, no dispatch.
    inline CharacterProfile::GazeCategory ClassifyCategory(const RE::Actor* a_actor,
                                                           bool isHumanoid) noexcept
    {
        if (!a_actor)
        {
            return CharacterProfile::GazeCategory::HumanoidNPC;
        }

        const auto* base = a_actor->GetActorBase();
        if (!base)
        {
            return isHumanoid ? CharacterProfile::GazeCategory::HumanoidNPC
                              : CharacterProfile::GazeCategory::OtherCreature;
        }

        // TESNPC::GetRace() is non-const in the SDK; race and keyword reads are
        // pure data-side accessors, so a const_cast here is safe by contract
        // (the same pattern the SDK's own non-const getters imply).
        auto* npcBase = const_cast<RE::TESNPC*>(base->As<RE::TESNPC>());
        if (!npcBase)
        {
            return isHumanoid ? CharacterProfile::GazeCategory::HumanoidNPC
                              : CharacterProfile::GazeCategory::OtherCreature;
        }

        // --- Supernatural / type keywords first (they override race) -----------
        // Keyword FormIDs are resolved once at first use and cached — the same
        // lazy-static pattern proven elsewhere in this codebase.
        struct KeywordCache
        {
            RE::BGSKeyword* vampire{nullptr};
            RE::BGSKeyword* werewolf{nullptr};
            RE::BGSKeyword* undead{nullptr};
            RE::BGSKeyword* daedra{nullptr};
            RE::BGSKeyword* construct{nullptr};
            RE::BGSKeyword* predator{nullptr};
            RE::BGSKeyword* prey{nullptr};
            bool resolved{false};

            void Resolve() noexcept
            {
                // Editor-ID lookup via TESForm::LookupByEditorID (data-side,
                // no relocation) — the same class of read as LookupByID.
                vampire = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("Vampire");
                werewolf = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("Werewolf");
                undead = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeUndead");
                daedra = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeDaedra");
                construct =
                    RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypeDwarvenAutomaton");
                predator = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypePredator");
                prey = RE::TESForm::LookupByEditorID<RE::BGSKeyword>("ActorTypePrey");
                resolved = true;
            }
        };
        static KeywordCache kw;
        if (!kw.resolved)
        {
            kw.Resolve();
        }

        const auto hasKeyword = [npcBase](RE::BGSKeyword* k) noexcept
        {
            return k && npcBase->HasKeyword(k);
        };

        // --- Creature branch ------------------------------------------------------
        if (!isHumanoid)
        {
            if (hasKeyword(kw.construct))
            {
                return CharacterProfile::GazeCategory::Construct;
            }
            if (hasKeyword(kw.daedra))
            {
                return CharacterProfile::GazeCategory::Daedra;
            }
            if (hasKeyword(kw.undead))
            {
                return CharacterProfile::GazeCategory::Undead;
            }
            // Dragon race check: the race's editor ID contains "Dragon".
            if (const auto* race = npcBase->GetRace())
            {
                const char* raceEdid = race->GetFormEditorID();
                if (raceEdid && std::strstr(raceEdid, "Dragon") != nullptr)
                {
                    return CharacterProfile::GazeCategory::Creature_Dragon;
                }
            }
            if (hasKeyword(kw.predator))
            {
                return CharacterProfile::GazeCategory::Creature_Predator;
            }
            if (hasKeyword(kw.prey))
            {
                return CharacterProfile::GazeCategory::Creature_Prey;
            }
            return CharacterProfile::GazeCategory::OtherCreature;
        }

        // --- Humanoid branch -------------------------------------------------------
        if (hasKeyword(kw.vampire))
        {
            return CharacterProfile::GazeCategory::Vampire;
        }
        if (hasKeyword(kw.werewolf))
        {
            return CharacterProfile::GazeCategory::Werewolf;
        }
        if (hasKeyword(kw.undead))
        {
            return CharacterProfile::GazeCategory::Undead;
        }
        if (hasKeyword(kw.daedra))
        {
            return CharacterProfile::GazeCategory::Daedra;
        }

        // --- Race match (vanilla editor IDs; unknown races fall back) ------------
        if (const auto* race = npcBase->GetRace())
        {
            const char* editorId = race->GetFormEditorID();
            if (editorId)
            {
                auto startsWithAny =
                    [editorId](std::initializer_list<const char*> prefixes) noexcept
                {
                    for (const char* p : prefixes)
                    {
                        const std::size_t len = std::strlen(p);
                        if (std::strncmp(editorId, p, len) == 0)
                        {
                            return true;
                        }
                    }
                    return false;
                };

                if (startsWithAny({"KhajiitRace", "KhajiitVampireRace"}))
                {
                    return CharacterProfile::GazeCategory::Khajiit;
                }
                if (startsWithAny({"ArgonianRace", "ArgonianVampireRace"}))
                {
                    return CharacterProfile::GazeCategory::Argonian;
                }
                if (startsWithAny({"OrcRace", "OrcVampireRace", "OrcWerewolfRace"}))
                {
                    return CharacterProfile::GazeCategory::Orc;
                }
                if (startsWithAny({"HighElfRace", "WoodElfRace", "DarkElfRace", "SnowElfRace"}))
                {
                    return CharacterProfile::GazeCategory::Elf;
                }
                // Elf vampire variants share the Elf prefix pattern.
                if (startsWithAny(
                        {"HighElfVampireRace", "WoodElfVampireRace", "DarkElfVampireRace"}))
                {
                    return CharacterProfile::GazeCategory::Elf;
                }
                // Nord/Imperial/Breton/Redguard and modded humanoid races:
                // the temperament axes carry the profile (neutral bundle).
                return CharacterProfile::GazeCategory::HumanoidNPC;
            }
        }

        // --- Archetype flags (race unknown) ---------------------------------------
        if (a_actor->IsGuard())
        {
            return CharacterProfile::GazeCategory::Guard;
        }
        if (a_actor->IsChild())
        {
            return CharacterProfile::GazeCategory::Child;
        }

        return CharacterProfile::GazeCategory::HumanoidNPC;
    }

    /// @brief R15 C2.5: player play-style bias from equipped state.
    ///
    /// Returns a bias in [-1, +1]: positive = stealth-leaning (wary, watchful),
    /// negative = heavy-combat-leaning (steady, direct), 0 = neutral/parity.
    /// Reads only equipped-item base forms — data-side, no dispatch. This is
    /// deliberately LIGHTWEIGHT AND OBSERVATIONAL (flagged for Kirk's review
    /// before any tuning pass changes the multipliers in Classify).
    inline float GatherPlayerPlayStyle(const RE::Actor* a_player) noexcept
    {
        if (!a_player)
        {
            return 0.0f;
        }

        // The equipped state lives on the Actor's runtime data; the *type* of
        // what is equipped is read from the base form (data-side).
        float bias = 0.0f;

        const auto* rightHand = a_player->GetEquippedObject(false); // false = right hand
        const auto* leftHand = a_player->GetEquippedObject(true);   // true = left hand

        auto classifyEquipped = [](const RE::TESForm* form) -> float
        {
            if (!form)
            {
                return 0.0f;
            }
            // Daggers and bows read as stealth tools.
            if (const auto* weapon = form->As<RE::TESObjectWEAP>())
            {
                const auto type = weapon->GetWeaponType();
                if (type == RE::WEAPON_TYPE::kOneHandDagger || type == RE::WEAPON_TYPE::kBow)
                {
                    return 1.0f; // stealth-leaning
                }
                if (type == RE::WEAPON_TYPE::kTwoHandAxe || type == RE::WEAPON_TYPE::kTwoHandSword)
                {
                    return -1.0f; // heavy-combat-leaning
                }
                return -0.5f; // one-handed melee: mildly direct
            }
            // Spells read as neutral-to-stealthy (illusion leans stealth).
            if (form->As<RE::SpellItem>())
            {
                return 0.5f;
            }
            return 0.0f;
        };

        bias += classifyEquipped(rightHand);
        bias += classifyEquipped(leftHand);

        // Two hands of input; normalise to [-1, +1].
        if (bias > 1.0f)
        {
            bias = 1.0f;
        }
        else if (bias < -1.0f)
        {
            bias = -1.0f;
        }
        return bias;
    }

} // namespace TrueGaze::Engine
#endif
