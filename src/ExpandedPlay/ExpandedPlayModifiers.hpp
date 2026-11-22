#pragma once

#include "ExpandedPlay/CombatAndCreatureSight.hpp"
#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <cmath>
#include <cstdint>
#include <vector>

namespace TrueGaze::ExpandedPlay
{
    struct ScholarshipBonus
    {
        float magickaRateMultBonus{0.0f};
        float presenceRegenBonusPct{0.0f};
        float speechIllusionBonus{0.0f};
    };

    /// @brief Maps Ocular Scholarship rank onto reversible Moth's Stillness stat bonuses.
    [[nodiscard]] constexpr ScholarshipBonus ComputeScholarshipBonus(ScholarshipRank rank) noexcept
    {
        switch (rank)
        {
        case ScholarshipRank::Reader: return {3.0f, 5.0f, 2.0f};
        case ScholarshipRank::Acolyte: return {6.0f, 8.0f, 4.0f};
        case ScholarshipRank::Scholar: return {9.0f, 12.0f, 6.0f};
        case ScholarshipRank::Prelate: return {12.0f, 16.0f, 8.0f};
        case ScholarshipRank::Canticle: return {15.0f, 20.0f, 10.0f};
        case ScholarshipRank::Novice:
        default: return {0.0f, 0.0f, 0.0f};
        }
    }

    struct TaggedActorModifier
    {
        uint32_t targetFormId{0};
        int32_t actorValueId{0};
        float delta{0.0f};
    };

    /// @brief SDK-free tracker for currently applied ActorValue deltas.
    /// Ensures every gameplay buff is symmetric, idempotent, and cleanly removable.
    class ModifierLedger
    {
    public:
        /// @brief Updates desired target delta and returns the net increment to apply now.
        [[nodiscard]] float SetTargetDelta(uint32_t targetFormId, int32_t actorValueId,
                                           float desiredDelta) noexcept
        {
            if (targetFormId == 0u)
            {
                return 0.0f;
            }
            if (!std::isfinite(desiredDelta))
            {
                desiredDelta = 0.0f;
            }

            constexpr float kEps = 1e-5f;
            for (auto it = _entries.begin(); it != _entries.end(); ++it)
            {
                if (it->targetFormId == targetFormId && it->actorValueId == actorValueId)
                {
                    const float increment = desiredDelta - it->delta;
                    if (std::abs(desiredDelta) <= kEps)
                    {
                        const float removeDelta = -it->delta;
                        _entries.erase(it);
                        return removeDelta;
                    }
                    it->delta = desiredDelta;
                    return increment;
                }
            }

            if (std::abs(desiredDelta) <= kEps)
            {
                return 0.0f;
            }
            _entries.push_back(TaggedActorModifier{targetFormId, actorValueId, desiredDelta});
            return desiredDelta;
        }

        /// @brief Returns inverse deltas for every active modifier and clears the ledger.
        [[nodiscard]] std::vector<TaggedActorModifier> CollectTeardownDeltas() noexcept
        {
            std::vector<TaggedActorModifier> teardown;
            teardown.reserve(_entries.size());
            constexpr float kEps = 1e-5f;
            for (const auto& entry : _entries)
            {
                if (std::abs(entry.delta) > kEps)
                {
                    teardown.push_back(
                        TaggedActorModifier{entry.targetFormId, entry.actorValueId, -entry.delta});
                }
            }
            _entries.clear();
            return teardown;
        }

        [[nodiscard]] size_t ActiveCount() const noexcept { return _entries.size(); }

        void ClearInMemory() noexcept { _entries.clear(); }

    private:
        std::vector<TaggedActorModifier> _entries{};
    };

    /// @brief Applies and cleanly strips reversible Expanded Play ActorValue modifiers.
    class ExpandedPlayModifiers
    {
    public:
        static ExpandedPlayModifiers& Get() noexcept;

        void SyncFromPersistentState(bool expandedPlayEnabled) noexcept;
        void StripAllActiveModifiers() noexcept;
        void RevertInMemory() noexcept;

        void SetTransientRapportSpeechBonus(float speechDelta) noexcept;
        [[nodiscard]] float TransientRapportSpeechBonus() const noexcept;

        void SetTransientTacticalBonuses(const TacticalSightBuffs& buffs) noexcept;
        [[nodiscard]] TacticalSightBuffs TransientTacticalBonuses() const noexcept;

        [[nodiscard]] size_t ActiveModifierCount() const noexcept;
    };
} // namespace TrueGaze::ExpandedPlay
