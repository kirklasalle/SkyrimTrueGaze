#pragma once

#include "ExpandedPlay/ExpandedPlayModifiers.hpp"
#include "ExpandedPlay/ExpandedPlayTypes.hpp"
#include "ExpandedPlay/InterpretationEngine.hpp"
#include "ExpandedPlay/ResponseLadder.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace TrueGaze::ExpandedPlay
{
    /// @brief Social negotiation vectors generalizing TES IV Oblivion's Persuasion Wheel
    ///        into real-time 3D gaze + demeanor + transactional cues.
    enum class PersuasionVector : uint8_t
    {
        None     = 0,
        Admire   = 1,
        Joke     = 2,
        Coerce   = 3,
        Boast    = 4,
        SlyBribe = 5,
        Plea     = 6,
        Count    = 7
    };

    /// @brief Per-NPC temperament affinity matching Oblivion's Love / Like / Dislike / Hate.
    enum class AffinityTier : int8_t
    {
        Hate    = -2,
        Dislike = -1,
        Like    = 1,
        Love    = 2
    };

    /// @brief Real-time transaction outcome archetypes extending beyond gold-only bribery.
    enum class TransactionType : uint8_t
    {
        None           = 0,
        RapportBarter  = 1,
        LeverageCoerce = 2,
        PrestigeBoast  = 3,
        SlyBribePrime  = 4,
        StandDownGrace = 5,
        SeenCompassion = 6
    };

    [[nodiscard]] constexpr const char* PersuasionVectorName(PersuasionVector v) noexcept
    {
        switch (v)
        {
        case PersuasionVector::Admire: return "Admire";
        case PersuasionVector::Joke: return "Joke";
        case PersuasionVector::Coerce: return "Coerce";
        case PersuasionVector::Boast: return "Boast";
        case PersuasionVector::SlyBribe: return "SlyBribe";
        case PersuasionVector::Plea: return "Plea";
        case PersuasionVector::None:
        default: return "None";
        }
    }

    [[nodiscard]] constexpr const char* AffinityTierName(AffinityTier a) noexcept
    {
        switch (a)
        {
        case AffinityTier::Hate: return "Hate";
        case AffinityTier::Dislike: return "Dislike";
        case AffinityTier::Like: return "Like";
        case AffinityTier::Love: return "Love";
        default: return "Neutral";
        }
    }

    [[nodiscard]] constexpr const char* TransactionTypeName(TransactionType t) noexcept
    {
        switch (t)
        {
        case TransactionType::RapportBarter: return "RapportBarter";
        case TransactionType::LeverageCoerce: return "LeverageCoerce";
        case TransactionType::PrestigeBoast: return "PrestigeBoast";
        case TransactionType::SlyBribePrime: return "SlyBribePrime";
        case TransactionType::StandDownGrace: return "StandDownGrace";
        case TransactionType::SeenCompassion: return "SeenCompassion";
        case TransactionType::None:
        default: return "None";
        }
    }

    struct NpcAffinityProfile
    {
        // Indexed by (static_cast<size_t>(vector) - 1) for Admire..Plea
        std::array<AffinityTier, 6> tiers{
            AffinityTier::Like,    // Admire
            AffinityTier::Like,    // Joke
            AffinityTier::Dislike, // Coerce
            AffinityTier::Dislike, // Boast
            AffinityTier::Dislike, // SlyBribe
            AffinityTier::Like     // Plea
        };

        [[nodiscard]] constexpr AffinityTier ForVector(PersuasionVector v) const noexcept
        {
            const auto idx = static_cast<size_t>(v);
            if (idx == 0u || idx > tiers.size())
            {
                return AffinityTier::Like;
            }
            return tiers[idx - 1u];
        }
    };

    namespace PersuasionDetail
    {
        [[nodiscard]] constexpr uint32_t Fnv1aMix(uint32_t value) noexcept
        {
            uint32_t hash = 2166136261u;
            for (int i = 0; i < 4; ++i)
            {
                hash ^= (value >> (i * 8)) & 0xFFu;
                hash *= 16777619u;
            }
            return hash;
        }

        [[nodiscard]] constexpr AffinityTier StepAffinity(AffinityTier t, int delta) noexcept
        {
            constexpr AffinityTier kOrdered[4]{
                AffinityTier::Hate, AffinityTier::Dislike, AffinityTier::Like, AffinityTier::Love};
            int idx = 1;
            for (int i = 0; i < 4; ++i)
            {
                if (kOrdered[i] == t)
                {
                    idx = i;
                    break;
                }
            }
            idx = std::clamp(idx + delta, 0, 3);
            return kOrdered[idx];
        }
    } // namespace PersuasionDetail

    /// @brief Resolves a target NPC's deterministic 6-vector Oblivion-style temperament matrix,
    ///        combining hard child safety, Riverwood named cast profiles, culture, and FNV-1a seed.
    [[nodiscard]] constexpr NpcAffinityProfile ResolveNpcAffinityProfile(
        uint32_t baseFormIdLow24, ObserverCulture culture, RelationshipTier relTier,
        bool isGuard, bool isChild) noexcept
    {
        if (isChild)
        {
            return NpcAffinityProfile{{
                AffinityTier::Like,    // Admire (praise/encouragement)
                AffinityTier::Love,    // Joke (playfulness)
                AffinityTier::Hate,    // Coerce (strictly blocked)
                AffinityTier::Dislike, // Boast
                AffinityTier::Hate,    // SlyBribe (strictly blocked)
                AffinityTier::Like     // Plea (kindness)
            }};
        }

        // Bespoke Riverwood First-Slice NPC profiles (ActorBase low-24 FormIDs in Skyrim.esm)
        switch (baseFormIdLow24 & 0x00FFFFFFu)
        {
        case 0x01347Bu: // Camilla Valerius
            return NpcAffinityProfile{{AffinityTier::Love, AffinityTier::Like, AffinityTier::Hate,
                                       AffinityTier::Dislike, AffinityTier::Dislike,
                                       AffinityTier::Like}};
        case 0x01347Cu: // Lucan Valerius
            return NpcAffinityProfile{{AffinityTier::Like, AffinityTier::Dislike, AffinityTier::Hate,
                                       AffinityTier::Love, AffinityTier::Like,
                                       AffinityTier::Dislike}};
        case 0x013475u: // Alvor
            return NpcAffinityProfile{{AffinityTier::Like, AffinityTier::Like, AffinityTier::Dislike,
                                       AffinityTier::Love, AffinityTier::Hate, AffinityTier::Like}};
        case 0x013478u: // Orgnar
            return NpcAffinityProfile{{AffinityTier::Dislike, AffinityTier::Love, AffinityTier::Like,
                                       AffinityTier::Dislike, AffinityTier::Like,
                                       AffinityTier::Dislike}};
        case 0x013485u: // Delphine
            return NpcAffinityProfile{{AffinityTier::Dislike, AffinityTier::Hate, AffinityTier::Love,
                                       AffinityTier::Like, AffinityTier::Dislike,
                                       AffinityTier::Hate}};
        case 0x01347Fu: // Sven
            return NpcAffinityProfile{{AffinityTier::Love, AffinityTier::Love, AffinityTier::Hate,
                                       AffinityTier::Like, AffinityTier::Dislike,
                                       AffinityTier::Like}};
        case 0x013480u: // Faendal
            return NpcAffinityProfile{{AffinityTier::Like, AffinityTier::Dislike,
                                       AffinityTier::Dislike, AffinityTier::Love,
                                       AffinityTier::Dislike, AffinityTier::Love}};
        case 0x013479u: // Gerdur
            return NpcAffinityProfile{{AffinityTier::Like, AffinityTier::Like, AffinityTier::Hate,
                                       AffinityTier::Dislike, AffinityTier::Hate,
                                       AffinityTier::Love}};
        default:
            break;
        }

        // Universal deterministic 4! permutation over Admire/Joke/Coerce/Boast
        constexpr std::array<std::array<AffinityTier, 4>, 24> kPerms{{
            {AffinityTier::Love, AffinityTier::Like, AffinityTier::Dislike, AffinityTier::Hate},
            {AffinityTier::Love, AffinityTier::Like, AffinityTier::Hate, AffinityTier::Dislike},
            {AffinityTier::Love, AffinityTier::Dislike, AffinityTier::Like, AffinityTier::Hate},
            {AffinityTier::Love, AffinityTier::Dislike, AffinityTier::Hate, AffinityTier::Like},
            {AffinityTier::Love, AffinityTier::Hate, AffinityTier::Like, AffinityTier::Dislike},
            {AffinityTier::Love, AffinityTier::Hate, AffinityTier::Dislike, AffinityTier::Like},
            {AffinityTier::Like, AffinityTier::Love, AffinityTier::Dislike, AffinityTier::Hate},
            {AffinityTier::Like, AffinityTier::Love, AffinityTier::Hate, AffinityTier::Dislike},
            {AffinityTier::Like, AffinityTier::Dislike, AffinityTier::Love, AffinityTier::Hate},
            {AffinityTier::Like, AffinityTier::Dislike, AffinityTier::Hate, AffinityTier::Love},
            {AffinityTier::Like, AffinityTier::Hate, AffinityTier::Love, AffinityTier::Dislike},
            {AffinityTier::Like, AffinityTier::Hate, AffinityTier::Dislike, AffinityTier::Love},
            {AffinityTier::Dislike, AffinityTier::Love, AffinityTier::Like, AffinityTier::Hate},
            {AffinityTier::Dislike, AffinityTier::Love, AffinityTier::Hate, AffinityTier::Like},
            {AffinityTier::Dislike, AffinityTier::Like, AffinityTier::Love, AffinityTier::Hate},
            {AffinityTier::Dislike, AffinityTier::Like, AffinityTier::Hate, AffinityTier::Love},
            {AffinityTier::Dislike, AffinityTier::Hate, AffinityTier::Love, AffinityTier::Like},
            {AffinityTier::Dislike, AffinityTier::Hate, AffinityTier::Like, AffinityTier::Love},
            {AffinityTier::Hate, AffinityTier::Love, AffinityTier::Like, AffinityTier::Dislike},
            {AffinityTier::Hate, AffinityTier::Love, AffinityTier::Dislike, AffinityTier::Like},
            {AffinityTier::Hate, AffinityTier::Like, AffinityTier::Love, AffinityTier::Dislike},
            {AffinityTier::Hate, AffinityTier::Like, AffinityTier::Dislike, AffinityTier::Love},
            {AffinityTier::Hate, AffinityTier::Dislike, AffinityTier::Love, AffinityTier::Like},
            {AffinityTier::Hate, AffinityTier::Dislike, AffinityTier::Like, AffinityTier::Love},
        }};

        const uint32_t mix = PersuasionDetail::Fnv1aMix(baseFormIdLow24);
        const auto& base4 = kPerms[mix % 24u];

        NpcAffinityProfile p{};
        p.tiers[0] = base4[0]; // Admire
        p.tiers[1] = base4[1]; // Joke
        p.tiers[2] = base4[2]; // Coerce
        p.tiers[3] = base4[3]; // Boast
        p.tiers[4] = ((mix >> 8) & 1u) ? AffinityTier::Like : AffinityTier::Dislike; // SlyBribe
        p.tiers[5] = ((mix >> 9) & 1u) ? AffinityTier::Like : AffinityTier::Dislike; // Plea

        switch (culture)
        {
        case ObserverCulture::Nord:
        case ObserverCulture::Orc:
            p.tiers[3] = PersuasionDetail::StepAffinity(p.tiers[3], +1); // Boast respect
            break;
        case ObserverCulture::Altmer:
            p.tiers[1] = PersuasionDetail::StepAffinity(p.tiers[1], -1); // Stiffer to casual Joke
            break;
        case ObserverCulture::Khajiit:
            p.tiers[1] = PersuasionDetail::StepAffinity(p.tiers[1], +1); // Joke
            p.tiers[4] = PersuasionDetail::StepAffinity(p.tiers[4], +1); // SlyBribe pragmatic
            break;
        case ObserverCulture::Dunmer:
            p.tiers[0] = PersuasionDetail::StepAffinity(p.tiers[0], -1); // Wary of easy flattery
            break;
        default:
            break;
        }

        if (isGuard)
        {
            p.tiers[2] = AffinityTier::Hate; // Open coercion hostility
            p.tiers[4] = ((mix >> 12) % 4u == 0u) ? AffinityTier::Like : AffinityTier::Dislike;
        }

        if (relTier >= RelationshipTier::Friend && relTier <= RelationshipTier::Follower)
        {
            p.tiers[0] = PersuasionDetail::StepAffinity(p.tiers[0], +1);
            p.tiers[5] = PersuasionDetail::StepAffinity(p.tiers[5], +1);
        }
        else if (relTier == RelationshipTier::Rival || relTier == RelationshipTier::Enemy)
        {
            p.tiers[0] = PersuasionDetail::StepAffinity(p.tiers[0], -1);
            p.tiers[1] = PersuasionDetail::StepAffinity(p.tiers[1], -1);
        }

        return p;
    }

    /// @brief Detects brief real-time physical cadences: Hand/Purse -> Eyes ("Knowing Look")
    ///        and Armed Eye Stare -> Sheathed Weapon ("Stand-Down Grace").
    class EyeHandEyeTracker
    {
    public:
        static constexpr float kHandWindowSec       = 1.8f;
        static constexpr float kWeaponGraceWinSec   = 2.5f;
        static constexpr float kCadenceValidHoldSec = 4.0f;

        void Observe(uint32_t actorFormId, uint8_t regionId, bool weaponDrawn,
                     float sessionElapsedSec) noexcept
        {
            if (actorFormId == 0u || !std::isfinite(sessionElapsedSec))
            {
                return;
            }
            if (actorFormId != _targetActorFormId)
            {
                _targetActorFormId = actorFormId;
                _lastHandSec = -100.0f;
                _lastArmedCoerceSec = -100.0f;
                _slyBribeUntilSec = -100.0f;
                _standDownUntilSec = -100.0f;
            }

            const bool isHandOrPurse =
                (regionId == Region::RightHand || regionId == Region::LeftHand);
            const bool isEyeOrBrow =
                (regionId == Region::LeftEye || regionId == Region::RightEye ||
                 regionId == Region::Forehead);

            if (isHandOrPurse)
            {
                _lastHandSec = sessionElapsedSec;
            }
            if (isEyeOrBrow && (sessionElapsedSec - _lastHandSec) <= kHandWindowSec)
            {
                _slyBribeUntilSec = sessionElapsedSec + kCadenceValidHoldSec;
            }

            if (weaponDrawn && (isEyeOrBrow || regionId == Region::Mouth))
            {
                _lastArmedCoerceSec = sessionElapsedSec;
                _standDownUntilSec = -100.0f;
            }
            else if (!weaponDrawn && (isEyeOrBrow || regionId == Region::Mouth) &&
                     (sessionElapsedSec - _lastArmedCoerceSec) <= kWeaponGraceWinSec)
            {
                _standDownUntilSec = sessionElapsedSec + kCadenceValidHoldSec;
            }
        }

        [[nodiscard]] bool IsSlyBribeCadenceActive(uint32_t actorFormId,
                                                   float sessionElapsedSec) const noexcept
        {
            return actorFormId != 0u && actorFormId == _targetActorFormId &&
                   sessionElapsedSec <= _slyBribeUntilSec;
        }

        [[nodiscard]] bool IsStandDownGraceActive(uint32_t actorFormId,
                                                  float sessionElapsedSec) const noexcept
        {
            return actorFormId != 0u && actorFormId == _targetActorFormId &&
                   sessionElapsedSec <= _standDownUntilSec;
        }

        void Reset() noexcept
        {
            _targetActorFormId = 0u;
            _lastHandSec = -100.0f;
            _lastArmedCoerceSec = -100.0f;
            _slyBribeUntilSec = -100.0f;
            _standDownUntilSec = -100.0f;
        }

    private:
        uint32_t _targetActorFormId{0};
        float _lastHandSec{-100.0f};
        float _lastArmedCoerceSec{-100.0f};
        float _slyBribeUntilSec{-100.0f};
        float _standDownUntilSec{-100.0f};
    };

    /// @brief Tracks recent per-NPC vector commits (45s novelty window) replacing turn-based rotation.
    class TransactionSaturationTracker
    {
    public:
        static constexpr float kWindowSec = 45.0f;
        static constexpr size_t kCapacity = 32u;

        [[nodiscard]] float NoveltyMultiplier(uint32_t npcFormId, PersuasionVector vec,
                                              float nowSec) const noexcept
        {
            if (npcFormId == 0u || vec == PersuasionVector::None)
            {
                return 1.0f;
            }
            for (const auto& s : _slots)
            {
                if (s.used && s.npcFormId == npcFormId && s.vector == vec &&
                    (nowSec - s.lastSec) < kWindowSec)
                {
                    if (s.repeatCount >= 2u) return 0.20f;
                    return 0.50f;
                }
            }
            return 1.0f;
        }

        void RecordCommit(uint32_t npcFormId, PersuasionVector vec, float nowSec) noexcept
        {
            if (npcFormId == 0u || vec == PersuasionVector::None || !std::isfinite(nowSec))
            {
                return;
            }
            Slot* target = nullptr;
            Slot* oldest = &_slots[0];
            for (auto& s : _slots)
            {
                if (s.used && s.npcFormId == npcFormId && s.vector == vec)
                {
                    target = &s;
                    break;
                }
                if (!s.used && !target) target = &s;
                if (s.used && (!oldest->used || s.seq < oldest->seq)) oldest = &s;
            }
            if (!target) target = oldest;

            if (target->used && target->npcFormId == npcFormId && target->vector == vec &&
                (nowSec - target->lastSec) < kWindowSec)
            {
                if (target->repeatCount < 250u) ++target->repeatCount;
            }
            else
            {
                target->repeatCount = 1u;
            }
            target->used = true;
            target->npcFormId = npcFormId;
            target->vector = vec;
            target->lastSec = nowSec;
            target->seq = _nextSeq++;
        }

        void Reset() noexcept
        {
            _slots = {};
            _nextSeq = 1u;
        }

    private:
        struct Slot
        {
            uint32_t npcFormId{0};
            PersuasionVector vector{PersuasionVector::None};
            uint8_t repeatCount{0};
            float lastSec{0.0f};
            uint64_t seq{0};
            bool used{false};
        };

        std::array<Slot, kCapacity> _slots{};
        uint64_t _nextSeq{1u};
    };

    /// @brief Resolves which Oblivion/Expanded social vector the player is signaling via 3D aim + posture.
    [[nodiscard]] constexpr PersuasionVector ClassifyPersuasionVector(
        uint8_t regionId, Demeanor demeanor, bool playerWeaponDrawn, bool isChild,
        bool isThane, PresenceTier presenceTier, bool compassionContext, bool emblemContext,
        bool slyBribeCadence, bool standDownGrace) noexcept
    {
        if (!isChild && slyBribeCadence &&
            (regionId == Region::LeftEye || regionId == Region::RightEye ||
             regionId == Region::Forehead))
        {
            return PersuasionVector::SlyBribe;
        }
        if (compassionContext &&
            (regionId == Region::LeftEye || regionId == Region::RightEye ||
             regionId == Region::Chin || regionId == Region::Ground ||
             regionId == Region::Torso))
        {
            return PersuasionVector::Plea;
        }
        if (standDownGrace)
        {
            return PersuasionVector::Admire;
        }
        if (!isChild && (playerWeaponDrawn || demeanor == Demeanor::Predatory ||
                         (demeanor == Demeanor::Stern && !isThane &&
                          presenceTier < PresenceTier::Commanding)) &&
            (regionId == Region::LeftEye || regionId == Region::RightEye ||
             regionId == Region::Forehead))
        {
            return PersuasionVector::Coerce;
        }
        if (demeanor == Demeanor::Stern &&
            (isThane || presenceTier >= PresenceTier::Steady) &&
            (regionId == Region::LeftEye || regionId == Region::RightEye ||
             regionId == Region::Forehead))
        {
            return PersuasionVector::Boast;
        }
        if (demeanor == Demeanor::Deferential &&
            (regionId == Region::Chin || regionId == Region::Ground))
        {
            return PersuasionVector::Plea;
        }
        if (regionId == Region::Mouth &&
            (demeanor == Demeanor::Warm || demeanor == Demeanor::Courteous))
        {
            return PersuasionVector::Joke;
        }
        if (emblemContext && regionId == Region::Torso)
        {
            return PersuasionVector::Admire;
        }
        if (regionId == Region::LeftEye || regionId == Region::RightEye ||
            regionId == Region::Forehead)
        {
            if (demeanor == Demeanor::Warm)
            {
                return PersuasionVector::Joke;
            }
            return PersuasionVector::Admire;
        }
        return PersuasionVector::None;
    }

    struct ProbeTellResult
    {
        bool active{false};
        PersuasionVector vector{PersuasionVector::None};
        AffinityTier affinity{AffinityTier::Like};
        bool guardGlanceInterrupt{false};
        bool applyModeOverride{false};
        uint8_t hcepMode{0};
        bool lockMutualReciprocity{false};
        bool forceAversion{false};
        FaceExpressionMood morphMood{FaceExpressionMood::Neutral};
        float morphWeight{0.0f};
        uint32_t whisperColorRgb{0xE6D8B8u};
        bool suggestWhisper{false};
    };

    /// @brief Phase 1 (0.35s..1.99s) Pre-Commit Expression & Guard-Glance Probe.
    ///        Telegraphs Love/Like/Dislike/Hate visually so the player can abort cleanly < 2.0s.
    [[nodiscard]] inline ProbeTellResult EvaluateProbeTell(
        PersuasionVector vector, const NpcAffinityProfile& profile, float dwellSec,
        uint32_t lawWitnessCount, bool isChild) noexcept
    {
        ProbeTellResult out{};
        if (vector == PersuasionVector::None || !std::isfinite(dwellSec) ||
            dwellSec < 0.35f || dwellSec >= 2.00f)
        {
            return out;
        }

        out.active = true;
        out.vector = vector;
        out.affinity = profile.ForVector(vector);

        if (isChild &&
            (vector == PersuasionVector::Coerce || vector == PersuasionVector::SlyBribe))
        {
            out.affinity = AffinityTier::Hate;
        }

        if (vector == PersuasionVector::SlyBribe && lawWitnessCount > 0u && !isChild)
        {
            out.guardGlanceInterrupt = true;
            out.applyModeOverride = true;
            out.hcepMode = 4u; // THINK / peripheral glance away
            out.forceAversion = true;
            out.morphMood = FaceExpressionMood::Fear;
            out.morphWeight = 0.30f;
            out.whisperColorRgb = 0xFFCC66u;
            out.suggestWhisper = true;
            return out;
        }

        out.applyModeOverride = true;
        switch (out.affinity)
        {
        case AffinityTier::Love:
            out.hcepMode = 3u; // HEART
            out.lockMutualReciprocity = true;
            out.morphMood = FaceExpressionMood::Happy;
            out.morphWeight = 0.34f;
            out.whisperColorRgb = 0xFFD966u;
            break;
        case AffinityTier::Like:
            out.hcepMode = 1u; // AFFECT
            out.lockMutualReciprocity = true;
            out.morphMood = FaceExpressionMood::Happy;
            out.morphWeight = 0.20f;
            out.whisperColorRgb = 0xE6D8B8u;
            break;
        case AffinityTier::Dislike:
            out.hcepMode = 4u; // THINK avert flicker
            out.forceAversion = true;
            out.morphMood = FaceExpressionMood::Puzzled;
            out.morphWeight = 0.24f;
            out.whisperColorRgb = 0xFFCC66u;
            break;
        case AffinityTier::Hate:
        default:
            out.hcepMode = 4u;
            out.forceAversion = true;
            out.morphMood =
                (vector == PersuasionVector::Coerce) ? FaceExpressionMood::Anger
                                                     : FaceExpressionMood::Disgusted;
            out.morphWeight = 0.38f;
            out.whisperColorRgb = 0xFF6666u;
            out.suggestWhisper = true;
            break;
        }
        return out;
    }

    struct TransactionInput
    {
        PersuasionVector vector{PersuasionVector::None};
        AffinityTier affinity{AffinityTier::Like};
        float dwellSec{0.0f};
        bool standDownGrace{false};
        bool isChild{false};
        bool isMutual{false};

        // Oblivion-grounded skill / level / risk parameters
        float playerSpeechcraft{15.0f};
        float npcSpeechcraft{20.0f};
        int32_t playerLevel{1};
        int32_t npcLevel{4};
        float presenceMax{20.0f};
        ScholarshipRank scholarshipRank{ScholarshipRank::Novice};
        RelationshipTier relTier{RelationshipTier::Stranger};
        const Impressions* currentImpressions{nullptr};

        uint32_t neutralWitnessCount{0};
        uint32_t lawWitnessCount{0};
        float noveltyMultiplier{1.0f};
        float baseBribeGold{60.0f};
    };

    struct TransactionOutcome
    {
        bool committed{false};
        bool blockedByWitness{false};
        bool blockedByChildGate{false};
        TransactionType type{TransactionType::None};
        int32_t computedBribeCostGold{0};
        float speechBarterDelta{0.0f}; // Reversible ActorValue::kSpeechcraftModifier contribution
        float patienceWindowSec{6.0f}; // Damped by Presence & Moth's Stillness
        float presenceDelta{0.0f};
        Impressions ledgerDelta{};
    };

    /// @brief Applies TES IV Oblivion GMST cost/yield curves (fBribeCostCurve=1.5, fBribeCurve=0.75,
    ///        fBribeSpeechcraftMult=0.03, fBribeNPCLevelMult=0.10) generalized across TrueGaze.
    [[nodiscard]] inline TransactionOutcome EvaluateTransactionCommit(
        const TransactionInput& in) noexcept
    {
        TransactionOutcome out{};

        // Oblivion Construction Set GMST defaults:
        constexpr float kBribeScale          = 1.00f;
        constexpr float kBribeCostCurve      = 1.50f;
        constexpr float kBribeSpeechMult     = 0.03f;
        constexpr float kBribeNpcLevelMult   = 0.10f;
        constexpr float kBribeYieldCurve     = 0.75f;

        // Patience window scaled by Moth's Stillness (Oblivion Journeyman analog) & Presence
        const float rankBonus = static_cast<float>(static_cast<int>(in.scholarshipRank));
        out.patienceWindowSec =
            std::clamp(6.0f + 0.5f * rankBonus + 0.02f * std::max(0.0f, in.presenceMax - 20.0f),
                       6.0f, 10.5f);

        if (in.vector == PersuasionVector::None || !std::isfinite(in.dwellSec) ||
            in.dwellSec < 2.00f)
        {
            return out;
        }

        if (in.isChild &&
            (in.vector == PersuasionVector::Coerce || in.vector == PersuasionVector::SlyBribe))
        {
            out.blockedByChildGate = true;
            return out;
        }

        if (in.vector == PersuasionVector::SlyBribe && in.lawWitnessCount > 0u)
        {
            out.blockedByWitness = true;
            out.ledgerDelta.suspicion = 2.0f;
            return out;
        }

        const auto scholarBonus = ComputeScholarshipBonus(in.scholarshipRank);
        const float effectiveSkill =
            std::clamp(in.playerSpeechcraft + 0.35f * in.presenceMax +
                           scholarBonus.speechIllusionBonus,
                       1.0f, 200.0f);

        const float levelDiff = static_cast<float>(in.npcLevel - in.playerLevel) / 10.0f;
        const float speechDiff = in.npcSpeechcraft - effectiveSkill;
        const float disparityTerm =
            std::clamp(1.0f + kBribeNpcLevelMult * levelDiff + kBribeSpeechMult * speechDiff,
                       0.25f, 3.0f);

        float relScalar = 1.0f;
        if (in.relTier >= RelationshipTier::Friend && in.relTier <= RelationshipTier::Follower)
        {
            relScalar = 0.80f;
        }
        else if (in.relTier == RelationshipTier::Rival || in.relTier == RelationshipTier::Enemy)
        {
            relScalar = 1.45f;
        }
        if (in.currentImpressions)
        {
            if (in.currentImpressions->warmth > 15.0f || in.currentImpressions->respect > 15.0f)
            {
                relScalar *= 0.88f;
            }
            if (in.currentImpressions->suspicion > 30.0f || in.currentImpressions->unease > 35.0f)
            {
                relScalar *= 1.35f;
            }
        }

        const float witnessRisk =
            1.0f + 0.50f * static_cast<float>(in.neutralWitnessCount) +
            1.25f * static_cast<float>(in.lawWitnessCount);
        const float masterHalfCost =
            (in.scholarshipRank >= ScholarshipRank::Canticle || in.playerSpeechcraft >= 100.0f)
                ? 0.50f
                : 1.0f;

        const float rawCost = std::max(10.0f, in.baseBribeGold) * kBribeScale *
                              std::pow(disparityTerm, kBribeCostCurve) * relScalar * witnessRisk *
                              masterHalfCost;
        out.computedBribeCostGold = std::clamp(static_cast<int32_t>(std::lround(rawCost)), 5, 5000);

        // Oblivion Wedge magnitude mapped to TrueGaze Dwell ticks (Tick 2..4)
        float wedgeScalar = 1.0f;
        if (in.dwellSec >= 9.00f)
        {
            wedgeScalar = 2.25f;
        }
        else if (in.dwellSec >= 4.50f)
        {
            wedgeScalar = 1.65f;
        }
        const float curvedWedge = std::pow(wedgeScalar / 1.0f, kBribeYieldCurve);

        float affWeight = 1.0f;
        switch (in.affinity)
        {
        case AffinityTier::Love: affWeight = 1.60f; break;
        case AffinityTier::Like: affWeight = 1.00f; break;
        case AffinityTier::Dislike: affWeight = -0.80f; break;
        case AffinityTier::Hate: affWeight = -1.50f; break;
        }

        const float novelty = std::clamp(in.noveltyMultiplier, 0.10f, 1.0f);
        const float gainMag = 1.8f * curvedWedge * affWeight * novelty;
        const float mutualFactor = in.isMutual ? 1.15f : 0.90f;
        const float netDelta = gainMag * mutualFactor;

        out.committed = true;

        if (in.standDownGrace && in.vector == PersuasionVector::Admire)
        {
            out.type = TransactionType::StandDownGrace;
            out.ledgerDelta.respect = 2.5f;
            out.ledgerDelta.warmth = 1.8f;
            out.ledgerDelta.fear = -2.0f;
            out.speechBarterDelta = 6.0f;
            out.presenceDelta = 0.25f;
            return out;
        }

        switch (in.vector)
        {
        case PersuasionVector::SlyBribe:
            out.type = TransactionType::SlyBribePrime;
            out.ledgerDelta.suspicion = 0.8f;
            out.ledgerDelta.respect = (in.affinity > AffinityTier::Dislike) ? 1.2f : -0.6f;
            out.ledgerDelta.unease = (in.affinity > AffinityTier::Dislike) ? -1.0f : 1.5f;
            out.speechBarterDelta =
                (in.affinity > AffinityTier::Dislike) ? std::clamp(6.0f * curvedWedge, 2.0f, 12.0f)
                                                      : -3.0f;
            break;

        case PersuasionVector::Coerce:
            out.type = TransactionType::LeverageCoerce;
            out.ledgerDelta.fear = std::max(0.4f, 1.6f * curvedWedge * novelty);
            out.ledgerDelta.respect = (netDelta >= 0.0f) ? netDelta * 0.7f : 0.0f;
            out.ledgerDelta.unease = (netDelta < 0.0f) ? std::abs(netDelta) * 0.9f : 0.3f;
            out.speechBarterDelta =
                (in.affinity > AffinityTier::Dislike)
                    ? std::clamp(4.0f * curvedWedge * novelty, 1.0f, 10.0f)
                    : std::clamp(-4.0f * curvedWedge, -8.0f, -1.0f);
            out.presenceDelta = (in.affinity > AffinityTier::Dislike) ? 0.15f : -0.10f;
            break;

        case PersuasionVector::Boast:
            out.type = TransactionType::PrestigeBoast;
            out.ledgerDelta.respect = (netDelta >= 0.0f) ? netDelta : 0.0f;
            out.ledgerDelta.unease = (netDelta < 0.0f) ? std::abs(netDelta) : 0.0f;
            out.speechBarterDelta = std::clamp(netDelta * 2.2f, -6.0f, 11.0f);
            out.presenceDelta = (netDelta >= 0.0f) ? 0.15f : 0.0f;
            break;

        case PersuasionVector::Joke:
        case PersuasionVector::Admire:
            out.type = TransactionType::RapportBarter;
            if (netDelta >= 0.0f)
            {
                out.ledgerDelta.warmth = netDelta * 0.85f;
                out.ledgerDelta.respect = netDelta * 0.45f;
            }
            else
            {
                out.ledgerDelta.unease = std::abs(netDelta) * 0.75f;
            }
            out.speechBarterDelta = std::clamp(netDelta * 2.5f, -7.0f, 12.0f);
            out.presenceDelta = (netDelta >= 0.0f) ? 0.10f : 0.0f;
            break;

        case PersuasionVector::Plea:
            out.type = TransactionType::SeenCompassion;
            out.ledgerDelta.warmth = (netDelta >= 0.0f) ? netDelta : 0.25f;
            out.ledgerDelta.unease = -0.8f;
            out.speechBarterDelta = std::clamp(std::max(1.5f, netDelta * 2.0f), 1.0f, 10.0f);
            out.presenceDelta = 0.20f;
            break;

        case PersuasionVector::None:
        default:
            out.committed = false;
            break;
        }

        return out;
    }

    [[nodiscard]] inline std::string FormatProbeWhisperText(std::string_view actorName,
                                                            const ProbeTellResult& probe)
    {
        const std::string_view who = actorName.empty() ? "They" : actorName;
        if (probe.guardGlanceInterrupt)
        {
            return std::format("{}'s eyes flick toward the hold guard—not while watched.", who);
        }
        if (probe.affinity == AffinityTier::Love)
        {
            switch (probe.vector)
            {
            case PersuasionVector::Admire:
                return std::format("{} brightens at your courteous appraisal.", who);
            case PersuasionVector::Joke:
                return std::format("{} warms readily to your light-hearted look.", who);
            case PersuasionVector::Boast:
                return std::format("{} respects the quiet weight of your bearing.", who);
            case PersuasionVector::Coerce:
                return std::format("{} respects a hard, unflinching stare.", who);
            case PersuasionVector::SlyBribe:
                return std::format("{} catches the quiet glint of coin in your look.", who);
            case PersuasionVector::Plea:
                return std::format("{} softens before your earnest plea.", who);
            default:
                return std::format("{} warms noticeably to your gaze.", who);
            }
        }
        if (probe.affinity == AffinityTier::Hate)
        {
            switch (probe.vector)
            {
            case PersuasionVector::Admire:
                return std::format("{} stiffens at what feels like empty flattery.", who);
            case PersuasionVector::Joke:
                return std::format("{} has no patience for levity right now.", who);
            case PersuasionVector::Boast:
                return std::format("{} bristles at your proud bearing.", who);
            case PersuasionVector::Coerce:
                return std::format("{}'s jaw hardens against your threat.", who);
            case PersuasionVector::SlyBribe:
                return std::format("{} recoils from the hint of a quiet bribe.", who);
            case PersuasionVector::Plea:
                return std::format("{} turns cold to any show of pleading.", who);
            default:
                return std::format("{} bristles at your look.", who);
            }
        }
        return {};
    }
} // namespace TrueGaze::ExpandedPlay
