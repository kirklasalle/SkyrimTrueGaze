#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace TrueGaze::ExpandedPlay
{
    enum class Demeanor : uint8_t
    {
        Courteous   = 0,
        Warm        = 1,
        Stern       = 2,
        Evasive     = 3,
        Deferential = 4,
        Predatory   = 5,
        Count       = 6
    };

    enum class DemeanorMode : uint8_t
    {
        Manual = 0,
        Auto   = 1,
        Hybrid = 2
    };

    enum class DwellBand : uint8_t
    {
        None     = 0,
        Glance   = 1,
        Look     = 2,
        Gaze     = 3,
        Stare    = 4,
        Fixation = 5
    };

    enum class Interpretation : uint8_t
    {
        None          = 0,
        Respect       = 1,
        Warmth        = 2,
        Attraction    = 3,
        Challenge     = 4,
        IntrusionLeer = 5,
        Suspicion     = 6,
        Deference     = 7,
        Contempt      = 8,
        Compassion    = 9,
        Deceit        = 10,
        Recognition   = 11
    };

    enum class RelationshipTier : uint8_t
    {
        Stranger     = 0,
        Acquaintance = 1,
        Friend       = 2,
        Confidant    = 3,
        Partner      = 4,
        Spouse       = 5,
        Follower     = 6,
        Rival        = 7,
        Enemy        = 8,
        Superior     = 9,
        Subordinate  = 10
    };

    enum class PresenceTier : uint8_t
    {
        Unnoticed  = 0, // 0..19
        Noticed    = 1, // 20..39
        Steady     = 2, // 40..59
        Commanding = 3, // 60..79
        Legendary  = 4  // 80..100
    };

    enum class ScholarshipRank : uint8_t
    {
        Novice   = 0, // < 5 books
        Reader   = 1, // >= 5 books
        Acolyte  = 2, // >= 15 books
        Scholar  = 3, // >= 35 books
        Prelate  = 4, // >= 75 books
        Canticle = 5  // >= 150 books
    };

    enum class NotableEventType : uint8_t
    {
        None              = 0,
        FirstMutualGaze   = 1,
        WarmSharedGlance  = 2,
        StareDownWon      = 3,
        StareDownLost     = 4,
        LeerPublic        = 5,
        SpouseJealousy    = 6,
        TriangleWitnessed = 7,
        ComfortedGrief    = 8,
        CanticleAwakening = 9
    };

    namespace Detail
    {
        [[nodiscard]] constexpr char AsciiLower(char c) noexcept
        {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
        }

        [[nodiscard]] constexpr std::string_view TrimAscii(std::string_view s) noexcept
        {
            while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r' ||
                                  s.front() == '\n'))
            {
                s.remove_prefix(1);
            }
            while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' ||
                                  s.back() == '\n'))
            {
                s.remove_suffix(1);
            }
            return s;
        }

        [[nodiscard]] constexpr bool EqualsIgnoreCase(std::string_view a,
                                                      std::string_view b) noexcept
        {
            if (a.size() != b.size())
            {
                return false;
            }
            for (size_t i = 0; i < a.size(); ++i)
            {
                if (AsciiLower(a[i]) != AsciiLower(b[i]))
                {
                    return false;
                }
            }
            return true;
        }
    } // namespace Detail

    /// @brief Human-readable name of a third-person Demeanor.
    [[nodiscard]] constexpr const char* DemeanorName(Demeanor demeanor) noexcept
    {
        switch (demeanor)
        {
        case Demeanor::Courteous: return "Courteous";
        case Demeanor::Warm: return "Warm";
        case Demeanor::Stern: return "Stern";
        case Demeanor::Evasive: return "Evasive";
        case Demeanor::Deferential: return "Deferential";
        case Demeanor::Predatory: return "Predatory";
        default: return "Unknown";
        }
    }

    /// @brief Canonical string name of a DemeanorMode.
    [[nodiscard]] constexpr const char* DemeanorModeName(DemeanorMode mode) noexcept
    {
        switch (mode)
        {
        case DemeanorMode::Manual: return "Manual";
        case DemeanorMode::Auto: return "Auto";
        case DemeanorMode::Hybrid: return "Hybrid";
        default: return "Hybrid";
        }
    }

    /// @brief Case-insensitive parser for sDemeanorMode; invalid inputs default to Hybrid.
    [[nodiscard]] constexpr DemeanorMode ParseDemeanorMode(std::string_view text,
                                                           bool* outValid = nullptr) noexcept
    {
        const auto trimmed = Detail::TrimAscii(text);
        if (Detail::EqualsIgnoreCase(trimmed, "Manual"))
        {
            if (outValid) *outValid = true;
            return DemeanorMode::Manual;
        }
        if (Detail::EqualsIgnoreCase(trimmed, "Auto"))
        {
            if (outValid) *outValid = true;
            return DemeanorMode::Auto;
        }
        if (Detail::EqualsIgnoreCase(trimmed, "Hybrid"))
        {
            if (outValid) *outValid = true;
            return DemeanorMode::Hybrid;
        }
        if (outValid) *outValid = false;
        return DemeanorMode::Hybrid;
    }

    /// @brief Human-readable name of a DwellBand.
    [[nodiscard]] constexpr const char* DwellBandName(DwellBand band) noexcept
    {
        switch (band)
        {
        case DwellBand::None: return "None";
        case DwellBand::Glance: return "Glance";
        case DwellBand::Look: return "Look";
        case DwellBand::Gaze: return "Gaze";
        case DwellBand::Stare: return "Stare";
        case DwellBand::Fixation: return "Fixation";
        default: return "None";
        }
    }

    /// @brief Maps continuous gaze duration (seconds) onto a DwellBand.
    /// Non-finite or sub-threshold (< 0.08 s) durations return DwellBand::None.
    [[nodiscard]] inline DwellBand ClassifyDwellBand(float dwellSec,
                                                     bool rapidEscalation = false) noexcept
    {
        if (!std::isfinite(dwellSec) || dwellSec < 0.08f)
        {
            return DwellBand::None;
        }

        const float lookStart     = rapidEscalation ? 0.45f : 0.60f;
        const float gazeStart     = rapidEscalation ? 1.50f : 2.00f;
        const float stareStart    = rapidEscalation ? 3.00f : 4.50f;
        const float fixationStart = rapidEscalation ? 6.00f : 9.00f;

        if (dwellSec < lookStart) return DwellBand::Glance;
        if (dwellSec < gazeStart) return DwellBand::Look;
        if (dwellSec < stareStart) return DwellBand::Gaze;
        if (dwellSec < fixationStart) return DwellBand::Stare;
        return DwellBand::Fixation;
    }

    /// @brief Human-readable name of an Interpretation.
    [[nodiscard]] constexpr const char* InterpretationName(Interpretation interp) noexcept
    {
        switch (interp)
        {
        case Interpretation::None: return "None";
        case Interpretation::Respect: return "Respect";
        case Interpretation::Warmth: return "Warmth";
        case Interpretation::Attraction: return "Attraction";
        case Interpretation::Challenge: return "Challenge";
        case Interpretation::IntrusionLeer: return "IntrusionLeer";
        case Interpretation::Suspicion: return "Suspicion";
        case Interpretation::Deference: return "Deference";
        case Interpretation::Contempt: return "Contempt";
        case Interpretation::Compassion: return "Compassion";
        case Interpretation::Deceit: return "Deceit";
        case Interpretation::Recognition: return "Recognition";
        default: return "None";
        }
    }

    /// @brief Human-readable name of a RelationshipTier.
    [[nodiscard]] constexpr const char* RelationshipTierName(RelationshipTier tier) noexcept
    {
        switch (tier)
        {
        case RelationshipTier::Stranger: return "Stranger";
        case RelationshipTier::Acquaintance: return "Acquaintance";
        case RelationshipTier::Friend: return "Friend";
        case RelationshipTier::Confidant: return "Confidant";
        case RelationshipTier::Partner: return "Partner";
        case RelationshipTier::Spouse: return "Spouse";
        case RelationshipTier::Follower: return "Follower";
        case RelationshipTier::Rival: return "Rival";
        case RelationshipTier::Enemy: return "Enemy";
        case RelationshipTier::Superior: return "Superior";
        case RelationshipTier::Subordinate: return "Subordinate";
        default: return "Stranger";
        }
    }

    /// @brief Maps Maximum Presence [0..100] onto its passive PresenceTier.
    [[nodiscard]] inline PresenceTier ClassifyPresenceTier(float maxPresence) noexcept
    {
        if (!std::isfinite(maxPresence) || maxPresence < 20.0f) return PresenceTier::Unnoticed;
        if (maxPresence < 40.0f) return PresenceTier::Noticed;
        if (maxPresence < 60.0f) return PresenceTier::Steady;
        if (maxPresence < 80.0f) return PresenceTier::Commanding;
        return PresenceTier::Legendary;
    }

    /// @brief Human-readable name of a PresenceTier.
    [[nodiscard]] constexpr const char* PresenceTierName(PresenceTier tier) noexcept
    {
        switch (tier)
        {
        case PresenceTier::Unnoticed: return "Unnoticed";
        case PresenceTier::Noticed: return "Noticed";
        case PresenceTier::Steady: return "Steady";
        case PresenceTier::Commanding: return "Commanding";
        case PresenceTier::Legendary: return "Legendary";
        default: return "Unnoticed";
        }
    }

    /// @brief Maps cumulative genuinely studied book count onto Moth's Stillness rank.
    [[nodiscard]] constexpr ScholarshipRank ClassifyScholarshipRank(uint32_t booksStudied) noexcept
    {
        if (booksStudied < 5u) return ScholarshipRank::Novice;
        if (booksStudied < 15u) return ScholarshipRank::Reader;
        if (booksStudied < 35u) return ScholarshipRank::Acolyte;
        if (booksStudied < 75u) return ScholarshipRank::Scholar;
        if (booksStudied < 150u) return ScholarshipRank::Prelate;
        return ScholarshipRank::Canticle;
    }

    /// @brief Human-readable name of a ScholarshipRank.
    [[nodiscard]] constexpr const char* ScholarshipRankName(ScholarshipRank rank) noexcept
    {
        switch (rank)
        {
        case ScholarshipRank::Novice: return "Novice";
        case ScholarshipRank::Reader: return "Reader";
        case ScholarshipRank::Acolyte: return "Acolyte";
        case ScholarshipRank::Scholar: return "Scholar";
        case ScholarshipRank::Prelate: return "Prelate";
        case ScholarshipRank::Canticle: return "Canticle";
        default: return "Novice";
        }
    }

    struct NotableEvent
    {
        NotableEventType type{NotableEventType::None};
        float gameDaysTimestamp{0.0f};
        uint32_t locationFormId{0};
        uint32_t thirdPartyFormId{0};
    };

    struct Impressions
    {
        float respect{0.0f};
        float warmth{0.0f};
        float unease{0.0f};
        float suspicion{0.0f};
        float attraction{0.0f};
        float fear{0.0f};
    };

    struct GazeLedgerEntry
    {
        uint32_t npcFormId{0};
        float lastSeenGameDays{0.0f};
        uint64_t lruSequence{0};
        Impressions impressions{};
        Impressions permanentFloor{};
        std::array<NotableEvent, 8> notableEvents{};
        uint8_t eventHead{0};
        uint8_t eventCount{0};
        bool recognized{false};
    };

    struct SignatureAxes
    {
        float warmth{0.0f};
        float dominance{0.0f};
        float candour{0.0f};
        float restraint{0.0f};
    };

    struct PresenceState
    {
        float current{20.0f};
        float maximum{20.0f};
        float shakenUntilGameDays{0.0f};
        bool timidDebuff{false};
        float dailyGainAccumulated{0.0f};
        float lastGainGameDay{0.0f};
        bool canticleAwakened{false};
    };

    struct BookStudyEntry
    {
        uint32_t bookFormId{0};
        uint64_t pagesReadMask{0};
        uint8_t totalSpreads{0};
        bool fullStudyAwarded{false};
        float lastStudiedGameDays{0.0f};
    };
} // namespace TrueGaze::ExpandedPlay
