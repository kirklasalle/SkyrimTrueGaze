#pragma once

#include "ExpandedPlay/ExpandedPlayTypes.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace TrueGaze::ExpandedPlay
{
    struct SpreadMetrics
    {
        uint32_t wordCount{0};
        float minDwellSec{2.5f};
    };

    struct BookStudySummary
    {
        uint32_t bookFormId{0};
        uint32_t totalSpreads{0};
        uint32_t newlyCreditedSpreads{0};
        uint32_t refreshedSpreads{0};
        uint32_t speedFlippedSpreads{0};
        uint32_t totalStudiedSpreadsInBook{0};
        float presenceAwarded{0.0f};
        bool fullStudyJustCompleted{false};
        bool skillBookDeepCraftGranted{false};
        uint32_t skillBoostActorValue{0};
        ScholarshipRank newScholarshipRank{ScholarshipRank::Novice};
    };

    namespace BookParseDetail
    {
        [[nodiscard]] constexpr bool IsSpace(char c) noexcept
        {
            return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f';
        }

        [[nodiscard]] inline bool ContainsIgnoreCase(std::string_view haystack,
                                                     std::string_view needle) noexcept
        {
            if (needle.empty() || haystack.size() < needle.size())
            {
                return false;
            }
            for (size_t i = 0; i <= haystack.size() - needle.size(); ++i)
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
                if (match)
                {
                    return true;
                }
            }
            return false;
        }

        [[nodiscard]] inline size_t FindIgnoreCase(std::string_view haystack,
                                                   std::string_view needle,
                                                   size_t startPos = 0u) noexcept
        {
            if (needle.empty() || startPos >= haystack.size() ||
                haystack.size() - startPos < needle.size())
            {
                return std::string_view::npos;
            }
            for (size_t i = startPos; i <= haystack.size() - needle.size(); ++i)
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
                if (match)
                {
                    return i;
                }
            }
            return std::string_view::npos;
        }

        [[nodiscard]] inline std::string StripXmlTags(std::string_view raw)
        {
            std::string clean;
            clean.reserve(raw.size());
            bool inTag = false;
            for (char c : raw)
            {
                if (c == '<')
                {
                    inTag = true;
                    clean.push_back(' ');
                    continue;
                }
                if (c == '>')
                {
                    inTag = false;
                    clean.push_back(' ');
                    continue;
                }
                if (!inTag)
                {
                    clean.push_back(c);
                }
            }
            return clean;
        }

        [[nodiscard]] inline uint32_t CountWords(std::string_view text) noexcept
        {
            uint32_t words = 0u;
            bool inWord    = false;
            for (char c : text)
            {
                if (IsSpace(c))
                {
                    inWord = false;
                }
                else if (!inWord)
                {
                    inWord = true;
                    ++words;
                }
            }
            return words;
        }
    } // namespace BookParseDetail

    /// @brief Computes minimum credible reading dwell for a spread (~390 WPM floor).
    [[nodiscard]] inline float ComputeMinSpreadDwell(uint32_t wordCount) noexcept
    {
        return std::clamp(static_cast<float>(wordCount) / 6.5f, 2.5f, 12.0f);
    }

    /// @brief Splits Skyrim book text into page spreads (<= 64) with word counts and min dwell.
    [[nodiscard]] inline std::vector<SpreadMetrics> ParseBookSpreads(std::string_view rawDescription)
    {
        constexpr size_t kMaxSpreads        = 64u;
        constexpr uint32_t kWordsPerSpread  = 220u;
        constexpr std::string_view kBreak   = "[pagebreak]";

        const std::string clean = BookParseDetail::StripXmlTags(rawDescription);
        std::vector<SpreadMetrics> spreads;

        size_t pos = 0u;
        while (pos <= clean.size() && spreads.size() < kMaxSpreads)
        {
            const size_t next = BookParseDetail::FindIgnoreCase(clean, kBreak, pos);
            const size_t end  = (next == std::string_view::npos) ? clean.size() : next;
            const std::string_view segment(clean.data() + pos, end - pos);

            const uint32_t words = BookParseDetail::CountWords(segment);
            if (words > 0u)
            {
                if (words <= kWordsPerSpread)
                {
                    spreads.push_back(SpreadMetrics{words, ComputeMinSpreadDwell(words)});
                }
                else
                {
                    uint32_t remaining = words;
                    while (remaining > 0u && spreads.size() < kMaxSpreads)
                    {
                        const uint32_t chunk = std::min(remaining, kWordsPerSpread);
                        spreads.push_back(SpreadMetrics{chunk, ComputeMinSpreadDwell(chunk)});
                        remaining -= chunk;
                    }
                }
            }

            if (next == std::string_view::npos)
            {
                break;
            }
            pos = next + kBreak.size();
        }

        if (spreads.empty())
        {
            spreads.push_back(SpreadMetrics{0u, 2.5f});
        }
        return spreads;
    }

    /// @brief Tracks spread dwell & anti-cheese guards for a single Book Menu reading session.
    class BookStudySession
    {
    public:
        static constexpr float kSpeedFlipCutoffSec  = 1.5f;
        static constexpr float kAfkSpreadCapSec     = 45.0f;
        static constexpr float kRefreshCooldownDays = 7.0f;
        static constexpr float kFullStudyThreshold  = 0.80f;

        void Begin(uint32_t bookFormId, std::string_view description, bool teachesSkill,
                   uint32_t skillActorValue, bool isRareCanticle)
        {
            _active          = (bookFormId != 0u);
            _bookFormId      = bookFormId;
            _teachesSkill    = teachesSkill;
            _skillActorValue = skillActorValue;
            _isRareCanticle  = isRareCanticle;
            _speedFlips      = 0u;

            if (_active)
            {
                _spreads = ParseBookSpreads(description);
                _spreadDwellSec.assign(_spreads.size(), 0.0f);
                _hadScan.assign(_spreads.size(), 0u);
            }
            else
            {
                _spreads.clear();
                _spreadDwellSec.clear();
                _hadScan.clear();
            }
        }

        void RecordSpreadDwell(uint32_t spreadIndex, float elapsedSec,
                               bool hadTrueReaderScan = false) noexcept
        {
            if (!_active || spreadIndex >= _spreads.size() || !std::isfinite(elapsedSec) ||
                elapsedSec <= 0.0f)
            {
                return;
            }

            if (elapsedSec < kSpeedFlipCutoffSec)
            {
                ++_speedFlips;
                return;
            }

            const float bounded = std::min(elapsedSec, kAfkSpreadCapSec);
            _spreadDwellSec[spreadIndex] =
                std::min(kAfkSpreadCapSec, _spreadDwellSec[spreadIndex] + bounded);

            if (hadTrueReaderScan)
            {
                _hadScan[spreadIndex] = 1u;
            }
        }

        [[nodiscard]] bool IsActive() const noexcept { return _active; }
        [[nodiscard]] uint32_t BookFormId() const noexcept { return _bookFormId; }
        [[nodiscard]] bool IsRareCanticle() const noexcept { return _isRareCanticle; }
        [[nodiscard]] size_t SpreadCount() const noexcept { return _spreads.size(); }
        [[nodiscard]] float SpreadMinDwell(size_t idx) const noexcept
        {
            return (idx < _spreads.size()) ? _spreads[idx].minDwellSec : 2.5f;
        }
        [[nodiscard]] float SpreadAccumulatedDwell(size_t idx) const noexcept
        {
            return (idx < _spreadDwellSec.size()) ? _spreadDwellSec[idx] : 0.0f;
        }

        BookStudySummary Finalize(BookStudyEntry& entry, uint32_t& totalBooksFullyStudied,
                                  float currentGameDays) noexcept
        {
            BookStudySummary summary{};
            if (!_active)
            {
                return summary;
            }

            const float day =
                (std::isfinite(currentGameDays) && currentGameDays >= 0.0f) ? currentGameDays : 0.0f;

            entry.bookFormId   = _bookFormId;
            entry.totalSpreads = static_cast<uint8_t>(_spreads.size());

            summary.bookFormId          = _bookFormId;
            summary.totalSpreads        = static_cast<uint32_t>(_spreads.size());
            summary.speedFlippedSpreads = _speedFlips;

            const bool refreshEligible =
                entry.fullStudyAwarded && ((day - entry.lastStudiedGameDays) >= kRefreshCooldownDays);
            const float basePerSpread = _isRareCanticle ? 0.15f : 0.05f;

            for (size_t i = 0; i < _spreads.size(); ++i)
            {
                if (_spreadDwellSec[i] < _spreads[i].minDwellSec)
                {
                    continue;
                }

                const uint64_t bit   = 1ULL << i;
                const float scanMult = (_hadScan[i] != 0u) ? 1.15f : 1.0f;

                if ((entry.pagesReadMask & bit) == 0ULL)
                {
                    entry.pagesReadMask |= bit;
                    ++summary.newlyCreditedSpreads;
                    summary.presenceAwarded += basePerSpread * scanMult;
                }
                else if (refreshEligible)
                {
                    ++summary.refreshedSpreads;
                    summary.presenceAwarded += basePerSpread * 0.25f * scanMult;
                }
            }

            summary.totalStudiedSpreadsInBook =
                static_cast<uint32_t>(std::popcount(entry.pagesReadMask));
            const float completionRatio =
                static_cast<float>(summary.totalStudiedSpreadsInBook) /
                static_cast<float>(_spreads.size());

            if (!entry.fullStudyAwarded && completionRatio >= kFullStudyThreshold)
            {
                entry.fullStudyAwarded        = true;
                summary.fullStudyJustCompleted = true;
                ++totalBooksFullyStudied;
                summary.presenceAwarded += _teachesSkill ? 0.50f : 0.25f;

                if (_teachesSkill && _skillActorValue != 0u)
                {
                    summary.skillBookDeepCraftGranted = true;
                    summary.skillBoostActorValue      = _skillActorValue;
                }
            }

            if (summary.newlyCreditedSpreads > 0u || summary.refreshedSpreads > 0u ||
                summary.fullStudyJustCompleted)
            {
                entry.lastStudiedGameDays = day;
            }

            summary.newScholarshipRank = ClassifyScholarshipRank(totalBooksFullyStudied);
            _active = false;
            return summary;
        }

    private:
        bool _active{false};
        uint32_t _bookFormId{0};
        bool _teachesSkill{false};
        uint32_t _skillActorValue{0};
        bool _isRareCanticle{false};
        uint32_t _speedFlips{0};
        std::vector<SpreadMetrics> _spreads{};
        std::vector<float> _spreadDwellSec{};
        std::vector<uint8_t> _hadScan{};
    };

    /// @brief SDK-free spread timing coordinator that reconciles delayed Scaleform
    /// pagination updates with keyboard/gamepad page-turn inputs.
    class BookSpreadClock
    {
    public:
        void Reset(uint32_t spreadCount, float openTimeSec = 0.0f) noexcept
        {
            _spreadCount         = spreadCount;
            _currentSpread       = 0u;
            _spreadEnteredAtSec  = std::isfinite(openTimeSec) ? openTimeSec : 0.0f;
            _pendingTurnDir      = 0;
            _pendingTurnAtSec    = 0.0f;
            _pendingFromSpread   = 0u;
            _pendingHadScan      = false;
        }

        /// @brief Called when the user presses next (+1) or prev (-1) page before
        /// BookMenu's Flash animation finishes updating the pagination index.
        void NotifyPageTurnInput(int direction, float nowSec, bool hadScanSoFar,
                                 BookStudySession& session) noexcept
        {
            if (_spreadCount <= 1u || direction == 0 || !std::isfinite(nowSec))
            {
                return;
            }
            const int dir = (direction > 0) ? 1 : -1;
            // If a previous turn was still pending, resolve it up to nowSec first.
            if (_pendingTurnDir != 0)
            {
                ResolvePendingTurn(nowSec, hadScanSoFar, session);
            }
            const uint32_t nextSpread = ClampStep(_currentSpread, dir);
            if (nextSpread == _currentSpread)
            {
                return;
            }
            _pendingTurnDir    = dir;
            _pendingTurnAtSec  = nowSec;
            _pendingFromSpread = _currentSpread;
            _pendingHadScan    = hadScanSoFar;
        }

        /// @brief Reconciles a live Scaleform spread index observation.
        void ObserveScaleformSpread(uint32_t observedSpread, float nowSec, bool hadScan,
                                    BookStudySession& session) noexcept
        {
            if (_spreadCount == 0u || !std::isfinite(nowSec))
            {
                return;
            }
            const uint32_t clamped = std::min(observedSpread, _spreadCount - 1u);
            if (clamped == _currentSpread)
            {
                return;
            }

            if (_pendingTurnDir != 0 && _pendingFromSpread == _currentSpread &&
                _pendingTurnAtSec >= _spreadEnteredAtSec && _pendingTurnAtSec <= nowSec)
            {
                const float beforeTurn = _pendingTurnAtSec - _spreadEnteredAtSec;
                session.RecordSpreadDwell(_currentSpread, beforeTurn, _pendingHadScan || hadScan);
                _currentSpread      = clamped;
                _spreadEnteredAtSec = _pendingTurnAtSec;
                _pendingTurnDir     = 0;
                _pendingHadScan     = false;
                return;
            }

            session.RecordSpreadDwell(_currentSpread, nowSec - _spreadEnteredAtSec, hadScan);
            _currentSpread      = clamped;
            _spreadEnteredAtSec = nowSec;
            _pendingTurnDir     = 0;
            _pendingHadScan     = false;
        }

        /// @brief Flushes the active spread (and any unresolved pending turn) on menu close.
        void FlushOnClose(float nowSec, bool hadScan, BookStudySession& session) noexcept
        {
            if (_spreadCount == 0u || !std::isfinite(nowSec))
            {
                return;
            }
            if (_pendingTurnDir != 0)
            {
                ResolvePendingTurn(nowSec, hadScan, session);
                return;
            }
            session.RecordSpreadDwell(_currentSpread, nowSec - _spreadEnteredAtSec, hadScan);
            _spreadEnteredAtSec = nowSec;
        }

        [[nodiscard]] uint32_t CurrentSpread() const noexcept { return _currentSpread; }
        [[nodiscard]] float SpreadEnteredAtSec() const noexcept { return _spreadEnteredAtSec; }
        [[nodiscard]] uint32_t EffectiveSpread() const noexcept
        {
            if (_pendingTurnDir != 0)
            {
                return ClampStep(_currentSpread, _pendingTurnDir);
            }
            return _currentSpread;
        }
        [[nodiscard]] float EffectiveSpreadStartSec() const noexcept
        {
            return (_pendingTurnDir != 0) ? _pendingTurnAtSec : _spreadEnteredAtSec;
        }

    private:
        [[nodiscard]] uint32_t ClampStep(uint32_t spread, int dir) const noexcept
        {
            if (_spreadCount == 0u)
            {
                return 0u;
            }
            if (dir > 0)
            {
                return std::min(spread + 1u, _spreadCount - 1u);
            }
            return (spread > 0u) ? (spread - 1u) : 0u;
        }

        void ResolvePendingTurn(float nowSec, bool hadScanAfterTurn,
                                BookStudySession& session) noexcept
        {
            const float splitAt = std::clamp(_pendingTurnAtSec, _spreadEnteredAtSec, nowSec);
            session.RecordSpreadDwell(_currentSpread, splitAt - _spreadEnteredAtSec,
                                      _pendingHadScan);
            _currentSpread      = ClampStep(_currentSpread, _pendingTurnDir);
            _spreadEnteredAtSec = splitAt;
            _pendingTurnDir     = 0;
            _pendingHadScan     = false;

            const float afterTurn = nowSec - splitAt;
            if (afterTurn > 0.0f)
            {
                session.RecordSpreadDwell(_currentSpread, afterTurn, hadScanAfterTurn);
                _spreadEnteredAtSec = nowSec;
            }
        }

        uint32_t _spreadCount{0};
        uint32_t _currentSpread{0};
        float _spreadEnteredAtSec{0.0f};
        int _pendingTurnDir{0};
        float _pendingTurnAtSec{0.0f};
        uint32_t _pendingFromSpread{0};
        bool _pendingHadScan{false};
    };

    /// @brief Strictly read-only RE::BookMenu observer driving Ocular Contemplation.
    class BookStudyTracker
    {
    public:
        static BookStudyTracker& Get() noexcept;

        void Install() noexcept;
        void Reset() noexcept;
        void TickLiveStudy() noexcept;
        [[nodiscard]] bool IsBookOpen() const noexcept;
        [[nodiscard]] BookStudySummary LastSummary() const noexcept;
    };
} // namespace TrueGaze::ExpandedPlay
