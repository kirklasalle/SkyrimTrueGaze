#pragma once

#include "ExpandedPlay/CoSaveSerializer.hpp"
#include "ExpandedPlay/ExpandedPlayModifiers.hpp"
#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace TrueGaze::ExpandedPlay
{
    enum class SignatureTitle : uint8_t
    {
        None          = 0,
        KindEyes      = 1,
        MarasOwn      = 2,
        IceGaze       = 3,
        TheUnblinking = 4,
        SteadyEyed    = 5,
        QuickEyes     = 6,
        WanderingEyes = 7,
        GentleSoul    = 8
    };

    [[nodiscard]] constexpr const char* SignatureTitleName(SignatureTitle title) noexcept
    {
        switch (title)
        {
        case SignatureTitle::KindEyes: return "Kind Eyes";
        case SignatureTitle::MarasOwn: return "Mara's Own";
        case SignatureTitle::IceGaze: return "Ice-Gaze";
        case SignatureTitle::TheUnblinking: return "The Unblinking";
        case SignatureTitle::SteadyEyed: return "Steady-Eyed";
        case SignatureTitle::QuickEyes: return "Quick-Eyes";
        case SignatureTitle::WanderingEyes: return "Wandering Eyes";
        case SignatureTitle::GentleSoul: return "Gentle Soul";
        case SignatureTitle::None:
        default: return "None";
        }
    }

    [[nodiscard]] constexpr SignatureTitle EvaluateSignatureTitle(
        const SignatureAxes& axes) noexcept
    {
        constexpr float kThreshold = 60.0f;
        if (axes.warmth >= kThreshold && axes.candour >= kThreshold)
        {
            return SignatureTitle::MarasOwn;
        }
        if (axes.warmth >= kThreshold && axes.dominance <= -kThreshold)
        {
            return SignatureTitle::GentleSoul;
        }
        if (axes.warmth >= kThreshold)
        {
            return SignatureTitle::KindEyes;
        }
        if (axes.warmth <= -kThreshold && axes.dominance >= kThreshold)
        {
            return SignatureTitle::IceGaze;
        }
        if (axes.dominance >= kThreshold && axes.restraint >= kThreshold)
        {
            return SignatureTitle::TheUnblinking;
        }
        if (axes.dominance >= kThreshold && axes.candour >= kThreshold)
        {
            return SignatureTitle::SteadyEyed;
        }
        if (axes.candour <= -kThreshold)
        {
            return SignatureTitle::QuickEyes;
        }
        if (axes.restraint <= -kThreshold)
        {
            return SignatureTitle::WanderingEyes;
        }
        return SignatureTitle::None;
    }

    /// @brief Tracks player Presence (0..100 with daily diminishing returns), 4-axis Gaze
    /// Signature, gossip titles, and the First Canticle awakening.
    class PresenceSignature
    {
    public:
        static constexpr float kMaxDailyPresenceGain = 6.0f;
        static constexpr float kDiminishingStart     = 3.0f;

        void Load(const SignRecordData& sign) noexcept
        {
            _presence  = sign.presence;
            _signature = sign.signature;
        }

        void SaveInto(SignRecordData& sign) const noexcept
        {
            sign.presence  = _presence;
            sign.signature = _signature;
        }

        [[nodiscard]] const PresenceState& Presence() const noexcept { return _presence; }
        [[nodiscard]] const SignatureAxes& Signature() const noexcept { return _signature; }

        [[nodiscard]] float EffectiveMaxPresence(float currentGameDays) const noexcept
        {
            const float day =
                (std::isfinite(currentGameDays) && currentGameDays >= 0.0f) ? currentGameDays : 0.0f;
            const float shakenPenalty = (day < _presence.shakenUntilGameDays) ? 0.5f : 0.0f;
            return std::max(0.0f, _presence.maximum - shakenPenalty);
        }

        [[nodiscard]] PresenceTier CurrentTier() const noexcept
        {
            return ClassifyPresenceTier(_presence.maximum);
        }

        [[nodiscard]] SignatureTitle CurrentTitle() const noexcept
        {
            return EvaluateSignatureTitle(_signature);
        }

        float AwardMaxPresence(float baseAmount, float currentGameDays,
                               bool bypassDailyCap = false) noexcept
        {
            if (!std::isfinite(baseAmount) || baseAmount <= 0.0f)
            {
                return 0.0f;
            }
            const float day =
                (std::isfinite(currentGameDays) && currentGameDays >= 0.0f) ? currentGameDays : 0.0f;

            if (std::floor(day) > std::floor(_presence.lastGainGameDay))
            {
                _presence.dailyGainAccumulated = 0.0f;
                _presence.lastGainGameDay      = day;
            }

            float granted = 0.0f;
            if (bypassDailyCap)
            {
                granted = baseAmount;
            }
            else
            {
                const float used = _presence.dailyGainAccumulated;
                if (used >= kMaxDailyPresenceGain)
                {
                    return 0.0f;
                }

                float remainingAmount = baseAmount;
                if (used < kDiminishingStart)
                {
                    const float fullSlot = std::min(remainingAmount, kDiminishingStart - used);
                    granted += fullSlot;
                    remainingAmount -= fullSlot;
                }
                if (remainingAmount > 0.0f)
                {
                    const float usedAfterFull = used + granted;
                    const float halfRoom      = std::max(0.0f, kMaxDailyPresenceGain - usedAfterFull);
                    granted += std::min(remainingAmount * 0.5f, halfRoom);
                }
                _presence.dailyGainAccumulated += granted;
            }

            const float prevMax = _presence.maximum;
            _presence.maximum   = std::clamp(_presence.maximum + granted, 0.0f, 100.0f);
            const float actual  = _presence.maximum - prevMax;
            _presence.current   = std::min(_presence.maximum, _presence.current + actual);
            return actual;
        }

        void RecordStareDownOutcome(bool won, float currentGameDays) noexcept
        {
            const float day =
                (std::isfinite(currentGameDays) && currentGameDays >= 0.0f) ? currentGameDays : 0.0f;
            if (won)
            {
                AwardMaxPresence(1.0f, day, false);
                ShiftAxis(_signature.dominance, 2.0f);
                _presence.timidDebuff = false;
            }
            else
            {
                _presence.current             = std::max(0.0f, _presence.current - 1.0f);
                _presence.shakenUntilGameDays = std::max(_presence.shakenUntilGameDays, day + 1.0f);
                ShiftAxis(_signature.dominance, -1.5f);
            }
        }

        void RecordLeerCaught(float currentGameDays) noexcept
        {
            (void)currentGameDays;
            _presence.current = std::max(0.0f, _presence.current - 2.0f);
            ShiftAxis(_signature.restraint, -3.0f);
            ShiftAxis(_signature.warmth, -1.5f);
        }

        void ApplyInterpretationHabit(Interpretation interp, float strength) noexcept
        {
            if (!std::isfinite(strength) || strength <= 0.0f)
            {
                return;
            }
            const float s = std::clamp(strength, 0.0f, 1.0f);
            switch (interp)
            {
            case Interpretation::Warmth:
            case Interpretation::Compassion:
                ShiftAxis(_signature.warmth, 0.6f * s);
                break;
            case Interpretation::Respect:
            case Interpretation::Challenge:
                ShiftAxis(_signature.dominance, 0.5f * s);
                ShiftAxis(_signature.candour, 0.3f * s);
                if (s >= 0.4f)
                {
                    _presence.timidDebuff = false;
                }
                break;
            case Interpretation::Deference:
                ShiftAxis(_signature.dominance, -0.5f * s);
                ShiftAxis(_signature.restraint, 0.3f * s);
                break;
            case Interpretation::Deceit:
            case Interpretation::Suspicion:
                ShiftAxis(_signature.candour, -0.6f * s);
                break;
            case Interpretation::IntrusionLeer:
                ShiftAxis(_signature.restraint, -1.0f * s);
                ShiftAxis(_signature.warmth, -0.4f * s);
                break;
            case Interpretation::Contempt:
                ShiftAxis(_signature.warmth, -0.4f * s);
                break;
            default:
                break;
            }
        }

        void ApplyBookStudyHabit(uint32_t newlyCreditedSpreads, bool completedBook) noexcept
        {
            if (newlyCreditedSpreads == 0u && !completedBook)
            {
                return;
            }
            const float fSpreads = static_cast<float>(newlyCreditedSpreads);
            ShiftAxis(_signature.restraint, 0.3f * fSpreads + (completedBook ? 0.8f : 0.0f));
            ShiftAxis(_signature.candour, 0.2f * fSpreads);
        }

        bool TriggerCanticleAwakening(float currentGameDays) noexcept
        {
            if (_presence.canticleAwakened)
            {
                return false;
            }
            _presence.canticleAwakened = true;
            AwardMaxPresence(5.0f, currentGameDays, true);
            return true;
        }

        void TickRegeneration(float deltaSec, float currentGameDays, bool inSocialSpace,
                              ScholarshipRank rank) noexcept
        {
            if (!std::isfinite(deltaSec) || deltaSec <= 0.0f)
            {
                return;
            }
            const float effMax = EffectiveMaxPresence(currentGameDays);
            if (_presence.current >= effMax)
            {
                _presence.current = effMax;
                return;
            }

            constexpr float kBaseRegenPerSec = 0.5f;
            const float socialMult           = inSocialSpace ? 2.0f : 0.6f;
            const float timidMult            = _presence.timidDebuff ? 0.5f : 1.0f;
            const float scholarshipMult =
                1.0f + ComputeScholarshipBonus(rank).presenceRegenBonusPct * 0.01f;

            const float delta = kBaseRegenPerSec * socialMult * timidMult * scholarshipMult * deltaSec;
            _presence.current = std::min(effMax, _presence.current + delta);
        }

    private:
        static void ShiftAxis(float& axis, float delta) noexcept
        {
            const float next = axis + delta;
            axis = std::isfinite(next) ? std::clamp(next, -100.0f, 100.0f) : 0.0f;
        }

        PresenceState _presence{};
        SignatureAxes _signature{};
    };
} // namespace TrueGaze::ExpandedPlay
