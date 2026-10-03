#pragma once

#include "PCH.h"
#include "Kinematics/SaccadeGenerator.hpp"
#include "Kinematics/VorCoordinator.hpp"
#include "Kinematics/MicroJitter.hpp"
#include "Kinematics/SocialTriangle.hpp"
#include "Integrations/EfmBlinkController.hpp"
#include "Engine/CharacterProfile.hpp"

namespace TrueGaze::Engine
{

    /// @brief Per-actor kinematics state, persisted across frames.
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

        /// Wall-clock seconds since this actor was last evaluated. Used for eviction.
        float idleSec{0.0f};

        /// True once the actor has been evaluated at least once.
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

        /// Monotonic engine frame number when this actor was last evaluated.
        /// Prevents double-ticking within a single render frame.
        uint64_t lastFrameTicked{0};

        /// Seconds the actor has continuously fixated on the current target.
        /// Enforces human gaze dwell time (hysteresis) to eliminate target flapping.
        float fixationHoldSec{0.0f};

        /// Seconds remaining of locked crosshair mutual gaze hold.
        /// Prevents rapid edge-chatter and head twitching when player crosshair grazes the NPC.
        float crosshairHoldTimerSec{0.0f};

        /// True when CGA is active for this actor (THINK mode aversion in progress).
        bool cgaActive{false};

        /// Per-actor dialogue-sync CGA return offset (±seconds). Randomised once
        /// per CGA episode so NPCs don't all snap back at the same instant.
        float cgaDialogueReturnOffsetSec{0.0f};

        /// True when the actor was NOT in dialogue last frame (edge detection).
        bool wasNotInDialogue{true};

        /// SCENE DEFER (vanilla direction yield). When the game's own AI has
        /// assigned this NPC a headtrack target via scripted scene direction
        /// (PackageStart scenes like the Helgen cart), TrueGaze yields and lets
        /// vanilla direct the gaze — UNLESS the player is that directed target.
        /// Vanilla scene direction is authoritative for its own staged moments;
        /// TrueGaze takes hold only when free-roaming or when the player is the
        /// scene's focus.
        bool sceneDeferActive{false};

        /// DIALOGUE PLAYER HOLD (2026-09-26, "Ralof and Lokir still do not
        /// target the main player during targeted dialogue"). The engine's
        /// direction signal (headTrackTarget slots / dialogueItemTarget)
        /// FLICKERS between frames mid-line — the log showed priority=4 (the
        /// player) for one trace window, then priority=2 (a nearer NPC winning
        /// the social scan) the next. Each dropout re-aimed the gaze away from
        /// the player mid-sentence. While this timer counts down, the player
        /// target is re-served at DialoguePartner priority even if the raw
        /// direction signal drops out. Armed (3 s) whenever a direction slot
        /// or the dialogue item resolves to the player; decays otherwise.
        float dialoguePlayerHoldSec{0.0f};

        /// CHARACTER GAZE PROFILE (temperament-driven gaze). Cached per actor;
        /// recomputed only on refresh events (cell change, combat edge), never
        /// per frame — the AV/relationship reads are not free.
        CharacterProfile::GazeProfile profile{};
        bool profileValid{false};
        /// True when the actor was in combat when the profile was computed
        /// (edge detection for combat-driven refresh).
        bool profileWasInCombat{false};

        /// PLAYER BEHAVIOURAL PROFILE (player actor only). Decaying accumulator
        /// of the player's own face-attention: rises while the crosshair holds on
        /// a face, decays otherwise. Feeds the player's own gaze profile so an
        /// attentive player's 3rd-person gaze reads steadier and warmer — the
        /// player's character reflects the player's behaviour.
        float playerAttentionSec{0.0f};

        // --- Trace telemetry state (A8) ---
        uint8_t lastClassifiedRegion{0xFF};
        float currentRegionDwellSec{0.0f};
        bool wasInBallisticSaccade{false};
        uint8_t saccadeFromRegion{0};
        bool wasInCga{false};
        float cgaDurationSec{0.0f};
        bool wasBlinking{false};
        float timeSinceLastBlink{0.0f};

        /// Cached resolved bones in the actor's 3D scene graph.
        /// Avoids thousands of redundant recursive traversals per frame.
        bool skeletonResolved{false};
        RE::NiAVObject *cachedRoot{nullptr};
        RE::NiAVObject *cachedSpine{nullptr};
        RE::NiAVObject *cachedNeck{nullptr};
        RE::NiAVObject *cachedHead{nullptr};
        RE::NiAVObject *cachedEyeL{nullptr};
        RE::NiAVObject *cachedEyeR{nullptr};

        /// Reset the kinematics runtime to a known state, e.g. after a cell change.
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
            triangle.rngSeeded = false; // Will be re-seeded on first use
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
            cgaActive = false;
            cgaDialogueReturnOffsetSec = 0.0f;
            wasNotInDialogue = true;
            lastClassifiedRegion = 0xFF;
            currentRegionDwellSec = 0.0f;
            wasInBallisticSaccade = false;
            saccadeFromRegion = 0;
            wasInCga = false;
            cgaDurationSec = 0.0f;
            wasBlinking = false;
            timeSinceLastBlink = 0.0f;
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