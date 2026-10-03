#include "Engine/TraceLogger.hpp"

#include <algorithm>
#include <cstdio>
#include <format>
#include <iostream>

#if __has_include(<SKSE/SKSE.h>)
#    include <SKSE/SKSE.h>
#endif

#if __has_include(<spdlog/spdlog.h>)
#    include <spdlog/spdlog.h>
#endif

namespace TrueGaze::Engine
{

    TraceLogger::TraceLogger() = default;

    TraceLogger::~TraceLogger()
    {
        StopTracing();
    }

    void TraceLogger::SetConfig(bool traceGazeTick, bool traceSaccades, bool traceRegionChanges,
                                bool traceCGA, bool traceBlinks, bool traceHitMismatches,
                                int bufferSize, int maxFileSizeMb) noexcept
    {
        _traceGazeTick = traceGazeTick;
        _traceSaccades = traceSaccades;
        _traceRegionChanges = traceRegionChanges;
        _traceCGA = traceCGA;
        _traceBlinks = traceBlinks;
        _traceHitMismatches = traceHitMismatches;
        _bufferSize = static_cast<size_t>(std::clamp(bufferSize, 16, 4096));
        _maxFileSizeBytes = static_cast<uint64_t>(std::clamp(maxFileSizeMb, 1, 1024)) * 1024ULL * 1024ULL;
    }

    void TraceLogger::StartTracing(const std::filesystem::path& customPath,
                                  const std::vector<uint32_t>& actorFilters) noexcept
    {
        if (_active.load(std::memory_order_relaxed))
        {
            StopTracing();
        }

        _actorFilters = actorFilters;
        _actorFilterSet.clear();
        for (auto id : actorFilters)
        {
            _actorFilterSet.insert(id);
        }

        if (!customPath.empty())
        {
            _logPath = customPath;
        }
        else
        {
#if __has_include(<SKSE/SKSE.h>)
            auto skseLogDir = SKSE::log::log_directory();
            if (skseLogDir.has_value())
            {
                _logPath = *skseLogDir / "TrueGaze_GazeTrace.jsonl";
            }
            else
            {
                _logPath = "Data/SKSE/Plugins/TrueGaze_GazeTrace.jsonl";
            }
#else
            _logPath = "TrueGaze_GazeTrace.jsonl";
#endif
        }

        try
        {
            std::error_code ec;
            auto parentDir = _logPath.parent_path();
            if (!parentDir.empty() && !std::filesystem::exists(parentDir, ec))
            {
                std::filesystem::create_directories(parentDir, ec);
            }

            _fileStream.open(_logPath, std::ios::out | std::ios::trunc);
            if (!_fileStream.is_open())
            {
#if __has_include(<spdlog/spdlog.h>)
                logger::error("[TrueGaze] Failed to open trace log file for writing: '{}'",
                              _logPath.string());
#endif
                return;
            }
        }
        catch (const std::exception&
#if __has_include(<spdlog/spdlog.h>)
                   e
#endif
        )
        {
#if __has_include(<spdlog/spdlog.h>)
            logger::error("[TrueGaze] Exception opening trace log: {}", e.what());
#endif
            return;
        }

        _totalEvents.store(0, std::memory_order_relaxed);
        _totalBytesWritten.store(0, std::memory_order_relaxed);
        _startTime = std::chrono::steady_clock::now();
        _stopping.store(false, std::memory_order_relaxed);
        _flushRequested.store(false, std::memory_order_relaxed);

        {
            std::lock_guard<std::mutex> lock(_bufferMutex);
            _buffer.clear();
            _buffer.reserve(_bufferSize * 2);
        }

        _active.store(true, std::memory_order_release);
        _workerThread = std::thread(&TraceLogger::WorkerLoop, this);

#if __has_include(<spdlog/spdlog.h>)
        logger::info("[TrueGaze] Trace logging STARTED -> '{}' (filtering {} actors, cap={} MB).",
                     _logPath.string(), _actorFilters.empty() ? 0 : _actorFilters.size(),
                     _maxFileSizeBytes / (1024 * 1024));
#endif
    }

    void TraceLogger::StopTracing() noexcept
    {
        if (!_active.exchange(false, std::memory_order_acq_rel))
        {
            return;
        }

        _stopping.store(true, std::memory_order_release);
        _cv.notify_all();

        if (_workerThread.joinable())
        {
            _workerThread.join();
        }

        if (_fileStream.is_open())
        {
            _fileStream.flush();
            _fileStream.close();
        }

#if __has_include(<spdlog/spdlog.h>)
        logger::info("[TrueGaze] Trace logging STOPPED. {} events written ({:.2f} MB) to '{}'.",
                     _totalEvents.load(std::memory_order_relaxed),
                     static_cast<double>(_totalBytesWritten.load(std::memory_order_relaxed)) /
                         (1024.0 * 1024.0),
                     _logPath.string());
#endif
    }

    void TraceLogger::Flush() noexcept
    {
        if (!_active.load(std::memory_order_relaxed))
        {
            return;
        }

        _flushRequested.store(true, std::memory_order_release);
        _cv.notify_all();
    }

    float TraceLogger::DurationSeconds() const noexcept
    {
        if (!_active.load(std::memory_order_relaxed))
        {
            return 0.0f;
        }
        const auto now = std::chrono::steady_clock::now();
        const std::chrono::duration<float> elapsed = now - _startTime;
        return elapsed.count();
    }

    bool TraceLogger::ShouldLogActor(uint32_t actorId) const noexcept
    {
        if (_actorFilterSet.empty())
        {
            return true;
        }
        return _actorFilterSet.find(actorId) != _actorFilterSet.end();
    }

    void TraceLogger::Enqueue(std::string&& jsonLine) noexcept
    {
        if (!_active.load(std::memory_order_relaxed))
        {
            return;
        }

        if (_totalBytesWritten.load(std::memory_order_relaxed) >= _maxFileSizeBytes)
        {
            // Auto-stop at hard cap (Schema 2.0 § 2)
            _active.store(false, std::memory_order_release);
#if __has_include(<spdlog/spdlog.h>)
            logger::warn("[TrueGaze] Trace log reached 100 MB hard cap. Auto-stopping trace.");
#endif
            _cv.notify_all();
            return;
        }

        {
            std::lock_guard<std::mutex> lock(_bufferMutex);
            _buffer.push_back(std::move(jsonLine));
            if (_buffer.size() >= _bufferSize)
            {
                _cv.notify_one();
            }
        }
    }

    void TraceLogger::WorkerLoop() noexcept
    {
        std::vector<std::string> localBatch;
        localBatch.reserve(_bufferSize * 2);

        while (!_stopping.load(std::memory_order_relaxed))
        {
            {
                std::unique_lock<std::mutex> lock(_bufferMutex);
                _cv.wait_for(lock, std::chrono::seconds(1),
                             [this]() {
                                 return _stopping.load(std::memory_order_relaxed) ||
                                        _flushRequested.load(std::memory_order_relaxed) ||
                                        _buffer.size() >= _bufferSize;
                             });

                if (!_buffer.empty())
                {
                    localBatch.swap(_buffer);
                }
                _flushRequested.store(false, std::memory_order_release);
            }

            if (!localBatch.empty() && _fileStream.is_open())
            {
                uint64_t bytesWrittenThisBatch = 0;
                for (const auto& line : localBatch)
                {
                    _fileStream << line << '\n';
                    bytesWrittenThisBatch += line.size() + 1;
                }
                _fileStream.flush();

                _totalEvents.fetch_add(localBatch.size(), std::memory_order_relaxed);
                const uint64_t totalBytes =
                    _totalBytesWritten.fetch_add(bytesWrittenThisBatch, std::memory_order_relaxed) +
                    bytesWrittenThisBatch;

                localBatch.clear();

                if (totalBytes >= _maxFileSizeBytes)
                {
                    _active.store(false, std::memory_order_release);
#if __has_include(<spdlog/spdlog.h>)
                    logger::warn("[TrueGaze] Trace log reached {} MB hard cap. Auto-stopped.",
                                 _maxFileSizeBytes / (1024 * 1024));
#endif
                    break;
                }
            }
        }

        // Drain any leftover events on shutdown
        {
            std::lock_guard<std::mutex> lock(_bufferMutex);
            if (!_buffer.empty() && _fileStream.is_open())
            {
                for (const auto& line : _buffer)
                {
                    _fileStream << line << '\n';
                    _totalBytesWritten.fetch_add(line.size() + 1, std::memory_order_relaxed);
                }
                _totalEvents.fetch_add(_buffer.size(), std::memory_order_relaxed);
                _buffer.clear();
                _fileStream.flush();
            }
        }
    }

    // --- Schema Event Implementations ---------------------------------------

    void TraceLogger::LogGazeTick(float ts, uint32_t actorId, uint32_t targetId, float yaw,
                                 float pitch, float eyeYaw, float eyePitch, uint8_t region,
                                 uint8_t hitRegion, const char* modeStr, int lod, bool mutual,
                                 float mutualSec, float headPct, float dt) noexcept
    {
        if (!_active.load(std::memory_order_relaxed) || !_traceGazeTick || !ShouldLogActor(actorId))
        {
            return;
        }

        const std::string targetStr =
            (targetId != 0) ? std::format("0x{:08X}", targetId) : "null";

        std::string line = std::format(
            R"({{"t":"GAZE_TICK","ts":{:.3f},"actor":"0x{:08X}","target":"{}","yaw":{:.2f},"pitch":{:.2f},"eyeYaw":{:.2f},"eyePitch":{:.2f},"region":{},"hitRegion":{},"mode":"{}","lod":{},"mutual":{},"mutualSec":{:.2f},"headPct":{:.2f},"dt":{:.4f}}})",
            ts, actorId, targetStr, yaw, pitch, eyeYaw, eyePitch, region, hitRegion,
            (modeStr ? modeStr : "UNKNOWN"), lod, mutual ? "true" : "false", mutualSec, headPct, dt);

        Enqueue(std::move(line));
    }

    void TraceLogger::LogSaccadeOnset(float ts, uint32_t actorId, uint8_t fromRegion,
                                     uint8_t toVertex, float angularDistance,
                                     float expectedDuration) noexcept
    {
        if (!_active.load(std::memory_order_relaxed) || !_traceSaccades || !ShouldLogActor(actorId))
        {
            return;
        }

        std::string line = std::format(
            R"({{"t":"SACCADE_ONSET","ts":{:.3f},"actor":"0x{:08X}","fromRegion":{},"toVertex":{},"angularDistance":{:.2f},"expectedDuration":{:.3f}}})",
            ts, actorId, fromRegion, toVertex, angularDistance, expectedDuration);

        Enqueue(std::move(line));
    }

    void TraceLogger::LogSaccadeComplete(float ts, uint32_t actorId, uint8_t landedRegion,
                                        float actualDuration, float overshootDeg) noexcept
    {
        if (!_active.load(std::memory_order_relaxed) || !_traceSaccades || !ShouldLogActor(actorId))
        {
            return;
        }

        std::string line = std::format(
            R"({{"t":"SACCADE_COMPLETE","ts":{:.3f},"actor":"0x{:08X}","landedRegion":{},"actualDuration":{:.3f},"overshootDeg":{:.2f}}})",
            ts, actorId, landedRegion, actualDuration, overshootDeg);

        Enqueue(std::move(line));
    }

    void TraceLogger::LogRegionChange(float ts, uint32_t actorId, uint8_t fromRegion,
                                      uint8_t toRegion, float dwellSec, const char* fromLabel,
                                      const char* toLabel) noexcept
    {
        if (!_active.load(std::memory_order_relaxed) || !_traceRegionChanges ||
            !ShouldLogActor(actorId))
        {
            return;
        }

        std::string line = std::format(
            R"({{"t":"REGION_CHANGE","ts":{:.3f},"actor":"0x{:08X}","from":{},"to":{},"dwellSec":{:.2f},"fromLabel":"{}","toLabel":"{}"}})",
            ts, actorId, fromRegion, toRegion, dwellSec, (fromLabel ? fromLabel : "Unknown"),
            (toLabel ? toLabel : "Unknown"));

        Enqueue(std::move(line));
    }

    void TraceLogger::LogCgaEnter(float ts, uint32_t actorId, const char* trigger, uint8_t quadrant,
                                 const char* quadrantLabel) noexcept
    {
        if (!_active.load(std::memory_order_relaxed) || !_traceCGA || !ShouldLogActor(actorId))
        {
            return;
        }

        std::string line = std::format(
            R"({{"t":"CGA_ENTER","ts":{:.3f},"actor":"0x{:08X}","trigger":"{}","quadrant":{},"quadrantLabel":"{}"}})",
            ts, actorId, (trigger ? trigger : "UNKNOWN"), quadrant,
            (quadrantLabel ? quadrantLabel : "Unknown"));

        Enqueue(std::move(line));
    }

    void TraceLogger::LogCgaExit(float ts, uint32_t actorId, uint8_t returnRegion,
                                float aversionDurationSec, const char* trigger) noexcept
    {
        if (!_active.load(std::memory_order_relaxed) || !_traceCGA || !ShouldLogActor(actorId))
        {
            return;
        }

        std::string line = std::format(
            R"({{"t":"CGA_EXIT","ts":{:.3f},"actor":"0x{:08X}","returnRegion":{},"aversionDurationSec":{:.2f},"trigger":"{}"}})",
            ts, actorId, returnRegion, aversionDurationSec, (trigger ? trigger : "UNKNOWN"));

        Enqueue(std::move(line));
    }

    void TraceLogger::LogBlink(float ts, uint32_t actorId, float durationMs,
                               float intervalSec) noexcept
    {
        if (!_active.load(std::memory_order_relaxed) || !_traceBlinks || !ShouldLogActor(actorId))
        {
            return;
        }

        std::string line = std::format(
            R"({{"t":"BLINK","ts":{:.3f},"actor":"0x{:08X}","durationMs":{:.0f},"intervalSec":{:.2f}}})",
            ts, actorId, durationMs, intervalSec);

        Enqueue(std::move(line));
    }

    void TraceLogger::LogHitMismatch(float ts, uint32_t actorId, uint8_t classified, uint8_t hit,
                                     float yaw, float pitch, float hitX, float hitZ,
                                     const char* reason) noexcept
    {
        if (!_active.load(std::memory_order_relaxed) || !_traceHitMismatches ||
            !ShouldLogActor(actorId))
        {
            return;
        }

        std::string line = std::format(
            R"({{"t":"HIT_MISMATCH","ts":{:.3f},"actor":"0x{:08X}","classified":{},"hit":{},"yaw":{:.1f},"pitch":{:.1f},"hitX":{:.2f},"hitZ":{:.2f},"reason":"{}"}})",
            ts, actorId, classified, hit, yaw, pitch, hitX, hitZ,
            (reason ? reason : "BOUNDARY"));

        Enqueue(std::move(line));
    }

    void TraceLogger::LogCalibration(float ts, const char* command, uint8_t region, float inputYaw,
                                     float inputPitch, uint8_t classified, uint8_t hit,
                                     bool pass) noexcept
    {
        if (!_active.load(std::memory_order_relaxed))
        {
            return;
        }

        std::string line = std::format(
            R"({{"t":"CALIBRATION","ts":{:.3f},"command":"{}","region":{},"inputYaw":{:.1f},"inputPitch":{:.1f},"classified":{},"hit":{},"pass":{}}})",
            ts, (command ? command : "sweep"), region, inputYaw, inputPitch, classified, hit,
            pass ? "true" : "false");

        Enqueue(std::move(line));
    }

} // namespace TrueGaze::Engine
