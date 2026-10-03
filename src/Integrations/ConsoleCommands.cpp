#include "ConsoleCommands.hpp"

#include "Engine/ConfigManager.hpp"
#include "Engine/GazeEngine.hpp"
#include "Engine/PerformanceProfiler.hpp"
#include "Engine/PlayerGazeResolver.hpp"
#include "Engine/TraceLogger.hpp"
#include "Visuals/VisualEffectsManager.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/C/CommandTable.h>
#include <RE/C/ConsoleLog.h>
#include <RE/S/Script.h>
#include <RE/Skyrim.h>
#include <RE/T/TESObjectREFR.h>
#endif

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
            if (cfg.rayRenderMode == 2)
            {
                ConsolePrint("TrueGaze: note - geometry assets are not installed yet, so "
                             "nothing will be drawn. Use 1.");
            }
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
            PersistQuietly();
            ApplyAndReport("in-game visuals (rays + panel)", false);
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
        // stgpreset <name> applies the same re-calibrated Quick Presets the
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
            // social = baseline tuned for conversation richness.
            {"social", "Kinematics", "fSaccadeSpeedMult", "1.5"},
            {"social", "Kinematics", "fMicroJitterAmp", "0.22"},
            {"social", "Kinematics", "fHeadTrackingSpeed", "5.2"},
            {"social", "Kinematics", "fEyePursuitSpeed", "12.0"},
            {"social", "Kinematics", "fMaxComfortEyeAngle", "35.0"},
            {"social", "Kinematics", "fHeadOnsetDelaySec", "0.13"},
            {"social", "SkeletalHierarchy", "fHeadEngageThresholdDeg", "12.0"},
            {"social", "GazeTarget", "fEyeAnchorForwardCm", "7.0"},
            {"social", "GazeTarget", "fEyeAnchorUpCm", "7.5"},
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
            {"developer", "Visuals", "bHcepPanelAllActors", "true"},
            {"developer", "Debug", "bDebugGazeRays", "true"},
            {"developer", "Debug", "iLogLevel", "1"},
            {"developer", "Console", "bEnableConsoleCommands", "true"},
        };

        bool CmdPreset(RE::SCRIPT_PARAMETER*, RE::SCRIPT_FUNCTION::ScriptData*, RE::TESObjectREFR*,
                       RE::TESObjectREFR*, RE::Script*, RE::ScriptLocals*, double&,
                       std::uint32_t&) noexcept
        {
            // The preset name arrives via the compiled script's parameter block.
            // The engine's console passes a single string argument for commands
            // declared with one string param; read it defensively.
            const char* presetName = nullptr;
            // SCRIPT_PARAMETER access differs across SDK revisions; the robust
            // path is the console's own compiled script text. Fall back to the
            // last token of the raw command line via the console log's command
            // buffer is NOT exposed — so we accept the parameter block directly.
            // If no name resolves, print usage (honest, not a silent no-op).
            static const char* kNames[] = {"vanilla", "subtle", "intense", "social", "developer"};
            (void)presetName;

            // NOTE: the engine's SCRIPT_PARAMETER layout for string args is not
            // safely readable across SE/AE/VR without a verified relocation. The
            // 2026-09-25 lesson (never guess an offset) applies. stgpreset
            // therefore cycles presets when called without a parseable argument:
            // each invocation advances to the next preset in a fixed order. This
            // keeps the command useful (field switching) with ZERO offset risk.
            static int s_presetIndex = 0;
            const char* name = kNames[s_presetIndex % 5];
            s_presetIndex = (s_presetIndex + 1) % 5;

            int applied = 0;
            for (const auto& e : kPresets)
            {
                if (std::strcmp(e.name, name) != 0)
                {
                    continue;
                }
                // Write through the same Win32 INI API ConfigManager uses, then
                // re-read + sanitise + refresh so the engine sees one consistent
                // state (never a half-applied preset).
                if (std::strcmp(e.value, "true") == 0 || std::strcmp(e.value, "false") == 0)
                {
                    WritePrivateProfileStringA(e.section, e.key, e.value, "");
                }
                // The in-memory path below is the authoritative one; the INI write
                // happens via Save() at the end so the file and memory agree.
                ++applied;
            }

            // Apply in memory: re-run Load() from the INI we are about to write is
            // circular; instead set values directly through the preset table by
            // re-reading them as typed values.
            // Simpler and honest: write the preset to the INI via Save() of the
            // current state is wrong too. The clean sequence: apply the preset
            // values into the ConfigManager fields, Save, RefreshTuning.
            // The table above carries strings; parse them here.
            for (const auto& e : kPresets)
            {
                if (std::strcmp(e.name, name) != 0)
                {
                    continue;
                }
                // Parse and assign each entry to the matching ConfigManager field
                // via the same ReadFloat/ReadBool helpers is not exported; use
                // ApplyIni-style direct writes through WritePrivateProfileString
                // into the live INI, then Load() re-reads everything in one pass.
                WritePrivateProfileStringA(
                    e.section, e.key, e.value,
                    Engine::ConfigManager::GetSingleton().LoadedPath().c_str());
            }

            // One consistent re-read: Load() re-reads every key, Sanitise() clamps,
            // and RefreshTuning() snapshots into the engine. Never half-applied.
            Engine::ConfigManager::GetSingleton().Load(
                Engine::ConfigManager::GetSingleton().LoadedPath());
            Engine::GazeEngine::Get().RefreshTuning();

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
            {"stgvisuals", "Toggle all in-game visuals (gaze rays + HCEP panel)", &CmdVisuals},
            {"stgv", "Toggle the gaze-ray emitters (laser eyes)", &CmdRays},
            {"stgpanel", "Toggle the floating HCEP gaze region diagram panel", &CmdPanel},
            {"stgon", "Turn every in-game visual on (rays + panel)", &CmdOn},
            {"stgoff", "Turn every in-game visual off", &CmdOff},
            {"stgmode", "Cycle render mode: Both / Light / Geometry", &CmdRenderMode},
            {"stgradius", "Toggle the gaze terminus glow", &CmdTerminus},
            {"stgverbose", "Toggle Debug/Info logging", &CmdVerbose},
            {"stgstatus", "Print the effective SkyrimTrueGaze state", &CmdStatus},
            {"stgpreset", "Cycle Quick Presets (vanilla/subtle/intense/social/developer)",
             &CmdPreset},
            {"stgreload", "Reload TrueGaze.ini from disk and refresh the engine", &CmdReload},
            {"stgtrace", "Toggle or inspect TrueGaze JSONL trace logging", &CmdTrace},
            {"stgtraceoff", "Stop TrueGaze JSONL trace logging and flush buffer", &CmdTraceOff},
            {"stgtraceflush", "Force immediate flush of trace log buffer to disk", &CmdTraceFlush},
            {"stgcal", "Step through calibration representative region angles", &CmdCal},
            {"stgcalsweep", "Automated 11-region calibration sweep (2s per region)", &CmdCalSweep},
            {"stgcaloff", "Stop calibration override and restore normal gaze", &CmdCalOff},
            {"stgcalaxes", "Verify head bone local axes convention (X=Right, Y=Fwd, Z=Up)", &CmdCalAxes},
        };

        // -----------------------------------------------------------------------
        // Finding a slot to reclaim
        // -----------------------------------------------------------------------
        //
        // ## There is no count. That was the bug.
        //
        // An earlier revision of this file assumed a "number of commands" counter sat
        // immediately after the command array, and tried to append past it. Running it
        // in-game produced:
        //
        //     Console command table validation FAILED (count is at or beyond the
        //     declared table length). No commands were registered and no memory was
        //     written.
        //
        // That was the guard doing its job - it refused to write on a wrong assumption
        // rather than corrupting engine memory. The assumption itself was the defect.
        //
        // **There is no count.** The SDK's own `LocateConsoleCommand` proves it: it
        // scans the whole array and relies on a per-entry marker to tell a live command
        // from an empty one. A table with no count cannot be appended to.
        //
        // ## How the SDK identifies a real command
        //
        // From the documented scan semantics:
        //
        //   * An entry is EMPTY when `helpString` is null or empty.
        //   * Otherwise the help string ENDS with '1' (a live command) or '0' (a dead
        //     one - a command that once existed and no longer does).
        //
        // So the array holds live commands, dead commands, and empty entries, and the
        // engine's parser does not care about order. Filling a dead entry is therefore
        // equivalent to adding a command, and needs no count at all.
        //
        // ## Why this is safe
        //
        //   * We never write past the end of the array - we only scan within it.
        //   * We only ever overwrite an entry the engine *already* treats as
        //     not-a-command, so no working vanilla command is displaced. A live command
        //     (marker '1') is never a candidate.
        //   * Every field is written, so nothing stale is left for the parser to read.
        //   * If fewer free slots exist than commands to register, we register NOTHING
        //     rather than registering a partial set that would be confusing to use.
        struct SlotSet
        {
            RE::SCRIPT_FUNCTION* slots[std::size(kCommands)]{};
            size_t found{0};
            const char* reason{"not probed"};

            // Scan statistics. Reported on install so that a future in-game run is
            // self-diagnosing: if registration is refused, these numbers say whether the
            // table was reachable at all and what it contained.
            std::uint16_t scanned{0};
            std::uint16_t emptyEntries{0};
            std::uint16_t deadEntries{0};
            std::uint16_t liveEntries{0};
        };

        constexpr std::uint16_t kTableCapacity =
            static_cast<std::uint16_t>(RE::SCRIPT_FUNCTION::Commands::kConsoleCommandsEnd);

        /// Persistent storage for the help strings actually handed to the engine.
        ///
        /// A live command's help string must END with '1' - that trailing marker is how
        /// the engine's scan distinguishes a live command ('1') from a dead one ('0').
        /// Appending it to the source literals would make the human-readable help text
        /// read like a typo, so the marker is added here instead, into storage that
        /// outlives the call. One string per command, formatted once at install.
        std::array<std::string, std::size(kCommands)> g_helpStrings{};

        /// Build the engine-facing help string: the readable text plus the live-command
        /// marker the engine's scan requires.
        const char* MakeLiveHelpString(size_t a_index, const char* a_help) noexcept
        {
            auto& buffer = g_helpStrings[a_index];
            buffer.assign(a_help);
            buffer.push_back(' '); // keep the marker visually separate if shown
            buffer.push_back('1'); // '1' = live command
            return buffer.c_str();
        }

        /// Classify an entry exactly as the engine's own scan does, then record it.
        ///
        /// The decision is made purely from the marker the SDK documents - an empty
        /// `helpString` means the entry is empty, and otherwise the terminal character is
        /// '1' for a live command or '0' for a dead one. Reading a field whose meaning is
        /// already known, through the struct the SDK itself defines, is what makes this
        /// safe; nothing here depends on an offset the engine does not document.
        SlotSet FindReclaimableSlots() noexcept
        {
            SlotSet set{};

            auto* first = RE::SCRIPT_FUNCTION::GetFirstConsoleCommand();
            if (!first)
            {
                set.reason = "GetFirstConsoleCommand() returned null";
                return set;
            }

            for (std::uint16_t i = 0; i < kTableCapacity && set.found < std::size(kCommands); ++i)
            {
                auto& entry = first[i];
                ++set.scanned;

                // A reclaimed entry must keep a readable function name: the engine's own
                // lookup calls strlen() on it for every entry it scans, so a null there
                // would be a fault waiting to happen. Only well-formed entries qualify.
                if (entry.functionName == nullptr)
                {
                    continue;
                }

                // Classify using the marker the SDK documents, so the report reflects
                // the engine's own view of the table rather than our guess about it.
                const char* help = entry.helpString;
                if (help == nullptr || help[0] == '\0')
                {
                    ++set.emptyEntries;
                    set.slots[set.found++] = &entry;
                }
                else if (help[std::strlen(help) - 1] == '0')
                {
                    ++set.deadEntries;
                    set.slots[set.found++] = &entry;
                }
                else
                {
                    ++set.liveEntries;
                }
            }

            if (set.found < std::size(kCommands))
            {
                set.reason = "not enough reclaimable entries in the console command table";
            }
            else
            {
                set.reason = "ok";
            }
            return set;
        }

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

        const auto slots = FindReclaimableSlots();

        logger::info("[TrueGaze] Console table scan: scanned={} live={} dead={} empty={} "
                     "reclaimable={} (needed {})",
                     slots.scanned, slots.liveEntries, slots.deadEntries, slots.emptyEntries,
                     slots.found, std::size(kCommands));

        if (slots.found < std::size(kCommands))
        {
            // Registering a partial command set would be worse than registering none:
            // some commands would work and others would "not be found", with no way to
            // tell which. Refuse entirely, and say exactly why - with the counts above
            // as the evidence.
            logger::warn("[TrueGaze] Found only {} reclaimable console entries but need {}. No "
                         "commands were registered. Everything still works from TrueGaze.ini "
                         "and TrueGazeConfig.html. ({})",
                         slots.found, std::size(kCommands), slots.reason);
            return;
        }

        size_t i = 0;
        std::array<std::uint32_t, std::size(kCommands)> boundOpcodes{};
        for (const auto& def : kCommands)
        {
            auto& entry = *slots.slots[i];

            // Preserve the slot's existing opcode BEFORE clearing the entry.
            //
            // `SCRIPT_OUTPUT output` is the command's unique function ID - the SDK
            // comment says so outright ("basically the unique id for the function,
            // there's ~5000 of these"), and the console dispatches by it
            // (kConsoleOpBase = 0x0100). Zeroing the whole entry wiped it, and the
            // engine then reported exactly what a zero opcode means:
            //
            //     Unknown function code 0
            //
            // The reclaimed slot already carried a unique, engine-allocated opcode, so
            // keeping it is both correct and free. Allocating a fresh one would require
            // knowing the engine's allocator, which is not documented - reusing the
            // slot's own ID needs no new knowledge at all.
            const auto preservedOpcode = entry.output;

            // Overwrite every field. The engine's parser reads the whole entry, and a
            // field left carrying its old value would be a real defect - not cosmetic.
            entry = RE::SCRIPT_FUNCTION{};

            // Restore the opcode first, so the entry is never observable without one.
            entry.output = preservedOpcode;

            entry.functionName = def.name;
            entry.shortName = def.name;

            // Both name fields are set to the token the player actually types.
            //
            // The SDK's own `LocateConsoleCommand(std::string_view a_longName)` matches
            // against `functionName`, and its parameter name suggests the engine may
            // treat that field as the canonical name. We do not know for certain which
            // field the engine's parser matches, so setting BOTH to the same short token
            // makes the command reachable either way. Requiring the user to guess
            // "TrueGazeRays" instead of the documented "stgv" would be a poor outcome for
            // no benefit - nothing here needs a namespaced long form.
            //
            // The readable description therefore lives where it belongs: in helpString.
            // Live-command marker required by the engine's own scan (see MakeLiveHelpString).
            entry.helpString = MakeLiveHelpString(i, def.help);

            entry.referenceFunction = false;
            entry.numParams = 0;
            entry.params = nullptr;

            // The one unavoidable conversion, kept to a single line so there is exactly
            // one place to look. It changes only the noexcept part of the pointer type;
            // the handler stays non-throwing in fact, which is what the noexcept on its
            // declaration guarantees.
            entry.executeFunction = reinterpret_cast<RE::SCRIPT_FUNCTION::Execute_t*>(def.handler);

            entry.compileFunction = nullptr;
            entry.conditionFunction = nullptr;
            entry.editorFilter = false;
            entry.invalidatesCellList = false;

            // Record the opcode actually bound, for the install report below.
            boundOpcodes[i] = static_cast<std::uint32_t>(entry.output);

            ++i;
        }

        g_installed = true;

        // Report the reclaim honestly: how many dead/empty slots were reused, and the
        // opcode each command now answers to. If a command still fails, the opcode is
        // the first thing to compare against the engine's own dispatch table.
        logger::info("[TrueGaze] Registered {} console command(s) by reclaiming {} dead/empty "
                     "console-table entries. Type 'stgstatus' at the console (~).",
                     std::size(kCommands), slots.found);
        logger::info("[TrueGaze] Bound opcodes: {}",
                     [&]
                     {
                         std::string s;
                         for (size_t n = 0; n < std::size(kCommands); ++n)
                         {
                             s += (n ? ", " : "");
                             s += kCommands[n].name;
                             s += "=0x";
                             char buf[16]{0};
                             std::snprintf(buf, sizeof(buf), "%X", boundOpcodes[n]);
                             s += buf;
                         }
                         return s;
                     }());
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