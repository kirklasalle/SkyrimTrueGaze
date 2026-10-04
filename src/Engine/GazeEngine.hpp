#pragma once

#include "ActorGazeRuntime.hpp"
#include "AnimationHook.hpp"
#include "Bridge/NamedPipeServer.hpp"
#include "GazeTuning.hpp"
#include "PCH.h"

#include <chrono>
#include <unordered_map>

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

namespace TrueGaze::Engine
{

    /// @brief Owns all gaze kinematics state and drives every actor each frame.
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
        static GazeEngine& Get() noexcept
        {
            static GazeEngine instance;
            return instance;
        }

        GazeEngine(const GazeEngine&) = delete;
        GazeEngine& operator=(const GazeEngine&) = delete;

        /// @brief Rebuilds the tuning snapshot from the current configuration.
        /// Called on load and whenever the INI is reloaded.
        void RefreshTuning() noexcept;

        /// @brief Installs the pipe server, if configuration enables the bridge.
        void StartBridge() noexcept;

        /// @brief Stops the bridge. policy=Join for graceful session teardown;
        /// Abandon for process exit (no join, no destruction — see NamedPipeServer).
        void StopBridge(Bridge::NamedPipeServer::JoinPolicy policy =
                            Bridge::NamedPipeServer::JoinPolicy::Join) noexcept;

        /// @brief Evaluates and applies biological kinematics for one actor. Game thread only.
        /// @param deltaSeconds Real frame time, used for all integration.
        void TickActor(RE::Actor* actor, float deltaSeconds) noexcept;

        /// @brief Advances per-actor idle timers and evicts long-dormant actors.
        /// Call once per frame from the hook, after all TickActor calls.
        void EndFrame(float deltaSeconds) noexcept;

        /// @brief Restores every modified bone. Call at frame end or on unload.
        void ReleaseBones() noexcept;

        /// @brief Drops all per-actor state. Call on cell change and game load.
        void ResetAll() noexcept;

        // --- Queries used by the API and OAR conditions ---------------------------

        /// @brief Returns the live kinematics state for an actor, or nullptr.
        [[nodiscard]] ActorGazeRuntime* FindActor(uint32_t formId) noexcept;

        /// @brief Number of actors currently tracked. Diagnostic.
        [[nodiscard]] size_t TrackedActorCount() const noexcept { return _actors.size(); }

        /// @brief Human-readable name of an HCEP mode id. Diagnostic.
        [[nodiscard]] static const char* ModeName(uint8_t mode) noexcept;

        /// @brief Human-readable name of a gaze region id. Diagnostic.
        [[nodiscard]] static const char* RegionName(uint8_t region) noexcept;

        [[nodiscard]] const Bridge::NamedPipeServer& Pipe() const noexcept { return _pipe; }
        [[nodiscard]] Bridge::NamedPipeServer& Pipe() noexcept { return _pipe; }

        /// @brief Monotonically increasing frame index.
        [[nodiscard]] uint64_t GetFrameCounter() const noexcept { return _frameCounter; }
        void AdvanceFrameCounter() noexcept { ++_frameCounter; }

        [[nodiscard]] const GazeTuning& Tuning() const noexcept { return _tuning; }

        [[nodiscard]] bool IsBridgeConnected() const noexcept { return _pipe.IsConnected(); }

        /// Per-frame timings, in microseconds. Diagnostic.
        [[nodiscard]] uint64_t LastFrameMicros() const noexcept { return _lastFrameUs; }
        [[nodiscard]] uint64_t PeakFrameMicros() const noexcept { return _peakFrameUs; }

        /// Distance from the player, in metres. Zero when unavailable.
        /// Public: the TrueGaze API derives its LOD tier from the same number
        /// the runtime uses, so the two can never disagree (R14 E7.7).
        [[nodiscard]] static float DistanceMetersForTier(RE::Actor* actor) noexcept;

        /// Relationship rank toward the player, mapped from Skyrim's
        /// RELATIONSHIP_LEVEL (0=Lover..8=Archnemesis) onto the feedback
        /// packet's documented -4..+4 axis. 0 when no relationship exists.
        /// R14 E7.6.
        [[nodiscard]] static int16_t RelationshipRankForActor(RE::Actor* actor) noexcept;

        /// R15: resolve the INI-configured multiplier bundle for a gaze
        /// category. When category profiles are disabled (or the category is
        /// neutral), returns an all-neutral bundle = exact pre-R15 parity.
        /// Single source: ConfigManager is the only bundle authority.
        [[nodiscard]] static CharacterProfile::CategoryProfileBundle
        CategoryBundleFor(CharacterProfile::GazeCategory category) noexcept;

        /// Runtime counters used to distinguish a missing actor hook from an
        /// eligibility/LOD/target-selection issue. These are diagnostic only.
        [[nodiscard]] uint64_t TickCalls() const noexcept { return _tickCalls; }
        [[nodiscard]] uint64_t EligibleTicks() const noexcept { return _eligibleTicks; }
        [[nodiscard]] uint64_t CulledTicks() const noexcept { return _culledTicks; }
        [[nodiscard]] uint64_t TargetResolutions() const noexcept { return _targetResolutions; }
        [[nodiscard]] uint64_t NoTargetResolutions() const noexcept { return _noTargetResolutions; }
        [[nodiscard]] uint8_t LastTargetPriority() const noexcept { return _lastTargetPriority; }
        [[nodiscard]] uint32_t LastTargetFormId() const noexcept { return _lastTargetFormId; }
        [[nodiscard]] uint64_t SaccadesTriggered() const noexcept { return _saccadesTriggered; }
        [[nodiscard]] uint64_t BlinksTriggered() const noexcept { return _blinksTriggered; }
        [[nodiscard]] uint64_t MutualGazeFrames() const noexcept { return _mutualGazeFrames; }

        /// Phase S2 rig capability matrix. Diagnostic only.
        /// The last probed actor's visual-origin mode as a stable string, plus
        /// running counts of the two distinct rig-resolution outcomes.
        [[nodiscard]] const char* LastRigOrigin() const noexcept { return _lastRigOrigin; }
        [[nodiscard]] uint64_t EyeNodeAbsentCount() const noexcept { return _eyeNodeAbsentCount; }
        [[nodiscard]] uint64_t HeadAnchorAbsentCount() const noexcept
        {
            return _headAnchorAbsentCount;
        }

        /// GOLD STANDARD scene-defer diagnostics. The defer contract (yield the
        /// head chain to vanilla direction during directed scenes; eyes never
        /// yield) is the core of flawless scene integration. These expose it.
        [[nodiscard]] bool LastDeferActive() const noexcept { return _lastDeferActive; }
        [[nodiscard]] uint64_t DeferFrames() const noexcept { return _deferFrames; }

        /// Records the outcome of one skeleton probe. Called from ApplyToSkeleton.
        void RecordRigProbe(const char* originMode, bool headResolved,
                            bool eyeNodeResolved) noexcept
        {
            _lastRigOrigin = originMode;
            if (!headResolved)
            {
                ++_headAnchorAbsentCount;
            }
            else if (!eyeNodeResolved)
            {
                ++_eyeNodeAbsentCount;
            }
        }

        // --- Calibration Mode (A8 / Technical Design §8) ---
        struct CalibrationState
        {
            bool active{false};
            bool sweepActive{false};
            float overrideYawDeg{0.0f};
            float overridePitchDeg{0.0f};
            uint32_t targetActorFormId{0}; // 0 = all actors
            int currentRegionIndex{0};
            float sweepTimerSec{0.0f};
            uint32_t sweepSettledFrames{0};
            uint32_t sweepAgreedFrames{0};
            uint32_t sweepMismatchFrames{0};
        };

        void SetCalibrationOverride(bool active, float yawDeg, float pitchDeg,
                                    uint32_t targetActorFormId = 0) noexcept;
        void StartCalibrationSweep(uint32_t targetActorFormId = 0) noexcept;
        void StepCalibrationNext(uint32_t targetActorFormId = 0) noexcept;
        void StopCalibration() noexcept;
        [[nodiscard]] const CalibrationState& GetCalibrationState() const noexcept { return _calState; }

    private:
        GazeEngine() = default;

        /// Compute the gaze deflection for an actor, in actor-relative degrees.
        /// speedScale: the calm/combat speed multiplier decided in TickActor
        /// (0.5 calm baseline, 1.0 combat). actorInCombat: the same predicate,
        /// passed so the catch-up-saccade threshold can switch with it.
        void ComputeDeflection(RE::Actor* actor, ActorGazeRuntime& state, float deltaSeconds,
                               float& outYaw, float& outPitch, float speedScale,
                               bool actorInCombat) noexcept;

        /// Resolve and cache skeleton bone pointers on the actor's 3D scene graph.
        void EnsureSkeletonResolved(RE::Actor* actor, ActorGazeRuntime& state) noexcept;

        /// Apply a computed deflection to the actor's bone chain.
        void ApplyToSkeleton(RE::Actor* actor, ActorGazeRuntime& state, float yawDeg,
                             float pitchDeg, float deltaSeconds) noexcept;

        /// Publish actor state to the OAR condition cache.
        void PublishState(RE::Actor* actor, const ActorGazeRuntime& state) noexcept;

        std::unordered_map<uint32_t, ActorGazeRuntime> _actors;
        Engine::GazeTuning _tuning{};
        Bridge::NamedPipeServer _pipe{};

        // Character Gaze Profile frame-scope values (set in ComputeDeflection,
        // consumed by the scanpath section in the same call). Not thread state —
        // purely per-invocation locals passed through members to keep the
        // ComputeDeflection signature stable.
        // R14 E7.8 — grouped into one struct so the frame-scope scratch state is
        // a single named unit instead of five loose members.
        struct FrameScope
        {
            float fixationScale{1.0f};
            bool triangleEnabled{true};
            const float* vertexWeights{nullptr};
            /// Per-frame uniform roll [0,1) used to gate CGA engagement by the
            /// character profile's aversion rate. -1 = always engage (no profile).
            float cgaRoll{-1.0f};
        };
        FrameScope _frame{};

        bool _bridgeStarted{false};

        // Diagnostics
        uint64_t _frameCounter{1};
        uint64_t _lastFrameUs{0};
        uint64_t _peakFrameUs{0};
        std::chrono::steady_clock::time_point _frameStart{};
        bool _frameOpen{false};

        float _gameTimeSec{0.0f};

        uint64_t _tickCalls{0};
        uint64_t _eligibleTicks{0};
        uint64_t _culledTicks{0};
        uint64_t _targetResolutions{0};
        uint64_t _noTargetResolutions{0};
        uint8_t _lastTargetPriority{0};
        uint32_t _lastTargetFormId{0};
        uint64_t _saccadesTriggered{0};
        uint64_t _blinksTriggered{0};
        uint64_t _mutualGazeFrames{0};

        // Phase S2 rig capability diagnostics.
        const char* _lastRigOrigin{"unknown"};
        uint64_t _eyeNodeAbsentCount{0};
        uint64_t _headAnchorAbsentCount{0};

        // Gold Standard scene-defer diagnostics (written in TickActor after
        // target resolution, read by stgstatus).
        bool _lastDeferActive{false};
        uint64_t _deferFrames{0};

        CalibrationState _calState{};

        /// Actors unseen for longer than this are evicted to bound memory (NFR-3).
        static constexpr float ACTOR_EVICTION_SEC = 30.0f;

        /// Hard cap on tracked actors. Guards against pathological cell sizes.
        static constexpr size_t MAX_TRACKED_ACTORS = 512;
    };

} // namespace TrueGaze::Engine
