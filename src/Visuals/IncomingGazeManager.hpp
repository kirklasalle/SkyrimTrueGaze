#pragma once

#include "PCH.h"
#include "Visuals/IncomingGaze.hpp"
#include "Visuals/VisualTuning.hpp"

namespace TrueGaze::Visuals
{
    class IncomingGazeManager
    {
    public:
        static IncomingGazeManager& Get();
        void InstallUi();
        void BeginFrame(uint64_t frame);
        void RegisterReceiver(RE::Actor* actor);
        void SubmitEyes(RE::Actor* actor, const RE::NiPoint3& origin,
                        const RE::NiPoint3& direction, float range);
        void EndFrame();
        void Reset();
        void AbandonFrame();
        void SetTuning(const VisualTuning& tuning);
        void StepInspection(int direction) noexcept;
        static bool DiagnosticsSuppressed() noexcept;
        const Incoming::Frame& Results() const noexcept;
        struct Cost
        {
            uint64_t finalizationUs{0}, peakFinalizationUs{0};
            uint64_t uiUs{0}, peakUiUs{0};
            size_t panels{0}, labels{0};
            size_t cards{0}, overflowCards{0};
            size_t finalizationSamples{0}, uiSamples{0};
            uint64_t finalizationMedianUs{0}, finalizationP95Us{0}, finalizationWindowPeakUs{0};
            uint64_t uiMedianUs{0}, uiP95Us{0}, uiWindowPeakUs{0};
        };
        Cost GetCost() const noexcept;

    private:
        IncomingGazeManager() = default;
    };
}
