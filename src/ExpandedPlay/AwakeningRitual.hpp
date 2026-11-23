#pragma once

#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <format>
#include <string>
#include <string_view>
#include <vector>

namespace TrueGaze::ExpandedPlay
{
    enum class UnlockMode : uint8_t
    {
        Ritual  = 0, // Staged 4-tumbler + Sanctuary Ceremony progression (default)
        Sandbox = 1  // Immediate Stage 4 full-power unlock (testing / dev / console override)
    };

    enum class AwakeningStage : uint8_t
    {
        Dormant          = 0, // 0 tumblers (classic visual TrueGaze only)
        FirstGlimmer     = 1, // 1 tumbler: Reticle, Hybrid Demeanor (basic), Ledger, Book Study
        SteadySight      = 2, // 2 tumblers: Persuasion Probes, Guard's Glance, Eye-Contact Greeting, Stern/Evasive
        LivingGlyph      = 3, // 3-4 tumblers (pre-ceremony): Full Transactions, Contests, Kindred Sight
        CanticleAwakened = 4  // All 4 tumblers + Final Sanctuary Ceremony: Full Mastery + Diegetic Toggle
    };

    namespace TumblerBits
    {
        inline constexpr uint8_t kNone        = 0u;
        inline constexpr uint8_t kWord        = 1u << 0; // A. Studied Canticle / Scroll tome >= 80%
        inline constexpr uint8_t kVessel      = 1u << 1; // B. Worn Relic/Focus + 3s mutual gaze
        inline constexpr uint8_t kSharedSight = 1u << 2; // C. Follower/NPC carries Sight Relic + mutual gaze
        inline constexpr uint8_t kPilgrimage  = 1u << 3; // D. Mindful stillness at 2+ base-Skyrim Sanctuaries
        inline constexpr uint8_t kAllFour     = kWord | kVessel | kSharedSight | kPilgrimage;
    } // namespace TumblerBits

    /// @brief Base Skyrim.esm sacred sanctuaries (zero DLC dependency).
    namespace SanctuaryBits
    {
        inline constexpr uint8_t kNone               = 0u;
        inline constexpr uint8_t kSleepingGiantInn   = 1u << 0;
        inline constexpr uint8_t kTempleOfKynareth   = 1u << 1;
        inline constexpr uint8_t kEldergleam         = 1u << 2;
        inline constexpr uint8_t kTempleOfMara       = 1u << 3;
        inline constexpr uint8_t kTempleOfTheDivines = 1u << 4;
        inline constexpr uint8_t kHighHrothgar       = 1u << 5;
        inline constexpr uint8_t kArcanaeum          = 1u << 6;
    } // namespace SanctuaryBits

    enum class RelicKind : uint8_t
    {
        None               = 0,
        CanticleBook       = 1, // The First Canticle of the Eye (ESL) or canon Elder Scroll tome
        PlayerToggleRelic  = 2, // Amulet of the Ancestor Moth / Moth-Silk Browband (ESL)
        GiftableSightRelic = 3, // Ring of Quiet Sight / Silver Moth Medallion / Moth-Silk Ribbon (ESL)
        VanillaFocus       = 4  // Vanilla Skyrim.esm Divine Amulet / Circlet / Silver-Gold Ring
    };

    /// @brief One-time diegetic in-world rumor & Elder Scroll quest discovery hints (WP-11D).
    namespace DiscoveryHintBits
    {
        inline constexpr uint8_t kNone              = 0u;
        inline constexpr uint8_t kInnRumor          = 1u << 0; // Pilgrimage Quest + Note received
        inline constexpr uint8_t kElderScrollQuest  = 1u << 1; // MQ205 / DA04 / Dawnguard Prophet
        inline constexpr uint8_t kSanctuaryArrival  = 1u << 2; // First entry into any Sanctuary
        inline constexpr uint8_t kBarNotePlaced     = 1u << 3; // Physical note placed on Inn bar
        inline constexpr uint8_t kInnVisitedLeft    = 1u << 4; // Left Inn without note -> Courier armed
        inline constexpr uint8_t kCourierSent       = 1u << 5; // Vanilla Skyrim WICourier dispatched
    } // namespace DiscoveryHintBits

    struct AwakeningState
    {
        uint8_t tumblerMask{TumblerBits::kNone};
        uint8_t visitedSanctuariesMask{SanctuaryBits::kNone};
        bool ceremonyCompleted{false};
        bool diegeticToggleOn{true};
        uint32_t followerRelicWitnessCount{0};
        uint8_t discoveredSanctuaryRelicsMask{SanctuaryBits::kNone};
        uint8_t discoveryHintsMask{DiscoveryHintBits::kNone};

        [[nodiscard]] bool HasAnyProgress() const noexcept
        {
            return tumblerMask != 0u || visitedSanctuariesMask != 0u || ceremonyCompleted ||
                   !diegeticToggleOn || followerRelicWitnessCount > 0u ||
                   discoveredSanctuaryRelicsMask != 0u || discoveryHintsMask != 0u;
        }
    };

    /// @brief Marks a diegetic discovery rumor/quest hint as shown once per save.
    [[nodiscard]] inline bool MarkDiscoveryHintShown(AwakeningState& state, uint8_t hintBit) noexcept
    {
        const uint8_t bit = static_cast<uint8_t>(hintBit & 0x3Fu);
        if (bit == 0u || (state.discoveryHintsMask & bit) != 0u)
        {
            return false;
        }
        state.discoveryHintsMask = static_cast<uint8_t>(state.discoveryHintsMask | bit);
        return true;
    }

    enum class SanctuaryRelicSlot : uint8_t
    {
        None             = 0,
        FirstCanticle    = 1, // Sleeping Giant Inn & Arcanaeum
        AmuletMoth       = 2, // Temple of Kynareth
        MothSilkBrowband = 3, // Eldergleam Sanctuary
        RingQuietSight   = 4, // Temple of Mara
        SilverMedallion  = 5, // Temple of the Divines
        MothSilkRibbon   = 6  // High Hrothgar
    };

    struct SanctuaryRelicSpec
    {
        SanctuaryRelicSlot slot{SanctuaryRelicSlot::None};
        const char* relicName{""};
        const char* sanctuaryTitle{""};
        const char* discoveryWhisper{""};
    };

    /// @brief Maps each of Skyrim's 7 Sanctuaries to its canonical lore artifact.
    /// Note: The Sleeping Giant Inn bestows only Brother Cassian's Living Note (via Orgnar) and
    /// the lingering Moth Priest Presence blessing; The First Canticle of the Eye awaits in the
    /// Arcanaeum (or any canon Elder Scroll / Moth tome across Skyrim).
    [[nodiscard]] constexpr SanctuaryRelicSpec GetSanctuaryRelicSpec(uint8_t sanctuaryBit) noexcept
    {
        switch (sanctuaryBit)
        {
        case SanctuaryBits::kSleepingGiantInn:
            return {}; // Inn grants Brother Cassian's Note from Orgnar + Moth Presence Blessing
        case SanctuaryBits::kTempleOfKynareth:
            return {SanctuaryRelicSlot::AmuletMoth, "Amulet of the Ancestor Moth",
                    "Temple of Kynareth",
                    "Beneath Kyne's dome, a Moth Vessel glints: Amulet of the Ancestor Moth."};
        case SanctuaryBits::kEldergleam:
            return {SanctuaryRelicSlot::MothSilkBrowband, "Moth-Silk Browband",
                    "Eldergleam Sanctuary",
                    "Spun beneath ancient roots: you discover the Moth-Silk Browband."};
        case SanctuaryBits::kTempleOfMara:
            return {SanctuaryRelicSlot::RingQuietSight, "Ring of Quiet Sight",
                    "Temple of Mara",
                    "Blessed in Mara's sanctuary: you discover the Ring of Quiet Sight."};
        case SanctuaryBits::kTempleOfTheDivines:
            return {SanctuaryRelicSlot::SilverMedallion, "Silver Moth Medallion",
                    "Temple of the Divines",
                    "Preserved among Solitude's shrines: you discover the Silver Moth Medallion."};
        case SanctuaryBits::kHighHrothgar:
            return {SanctuaryRelicSlot::MothSilkRibbon, "Moth-Silk Ribbon",
                    "High Hrothgar",
                    "Left by a Cyrodiilic envoy beside the Voice: you discover the Moth-Silk Ribbon."};
        case SanctuaryBits::kArcanaeum:
            return {SanctuaryRelicSlot::FirstCanticle, "The First Canticle of the Eye",
                    "The Arcanaeum",
                    "Among Winterhold's shelved folios lies a copy of The First Canticle of the Eye."};
        default:
            return {};
        }
    }

    /// @brief Marks a Sanctuary's relic as discovered once per save.
    [[nodiscard]] inline bool MarkSanctuaryRelicDiscovered(AwakeningState& state,
                                                           uint8_t sanctuaryBit) noexcept
    {
        const uint8_t bit = static_cast<uint8_t>(sanctuaryBit & 0x7Fu);
        if (bit == 0u || (state.discoveredSanctuaryRelicsMask & bit) != 0u)
        {
            return false;
        }
        state.discoveredSanctuaryRelicsMask =
            static_cast<uint8_t>(state.discoveredSanctuaryRelicsMask | bit);
        return true;
    }

    struct StageCapabilities
    {
        AwakeningStage stage{AwakeningStage::Dormant};
        bool expandedActive{false};
        bool allowReticle{false};
        bool allowSocialMemory{false};
        bool allowBookStudy{false};
        bool allowPersuasionProbes{false};
        bool allowEyeContactGreeting{false};
        bool allowSternEvasiveDemeanor{false};
        bool allowTransactions{false};
        bool allowStaringContest{false};
        bool allowKindredSight{false};
        bool allowPredatoryDemeanor{false};
        bool allowFullSignatureResonance{false};
        float maxPresenceCap{20.0f};
        ScholarshipRank maxScholarshipRank{ScholarshipRank::Novice};
    };

    struct AwakeningEventResult
    {
        bool progressed{false};
        bool stageAdvanced{false};
        AwakeningStage prevStage{AwakeningStage::Dormant};
        AwakeningStage newStage{AwakeningStage::Dormant};
        std::string whisper{};
        uint32_t whisperColorRgb{0xFFD966u};
    };

    [[nodiscard]] constexpr const char* UnlockModeName(UnlockMode mode) noexcept
    {
        switch (mode)
        {
        case UnlockMode::Sandbox: return "Sandbox";
        case UnlockMode::Ritual:
        default: return "Ritual";
        }
    }

    [[nodiscard]] constexpr UnlockMode ParseUnlockMode(std::string_view text,
                                                       bool* outValid = nullptr) noexcept
    {
        const auto trimmed = Detail::TrimAscii(text);
        if (Detail::EqualsIgnoreCase(trimmed, "Ritual"))
        {
            if (outValid) *outValid = true;
            return UnlockMode::Ritual;
        }
        if (Detail::EqualsIgnoreCase(trimmed, "Sandbox"))
        {
            if (outValid) *outValid = true;
            return UnlockMode::Sandbox;
        }
        if (outValid) *outValid = false;
        return UnlockMode::Ritual;
    }

    [[nodiscard]] constexpr const char* AwakeningStageName(AwakeningStage stage) noexcept
    {
        switch (stage)
        {
        case AwakeningStage::FirstGlimmer: return "First Glimmer";
        case AwakeningStage::SteadySight: return "Steady Sight";
        case AwakeningStage::LivingGlyph: return "Living Glyph";
        case AwakeningStage::CanticleAwakened: return "Canticle Awakened";
        case AwakeningStage::Dormant:
        default: return "Dormant";
        }
    }

    [[nodiscard]] inline uint8_t CountLitTumblers(uint8_t tumblerMask) noexcept
    {
        return static_cast<uint8_t>(std::popcount(static_cast<unsigned>(tumblerMask & TumblerBits::kAllFour)));
    }

    [[nodiscard]] inline uint8_t CountVisitedSanctuaries(uint8_t sanctuaryMask) noexcept
    {
        return static_cast<uint8_t>(std::popcount(static_cast<unsigned>(sanctuaryMask & 0x7Fu)));
    }

    [[nodiscard]] inline AwakeningStage ComputeAwakeningStage(const AwakeningState& state,
                                                              UnlockMode mode) noexcept
    {
        if (mode == UnlockMode::Sandbox)
        {
            return AwakeningStage::CanticleAwakened;
        }
        const uint8_t masked = static_cast<uint8_t>(state.tumblerMask & TumblerBits::kAllFour);
        if (state.ceremonyCompleted && masked == TumblerBits::kAllFour)
        {
            return AwakeningStage::CanticleAwakened;
        }
        const uint8_t lit = CountLitTumblers(masked);
        if (lit == 0u) return AwakeningStage::Dormant;
        if (lit == 1u) return AwakeningStage::FirstGlimmer;
        if (lit == 2u) return AwakeningStage::SteadySight;
        return AwakeningStage::LivingGlyph; // 3 or 4 tumblers prior to final Sanctuary Ceremony
    }

    [[nodiscard]] inline StageCapabilities ResolveStageCapabilities(const AwakeningState& state,
                                                                    UnlockMode mode) noexcept
    {
        StageCapabilities cap{};
        cap.stage = ComputeAwakeningStage(state, mode);
        if (mode == UnlockMode::Ritual && (!state.diegeticToggleOn || cap.stage == AwakeningStage::Dormant))
        {
            return cap;
        }

        cap.expandedActive = true;
        cap.allowReticle = true;
        cap.allowSocialMemory = true;
        cap.allowBookStudy = true;
        cap.maxPresenceCap = 39.0f;
        cap.maxScholarshipRank = ScholarshipRank::Novice;

        if (cap.stage >= AwakeningStage::SteadySight)
        {
            cap.allowPersuasionProbes = true;
            cap.allowEyeContactGreeting = true;
            cap.allowSternEvasiveDemeanor = true;
            cap.maxPresenceCap = 59.0f;
            cap.maxScholarshipRank = ScholarshipRank::Acolyte;
        }
        if (cap.stage >= AwakeningStage::LivingGlyph)
        {
            cap.allowTransactions = true;
            cap.allowStaringContest = true;
            cap.allowKindredSight = true;
            cap.maxPresenceCap = 79.0f;
            cap.maxScholarshipRank = ScholarshipRank::Prelate;
        }
        if (cap.stage >= AwakeningStage::CanticleAwakened)
        {
            cap.allowPredatoryDemeanor = true;
            cap.allowFullSignatureResonance = true;
            cap.maxPresenceCap = 100.0f;
            cap.maxScholarshipRank = ScholarshipRank::Canticle;
        }
        return cap;
    }

    /// @brief Clamps a requested Demeanor to the player's current AwakeningStage capabilities.
    [[nodiscard]] constexpr Demeanor ClampDemeanorToStage(Demeanor requested,
                                                          const StageCapabilities& cap) noexcept
    {
        if (requested == Demeanor::Predatory && !cap.allowPredatoryDemeanor)
        {
            return cap.allowSternEvasiveDemeanor ? Demeanor::Stern : Demeanor::Courteous;
        }
        if ((requested == Demeanor::Stern || requested == Demeanor::Evasive) &&
            !cap.allowSternEvasiveDemeanor)
        {
            return Demeanor::Courteous;
        }
        return requested;
    }

    namespace AwakeningDetail
    {
        [[nodiscard]] inline bool ContainsICase(std::string_view haystack,
                                                std::string_view needle) noexcept
        {
            if (needle.empty() || haystack.size() < needle.size())
            {
                return false;
            }
            for (size_t i = 0; i + needle.size() <= haystack.size(); ++i)
            {
                bool match = true;
                for (size_t j = 0; j < needle.size(); ++j)
                {
                    if (Detail::AsciiLower(haystack[i + j]) != Detail::AsciiLower(needle[j]))
                    {
                        match = false;
                        break;
                    }
                }
                if (match) return true;
            }
            return false;
        }
    } // namespace AwakeningDetail

    /// @brief Matches a Skyrim location/cell name to one of the 7 base-Skyrim Sanctuaries.
    [[nodiscard]] inline uint8_t MatchSanctuaryBit(std::string_view locationName) noexcept
    {
        if (locationName.empty()) return SanctuaryBits::kNone;
        if (AwakeningDetail::ContainsICase(locationName, "Sleeping Giant"))
            return SanctuaryBits::kSleepingGiantInn;
        if (AwakeningDetail::ContainsICase(locationName, "Temple of Kynareth"))
            return SanctuaryBits::kTempleOfKynareth;
        if (AwakeningDetail::ContainsICase(locationName, "Eldergleam"))
            return SanctuaryBits::kEldergleam;
        if (AwakeningDetail::ContainsICase(locationName, "Temple of Mara"))
            return SanctuaryBits::kTempleOfMara;
        if (AwakeningDetail::ContainsICase(locationName, "Temple of the Divines"))
            return SanctuaryBits::kTempleOfTheDivines;
        if (AwakeningDetail::ContainsICase(locationName, "High Hrothgar"))
            return SanctuaryBits::kHighHrothgar;
        if (AwakeningDetail::ContainsICase(locationName, "Arcanaeum"))
            return SanctuaryBits::kArcanaeum;
        return SanctuaryBits::kNone;
    }

    /// @brief True for bespoke Canticle or base-Skyrim Elder Scroll / Moth lore books.
    [[nodiscard]] inline bool IsCanticleOrScrollBookTitle(std::string_view title) noexcept
    {
        if (title.empty()) return false;
        return AwakeningDetail::ContainsICase(title, "First Canticle") ||
               AwakeningDetail::ContainsICase(title, "Elder Scroll") ||
               AwakeningDetail::ContainsICase(title, "Accounting of the Scrolls") ||
               AwakeningDetail::ContainsICase(title, "Book of the Dragonborn") ||
               AwakeningDetail::ContainsICase(title, "Ancestor Moth") ||
               AwakeningDetail::ContainsICase(title, "Moth");
    }

    /// @brief Classifies bespoke ESL artifacts and vanilla Skyrim.esm fallback focuses by name & type.
    [[nodiscard]] inline RelicKind ClassifyRelicByName(std::string_view itemName, bool isBook,
                                                       bool isArmor, bool isMisc) noexcept
    {
        if (itemName.empty()) return RelicKind::None;

        if (isBook)
        {
            return IsCanticleOrScrollBookTitle(itemName) ? RelicKind::CanticleBook
                                                         : RelicKind::None;
        }

        // Bespoke optional ESL player toggle wearables
        if (isArmor &&
            (AwakeningDetail::ContainsICase(itemName, "Amulet of the Ancestor Moth") ||
             AwakeningDetail::ContainsICase(itemName, "Moth-Silk Browband") ||
             AwakeningDetail::ContainsICase(itemName, "First Canticle")))
        {
            return RelicKind::PlayerToggleRelic;
        }

        // Bespoke optional ESL giftable Sight relics (Player, Followers, NPCs)
        if ((isArmor || isMisc) &&
            (AwakeningDetail::ContainsICase(itemName, "Ring of Quiet Sight") ||
             AwakeningDetail::ContainsICase(itemName, "Silver Moth Medallion") ||
             AwakeningDetail::ContainsICase(itemName, "Moth-Silk Ribbon")))
        {
            return RelicKind::GiftableSightRelic;
        }

        // Vanilla Skyrim.esm fallback wearables (zero DLC, zero ESP)
        if (isArmor &&
            (AwakeningDetail::ContainsICase(itemName, "Amulet of Julianos") ||
             AwakeningDetail::ContainsICase(itemName, "Amulet of Mara") ||
             AwakeningDetail::ContainsICase(itemName, "Amulet of Akatosh") ||
             AwakeningDetail::ContainsICase(itemName, "Amulet of Kynareth") ||
             AwakeningDetail::ContainsICase(itemName, "Amulet of Dibella") ||
             AwakeningDetail::ContainsICase(itemName, "Circlet")))
        {
            return RelicKind::VanillaFocus;
        }

        return RelicKind::None;
    }

    [[nodiscard]] inline AwakeningEventResult MarkTumbler(AwakeningState& state, uint8_t bit,
                                                          UnlockMode mode,
                                                          std::string_view flavorText)
    {
        AwakeningEventResult res{};
        res.prevStage = ComputeAwakeningStage(state, mode);
        const uint8_t maskedBit = static_cast<uint8_t>(bit & TumblerBits::kAllFour);
        if (maskedBit == 0u || (state.tumblerMask & maskedBit) != 0u)
        {
            res.newStage = res.prevStage;
            return res;
        }

        state.tumblerMask = static_cast<uint8_t>(state.tumblerMask | maskedBit);
        state.diegeticToggleOn = true;
        res.progressed = true;
        res.newStage = ComputeAwakeningStage(state, mode);
        res.stageAdvanced = (res.newStage > res.prevStage);

        const uint8_t lit = CountLitTumblers(state.tumblerMask);
        if (lit == 4u && !state.ceremonyCompleted)
        {
            res.whisper = std::format(
                "{} - all four keys align; seek a Sanctuary to complete the First Canticle.",
                flavorText);
            res.whisperColorRgb = 0xFFD966u;
        }
        else if (res.stageAdvanced)
        {
            res.whisper = std::format("{} - TrueGaze awakens [Stage {}: {}].", flavorText,
                                      static_cast<unsigned>(res.newStage),
                                      AwakeningStageName(res.newStage));
            res.whisperColorRgb = 0xFFD966u;
        }
        else
        {
            res.whisper = std::format("{} ({}/4 keys of the First Canticle).", flavorText, lit);
            res.whisperColorRgb = 0xA6D8FFu;
        }
        return res;
    }

    [[nodiscard]] inline AwakeningEventResult RecordSanctuaryStillness(AwakeningState& state,
                                                                       uint8_t sanctuaryBit,
                                                                       UnlockMode mode)
    {
        AwakeningEventResult res{};
        res.prevStage = ComputeAwakeningStage(state, mode);
        res.newStage = res.prevStage;
        if (sanctuaryBit == SanctuaryBits::kNone)
        {
            return res;
        }

        const bool newlyVisited = (state.visitedSanctuariesMask & sanctuaryBit) == 0u;
        if (!newlyVisited)
        {
            return res;
        }
        state.visitedSanctuariesMask = static_cast<uint8_t>(state.visitedSanctuariesMask | sanctuaryBit);
        const uint8_t count = CountVisitedSanctuaries(state.visitedSanctuariesMask);
        if (count >= 2u && (state.tumblerMask & TumblerBits::kPilgrimage) == 0u)
        {
            return MarkTumbler(state, TumblerBits::kPilgrimage, mode,
                               "Two sanctuaries have stilled your sight");
        }
        res.progressed = true;
        res.whisper = "The sanctuary's hush steadies your eyes (1/2 Pilgrimage shrines).";
        res.whisperColorRgb = 0xA6D8FFu;
        return res;
    }

    [[nodiscard]] inline AwakeningEventResult TryCompleteSanctuaryCeremony(
        AwakeningState& state, bool inSanctuary, bool wearingFocus, bool carryingStudiedBook,
        UnlockMode mode)
    {
        AwakeningEventResult res{};
        res.prevStage = ComputeAwakeningStage(state, mode);
        res.newStage = res.prevStage;

        if (state.ceremonyCompleted)
        {
            return res;
        }
        if ((state.tumblerMask & TumblerBits::kAllFour) != TumblerBits::kAllFour)
        {
            return res;
        }
        if (!inSanctuary || !wearingFocus || !carryingStudiedBook)
        {
            return res;
        }

        state.ceremonyCompleted = true;
        state.diegeticToggleOn = true;
        res.progressed = true;
        res.newStage = ComputeAwakeningStage(state, mode);
        res.stageAdvanced = (res.newStage > res.prevStage);
        res.whisper =
            "The four keys turn as one - the First Canticle awakens in full (+5.0 Presence).";
        res.whisperColorRgb = 0xFFD966u;
        return res;
    }

    inline void ForceUnlockAllForConsole(AwakeningState& state) noexcept
    {
        state.tumblerMask = TumblerBits::kAllFour;
        state.visitedSanctuariesMask =
            static_cast<uint8_t>(SanctuaryBits::kSleepingGiantInn | SanctuaryBits::kTempleOfKynareth);
        state.ceremonyCompleted = true;
        state.diegeticToggleOn = true;
    }

    [[nodiscard]] inline std::string FormatTumblerGlyphs(const AwakeningState& state)
    {
        return std::format(
            "[W:{} V:{} S:{} P:{} | Cer:{}]",
            (state.tumblerMask & TumblerBits::kWord) ? "x" : ".",
            (state.tumblerMask & TumblerBits::kVessel) ? "x" : ".",
            (state.tumblerMask & TumblerBits::kSharedSight) ? "x" : ".",
            (state.tumblerMask & TumblerBits::kPilgrimage) ? "x" : ".",
            state.ceremonyCompleted ? "x" : ".");
    }

    /// @brief Formats the Skyrim parchment MessageBox checklist shown when casting
    /// Sight of the First Canticle before all Awakening requirements are satisfied.
    [[nodiscard]] inline std::string FormatRitualStatusChecklist(
        const AwakeningState& state, UnlockMode mode, bool inSanctuary,
        std::string_view sanctuaryName, bool wearingFocus, bool carryingStudiedBook)
    {
        const auto stage = ComputeAwakeningStage(state, mode);
        const uint8_t lit = CountLitTumblers(state.tumblerMask);
        const uint8_t shrines = CountVisitedSanctuaries(state.visitedSanctuariesMask);

        const bool hasWord   = (state.tumblerMask & TumblerBits::kWord) != 0u;
        const bool hasVessel = (state.tumblerMask & TumblerBits::kVessel) != 0u;
        const bool hasShared = (state.tumblerMask & TumblerBits::kSharedSight) != 0u;
        const bool hasPilg   = (state.tumblerMask & TumblerBits::kPilgrimage) != 0u;

        std::string msg = std::format(
            "Sight of the First Canticle\n"
            "Stage {}: {} ({}/4 Keys Lit)\n\n"
            "The Four Keys (any order):\n",
            static_cast<unsigned>(stage), AwakeningStageName(stage), lit);

        msg += hasWord
                   ? "[X] The Word - Studied\n"
                   : "[ ] The Word - Read 'The First Canticle of the Eye' (or an Elder Scroll "
                     "tome) slowly from start to finish.\n";

        msg += hasVessel
                   ? "[X] The Vessel - Attuned\n"
                   : "[ ] The Vessel - Wear a Moth relic (or Divine Amulet/Circlet) and hold an "
                     "NPC's gaze for 3 breaths.\n";

        msg += hasShared
                   ? "[X] Shared Sight - Kindred Awakened\n"
                   : "[ ] Shared Sight - Trade OR drop a Ring of Quiet Sight, Silver Moth "
                     "Medallion, or Moth-Silk Ribbon beside a friendly NPC/follower and meet "
                     "their eyes.\n";

        if (hasPilg)
        {
            msg += std::format("[X] The Pilgrimage - Completed ({}/2 Sanctuaries)\n", shrines);
        }
        else
        {
            msg += std::format(
                "[ ] The Pilgrimage ({}/2) - Hold stillness in 2 Sanctuaries (Temple of "
                "Kynareth, Sleeping Giant Inn, Eldergleam, Temple of Mara/Divines, High "
                "Hrothgar, or Arcanaeum).\n",
                shrines);
        }

        msg += "\nFinal Sanctuary Ceremony:\n";
        if (inSanctuary)
        {
            msg += std::format("[X] Inside Sanctuary ({})\n",
                               sanctuaryName.empty() ? "Sacred Ground" : sanctuaryName);
        }
        else
        {
            msg += "[ ] Stand inside a Skyrim Sanctuary\n";
        }
        msg += wearingFocus ? "[X] Wearing a Vessel relic\n"
                            : "[ ] Equip a Vessel (Amulet, Browband, or Circlet)\n";
        msg += carryingStudiedBook
                   ? "[X] Carrying a studied Canticle tome\n"
                   : "[ ] Carry your studied Canticle book in inventory\n";

        if (lit < 4u)
        {
            msg += "\nThe Sight cannot awaken in full until all four keys align.";
        }
        else
        {
            msg += "\nAll four keys are lit! Complete the remaining ceremony checks above and "
                   "cast again to awaken Stage 4.";
        }
        return msg;
    }

    /// @brief Formats the living Innkeeper's Note parchment (TGZ_Note_CassianRumor) with TrueGaze
    /// lore intro AND a live-updating [X]/[ ] checklist matching FormatRitualStatusChecklist.
    /// Uses pure ASCII & clean vanilla Skyrim BookMenu note markup (<p align="left">).
    [[nodiscard]] inline std::string FormatCassianLivingNoteMarkup(
        const AwakeningState& state, UnlockMode mode, bool inSanctuary,
        std::string_view sanctuaryName, bool wearingFocus, bool carryingStudiedBook,
        float presenceMax, ScholarshipRank scholarshipRank)
    {
        const auto stage = ComputeAwakeningStage(state, mode);
        const uint8_t lit = CountLitTumblers(state.tumblerMask);
        const uint8_t shrines = CountVisitedSanctuaries(state.visitedSanctuariesMask);

        const bool hasWord   = (state.tumblerMask & TumblerBits::kWord) != 0u;
        const bool hasVessel = (state.tumblerMask & TumblerBits::kVessel) != 0u;
        const bool hasShared = (state.tumblerMask & TumblerBits::kSharedSight) != 0u;
        const bool hasPilg   = (state.tumblerMask & TumblerBits::kPilgrimage) != 0u;

        std::string out =
            "<p align=\"left\">\r\n"
            "Brother Cassian's Ledger of the TrueGaze\r\n\r\n"
            "Before an Ancestor Moth acolyte reads an Elder Scroll, they learn the First "
            "Canticle -- stillness of the eye that reads intention, steadies barter and "
            "persuasion, anticipates blows in battle, and meets beasts and Dragons without "
            "flinching.\r\n\r\n"
            "The lingering Presence of my stay at the Sleeping Giant Inn has stirred your Sight. "
            "Seek 'The First Canticle of the Eye' in Winterhold's Arcanaeum (or any Elder Scroll "
            "tome in Skyrim) and read it slowly from start to finish to learn the Sight of the "
            "First Canticle spell. The other Moth relics rest across Skyrim's Sanctuaries: "
            "Temple of Kynareth (Amulet), Eldergleam (Browband), Temple of Mara (Ring), Temple "
            "of the Divines (Medallion), and High Hrothgar (Ribbon).\r\n\r\n";

        out += std::format(
            "--- Awakening Status ---\r\n"
            "Stage {}: {} ({}/4 Keys Lit)\r\n"
            "Presence: {:.0f}/100  |  Moth's Stillness: {}\r\n\r\n"
            "The Four Keys (complete in any order):\r\n",
            static_cast<unsigned>(stage), AwakeningStageName(stage), lit, presenceMax,
            ScholarshipRankName(scholarshipRank));

        out += hasWord
                   ? "[X] The Word -- Studied\r\n"
                   : "[ ] The Word -- Read 'The First Canticle of the Eye' (or an Elder Scroll "
                     "tome) slowly from start to finish.\r\n";

        out += hasVessel
                   ? "[X] The Vessel -- Attuned\r\n"
                   : "[ ] The Vessel -- Wear the Amulet of the Ancestor Moth, Moth-Silk Browband, "
                     "or a Divine Amulet/Circlet and hold an NPC's gaze for 3 breaths.\r\n";

        out += hasShared
                   ? "[X] Shared Sight -- Kindred Awakened\r\n"
                   : "[ ] Shared Sight -- Trade or drop a Ring of Quiet Sight, Silver Moth "
                     "Medallion, or Moth-Silk Ribbon beside a friendly NPC and meet their eyes.\r\n";

        if (hasPilg)
        {
            out += std::format("[X] The Pilgrimage -- Completed ({}/2 Sanctuaries)\r\n", shrines);
        }
        else
        {
            out += std::format(
                "[ ] The Pilgrimage ({}/2) -- Hold calm stillness in 2 Skyrim Sanctuaries.\r\n",
                shrines);
        }

        out += "\r\nFinal Sanctuary Ceremony:\r\n";
        if (state.ceremonyCompleted || stage == AwakeningStage::CanticleAwakened)
        {
            out += "[X] Final Sanctuary Ceremony -- Complete!\r\n\r\n"
                   "All four keys stand aligned. You have awakened the full TrueGaze. Cast "
                   "Sight of the First Canticle (or equip/unequip your Moth Vessel) anytime to "
                   "veil or unveil the Sight.\r\n";
        }
        else
        {
            if (inSanctuary)
            {
                out += std::format("[X] Inside Sanctuary ({})\r\n",
                                   sanctuaryName.empty() ? "Sacred Ground" : sanctuaryName);
            }
            else
            {
                out += "[ ] Stand inside any Skyrim Sanctuary\r\n";
            }
            out += wearingFocus ? "[X] Wearing a Vessel relic\r\n"
                                : "[ ] Equip a Vessel (Amulet, Browband, or Circlet)\r\n";
            out += carryingStudiedBook
                       ? "[X] Carrying your studied Canticle tome\r\n"
                       : "[ ] Carry your studied Canticle book in inventory\r\n";

            if (lit < 4u)
            {
                out += "\r\nOpen this note anytime to check your progress. The Sight awakens in "
                       "full once all four keys align.\r\n";
            }
            else
            {
                out += "\r\nAll four keys are lit! Stand inside any Sanctuary wearing your Vessel "
                       "with your Canticle book and cast Sight of the First Canticle to complete "
                       "Stage 4.\r\n";
            }
        }

        out += "\r\n-- Brother Cassian\r\n</p>";
        return out;
    }

    struct DevDropResult
    {
        uint32_t itemsDropped{0};
        bool spellGranted{false};
        bool usedEslItems{false};
    };

    /// @brief Game-side SKSE/RE bridge for zero-script relic equip, spell-cast, left-hand open
    /// Canticle book visual (1st & 3rd person), and follower scanning.
    class AwakeningRitual
    {
    public:
        static AwakeningRitual& Get() noexcept;

        void Install() noexcept;
        void Reset() noexcept;

        [[nodiscard]] bool IsEslPluginLoaded() const noexcept;
        [[nodiscard]] bool IsCassianNoteFormId(uint32_t formId,
                                               std::string_view title = {}) const noexcept;
        [[nodiscard]] bool IsPlayerWearingFocus() const noexcept;
        [[nodiscard]] bool IsPlayerCarryingStudiedCanticle() const noexcept;
        [[nodiscard]] uint8_t CurrentPlayerSanctuaryBit() const noexcept;
        [[nodiscard]] std::string CurrentPlayerSanctuaryName() const;
        [[nodiscard]] bool DoesActorCarrySightRelic(uint32_t actorFormId) const noexcept;
        [[nodiscard]] bool TryOfferNearbySightRelicToActor(uint32_t actorFormId) const noexcept;
        [[nodiscard]] bool ConsumePendingCeremonyCast() noexcept;
        [[nodiscard]] bool ConsumePendingDiegeticToggle(bool& outNewState) noexcept;
        void UpdateLeftHandBookVisual(bool expandedEnabled) noexcept;
        void DetachLeftHandBookVisual() noexcept;
        [[nodiscard]] bool EnsureCanticleSpellLearned() noexcept;
        [[nodiscard]] uint32_t GetInnRumorInfoFormId() const noexcept;
        [[nodiscard]] uint32_t GetInnNoteInfoFormId() const noexcept;
        void EnsurePilgrimageQuestRunning() noexcept;
        void SyncPilgrimageQuestProgress(const AwakeningState& state,
                                         bool startJournalIfEligible) noexcept;
        void SetInnRumorInfoSaidOnce(bool said) noexcept;
        [[nodiscard]] bool IsPlayerCarryingCassianNote() const noexcept;
        void MarkCassianNoteOpened() noexcept;
        [[nodiscard]] bool ConsumeCassianNoteOpened() noexcept;
        [[nodiscard]] bool PlaceNoteOnSleepingGiantBar() noexcept;
        void RemovePlacedBarNoteIfStillInWorld() noexcept;
        [[nodiscard]] bool DispatchCourierWithCassianNote() noexcept;
        [[nodiscard]] bool GrantInnNoteToPlayerOnly(const char* source = "OrgnarDialogue") noexcept;
        [[nodiscard]] bool GiveInnNoteToPlayerAndSpeak(uint32_t speakerActorFormId) noexcept;
        [[nodiscard]] bool BestowSanctuaryRelicIfEslLoaded(uint8_t sanctuaryBit,
                                                           std::string& outRelicName) noexcept;
        DevDropResult DropAllDevItemsInFrontOfPlayer() noexcept;
    };
} // namespace TrueGaze::ExpandedPlay
