#include "GazeEngine.hpp"
#include "BoneController.hpp"
#include "EyeAimConstraint.hpp"
#include "LodManager.hpp"
#include "TargetSelector.hpp"
#include "ConfigManager.hpp"
#include "PerformanceProfiler.hpp"
#include "Integrations/OarConditions.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

#include <cmath>
#include <algorithm>

namespace TrueGaze::Engine
{

    namespace
    {

        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kRadToDeg = 180.0f / kPi;

        /// Skyrim world units per metre.
        constexpr float kUnitsPerMeter = 70.0f;

        /// Approximate eye height above an actor's origin, in Skyrim units (~1.6 m).
        constexpr float kEyeHeightUnits = 160.0f;

        /// Wrap an angle in radians to (-pi, pi].
        float WrapPi(float radians) noexcept
        {
            while (radians > kPi)
                radians -= 2.0f * kPi;
            while (radians < -kPi)
                radians += 2.0f * kPi;
            return radians;
        }

#if __has_include(<RE/Skyrim.h>)

        /// Bone names to try, in priority order. Different rigs (vanilla, XP32, custom
        /// creature skeletons) name these differently, so a candidate list is used rather
        /// than a single hardcoded name.
        constexpr const char *kSpineCandidates[] = {
            "NPC Spine2 [Spine2]", "Spine2", "NPC Spine1 [Spine1]"};
        constexpr const char *kNeckCandidates[] = {
            "NPC Neck [Neck]", "Neck", "Neck1"};
        constexpr const char *kHeadCandidates[] = {
            "NPC Head [Head]", "Head", "Head1"};
        constexpr const char *kEyeLeftCandidates[] = {
            "NPC L Eye [LEye]", "NPC L Eye [L Eye]", "L Eye", "LEye", "EyeLeft"};
        constexpr const char *kEyeRightCandidates[] = {
            "NPC R Eye [REye]", "NPC R Eye [R Eye]", "R Eye", "REye", "EyeRight"};

        RE::NiAVObject *FindFirstBone(RE::NiAVObject *root,
                                      const char *const *candidates,
                                      size_t count) noexcept
        {
            if (!root)
            {
                return nullptr;
            }

            // GetObjectByName is virtual and declared on NiAVObject; a cast to NiNode is
            // not required. This keeps the helper usable for a bone that is itself a
            // leaf property node on some rigs.
            for (size_t i = 0; i < count; ++i)
            {
                if (auto *found = root->GetObjectByName(RE::BSFixedString(candidates[i])))
                {
                    return found;
                }
            }
            return nullptr;
        }

        /// Convert a world-space target into actor-relative yaw/pitch, in degrees.
        void WorldTargetToLocalGaze(const RE::NiPoint3 &actorPos,
                                    float actorYawRad,
                                    const RE::NiPoint3 &targetPos,
                                    float &outYawDeg,
                                    float &outPitchDeg) noexcept
        {
            const float dx = targetPos.x - actorPos.x;
            const float dy = targetPos.y - actorPos.y;
            const float dz = targetPos.z - (actorPos.z + kEyeHeightUnits);

            const float horizontal = std::sqrt(dx * dx + dy * dy);

            // Skyrim's actor forward is +Y, so the bearing to the target is atan2(dx, dy).
            const float bearing = std::atan2(dx, dy);
            const float localYaw = WrapPi(bearing - actorYawRad);

            const float pitch = std::atan2(dz, std::max(horizontal, 1.0f));

            outYawDeg = localYaw * kRadToDeg;
            outPitchDeg = pitch * kRadToDeg;
        }

        /// Map a desired gaze deflection onto one of the 13 documented regions.
        uint8_t ClassifyRegion(float yawDeg, float pitchDeg) noexcept
        {
            // Aversion quadrants take priority: they are the cognitively meaningful ones.
            if (pitchDeg > 12.0f)
            {
                if (yawDeg < -5.0f)
                    return 9; // Upper-left peripheral (recall)
                if (yawDeg > 5.0f)
                    return 10; // Upper-right peripheral (recall)
                return 3;      // Forehead / upper face
            }

            if (pitchDeg < -12.0f)
            {
                return 8; // Floor / ground (shame, submission)
            }

            // Within the face → social triangle vertices.
            if (std::abs(pitchDeg) <= 3.0f)
            {
                if (yawDeg < -1.0f)
                    return 0; // Left eye
                if (yawDeg > 1.0f)
                    return 1; // Right eye
                return 0;
            }

            if (pitchDeg < 0.0f)
                return 2; // Mouth / lips
            return 3;     // Forehead
        }

#endif // __has_include(<RE/Skyrim.h>)

    } // namespace

    // ---------------------------------------------------------------------------
    // Lifecycle
    // ---------------------------------------------------------------------------

    void GazeEngine::RefreshTuning() noexcept
    {
        const auto &cfg = ConfigManager::GetSingleton();

        _tuning.enableTrueGaze = cfg.enableTrueGaze;
        _tuning.enableCreatures = cfg.enableCreatures;

        _tuning.saccadeSpeedMult = cfg.saccadeSpeedMult;
        _tuning.velocitySaturation = cfg.velocitySaturation;
        _tuning.microJitterAmp = cfg.microJitterAmp;
        _tuning.headTrackingSpeed = cfg.headTrackingSpeed;
        _tuning.maxComfortEyeAngle = cfg.maxComfortEyeAngle;

        _tuning.enableGazeAversion = cfg.enableGazeAversion;
        _tuning.enableSocialTriangle = cfg.enableSocialTriangle;
        _tuning.triangleFixationDuration = cfg.triangleFixationDuration;
        _tuning.mutualGazeThreshold = cfg.mutualGazeThreshold;

        _tuning.tier1DistanceMeters = cfg.tier1DistanceMeters;
        _tuning.tier2DistanceMeters = cfg.tier2DistanceMeters;

        logger::info("[TrueGaze] Tuning refreshed: saccadeMult={:.2f} jitter={:.2f} "
                     "headSpeed={:.2f} eyeMax={:.1f}",
                     _tuning.saccadeSpeedMult, _tuning.microJitterAmp,
                     _tuning.headTrackingSpeed, _tuning.maxComfortEyeAngle);
    }

    void GazeEngine::StartBridge() noexcept
    {
        if (_bridgeStarted)
        {
            return;
        }

        if (!ConfigManager::GetSingleton().connectHcepBridge)
        {
            logger::info("[TrueGaze] HCEP bridge disabled by configuration.");
            return;
        }

        _pipe.Start();
        _bridgeStarted = true;
    }

    void GazeEngine::StopBridge() noexcept
    {
        if (!_bridgeStarted)
        {
            return;
        }

        _pipe.Stop();
        _bridgeStarted = false;
    }

    void GazeEngine::ResetAll() noexcept
    {
        _actors.clear();
        logger::info("[TrueGaze] Actor gaze state cleared.");
    }

    // ---------------------------------------------------------------------------
    // Frame lifecycle
    // ---------------------------------------------------------------------------

    void GazeEngine::EndFrame(float deltaSeconds) noexcept
    {
        // Retire actors the hook did not touch this frame. Without this the map grows
        // without bound across a long session (NFR-3: memory footprint under 12 MB).
        for (auto it = _actors.begin(); it != _actors.end();)
        {
            it->second.idleSec += deltaSeconds;
            if (it->second.idleSec > ACTOR_EVICTION_SEC)
            {
                it = _actors.erase(it);
            }
            else
            {
                ++it;
            }
        }

        if (_frameOpen)
        {
            const auto elapsed = std::chrono::steady_clock::now() - _frameStart;
            _lastFrameUs = static_cast<uint64_t>(
                std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
            _peakFrameUs = std::max(_peakFrameUs, _lastFrameUs);
            _frameOpen = false;
        }

        // A frame that overran its budget is worth knowing about — and is the signal
        // an adaptive LOD would consume. Recorded, not acted on, for now.
        if (_lastFrameUs > 150)
        {
            logger::warn("[TrueGaze] Frame budget exceeded: {} us across {} actors.",
                         _lastFrameUs, _actors.size());
        }
    }

    void GazeEngine::ReleaseBones() noexcept
    {
        EyeAimConstraint::Withdraw();
    }

    // ---------------------------------------------------------------------------
    // Per-actor simulation
    // ---------------------------------------------------------------------------

    void GazeEngine::TickActor(RE::Actor *actor, float deltaSeconds) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (!actor || deltaSeconds <= 0.0f)
        {
            return;
        }

        if (!_tuning.enableTrueGaze)
        {
            return;
        }

        if (!_frameOpen)
        {
            _frameOpen = true;
            _frameStart = std::chrono::steady_clock::now();
        }

        const uint32_t formId = actor->GetFormID();
        if (formId == 0)
        {
            return;
        }

        // Eligibility mirrors the original filter but is now actually reached.
        if (!AnimationHook::IsActorEligibleForGaze(formId))
        {
            return;
        }

        auto *root = actor->Get3D();
        if (!root)
        {
            return;
        }

        // --- Resolve or create per-actor state ----------------------------------
        auto [it, inserted] = _actors.try_emplace(formId);
        if (inserted && _actors.size() > MAX_TRACKED_ACTORS)
        {
            _actors.erase(it);
            return;
        }

        ActorGazeRuntime &state = it->second;
        state.idleSec = 0.0f;

        if (!state.initialised)
        {
            const float startYaw = actor->GetAngleZ() * kRadToDeg;
            state.Reset(0.0f, 0.0f);
            state.rngSeed = formId;
            Kinematics::MicroJitter::Init(state.jitter, formId, _tuning.microJitterAmp);
            (void)startYaw;
        }

        // Keep the drift amplitude in step with configuration changes.
        state.jitter.amplitudeDeg = _tuning.microJitterAmp;
        state.vor.headTrackingSpeed = _tuning.headTrackingSpeed;
        state.vor.eyeMaxAngle = _tuning.maxComfortEyeAngle;

        // --- LOD tiering --------------------------------------------------------
        const auto *player = RE::PlayerCharacter::GetSingleton();
        float distanceMeters = 0.0f;

        if (player)
        {
            const float units = actor->GetPosition().GetDistance(player->GetPosition());
            distanceMeters = units / kUnitsPerMeter;
        }

        const auto tier = LodManager::GetLodTier(distanceMeters);

        if (tier == LodManager::LodTier::Tier3_Culled)
        {
            // Beyond 15 m the eyes are sub-pixel and the engine's own LOD applies.
            return;
        }

        // --- Compute and apply ---------------------------------------------------
        float yawDeg = 0.0f;
        float pitchDeg = 0.0f;
        ComputeDeflection(actor, state, deltaSeconds, yawDeg, pitchDeg);

        state.lastYawDeg = yawDeg;
        state.lastPitchDeg = pitchDeg;
        state.gazeRegion = ClassifyRegion(yawDeg, pitchDeg);

        ApplyToSkeleton(actor, state, yawDeg, pitchDeg);
        PublishState(actor, state);
#else
        (void)actor;
        (void)deltaSeconds;
#endif
    }

    void GazeEngine::ComputeDeflection(RE::Actor *actor, ActorGazeRuntime &state,
                                       float deltaSeconds,
                                       float &outYaw, float &outPitch) noexcept
    {
        outYaw = 0.0f;
        outPitch = 0.0f;

#if __has_include(<RE/Skyrim.h>)
        // --- Salience -----------------------------------------------------------
        const auto target = TargetSelector::ResolveTarget(actor->GetFormID());

        float desiredYaw = 0.0f;
        float desiredPitch = 0.0f;

        if (target.priority != TargetSelector::TargetPriority::None)
        {
            const RE::NiPoint3 targetPos{target.worldX, target.worldY, target.worldZ};
            WorldTargetToLocalGaze(actor->GetPosition(), actor->GetAngleZ(),
                                   targetPos, desiredYaw, desiredPitch);
        }

        // --- Cognitive state drives the HCEP mode -------------------------------
        // UI::IsMenuOpen is non-const, so the singleton must not be captured as const.
        auto *ui = RE::UI::GetSingleton();
        const bool inDialogue = ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);

        if (inDialogue)
        {
            // AFFECT during conversation: social triangle scanning.
            state.hcepMode = 1;
        }
        else if (actor->IsInCombat())
        {
            state.hcepMode = 0; // LOGIC: locked, analytical
        }
        else
        {
            state.hcepMode = 0;
        }

        // --- Social triangle (AFFECT) -------------------------------------------
        if (_tuning.enableSocialTriangle && state.hcepMode == 1)
        {
            state.triangle.fixationDurationSec = _tuning.triangleFixationDuration;
            Kinematics::SocialTriangle::Update(state.triangle, deltaSeconds,
                                               std::max(0.5f, target.distanceMeters));
            desiredYaw += state.triangle.vertexOffsetXDeg;
            desiredPitch += state.triangle.vertexOffsetYDeg;
        }

        // --- Cognitive gaze aversion (THINK) ------------------------------------
        // Occasional, deterministic-per-actor aversion so it does not read as random
        // twitching. Driven by the actor's own drift RNG rather than a global one.
        if (_tuning.enableGazeAversion && state.hcepMode == 4)
        {
            const float magic =
                static_cast<float>(state.rngSeed % 1000u) / 1000.0f;
            desiredYaw += (magic < 0.5f ? -1.0f : 1.0f) * 22.0f;
            desiredPitch += 9.0f;
        }

        // --- Target classification & saccade ------------------------------------
        const uint32_t targetFormId = target.targetFormId;
        if (state.trackedTargetFormId != targetFormId)
        {
            // Salience changed: commit to a new ballistic saccade.
            state.trackedTargetFormId = targetFormId;

            Kinematics::SaccadeGenerator::TriggerSaccade(
                state.saccade, desiredYaw, desiredPitch,
                _tuning.EffectiveVMax(Kinematics::SaccadeGenerator::DEFAULT_VMAX),
                _tuning.velocitySaturation);

            // A large saccade triggers a micro-blink (saccadic suppression).
            Integrations::EfmBlinkController::OnSaccadeTriggered(
                state.blink, state.saccade.amplitudeDeg);
        }

        Kinematics::SaccadeGenerator::Update(state.saccade, deltaSeconds);

        // --- Vestibulo-ocular reflex: split eye vs head -------------------------
        state.vor.targetYaw = state.saccade.currentYaw;
        state.vor.targetPitch = state.saccade.currentPitch;
        Kinematics::VorCoordinator::Update(state.vor, deltaSeconds);

        // --- Micro-saccadic drift -----------------------------------------------
        // Tier 2 keeps the head and neck moving but drops the sub-degree drift,
        // which is invisible at that range and costs a per-actor Gaussian draw.
        float jitterYaw = 0.0f;
        float jitterPitch = 0.0f;

        if (DistanceMetersForTier(actor) <= _tuning.tier1DistanceMeters)
        {
            Kinematics::MicroJitter::Update(state.jitter, deltaSeconds);
            jitterYaw = state.jitter.currentYawOffset;
            jitterPitch = state.jitter.currentPitchOffset;
        }

        outYaw = state.saccade.currentYaw + jitterYaw;
        outPitch = state.saccade.currentPitch + jitterPitch;
#else
        (void)actor;
        (void)state;
        (void)deltaSeconds;
#endif
    }

    void GazeEngine::ApplyToSkeleton(RE::Actor *actor, ActorGazeRuntime &state,
                                     float yawDeg, float pitchDeg) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto *root = actor->Get3D();
        if (!root)
        {
            return;
        }

        // Distribute the deflection anatomically. The eyes receive the residual the
        // head chain did not cover, which is what produces "eyes lead, head follows".
        const auto strain = BoneController::CalculateHierarchyStrain(
            yawDeg, pitchDeg, state.vor.eyeMaxAngle, state.vor.eyeMaxAngle);

        auto *spine = FindFirstBone(root, kSpineCandidates, std::size(kSpineCandidates));
        auto *neck = FindFirstBone(root, kNeckCandidates, std::size(kNeckCandidates));
        auto *head = FindFirstBone(root, kHeadCandidates, std::size(kHeadCandidates));
        auto *eyeL = FindFirstBone(root, kEyeLeftCandidates, std::size(kEyeLeftCandidates));
        auto *eyeR = FindFirstBone(root, kEyeRightCandidates, std::size(kEyeRightCandidates));

        // Report the skeleton probe once per actor.
        //
        // Bone names are matched by string, and these candidate lists have never
        // been confirmed against a live rig. If every name misses, the engine runs
        // perfectly and rotates nothing - a silent no-op, which is precisely the
        // failure this project exists to stop repeating. One log line per actor
        // converts that into an answerable question: did the bones resolve?
        if (!state.bonesReported)
        {
            const int found = (spine ? 1 : 0) + (neck ? 1 : 0) + (head ? 1 : 0) +
                              (eyeL ? 1 : 0) + (eyeR ? 1 : 0);

            logger::info("[TrueGaze] Skeleton probe for {:08X}: spine={} neck={} head={} "
                         "eyeL={} eyeR={} ({} of 5 resolved)",
                         actor->GetFormID(),
                         spine ? "yes" : "NO", neck ? "yes" : "NO", head ? "yes" : "NO",
                         eyeL ? "yes" : "NO", eyeR ? "yes" : "NO", found);

            // The head is the one that absolutely must resolve; without it there is
            // no gaze to see. Be loud rather than let this pass as a quiet zero.
            if (!head)
            {
                logger::warn("[TrueGaze] No head bone found for {:08X}. Gaze will not be "
                             "visible for this actor. The bone-name candidates in "
                             "GazeEngine.cpp need extending for this rig.",
                             actor->GetFormID());
            }

            state.bonesReported = true;
        }

        if (spine)
        {
            EyeAimConstraint::Apply(spine, strain.spineYaw, 0.0f);
        }

        if (neck)
        {
            EyeAimConstraint::Apply(neck, strain.neckYaw, strain.neckPitch);
        }

        if (head)
        {
            EyeAimConstraint::Apply(head, strain.headYaw, strain.headPitch);
        }

        // Eyes take the residual. During a ballistic saccade this is where the eye
        // lead is visible: the eyes snap while the neck and head are still damping in.
        if (eyeL)
        {
            EyeAimConstraint::Apply(eyeL, strain.eyeYaw, strain.eyePitch);
        }

        if (eyeR)
        {
            EyeAimConstraint::Apply(eyeR, strain.eyeYaw, strain.eyePitch);
        }

        // Eyelid morph writes are applied separately by EfmBlinkController.
        if (state.blink.isBlinking)
        {
            Integrations::EfmBlinkController::ApplyMorphs(actor->GetFormID(),
                                                          state.blink.eyelidCloseWeight);
        }
#else
        (void)actor;
        (void)state;
        (void)yawDeg;
        (void)pitchDeg;
#endif
    }

    void GazeEngine::PublishState(RE::Actor *actor, const ActorGazeRuntime &state) noexcept
    {
        Integrations::OarConditions::PublishActorState(
            actor->GetFormID(), state.hcepMode, state.gazeRegion,
            state.mutualGazeHoldSec);
    }

    float GazeEngine::DistanceMetersForTier(RE::Actor *actor) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        const auto *player = RE::PlayerCharacter::GetSingleton();
        if (!player || !actor)
        {
            return 0.0f;
        }
        return actor->GetPosition().GetDistance(player->GetPosition()) / kUnitsPerMeter;
#else
        (void)actor;
        return 0.0f;
#endif
    }

    // ---------------------------------------------------------------------------
    // Queries
    // ---------------------------------------------------------------------------

    ActorGazeRuntime *GazeEngine::FindActor(uint32_t formId) noexcept
    {
        auto it = _actors.find(formId);
        return it != _actors.end() ? &it->second : nullptr;
    }

    const char *GazeEngine::ModeName(uint8_t mode) noexcept
    {
        switch (mode)
        {
        case 0:
            return "LOGIC";
        case 1:
            return "AFFECT";
        case 2:
            return "SPIRIT";
        case 3:
            return "HEART";
        case 4:
            return "THINK";
        default:
            return "UNKNOWN";
        }
    }

    const char *GazeEngine::RegionName(uint8_t region) noexcept
    {
        switch (region)
        {
        case 0:
            return "LeftEye";
        case 1:
            return "RightEye";
        case 2:
            return "Mouth";
        case 3:
            return "Forehead";
        case 4:
            return "Chin";
        case 5:
            return "Torso";
        case 6:
            return "RightHand";
        case 7:
            return "LeftHand";
        case 8:
            return "Ground";
        case 9:
            return "UpperLeftPeripheral";
        case 10:
            return "UpperRightPeripheral";
        case 11:
            return "Horizon";
        case 12:
            return "Defocused";
        default:
            return "Unknown";
        }
    }

} // namespace TrueGaze::Engine
