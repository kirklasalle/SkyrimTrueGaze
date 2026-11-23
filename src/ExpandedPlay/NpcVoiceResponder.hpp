#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace TrueGaze::ExpandedPlay
{
    /// @brief Contextual social events eligible for restrained vanilla NPC voice responses (WP-11B).
    enum class VoiceEventKind : uint8_t
    {
        None            = 0,
        WarmCommit      = 1, // Warm Admire / Joke / high rapport commit
        IntrusionStare  = 2, // Prolonged unwanted stare / IntrusionLeer / Hate probe
        ArmedCoerce     = 3, // Lethal weapon brandish / Coerce commit
        StandDownGrace  = 4, // Sheathing weapon mid-gaze ("Blood or Coin" de-escalation)
        GuardGlance     = 5, // Guard spots covert bribe / suspicious move
        ContestTension  = 6, // Staring contest round 3 / win / loss reaction
        FactionScrutiny = 7  // Faction drawback scrutiny (Shrouded armor, Vampire, rival uniform)
    };

    struct VoiceBudgetInput
    {
        uint32_t actorFormId{0};
        VoiceEventKind kind{VoiceEventKind::None};
        float nowSessionSec{0.0f};
        bool featureEnabled{true};
        bool actorSpeaking{false};
        bool inDialogueMenu{false};
        bool inScene{false};
        bool inCombat{false};
        bool actorDeadOrDisabled{false};
        bool actorIsChild{false};
        bool playerSneaking{false};
        uint32_t entropySeed{0};
    };

    /// @brief SDK-free restraint governor ensuring NPC voice barks feel natural and never spammy.
    class VoiceResponseBudget
    {
    public:
        static constexpr float kPerNpcCooldownSec = 60.0f;
        static constexpr float kGlobalCooldownSec = 18.0f;

        void Reset() noexcept
        {
            _lastGlobalSpeakSec = -1000.0f;
            _slots = {};
        }

        /// @brief Probability threshold [0..100] per event kind (everyday subtle, dramatic clearer).
        [[nodiscard]] static constexpr uint32_t EventChancePct(VoiceEventKind kind) noexcept
        {
            switch (kind)
            {
            case VoiceEventKind::GuardGlance:     return 80u;
            case VoiceEventKind::StandDownGrace:  return 75u;
            case VoiceEventKind::ContestTension:  return 75u;
            case VoiceEventKind::ArmedCoerce:     return 65u;
            case VoiceEventKind::FactionScrutiny: return 55u;
            case VoiceEventKind::IntrusionStare:  return 40u;
            case VoiceEventKind::WarmCommit:      return 30u;
            case VoiceEventKind::None:
            default:                              return 0u;
            }
        }

        /// @brief Evaluates all restraint gates and consumes cooldowns if allowed to speak.
        [[nodiscard]] bool TryApprove(const VoiceBudgetInput& in) noexcept
        {
            if (!in.featureEnabled || in.actorFormId == 0u || in.kind == VoiceEventKind::None ||
                !std::isfinite(in.nowSessionSec))
            {
                return false;
            }
            if (in.actorSpeaking || in.inDialogueMenu || in.inScene || in.inCombat ||
                in.actorDeadOrDisabled || in.actorIsChild)
            {
                return false;
            }
            // Never break stealth immersion with casual chatter; only guard/scrutiny alerts speak.
            if (in.playerSneaking && in.kind != VoiceEventKind::GuardGlance &&
                in.kind != VoiceEventKind::FactionScrutiny)
            {
                return false;
            }
            if ((in.nowSessionSec - _lastGlobalSpeakSec) < kGlobalCooldownSec)
            {
                return false;
            }

            // Per-NPC cooldown check
            for (const auto& slot : _slots)
            {
                if (slot.actorFormId == in.actorFormId &&
                    (in.nowSessionSec - slot.lastSpeakSec) < kPerNpcCooldownSec)
                {
                    return false;
                }
            }

            // Deterministic restraint roll
            const uint32_t chance = EventChancePct(in.kind);
            if (chance == 0u)
            {
                return false;
            }
            const uint32_t h = MixHash(in.actorFormId, static_cast<uint32_t>(in.kind),
                                       in.entropySeed,
                                       static_cast<uint32_t>(std::max(0.0f, in.nowSessionSec)));
            if ((h % 100u) >= chance)
            {
                return false;
            }

            // Record cooldown consumption
            _lastGlobalSpeakSec = in.nowSessionSec;
            Slot* target = &_slots[0];
            for (auto& slot : _slots)
            {
                if (slot.actorFormId == in.actorFormId || slot.actorFormId == 0u)
                {
                    target = &slot;
                    break;
                }
                if (slot.lastSpeakSec < target->lastSpeakSec)
                {
                    target = &slot;
                }
            }
            target->actorFormId = in.actorFormId;
            target->lastSpeakSec = in.nowSessionSec;
            return true;
        }

    private:
        struct Slot
        {
            uint32_t actorFormId{0};
            float lastSpeakSec{-1000.0f};
        };

        [[nodiscard]] static constexpr uint32_t MixHash(uint32_t a, uint32_t b, uint32_t c,
                                                        uint32_t d) noexcept
        {
            uint32_t h = 2166136261u;
            const uint32_t vals[4]{a, b, c, d};
            for (uint32_t v : vals)
            {
                h ^= v;
                h *= 16777619u;
            }
            h ^= h >> 16;
            h *= 0x7FEB352Du;
            h ^= h >> 15;
            return h;
        }

        float _lastGlobalSpeakSec{-1000.0f};
        std::array<Slot, 16> _slots{};
    };

    /// @brief Game-side native vanilla TESTopic speech dispatcher (zero custom audio, zero Papyrus).
    class NpcVoiceResponder
    {
    public:
        static NpcVoiceResponder& Get() noexcept;

        void Reset() noexcept;
        bool TrySpeak(uint32_t actorFormId, VoiceEventKind kind, float nowSessionSec,
                      bool inDialogueMenu, bool playerSneaking, bool featureEnabled) noexcept;
    };
} // namespace TrueGaze::ExpandedPlay
