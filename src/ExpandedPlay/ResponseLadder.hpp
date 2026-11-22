#pragma once

#include "ExpandedPlay/ExpandedPlayTypes.hpp"
#include "ExpandedPlay/InterpretationEngine.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>

namespace TrueGaze::ExpandedPlay
{
    /// @brief Mirrors RE::BSFaceGenKeyframeMultiple::Expression mood indices for SDK-free logic.
    enum class FaceExpressionMood : uint8_t
    {
        Neutral   = 7,
        Anger     = 8,
        Fear      = 9,
        Happy     = 10,
        Sad       = 11,
        Surprise  = 12,
        Puzzled   = 13,
        Disgusted = 14
    };

    struct ResponseRung1Eyes
    {
        bool applyModeOverride{false};
        uint8_t hcepMode{0}; // 0=LOGIC, 1=AFFECT, 2=SPIRIT, 3=HEART, 4=THINK
        float overrideDurationSec{0.0f};
        bool lockMutualReciprocity{false};
        bool forceAversion{false};
        uint8_t aversionRegionId{Region::LRPeriph};
    };

    struct ResponseRung2Face
    {
        bool applyExpression{false};
        FaceExpressionMood mood{FaceExpressionMood::Neutral};
        float weight{0.0f}; // Gently clamped to [0.0, 0.50]
        float durationSec{0.0f};
    };

    struct ResponseRung3Oar
    {
        uint8_t interpretation{0};
        uint8_t demeanor{0};
        uint8_t presenceTier{0};
        float dominantImpression{0.0f};
    };

    struct ResponseRung4Broadcast
    {
        bool emitWhisper{false};
        uint32_t whisperColorRgb{0xE6D8B8u};
        bool emitModEvent{false};
        bool emitAiNpcBridge{false};
    };

    struct ResponseDecision
    {
        ResponseRung1Eyes rung1{};
        ResponseRung2Face rung2{};
        ResponseRung3Oar rung3{};
        ResponseRung4Broadcast rung4{};
    };

    /// @brief Selects the strongest signed dimension from a GazeLedger entry [-100..+100].
    [[nodiscard]] inline float DominantImpressionScore(const Impressions& imp) noexcept
    {
        const std::array<float, 6> signedDims{
            imp.respect, imp.warmth, imp.attraction, -imp.unease, -imp.suspicion, -imp.fear};
        float best = 0.0f;
        float bestAbs = 0.0f;
        for (float v : signedDims)
        {
            if (!std::isfinite(v))
            {
                continue;
            }
            const float absV = std::abs(v);
            if (absV > bestAbs)
            {
                bestAbs = absV;
                best = v;
            }
        }
        return std::clamp(best, -100.0f, 100.0f);
    }

    /// @brief Pure 4-rung social response planner driven by InterpretationEngine & GazeLedger.
    [[nodiscard]] inline ResponseDecision EvaluateResponseLadder(
        const InterpretationResult& interpResult, Demeanor demeanor, PresenceTier presenceTier,
        const Impressions& ledgerImpressions, bool socialModuleEnabled, bool whispersEnabled,
        bool aiBridgeEnabled, bool cooldownReady) noexcept
    {
        ResponseDecision out{};
        out.rung3.interpretation = static_cast<uint8_t>(interpResult.interpretation);
        out.rung3.demeanor = static_cast<uint8_t>(demeanor);
        out.rung3.presenceTier = static_cast<uint8_t>(presenceTier);
        out.rung3.dominantImpression = DominantImpressionScore(ledgerImpressions);

        if (!socialModuleEnabled || interpResult.interpretation == Interpretation::None ||
            !std::isfinite(interpResult.strength) || interpResult.strength <= 0.05f)
        {
            return out;
        }

        const float s = std::clamp(interpResult.strength, 0.0f, 1.0f);
        switch (interpResult.interpretation)
        {
        case Interpretation::Warmth:
        case Interpretation::Attraction:
            out.rung1.applyModeOverride = true;
            out.rung1.hcepMode = (interpResult.interpretation == Interpretation::Attraction) ? 3u : 1u;
            out.rung1.overrideDurationSec = 2.0f;
            out.rung1.lockMutualReciprocity = true;
            out.rung2.applyExpression = true;
            out.rung2.mood = FaceExpressionMood::Happy;
            out.rung2.weight = std::clamp(0.18f + 0.25f * s, 0.15f, 0.45f);
            out.rung2.durationSec = 2.5f;
            out.rung4.whisperColorRgb = 0xFFD966u;
            break;

        case Interpretation::Compassion:
            out.rung1.applyModeOverride = true;
            out.rung1.hcepMode = 3u;
            out.rung1.overrideDurationSec = 2.5f;
            out.rung1.lockMutualReciprocity = true;
            out.rung2.applyExpression = true;
            out.rung2.mood = FaceExpressionMood::Sad;
            out.rung2.weight = std::clamp(0.18f + 0.20f * s, 0.15f, 0.38f);
            out.rung2.durationSec = 2.5f;
            out.rung4.whisperColorRgb = 0xA6D8FFu;
            break;

        case Interpretation::Respect:
        case Interpretation::Recognition:
            out.rung1.applyModeOverride = true;
            out.rung1.hcepMode =
                (interpResult.interpretation == Interpretation::Recognition) ? 2u : 0u;
            out.rung1.overrideDurationSec = 2.0f;
            out.rung1.lockMutualReciprocity = true;
            out.rung2.applyExpression = true;
            out.rung2.mood = FaceExpressionMood::Happy;
            out.rung2.weight = std::clamp(0.12f + 0.15f * s, 0.10f, 0.28f);
            out.rung2.durationSec = 2.0f;
            out.rung4.whisperColorRgb = 0xE6D8B8u;
            break;

        case Interpretation::Challenge:
            out.rung1.applyModeOverride = true;
            out.rung1.hcepMode = 0u;
            out.rung1.overrideDurationSec = 2.5f;
            out.rung1.lockMutualReciprocity = true;
            out.rung2.applyExpression = true;
            out.rung2.mood = FaceExpressionMood::Anger;
            out.rung2.weight = std::clamp(0.22f + 0.25f * s, 0.20f, 0.48f);
            out.rung2.durationSec = 2.5f;
            out.rung4.whisperColorRgb = 0xFF9966u;
            break;

        case Interpretation::IntrusionLeer:
            out.rung1.applyModeOverride = true;
            out.rung1.hcepMode = 4u;
            out.rung1.overrideDurationSec = 3.0f;
            out.rung1.forceAversion = true;
            out.rung1.aversionRegionId = Region::LRPeriph;
            out.rung2.applyExpression = true;
            out.rung2.mood = FaceExpressionMood::Disgusted;
            out.rung2.weight = std::clamp(0.28f + 0.22f * s, 0.25f, 0.50f);
            out.rung2.durationSec = 3.0f;
            out.rung4.whisperColorRgb = 0xFF6666u;
            break;

        case Interpretation::Suspicion:
        case Interpretation::Deceit:
            out.rung1.applyModeOverride = true;
            out.rung1.hcepMode = 4u;
            out.rung1.overrideDurationSec = 2.5f;
            out.rung2.applyExpression = true;
            out.rung2.mood = FaceExpressionMood::Puzzled;
            out.rung2.weight = std::clamp(0.20f + 0.22f * s, 0.18f, 0.42f);
            out.rung2.durationSec = 2.5f;
            out.rung4.whisperColorRgb = 0xFFCC66u;
            break;

        case Interpretation::Deference:
            out.rung1.applyModeOverride = true;
            out.rung1.hcepMode = 2u;
            out.rung1.overrideDurationSec = 2.0f;
            out.rung1.forceAversion = true;
            out.rung1.aversionRegionId = Region::LRPeriph;
            out.rung2.applyExpression = true;
            out.rung2.mood = FaceExpressionMood::Neutral;
            out.rung2.weight = 0.15f;
            out.rung2.durationSec = 2.0f;
            out.rung4.whisperColorRgb = 0xB8D4E6u;
            break;

        case Interpretation::Contempt:
            out.rung1.applyModeOverride = true;
            out.rung1.hcepMode = 4u;
            out.rung1.overrideDurationSec = 2.5f;
            out.rung1.forceAversion = true;
            out.rung1.aversionRegionId = Region::LLPeriph;
            out.rung2.applyExpression = true;
            out.rung2.mood = FaceExpressionMood::Disgusted;
            out.rung2.weight = std::clamp(0.20f + 0.20f * s, 0.18f, 0.40f);
            out.rung2.durationSec = 2.5f;
            out.rung4.whisperColorRgb = 0xA6B3CCu;
            break;

        case Interpretation::None:
        default:
            return out;
        }

        if (cooldownReady && interpResult.band >= DwellBand::Gaze && s >= 0.35f)
        {
            out.rung4.emitWhisper = whispersEnabled;
            out.rung4.emitModEvent = true;
            out.rung4.emitAiNpcBridge = aiBridgeEnabled;
        }
        return out;
    }

    /// @brief Fixed-capacity per-NPC cooldown guard for Rung-4 barks/whispers/events (>= 20s).
    class NpcCooldownTracker
    {
    public:
        static constexpr float kCooldownSec = 20.0f;
        static constexpr size_t kCapacity = 64u;

        [[nodiscard]] bool IsReady(uint32_t npcFormId, float nowSec,
                                   float cooldownSec = kCooldownSec) const noexcept
        {
            if (npcFormId == 0u || !std::isfinite(nowSec))
            {
                return false;
            }
            for (const auto& s : _slots)
            {
                if (s.used && s.npcFormId == npcFormId)
                {
                    return (nowSec - s.lastTriggerSec) >= cooldownSec;
                }
            }
            return true;
        }

        bool TryConsume(uint32_t npcFormId, float nowSec,
                        float cooldownSec = kCooldownSec) noexcept
        {
            if (!IsReady(npcFormId, nowSec, cooldownSec))
            {
                return false;
            }

            Slot* target = nullptr;
            Slot* oldest = &_slots[0];
            for (auto& s : _slots)
            {
                if (s.used && s.npcFormId == npcFormId)
                {
                    target = &s;
                    break;
                }
                if (!s.used && !target)
                {
                    target = &s;
                }
                if (s.used && (!oldest->used || s.seq < oldest->seq))
                {
                    oldest = &s;
                }
            }
            if (!target)
            {
                target = oldest;
            }

            target->used = true;
            target->npcFormId = npcFormId;
            target->lastTriggerSec = nowSec;
            target->seq = _nextSeq++;
            return true;
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
            float lastTriggerSec{0.0f};
            uint64_t seq{0};
            bool used{false};
        };

        std::array<Slot, kCapacity> _slots{};
        uint64_t _nextSeq{1u};
    };

    [[nodiscard]] inline std::string_view ShortRegionLabel(uint8_t regionId) noexcept
    {
        switch (regionId)
        {
        case Region::LeftEye: return "LeftEye";
        case Region::RightEye: return "RightEye";
        case Region::Mouth: return "Mouth";
        case Region::Forehead: return "Forehead";
        case Region::Chin: return "Chin";
        case Region::Torso: return "Torso";
        case Region::RightHand: return "RightHand";
        case Region::LeftHand: return "LeftHand";
        case Region::Ground: return "Ground";
        case Region::ULPeriph: return "UpperLeft";
        case Region::URPeriph: return "UpperRight";
        case Region::LLPeriph: return "LowerLeft";
        case Region::LRPeriph: return "LowerRight";
        default: return "Gaze";
        }
    }

    [[nodiscard]] inline std::string FormatWhisperText(std::string_view actorName,
                                                       Interpretation interp)
    {
        const std::string_view who = actorName.empty() ? "They" : actorName;
        switch (interp)
        {
        case Interpretation::Warmth:
            return std::format("{} softens under your warmth.", who);
        case Interpretation::Attraction:
            return std::format("{} lingers on your quiet smile.", who);
        case Interpretation::Respect:
            return std::format("{} weighs your steady eyes with respect.", who);
        case Interpretation::Recognition:
            return std::format("{} knows your gaze from before.", who);
        case Interpretation::Challenge:
            return std::format("{} answers your stare without flinching.", who);
        case Interpretation::IntrusionLeer:
            return std::format("{} draws back from your intrusive stare.", who);
        case Interpretation::Suspicion:
            return std::format("{} narrows an eye at your restless glance.", who);
        case Interpretation::Deference:
            return std::format("{} acknowledges your quiet deference.", who);
        case Interpretation::Contempt:
            return std::format("{} bristles under your cold dismissal.", who);
        case Interpretation::Compassion:
            return std::format("{} senses Mara's mercy in your look.", who);
        case Interpretation::Deceit:
            return std::format("{} catches your eyes darting aside.", who);
        case Interpretation::None:
        default:
            return {};
        }
    }

    [[nodiscard]] inline std::string FormatAiNpcBridgeContext(std::string_view actorName,
                                                              uint8_t regionId,
                                                              Interpretation interp,
                                                              DwellBand band, float dwellSec,
                                                              Demeanor demeanor)
    {
        const std::string_view who = actorName.empty() ? "NPC" : actorName;
        const float safeDwell =
            (std::isfinite(dwellSec) && dwellSec > 0.0f) ? dwellSec : 0.0f;
        return std::format(
            "[TrueGaze] During a {} encounter, the Dragonborn held a {} on {}'s {} for {:.1f}s "
            "({}).",
            DemeanorName(demeanor), DwellBandName(band), who, ShortRegionLabel(regionId),
            safeDwell, InterpretationName(interp));
    }

    struct EyeContactGreetingInput
    {
        bool enabled{false};
        bool featureToggle{false};
        PresenceTier presenceTier{PresenceTier::Unnoticed};
        bool inTutorialInn{false};
        bool isMutual{false};
        Interpretation interpretation{Interpretation::None};
        float dwellSec{0.0f};
        float distanceMeters{0.0f};
        bool inCombat{false};
        bool inDialogue{false};
        bool inScene{false};
        bool targetIsChild{false};
        bool playerWeaponDrawn{false};
        bool cooldownReady{false};
    };

    /// @brief True when sustained mutual eye contact cleanly triggers NPC greeting activation.
    [[nodiscard]] constexpr bool ShouldInitiateEyeContactGreeting(
        const EyeContactGreetingInput& in) noexcept
    {
        if (!in.enabled || !in.featureToggle || !in.isMutual || in.inCombat || in.inDialogue ||
            in.inScene || in.targetIsChild || in.playerWeaponDrawn || !in.cooldownReady)
        {
            return false;
        }
        if (in.presenceTier < PresenceTier::Noticed && !in.inTutorialInn)
        {
            return false;
        }
        if (in.interpretation != Interpretation::Respect &&
            in.interpretation != Interpretation::Warmth)
        {
            return false;
        }
        if (in.dwellSec < 2.0f || in.distanceMeters <= 0.0f || in.distanceMeters > 4.5f)
        {
            return false;
        }
        return true;
    }

    enum class ContestStatus : uint8_t
    {
        Inactive       = 0,
        InProgress     = 1,
        Won            = 2,
        LostLookedAway = 3,
        LostBlinked    = 4,
        Aborted        = 5
    };

    /// @brief Escalating 3-round tavern staring contest state machine (5s -> 10s -> 15s).
    class StaringContestController
    {
    public:
        static constexpr float kRound1EndSec      = 5.0f;
        static constexpr float kRound2EndSec      = 10.0f;
        static constexpr float kTotalDurationSec  = 15.0f;
        static constexpr float kLookAwayGraceSec  = 0.45f;

        void Start(uint32_t opponentFormId) noexcept
        {
            if (opponentFormId == 0u)
            {
                return;
            }
            _opponentFormId = opponentFormId;
            _elapsedSec = 0.0f;
            _awayGraceSec = 0.0f;
            _blinkPressure01 = 0.0f;
            _round = 1u;
            _status = ContestStatus::InProgress;
        }

        void Abort() noexcept
        {
            _status = IsActive() ? ContestStatus::Aborted : ContestStatus::Inactive;
            _opponentFormId = 0u;
            _elapsedSec = 0.0f;
            _awayGraceSec = 0.0f;
            _blinkPressure01 = 0.0f;
            _round = 0u;
        }

        void Reset() noexcept
        {
            _status = ContestStatus::Inactive;
            _opponentFormId = 0u;
            _elapsedSec = 0.0f;
            _awayGraceSec = 0.0f;
            _blinkPressure01 = 0.0f;
            _round = 0u;
        }

        [[nodiscard]] bool IsActive() const noexcept
        {
            return _status == ContestStatus::InProgress;
        }
        [[nodiscard]] uint32_t OpponentFormId() const noexcept { return _opponentFormId; }
        [[nodiscard]] uint8_t CurrentRound() const noexcept { return _round; }
        [[nodiscard]] float ElapsedSec() const noexcept { return _elapsedSec; }
        [[nodiscard]] float BlinkPressure01() const noexcept { return _blinkPressure01; }
        [[nodiscard]] ContestStatus Status() const noexcept { return _status; }

        ContestStatus Tick(float deltaSec, bool playerOnOpponentEyes, bool playerBlinkedThisTick,
                           float playerPresenceMax) noexcept
        {
            if (!IsActive())
            {
                return _status;
            }

            const float dt = (std::isfinite(deltaSec) && deltaSec > 0.0f) ? deltaSec : 0.0f;
            const float presence =
                std::isfinite(playerPresenceMax) ? std::clamp(playerPresenceMax, 0.0f, 100.0f) : 20.0f;
            const float presenceRelief = std::clamp((presence - 20.0f) / 80.0f, 0.0f, 0.40f);

            if (!playerOnOpponentEyes)
            {
                _awayGraceSec += dt;
                if (_awayGraceSec >= kLookAwayGraceSec)
                {
                    _status = ContestStatus::LostLookedAway;
                    return _status;
                }
                return _status;
            }

            _awayGraceSec = 0.0f;
            _elapsedSec += dt;
            if (_elapsedSec >= kRound2EndSec)
            {
                _round = 3u;
            }
            else if (_elapsedSec >= kRound1EndSec)
            {
                _round = 2u;
            }
            else
            {
                _round = 1u;
            }

            const float roundRate =
                (_round == 1u) ? 0.045f : ((_round == 2u) ? 0.060f : 0.075f);
            _blinkPressure01 =
                std::clamp(_blinkPressure01 + roundRate * (1.0f - presenceRelief) * dt, 0.0f, 1.0f);

            if (playerBlinkedThisTick || _blinkPressure01 >= 1.0f)
            {
                _status = ContestStatus::LostBlinked;
                return _status;
            }

            if (_elapsedSec >= kTotalDurationSec)
            {
                _status = ContestStatus::Won;
                return _status;
            }

            return _status;
        }

    private:
        uint32_t _opponentFormId{0};
        float _elapsedSec{0.0f};
        float _awayGraceSec{0.0f};
        float _blinkPressure01{0.0f};
        uint8_t _round{0};
        ContestStatus _status{ContestStatus::Inactive};
    };

    /// @brief Game-side dispatcher applying Rungs 1-4 and Eye-Contact Greetings.
    class ResponseLadder
    {
    public:
        static void ApplyDecision(uint32_t actorFormId, std::string_view actorName,
                                  uint8_t regionId, float dwellSec,
                                  const InterpretationResult& interpResult,
                                  const ResponseDecision& decision,
                                  float nowSessionSec = 0.0f) noexcept;

        static void ApplyTransientExpression(uint32_t actorFormId, FaceExpressionMood mood,
                                             float weight, float durationSec,
                                             float nowSessionSec) noexcept;

        static void TickExpressionResets(float nowSessionSec, float deltaSec,
                                         uint32_t activeFocusActorFormId) noexcept;

        static void ClearAllTransientExpressions() noexcept;

        static bool TryInitiateEyeContactGreeting(uint32_t actorFormId,
                                                  std::string_view actorName,
                                                  bool whispersEnabled) noexcept;
    };
} // namespace TrueGaze::ExpandedPlay
