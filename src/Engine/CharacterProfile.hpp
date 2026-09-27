#pragma once

#include "PCH.h"
#include <cstdint>
#include <array>

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
    class CharacterProfile
    {
    public:
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
        };

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
            std::array<float, 9> vertexWeights{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
            /// False for creatures: no social triangle, fixation + head-dominant
            /// tracking instead (a wolf does not scan eyes/mouth/third-eye).
            bool triangleEnabled{true};
        };

        /// @brief Classifies temperament into a gaze profile. Pure, no SDK calls.
        /// @param in Raw temperament inputs (from the SDK adapter below).
        /// @param profileEnabled Master switch (INI [CharacterProfile] bEnableCharacterProfiles).
        static GazeProfile Classify(const TemperamentInput &in, bool profileEnabled = true) noexcept
        {
            GazeProfile p{};

            if (!profileEnabled)
            {
                return p; // default parity: all 1.0 multipliers
            }

            // --- Creatures: simplified profile ---------------------------------
            // A wolf does not run the social triangle. Fixation-dominant tracking
            // with slightly longer dwells reads as animal attention.
            if (!in.isHumanoid)
            {
                p.triangleEnabled = false;
                p.fixationScaleMult = 1.3f;
                p.aversionRateMult = 0.4f; // animals hold a stare; they don't avert socially
                p.aversionDwellMult = 0.6f;
                p.mutualGazeThresholdMult = 0.5f; // a staring wolf IS a signal
                return p;
            }

            // --- Confidence axis (0-3) -----------------------------------------
            // Low confidence -> more aversion, shorter mutual gaze, faster triangle
            // (Shackelford 1996: gaze avoidance correlates with shyness/low confidence).
            // Normalise to -1..+1 around Cautious/Brave midpoint (1.5).
            const float conf = (in.confidence - 1.5f) / 1.5f; // -1..+1

            p.aversionRateMult *= (1.0f - 0.45f * conf);        // cowardly: 1.45x, foolhardy: 0.55x
            p.aversionDwellMult *= (1.0f - 0.30f * conf);       // cowardly dwells longer in aversion
            p.mutualGazeThresholdMult *= (1.0f + 0.50f * conf); // foolhardy holds eye contact
            p.fixationScaleMult *= (1.0f - 0.20f * conf);       // cowardly: quicker, darting glances

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
                p.mutualGazeThresholdMult *= (1.0f + 0.4f * (-rel)); // enemies stare too — hostile lock
                p.modeBiasAffect -= 0.4f * (-rel);                   // toward LOGIC
                p.aversionRateMult *= (1.0f - 0.3f * (-rel));        // enemies don't look away
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

            // Clamp all multipliers to sane ranges so no combination can produce
            // degenerate behaviour (0 = never, >3 = tripled).
            p.aversionRateMult = Clamp(p.aversionRateMult, 0.1f, 3.0f);
            p.aversionDwellMult = Clamp(p.aversionDwellMult, 0.1f, 3.0f);
            p.mutualGazeThresholdMult = Clamp(p.mutualGazeThresholdMult, 0.2f, 3.0f);
            p.fixationScaleMult = Clamp(p.fixationScaleMult, 0.3f, 3.0f);
            p.pathRandomnessMult = Clamp(p.pathRandomnessMult, 0.0f, 2.0f);
            p.modeBiasAffect = Clamp(p.modeBiasAffect, -1.0f, 1.0f);
            for (auto &w : p.vertexWeights)
            {
                w = Clamp(w, 0.1f, 3.0f);
            }

            return p;
        }

        /// @brief Default profile — exact parity with pre-profile engine behaviour.
        static GazeProfile Default() noexcept
        {
            return GazeProfile{};
        }

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
    inline CharacterProfile::TemperamentInput GatherTemperament(const RE::Actor *a_actor,
                                                                const RE::Actor *a_observer) noexcept
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
        if (const auto *base = a_actor->GetActorBase())
        {
            in.confidence = base->GetBaseActorValue(RE::ActorValue::kConfidence);
            in.aggression = base->GetBaseActorValue(RE::ActorValue::kAggression);
            in.assistance = base->GetBaseActorValue(RE::ActorValue::kAssistance);
        }

        // --- Relationship rank to the observer (usually the player) ------------
        // NOTE: the SDK's BGSRelationship::GetRelationship is a raw static
        // REL::Relocation call. It crashed the game on first actor tick
        // (2026-09-25, actor 000BB971) — an unverified relocation is the single
        // most dangerous call class in this codebase (see the console-command
        // and vtable-slot lessons). Relationship input is therefore DISABLED
        // until a verified read path is established in-engine. The profile still
        // differentiates via Confidence/Aggression/archetypes; relationship
        // ranks read as neutral (0).
        in.relationshipRank = 0.0f;

        // --- Archetype flags ----------------------------------------------------
        // IsGuard/IsChild/IsHumanoid/IsInCombat are Actor virtuals with SDK
        // default implementations — proven safe on the live actor (already used
        // in the eligibility log line and the HCEP mode block in prior builds).
        in.isGuard = a_actor->IsGuard();
        in.isChild = a_actor->IsChild();
        in.isHumanoid = a_actor->IsHumanoid();
        in.inCombat = a_actor->IsInCombat();

        return in;
    }

} // namespace TrueGaze::Engine
#endif
