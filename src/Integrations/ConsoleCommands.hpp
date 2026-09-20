#pragma once

namespace TrueGaze::Integrations
{

    /// @brief Registers runtime console commands for controlling TrueGaze.
    ///
    /// ## Why the console, and why without Papyrus
    ///
    /// The console (`~`) is the runtime control surface; `TrueGaze.ini` and the HTML
    /// page are the authoring surface. They do different jobs. The console is where
    /// you flip things on and off to *look* at them, because `~` pauses the game and
    /// frees the camera - which is exactly the moment you want to inspect a gaze beam.
    /// An INI edit plus a restart is the wrong tool for that.
    ///
    /// TrueGaze ships **no Papyrus** by design, and this must not reintroduce it.
    /// That is also not merely a preference - it is a hard technical requirement:
    /// **Papyrus native functions cannot be invoked from the console.** `~` executes
    /// console command-table entries and script text, not Papyrus natives. So the
    /// commands below are registered as genuine `SCRIPT_FUNCTION` entries in the
    /// engine's own console command table, which is the only route that works with
    /// no Papyrus, no ESP and no MCM.
    ///
    /// ## How registration works
    ///
    /// There is **no count** of console commands. The SDK's own
    /// `LocateConsoleCommand` scans the whole array and identifies a real command by a
    /// per-entry marker: an entry with an empty `helpString` is empty, and otherwise the
    /// help string ends with '1' (live) or '0' (a command that once existed).
    ///
    /// So this does not append - it **reclaims** entries the engine already treats as
    /// not-a-command (dead or empty) and writes our command into them. That needs no
    /// count, and it cannot displace a working vanilla command because a live entry
    /// (marker '1') is never a candidate. No vtable is touched.
    ///
    /// This replaced an earlier revision that assumed a count sat after the array. That
    /// version safely refused to write at runtime - it did not corrupt anything - but it
    /// was wrong, and the refusal is recorded in this file so the mistake is not
    /// repeated.
    ///
    /// ## Scope, stated plainly
    ///
    /// The commands deliberately re-use the **existing** configuration and tuning
    /// path: they set `ConfigManager` values, persist them, and call
    /// `GazeEngine::RefreshTuning()`. Nothing here is a second source of truth, and
    /// nothing is invented that the INI cannot also express. A command is a remote
    /// control, not a new subsystem.
    ///
    /// Commands are added here and nowhere else. A Papyrus interface was removed
    /// from this project on purpose and must not be reintroduced in any form.
    class ConsoleCommands
    {
    public:
        /// @brief Registers the console commands with the engine.
        ///
        /// Idempotent: a second call is a no-op. Safe to call on `kDataLoaded`.
        /// Reports honestly if the command table could not be reached, rather than
        /// claiming success for work it did not do.
        static void Install() noexcept;

        /// @brief True once the commands have been registered successfully.
        [[nodiscard]] static bool IsInstalled() noexcept;
    };

} // namespace TrueGaze::Integrations