#include "ExpandedPlay/BookStudyTracker.hpp"

#include "Engine/ConfigManager.hpp"
#include "Engine/TraceLogger.hpp"
#include "ExpandedPlay/AwakeningRitual.hpp"
#include "ExpandedPlay/CoSaveSerializer.hpp"
#include "ExpandedPlay/ExpandedPlayModifiers.hpp"
#include "ExpandedPlay/GazeReticle.hpp"
#include "ExpandedPlay/PresenceSignature.hpp"
#include "PCH.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <format>
#include <mutex>

namespace TrueGaze::ExpandedPlay
{
    namespace
    {
        std::mutex g_studyMutex;
        BookStudySummary g_lastSummary{};

#if __has_include(<RE/Skyrim.h>)
        using SteadyClock = std::chrono::steady_clock;

        bool g_installed{false};
        std::atomic<bool> g_bookOpenAtomic{false};
        bool g_menuOpen{false};
        BookStudySession g_session{};
        BookSpreadClock g_clock{};
        SteadyClock::time_point g_menuOpenedAt{};
        SteadyClock::time_point g_lastActivityAt{};
        SteadyClock::time_point g_lastHeartbeatAt{};
        bool g_sawLeftScan{false};
        bool g_sawRightScan{false};
        int32_t g_cursorAccumX{0};
        uint64_t g_priorReadMask{0u};
        uint64_t g_announcedAbsorbedMask{0u};
        uint32_t g_announcedEnteredSpread{0xFFFFFFFFu};
        int32_t g_lastRawScaleformPage{-1};
        bool g_pendingNoteInject{false};
        std::string g_pendingNoteText{};

        [[nodiscard]] bool TryInjectLivingNoteIntoBookMenu(const std::string& markup) noexcept
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui || !ui->IsMenuOpen(RE::BookMenu::MENU_NAME))
            {
                return false;
            }
            auto bookMenu = ui->GetMenu<RE::BookMenu>();
            if (!bookMenu)
            {
                return false;
            }
            auto& rt = bookMenu->GetRuntimeData();
            if (!rt.book || !rt.bookInitialized)
            {
                return false;
            }
            // Force single-sheet Parchment Note mode (bNote=true) so Scaleform book.swf never
            // formats Brother Cassian's Note as a 2-page leather book spread.
            rt.isNote = true;
            std::array<RE::GFxValue, 2> args{RE::GFxValue(markup.c_str()), RE::GFxValue(true)};
            const bool ok =
                rt.book->Invoke("_root.BookMenuInstance.SetBookText", nullptr, args.data(),
                                args.size()) ||
                rt.book->Invoke("_root.BookMenu_mc.SetBookText", nullptr, args.data(),
                                args.size());
            if (ok)
            {
                logger::info("[TrueGaze::ExpandedPlay] Living Awakening Note refreshed in BookMenu "
                             "(isNote=yes, len={}).",
                             markup.size());
            }
            return ok;
        }

        struct LiveStudyNotice
        {
            bool emit{false};
            std::string message{};
            const char* soundId{nullptr};
            bool cancelQueued{false};
            bool logAbsorption{false};
            float liveDwell{0.0f};
            float minDwell{0.0f};
        };

        [[nodiscard]] float ElapsedSinceOpen(SteadyClock::time_point now) noexcept
        {
            return std::max(0.0f, std::chrono::duration<float>(now - g_menuOpenedAt).count());
        }

        void ResetScanTracker() noexcept
        {
            g_sawLeftScan  = false;
            g_sawRightScan = false;
            g_cursorAccumX = 0;
        }

        /// @brief Computes live non-invasive study progress under g_studyMutex (never calls RE::UI
        ///        or SendHUDMessage while holding the mutex).
        [[nodiscard]] LiveStudyNotice ComputeLiveStudyNoticeLocked(
            SteadyClock::time_point now) noexcept
        {
            LiveStudyNotice notice{};
            if (!g_menuOpen || !g_session.IsActive())
            {
                return notice;
            }
            const float nowSec = ElapsedSinceOpen(now);
            const uint32_t spread = g_clock.EffectiveSpread();
            const size_t total = g_session.SpreadCount();
            if (spread >= total || total == 0u)
            {
                return notice;
            }
            const uint64_t bit = (spread < 64u) ? (1ULL << spread) : 0ULL;
            const float minDwell = g_session.SpreadMinDwell(spread);
            const float liveDwell =
                g_session.SpreadAccumulatedDwell(spread) +
                std::max(0.0f, nowSec - g_clock.EffectiveSpreadStartSec());

            const bool alreadyCredited = ((g_priorReadMask | g_announcedAbsorbedMask) & bit) != 0ULL;

            if (!alreadyCredited && liveDwell >= minDwell)
            {
                g_announcedAbsorbedMask |= bit;
                g_announcedEnteredSpread = spread;
                const uint64_t combinedMask = g_priorReadMask | g_announcedAbsorbedMask;
                const uint32_t absorbedCount = static_cast<uint32_t>(std::popcount(combinedMask));
                const bool bookDone = (static_cast<float>(absorbedCount) /
                                       static_cast<float>(total)) >=
                                      BookStudySession::kFullStudyThreshold;
                notice.emit = true;
                notice.message =
                    bookDone
                        ? std::format("TrueGaze Study: Spread {}/{} absorbed [OK] - Book complete! "
                                      "Close to claim.",
                                      spread + 1u, total)
                        : std::format("TrueGaze Study: Spread {}/{} absorbed [OK] - Turn page to "
                                      "continue.",
                                      spread + 1u, total);
                notice.soundId = "UISkillsGlowSD";
                notice.cancelQueued = true;
                notice.logAbsorption = true;
                notice.liveDwell = liveDwell;
                notice.minDwell = minDwell;
                return notice;
            }

            if (g_announcedEnteredSpread != spread)
            {
                g_announcedEnteredSpread = spread;
                notice.emit = true;
                notice.cancelQueued = false;
                if (alreadyCredited)
                {
                    notice.message = std::format(
                        "TrueGaze Study: Spread {}/{} (already absorbed)", spread + 1u, total);
                }
                else
                {
                    const int reqSec = std::max(2, static_cast<int>(std::ceil(minDwell)));
                    notice.message = std::format(
                        "TrueGaze Study: Spread {}/{} (read ~{}s to absorb)...", spread + 1u,
                        total, reqSec);
                }
            }
            return notice;
        }

        void DispatchLiveStudyNotice(const LiveStudyNotice& notice) noexcept
        {
            if (!notice.emit || notice.message.empty())
            {
                return;
            }
            if (auto* ui = RE::UI::GetSingleton();
                !ui || !ui->IsMenuOpen(RE::BookMenu::MENU_NAME))
            {
                return;
            }
            RE::SendHUDMessage::ShowHUDMessage(notice.message.c_str(), notice.soundId,
                                               notice.cancelQueued);
            if (notice.logAbsorption)
            {
                logger::info("[TrueGaze::ExpandedPlay] Live book study: {} (dwell={:.1f}s/{:.1f}s)",
                             notice.message, notice.liveDwell, notice.minDwell);
            }
        }

        /// @brief Recognises standard Skyrim BookMenu page-turn inputs (UserEvent, mouse M1/M2/wheel, keyboard, gamepad).
        [[nodiscard]] int ClassifyPageTurnButton(const RE::ButtonEvent* btn) noexcept
        {
            if (!btn || !btn->IsDown())
            {
                return 0;
            }
            const auto& userEv = btn->QUserEvent();
            if (userEv == "PrevPage")
            {
                return -1;
            }
            if (userEv == "NextPage")
            {
                return +1;
            }

            const auto dev = btn->GetDevice();
            const auto code = btn->GetIDCode();
            if (dev == RE::INPUT_DEVICE::kMouse)
            {
                // Skyrim Book controlmap: M1 (0x0) / WheelDown (0x9) = PrevPage, M2 (0x1) / WheelUp (0x8) = NextPage
                if (code == 0x0u || code == 0x9u)
                {
                    return -1;
                }
                if (code == 0x1u || code == 0x8u)
                {
                    return +1;
                }
            }
            else if (dev == RE::INPUT_DEVICE::kKeyboard)
            {
                // A (0x1E), Left Arrow (0xCB), PageUp (0xC9) -> previous spread
                if (code == 0x1Eu || code == 0xCBu || code == 0xC9u)
                {
                    return -1;
                }
                // D (0x20), Right Arrow (0xCD), PageDown (0xD1) -> next spread
                if (code == 0x20u || code == 0xCDu || code == 0xD1u)
                {
                    return +1;
                }
            }
            else if (dev == RE::INPUT_DEVICE::kGamepad)
            {
                // XINPUT_GAMEPAD_DPAD_LEFT (0x0004), LEFT_SHOULDER (0x0100)
                if (code == 0x0004u || code == 0x0100u)
                {
                    return -1;
                }
                // XINPUT_GAMEPAD_DPAD_RIGHT (0x0008), RIGHT_SHOULDER (0x0200)
                if (code == 0x0008u || code == 0x0200u)
                {
                    return +1;
                }
            }
            return 0;
        }

        [[nodiscard]] float QueryGameDaysPassed() noexcept
        {
            if (auto* cal = RE::Calendar::GetSingleton())
            {
                const float days = cal->GetDaysPassed();
                if (std::isfinite(days) && days >= 0.0f)
                {
                    return days;
                }
            }
            return 0.0f;
        }

        [[nodiscard]] bool IsRareLoreTitle(const char* title) noexcept
        {
            if (!title)
            {
                return false;
            }
            const std::string_view sv(title);
            return IsCanticleOrScrollBookTitle(sv) ||
                   BookParseDetail::ContainsIgnoreCase(sv, "Oghma") ||
                   BookParseDetail::ContainsIgnoreCase(sv, "Black Book");
        }

        [[nodiscard]] bool IsInSleepingGiantInn() noexcept
        {
            if (auto* player = RE::PlayerCharacter::GetSingleton())
            {
                if (auto* loc = player->GetCurrentLocation())
                {
                    if (const char* name = loc->GetName(); name && name[0] != '\0')
                    {
                        return BookParseDetail::ContainsIgnoreCase(name, "Sleeping Giant");
                    }
                }
            }
            return false;
        }

        /// @brief Only returns a Scaleform spread index when book.swf's page counter actually
        /// changes from its last raw reading (so a static iPaginationIndex never resets our clock
        /// back to Spread 0 on mouse movement).
        [[nodiscard]] bool TryProbeChangedScaleformSpread(size_t spreadCount,
                                                          uint32_t& outSpread) noexcept
        {
            auto* ui = RE::UI::GetSingleton();
            if (!ui || spreadCount <= 1u)
            {
                return false;
            }
            auto bookMenu = ui->GetMenu<RE::BookMenu>();
            if (!bookMenu)
            {
                return false;
            }
            auto movie = bookMenu->GetRuntimeData().book;
            if (!movie)
            {
                return false;
            }

            RE::GFxValue pageVal;
            const bool found =
                movie->GetVariable(&pageVal, "_root.BookMenu_mc.iCurrentPage") ||
                movie->GetVariable(&pageVal, "_root.BookMenu_mc.iPaginationIndex") ||
                movie->GetVariable(&pageVal, "_root.iPaginationIndex");
            if (!found || !pageVal.IsNumber() || !std::isfinite(pageVal.GetNumber()))
            {
                return false;
            }

            const int32_t rawInt = static_cast<int32_t>(pageVal.GetNumber());
            if (g_lastRawScaleformPage < 0)
            {
                g_lastRawScaleformPage = rawInt;
                return false;
            }
            if (rawInt == g_lastRawScaleformPage)
            {
                return false;
            }
            g_lastRawScaleformPage = rawInt;
            const int idx = std::max(0, rawInt / 2);
            outSpread = std::min(static_cast<uint32_t>(idx),
                                 static_cast<uint32_t>(spreadCount - 1u));
            return true;
        }

        void FlushOnBookClose(SteadyClock::time_point now) noexcept
        {
            if (!g_menuOpen || !g_session.IsActive())
            {
                return;
            }
            const float nowSec = ElapsedSinceOpen(now);
            const bool hadScan = g_sawLeftScan && g_sawRightScan;
            uint32_t observed = 0u;
            if (TryProbeChangedScaleformSpread(g_session.SpreadCount(), observed))
            {
                g_clock.ObserveScaleformSpread(observed, nowSec, hadScan, g_session);
            }
            g_clock.FlushOnClose(nowSec, hadScan, g_session);
            ResetScanTracker();
        }

        BookStudyEntry& FindOrInsertBookEntry(std::vector<BookStudyEntry>& books,
                                              uint32_t bookFormId, float gameDays)
        {
            for (auto& b : books)
            {
                if (b.bookFormId == bookFormId)
                {
                    return b;
                }
            }
            if (books.size() >= kMaxBookEntries)
            {
                auto oldest = std::min_element(books.begin(), books.end(),
                                               [](const BookStudyEntry& a, const BookStudyEntry& b)
                                               { return a.lastStudiedGameDays < b.lastStudiedGameDays; });
                if (oldest != books.end())
                {
                    books.erase(oldest);
                }
            }
            BookStudyEntry fresh{};
            fresh.bookFormId          = bookFormId;
            fresh.lastStudiedGameDays = gameDays;
            books.push_back(fresh);
            return books.back();
        }

        class BookMenuObserver final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                const RE::MenuOpenCloseEvent* a_event,
                RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
            {
                if (!a_event || a_event->menuName != RE::BookMenu::MENU_NAME)
                {
                    return RE::BSEventNotifyControl::kContinue;
                }

                const auto& cfg = Engine::ConfigManager::GetSingleton();
                if (a_event->opening)
                {
                    auto* book = RE::BookMenu::GetTargetForm();
                    if (!book)
                    {
                        return RE::BSEventNotifyControl::kContinue;
                    }
                    const uint32_t bookId = static_cast<uint32_t>(book->GetFormID());
                    const char* bookTitle = book->GetName() ? book->GetName() : "";

                    // Living Awakening Checklist Note (TGZ_Note_CassianRumor): dynamically inject
                    // the live [X]/[ ] checklist whenever the player opens the note from anywhere.
                    if (AwakeningRitual::Get().IsCassianNoteFormId(bookId, bookTitle))
                    {
                        AwakeningRitual::Get().MarkCassianNoteOpened();
                        const auto saved = CoSaveSerializer::Get().SnapshotState();
                        const auto epCfg = cfg.SnapshotExpandedPlay();
                        const uint8_t sBit = AwakeningRitual::Get().CurrentPlayerSanctuaryBit();
                        const bool inSanctuary = (sBit != SanctuaryBits::kNone);
                        const std::string sName =
                            inSanctuary ? AwakeningRitual::Get().CurrentPlayerSanctuaryName()
                                        : std::string{};
                        const bool wearingFocus = AwakeningRitual::Get().IsPlayerWearingFocus();
                        const bool hasStudied =
                            AwakeningRitual::Get().IsPlayerCarryingStudiedCanticle();
                        const auto rank =
                            ClassifyScholarshipRank(saved.sign.booksFullyStudiedCount);

                        std::string liveMarkup = FormatCassianLivingNoteMarkup(
                            saved.awakening, epCfg.unlockMode, inSanctuary, sName, wearingFocus,
                            hasStudied, saved.sign.presence.maximum, rank);

                        // Update BookMenu's static description buffer immediately (covers frame-0
                        // initialization) AND schedule Scaleform SetBookText injection once
                        // bookInitialized becomes true.
                        RE::BookMenu::GetDescription() = liveMarkup.c_str();
                        const bool injectedNow = TryInjectLivingNoteIntoBookMenu(liveMarkup);
                        {
                            std::lock_guard lock(g_studyMutex);
                            g_menuOpen = false;
                            g_session = {};
                            g_pendingNoteInject = !injectedNow;
                            g_pendingNoteText = injectedNow ? std::string{} : std::move(liveMarkup);
                            g_bookOpenAtomic.store(g_pendingNoteInject, std::memory_order_release);
                        }
                        if (g_pendingNoteInject)
                        {
                            if (auto* ui = RE::UI::GetSingleton();
                                ui && !ui->IsMenuOpen("TrueGazeReticle"))
                            {
                                if (auto* queue = RE::UIMessageQueue::GetSingleton())
                                {
                                    queue->AddMessage(RE::BSFixedString("TrueGazeReticle"),
                                                      RE::UI_MESSAGE_TYPE::kShow, nullptr);
                                }
                            }
                        }
                        logger::info("[TrueGaze::ExpandedPlay] Opened Living Awakening Note "
                                     "({:08X}, injectedImmediate={}).",
                                     bookId, injectedNow ? "yes" : "pending");
                        return RE::BSEventNotifyControl::kContinue;
                    }

                    if (!cfg.expandedPlayEnabled || !cfg.moduleBookStudy)
                    {
                        std::lock_guard lock(g_studyMutex);
                        g_menuOpen = false;
                        return RE::BSEventNotifyControl::kContinue;
                    }

                    const auto& desc = RE::BookMenu::GetDescription();
                    const char* text = desc.c_str() ? desc.c_str() : "";
                    const bool teachesSkill = book->TeachesSkill();
                    const uint32_t skillAv =
                        teachesSkill ? static_cast<uint32_t>(book->GetSkill()) : 0u;
                    const bool isRare = IsRareLoreTitle(book->GetName());

                    size_t spreadCount = 0u;
                    LiveStudyNotice openNotice{};
                    {
                        const auto saved = CoSaveSerializer::Get().SnapshotState();
                        uint64_t priorMask = 0u;
                        for (const auto& b : saved.books)
                        {
                            if (b.bookFormId == bookId)
                            {
                                priorMask = b.pagesReadMask;
                                break;
                            }
                        }
                        std::lock_guard lock(g_studyMutex);
                        g_session.Begin(bookId, text, teachesSkill, skillAv, isRare);
                        g_menuOpen               = g_session.IsActive();
                        g_bookOpenAtomic.store(g_menuOpen, std::memory_order_release);
                        g_menuOpenedAt           = SteadyClock::now();
                        g_lastActivityAt         = g_menuOpenedAt;
                        g_lastHeartbeatAt        = g_menuOpenedAt;
                        spreadCount              = g_session.SpreadCount();
                        g_clock.Reset(static_cast<uint32_t>(spreadCount), 0.0f);
                        g_priorReadMask          = priorMask;
                        g_announcedAbsorbedMask  = 0u;
                        g_announcedEnteredSpread = 0xFFFFFFFFu;
                        g_lastRawScaleformPage   = -1;
                        ResetScanTracker();
                        openNotice = ComputeLiveStudyNoticeLocked(g_menuOpenedAt);
                    }
                    // Ensure GazeReticleMenu stays alive (with empty canvas) while BookMenu is open
                    // so its safe per-frame AdvanceMovie drives TickLiveStudy() without SKSE task loops.
                    if (g_bookOpenAtomic.load(std::memory_order_acquire))
                    {
                        if (auto* ui = RE::UI::GetSingleton();
                            ui && !ui->IsMenuOpen("TrueGazeReticle"))
                        {
                            if (auto* queue = RE::UIMessageQueue::GetSingleton())
                            {
                                queue->AddMessage(RE::BSFixedString("TrueGazeReticle"),
                                                  RE::UI_MESSAGE_TYPE::kShow, nullptr);
                            }
                        }
                    }
                    DispatchLiveStudyNotice(openNotice);

                    const std::string openDetail =
                        std::format("spreads={} skill={} rare={}", spreadCount,
                                    teachesSkill ? "yes" : "no", isRare ? "yes" : "no");
                    Engine::TraceLogger::Get().LogExpandedPlayEvent(
                        Engine::TraceLogger::Get().DurationSeconds(), "BOOK_OPEN", bookId,
                        bookTitle, 0u, 0.0f, openDetail.c_str());
                    logger::info("[TrueGaze::ExpandedPlay] Book opened {:08X} ('{}'): {}", bookId,
                                 bookTitle, openDetail);
                    return RE::BSEventNotifyControl::kContinue;
                }

                // Closing Book Menu: flush final spread and apply end-of-book summation.
                BookStudySummary summary{};
                AwakeningEventResult wordEvent{};
                bool rankUp      = false;
                bool wasRareBook = false;
                {
                    std::lock_guard lock(g_studyMutex);
                    g_pendingNoteInject = false;
                    g_pendingNoteText.clear();
                    if (!g_menuOpen || !g_session.IsActive())
                    {
                        g_menuOpen = false;
                        g_bookOpenAtomic.store(false, std::memory_order_release);
                        return RE::BSEventNotifyControl::kContinue;
                    }

                    const auto now = SteadyClock::now();
                    FlushOnBookClose(now);
                    g_menuOpen = false;
                    g_bookOpenAtomic.store(false, std::memory_order_release);
                    wasRareBook = g_session.IsRareCanticle();

                    const float gameDays = QueryGameDaysPassed();
                    const auto epCfg     = cfg.SnapshotExpandedPlay();
                    auto state           = CoSaveSerializer::Get().SnapshotState();
                    const auto prevRank  = ClassifyScholarshipRank(state.sign.booksFullyStudiedCount);

                    auto& bookEntry =
                        FindOrInsertBookEntry(state.books, g_session.BookFormId(), gameDays);
                    summary = g_session.Finalize(bookEntry, state.sign.booksFullyStudiedCount,
                                                 gameDays);

                    // Studying The First Canticle of the Eye or a canon Elder Scroll / Moth book
                    // lights Tumbler A (The Word) in Ritual mode (or tracks progress in Sandbox).
                    if (summary.fullStudyJustCompleted && g_session.IsRareCanticle())
                    {
                        wordEvent = MarkTumbler(
                            state.awakening, TumblerBits::kWord, epCfg.unlockMode,
                            "The Canticle's cadence settles behind your eyes");
                    }

                    const auto caps = ResolveStageCapabilities(state.awakening, epCfg.unlockMode);
                    PresenceSignature prog;
                    prog.Load(state.sign);
                    const float grantedPresence =
                        prog.AwardMaxPresence(summary.presenceAwarded, gameDays, false);
                    prog.ApplyBookStudyHabit(summary.newlyCreditedSpreads + summary.refreshedSpreads,
                                             summary.fullStudyJustCompleted);
                    prog.SaveInto(state.sign);
                    if (epCfg.unlockMode == UnlockMode::Ritual &&
                        state.sign.presence.maximum > caps.maxPresenceCap)
                    {
                        state.sign.presence.maximum = caps.maxPresenceCap;
                        state.sign.presence.current =
                            std::min(state.sign.presence.current, state.sign.presence.maximum);
                    }

                    if (summary.skillBookDeepCraftGranted)
                    {
                        state.sign.skillBoostActorValue      = summary.skillBoostActorValue;
                        state.sign.skillBoostExpiresGameDays = gameDays + (2.0f / 24.0f);
                    }

                    summary.presenceAwarded = grantedPresence;
                    rankUp = (summary.newScholarshipRank > prevRank);
                    g_lastSummary = summary;

                    CoSaveSerializer::Get().SetState(std::move(state));
                }

                ExpandedPlayModifiers::Get().SyncFromPersistentState(cfg.expandedPlayEnabled);

                if (wordEvent.progressed)
                {
                    const std::string detail = std::format(
                        "tumbler=Word stage={}({})",
                        static_cast<unsigned>(wordEvent.newStage),
                        AwakeningStageName(wordEvent.newStage));
                    Engine::TraceLogger::Get().LogExpandedPlayEvent(
                        Engine::TraceLogger::Get().DurationSeconds(), "AWAKENING_TUMBLER",
                        summary.bookFormId, "", 0u, 0.0f, detail.c_str());
                    logger::info("[TrueGaze::ExpandedPlay] Awakening tumbler lit: {}", detail);
                }

                if (wordEvent.progressed && !wordEvent.whisper.empty())
                {
                    // ONLY finding and reading The First Canticle / Elder Scroll tome teaches
                    // the Sight of the First Canticle spell!
                    const bool taughtSpell = AwakeningRitual::Get().EnsureCanticleSpellLearned();
                    const std::string msg =
                        taughtSpell
                            ? std::format("{} -- Spell learned: Sight of the First Canticle!",
                                          wordEvent.whisper)
                            : wordEvent.whisper;
                    GazeReticle::Get().NotifyMilestone(msg, wordEvent.whisperColorRgb,
                                                       "UISkillIncreaseSD", true);
                }
                else if (rankUp)
                {
                    GazeReticle::Get().NotifyMilestone(
                        std::format("Moth's Stillness deepened: {}",
                                    ScholarshipRankName(summary.newScholarshipRank)),
                        0xFFD966u, "UIQuestUpdate", true);
                }
                else if (summary.fullStudyJustCompleted)
                {
                    GazeReticle::Get().NotifyMilestone(
                        std::format("Book fully studied ({}/{} spreads, +{:.2f} Presence)",
                                    summary.totalStudiedSpreadsInBook, summary.totalSpreads,
                                    summary.presenceAwarded),
                        0xFFD966u, "UIQuestUpdate", true);
                }
                else if (summary.presenceAwarded > 0.01f)
                {
                    GazeReticle::Get().NotifyMilestone(
                        std::format("Ocular Contemplation ({}/{} spreads, +{:.2f} Presence)",
                                    summary.totalStudiedSpreadsInBook, summary.totalSpreads,
                                    summary.presenceAwarded),
                        0xA6D8FFu, "UISkillsGlowSD", true);
                }
                else if (wasRareBook && summary.totalStudiedSpreadsInBook < summary.totalSpreads)
                {
                    GazeReticle::Get().NotifyMilestone(
                        std::format(
                            "TrueGaze: read slowly to absorb each spread ({}/{} studied).",
                            summary.totalStudiedSpreadsInBook, summary.totalSpreads),
                        0xA6D8FFu, "UIMenuFocus", false);
                }

                {
                    const std::string closeDetail = std::format(
                        "spreads={}/{} new={} refresh={} flips={} +presence={:.2f} full={} rank={}",
                        summary.totalStudiedSpreadsInBook, summary.totalSpreads,
                        summary.newlyCreditedSpreads, summary.refreshedSpreads,
                        summary.speedFlippedSpreads, summary.presenceAwarded,
                        summary.fullStudyJustCompleted ? "yes" : "no",
                        ScholarshipRankName(summary.newScholarshipRank));
                    Engine::TraceLogger::Get().LogExpandedPlayEvent(
                        Engine::TraceLogger::Get().DurationSeconds(), "BOOK_CLOSE",
                        summary.bookFormId, "", 0u, 0.0f, closeDetail.c_str());
                    logger::info("[TrueGaze::ExpandedPlay] Book {:08X} closed: {} totalBooks={}",
                                 summary.bookFormId, closeDetail,
                                 CoSaveSerializer::Get().BooksFullyStudiedCount());
                }

                return RE::BSEventNotifyControl::kContinue;
            }
        };

        class BookInputObserver final : public RE::BSTEventSink<RE::InputEvent*>
        {
        public:
            RE::BSEventNotifyControl ProcessEvent(
                RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
            {
                if (!a_event || !*a_event)
                {
                    return RE::BSEventNotifyControl::kContinue;
                }

                LiveStudyNotice liveNotice{};
                {
                    std::lock_guard lock(g_studyMutex);
                    if (!g_menuOpen || !g_session.IsActive())
                    {
                        return RE::BSEventNotifyControl::kContinue;
                    }

                    const auto now = SteadyClock::now();
                    g_lastActivityAt = now;
                    const float nowSec = ElapsedSinceOpen(now);

                    int pageTurnDir = 0;
                    for (auto* ev = *a_event; ev != nullptr; ev = ev->next)
                    {
                        if (ev->GetEventType() == RE::INPUT_EVENT_TYPE::kMouseMove)
                        {
                            if (auto* mm = ev->AsMouseMoveEvent())
                            {
                                g_cursorAccumX += mm->mouseInputX;
                                if (g_cursorAccumX < -40)
                                {
                                    g_sawLeftScan = true;
                                }
                                if (g_cursorAccumX > 40)
                                {
                                    g_sawRightScan = true;
                                }
                            }
                        }
                        else if (ev->GetEventType() == RE::INPUT_EVENT_TYPE::kButton)
                        {
                            if (const int dir = ClassifyPageTurnButton(ev->AsButtonEvent());
                                dir != 0)
                            {
                                pageTurnDir = dir;
                            }
                        }
                    }

                    const bool hadScan = g_sawLeftScan && g_sawRightScan;
                    const uint32_t prevSpread = g_clock.EffectiveSpread();
                    uint32_t observed = 0u;
                    if (TryProbeChangedScaleformSpread(g_session.SpreadCount(), observed))
                    {
                        g_clock.ObserveScaleformSpread(observed, nowSec, hadScan, g_session);
                    }
                    if (g_clock.EffectiveSpread() != prevSpread)
                    {
                        ResetScanTracker();
                        liveNotice = ComputeLiveStudyNoticeLocked(now);
                    }
                    else if (pageTurnDir != 0)
                    {
                        g_clock.NotifyPageTurnInput(pageTurnDir, nowSec, hadScan, g_session);
                        ResetScanTracker();
                        liveNotice = ComputeLiveStudyNoticeLocked(now);
                    }
                }
                DispatchLiveStudyNotice(liveNotice);

                return RE::BSEventNotifyControl::kContinue;
            }
        };
#endif
    } // namespace

    BookStudyTracker& BookStudyTracker::Get() noexcept
    {
        static BookStudyTracker instance;
        return instance;
    }

    void BookStudyTracker::Install() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (g_installed)
        {
            return;
        }
        static BookMenuObserver menuObserver;
        static BookInputObserver inputObserver;

        if (auto* ui = RE::UI::GetSingleton())
        {
            ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuObserver);
        }
        if (auto* input = RE::BSInputDeviceManager::GetSingleton())
        {
            input->AddEventSink(&inputObserver);
        }
        g_installed = true;
        logger::info("[TrueGaze::ExpandedPlay] Read-only BookStudyTracker installed.");
#endif
    }

    void BookStudyTracker::Reset() noexcept
    {
        std::lock_guard lock(g_studyMutex);
        g_lastSummary = {};
#if __has_include(<RE/Skyrim.h>)
        g_menuOpen = false;
        g_pendingNoteInject = false;
        g_pendingNoteText.clear();
        g_bookOpenAtomic.store(false, std::memory_order_release);
        g_session  = {};
        g_clock.Reset(0u, 0.0f);
        ResetScanTracker();
#endif
    }

    bool BookStudyTracker::IsBookOpen() const noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        return g_bookOpenAtomic.load(std::memory_order_acquire);
#else
        return false;
#endif
    }

    void BookStudyTracker::TickLiveStudy() noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (!g_bookOpenAtomic.load(std::memory_order_acquire))
        {
            return;
        }

        // Handle deferred Scaleform SetBookText injection for the Living Awakening Note once
        // BookMenu finishes initializing book.swf (runs on game thread via AdvanceMovie).
        std::string noteToInject;
        {
            std::lock_guard lock(g_studyMutex);
            if (g_pendingNoteInject && !g_pendingNoteText.empty())
            {
                noteToInject = g_pendingNoteText;
            }
        }
        if (!noteToInject.empty())
        {
            if (TryInjectLivingNoteIntoBookMenu(noteToInject))
            {
                std::lock_guard lock(g_studyMutex);
                g_pendingNoteInject = false;
                g_pendingNoteText.clear();
                if (!g_menuOpen)
                {
                    g_bookOpenAtomic.store(false, std::memory_order_release);
                }
            }
            return;
        }

        LiveStudyNotice notice{};
        {
            std::lock_guard lock(g_studyMutex);
            if (!g_menuOpen || !g_session.IsActive())
            {
                g_bookOpenAtomic.store(false, std::memory_order_release);
                return;
            }
            const auto now = SteadyClock::now();
            const float sinceBeat = std::chrono::duration<float>(now - g_lastHeartbeatAt).count();
            if (sinceBeat < 0.25f)
            {
                return;
            }
            g_lastHeartbeatAt = now;
            notice = ComputeLiveStudyNoticeLocked(now);
        }
        DispatchLiveStudyNotice(notice);
#endif
    }

    BookStudySummary BookStudyTracker::LastSummary() const noexcept
    {
        std::lock_guard lock(g_studyMutex);
        return g_lastSummary;
    }
} // namespace TrueGaze::ExpandedPlay
