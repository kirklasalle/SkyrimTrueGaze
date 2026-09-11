#pragma once

#include <cmath>
#include <cstdint>
#include <chrono>
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <random>
#include <numbers>

// Logging
#if __has_include(<spdlog/spdlog.h>)
    #include <spdlog/spdlog.h>
    #include <spdlog/sinks/basic_file_sink.h>
#else
    namespace spdlog {
        inline void info(std::string_view) {}
        inline void error(std::string_view) {}
        inline void warn(std::string_view) {}
    }
#endif

// CommonLibSSE-NG
#if __has_include(<RE/Skyrim.h>)
    #include <RE/Skyrim.h>
    #include <SKSE/SKSE.h>
    using namespace std::literals;
    namespace logger = SKSE::log;
#else
    // Fallback forward-declarations for standalone analysis/testing without full game SDK
    namespace RE {
        class Actor;
        class TESObjectREFR;
        class NiNode;
        class NiPoint3;
        class NiMatrix3;
    }
#endif
