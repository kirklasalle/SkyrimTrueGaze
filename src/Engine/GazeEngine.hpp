#pragma once

#include "PCH.h"
#include "GazeTuning.hpp"
#include "ActorGazeRuntime.hpp"
#include "AnimationHook.hpp"
#include "Bridge/NamedPipeServer.hpp"

#include <unordered_map>
#include <chrono>

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

namespace TrueGaze::Engine
{

    /// @brief Owns all gaze simulation state and drives every actor each frame.
    ///
    /// ## Why this exists
    ///
    /// Before this class, the kinematics modules were pure functions with no caller
    /// and no persistent state. `SaccadeGenerator`, `VorCoordinator`, `MicroJitter`,
    /// `SocialTriangle`, and `BoneController` were referenced only by the unit tests,
    /// so no NPC's eyes ever moved regardless of configuration. See
    /// docs/AUDIT_REPORT_2026-09-11.md finding C-1.
    ///
    /// GazeEngine is the missing middle: it holds a per-actor `ActorGazeRuntime`,
    /// snapshots configuration into a `GazeTuning`, runs the kinematics pipeline, and
    /// hands the result to `BoneController` and `EyeAimConstraint`.
    ///
    /// ## Threading
    ///
    /// Game thread only. The actor map and every `ActorGazeRuntime` are touched
    /// exclusively from `TickActor`, which the animation hook calls on the game
    /// thread. The only cross-thread state is `NamedPipeServer`, which is itself
    /// safe by design. Deliberately no lock on the map: adding one would be a
    /// per-actor mutex acquisition in the hottest path for no benefit.
    class GazeEngine
    {
    public:
        static GazeEngine &Get() noexcept
        {
            static GazeEngine instance;
            return instance;
        }

        GazeEngine(const GazeEngine &) = delete;
        GazeEngine &operator=(const GazeEngine &) = delete;

        /// @brief Rebuilds the tuning snapshot from the current configuration.
        /// Called on load and whenever the MCM or INI is reloaded.
        void RefreshTuning() noexcept;

        /// @brief Installs the pipe server, if configuration enables the bridge.
        void StartBridge() noexcept;
        void StopBridge() noexcept;

        /// @brief Simulates and applies gaze for one actor. Game thread only.
        /// @param deltaSeconds Real frame time, used for all integration.
        void TickActor(RE::Actor *actor, float deltaSeconds) noexcept;

        /// @brief Advances per-actor idle timers and evicts long-dormant actors.
        /// Call once per frame from the hook, after all TickActor calls.
        void EndFrame(float deltaSeconds) noexcept;

        /// @brief Restores every modified bone. Call at frame end or on unload.
        void ReleaseBones() noexcept;

        /// @brief Drops all per-actor state. Call on cell change and game load.
        void ResetAll() noexcept;

        // --- Queries used by the API, OAR conditions, and Papyrus ----------------

        /// @brief Returns the live simulation state for an actor, or nullptr.
        [[nodiscard]] ActorGazeRuntime *FindActor(uint32_t formId) noexcept;

        /// @brief Number of actors currently simulated. Diagnostic.
        [[nodiscard]] size_t TrackedActorCount() const noexcept { return _actors.size(); }

        /// @brief Human-readable name of an HCEP mode id. Diagnostic.
        [[nodiscard]] static const char *ModeName(uint8_t mode) noexcept;

        /// @brief Human-readable name of a gaze region id. Diagnostic.
        [[nodiscard]] static const char *RegionName(uint8_t region) noexcept;

        [[nodiscard]] const Bridge::NamedPipeServer &Pipe() const noexcept { return _pipe; }
        [[nodiscard]] Bridge::NamedPipeServer &Pipe() noexcept { return _pipe; }

        [[nodiscard]] const GazeTuning &Tuning() const noexcept { return _tuning; }

        [[nodiscard]] bool IsBridgeConnected() const noexcept { return _pipe.IsConnected(); }

        /// Per-frame timings, in microseconds. Diagnostic.
        [[nodiscard]] uint64_t LastFrameMicros() const noexcept { return _lastFrameUs; }
        [[nodiscard]] uint64_t PeakFrameMicros() const noexcept { return _peakFrameUs; }

    private:
        GazeEngine() = default;

        /// Compute the gaze deflection for an actor, in actor-relative degrees.
        void ComputeDeflection(RE::Actor *actor, ActorGazeRuntime &state,
                               float deltaSeconds, float &outYaw, float &outPitch) noexcept;

        /// Distance from the player, in metres. Zero when unavailable.
        [[nodiscard]] static float DistanceMetersForTier(RE::Actor *actor) noexcept;

        /// Apply a computed deflection to the actor's bone chain.
        ///
        /// Takes a mutable state because it records whether the skeleton probe has
        /// already been logged for this actor (see ActorGazeRuntime::bonesReported).
        void ApplyToSkeleton(RE::Actor *actor, ActorGazeRuntime &state,
                             float yawDeg, float pitchDeg) noexcept;

        /// Publish actor state to the OAR condition cache.
        void PublishState(RE::Actor *actor, const ActorGazeRuntime &state) noexcept;

        std::unordered_map<uint32_t, ActorGazeRuntime> _actors;
        Engine::GazeTuning _tuning{};
        Bridge::NamedPipeServer _pipe{};

        bool _bridgeStarted{false};

        // Diagnostics
        uint64_t _lastFrameUs{0};
        uint64_t _peakFrameUs{0};
        std::chrono::steady_clock::time_point _frameStart{};
        bool _frameOpen{false};

        /// Actors unseen for longer than this are evicted to bound memory (NFR-3).
        static constexpr float ACTOR_EVICTION_SEC = 30.0f;

        /// Hard cap on tracked actors. Guards against pathological cell sizes.
        static constexpr size_t MAX_TRACKED_ACTORS = 512;
    };

} // namespace TrueGaze::Engine
