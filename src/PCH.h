#pragma once

// Windows headers define min/max as macros, which breaks std::min, std::max,
// std::clamp and any qualification ending in one of those names. NOMINMAX must
// be defined before the first Windows header is pulled in, which is why it lives
// here rather than in the .cpp files.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numbers>
#include <random>
#include <string>
#include <string_view>
#include <vector>

// Logging
//
// The `logger` namespace is SKSE's log wrapper and is only defined when the
// CommonLibSSE SDK headers are on the include path. That is not the case for the
// standalone test targets, so a spdlog-backed fallback is provided below. This
// lets shared translation units — notably NamedPipeServer.cpp, which is compiled
// into both the plugin and the bridge mock — log unconditionally.
#if __has_include(<spdlog/spdlog.h>)
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/spdlog.h>
#else
namespace spdlog
{
    inline void info(std::string_view) {}
    inline void error(std::string_view) {}
    inline void warn(std::string_view) {}
    inline void debug(std::string_view) {}
} // namespace spdlog
#endif

// CommonLibSSE-NG
#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#include <SKSE/SKSE.h>
using namespace std::literals;
namespace logger = SKSE::log;
#else
// Fallback forward-declarations for standalone analysis/testing without full game SDK
namespace RE
{
    class Actor;
    class TESObjectREFR;
    class NiNode;
    class NiAVObject;
    class NiPoint3;
    class NiMatrix3;
} // namespace RE

// Standalone builds log through spdlog so the same source lines compile and run
// outside the game.
namespace logger = spdlog;
#endif
