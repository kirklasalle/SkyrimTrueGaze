#pragma once

#include "PCH.h"
#include "Kinematics/SaccadeGenerator.hpp"
#include "Kinematics/VorCoordinator.hpp"
#include "Kinematics/MicroJitter.hpp"
#include "Kinematics/SocialTriangle.hpp"
#include "Integrations/EfmBlinkController.hpp"

namespace TrueGaze::Engine
{

    /// @brief Per-actor simulation state, persisted across frames.
    ///
    /// Every kinematics module is a pure function over a state struct. Something has
    /// to own those structs for the lifetime of an actor. Before this existed, no
    /// code held them at all — the modules were only ever exercised by the unit
    /// tests, which is why the engine was inert. See
    /// docs/AUDIT_REPORT_2026-09-11.md finding C-1.
    struct ActorGazeRuntime
    {
        Kinematics::SaccadeGenerator::SaccadeState saccade{};
        Kinematics::VorCoordinator::VorState vor{};
        Kinematics::MicroJitter::JitterState jitter{};
        Kinematics::SocialTriangle::TriangleState triangle{};
        Integrations::EfmBlinkController::BlinkState blink{};

        /// HCEP cognitive mode (0=LOGIC .. 4=THINK).
        uint8_t hcepMode{0};

        /// Classified gaze region (0..12), published to OAR.
        uint8_t gazeRegion{0};

        /// The salience target the current saccade was committed to. A change
        /// here is what triggers a new ballistic saccade.
        uint32_t trackedTargetFormId{0};

        /// Continuous seconds of mutual eye contact with the player.
        float mutualGazeHoldSec{0.0f};

        /// Last computed gaze deflection, for the debug visualiser and the API.
        float lastYawDeg{0.0f};
        float lastPitchDeg{0.0f};

        /// True while the eyes are at their limit and cannot reach the target.
        bool eyeSaturated{false};

        /// Wall-clock seconds since this actor was last simulated. Used for eviction.
        float idleSec{0.0f};

        /// True once the actor has been simulated at least once.
        bool initialised{false};

        /// True once this actor's skeleton has been probed and the result logged.
        /// Bone names are matched by string and were never confirmed against a real
        /// rig, so a silent miss is the most likely failure mode. Logging the first
        /// probe per actor turns that into an explicit, readable answer.
        bool bonesReported{false};

        /// Seed for this actor's jitter RNG. Derived from the FormID so that two
        /// actors never share a drift sequence, and so a given actor is stable
        /// across sessions. See docs/AUDIT_REPORT_2026-09-11.md section 5.3.
        uint32_t rngSeed{0};

        /// Timer for API-directed mode overrides (seconds remaining).
        float modeOverrideTimerSec{0.0f};
        bool hasModeOverride{false};

        /// Timer for throttled 3D gaze ray diagnostic logging.
        float rayDebugTimerSec{0.0f};

        /// Monotonic engine frame number when this actor was last simulated.
        /// Prevents double-ticking within a single render frame.
        uint64_t lastFrameTicked{0};

        /// Seconds the actor has continuously fixated on the current target.
        /// Enforces human gaze dwell time (hysteresis) to eliminate target flapping.
        float fixationHoldSec{0.0f};

        /// Seconds remaining of locked crosshair mutual gaze hold.
        /// Prevents rapid edge-chatter and head twitching when player crosshair grazes the NPC.
        float crosshairHoldTimerSec{0.0f};

        /// Cached resolved bones in the actor's 3D scene graph.
        /// Avoids thousands of redundant recursive traversals per frame.
        bool skeletonResolved{false};
        RE::NiAVObject *cachedRoot{nullptr};
        RE::NiAVObject *cachedSpine{nullptr};
        RE::NiAVObject *cachedNeck{nullptr};
        RE::NiAVObject *cachedHead{nullptr};
        RE::NiAVObject *cachedEyeL{nullptr};
        RE::NiAVObject *cachedEyeR{nullptr};

        /// Reset the simulation to a known state, e.g. after a cell change.
        void Reset(float startYaw, float startPitch) noexcept
        {
            saccade = {};
            saccade.currentYaw = startYaw;
            saccade.currentPitch = startPitch;

            vor = {};
            vor.headYaw = startYaw;
            vor.headPitch = startPitch;

            jitter = {};
            triangle = {};
            blink = {};

            mutualGazeHoldSec = 0.0f;
            lastYawDeg = startYaw;
            lastPitchDeg = startPitch;
            eyeSaturated = false;
            trackedTargetFormId = 0;
            bonesReported = false;
            modeOverrideTimerSec = 0.0f;
            hasModeOverride = false;
            rayDebugTimerSec = 0.0f;
            lastFrameTicked = 0;
            fixationHoldSec = 0.0f;
            crosshairHoldTimerSec = 0.0f;
            skeletonResolved = false;
            cachedRoot = nullptr;
            cachedSpine = nullptr;
            cachedNeck = nullptr;
            cachedHead = nullptr;
            cachedEyeL = nullptr;
            cachedEyeR = nullptr;
            initialised = true;
        }
    };

} // namespace TrueGaze::Engine