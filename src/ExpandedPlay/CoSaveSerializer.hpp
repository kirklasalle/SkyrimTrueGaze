#pragma once

#include "ExpandedPlay/AwakeningRitual.hpp"
#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace TrueGaze::ExpandedPlay
{
    [[nodiscard]] constexpr uint32_t MakeFourCC(char a, char b, char c, char d) noexcept
    {
        return (static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24) |
               (static_cast<uint32_t>(static_cast<uint8_t>(b)) << 16) |
               (static_cast<uint32_t>(static_cast<uint8_t>(c)) << 8) |
               static_cast<uint32_t>(static_cast<uint8_t>(d));
    }

    inline constexpr uint32_t kSerializationUid = MakeFourCC('T', 'G', 'Z', 'E');
    inline constexpr uint32_t kSerializationVersion = 1u;
    inline constexpr uint32_t kRecordTagSign = MakeFourCC('S', 'I', 'G', 'N');
    inline constexpr uint32_t kRecordTagLedg = MakeFourCC('L', 'E', 'D', 'G');
    inline constexpr uint32_t kRecordTagBook = MakeFourCC('B', 'O', 'O', 'K');
    inline constexpr uint32_t kRecordTagAwkn = MakeFourCC('A', 'W', 'K', 'N');

    inline constexpr size_t kMaxLedgerEntries = 512u;
    inline constexpr size_t kMaxBookEntries = 1024u;

    struct SignRecordData
    {
        PresenceState presence{};
        SignatureAxes signature{};
        Demeanor activeDemeanor{Demeanor::Courteous};
        DemeanorMode demeanorMode{DemeanorMode::Hybrid};
        uint32_t booksFullyStudiedCount{0};
        uint32_t skillBoostActorValue{0};
        float skillBoostExpiresGameDays{0.0f};
    };

    struct PersistentState
    {
        SignRecordData sign{};
        std::vector<GazeLedgerEntry> ledger{};
        std::vector<BookStudyEntry> books{};
        AwakeningState awakening{};

        void Clear() noexcept
        {
            sign = SignRecordData{};
            ledger.clear();
            books.clear();
            awakening = AwakeningState{};
        }

        [[nodiscard]] bool HasMeaningfulState() const noexcept
        {
            constexpr float kEps = 1e-4f;
            if (sign.presence.canticleAwakened || sign.booksFullyStudiedCount > 0 ||
                sign.skillBoostActorValue != 0 || std::abs(sign.presence.maximum - 20.0f) > kEps ||
                std::abs(sign.presence.current - 20.0f) > kEps || sign.presence.timidDebuff ||
                sign.presence.shakenUntilGameDays > kEps || awakening.HasAnyProgress())
            {
                return true;
            }
            if (std::abs(sign.signature.warmth) > kEps ||
                std::abs(sign.signature.dominance) > kEps ||
                std::abs(sign.signature.candour) > kEps ||
                std::abs(sign.signature.restraint) > kEps)
            {
                return true;
            }
            return !ledger.empty() || !books.empty();
        }
    };

    using FormIdResolver = std::function<bool(uint32_t oldId, uint32_t& newId)>;

    namespace BinaryCodec
    {
        inline void WriteU8(std::vector<uint8_t>& out, uint8_t value)
        {
            out.push_back(value);
        }

        inline void WriteU32(std::vector<uint8_t>& out, uint32_t value)
        {
            for (int shift = 0; shift < 32; shift += 8)
            {
                out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
            }
        }

        inline void WriteU64(std::vector<uint8_t>& out, uint64_t value)
        {
            for (int shift = 0; shift < 64; shift += 8)
            {
                out.push_back(static_cast<uint8_t>((value >> shift) & 0xFFu));
            }
        }

        inline void WriteF32(std::vector<uint8_t>& out, float value)
        {
            WriteU32(out, std::bit_cast<uint32_t>(value));
        }

        class Reader
        {
        public:
            explicit Reader(std::span<const uint8_t> data) noexcept : _data(data) {}

            [[nodiscard]] size_t Remaining() const noexcept { return _data.size() - _pos; }
            [[nodiscard]] bool Ok() const noexcept { return _ok; }

            bool ReadU8(uint8_t& out) noexcept
            {
                if (!_ok || Remaining() < 1u)
                {
                    _ok = false;
                    return false;
                }
                out = _data[_pos++];
                return true;
            }

            bool ReadU32(uint32_t& out) noexcept
            {
                if (!_ok || Remaining() < 4u)
                {
                    _ok = false;
                    return false;
                }
                uint32_t value = 0;
                for (int shift = 0; shift < 32; shift += 8)
                {
                    value |= static_cast<uint32_t>(_data[_pos++]) << shift;
                }
                out = value;
                return true;
            }

            bool ReadU64(uint64_t& out) noexcept
            {
                if (!_ok || Remaining() < 8u)
                {
                    _ok = false;
                    return false;
                }
                uint64_t value = 0;
                for (int shift = 0; shift < 64; shift += 8)
                {
                    value |= static_cast<uint64_t>(_data[_pos++]) << shift;
                }
                out = value;
                return true;
            }

            bool ReadF32(float& out) noexcept
            {
                uint32_t bits = 0;
                if (!ReadU32(bits))
                {
                    return false;
                }
                out = std::bit_cast<float>(bits);
                return true;
            }

        private:
            std::span<const uint8_t> _data{};
            size_t _pos{0};
            bool _ok{true};
        };

        [[nodiscard]] inline float SanitiseClamped(float v, float lo, float hi,
                                                   float fallback) noexcept
        {
            if (!std::isfinite(v))
            {
                return fallback;
            }
            return std::clamp(v, lo, hi);
        }

        [[nodiscard]] inline float SanitiseNonNeg(float v, float fallback = 0.0f) noexcept
        {
            if (!std::isfinite(v) || v < 0.0f)
            {
                return fallback;
            }
            return v;
        }

        inline void WriteImpressions(std::vector<uint8_t>& out, const Impressions& imp)
        {
            WriteF32(out, imp.respect);
            WriteF32(out, imp.warmth);
            WriteF32(out, imp.unease);
            WriteF32(out, imp.suspicion);
            WriteF32(out, imp.attraction);
            WriteF32(out, imp.fear);
        }

        inline bool ReadImpressions(Reader& r, Impressions& imp) noexcept
        {
            if (!r.ReadF32(imp.respect) || !r.ReadF32(imp.warmth) || !r.ReadF32(imp.unease) ||
                !r.ReadF32(imp.suspicion) || !r.ReadF32(imp.attraction) || !r.ReadF32(imp.fear))
            {
                return false;
            }
            imp.respect = SanitiseClamped(imp.respect, -100.0f, 100.0f, 0.0f);
            imp.warmth = SanitiseClamped(imp.warmth, -100.0f, 100.0f, 0.0f);
            imp.unease = SanitiseClamped(imp.unease, -100.0f, 100.0f, 0.0f);
            imp.suspicion = SanitiseClamped(imp.suspicion, -100.0f, 100.0f, 0.0f);
            imp.attraction = SanitiseClamped(imp.attraction, -100.0f, 100.0f, 0.0f);
            imp.fear = SanitiseClamped(imp.fear, -100.0f, 100.0f, 0.0f);
            return true;
        }

        [[nodiscard]] inline bool ResolveOrPass(uint32_t oldId, uint32_t& newId,
                                                const FormIdResolver& resolver)
        {
            if (oldId == 0u)
            {
                newId = 0u;
                return false;
            }
            if (resolver)
            {
                return resolver(oldId, newId) && newId != 0u;
            }
            newId = oldId;
            return true;
        }
    } // namespace BinaryCodec

    [[nodiscard]] inline std::vector<uint8_t> EncodeSignRecord(const SignRecordData& sign)
    {
        std::vector<uint8_t> out;
        out.reserve(64);
        BinaryCodec::WriteU8(out, sign.presence.canticleAwakened ? 1u : 0u);
        BinaryCodec::WriteU8(out, static_cast<uint8_t>(sign.activeDemeanor));
        BinaryCodec::WriteU8(out, static_cast<uint8_t>(sign.demeanorMode));
        BinaryCodec::WriteU8(out, 0u); // reserved

        BinaryCodec::WriteF32(out, sign.presence.current);
        BinaryCodec::WriteF32(out, sign.presence.maximum);
        BinaryCodec::WriteF32(out, sign.presence.shakenUntilGameDays);
        BinaryCodec::WriteU8(out, sign.presence.timidDebuff ? 1u : 0u);
        BinaryCodec::WriteF32(out, sign.presence.dailyGainAccumulated);
        BinaryCodec::WriteF32(out, sign.presence.lastGainGameDay);

        BinaryCodec::WriteF32(out, sign.signature.warmth);
        BinaryCodec::WriteF32(out, sign.signature.dominance);
        BinaryCodec::WriteF32(out, sign.signature.candour);
        BinaryCodec::WriteF32(out, sign.signature.restraint);

        BinaryCodec::WriteU32(out, sign.booksFullyStudiedCount);
        BinaryCodec::WriteU32(out, sign.skillBoostActorValue);
        BinaryCodec::WriteF32(out, sign.skillBoostExpiresGameDays);
        return out;
    }

    [[nodiscard]] inline bool DecodeSignRecord(std::span<const uint8_t> bytes, uint32_t version,
                                               SignRecordData& outSign) noexcept
    {
        if (version != kSerializationVersion)
        {
            return false;
        }

        BinaryCodec::Reader r(bytes);
        uint8_t awakened = 0;
        uint8_t demeanorRaw = 0;
        uint8_t modeRaw = 0;
        uint8_t reserved = 0;
        uint8_t timid = 0;

        SignRecordData decoded{};
        if (!r.ReadU8(awakened) || !r.ReadU8(demeanorRaw) || !r.ReadU8(modeRaw) ||
            !r.ReadU8(reserved) || !r.ReadF32(decoded.presence.current) ||
            !r.ReadF32(decoded.presence.maximum) ||
            !r.ReadF32(decoded.presence.shakenUntilGameDays) || !r.ReadU8(timid) ||
            !r.ReadF32(decoded.presence.dailyGainAccumulated) ||
            !r.ReadF32(decoded.presence.lastGainGameDay) || !r.ReadF32(decoded.signature.warmth) ||
            !r.ReadF32(decoded.signature.dominance) || !r.ReadF32(decoded.signature.candour) ||
            !r.ReadF32(decoded.signature.restraint) || !r.ReadU32(decoded.booksFullyStudiedCount) ||
            !r.ReadU32(decoded.skillBoostActorValue) ||
            !r.ReadF32(decoded.skillBoostExpiresGameDays))
        {
            return false;
        }
        if (!r.Ok() || r.Remaining() != 0u)
        {
            return false;
        }

        decoded.presence.canticleAwakened = (awakened != 0u);
        decoded.presence.timidDebuff = (timid != 0u);
        decoded.activeDemeanor = demeanorRaw < static_cast<uint8_t>(Demeanor::Count)
                                     ? static_cast<Demeanor>(demeanorRaw)
                                     : Demeanor::Courteous;
        decoded.demeanorMode = modeRaw <= static_cast<uint8_t>(DemeanorMode::Hybrid)
                                   ? static_cast<DemeanorMode>(modeRaw)
                                   : DemeanorMode::Hybrid;

        decoded.presence.maximum =
            BinaryCodec::SanitiseClamped(decoded.presence.maximum, 0.0f, 100.0f, 20.0f);
        decoded.presence.current = BinaryCodec::SanitiseClamped(
            decoded.presence.current, 0.0f, decoded.presence.maximum, decoded.presence.maximum);
        decoded.presence.shakenUntilGameDays =
            BinaryCodec::SanitiseNonNeg(decoded.presence.shakenUntilGameDays);
        decoded.presence.dailyGainAccumulated =
            BinaryCodec::SanitiseNonNeg(decoded.presence.dailyGainAccumulated);
        decoded.presence.lastGainGameDay =
            BinaryCodec::SanitiseNonNeg(decoded.presence.lastGainGameDay);

        decoded.signature.warmth =
            BinaryCodec::SanitiseClamped(decoded.signature.warmth, -100.0f, 100.0f, 0.0f);
        decoded.signature.dominance =
            BinaryCodec::SanitiseClamped(decoded.signature.dominance, -100.0f, 100.0f, 0.0f);
        decoded.signature.candour =
            BinaryCodec::SanitiseClamped(decoded.signature.candour, -100.0f, 100.0f, 0.0f);
        decoded.signature.restraint =
            BinaryCodec::SanitiseClamped(decoded.signature.restraint, -100.0f, 100.0f, 0.0f);

        decoded.skillBoostExpiresGameDays =
            BinaryCodec::SanitiseNonNeg(decoded.skillBoostExpiresGameDays);

        outSign = decoded;
        return true;
    }

    [[nodiscard]] inline std::vector<uint8_t>
    EncodeLedgerRecord(std::span<const GazeLedgerEntry> entries)
    {
        const uint32_t count = static_cast<uint32_t>(std::min(entries.size(), kMaxLedgerEntries));

        std::vector<uint8_t> out;
        out.reserve(4u + static_cast<size_t>(count) * 80u);
        BinaryCodec::WriteU32(out, count);

        for (uint32_t i = 0; i < count; ++i)
        {
            const auto& e = entries[i];
            BinaryCodec::WriteU32(out, e.npcFormId);
            BinaryCodec::WriteF32(out, e.lastSeenGameDays);
            BinaryCodec::WriteU64(out, e.lruSequence);
            BinaryCodec::WriteImpressions(out, e.impressions);
            BinaryCodec::WriteImpressions(out, e.permanentFloor);

            const uint8_t evCount = std::min<uint8_t>(e.eventCount, 8u);
            BinaryCodec::WriteU8(out, evCount);
            BinaryCodec::WriteU8(out, e.recognized ? 1u : 0u);

            const uint8_t startIdx = static_cast<uint8_t>((e.eventHead + 8u - evCount) % 8u);
            for (uint8_t k = 0; k < evCount; ++k)
            {
                const auto& ev = e.notableEvents[(startIdx + k) % 8u];
                BinaryCodec::WriteU8(out, static_cast<uint8_t>(ev.type));
                BinaryCodec::WriteF32(out, ev.gameDaysTimestamp);
                BinaryCodec::WriteU32(out, ev.locationFormId);
                BinaryCodec::WriteU32(out, ev.thirdPartyFormId);
            }
        }
        return out;
    }

    [[nodiscard]] inline bool DecodeLedgerRecord(std::span<const uint8_t> bytes, uint32_t version,
                                                 std::vector<GazeLedgerEntry>& outEntries,
                                                 FormIdResolver resolver = {})
    {
        if (version != kSerializationVersion)
        {
            return false;
        }

        BinaryCodec::Reader r(bytes);
        uint32_t count = 0;
        if (!r.ReadU32(count) || count > kMaxLedgerEntries)
        {
            return false;
        }

        std::vector<GazeLedgerEntry> decoded;
        decoded.reserve(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            GazeLedgerEntry entry{};
            uint32_t rawNpcId = 0;
            uint8_t evCount = 0;
            uint8_t recognizedRaw = 0;
            if (!r.ReadU32(rawNpcId) || !r.ReadF32(entry.lastSeenGameDays) ||
                !r.ReadU64(entry.lruSequence) ||
                !BinaryCodec::ReadImpressions(r, entry.impressions) ||
                !BinaryCodec::ReadImpressions(r, entry.permanentFloor) || !r.ReadU8(evCount) ||
                !r.ReadU8(recognizedRaw) || evCount > 8u)
            {
                return false;
            }

            entry.lastSeenGameDays = BinaryCodec::SanitiseNonNeg(entry.lastSeenGameDays);
            entry.eventCount = evCount;
            entry.eventHead = static_cast<uint8_t>(evCount % 8u);
            entry.recognized = (recognizedRaw != 0u);

            for (uint8_t k = 0; k < evCount; ++k)
            {
                uint8_t typeRaw = 0;
                uint32_t rawLocId = 0;
                uint32_t rawThirdId = 0;
                auto& ev = entry.notableEvents[k];
                if (!r.ReadU8(typeRaw) || !r.ReadF32(ev.gameDaysTimestamp) ||
                    !r.ReadU32(rawLocId) || !r.ReadU32(rawThirdId))
                {
                    return false;
                }
                ev.type = typeRaw <= static_cast<uint8_t>(NotableEventType::CanticleAwakening)
                              ? static_cast<NotableEventType>(typeRaw)
                              : NotableEventType::None;
                ev.gameDaysTimestamp = BinaryCodec::SanitiseNonNeg(ev.gameDaysTimestamp);

                uint32_t resolvedLoc = 0;
                ev.locationFormId =
                    BinaryCodec::ResolveOrPass(rawLocId, resolvedLoc, resolver) ? resolvedLoc : 0u;
                uint32_t resolvedThird = 0;
                ev.thirdPartyFormId =
                    BinaryCodec::ResolveOrPass(rawThirdId, resolvedThird, resolver) ? resolvedThird
                                                                                    : 0u;
            }

            uint32_t resolvedNpcId = 0;
            if (BinaryCodec::ResolveOrPass(rawNpcId, resolvedNpcId, resolver))
            {
                entry.npcFormId = resolvedNpcId;
                decoded.push_back(entry);
            }
        }

        if (!r.Ok() || r.Remaining() != 0u)
        {
            return false;
        }

        outEntries = std::move(decoded);
        return true;
    }

    [[nodiscard]] inline std::vector<uint8_t>
    EncodeBookRecord(std::span<const BookStudyEntry> books)
    {
        const uint32_t count = static_cast<uint32_t>(std::min(books.size(), kMaxBookEntries));

        std::vector<uint8_t> out;
        out.reserve(4u + static_cast<size_t>(count) * 18u);
        BinaryCodec::WriteU32(out, count);

        for (uint32_t i = 0; i < count; ++i)
        {
            const auto& b = books[i];
            BinaryCodec::WriteU32(out, b.bookFormId);
            BinaryCodec::WriteU64(out, b.pagesReadMask);
            BinaryCodec::WriteU8(out, b.totalSpreads);
            BinaryCodec::WriteU8(out, b.fullStudyAwarded ? 1u : 0u);
            BinaryCodec::WriteF32(out, b.lastStudiedGameDays);
        }
        return out;
    }

    [[nodiscard]] inline bool DecodeBookRecord(std::span<const uint8_t> bytes, uint32_t version,
                                               std::vector<BookStudyEntry>& outBooks,
                                               FormIdResolver resolver = {})
    {
        if (version != kSerializationVersion)
        {
            return false;
        }

        BinaryCodec::Reader r(bytes);
        uint32_t count = 0;
        if (!r.ReadU32(count) || count > kMaxBookEntries)
        {
            return false;
        }

        std::vector<BookStudyEntry> decoded;
        decoded.reserve(count);

        for (uint32_t i = 0; i < count; ++i)
        {
            BookStudyEntry entry{};
            uint32_t rawBookId = 0;
            uint8_t fullAwardedRaw = 0;
            if (!r.ReadU32(rawBookId) || !r.ReadU64(entry.pagesReadMask) ||
                !r.ReadU8(entry.totalSpreads) || !r.ReadU8(fullAwardedRaw) ||
                !r.ReadF32(entry.lastStudiedGameDays))
            {
                return false;
            }

            entry.fullStudyAwarded = (fullAwardedRaw != 0u);
            entry.lastStudiedGameDays = BinaryCodec::SanitiseNonNeg(entry.lastStudiedGameDays);

            uint32_t resolvedBookId = 0;
            if (BinaryCodec::ResolveOrPass(rawBookId, resolvedBookId, resolver))
            {
                entry.bookFormId = resolvedBookId;
                decoded.push_back(entry);
            }
        }

        if (!r.Ok() || r.Remaining() != 0u)
        {
            return false;
        }

        outBooks = std::move(decoded);
        return true;
    }

    [[nodiscard]] inline std::vector<uint8_t> EncodeAwakeningRecord(const AwakeningState& awkn)
    {
        std::vector<uint8_t> out;
        out.reserve(12);
        BinaryCodec::WriteU8(out, static_cast<uint8_t>(awkn.tumblerMask & TumblerBits::kAllFour));
        BinaryCodec::WriteU8(out, static_cast<uint8_t>(awkn.visitedSanctuariesMask & 0x7Fu));
        BinaryCodec::WriteU8(out, awkn.ceremonyCompleted ? 1u : 0u);
        BinaryCodec::WriteU8(out, awkn.diegeticToggleOn ? 1u : 0u);
        BinaryCodec::WriteU32(out, awkn.followerRelicWitnessCount);
        BinaryCodec::WriteU8(out, static_cast<uint8_t>(awkn.discoveredSanctuaryRelicsMask & 0x7Fu));
        return out;
    }

    [[nodiscard]] inline bool DecodeAwakeningRecord(std::span<const uint8_t> bytes,
                                                    uint32_t version,
                                                    AwakeningState& outAwkn) noexcept
    {
        if (version != kSerializationVersion)
        {
            return false;
        }
        BinaryCodec::Reader r(bytes);
        uint8_t tumblers = 0;
        uint8_t sanctuaries = 0;
        uint8_t ceremony = 0;
        uint8_t toggleOn = 1;
        uint32_t witnessCount = 0;
        uint8_t discoveredRelics = 0;
        if (!r.ReadU8(tumblers) || !r.ReadU8(sanctuaries) || !r.ReadU8(ceremony) ||
            !r.ReadU8(toggleOn) || !r.ReadU32(witnessCount) || !r.Ok())
            {
                return false;
            }
        if (r.Remaining() == 1u)
        {
            if (!r.ReadU8(discoveredRelics))
            {
                return false;
            }
        }
        if (!r.Ok() || r.Remaining() != 0u)
        {
            return false;
        }
        outAwkn.tumblerMask = static_cast<uint8_t>(tumblers & TumblerBits::kAllFour);
        outAwkn.visitedSanctuariesMask = static_cast<uint8_t>(sanctuaries & 0x7Fu);
        outAwkn.ceremonyCompleted = (ceremony != 0u);
        outAwkn.diegeticToggleOn = (toggleOn != 0u);
        outAwkn.followerRelicWitnessCount = witnessCount;
        outAwkn.discoveredSanctuaryRelicsMask = static_cast<uint8_t>(discoveredRelics & 0x7Fu);
        return true;
    }

    /// @brief Owns runtime Expanded Play persistent state and registers SKSE co-save callbacks.
    class CoSaveSerializer
    {
    public:
        static CoSaveSerializer& Get() noexcept;
        static void RegisterWithSkse() noexcept;

        void ResetState() noexcept;
        [[nodiscard]] PersistentState SnapshotState() const;
        void SetState(PersistentState state);

        [[nodiscard]] size_t LedgerCount() const noexcept;
        [[nodiscard]] size_t BookCount() const noexcept;
        [[nodiscard]] uint32_t BooksFullyStudiedCount() const noexcept;
    };
} // namespace TrueGaze::ExpandedPlay
