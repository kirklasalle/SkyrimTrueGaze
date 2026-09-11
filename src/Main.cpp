#include "PCH.h"
#include "Bridge/NamedPipeServer.hpp"
#include "Engine/AnimationHook.hpp"
#include "Engine/ConfigManager.hpp"
#include "Integrations/OarConditions.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#include <filesystem>

namespace {
    std::unique_ptr<TrueGaze::Bridge::NamedPipeServer> g_pipeServer;

    void InitializeLogging()
    {
#if __has_include(<SKSE/SKSE.h>)
        auto path = logger::log_directory();
        if (!path) {
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
        char myDocs[MAX_PATH]{ 0 };
        if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS, nullptr, 0, myDocs))) {
            std::string logDir = std::string(myDocs) + "\\My Games\\Skyrim Special Edition\\SKSE";
            std::error_code ec;
            std::filesystem::create_directories(logDir, ec);
            std::string logPath = logDir + "\\TrueGaze.log";
            FILE* f = fopen(logPath.c_str(), "w");
            if (f) {
                fprintf(f, "[TrueGaze] True Gaze v1.0.0 (An HCEP Product by Kirk LaSalle) loaded.\n");
                fprintf(f, "[TrueGaze] Biomechanical Oculomotor Kinematics Engine initialized.\n");
                fprintf(f, "[TrueGaze] Target engine: Skyrim Special Edition / AE.\n");
                fclose(f);
            }
        }
#endif
    }

#if __has_include(<SKSE/SKSE.h>)
    void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
    {
        switch (a_msg->type) {
            case SKSE::MessagingInterface::kDataLoaded:
                logger::info("[TrueGaze] Game data loaded. Initializing biomechanical gaze hooks...");

                // 1. Install Havok animation update hook
                TrueGaze::Engine::AnimationHook::Install();

                // 2. Register custom conditions with Open Animation Replacer (OAR)
                TrueGaze::Integrations::OarConditions::RegisterWithOar();

                // 3. Start HCEP Desktop Bridge Pipe Listener in background thread
                g_pipeServer = std::make_unique<TrueGaze::Bridge::NamedPipeServer>();
                g_pipeServer->Start();

                logger::info("[TrueGaze] Biological Oculomotor Engine initialized successfully.");
                break;

            case SKSE::MessagingInterface::kPreLoadGame:
            case SKSE::MessagingInterface::kSaveGame:
                break;
        }
    }
#endif
}

#if __has_include(<SKSE/SKSE.h>)
SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    InitializeLogging();
    logger::info("[TrueGaze] Loading True Gaze v1.0.0 (An HCEP Product by Kirk LaSalle)...");

    SKSE::Init(a_skse);

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MessageHandler)) {
        logger::error("[TrueGaze] Failed to register SKSE messaging listener.");
        return false;
    }

    logger::info("[TrueGaze] SKSE plugin loaded successfully.");
    return true;
}
#else
// Standard SKSE64 plugin interface definitions for Skyrim Special Edition (1.5.97)
struct SKSEPluginInfo
{
    enum { kVersion = 1 };
    uint32_t infoVersion{ kVersion };
    const char* name{ "TrueGaze" };
    uint32_t version{ 1 };
};

#pragma pack(push, 1)
// SKSE64 version-independence struct for Skyrim Anniversary Edition (1.6+) and Address Library
struct SKSEPluginVersionData
{
    uint32_t dataVersion{ 1 };
    uint32_t pluginVersion{ 0x01000000 };
    char name[256]{ "TrueGaze" };
    char author[256]{ "Kirk LaSalle" };
    char supportEmail[256]{ "" };
    uint32_t versionIndependence{ 1 }; // 1 = Address Library version independent
    uint32_t compatibleVersions[16]{ 0 };
    uint32_t xseMinimum{ 0 };
};
#pragma pack(pop)

extern "C" __declspec(dllexport) SKSEPluginVersionData SKSEPlugin_Version = {};

struct SKSEInterface
{
    uint32_t skseVersion;
    uint32_t runtimeVersion;
    uint32_t editorVersion;
    uint32_t isEditor;
    void* (*QueryInterface)(uint32_t id);
    uint32_t (*GetPluginHandle)(void);
    uint32_t (*GetReleaseIndex)(void);
    void* (*GetTrampolineInterface)(uint32_t version);
};

extern "C" __declspec(dllexport) bool SKSEPlugin_Query(const SKSEInterface*, SKSEPluginInfo* a_info)
{
    if (a_info) {
        a_info->infoVersion = SKSEPluginInfo::kVersion;
        a_info->name = "TrueGaze";
        a_info->version = 1;
    }
    return true;
}

extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSEInterface*)
{
    InitializeLogging();
    TrueGaze::Engine::ConfigManager::GetSingleton().Load();
    TrueGaze::Engine::AnimationHook::Install();
    TrueGaze::Integrations::OarConditions::RegisterWithOar();

    if (TrueGaze::Engine::ConfigManager::GetSingleton().connectHcepBridge) {
        g_pipeServer = std::make_unique<TrueGaze::Bridge::NamedPipeServer>();
        g_pipeServer->Start();
    }
    return true;
}

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID)
{
    switch (ul_reason_for_call) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            break;
        case DLL_PROCESS_DETACH:
            if (g_pipeServer) {
                g_pipeServer->Stop();
                g_pipeServer.reset();
            }
            break;
    }
    return TRUE;
}
#endif
