#include "PCH.h"
#include "Bridge/NamedPipeServer.hpp"
#include "Engine/AnimationHook.hpp"
#include "Engine/ConfigManager.hpp"
#include "Engine/GazeEngine.hpp"
#include "Integrations/OarConditions.hpp"
#include "Integrations/ConsoleCommands.hpp"

#if __has_include(<SKSE/SKSE.h>)
#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <filesystem>

namespace
{
    std::unique_ptr<TrueGaze::Bridge::NamedPipeServer> g_pipeServer;

    void InitializeLogging()
    {
#if __has_include(<SKSE/SKSE.h>)
        auto path = logger::log_directory();
        if (!path)
        {
            return;
        }

        *path /= "TrueGaze.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto log = std::make_shared<spdlog::logger>("global log", std::move(sink));

        log->set_level(spdlog::level::info);
        log->flush_on(spdlog::level::info);

        spdlog::set_default_logger(std::move(log));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

        // Clean up bridge server gracefully before Windows Loader Lock is acquired on process exit.
        std::atexit([]()
                    { TrueGaze::Engine::GazeEngine::Get().StopBridge(); });
#else
        char myDocs[MAX_PATH]{0};
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, myDocs)))
        {
            std::string logDir = std::string(myDocs) + "\\My Games\\Skyrim Special Edition\\SKSE";
            std::error_code ec;
            std::filesystem::create_directories(logDir, ec);
            std::string logPath = logDir + "\\TrueGaze.log";
            FILE *f = fopen(logPath.c_str(), "w");
            if (f)
            {
                fprintf(f, "[TrueGaze] True Gaze v1.0.0 (An HCEP Product by Kirk LaSalle) loaded.\n");
                fprintf(f, "[TrueGaze] Biomechanical Oculomotor Kinematics Engine initialized.\n");
                fprintf(f, "[TrueGaze] Target engine: Skyrim Special Edition / AE.\n");
                fclose(f);
            }
        }
#endif
    }

#if __has_include(<SKSE/SKSE.h>)
    void ApplyLogLevel(int a_level) noexcept
    {
        spdlog::level::level_enum lvl = spdlog::level::info;
        switch (a_level)
        {
        case 0:
            lvl = spdlog::level::trace;
            break;
        case 1:
            lvl = spdlog::level::debug;
            break;
        case 2:
            lvl = spdlog::level::info;
            break;
        case 3:
            lvl = spdlog::level::warn;
            break;
        case 4:
            lvl = spdlog::level::err;
            break;
        default:
            lvl = spdlog::level::info;
            break;
        }

        // If debug gaze rays are enabled, promote the level to at least info
        // so the user's requested 3D ray diagnostics are never silenced.
        if (TrueGaze::Engine::ConfigManager::GetSingleton().debugGazeRays && lvl > spdlog::level::info)
        {
            lvl = spdlog::level::info;
        }

        if (auto log = spdlog::default_logger())
        {
            log->set_level(lvl);
            log->flush_on(lvl);
        }
    }

    /// Phase S1 (SOTA plan): emit a reproducible startup fingerprint.
    ///
    /// Every in-game verification depends on knowing *exactly* what ran. This logs
    /// the plugin build identity, the detected game runtime, and the effective INI
    /// path in one place so a support report or acceptance artifact is unambiguous.
    /// It never throws and never blocks: a missing value is reported as "unknown".
    void LogRuntimeIdentity() noexcept
    {
#if __has_include(<SKSE/SKSE.h>)
        // Plugin build identity. __DATE__/__TIME__ pin the exact binary that ran,
        // which is the cheapest defence against the stale-DLL class of confusion.
        logger::info("[TrueGaze] Runtime identity: plugin v1.0.0 build {} {}",
                     __DATE__, __TIME__);

        // Game runtime version, formatted as the human-readable dotted string the
        // Address Library and SKSE filenames are derived from.
        try
        {
            const auto ver = REL::Module::get().version();
            logger::info("[TrueGaze] Game runtime: {}.{}.{}.{}",
                         ver[0], ver[1], ver[2], ver[3]);
        }
        catch (...)
        {
            logger::info("[TrueGaze] Game runtime: unknown (version query failed)");
        }

        // Effective configuration path, so a reader knows which INI actually drove
        // this session rather than assuming the repository default.
        {
            const auto &cfg = TrueGaze::Engine::ConfigManager::GetSingleton();
            logger::info("[TrueGaze] Effective config: '{}'",
                         cfg.LoadedPath().empty() ? "compiled defaults" : cfg.LoadedPath());
        }
#endif
    }

    void MessageHandler(SKSE::MessagingInterface::Message *a_msg)
    {
        if (!a_msg)
        {
            return;
        }

        auto &engine = TrueGaze::Engine::GazeEngine::Get();
        auto &config = TrueGaze::Engine::ConfigManager::GetSingleton();

        switch (a_msg->type)
        {
        case SKSE::MessagingInterface::kPostLoad:
            // Attempted before data so condition state exists as early as OAR
            // first evaluates. Reports honestly if OAR is unavailable.
            TrueGaze::Integrations::OarConditions::RegisterWithOar();
            break;

        case SKSE::MessagingInterface::kDataLoaded:
            logger::info("[TrueGaze] Game data loaded. Initialising gaze engine.");

            // Configuration is loaded HERE, on the real plugin path. It was
            // previously only loaded in the unreachable #else branch below, so
            // every setting in TrueGaze.ini was inert. See audit finding C-3.
            config.Load();
            ApplyLogLevel(config.logLevel);

            // Phase S1 evidence baseline: fingerprint exactly what is running before
            // any actor ticks, so acceptance logs are self-describing.
            LogRuntimeIdentity();

            engine.RefreshTuning();
            engine.StartBridge();
            TrueGaze::Engine::AnimationHook::Install();

            // Register the runtime console commands (~). These are the interactive
            // control surface; the INI remains the authoring surface. Registered here
            // because it is the earliest message where the engine's command table is
            // populated and the config has been read.
            TrueGaze::Integrations::ConsoleCommands::Install();

            logger::info("[TrueGaze] Gaze engine ready. Bridge {}.",
                         engine.IsBridgeConnected() ? "connected" : "idle");
            break;

        case SKSE::MessagingInterface::kPreLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            // The previous session's skeleton state is unrelated to the new one.
            config.Load();
            ApplyLogLevel(config.logLevel);
            engine.RefreshTuning();
            engine.ResetAll();
            TrueGaze::Integrations::OarConditions::ClearCache();
            logger::info("[TrueGaze] Session reset; actor gaze state cleared.");
            break;

        case SKSE::MessagingInterface::kPostLoadGame:
            // Reload settings so changes made via the INI in a prior session are
            // picked up by the engine.
            config.Load();
            ApplyLogLevel(config.logLevel);
            engine.RefreshTuning();
            break;

        case SKSE::MessagingInterface::kSaveGame:
            // Never let a procedural deflection be baked into a save.
            engine.ReleaseBones();
            break;

        default:
            break;
        }
    }
#endif
}

#if __has_include(<SKSE/SKSE.h>)
// ---------------------------------------------------------------------------
// SKSE loader contract.
//
// This plugin previously exported SKSEPlugin_Load and nothing else. That is the
// legacy shape, and it tells SKSE nothing about which runtimes the plugin
// supports. With no declaration SKSE falls back to the legacy path, which
// matches against the runtime version the plugin happened to be compiled
// against - exactly the brittleness an AE-era plugin has to avoid.
//
// SKSEPluginInfo emits SKSEPlugin_Version, the structured declaration modern
// SKSE reads (name, author, version, runtime independence, minimum SKSE), and
// also emits SKSEPlugin_Query for the legacy path. Both load paths are then
// covered.
//
// RuntimeCompatibility is left at its default, which declares Address Library
// independence. That is accurate: the plugin resolves RE:: offsets through the
// Address Library, and therefore requires
// Data/SKSE/Plugins/versionlib-<game version>.bin to be installed.
// ---------------------------------------------------------------------------
// The trailing semicolon is deliberate. SKSEPluginInfo expands to a whole
// declaration, and clang-format cannot tell where the statement ends without
// it, so it indents the following function as if it were still part of the
// macro arguments. An empty declaration at namespace scope is legal C++.
SKSEPluginInfo(
        .Version = SKSE::PluginDeclaration::VersionNumber{1, 0, 0, 0},
        .Name = "TrueGaze",
        .Author = "Kirk LaSalle (HCEP)",
        .SupportEmail = "",
        .StructCompatibility = SKSE::StructCompatibility::Independent,
        .RuntimeCompatibility = SKSE::PluginDeclaration::RuntimeCompatibility(
            SKSE::VersionIndependence::AddressLibrary),
        .MinimumSKSEVersion = SKSE::PluginDeclaration::VersionNumber{0, 0, 0, 0});

SKSEPluginLoad(const SKSE::LoadInterface *a_skse)
{
    InitializeLogging();
    logger::info("[TrueGaze] Loading True Gaze v1.0.0 (An HCEP Product by Kirk LaSalle)...");

    SKSE::Init(a_skse);

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MessageHandler))
    {
        logger::error("[TrueGaze] Failed to register SKSE messaging listener.");
        return false;
    }

    logger::info("[TrueGaze] SKSE plugin loaded successfully.");
    return true;
}
#else
// ---------------------------------------------------------------------------
// Standalone build: no SKSE headers available.
//
// This branch exists ONLY for the TRUEGAZE_STANDALONE configuration, which
// builds the kinematics library and its tests without the game SDK. It does not
// produce a loadable plugin, and the CMake build refuses to reach this path in a
// normal build. See docs/AUDIT_REPORT_2026-09-11.md finding C-2.
// ---------------------------------------------------------------------------
#pragma message("TrueGaze: SKSE headers not found - building standalone stub (not a loadable plugin).")

int main()
{
    TrueGaze::Engine::ConfigManager::GetSingleton().Load();
    TrueGaze::Engine::GazeEngine::Get().RefreshTuning();
    return 0;
}
#endif
