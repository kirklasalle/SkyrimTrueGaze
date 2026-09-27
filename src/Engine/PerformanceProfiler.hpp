#pragma once

#include <chrono>
#include <atomic>
#include <cstdint>

namespace TrueGaze::Engine
{

    /// @brief High-precision performance profiler and frame budget monitor.
    ///
    /// ## R14 E2.1 — made real
    ///
    /// Before this pass the profiler was DEAD CODE (included by GazeEngine.cpp but
    /// never instantiated) and its ScopedTimer used `.store()`, so only the last
    /// measurement in a scope survived — audit findings Q-4 / P2-1. The documented
    /// 150 µs budget was checked nowhere: the actual EndFrame warning threshold was
    /// 1,500 µs, a 10x disagreement.
    ///
    /// This version:
    ///
    ///   * accumulates with `fetch_add` (what "profiler" always implied),
    ///   * records last + peak frame cost with a CAS max,
    ///   * exposes the budget as ONE named constant (kFrameBudgetUs) consumed by
    ///     the EndFrame check and stgstatus, so the constant, the code, and the
    ///     docs can never drift apart again.
    ///
    /// The budget is a target pending measurement (E2.4 benchmark artifacts will
    /// either validate it or re-baseline it — the constant is the single point to
    /// update when that happens).
    class PerformanceProfiler
    {
    public:
        /// The documented frame budget (PRD NFR-1: < 0.15 ms per frame).
        static constexpr uint64_t kFrameBudgetUs = 150;

        class ScopedTimer
        {
        public:
            explicit ScopedTimer(std::atomic<uint64_t> &outAccumulatorUs) noexcept
                : _accumulator(outAccumulatorUs), _startTime(std::chrono::steady_clock::now())
            {
            }

            ~ScopedTimer() noexcept
            {
                auto endTime = std::chrono::steady_clock::now();
                auto durationUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime - _startTime).count();
                // ACCUMULATE, not overwrite (the original .store() here kept only
                // the last measurement — the audit's P2-1 accumulation bug).
                _accumulator.fetch_add(static_cast<uint64_t>(durationUs), std::memory_order_relaxed);
            }

        private:
            std::atomic<uint64_t> &_accumulator;
            std::chrono::steady_clock::time_point _startTime;
        };

        static inline std::atomic<uint64_t> s_lastFrameTimeUs{0};
        static inline std::atomic<uint32_t> s_activeActorCount{0};

        /// @brief Records one frame measurement (game thread). Tracks last + peak.
        static void RecordFrame(uint64_t durationUs) noexcept
        {
            s_lastFrameTimeUs.store(durationUs, std::memory_order_relaxed);
            uint64_t prev = s_peakFrameUs.load(std::memory_order_relaxed);
            while (durationUs > prev &&
                   !s_peakFrameUs.compare_exchange_weak(prev, durationUs, std::memory_order_relaxed))
            {
                // compare_exchange_weak reloaded prev on failure; loop exits once
                // the stored peak is >= this measurement.
            }
        }

        /// @brief Checks whether the last frame duration exceeded the budget.
        static bool IsWithinBudget() noexcept
        {
            return s_lastFrameTimeUs.load(std::memory_order_relaxed) <= kFrameBudgetUs;
        }

        /// @brief Returns the last recorded frame processing time in microseconds.
        static uint64_t GetLastFrameTimeUs() noexcept
        {
            return s_lastFrameTimeUs.load(std::memory_order_relaxed);
        }

        /// @brief Returns the peak recorded frame processing time in microseconds.
        static uint64_t GetPeakFrameTimeUs() noexcept
        {
            return s_peakFrameUs.load(std::memory_order_relaxed);
        }

        /// @brief The budget constant, so console diagnostics and docs agree.
        static constexpr uint64_t BudgetMicros() noexcept { return kFrameBudgetUs; }

    private:
        static inline std::atomic<uint64_t> s_peakFrameUs{0};
    };

} // namespace TrueGaze::Engine
