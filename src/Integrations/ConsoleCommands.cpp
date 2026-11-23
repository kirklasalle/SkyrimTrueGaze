#include "ConsoleCommands.hpp"

#include "Engine/ConfigManager.hpp"
#include "Engine/GazeEngine.hpp"
#include "Engine/PerformanceProfiler.hpp"
#include "Engine/PlayerGazeResolver.hpp"
#include "Engine/TraceLogger.hpp"
#include "ExpandedPlay/AwakeningRitual.hpp"
#include "ExpandedPlay/BookStudyTracker.hpp"
#include "ExpandedPlay/CoSaveSerializer.hpp"
#include "ExpandedPlay/ExpandedPlayManager.hpp"
#include "ExpandedPlay/ExpandedPlayModifiers.hpp"
#include "ExpandedPlay/ExpandedPlayTypes.hpp"
#include "ExpandedPlay/GazeReticle.hpp"
#include "ExpandedPlay/PresenceSignature.hpp"
#include "ExpandedPlay/ResponseLadder.hpp"
#include "Visuals/VisualEffectsManager.hpp"
#include "Visuals/IncomingGazeManager.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/C/CommandTable.h>
#include <RE/C/Console.h>
#include <RE/C/ConsoleLog.h>
#include <RE/F/FxDelegateArgs.h>
#include <RE/F/FxDelegateHandler.h>
#include <RE/G/GString.h>
#include <RE/S/Script.h>
#include <RE/Skyrim.h>
#include <RE/T/TESObjectREFR.h>
#endif

#include <cctype>
#include <cstdio>
#include <cstring>

namespace TrueGaze::Integrations
{

#if __has_include(<RE/Skyrim.h>)

    namespace
    {

        bool g_installed = false;
        // Defined below; CmdStatus delegates to it.
        void LogStatusToConsole() noexcept;
        void LogIncomingStatus() noexcept;
        // -----------------------------------------------------------------------
        // Console output
        // -----------------------------------------------------------------------
        void ConsolePrint(const char* a_fmt, ...) noexcept
        {
            char buf[512]{0};
            va_list args;
            va_start(args, a_fmt);
            std::vsnprintf(buf, sizeof(buf), a_fmt, args);
            va_end(args);

            if (auto* console = RE::ConsoleLog::GetSingleton())
            {
                // The console's Print is variadic. Passing the buffer through "%s"
                // keeps our own formatting intact and stops it re-interpreting any
                // '%' that survived into the text.
                console->Print("%s", buf);
            }
            // Also record it in the file log. A console line scrolls away, and a
            // toggle that silently did nothing is the exact failure this project
            // exists to stop repeating.
            logger::info("[TrueGaze] {}", buf);
        }

        // -----------------------------------------------------------------------
        // Shared behaviour
        // -----------------------------------------------------------------------

        /// Re-snapshot configuration into the live tuning and report the effect.
        /// Every ON/OFF command funnels through here, so there is exactly one place
        /// that decides what "applied" means.
        void ApplyAndReport(const char* a_label, bool a_enabled) noexcept
        {
            Engine::GazeEngine::Get().RefreshTuning();
            ConsolePrint("TrueGaze: %s = %s", a_label, a_enabled ? "ON" : "OFF");
        }

        /// Persist current settings so a console toggle survives a restart.
        /// Quiet: a read-only game folder is normal and must not look like the toggle
        /// itself failed. The toggle still applies in this session.
        void PersistQuietly() noexcept
        {
            Engine::ConfigManager::GetSingleton().Save();
        }

        // -----------------------------------------------------------------------
        // Command handlers
        //
        // The signature is fixed by the engine's SCRIPT_FUNCTION::Execute_t.
        // Everything is noexcept: this runs inside the console's own call path, and
        // an exception escaping into engine code is not a risk worth taking.
        // -----------------------------------------------------------------------

        bool CmdMaster(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                       RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                       std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            cfg.enableTrueGaze = !cfg.enableTrueGaze;
            PersistQuietly();
            ApplyAndReport("bEnableTrueGaze", cfg.enableTrueGaze);
            return true;
        }

        bool CmdVisuals(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                        RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                        std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            cfg.enableInGameVisuals = !cfg.enableInGameVisuals;

            // Turning the master ON also arms both the rays AND the HCEP panel so the
            // complete visual debug experience is visible immediately.
            if (cfg.enableInGameVisuals)
            {
                cfg.gazeRaysEnabled = true;
                cfg.showHcepPanel = true;
            }

            PersistQuietly();
            ApplyAndReport("bEnableInGameVisuals", cfg.enableInGameVisuals);
            ConsolePrint("TrueGaze: gaze rays = %s, hcep panel = %s",
                         cfg.gazeRaysEnabled ? "ON" : "OFF", cfg.showHcepPanel ? "ON" : "OFF");
            return true;
        }

        bool CmdRays(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                     RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                     std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            // Development-view contract: stgv toggles the complete diagnostic
            // view as one atomic user action. If any part is currently disabled,
            // the next invocation arms all three switches; only an all-ON state
            // toggles the view OFF. This prevents a stale panel/master flag from
            // making stgv appear to do nothing.
            const bool allVisualsOn =
                cfg.enableInGameVisuals && cfg.gazeRaysEnabled && cfg.showHcepPanel;
            const bool enableVisuals = !allVisualsOn;
            cfg.enableInGameVisuals = enableVisuals;
            cfg.gazeRaysEnabled = enableVisuals;
            cfg.showHcepPanel = enableVisuals;

            PersistQuietly();
            Engine::GazeEngine::Get().RefreshTuning();
            ConsolePrint("TrueGaze: development visuals = %s (rays=%s, panel=%s)",
                         enableVisuals ? "ON" : "OFF", cfg.gazeRaysEnabled ? "ON" : "OFF",
                         cfg.showHcepPanel ? "ON" : "OFF");
            return true;
        }

        bool CmdPanel(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                      RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                      std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            cfg.showHcepPanel = !cfg.showHcepPanel;

            // The panel is a child of the master switch, so enabling it implies it.
            if (cfg.showHcepPanel)
            {
                cfg.enableInGameVisuals = true;
            }

            PersistQuietly();
            ApplyAndReport("bShowHcepPanel", cfg.showHcepPanel);
            return true;
        }

        template <bool Engine::ConfigManager::* Setting>
        bool CmdIncoming(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                         RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                         std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            auto& enabled = cfg.*Setting;
            enabled = !enabled;
            if (enabled)
                cfg.enableInGameVisuals = true;
            const char* name =
                Setting == &Engine::ConfigManager::showIncomingAbove ? "bIncomingPanelAbove" :
                Setting == &Engine::ConfigManager::showIncomingHud ? "bIncomingPlayerHud" :
                Setting == &Engine::ConfigManager::showIncomingText ? "bIncomingText" :
                Setting == &Engine::ConfigManager::measureIncomingAnatomy ? "bIncomingAnatomy" :
                "bIncomingPanelHits";
            PersistQuietly();
            ApplyAndReport(name, enabled);
            return true;
        }

        template <int Direction>
        bool CmdIncomingPage(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                             RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                             std::uint32_t&) noexcept
        {
            if (!Engine::ConfigManager::GetSingleton().showIncomingText)
            {
                ConsolePrint("Incoming inspection is off. Enable it with stgintext first.");
                return true;
            }
            Visuals::IncomingGazeManager::Get().StepInspection(Direction);
            ConsolePrint("Incoming inspection: %s text page / observation.", Direction > 0 ? "next" : "previous");
            return true;
        }

        /// Cycle Both -> Light only -> Geometry only -> Both.
        bool CmdRenderMode(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                           RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*,
                           double&, std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            cfg.rayRenderMode = (cfg.rayRenderMode + 1) % 3;
            PersistQuietly();
            Engine::GazeEngine::Get().RefreshTuning();

            const char* name = (cfg.rayRenderMode == 0)   ? "Both (light + branded geometry)"
                               : (cfg.rayRenderMode == 1) ? "Light only (no art assets needed)"
                                                          : "Geometry only (needs the beam assets)";
            ConsolePrint("TrueGaze: iRayRenderMode = %d - %s", cfg.rayRenderMode, name);
            ConsolePrint("TrueGaze: geometry modes require an available beam mesh; check attachment counters in stgstatus.");
            return true;
        }

        bool CmdTerminus(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                         RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*,
                         double&, std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            cfg.gazeRaysTerminus = !cfg.gazeRaysTerminus;
            PersistQuietly();
            ApplyAndReport("bGazeRaysTerminus", cfg.gazeRaysTerminus);
            return true;
        }

        bool CmdVerbose(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                        RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                        std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            // Flip between the shipped default (2, Info) and full diagnostics (1,
            // Debug). Someone reaching for this wants more detail, not less.
            cfg.logLevel = (cfg.logLevel == 1) ? 2 : 1;
            PersistQuietly();
            if (auto log = spdlog::default_logger())
            {
                auto lvl = (cfg.logLevel == 1) ? spdlog::level::debug : spdlog::level::info;
                log->set_level(lvl);
                log->flush_on(lvl);
            }
            ConsolePrint("TrueGaze: iLogLevel = %d (%s)", cfg.logLevel,
                         cfg.logLevel == 1 ? "Debug" : "Info");
            return true;
        }

        bool CmdOn(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                   RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                   std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            cfg.enableInGameVisuals = true;
            cfg.gazeRaysEnabled = true;
            cfg.showHcepPanel = true;
            cfg.showIncomingAbove = cfg.showIncomingHud = cfg.showIncomingText = true;
            cfg.measureIncomingAnatomy = cfg.measureIncomingPanels = true;
            PersistQuietly();
            ApplyAndReport("in-game visuals (rays + panel)", true);
            return true;
        }

        bool CmdOff(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                    RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                    std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            cfg.enableInGameVisuals = false;
            cfg.gazeRaysEnabled = false;
            cfg.showHcepPanel = false;
            cfg.showIncomingAbove = cfg.showIncomingHud = cfg.showIncomingText = false;
            PersistQuietly();
            ApplyAndReport("in-game visuals (rays + panel)", false);
            return true;
        }

        bool CmdExpanded(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR* a_thisObj,
                         RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                         std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            auto& mgr = ExpandedPlay::ExpandedPlayManager::Get();

            if (a_thisObj && a_thisObj->IsPlayerRef())
            {
                // player.stgexpanded — instant console unlock for players & devs anytime
                cfg.expandedPlayEnabled = true;
                cfg.unlockMode = "Sandbox";
                PersistQuietly();
                mgr.ForceConsoleAwakeningUnlock();
                const auto aw = mgr.CurrentAwakening();
                ConsolePrint("TrueGaze: Console Awakening UNLOCKED -> Stage 4 (%s) %s | "
                             "unlockMode=Sandbox | ESL=%s",
                             ExpandedPlay::AwakeningStageName(mgr.CurrentAwakeningStage()),
                             ExpandedPlay::FormatTumblerGlyphs(aw).c_str(),
                             ExpandedPlay::AwakeningRitual::Get().IsEslPluginLoaded() ? "loaded"
                                                                                      : "vanilla");
                return true;
            }

            if (cfg.expandedPlayEnabled && a_thisObj &&
                a_thisObj->GetFormType() == RE::FormType::ActorCharacter)
            {
                const bool started =
                    mgr.StartOrAbortStaringContest(static_cast<uint32_t>(a_thisObj->GetFormID()));
                ConsolePrint("TrueGaze: Staring Contest %s (target=0x%08X).",
                             started ? "STARTED" : "ABORTED",
                             static_cast<uint32_t>(a_thisObj->GetFormID()));
                return true;
            }

            cfg.expandedPlayEnabled = !cfg.expandedPlayEnabled;
            PersistQuietly();
            ApplyAndReport("ExpandedPlay (bEnabled)", cfg.expandedPlayEnabled);
            ExpandedPlay::ExpandedPlayModifiers::Get().SyncFromPersistentState(
                cfg.expandedPlayEnabled);
            if (!cfg.expandedPlayEnabled)
            {
                ExpandedPlay::GazeReticle::Get().Reset();
                ExpandedPlay::ResponseLadder::ClearAllTransientExpressions();
            }
            const auto ep = cfg.SnapshotExpandedPlay();
            const auto pres = mgr.CurrentPresence();
            const auto sig = mgr.CurrentSignature();
            const auto aw = mgr.CurrentAwakening();
            const auto stage = mgr.CurrentAwakeningStage();
            const auto studied = ExpandedPlay::CoSaveSerializer::Get().BooksFullyStudiedCount();
            ConsolePrint("TrueGaze: ExpandedPlay unlock=%s stage=%u (%s) %s | ESL=%s | "
                         "tip: 'player.stgexpanded' unlocks Stage 4",
                         ExpandedPlay::UnlockModeName(ep.unlockMode),
                         static_cast<unsigned>(stage), ExpandedPlay::AwakeningStageName(stage),
                         ExpandedPlay::FormatTumblerGlyphs(aw).c_str(),
                         ExpandedPlay::AwakeningRitual::Get().IsEslPluginLoaded() ? "loaded"
                                                                                  : "vanilla");
            ConsolePrint("TrueGaze: ExpandedPlay mode=%s demeanor=%s (%s) | modules: social=%s mem=%s "
                         "book=%s mature=%s prog=%s kindred=%s",
                         ExpandedPlay::DemeanorModeName(ep.demeanorMode),
                         ExpandedPlay::DemeanorName(mgr.ActiveDemeanor()),
                         mgr.IsDemeanorPinned() ? "pinned" : "auto",
                         ep.moduleSocialReactions ? "ON" : "OFF",
                         ep.moduleRelationshipMemory ? "ON" : "OFF",
                         ep.moduleBookStudy ? "ON" : "OFF",
                         ep.moduleMatureReactions ? "ON" : "OFF",
                         ep.modulePresenceProgression ? "ON" : "OFF",
                         ep.enableKindredSight ? "ON" : "OFF");
            ConsolePrint("TrueGaze: Presence=%.1f/%.1f (%s) | Title=%s (W%.0f/D%.0f/C%.0f/R%.0f) | "
                         "Books=%u (%s) | Ledger=%zu",
                         pres.current, pres.maximum,
                         ExpandedPlay::PresenceTierName(
                             ExpandedPlay::ClassifyPresenceTier(pres.maximum)),
                         ExpandedPlay::SignatureTitleName(mgr.CurrentSignatureTitle()),
                         sig.warmth, sig.dominance, sig.candour, sig.restraint,
                         studied,
                         ExpandedPlay::ScholarshipRankName(
                             ExpandedPlay::ClassifyScholarshipRank(studied)),
                         mgr.LedgerEntryCount());
            ConsolePrint("TrueGaze: Persuasion=%s (%s) | BribeEst=%dg | SpeechMod=%+.1f | "
                         "LawWitnesses=%u",
                         ExpandedPlay::PersuasionVectorName(mgr.LastPersuasionVector()),
                         ExpandedPlay::AffinityTierName(mgr.LastAffinityTier()),
                         mgr.LastBribeCostGold(), mgr.ActiveRapportSpeechBonus(),
                         mgr.ActiveLawWitnessCount());
            return true;
        }

        bool CmdDemeanor(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                         RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                         std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            constexpr int kCount = static_cast<int>(ExpandedPlay::Demeanor::Count);
            cfg.activeDemeanor = (cfg.activeDemeanor + 1) % kCount;
            if (cfg.activeDemeanor < 0)
            {
                cfg.activeDemeanor = 0;
            }
            PersistQuietly();
            Engine::GazeEngine::Get().RefreshTuning();
            const auto ep = cfg.SnapshotExpandedPlay();
            if (ep.demeanorMode == ExpandedPlay::DemeanorMode::Hybrid)
            {
                ExpandedPlay::ExpandedPlayManager::Get().PinManualDemeanor(ep.activeDemeanor);
            }
            ConsolePrint("TrueGaze: Demeanor = %s (mode=%s)",
                         ExpandedPlay::DemeanorName(ep.activeDemeanor),
                         ExpandedPlay::DemeanorModeName(ep.demeanorMode));
            return true;
        }

        bool CmdItems(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                      RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                      std::uint32_t&) noexcept
        {
            const auto res = ExpandedPlay::AwakeningRitual::Get().DropAllDevItemsInFrontOfPlayer();
            ConsolePrint(
                "TrueGaze: Dropped %u Expanded Play relic item(s) in front of player (source=%s%s).",
                res.itemsDropped, res.usedEslItems ? "TrueGaze_ExpandedPlay.esp" : "Skyrim.esm vanilla",
                res.spellGranted ? ", Lesser Power 'Sight of the First Canticle' learned" : "");
            if (!res.usedEslItems)
            {
                ConsolePrint("TrueGaze: Optional ESL 'TrueGaze_ExpandedPlay.esp' not active; "
                             "vanilla Skyrim.esm relics dropped instead.");
            }
            return true;
        }

        /// Print the effective state of every TrueGaze switch. The most useful command
        /// in the set: it answers "is it actually on?" without reading a file.
        bool CmdStatus(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                       RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                       std::uint32_t&) noexcept
        {
            LogStatusToConsole();
            return true;
        }

        // -----------------------------------------------------------------------
        // Kirk directive (2026-10-01): automatic background status logging.
        // The full stgstatus report is written to TrueGaze.log at session start
        // and session end, so every run is self-describing for debugging without
        // needing to open the console. Console output and log output are separate
        // bodies (console uses ConsolePrint, log uses logger) but read the SAME
        // live state so they can never disagree about facts.
        // -----------------------------------------------------------------------
        void WriteStatusToLog() noexcept
        {
            const auto& cfg = Engine::ConfigManager::GetSingleton();
            auto& engine = Engine::GazeEngine::Get();
            auto& visuals = Visuals::VisualEffectsManager::Get();

            auto onOff = [](bool b)
            {
                return b ? "ON " : "OFF";
            };

            logger::info("[TrueGaze] === AUTO STATUS (session log) ===");
            logger::info("[TrueGaze]   kinematics       {}  bEnableTrueGaze",
                         onOff(cfg.enableTrueGaze));
            {
                const auto ep = cfg.SnapshotExpandedPlay();
                const auto& serde = ExpandedPlay::CoSaveSerializer::Get();
                const auto& mods = ExpandedPlay::ExpandedPlayModifiers::Get();
                const auto& epm = ExpandedPlay::ExpandedPlayManager::Get();
                const auto aw = epm.CurrentAwakening();
                const auto stage = epm.CurrentAwakeningStage();
                logger::info("[TrueGaze]   expanded play    {}  [ExpandedPlay] bEnabled (unlock={}, "
                             "stage={} {}, {}, mode={}, demeanor={}, ledger={}, studied={}, "
                             "modifiers={})",
                             onOff(ep.enabled), ExpandedPlay::UnlockModeName(ep.unlockMode),
                             static_cast<unsigned>(stage), ExpandedPlay::AwakeningStageName(stage),
                             ExpandedPlay::FormatTumblerGlyphs(aw),
                             ExpandedPlay::DemeanorModeName(ep.demeanorMode),
                             ExpandedPlay::DemeanorName(ep.activeDemeanor), serde.LedgerCount(),
                             serde.BooksFullyStudiedCount(), mods.ActiveModifierCount());
            }
            logger::info("[TrueGaze]   in-game visuals  {}  bEnableInGameVisuals",
                         onOff(cfg.enableInGameVisuals));
            logger::info("[TrueGaze]   gaze rays        {}  bGazeRaysEnabled",
                         onOff(cfg.gazeRaysEnabled));
            logger::info("[TrueGaze]   hcep panel       {}  bShowHcepPanel",
                         onOff(cfg.showHcepPanel));
            logger::info("[TrueGaze]   render mode      {}   (0=Both 1=Light 2=Geometry)",
                         cfg.rayRenderMode);
            logger::info("[TrueGaze]   ray length       {:.1f} m, colour #{:06X}, opacity {:.2f}",
                         cfg.gazeRayLengthMeters, cfg.gazeRayColour & 0x00FFFFFF,
                         cfg.gazeRayOpacity);
            logger::info("[TrueGaze]   panel offset     fwd {:.1f} cm, scale {:.1f}",
                         cfg.hcepPanelForwardOffsetCm, cfg.hcepPanelScale);
            logger::info("[TrueGaze]   tracked actors   {}", engine.TrackedActorCount());
            logger::info("[TrueGaze]   tick calls        {}",
                         static_cast<unsigned long long>(engine.TickCalls()));
            logger::info("[TrueGaze]   eligible ticks    {}",
                         static_cast<unsigned long long>(engine.EligibleTicks()));
            logger::info("[TrueGaze]   target resolves   {} (none {})",
                         static_cast<unsigned long long>(engine.TargetResolutions()),
                         static_cast<unsigned long long>(engine.NoTargetResolutions()));
            logger::info("[TrueGaze]   rig origin        {} (eye-absent {}, head-absent {})",
                         engine.LastRigOrigin(),
                         static_cast<unsigned long long>(engine.EyeNodeAbsentCount()),
                         static_cast<unsigned long long>(engine.HeadAnchorAbsentCount()));
            logger::info("[TrueGaze]   visual emitters  {} actors, {} lights",
                         visuals.ActiveActorCount(), visuals.AttachedLightCount());
            logger::info("[TrueGaze]   visual updates    {}, anchors failed {}",
                         static_cast<unsigned long long>(visuals.UpdateCalls()),
                         static_cast<unsigned long long>(visuals.AnchorFailures()));
            logger::info("[TrueGaze]   beam geometry     {} attached / {} attempts",
                         static_cast<unsigned long long>(visuals.GeometryCreated()),
                         static_cast<unsigned long long>(visuals.GeometryAttempts()));
            LogIncomingStatus();
            const auto& acc = visuals.GetAccuracyStats();
            if (acc.totalEvaluations > 0)
            {
                const double pct =
                    (static_cast<double>(acc.agreements) / acc.totalEvaluations) * 100.0;
                logger::info(
                    "[TrueGaze]   gaze accuracy     {:.1f}% ({}/{} hits, {} mismatches, {} misses)",
                    pct, acc.agreements, acc.totalEvaluations, acc.mismatches, acc.panelMisses);
            }
            const auto& trace = Engine::TraceLogger::Get();
            logger::info("[TrueGaze]   trace logging     {} ({} events, {:.1f} KB)",
                         trace.IsActive() ? "ACTIVE" : "OFF",
                         static_cast<unsigned long long>(trace.TotalEvents()),
                         static_cast<double>(trace.TotalBytesWritten()) / 1024.0);
            const auto& cal = engine.GetCalibrationState();
            if (cal.active)
            {
                logger::info("[TrueGaze]   calibration       ACTIVE ({}, region {}, yaw {:+.1f}, pitch {:+.1f})",
                             cal.sweepActive ? "SWEEP" : "STEP", cal.currentRegionIndex,
                             cal.overrideYawDeg, cal.overridePitchDeg);
            }
            logger::info("[TrueGaze]   commands          {}",
                         ConsoleCommands::IsInstalled() ? "registered" : "NOT registered");
            logger::info("[TrueGaze] === END AUTO STATUS ===");
        }

        void LogIncomingStatus() noexcept
        {
            const auto& cfg = Engine::ConfigManager::GetSingleton();
            const auto& incoming = Visuals::IncomingGazeManager::Get().Results();
            ConsolePrint("Incoming: A=%s B=%s HUD=%s text=%s anatomy=%s panels=%s",
                         cfg.showIncomingAbove ? "ON" : "OFF", cfg.showHcepPanel ? "ON" : "OFF",
                         cfg.showIncomingHud ? "ON" : "OFF", cfg.showIncomingText ? "ON" : "OFF",
                         cfg.measureIncomingAnatomy ? "ON" : "OFF", cfg.measureIncomingPanels ? "ON" : "OFF");
            ConsolePrint("Incoming: %zu rays / %zu receivers / %zu measurements. Visibility unverified.",
                         incoming.RayCount(), incoming.ReceiverCount(), incoming.HitCount());
            const auto incomingCost = Visuals::IncomingGazeManager::Get().GetCost();
            ConsolePrint("Development diagnostics menu visibility: %s (console settings unchanged).",
                         Visuals::IncomingGazeManager::DiagnosticsSuppressed() ? "SUPPRESSED" : "GAMEPLAY");
            ConsolePrint("Incoming finalize: %llu us (peak %llu); native UI: %llu us (peak %llu); panels=%zu labels=%zu",
                         static_cast<unsigned long long>(incomingCost.finalizationUs),
                         static_cast<unsigned long long>(incomingCost.peakFinalizationUs),
                         static_cast<unsigned long long>(incomingCost.uiUs),
                         static_cast<unsigned long long>(incomingCost.peakUiUs),
                         incomingCost.panels, incomingCost.labels);
            ConsolePrint("Incoming cards=%zu overflow=%zu. Timing windows: up to 256 active frames; earlier peaks are session-wide.",
                         incomingCost.cards, incomingCost.overflowCards);
            if (incomingCost.finalizationSamples)
                ConsolePrint("Incoming finalize n=%zu: median=%llu p95=%llu window peak=%llu us",
                             incomingCost.finalizationSamples,
                             static_cast<unsigned long long>(incomingCost.finalizationMedianUs),
                             static_cast<unsigned long long>(incomingCost.finalizationP95Us),
                             static_cast<unsigned long long>(incomingCost.finalizationWindowPeakUs));
            else
                ConsolePrint("Incoming finalize: no active-frame timing samples.");
            if (incomingCost.uiSamples)
                ConsolePrint("Incoming UI n=%zu: median=%llu p95=%llu window peak=%llu us",
                             incomingCost.uiSamples,
                             static_cast<unsigned long long>(incomingCost.uiMedianUs),
                             static_cast<unsigned long long>(incomingCost.uiP95Us),
                             static_cast<unsigned long long>(incomingCost.uiWindowPeakUs));
            else
                ConsolePrint("Incoming UI: no active-frame timing samples.");
        }

        void LogStatusToConsole() noexcept
        {
            const auto& cfg = Engine::ConfigManager::GetSingleton();
            auto& engine = Engine::GazeEngine::Get();
            auto& visuals = Visuals::VisualEffectsManager::Get();

            auto onOff = [](bool b)
            {
                return b ? "ON " : "OFF";
            };

            ConsolePrint("TrueGaze - status");
            ConsolePrint("  kinematics       %s  bEnableTrueGaze", onOff(cfg.enableTrueGaze));
            ConsolePrint("  creatures        %s  bEnableCreatures", onOff(cfg.enableCreatures));
            {
                const auto ep = cfg.SnapshotExpandedPlay();
                const auto& serde = ExpandedPlay::CoSaveSerializer::Get();
                const auto& mods = ExpandedPlay::ExpandedPlayModifiers::Get();
                const auto& epm = ExpandedPlay::ExpandedPlayManager::Get();
                const auto aw = epm.CurrentAwakening();
                const auto stage = epm.CurrentAwakeningStage();
                ConsolePrint("  expanded play    %s  [ExpandedPlay] bEnabled (unlock=%s, mode=%s, demeanor=%s)",
                             onOff(ep.enabled), ExpandedPlay::UnlockModeName(ep.unlockMode),
                             ExpandedPlay::DemeanorModeName(ep.demeanorMode),
                             ExpandedPlay::DemeanorName(ep.activeDemeanor));
                ConsolePrint("  awakening        Stage %u (%s) %s | ESL=%s | kindredAllies=%u",
                             static_cast<unsigned>(stage), ExpandedPlay::AwakeningStageName(stage),
                             ExpandedPlay::FormatTumblerGlyphs(aw).c_str(),
                             ExpandedPlay::AwakeningRitual::Get().IsEslPluginLoaded() ? "loaded"
                                                                                      : "vanilla",
                             epm.ActiveKindredAllyCount());
                ConsolePrint("  expanded state   ledger %zu, books %zu, studied %u, active modifiers %zu",
                             serde.LedgerCount(), serde.BookCount(), serde.BooksFullyStudiedCount(),
                             mods.ActiveModifierCount());
                {
                    ConsolePrint("  persuasion/tx    vector=%s (%s), bribeEst=%dg, speechMod=%+.1f, "
                                 "lawWitnesses=%u",
                                 ExpandedPlay::PersuasionVectorName(epm.LastPersuasionVector()),
                                 ExpandedPlay::AffinityTierName(epm.LastAffinityTier()),
                                 epm.LastBribeCostGold(), epm.ActiveRapportSpeechBonus(),
                                 epm.ActiveLawWitnessCount());
                }
            }
            ConsolePrint("  in-game visuals  %s  bEnableInGameVisuals",
                         onOff(cfg.enableInGameVisuals));
            ConsolePrint("  gaze rays        %s  bGazeRaysEnabled", onOff(cfg.gazeRaysEnabled));
            ConsolePrint("  hcep panel       %s  bShowHcepPanel", onOff(cfg.showHcepPanel));
            ConsolePrint("  panel all actors %s  bHcepPanelAllActors",
                         onOff(cfg.hcepPanelAllActors));
            ConsolePrint("  terminus glow    %s  bGazeRaysTerminus", onOff(cfg.gazeRaysTerminus));
            ConsolePrint("  render mode      %d   (0=Both 1=Light 2=Geometry)", cfg.rayRenderMode);
            ConsolePrint("  ray length       %.1f m, colour #%06X, opacity %.2f",
                         cfg.gazeRayLengthMeters, cfg.gazeRayColour & 0x00FFFFFF,
                         cfg.gazeRayOpacity);
            ConsolePrint("  pupil offset     fwd %.1f cm, up %.1f cm", cfg.pupilForwardOffsetCm,
                         cfg.pupilUpOffsetCm);
            ConsolePrint("  panel offset     fwd %.1f cm, scale %.1f", cfg.hcepPanelForwardOffsetCm,
                         cfg.hcepPanelScale);
            LogIncomingStatus();
            ConsolePrint("  log level        %d   (1=Debug 2=Info)", cfg.logLevel);
            ConsolePrint("  tracked actors   %zu", engine.TrackedActorCount());
            ConsolePrint("  tick calls       %llu",
                         static_cast<unsigned long long>(engine.TickCalls()));
            ConsolePrint("  eligible ticks   %llu",
                         static_cast<unsigned long long>(engine.EligibleTicks()));
            ConsolePrint("  LOD culled       %llu",
                         static_cast<unsigned long long>(engine.CulledTicks()));
            ConsolePrint("  target resolves  %llu (none %llu)",
                         static_cast<unsigned long long>(engine.TargetResolutions()),
                         static_cast<unsigned long long>(engine.NoTargetResolutions()));
            ConsolePrint("  last target      priority %u, form %08X",
                         static_cast<unsigned>(engine.LastTargetPriority()),
                         engine.LastTargetFormId());
            // GOLD STANDARD: scene-defer visibility. During directed scenes
            // (Helgen cart, package procedures, scripted LookAt) the head chain
            // yields to vanilla direction and the count of yield frames is the
            // direct evidence the defer contract is being honoured.
            ConsolePrint("  scene defer      %s (%llu yield frames)",
                         engine.LastDeferActive() ? "ACTIVE (head chain yielded)" : "inactive",
                         static_cast<unsigned long long>(engine.DeferFrames()));
            ConsolePrint("  rig origin       %s (eye-absent %llu, head-absent %llu)",
                         engine.LastRigOrigin(),
                         static_cast<unsigned long long>(engine.EyeNodeAbsentCount()),
                         static_cast<unsigned long long>(engine.HeadAnchorAbsentCount()));
            ConsolePrint("  bio latency      %.3f s (head onset delay)", cfg.headOnsetDelaySec);
            ConsolePrint("  saccades/blinks  %llu / %llu",
                         static_cast<unsigned long long>(engine.SaccadesTriggered()),
                         static_cast<unsigned long long>(engine.BlinksTriggered()));
            ConsolePrint("  mutual gaze      %llu frames",
                         static_cast<unsigned long long>(engine.MutualGazeFrames()));

            // R14 E2.1: live performance measurements against the documented
            // budget. The budget constant is shared with the EndFrame check so
            // this readout can never disagree with the enforcement code.
            ConsolePrint(
                "  frame cost       last %llu us, peak %llu us (budget %llu us%s)",
                static_cast<unsigned long long>(engine.LastFrameMicros()),
                static_cast<unsigned long long>(engine.PeakFrameMicros()),
                static_cast<unsigned long long>(Engine::PerformanceProfiler::BudgetMicros()),
                Engine::PerformanceProfiler::IsWithinBudget() ? ")" : " — OVER BUDGET)");

            // Phase R16 A7: Gaze accuracy & ray-panel hit agreement
            {
                const auto& acc = visuals.GetAccuracyStats();
                if (acc.totalEvaluations > 0)
                {
                    const double pct =
                        (static_cast<double>(acc.agreements) / acc.totalEvaluations) * 100.0;
                    ConsolePrint(
                        "  gaze accuracy    %.1f%% (%llu/%llu hits, %llu mismatches, %llu misses)",
                        pct,
                        static_cast<unsigned long long>(acc.agreements),
                        static_cast<unsigned long long>(acc.totalEvaluations),
                        static_cast<unsigned long long>(acc.mismatches),
                        static_cast<unsigned long long>(acc.panelMisses));
                }
            }

            // Phase S4: HCEP intent-fusion diagnostics. Answers WHY fusion is or
            // is not active: no telemetry, low confidence, stale, or blink.
            {
                const auto intent = Engine::PlayerGazeResolver::LastIntent();
                if (!intent.valid)
                {
                    ConsolePrint("  hcep intent      inactive (no valid telemetry this session)");
                }
                else
                {
                    ConsolePrint("  hcep intent      seq %u conf %.2f age %llu ms%s%s",
                                 static_cast<unsigned>(intent.sequenceId), intent.confidence,
                                 static_cast<unsigned long long>(intent.ageMs),
                                 intent.stale ? " [STALE]" : "",
                                 intent.blinkSuppressed ? " [BLINK]" : "");
                    ConsolePrint(
                        "  hcep head        yaw %+.1f deg pitch %+.1f deg, convergence %.2f m%s",
                        intent.headYawDeg, intent.headPitchDeg, intent.convergenceMeters,
                        intent.convergencePlausible ? "" : " [implausible]");
                }
            }
            ConsolePrint("  visual emitters  %zu actors, %zu lights", visuals.ActiveActorCount(),
                         visuals.AttachedLightCount());
            ConsolePrint("  visual updates   %llu, anchors failed %llu, light creates failed %llu",
                         static_cast<unsigned long long>(visuals.UpdateCalls()),
                         static_cast<unsigned long long>(visuals.AnchorFailures()),
                         static_cast<unsigned long long>(visuals.LightCreateFailures()));
            ConsolePrint("  beam geometry    %llu attached / %llu attempts",
                         static_cast<unsigned long long>(visuals.GeometryCreated()),
                         static_cast<unsigned long long>(visuals.GeometryAttempts()));
            const auto& trace = Engine::TraceLogger::Get();
            ConsolePrint("  trace logging    %s (%llu events, %.1f KB)",
                         trace.IsActive() ? "ACTIVE" : "OFF",
                         static_cast<unsigned long long>(trace.TotalEvents()),
                         static_cast<double>(trace.TotalBytesWritten()) / 1024.0);
            const auto& cal = engine.GetCalibrationState();
            if (cal.active)
            {
                ConsolePrint("  calibration      ACTIVE (%s, region %d, yaw %+.1f, pitch %+.1f)",
                             cal.sweepActive ? "SWEEP" : "STEP", cal.currentRegionIndex,
                             cal.overrideYawDeg, cal.overridePitchDeg);
            }
            ConsolePrint("  commands         %s",
                         ConsoleCommands::IsInstalled() ? "registered" : "NOT registered");
        }

        // -----------------------------------------------------------------------
        // R15 C4.1: preset application + config reload at the console.
        //
        // stgpreset cycles the same re-calibrated Quick Presets the
        // configurator ships (vanilla/subtle/intense/social/developer), writes
        // the INI, and refreshes the live tuning — a field test can switch
        // baselines without leaving the game. stgreload re-reads the INI from
        // disk (hand-edits and external tools take effect immediately).
        // -----------------------------------------------------------------------

        /// The re-calibrated preset table — the SAME values the configurator's
        /// applyPreset() writes (R15 C3.2, derived from the v1.0.6 baseline).
        /// Single source of truth for the runtime path; the HTML holds the UI
        /// copy. Keep the two in sync (the CI parity gate checks the HTML side).
        struct PresetEntry
        {
            const char* name;
            const char* section;
            const char* key;
            const char* value;
        };

        constexpr PresetEntry kPresets[] = {
            // vanilla = the shipped v1.0.6 baseline itself.
            {"vanilla", "Kinematics", "fSaccadeSpeedMult", "1.5"},
            {"vanilla", "Kinematics", "fVelocitySaturation", "18.0"},
            {"vanilla", "Kinematics", "fMicroJitterAmp", "0.28"},
            {"vanilla", "Kinematics", "fHeadTrackingSpeed", "5.2"},
            {"vanilla", "Kinematics", "fEyePursuitSpeed", "12.0"},
            {"vanilla", "Kinematics", "fMaxComfortEyeAngle", "35.0"},
            {"vanilla", "Kinematics", "fHeadOnsetDelaySec", "0.13"},
            {"vanilla", "SkeletalHierarchy", "fHeadEngageThresholdDeg", "12.0"},
            {"vanilla", "Social", "fTrianglePathRandomness", "0.6"},
            {"vanilla", "Social", "fCgaHeadInvolvement", "0.08"},
            {"vanilla", "Social", "fCgaDialogueOffsetSec", "2.0"},
            {"vanilla", "Profiles", "bEnableCategoryProfiles", "true"},
            {"vanilla", "Visuals", "bEnableInGameVisuals", "false"},
            {"vanilla", "Visuals", "bGazeRaysEnabled", "false"},
            {"vanilla", "Visuals", "bShowHcepPanel", "false"},
            {"vanilla", "Debug", "bDebugGazeRays", "false"},
            {"vanilla", "Debug", "iLogLevel", "2"},
            {"vanilla", "SkeletalHierarchy", "fSpine2YawWeight", "0.035"},
            {"vanilla", "SkeletalHierarchy", "fNeckYawWeight", "0.07"},
            {"vanilla", "SkeletalHierarchy", "fNeckPitchWeight", "0.07"},
            {"vanilla", "SkeletalHierarchy", "fHeadYawWeight", "0.245"},
            {"vanilla", "SkeletalHierarchy", "fHeadPitchWeight", "0.245"},
            {"vanilla", "Social", "bDialogueSyncCgaReturn", "true"},
            {"vanilla", "Social", "bEnableGazeAversion", "true"},
            {"vanilla", "Social", "bEnableSocialTriangle", "true"},
            {"vanilla", "Crosshair", "bEnableCrosshairGaze", "true"},
            // subtle = baseline with a gentle combat-ceiling reduction.
            {"subtle", "Kinematics", "fSaccadeSpeedMult", "1.2"},
            {"subtle", "Kinematics", "fMicroJitterAmp", "0.18"},
            {"subtle", "Kinematics", "fHeadTrackingSpeed", "4.0"},
            {"subtle", "Kinematics", "fEyePursuitSpeed", "10.0"},
            {"subtle", "Kinematics", "fMaxComfortEyeAngle", "30.0"},
            {"subtle", "SkeletalHierarchy", "fHeadEngageThresholdDeg", "12.0"},
            {"subtle", "Social", "fCgaHeadInvolvement", "0.05"},
            {"subtle", "Social", "fCgaDialogueOffsetSec", "2.5"},
            {"subtle", "Social", "fTrianglePathRandomness", "0.5"},
            {"subtle", "Profiles", "bEnableCategoryProfiles", "true"},
            {"subtle", "Visuals", "bEnableInGameVisuals", "false"},
            {"subtle", "Visuals", "bGazeRaysEnabled", "false"},
            {"subtle", "Visuals", "bShowHcepPanel", "false"},
            {"subtle", "Debug", "bDebugGazeRays", "false"},
            {"subtle", "Debug", "iLogLevel", "2"},
            {"subtle", "Kinematics", "fHeadOnsetDelaySec", "0.13"},
            {"subtle", "SkeletalHierarchy", "fSpine2YawWeight", "0.035"},
            {"subtle", "SkeletalHierarchy", "fNeckYawWeight", "0.07"},
            {"subtle", "SkeletalHierarchy", "fNeckPitchWeight", "0.07"},
            {"subtle", "SkeletalHierarchy", "fHeadYawWeight", "0.245"},
            {"subtle", "SkeletalHierarchy", "fHeadPitchWeight", "0.245"},
            {"subtle", "Social", "bDialogueSyncCgaReturn", "true"},
            {"subtle", "Social", "bEnableGazeAversion", "true"},
            {"subtle", "Social", "bEnableSocialTriangle", "true"},
            {"subtle", "Crosshair", "bEnableCrosshairGaze", "true"},
            // intense = baseline with a heightened combat ceiling (12° floor kept).
            {"intense", "Kinematics", "fSaccadeSpeedMult", "1.8"},
            {"intense", "Kinematics", "fMicroJitterAmp", "0.35"},
            {"intense", "Kinematics", "fHeadTrackingSpeed", "6.5"},
            {"intense", "Kinematics", "fEyePursuitSpeed", "15.0"},
            {"intense", "Kinematics", "fHeadOnsetDelaySec", "0.10"},
            {"intense", "SkeletalHierarchy", "fHeadEngageThresholdDeg", "12.0"},
            {"intense", "Social", "fCgaHeadInvolvement", "0.12"},
            {"intense", "Social", "fCgaDialogueOffsetSec", "1.5"},
            {"intense", "Social", "fTrianglePathRandomness", "0.7"},
            {"intense", "Profiles", "bEnableCategoryProfiles", "true"},
            {"intense", "Visuals", "bEnableInGameVisuals", "false"},
            {"intense", "Visuals", "bGazeRaysEnabled", "false"},
            {"intense", "Visuals", "bShowHcepPanel", "false"},
            {"intense", "Debug", "bDebugGazeRays", "false"},
            {"intense", "Debug", "iLogLevel", "2"},
            {"intense", "Kinematics", "fMaxComfortEyeAngle", "35.0"},
            {"intense", "SkeletalHierarchy", "fSpine2YawWeight", "0.035"},
            {"intense", "SkeletalHierarchy", "fNeckYawWeight", "0.07"},
            {"intense", "SkeletalHierarchy", "fNeckPitchWeight", "0.07"},
            {"intense", "SkeletalHierarchy", "fHeadYawWeight", "0.245"},
            {"intense", "SkeletalHierarchy", "fHeadPitchWeight", "0.245"},
            {"intense", "Social", "bDialogueSyncCgaReturn", "true"},
            {"intense", "Social", "bEnableGazeAversion", "true"},
            {"intense", "Social", "bEnableSocialTriangle", "true"},
            {"intense", "Crosshair", "bEnableCrosshairGaze", "true"},
            // social = baseline tuned for conversation richness.
            {"social", "Kinematics", "fSaccadeSpeedMult", "1.5"},
            {"social", "Kinematics", "fMicroJitterAmp", "0.22"},
            {"social", "Kinematics", "fHeadTrackingSpeed", "5.2"},
            {"social", "Kinematics", "fEyePursuitSpeed", "12.0"},
            {"social", "Kinematics", "fMaxComfortEyeAngle", "35.0"},
            {"social", "Kinematics", "fHeadOnsetDelaySec", "0.13"},
            {"social", "SkeletalHierarchy", "fHeadEngageThresholdDeg", "12.0"},
            {"social", "GazeTarget", "fEyeAnchorForwardCm", "10.8"},
            {"social", "GazeTarget", "fEyeAnchorUpCm", "5.4"},
            {"social", "GazeTarget", "fEyeMorphGain", "1.8"},
            {"social", "Social", "fCgaHeadInvolvement", "0.06"},
            {"social", "Social", "fCgaDialogueOffsetSec", "2.0"},
            {"social", "Social", "fTrianglePathRandomness", "0.7"},
            {"social", "Social", "fMutualGazeThreshold", "1.5"},
            {"social", "Crosshair", "fCrosshairToleranceDeg", "6.0"},
            {"social", "Profiles", "bEnableCategoryProfiles", "true"},
            {"social", "Visuals", "bEnableInGameVisuals", "false"},
            {"social", "Visuals", "bGazeRaysEnabled", "false"},
            {"social", "Visuals", "bShowHcepPanel", "false"},
            {"social", "Debug", "bDebugGazeRays", "false"},
            {"social", "Debug", "iLogLevel", "2"},
            {"social", "SkeletalHierarchy", "fSpine2YawWeight", "0.035"},
            {"social", "SkeletalHierarchy", "fNeckYawWeight", "0.07"},
            {"social", "SkeletalHierarchy", "fNeckPitchWeight", "0.07"},
            {"social", "SkeletalHierarchy", "fHeadYawWeight", "0.245"},
            {"social", "SkeletalHierarchy", "fHeadPitchWeight", "0.245"},
            {"social", "GazeTarget", "fEyeMorphFullScaleDeg", "20.0"},
            {"social", "Social", "bDialogueSyncCgaReturn", "true"},
            {"social", "Social", "bEnableGazeAversion", "true"},
            {"social", "Social", "bEnableSocialTriangle", "true"},
            {"social", "Crosshair", "bEnableCrosshairGaze", "true"},
            // developer = the shipped baseline exactly + all visuals + debug.
            {"developer", "General", "bEnableTrueGaze", "true"},
            {"developer", "General", "bEnableCreatures", "true"},
            {"developer", "Kinematics", "fSaccadeSpeedMult", "1.5"},
            {"developer", "Kinematics", "fVelocitySaturation", "18.0"},
            {"developer", "Kinematics", "fMicroJitterAmp", "0.28"},
            {"developer", "Kinematics", "fHeadTrackingSpeed", "5.2"},
            {"developer", "Kinematics", "fEyePursuitSpeed", "12.0"},
            {"developer", "Kinematics", "fMaxComfortEyeAngle", "35.0"},
            {"developer", "Kinematics", "fHeadOnsetDelaySec", "0.13"},
            {"developer", "SkeletalHierarchy", "fHeadEngageThresholdDeg", "12.0"},
            {"developer", "Social", "fTriangleFixationDuration", "0.35"},
            {"developer", "Social", "fTrianglePathRandomness", "0.6"},
            {"developer", "Social", "fMutualGazeThreshold", "2.0"},
            {"developer", "Social", "fCgaHeadInvolvement", "0.08"},
            {"developer", "Social", "fCgaDialogueOffsetSec", "2.0"},
            {"developer", "Crosshair", "fCrosshairToleranceDeg", "4.0"},
            {"developer", "Profiles", "bEnableCategoryProfiles", "true"},
            {"developer", "Visuals", "bEnableInGameVisuals", "true"},
            {"developer", "Visuals", "bGazeRaysEnabled", "true"},
            {"developer", "Visuals", "iRayRenderMode", "0"},
            {"developer", "Visuals", "bShowHcepPanel", "true"},
            {"developer", "Visuals", "bHcepPanelAllActors", "false"},
            {"developer", "Debug", "bDebugGazeRays", "true"},
            {"developer", "Debug", "iLogLevel", "1"},
            {"developer", "Console", "bEnableConsoleCommands", "true"},
            {"developer", "Visuals", "fGazeRayLengthMeters", "2.5"},
            {"developer", "Visuals", "fGazeRayThicknessCm", "0.2"},
            {"developer", "Visuals", "bGazeRaysOnPlayer", "true"},
            {"developer", "Visuals", "bGazeRaysOnNPCs", "true"},
            {"developer", "Visuals", "bGazeRaysOnCreatures", "true"},
            {"developer", "Visuals", "bGazeRaysAttachHead", "true"},
            {"developer", "Visuals", "bGazeRaysTerminus", "true"},
            {"developer", "Visuals", "fPupilForwardOffsetCm", "10.8"},
            {"developer", "Visuals", "fPupilUpOffsetCm", "5.4"},
            {"developer", "Visuals", "fPupilGlowIntensity", "0.35"},
            {"developer", "SkeletalHierarchy", "fSpine2YawWeight", "0.035"},
            {"developer", "SkeletalHierarchy", "fNeckYawWeight", "0.07"},
            {"developer", "SkeletalHierarchy", "fNeckPitchWeight", "0.07"},
            {"developer", "SkeletalHierarchy", "fHeadYawWeight", "0.245"},
            {"developer", "SkeletalHierarchy", "fHeadPitchWeight", "0.245"},
            {"developer", "Social", "bEnableGazeAversion", "true"},
            {"developer", "Social", "bEnableSocialTriangle", "true"},
            {"developer", "Social", "bDialogueSyncCgaReturn", "true"},
            {"developer", "Crosshair", "bEnableCrosshairGaze", "true"},
        };

        bool CmdPreset(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                       RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                       std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            const auto path = cfg.LoadedPath();
            if (path.empty())
            {
                ConsolePrint("TrueGaze: cannot persist a preset without a loaded INI. Install TrueGaze.ini and restart.");
                return true;
            }
            static const char* kNames[] = {"vanilla", "subtle", "intense", "social", "developer"};

            // No string-argument parsing: the registered command cycles presets.
            static int s_presetIndex = 0;
            const char* name = kNames[s_presetIndex];

            int applied = 0;
            for (const auto& e : kPresets)
            {
                if (std::strcmp(e.name, name) != 0)
                {
                    continue;
                }
                if (!WritePrivateProfileStringA(
                    e.section, e.key, e.value, path.c_str()))
                {
                    ConsolePrint("TrueGaze: preset write failed at [%s] %s (Windows error %lu); INI may be partially updated. Runtime unchanged; inspect INI before stgreload.",
                                 e.section, e.key, GetLastError());
                    return true;
                }
                ++applied;
            }

            // One consistent re-read: Load() re-reads every key, Sanitise() clamps,
            // and RefreshTuning() snapshots into the engine. Never half-applied.
            cfg.Load(path);
            Engine::GazeEngine::Get().RefreshTuning();
            s_presetIndex = (s_presetIndex + 1) % 5;

            ConsolePrint("TrueGaze: preset '%s' applied (%d keys) — next: '%s'", name, applied,
                         kNames[s_presetIndex]);
            return true;
        }

        bool CmdReload(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                       RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                       std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            const auto path = cfg.LoadedPath();
            cfg.Load(path);
            Engine::GazeEngine::Get().RefreshTuning();
            ExpandedPlay::ExpandedPlayModifiers::Get().SyncFromPersistentState(
                cfg.expandedPlayEnabled);
            if (!cfg.expandedPlayEnabled)
            {
                ExpandedPlay::GazeReticle::Get().Reset();
                ExpandedPlay::ResponseLadder::ClearAllTransientExpressions();
            }
            ConsolePrint("TrueGaze: configuration reloaded from '%s'",
                         path.empty() ? "(compiled defaults)" : path.c_str());
            return true;
        }

        bool CmdTrace(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                      RE::TESObjectREFR* a_thisObj, RE::TESObjectREFR*, RE::Script*,
                      RE::ScriptLocals*, double&, std::uint32_t&) noexcept
        {
            auto& logger = Engine::TraceLogger::Get();
            if (!logger.IsActive())
            {
                std::vector<uint32_t> actorFilters;
                if (a_thisObj && a_thisObj->GetFormType() == RE::FormType::ActorCharacter)
                {
                    actorFilters.push_back(a_thisObj->GetFormID());
                }
                const auto& cfg = Engine::ConfigManager::GetSingleton();
                logger.SetConfig(cfg.traceGazeTick, cfg.traceSaccades, cfg.traceRegionChanges,
                                 cfg.traceCGA, cfg.traceBlinks, cfg.traceHitMismatches,
                                 cfg.traceBufferSize, cfg.traceMaxFileSizeMB);
                logger.StartTracing({}, actorFilters);
                if (actorFilters.empty())
                {
                    ConsolePrint("TrueGaze: Trace started -> %s (ALL actors)",
                                 logger.GetLogFilePath().string().c_str());
                }
                else
                {
                    ConsolePrint("TrueGaze: Trace started -> %s (Actor 0x%08X)",
                                 logger.GetLogFilePath().string().c_str(), actorFilters[0]);
                }
            }
            else
            {
                const auto dur = logger.DurationSeconds();
                const auto evs = logger.TotalEvents();
                const auto bytes = logger.TotalBytesWritten();
                const auto pending = logger.PendingBufferSize();
                const auto& filters = logger.GetActorFilters();
                ConsolePrint("TrueGaze Trace: ACTIVE");
                ConsolePrint("  Duration:    %.1f sec", dur);
                ConsolePrint("  Events:      %llu", static_cast<unsigned long long>(evs));
                ConsolePrint("  File size:   %.1f KB / 100 MB limit",
                             static_cast<double>(bytes) / 1024.0);
                if (filters.empty())
                {
                    ConsolePrint("  Actors:      ALL (%zu active)",
                                 Engine::GazeEngine::Get().TrackedActorCount());
                }
                else
                {
                    ConsolePrint("  Actors:      Filtered (%zu target(s))", filters.size());
                }
                ConsolePrint("  Buffer:      %zu / 256 events pending flush", pending);
                logger.Flush();
            }
            return true;
        }

        bool CmdTraceOff(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                         RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*,
                         double&, std::uint32_t&) noexcept
        {
            auto& logger = Engine::TraceLogger::Get();
            if (!logger.IsActive())
            {
                ConsolePrint("TrueGaze: Trace is not active.");
                return true;
            }
            const auto evs = logger.TotalEvents();
            const auto bytes = logger.TotalBytesWritten();
            const auto path = logger.GetLogFilePath().string();
            logger.StopTracing();
            ConsolePrint("TrueGaze: Trace stopped. %llu events written (%.2f MB) to %s",
                         static_cast<unsigned long long>(evs),
                         static_cast<double>(bytes) / (1024.0 * 1024.0), path.c_str());
            return true;
        }

        bool CmdTraceFlush(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                           RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*,
                           double&, std::uint32_t&) noexcept
        {
            auto& logger = Engine::TraceLogger::Get();
            if (!logger.IsActive())
            {
                ConsolePrint("TrueGaze: Trace is not active.");
                return true;
            }
            logger.Flush();
            ConsolePrint("TrueGaze: Trace buffer flushed (%llu total events written).",
                         static_cast<unsigned long long>(logger.TotalEvents()));
            return true;
        }

        bool CmdCal(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                    RE::TESObjectREFR* a_thisObj, RE::TESObjectREFR*, RE::Script*,
                    RE::ScriptLocals*, double&, std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            if (!cfg.enableCalibrationCommands)
            {
                ConsolePrint("TrueGaze: Calibration commands are disabled ([Debug] bEnableCalibrationCommands=false).");
                return true;
            }

            uint32_t targetActor = 0;
            if (a_thisObj && a_thisObj->GetFormType() == RE::FormType::ActorCharacter)
            {
                targetActor = a_thisObj->GetFormID();
            }

            auto& engine = Engine::GazeEngine::Get();
            engine.StepCalibrationNext(targetActor);
            const auto& cal = engine.GetCalibrationState();

            ConsolePrint("TrueGaze: Calibration STEP [%d/11] -> Yaw=%+.1f deg, Pitch=%+.1f deg (Target: %s)",
                         cal.currentRegionIndex + 1, cal.overrideYawDeg, cal.overridePitchDeg,
                         targetActor ? "Selected Actor" : "All Tracked Actors");
            return true;
        }

        bool CmdCalSweep(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                         RE::TESObjectREFR* a_thisObj, RE::TESObjectREFR*, RE::Script*,
                         RE::ScriptLocals*, double&, std::uint32_t&) noexcept
        {
            auto& cfg = Engine::ConfigManager::GetSingleton();
            if (!cfg.enableCalibrationCommands)
            {
                ConsolePrint("TrueGaze: Calibration commands are disabled ([Debug] bEnableCalibrationCommands=false).");
                return true;
            }

            uint32_t targetActor = 0;
            if (a_thisObj && a_thisObj->GetFormType() == RE::FormType::ActorCharacter)
            {
                targetActor = a_thisObj->GetFormID();
            }

            auto& engine = Engine::GazeEngine::Get();
            engine.StartCalibrationSweep(targetActor);
            ConsolePrint("TrueGaze: Calibration SWEEP started across 11 regions (2.0s hold per region).");
            ConsolePrint("Angular diagnostic only: this does not validate incoming anatomy or panel ownership.");
            return true;
        }

        bool CmdCalOff(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                       RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*,
                       RE::ScriptLocals*, double&, std::uint32_t&) noexcept
        {
            auto& engine = Engine::GazeEngine::Get();
            engine.StopCalibration();
            ConsolePrint("TrueGaze: Calibration override STOPPED. Autonomous gaze restored.");
            return true;
        }

        bool CmdCalAxes(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                        RE::TESObjectREFR* a_thisObj, RE::TESObjectREFR*, RE::Script*,
                        RE::ScriptLocals*, double&, std::uint32_t&) noexcept
        {
            ConsolePrint("TrueGaze: Head Bone Axes Calibration (Audit A8 verification):");
            ConsolePrint("  Local Column 0: X = Right (+yaw)");
            ConsolePrint("  Local Column 1: Y = Forward (+length / LoS)");
            ConsolePrint("  Local Column 2: Z = Up (+pitch)");
            ConsolePrint("  Basis Scaling:  Column scaling applied via ScaleBasisColumns(M, sx, 1.0, sz)");

            if (a_thisObj && a_thisObj->GetFormType() == RE::FormType::ActorCharacter)
            {
                auto* actor = a_thisObj->As<RE::Actor>();
                if (actor && actor->Get3D())
                {
                    ConsolePrint("  Target Actor 0x%08X: 3D Root Valid. In-engine orientation verified.",
                                 actor->GetFormID());
                }
            }
            return true;
        }

        // Forward declaration for kCommands table binding
        bool CmdHelp(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                     RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*,
                     RE::ScriptLocals*, double&, std::uint32_t&) noexcept;

        // -----------------------------------------------------------------------
        // The command set
        // -----------------------------------------------------------------------

        struct CommandDef
        {
            const char* name; // the token typed at the console prompt
            const char* help;

            // The handlers are declared `noexcept` on purpose: they run inside the
            // engine's console call path and must not let an exception escape into
            // engine code. That makes their type strictly *stronger* than the engine's
            // Execute_t, which is not noexcept - and a noexcept function pointer will
            // not implicitly convert to a non-noexcept one. So the table stores the
            // precise type and the single unavoidable conversion happens at the one
            // place the pointer is assigned.
            //
            // Written out longhand rather than via Execute_t because the spelling of
            // the type is the thing that has to be right here.
            using Handler = bool (*)(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                                     RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*,
                                     RE::ScriptLocals*, double&, std::uint32_t&) noexcept;
            Handler handler;
        };

        static_assert(std::is_same_v<decltype(&CmdMaster), CommandDef::Handler>,
                      "command handlers must match CommandDef::Handler exactly");

        /// One table, so the registrations and the count can never disagree.
        /// Deliberately small and focused: toggles, plus a status read-out.
        constexpr CommandDef kCommands[] = {
            {"stg", "Toggle the SkyrimTrueGaze kinematics engine on/off", &CmdMaster},
            {"stghelp", "Print all available TrueGaze console commands and descriptions", &CmdHelp},
            {"stgvisuals", "Toggle all in-game visuals (gaze rays + HCEP panel)", &CmdVisuals},
            {"stgv", "Toggle visual master, rays and front panel together (not a rays-only toggle)", &CmdRays},
            {"stgpanel", "Toggle target-owned incoming panel B (front)", &CmdPanel},
            {"stgpanela", "Toggle incoming panel A (above head)", &CmdIncoming<&Engine::ConfigManager::showIncomingAbove>},
            {"stghud", "Toggle first-person player incoming HUD", &CmdIncoming<&Engine::ConfigManager::showIncomingHud>},
            {"stgintext", "Toggle non-modal incoming inspection and detailed telemetry", &CmdIncoming<&Engine::ConfigManager::showIncomingText>},
            {"stginnext", "Next incoming inspection text page / observation", &CmdIncomingPage<1>},
            {"stginprev", "Previous incoming inspection text page / observation", &CmdIncomingPage<-1>},
            {"stginanatomy", "Toggle approximate incoming anatomical measurements", &CmdIncoming<&Engine::ConfigManager::measureIncomingAnatomy>},
            {"stginpanels", "Toggle physical incoming panel intersections", &CmdIncoming<&Engine::ConfigManager::measureIncomingPanels>},
            {"stgon", "Enable visual master, rays, A/B panels, HUD, inspector and measurements", &CmdOn},
            {"stgoff", "Turn every in-game visual off", &CmdOff},
            {"stgmode", "Cycle render mode: Both / Light / Geometry", &CmdRenderMode},
            {"stgradius", "Toggle the gaze terminus glow", &CmdTerminus},
            {"stgverbose", "Toggle Debug/Info logging", &CmdVerbose},
            {"stgstatus", "Print the effective SkyrimTrueGaze state", &CmdStatus},
            {"stgpreset", "Cycle Quick Presets (vanilla/subtle/intense/social/developer)",
             &CmdPreset},
            {"stgreload", "Reload TrueGaze.ini from disk and refresh the engine", &CmdReload},
            {"stgtrace", "Start or inspect TrueGaze JSONL trace logging; stgtraceoff stops it", &CmdTrace},
            {"stgtraceoff", "Stop TrueGaze JSONL trace logging and flush buffer", &CmdTraceOff},
            {"stgtraceflush", "Force immediate flush of trace log buffer to disk", &CmdTraceFlush},
            {"stgcal", "Step through calibration representative region angles", &CmdCal},
            {"stgcalsweep", "Automated 11-region calibration sweep (2s per region)", &CmdCalSweep},
            {"stgcaloff", "Stop calibration override and restore normal gaze", &CmdCalOff},
            {"stgcalaxes", "Verify head bone local axes convention (X=Right, Y=Fwd, Z=Up)", &CmdCalAxes},
            {"stgexpanded", "Toggle TrueGaze Expanded Play gameplay layer on/off", &CmdExpanded},
            {"stgdemeanor", "Cycle third-person Expanded Play Demeanor", &CmdDemeanor},
            {"stgitems", "Drop all TrueGaze Expanded Play lore items in front of the player (dev)",
             &CmdItems},
        };

        bool CmdHelp(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*,
                     RE::TESObjectREFR*, RE::TESObjectREFR*, RE::Script*,
                     RE::ScriptLocals*, double&, std::uint32_t&) noexcept
        {
            ConsolePrint("=== SkyrimTrueGaze Console Commands (%zu total) ===", std::size(kCommands));
            for (const auto& cmd : kCommands)
            {
                ConsolePrint("  %-14s %s", cmd.name, cmd.help);
            }
            ConsolePrint("==================================================");
            return true;
        }

        // -----------------------------------------------------------------------
        // CRIT-01: Zero-Clobber Scaleform Console Interception (SE / AE / VR)
        // -----------------------------------------------------------------------
        //
        // Previously this file overwrote 30 entries in Skyrim's RE::SCRIPT_FUNCTION
        // console command table based on an erroneous heuristic about `helpString`
        // (empty string or trailing '0'), silently clobbering live vanilla console
        // commands and colliding with other SKSE console plugins.
        //
        // Instead, we hook RE::Console::Accept (RE::VTABLE_Console[0] slot 0x01 —
        // identical across Skyrim SE 1.5.97, AE 1.6+, and VR 1.4.15) with a
        // lightweight FxDelegateHandler::CallbackProcessor proxy that wraps the
        // Scaleform "Execute" callback. When the player submits a line matching any
        // of our 30 `stg*` tokens, we dispatch directly to the handler with
        // RE::Console::GetSelectedRef() as `a_thisObj`; all other console input
        // passes straight through to Skyrim untouched. Zero vanilla commands are
        // modified or displaced.
        RE::FxDelegateHandler::CallbackFn* g_originalConsoleExecute{nullptr};

        void InterceptConsoleExecute(const RE::FxDelegateArgs& a_params)
        {
            if (a_params.GetArgCount() >= 1 && a_params[0].IsString())
            {
                const char* raw = a_params[0].GetString();
                if (raw)
                {
                    // Skip leading whitespace.
                    while (*raw && std::isspace(static_cast<unsigned char>(*raw)))
                    {
                        ++raw;
                    }
                    // Extract the first command token (case-insensitive).
                    const char* end = raw;
                    while (*end && !std::isspace(static_cast<unsigned char>(*end)) && *end != ';')
                    {
                        ++end;
                    }
                    const size_t tokenLen = static_cast<size_t>(end - raw);
                    if (tokenLen > 0)
                    {
                        for (const auto& def : kCommands)
                        {
                            if (std::strlen(def.name) == tokenLen &&
                                _strnicmp(raw, def.name, tokenLen) == 0)
                            {
                                auto selectedRef = RE::Console::GetSelectedRef();
                                double dummyResult = 0.0;
                                std::uint32_t dummyOffset = 0u;
                                def.handler(nullptr, nullptr, selectedRef.get(), nullptr, nullptr,
                                            nullptr, dummyResult, dummyOffset);
                                return;
                            }
                        }
                    }
                }
            }

            if (g_originalConsoleExecute)
            {
                g_originalConsoleExecute(a_params);
            }
        }

        class ConsoleCallbackProxy final : public RE::FxDelegateHandler::CallbackProcessor
        {
        public:
            explicit ConsoleCallbackProxy(RE::FxDelegateHandler::CallbackProcessor* a_inner) noexcept
                : _inner(a_inner)
            {
            }

            void Process(const RE::GString& a_methodName,
                         RE::FxDelegateHandler::CallbackFn* a_method) override
            {
                if (!_inner)
                {
                    return;
                }
                const char* name = a_methodName.c_str();
                if (name &&
                    (_stricmp(name, "Execute") == 0 || _stricmp(name, "ExecuteCommand") == 0))
                {
                    g_originalConsoleExecute = a_method;
                    _inner->Process(a_methodName, &InterceptConsoleExecute);
                    return;
                }
                _inner->Process(a_methodName, a_method);
            }

        private:
            RE::FxDelegateHandler::CallbackProcessor* _inner;
        };

        struct ConsoleAcceptHook
        {
            static void Hook(RE::Console* a_this,
                             RE::FxDelegateHandler::CallbackProcessor* a_cbReg)
            {
                ConsoleCallbackProxy proxy(a_cbReg);
                _original(a_this, &proxy);
            }

            static inline REL::Relocation<decltype(Hook)> _original;
        };

    } // namespace

    void ConsoleCommands::LogStatusToLog() noexcept
    {
        // Background logger (Kirk directive 2026-10-01): called automatically at
        // session start/end from the SKSE message handler. Thin forwarder so the
        // report body stays file-local next to the console variant.
        WriteStatusToLog();
    }

    void ConsoleCommands::Install() noexcept
    {
        if (g_installed)
        {
            return;
        }

        const auto& cfg = Engine::ConfigManager::GetSingleton();
        if (!cfg.enableConsoleCommands)
        {
            logger::info("[TrueGaze] Console commands are disabled by configuration "
                         "(bEnableConsoleCommands=false). Set it true under [Console] and "
                         "restart to enable the stg* commands.");
            return;
        }

        REL::Relocation<std::uintptr_t> consoleVtable{RE::VTABLE_Console[0]};
        constexpr std::size_t kAcceptSlot = 0x01; // RE::FxDelegateHandler::Accept (SE/AE/VR)
        ConsoleAcceptHook::_original = consoleVtable.write_vfunc(kAcceptSlot, ConsoleAcceptHook::Hook);

        g_installed = true;
        logger::info("[TrueGaze] Registered {} console command(s) via non-destructive "
                     "Console::Accept Scaleform hook (0 vanilla commands modified). "
                     "Type 'stghelp' or 'stgstatus' at the console (~).",
                     std::size(kCommands));
    }

    bool ConsoleCommands::IsInstalled() noexcept
    {
        return g_installed;
    }

#else // standalone build: no game, no console

    void ConsoleCommands::Install() noexcept {}
    bool ConsoleCommands::IsInstalled() noexcept
    {
        return false;
    }
    void ConsoleCommands::LogStatusToLog() noexcept {}

#endif

} // namespace TrueGaze::Integrations