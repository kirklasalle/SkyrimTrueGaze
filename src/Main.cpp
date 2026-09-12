#include "PCH.h"
#include "Bridge/NamedPipeServer.hpp"
#include "Engine/AnimationHook.hpp"
#include "Engine/ConfigManager.hpp"
#include "Engine/GazeEngine.hpp"
#include "Integrations/OarConditions.hpp"
#include "Integrations/PapyrusInterface.hpp"

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
            engine.RefreshTuning();
            engine.StartBridge();
            TrueGaze::Engine::AnimationHook::Install();

            logger::info("[TrueGaze] Gaze engine ready. Bridge {}.",
                         engine.IsBridgeConnected() ? "connected" : "idle");
            break;

        case SKSE::MessagingInterface::kPreLoadGame:
        case SKSE::MessagingInterface::kNewGame:
            // The previous session's skeleton state is unrelated to the new one.
            config.Load();
            engine.RefreshTuning();
            engine.ResetAll();
            TrueGaze::Integrations::OarConditions::ClearCache();
            logger::info("[TrueGaze] Session reset; actor gaze state cleared.");
            break;

        case SKSE::MessagingInterface::kPostLoadGame:
            config.Load();
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
    .Version = SKSE::PluginDeclaration::VersionNumber{ 1, 0, 0, 0 },
    .Name = "TrueGaze",
    .Author = "Kirk LaSalle (HCEP)",
    .SupportEmail = "",
    .StructCompatibility = SKSE::StructCompatibility::Independent,
    .MinimumSKSEVersion = SKSE::PluginDeclaration::VersionNumber{ 0, 0, 0, 0 });

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

    // Papyrus bindings must be registered during plugin load. Registering later,
    // or not at all, is why the script API was previously non-functional.
    TrueGaze::Integrations::PapyrusInterface::RegisterFunctions();

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
