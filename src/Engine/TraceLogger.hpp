#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace TrueGaze::Engine
{

    /// @brief Structured JSONL Trace Logger conforming to
    /// "docs/Trace Log Schema — TrueGaze Gaze Diagnostics.md".
    ///
    /// ## Architecture & Performance Budget
    /// - Emits per-frame gaze events in compact JSON Lines format.
    /// - Low latency: Enqueue overhead <= 50 μs per actor (serialize to pre-allocated buffer).
    /// - I/O decoupled: Dedicated background worker thread flushes to disk.
    /// - Flush cadence: Every 256 events or 1.0 second, whichever occurs first.
    /// - Safety: 100 MB hard cap by default; tracing auto-stops if reached.
    class TraceLogger
    {
    public:
        static TraceLogger& Get() noexcept
        {
            static TraceLogger instance;
            return instance;
        }

        TraceLogger(const TraceLogger&) = delete;
        TraceLogger& operator=(const TraceLogger&) = delete;

        /// @brief Starts tracing to disk. Overwrites existing trace file.
        /// @param customPath Optional custom file path. If empty, uses default SKSE plugins path.
        /// @param actorFilters Optional list of FormIDs to trace. If empty, traces all actors.
        void StartTracing(const std::filesystem::path& customPath = {},
                          const std::vector<uint32_t>& actorFilters = {}) noexcept;

        /// @brief Stops tracing, flushes all pending events, and closes the file.
        void StopTracing() noexcept;

        /// @brief Forces an immediate flush of the memory buffer to disk.
        void Flush() noexcept;

        /// @brief Reconfigures event filtering and buffer sizing from live config.
        void SetConfig(bool traceGazeTick, bool traceSaccades, bool traceRegionChanges,
                       bool traceCGA, bool traceBlinks, bool traceHitMismatches,
                       int bufferSize, int maxFileSizeMb) noexcept;

        [[nodiscard]] bool IsActive() const noexcept
        {
            return _active.load(std::memory_order_relaxed);
        }

        [[nodiscard]] uint64_t TotalEvents() const noexcept
        {
            return _totalEvents.load(std::memory_order_relaxed);
        }

        [[nodiscard]] uint64_t TotalBytesWritten() const noexcept
        {
            return _totalBytesWritten.load(std::memory_order_relaxed);
        }

        [[nodiscard]] size_t PendingBufferSize() const noexcept
        {
            std::lock_guard<std::mutex> lock(_bufferMutex);
            return _buffer.size();
        }

        [[nodiscard]] float DurationSeconds() const noexcept;

        [[nodiscard]] const std::filesystem::path& GetLogFilePath() const noexcept
        {
            return _logPath;
        }

        [[nodiscard]] const std::vector<uint32_t>& GetActorFilters() const noexcept
        {
            return _actorFilters;
        }

        // --- Event Logging API (Schema Conforming) -------------------------------

        /// @brief 3.1 GAZE_TICK — Emitted once per actor per engine tick.
        void LogGazeTick(float ts, uint32_t actorId, uint32_t targetId, float yaw, float pitch,
                         float eyeYaw, float eyePitch, uint8_t region, uint8_t hitRegion,
                         const char* modeStr, int lod, bool mutual, float mutualSec,
                         float headPct, float dt) noexcept;

        /// @brief 3.2 SACCADE_ONSET — Emitted when a new saccade begins.
        void LogSaccadeOnset(float ts, uint32_t actorId, uint8_t fromRegion, uint8_t toVertex,
                             float angularDistance, float expectedDuration) noexcept;

        /// @brief 3.3 SACCADE_COMPLETE — Emitted when the saccade settles.
        void LogSaccadeComplete(float ts, uint32_t actorId, uint8_t landedRegion,
                                float actualDuration, float overshootDeg) noexcept;

        /// @brief 3.4 REGION_CHANGE — Emitted when classified region changes.
        void LogRegionChange(float ts, uint32_t actorId, uint8_t fromRegion, uint8_t toRegion,
                             float dwellSec, const char* fromLabel, const char* toLabel) noexcept;

        /// @brief 3.5 CGA_ENTER — Emitted when Controlled Gaze Aversion begins.
        void LogCgaEnter(float ts, uint32_t actorId, const char* trigger,
                         uint8_t quadrant, const char* quadrantLabel) noexcept;

        /// @brief 3.5 CGA_EXIT — Emitted when Controlled Gaze Aversion ends.
        void LogCgaExit(float ts, uint32_t actorId, uint8_t returnRegion,
                        float aversionDurationSec, const char* trigger) noexcept;

        /// @brief 3.6 BLINK — Emitted at each blink onset.
        void LogBlink(float ts, uint32_t actorId, float durationMs, float intervalSec) noexcept;

        /// @brief 3.7 HIT_MISMATCH — Emitted when hitRegion != classifiedRegion.
        void LogHitMismatch(float ts, uint32_t actorId, uint8_t classified, uint8_t hit,
                            float yaw, float pitch, float hitX, float hitZ,
                            const char* reason) noexcept;

        /// @brief 3.8 CALIBRATION — Emitted by calibration sweeps.
        void LogCalibration(float ts, const char* command, uint8_t region, float inputYaw,
                            float inputPitch, uint8_t classified, uint8_t hit, bool pass) noexcept;

    private:
        TraceLogger();
        ~TraceLogger();

        void WorkerLoop() noexcept;
        void Enqueue(std::string&& jsonLine) noexcept;
        bool ShouldLogActor(uint32_t actorId) const noexcept;

        std::atomic<bool> _active{false};
        std::atomic<bool> _stopping{false};
        std::atomic<bool> _flushRequested{false};
        std::atomic<uint64_t> _totalEvents{0};
        std::atomic<uint64_t> _totalBytesWritten{0};

        std::filesystem::path _logPath{};
        std::ofstream _fileStream{};

        std::chrono::steady_clock::time_point _startTime{};

        // Event filters from config
        bool _traceGazeTick{true};
        bool _traceSaccades{true};
        bool _traceRegionChanges{true};
        bool _traceCGA{true};
        bool _traceBlinks{false};
        bool _traceHitMismatches{true};
        size_t _bufferSize{256};
        uint64_t _maxFileSizeBytes{100ULL * 1024ULL * 1024ULL}; // 100 MB hard cap

        std::vector<uint32_t> _actorFilters{};
        std::unordered_set<uint32_t> _actorFilterSet{};

        mutable std::mutex _bufferMutex{};
        std::condition_variable _cv{};
        std::vector<std::string> _buffer{};
        std::thread _workerThread{};
    };

} // namespace TrueGaze::Engine
