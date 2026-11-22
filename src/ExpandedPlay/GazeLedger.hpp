#pragma once

#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace TrueGaze::ExpandedPlay
{
    enum class WitnessRole : uint8_t
    {
        Bystander         = 0,
        Spouse            = 1,
        Partner           = 2,
        LoveTriangleRival = 3
    };

    /// @brief Per-NPC non-verbal social memory with 512-entry LRU eviction, 14-day exponential
    /// decay toward permanent floors, and spouse / love-triangle witness hooks.
    class GazeLedger
    {
    public:
        static constexpr size_t kMaxPersistentEntries        = 512u;
        static constexpr size_t kMaxTransientEntries         = 16u;
        static constexpr float kDecayHalfLifeDays            = 14.0f;
        static constexpr float kNeutralPersistenceThreshold  = 5.0f;

        static void ApplyDecay(GazeLedgerEntry& entry, float currentGameDays) noexcept
        {
            if (!std::isfinite(currentGameDays) || !std::isfinite(entry.lastSeenGameDays))
            {
                return;
            }
            const float deltaDays = currentGameDays - entry.lastSeenGameDays;
            if (deltaDays <= 0.0f)
            {
                return;
            }

            const float factor = std::exp2(-deltaDays / kDecayHalfLifeDays);
            const auto decayField = [factor](float& value, float floor) noexcept
            {
                const float next = floor + (value - floor) * factor;
                value = std::isfinite(next) ? std::clamp(next, -100.0f, 100.0f) : 0.0f;
            };

            decayField(entry.impressions.respect, entry.permanentFloor.respect);
            decayField(entry.impressions.warmth, entry.permanentFloor.warmth);
            decayField(entry.impressions.unease, entry.permanentFloor.unease);
            decayField(entry.impressions.suspicion, entry.permanentFloor.suspicion);
            decayField(entry.impressions.attraction, entry.permanentFloor.attraction);
            decayField(entry.impressions.fear, entry.permanentFloor.fear);
            entry.lastSeenGameDays = currentGameDays;
        }

        void LoadFromVector(std::vector<GazeLedgerEntry> entries) noexcept
        {
            _persistent.clear();
            _transient.clear();
            _nextSeq = 1u;

            const size_t count = std::min(entries.size(), kMaxPersistentEntries);
            _persistent.reserve(count);
            for (size_t i = 0; i < count; ++i)
            {
                if (entries[i].npcFormId == 0u)
                {
                    continue;
                }
                _nextSeq = std::max(_nextSeq, entries[i].lruSequence + 1u);
                _persistent.push_back(entries[i]);
            }
        }

        [[nodiscard]] std::vector<GazeLedgerEntry> ExportVector() const
        {
            return _persistent;
        }

        void Clear() noexcept
        {
            _persistent.clear();
            _transient.clear();
            _nextSeq = 1u;
        }

        [[nodiscard]] size_t PersistentCount() const noexcept { return _persistent.size(); }
        [[nodiscard]] size_t TransientCount() const noexcept { return _transient.size(); }

        [[nodiscard]] const GazeLedgerEntry* Find(uint32_t npcFormId) const noexcept
        {
            for (const auto& e : _persistent)
            {
                if (e.npcFormId == npcFormId) return &e;
            }
            for (const auto& e : _transient)
            {
                if (e.npcFormId == npcFormId) return &e;
            }
            return nullptr;
        }

        void RecordInterpretation(uint32_t npcFormId, Interpretation interp, float strength,
                                  float currentGameDays, uint32_t locationFormId = 0u,
                                  bool isMutual = false) noexcept
        {
            if (npcFormId == 0u || interp == Interpretation::None || !std::isfinite(strength) ||
                strength <= 0.0f)
            {
                return;
            }

            const float s = std::clamp(strength, 0.0f, 1.0f);
            const float day =
                (std::isfinite(currentGameDays) && currentGameDays >= 0.0f) ? currentGameDays : 0.0f;

            bool isPersistent = false;
            size_t idx = 0u;
            GazeLedgerEntry* entry = FindOrCreate(npcFormId, day, isPersistent, idx);
            if (!entry)
            {
                return;
            }

            ApplyDecay(*entry, day);
            entry->lastSeenGameDays = day;
            entry->lruSequence      = _nextSeq++;

            if (isMutual && !entry->recognized)
            {
                entry->recognized = true;
                PushEvent(*entry, NotableEventType::FirstMutualGaze, day, locationFormId, 0u);
            }

            auto& imp = entry->impressions;
            switch (interp)
            {
            case Interpretation::Respect:
                imp.respect += 4.0f * s;
                break;
            case Interpretation::Warmth:
                imp.warmth += 6.0f * s;
                imp.unease -= 2.0f * s;
                break;
            case Interpretation::Attraction:
                imp.attraction += 7.0f * s;
                imp.warmth     += 3.0f * s;
                break;
            case Interpretation::Challenge:
                imp.respect -= 1.0f * s;
                imp.unease  += 5.0f * s;
                imp.fear    += 2.0f * s;
                break;
            case Interpretation::IntrusionLeer:
                imp.unease  += 12.0f * s;
                imp.warmth  -= 8.0f * s;
                imp.respect -= 6.0f * s;
                entry->permanentFloor.unease = std::max(entry->permanentFloor.unease, 10.0f);
                PushEvent(*entry, NotableEventType::LeerPublic, day, locationFormId, 0u);
                break;
            case Interpretation::Suspicion:
                imp.suspicion += 8.0f * s;
                break;
            case Interpretation::Deference:
                imp.respect += 5.0f * s;
                imp.unease  -= 2.0f * s;
                break;
            case Interpretation::Contempt:
                imp.warmth  -= 5.0f * s;
                imp.respect -= 4.0f * s;
                imp.unease  += 3.0f * s;
                break;
            case Interpretation::Compassion:
                imp.warmth  += 8.0f * s;
                imp.respect += 4.0f * s;
                entry->permanentFloor.warmth = std::max(entry->permanentFloor.warmth, 10.0f);
                PushEvent(*entry, NotableEventType::ComfortedGrief, day, locationFormId, 0u);
                break;
            case Interpretation::Deceit:
                imp.suspicion += 10.0f * s;
                imp.respect   -= 3.0f * s;
                break;
            case Interpretation::Recognition:
                imp.respect += 2.0f * s;
                imp.warmth  += 2.0f * s;
                break;
            case Interpretation::None:
            default:
                break;
            }

            ClampImpressions(imp);

            if (!isPersistent && ShouldPersist(*entry))
            {
                PromoteTransient(idx);
            }
        }

        /// @brief Applies a multi-channel signed impression delta (e.g., from PersuasionTransactions).
        void ApplyImpressionDelta(uint32_t npcFormId, const Impressions& delta,
                                  float currentGameDays, uint32_t locationFormId = 0u,
                                  bool isMutual = false) noexcept
        {
            if (npcFormId == 0u)
            {
                return;
            }
            const auto safe = [](float v) noexcept -> float
            {
                return std::isfinite(v) ? v : 0.0f;
            };
            const float dRespect    = safe(delta.respect);
            const float dWarmth     = safe(delta.warmth);
            const float dUnease     = safe(delta.unease);
            const float dSuspicion  = safe(delta.suspicion);
            const float dAttraction = safe(delta.attraction);
            const float dFear       = safe(delta.fear);

            constexpr float kEps = 1e-4f;
            if (std::abs(dRespect) <= kEps && std::abs(dWarmth) <= kEps &&
                std::abs(dUnease) <= kEps && std::abs(dSuspicion) <= kEps &&
                std::abs(dAttraction) <= kEps && std::abs(dFear) <= kEps)
            {
                return;
            }

            const float day =
                (std::isfinite(currentGameDays) && currentGameDays >= 0.0f) ? currentGameDays : 0.0f;

            bool isPersistent = false;
            size_t idx = 0u;
            GazeLedgerEntry* entry = FindOrCreate(npcFormId, day, isPersistent, idx);
            if (!entry)
            {
                return;
            }

            ApplyDecay(*entry, day);
            entry->lastSeenGameDays = day;
            entry->lruSequence      = _nextSeq++;

            if (isMutual && !entry->recognized)
            {
                entry->recognized = true;
                PushEvent(*entry, NotableEventType::FirstMutualGaze, day, locationFormId, 0u);
            }

            entry->impressions.respect    += dRespect;
            entry->impressions.warmth     += dWarmth;
            entry->impressions.unease     += dUnease;
            entry->impressions.suspicion  += dSuspicion;
            entry->impressions.attraction += dAttraction;
            entry->impressions.fear       += dFear;
            ClampImpressions(entry->impressions);

            if (!isPersistent && ShouldPersist(*entry))
            {
                PromoteTransient(idx);
            }
        }

        void RecordNotableEvent(uint32_t npcFormId, NotableEventType type, float currentGameDays,
                                uint32_t locationFormId = 0u,
                                uint32_t thirdPartyFormId = 0u) noexcept
        {
            if (npcFormId == 0u || type == NotableEventType::None)
            {
                return;
            }

            const float day =
                (std::isfinite(currentGameDays) && currentGameDays >= 0.0f) ? currentGameDays : 0.0f;

            bool isPersistent = false;
            size_t idx = 0u;
            GazeLedgerEntry* entry = FindOrCreate(npcFormId, day, isPersistent, idx);
            if (!entry)
            {
                return;
            }

            ApplyDecay(*entry, day);
            entry->lastSeenGameDays = day;
            entry->lruSequence      = _nextSeq++;
            PushEvent(*entry, type, day, locationFormId, thirdPartyFormId);
            ApplyNotableFloor(*entry, type);

            if (!isPersistent)
            {
                PromoteTransient(idx);
            }
        }

        /// @brief Logs witness reactions (e.g., Spouse jealousy or Riverwood Sven/Camilla/Faendal).
        bool RecordWitnessedInteraction(uint32_t witnessFormId, WitnessRole role,
                                        uint32_t targetFormId, Interpretation targetInterp,
                                        float strength, float currentGameDays,
                                        uint32_t locationFormId = 0u) noexcept
        {
            if (witnessFormId == 0u || targetFormId == 0u || witnessFormId == targetFormId ||
                !std::isfinite(strength) || strength <= 0.0f)
            {
                return false;
            }

            const bool romanticOrWarm = targetInterp == Interpretation::Warmth ||
                                        targetInterp == Interpretation::Attraction ||
                                        targetInterp == Interpretation::IntrusionLeer;
            if ((role == WitnessRole::Spouse || role == WitnessRole::Partner) && !romanticOrWarm)
            {
                return false;
            }
            if (role == WitnessRole::LoveTriangleRival && !romanticOrWarm)
            {
                return false;
            }
            if (role == WitnessRole::Bystander && targetInterp != Interpretation::IntrusionLeer)
            {
                return false;
            }

            const float s = std::clamp(strength, 0.0f, 1.0f);
            const float day =
                (std::isfinite(currentGameDays) && currentGameDays >= 0.0f) ? currentGameDays : 0.0f;

            bool isPersistent = false;
            size_t idx = 0u;
            GazeLedgerEntry* entry = FindOrCreate(witnessFormId, day, isPersistent, idx);
            if (!entry)
            {
                return false;
            }

            ApplyDecay(*entry, day);
            entry->lastSeenGameDays = day;
            entry->lruSequence      = _nextSeq++;

            if (role == WitnessRole::Spouse || role == WitnessRole::Partner)
            {
                entry->impressions.unease += 15.0f * s;
                entry->impressions.warmth -= 10.0f * s;
                entry->permanentFloor.unease = std::max(entry->permanentFloor.unease, 15.0f);
                PushEvent(*entry, NotableEventType::SpouseJealousy, day, locationFormId,
                          targetFormId);
            }
            else if (role == WitnessRole::LoveTriangleRival)
            {
                entry->impressions.unease    += 10.0f * s;
                entry->impressions.suspicion += 8.0f * s;
                entry->impressions.warmth    -= 4.0f * s;
                PushEvent(*entry, NotableEventType::TriangleWitnessed, day, locationFormId,
                          targetFormId);
            }
            else
            {
                entry->impressions.unease  += 6.0f * s;
                entry->impressions.respect -= 3.0f * s;
                if (s >= 0.5f)
                {
                    PushEvent(*entry, NotableEventType::LeerPublic, day, locationFormId,
                              targetFormId);
                }
            }

            ClampImpressions(entry->impressions);
            if (!isPersistent && ShouldPersist(*entry))
            {
                PromoteTransient(idx);
            }
            return true;
        }

    private:
        static void ClampImpressions(Impressions& imp) noexcept
        {
            imp.respect    = std::clamp(imp.respect, -100.0f, 100.0f);
            imp.warmth     = std::clamp(imp.warmth, -100.0f, 100.0f);
            imp.unease     = std::clamp(imp.unease, -100.0f, 100.0f);
            imp.suspicion  = std::clamp(imp.suspicion, -100.0f, 100.0f);
            imp.attraction = std::clamp(imp.attraction, -100.0f, 100.0f);
            imp.fear       = std::clamp(imp.fear, -100.0f, 100.0f);
        }

        [[nodiscard]] static float MaxAbsImpression(const Impressions& imp) noexcept
        {
            return std::max({std::abs(imp.respect), std::abs(imp.warmth), std::abs(imp.unease),
                             std::abs(imp.suspicion), std::abs(imp.attraction), std::abs(imp.fear)});
        }

        [[nodiscard]] static bool ShouldPersist(const GazeLedgerEntry& entry) noexcept
        {
            return MaxAbsImpression(entry.impressions) >= kNeutralPersistenceThreshold ||
                   entry.eventCount > 0u || entry.recognized;
        }

        static void PushEvent(GazeLedgerEntry& entry, NotableEventType type, float day,
                              uint32_t locationFormId, uint32_t thirdPartyFormId) noexcept
        {
            entry.notableEvents[entry.eventHead] =
                NotableEvent{type, day, locationFormId, thirdPartyFormId};
            entry.eventHead = static_cast<uint8_t>((entry.eventHead + 1u) % 8u);
            if (entry.eventCount < 8u)
            {
                ++entry.eventCount;
            }
        }

        static void ApplyNotableFloor(GazeLedgerEntry& entry, NotableEventType type) noexcept
        {
            switch (type)
            {
            case NotableEventType::LeerPublic:
                entry.permanentFloor.unease = std::max(entry.permanentFloor.unease, 10.0f);
                break;
            case NotableEventType::ComfortedGrief:
                entry.permanentFloor.warmth = std::max(entry.permanentFloor.warmth, 10.0f);
                break;
            case NotableEventType::StareDownWon:
                entry.permanentFloor.respect = std::max(entry.permanentFloor.respect, 12.0f);
                break;
            case NotableEventType::SpouseJealousy:
                entry.permanentFloor.unease = std::max(entry.permanentFloor.unease, 15.0f);
                break;
            default:
                break;
            }
        }

        GazeLedgerEntry* FindOrCreate(uint32_t npcFormId, float day, bool& outIsPersistent,
                                      size_t& outIdx) noexcept
        {
            for (size_t i = 0; i < _persistent.size(); ++i)
            {
                if (_persistent[i].npcFormId == npcFormId)
                {
                    outIsPersistent = true;
                    outIdx          = i;
                    return &_persistent[i];
                }
            }
            for (size_t i = 0; i < _transient.size(); ++i)
            {
                if (_transient[i].npcFormId == npcFormId)
                {
                    outIsPersistent = false;
                    outIdx          = i;
                    return &_transient[i];
                }
            }

            if (_transient.size() >= kMaxTransientEntries)
            {
                size_t lruIdx = 0u;
                for (size_t i = 1; i < _transient.size(); ++i)
                {
                    if (_transient[i].lruSequence < _transient[lruIdx].lruSequence)
                    {
                        lruIdx = i;
                    }
                }
                _transient.erase(_transient.begin() + static_cast<ptrdiff_t>(lruIdx));
            }

            GazeLedgerEntry fresh{};
            fresh.npcFormId        = npcFormId;
            fresh.lastSeenGameDays = day;
            fresh.lruSequence      = _nextSeq++;
            _transient.push_back(fresh);
            outIsPersistent = false;
            outIdx          = _transient.size() - 1u;
            return &_transient.back();
        }

        void PromoteTransient(size_t transientIdx) noexcept
        {
            if (transientIdx >= _transient.size())
            {
                return;
            }
            const GazeLedgerEntry promoted = _transient[transientIdx];
            _transient.erase(_transient.begin() + static_cast<ptrdiff_t>(transientIdx));

            if (_persistent.size() >= kMaxPersistentEntries)
            {
                size_t lruIdx = 0u;
                for (size_t i = 1; i < _persistent.size(); ++i)
                {
                    if (_persistent[i].lruSequence < _persistent[lruIdx].lruSequence)
                    {
                        lruIdx = i;
                    }
                }
                _persistent.erase(_persistent.begin() + static_cast<ptrdiff_t>(lruIdx));
            }
            _persistent.push_back(promoted);
        }

        std::vector<GazeLedgerEntry> _persistent{};
        std::vector<GazeLedgerEntry> _transient{};
        uint64_t _nextSeq{1u};
    };
} // namespace TrueGaze::ExpandedPlay
