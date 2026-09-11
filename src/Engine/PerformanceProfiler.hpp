#pragma once

#include <chrono>
#include <atomic>
#include <cstdint>

namespace TrueGaze::Engine {

/// @brief High-precision performance profiler and frame budget monitor.
/// Ensures TrueGaze never consumes more than 0.15ms (< 150 microseconds) of frame time.
class PerformanceProfiler
{
public:
    class ScopedTimer
    {
    public:
        explicit ScopedTimer(std::atomic<uint64_t>& outAccumulatorUs) noexcept
            : _accumulator(outAccumulatorUs)
            , _startTime(std::chrono::steady_clock::now())
        {}

        ~ScopedTimer() noexcept
        {
            auto endTime = std::chrono::steady_clock::now();
            auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - _startTime).count();
            _accumulator.store(static_cast<uint64_t>(durationUs), std::memory_order_relaxed);
        }

    private:
        std::atomic<uint64_t>& _accumulator;
        std::chrono::steady_clock::time_point _startTime;
    };

    static inline std::atomic<uint64_t> s_lastFrameTimeUs{ 0 };
    static inline std::atomic<uint32_t> s_activeActorCount{ 0 };

    /// @brief Checks whether the last frame duration exceeded the 150us safety budget.
    static bool IsWithinBudget() noexcept
    {
        return s_lastFrameTimeUs.load(std::memory_order_relaxed) <= 150;
    }

    /// @brief Returns the last recorded frame processing time in microseconds.
    static uint64_t GetLastFrameTimeUs() noexcept
    {
        return s_lastFrameTimeUs.load(std::memory_order_relaxed);
    }
};

} // namespace TrueGaze::Engine
