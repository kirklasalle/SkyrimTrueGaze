#include "ConsoleCommands.hpp"

#include "Engine/ConfigManager.hpp"
#include "Engine/GazeEngine.hpp"
#include "Engine/PlayerGazeResolver.hpp"
#include "Visuals/VisualEffectsManager.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#include <RE/C/CommandTable.h>
#include <RE/C/ConsoleLog.h>
#include <RE/S/Script.h>
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

        // -----------------------------------------------------------------------
        // Console output
        // -----------------------------------------------------------------------
        void ConsolePrint(const char *a_fmt, ...) noexcept
        {
            char buf[512]{0};
            va_list args;
            va_start(args, a_fmt);
            std::vsnprintf(buf, sizeof(buf), a_fmt, args);
            va_end(args);

            if (auto *console = RE::ConsoleLog::GetSingleton())
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
        void ApplyAndReport(const char *a_label, bool a_enabled) noexcept
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

        bool CmdMaster(RE::SCRIPT_PARAMETER *, RE::SCRIPT_FUNCTION::ScriptData *, RE::TESObjectREFR *,
                       RE::TESObjectREFR *, RE::Script *, RE::ScriptLocals *, double &, std::uint32_t &) noexcept
        {
            auto &cfg = Engine::ConfigManager::GetSingleton();
            cfg.enableTrueGaze = !cfg.enableTrueGaze;
            PersistQuietly();
            ApplyAndReport("bEnableTrueGaze", cfg.enableTrueGaze);
            return true;
        }

        bool CmdVisuals(RE::SCRIPT_PARAMETER *, RE::SCRIPT_FUNCTION::ScriptData *, RE::TESObjectREFR *,
                        RE::TESObjectREFR *, RE::Script *, RE::ScriptLocals *, double &, std::uint32_t &) noexcept
        {
            auto &cfg = Engine::ConfigManager::GetSingleton();
            cfg.enableInGameVisuals = !cfg.enableInGameVisuals;

            // Turning the master ON also arms the rays. The master switch alone would
            // show nothing - the rays key is separate - and "turn the visuals on"
            // plainly means "show me the visuals".
            if (cfg.enableInGameVisuals && !cfg.gazeRaysEnabled)
            {
                cfg.gazeRaysEnabled = true;
            }

            PersistQuietly();
            ApplyAndReport("bEnableInGameVisuals", cfg.enableInGameVisuals);
            ConsolePrint("TrueGaze: gaze rays = %s", cfg.gazeRaysEnabled ? "ON" : "OFF");
            return true;
        }

        bool CmdRays(RE::SCRIPT_PARAMETER *, RE::SCRIPT_FUNCTION::ScriptData *, RE::TESObjectREFR *,
                     RE::TESObjectREFR *, RE::Script *, RE::ScriptLocals *, double &, std::uint32_t &) noexcept
        {
            auto &cfg = Engine::ConfigManager::GetSingleton();
            cfg.gazeRaysEnabled = !cfg.gazeRaysEnabled;

            // The rays are a child of the master switch, so enabling them implies it.
            if (cfg.gazeRaysEnabled)
            {
                cfg.enableInGameVisuals = true;
            }

            PersistQuietly();
            ApplyAndReport("bGazeRaysEnabled", cfg.gazeRaysEnabled);
            return true;
        }

        /// Cycle Both -> Light only -> Geometry only -> Both.
        bool CmdRenderMode(RE::SCRIPT_PARAMETER *, RE::SCRIPT_FUNCTION::ScriptData *, RE::TESObjectREFR *,
                           RE::TESObjectREFR *, RE::Script *, RE::ScriptLocals *, double &, std::uint32_t &) noexcept
        {
            auto &cfg = Engine::ConfigManager::GetSingleton();
            cfg.rayRenderMode = (cfg.rayRenderMode + 1) % 3;
            PersistQuietly();
            Engine::GazeEngine::Get().RefreshTuning();

            const char *name = (cfg.rayRenderMode == 0)   ? "Both (light + branded geometry)"
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

        bool CmdTerminus(RE::SCRIPT_PARAMETER *, RE::SCRIPT_FUNCTION::ScriptData *, RE::TESObjectREFR *,
                         RE::TESObjectREFR *, RE::Script *, RE::ScriptLocals *, double &, std::uint32_t &) noexcept
        {
            auto &cfg = Engine::ConfigManager::GetSingleton();
            cfg.gazeRaysTerminus = !cfg.gazeRaysTerminus;
            PersistQuietly();
            ApplyAndReport("bGazeRaysTerminus", cfg.gazeRaysTerminus);
            return true;
        }

        bool CmdVerbose(RE::SCRIPT_PARAMETER *, RE::SCRIPT_FUNCTION::ScriptData *, RE::TESObjectREFR *,
                        RE::TESObjectREFR *, RE::Script *, RE::ScriptLocals *, double &, std::uint32_t &) noexcept
        {
            auto &cfg = Engine::ConfigManager::GetSingleton();
            // Flip between the shipped default (2, Info) and full diagnostics (1,
            // Debug). Someone reaching for this wants more detail, not less.
            cfg.logLevel = (cfg.logLevel == 1) ? 2 : 1;
            PersistQuietly();
            ConsolePrint("TrueGaze: iLogLevel = %d (%s)", cfg.logLevel,
                         cfg.logLevel == 1 ? "Debug" : "Info");
            return true;
        }

        bool CmdOn(RE::SCRIPT_PARAMETER *, RE::SCRIPT_FUNCTION::ScriptData *, RE::TESObjectREFR *,
                   RE::TESObjectREFR *, RE::Script *, RE::ScriptLocals *, double &, std::uint32_t &) noexcept
        {
            auto &cfg = Engine::ConfigManager::GetSingleton();
            cfg.enableInGameVisuals = true;
            cfg.gazeRaysEnabled = true;
            PersistQuietly();
            ApplyAndReport("in-game visuals", true);
            return true;
        }

        bool CmdOff(RE::SCRIPT_PARAMETER *, RE::SCRIPT_FUNCTION::ScriptData *, RE::TESObjectREFR *,
                    RE::TESObjectREFR *, RE::Script *, RE::ScriptLocals *, double &, std::uint32_t &) noexcept
        {
            auto &cfg = Engine::ConfigManager::GetSingleton();
            cfg.enableInGameVisuals = false;
            cfg.gazeRaysEnabled = false;
            PersistQuietly();
            ApplyAndReport("in-game visuals", false);
            return true;
        }

        /// Print the effective state of every TrueGaze switch. The most useful command
        /// in the set: it answers "is it actually on?" without reading a file.
        bool CmdStatus(RE::SCRIPT_PARAMETER *, RE::SCRIPT_FUNCTION::ScriptData *, RE::TESObjectREFR *,
                       RE::TESObjectREFR *, RE::Script *, RE::ScriptLocals *, double &, std::uint32_t &) noexcept
        {
            const auto &cfg = Engine::ConfigManager::GetSingleton();
            auto &engine = Engine::GazeEngine::Get();
            auto &visuals = Visuals::VisualEffectsManager::Get();

            auto onOff = [](bool b)
            { return b ? "ON " : "OFF"; };

            ConsolePrint("TrueGaze - status");
            ConsolePrint("  simulation       %s  bEnableTrueGaze", onOff(cfg.enableTrueGaze));
            ConsolePrint("  creatures        %s  bEnableCreatures", onOff(cfg.enableCreatures));
            ConsolePrint("  in-game visuals  %s  bEnableInGameVisuals", onOff(cfg.enableInGameVisuals));
            ConsolePrint("  gaze rays        %s  bGazeRaysEnabled", onOff(cfg.gazeRaysEnabled));
            ConsolePrint("  terminus glow    %s  bGazeRaysTerminus", onOff(cfg.gazeRaysTerminus));
            ConsolePrint("  render mode      %d   (0=Both 1=Light 2=Geometry)", cfg.rayRenderMode);
            ConsolePrint("  ray length       %.1f m, colour #%06X, opacity %.2f",
                         cfg.gazeRayLengthMeters, cfg.gazeRayColour & 0x00FFFFFF, cfg.gazeRayOpacity);
            ConsolePrint("  pupil offset     fwd %.1f cm, up %.1f cm",
                         cfg.pupilForwardOffsetCm, cfg.pupilUpOffsetCm);
            ConsolePrint("  log level        %d   (1=Debug 2=Info)", cfg.logLevel);
            ConsolePrint("  tracked actors   %zu", engine.TrackedActorCount());
            ConsolePrint("  tick calls       %llu", static_cast<unsigned long long>(engine.TickCalls()));
            ConsolePrint("  eligible ticks   %llu", static_cast<unsigned long long>(engine.EligibleTicks()));
            ConsolePrint("  LOD culled       %llu", static_cast<unsigned long long>(engine.CulledTicks()));
            ConsolePrint("  target resolves  %llu (none %llu)",
                         static_cast<unsigned long long>(engine.TargetResolutions()),
                         static_cast<unsigned long long>(engine.NoTargetResolutions()));
            ConsolePrint("  last target      priority %u, form %08X",
                         static_cast<unsigned>(engine.LastTargetPriority()),
                         engine.LastTargetFormId());
            ConsolePrint("  rig origin       %s (eye-absent %llu, head-absent %llu)",
                         engine.LastRigOrigin(),
                         static_cast<unsigned long long>(engine.EyeNodeAbsentCount()),
                         static_cast<unsigned long long>(engine.HeadAnchorAbsentCount()));

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
                                 static_cast<unsigned>(intent.sequenceId),
                                 intent.confidence,
                                 static_cast<unsigned long long>(intent.ageMs),
                                 intent.stale ? " [STALE]" : "",
                                 intent.blinkSuppressed ? " [BLINK]" : "");
                    ConsolePrint("  hcep head        yaw %+.1f deg pitch %+.1f deg, convergence %.2f m%s",
                                 intent.headYawDeg,
                                 intent.headPitchDeg,
                                 intent.convergenceMeters,
                                 intent.convergencePlausible ? "" : " [implausible]");
                }
            }
            ConsolePrint("  visual emitters  %zu actors, %zu lights",
                         visuals.ActiveActorCount(), visuals.AttachedLightCount());
            ConsolePrint("  visual updates   %llu, anchors failed %llu, light creates failed %llu",
                         static_cast<unsigned long long>(visuals.UpdateCalls()),
                         static_cast<unsigned long long>(visuals.AnchorFailures()),
                         static_cast<unsigned long long>(visuals.LightCreateFailures()));
            ConsolePrint("  beam geometry    %llu attached / %llu attempts",
                         static_cast<unsigned long long>(visuals.GeometryCreated()),
                         static_cast<unsigned long long>(visuals.GeometryAttempts()));
            ConsolePrint("  commands         %s", ConsoleCommands::IsInstalled() ? "registered" : "NOT registered");
            return true;
        }

        // -----------------------------------------------------------------------
        // The command set
        // -----------------------------------------------------------------------

        struct CommandDef
        {
            const char *name; // the token typed at the console prompt
            const char *help;

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
            using Handler = bool (*)(RE::SCRIPT_PARAMETER *, RE::SCRIPT_FUNCTION::ScriptData *,
                                     RE::TESObjectREFR *, RE::TESObjectREFR *, RE::Script *,
                                     RE::ScriptLocals *, double &, std::uint32_t &) noexcept;
            Handler handler;
        };

        static_assert(std::is_same_v<decltype(&CmdMaster), CommandDef::Handler>,
                      "command handlers must match CommandDef::Handler exactly");

        /// One table, so the registrations and the count can never disagree.
        /// Deliberately small and focused: toggles, plus a status read-out.
        constexpr CommandDef kCommands[] = {
            {"tg", "Toggle the TrueGaze simulation on/off", &CmdMaster},
            {"tgvisuals", "Toggle all in-game visuals on/off", &CmdVisuals},
            {"tgv", "Toggle the gaze-ray emitters (laser eyes)", &CmdRays},
            {"tgon", "Turn every in-game visual on", &CmdOn},
            {"tgoff", "Turn every in-game visual off", &CmdOff},
            {"tgmode", "Cycle render mode: Both / Light / Geometry", &CmdRenderMode},
            {"tgradius", "Toggle the gaze terminus glow", &CmdTerminus},
            {"tgverbose", "Toggle Debug/Info logging", &CmdVerbose},
            {"tgstatus", "Print the effective TrueGaze state", &CmdStatus},
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
            RE::SCRIPT_FUNCTION *slots[std::size(kCommands)]{};
            size_t found{0};
            const char *reason{"not probed"};

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
        const char *MakeLiveHelpString(size_t a_index, const char *a_help) noexcept
        {
            auto &buffer = g_helpStrings[a_index];
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

            auto *first = RE::SCRIPT_FUNCTION::GetFirstConsoleCommand();
            if (!first)
            {
                set.reason = "GetFirstConsoleCommand() returned null";
                return set;
            }

            for (std::uint16_t i = 0; i < kTableCapacity && set.found < std::size(kCommands); ++i)
            {
                auto &entry = first[i];
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
                const char *help = entry.helpString;
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

    void ConsoleCommands::Install() noexcept
    {
        if (g_installed)
        {
            return;
        }

        const auto &cfg = Engine::ConfigManager::GetSingleton();
        if (!cfg.enableConsoleCommands)
        {
            logger::info("[TrueGaze] Console commands are disabled by configuration "
                         "(bEnableConsoleCommands=false). Set it true under [Console] and "
                         "restart to enable the tg* commands.");
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
        for (const auto &def : kCommands)
        {
            auto &entry = *slots.slots[i];

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
            // "TrueGazeRays" instead of the documented "tgv" would be a poor outcome for
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
            entry.executeFunction = reinterpret_cast<RE::SCRIPT_FUNCTION::Execute_t *>(def.handler);

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
                     "console-table entries. Type 'tgstatus' at the console (~).",
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
    bool ConsoleCommands::IsInstalled() noexcept { return false; }

#endif

} // namespace TrueGaze::Integrations