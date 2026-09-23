#include "GazeEngine.hpp"
#include "BoneController.hpp"
#include "EyeAimConstraint.hpp"
#include "LodManager.hpp"
#include "TargetSelector.hpp"
#include "PlayerGazeResolver.hpp"
#include "ConfigManager.hpp"
#include "PerformanceProfiler.hpp"
#include "Integrations/OarConditions.hpp"
#include "Visuals/VisualEffectsManager.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#include <RE/H/HighProcessData.h>
#include <RE/S/SendHUDMessage.h>
#include <RE/C/ConsoleLog.h>
#include <RE/B/BSVisit.h>
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

        /// Approximate eye height above an actor's origin, in Skyrim units (~1.25 m).
        constexpr float kEyeHeightUnits = 125.0f;

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

        /// Bone names to try, in priority order. Different rigs (vanilla, XP32/XPMSSE, custom
        /// creature skeletons) name these differently. Standard Skyrim skeleton uses literal
        /// "NPC L Eye" and "NPC R Eye", and "NPC Spine2 [Spn2]".
        constexpr const char *kSpineCandidates[] = {
            "NPC Spine2 [Spn2]", "NPC Spine1 [Spn1]", "NPC Spine [Spn0]",
            "NPC Spine2 [Spine2]", "Spine2", "NPC Spine1 [Spine1]", "Spine"};
        constexpr const char *kNeckCandidates[] = {
            "NPC Neck [Neck]", "Neck", "Neck1"};
        constexpr const char *kHeadCandidates[] = {
            "NPC Head [Head]", "Head", "Head1"};
        constexpr const char *kEyeLeftCandidates[] = {
            "NPC L Eye", "NPC L Eye [LEye]", "NPC L Eye [L Eye]", "Eye_L", "EyeLeft", "L Eye", "LEye"};
        constexpr const char *kEyeRightCandidates[] = {
            "NPC R Eye", "NPC R Eye [REye]", "NPC R Eye [R Eye]", "Eye_R", "EyeRight", "R Eye", "REye"};

        RE::NiAVObject *FindFirstBone(RE::NiAVObject *root,
                                      const char *const *candidates,
                                      size_t count) noexcept
        {
            if (!root)
            {
                return nullptr;
            }

            for (size_t i = 0; i < count; ++i)
            {
                if (auto *found = root->GetObjectByName(RE::BSFixedString(candidates[i])))
                {
                    return found;
                }
            }
            return nullptr;
        }

        RE::NiAVObject *FindBoneFuzzy(RE::NiAVObject *root, std::string_view a_needle) noexcept
        {
            if (!root)
            {
                return nullptr;
            }

            RE::NiAVObject *result = nullptr;
            RE::BSVisit::TraverseScenegraphObjects(root, [&](RE::NiAVObject *obj)
                                                   {
                if (obj && obj->name.c_str())
                {
                    std::string s = obj->name.c_str();
                    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (s.find(a_needle) != std::string::npos)
                    {
                        result = obj;
                        return RE::BSVisit::BSVisitControl::kStop;
                    }
                }
                return RE::BSVisit::BSVisitControl::kContinue; });
            return result;
        }

        /// Convert a world-space target into actor-relative yaw/pitch, in degrees.
        /// observerEyePos is the world position of the observer's head/eyes.
        /// targetPos is the true 3D world position of the target's head/eyes.
        void WorldTargetToLocalGaze(const RE::NiPoint3 &observerEyePos,
                                    float actorYawRad,
                                    const RE::NiPoint3 &targetPos,
                                    float &outYawDeg,
                                    float &outPitchDeg) noexcept
        {
            const float dx = targetPos.x - observerEyePos.x;
            const float dy = targetPos.y - observerEyePos.y;
            const float dz = targetPos.z - observerEyePos.z;

            const float horizontal = std::sqrt(dx * dx + dy * dy);

            // If the target is co-located with the actor (horizontal distance < 25 units / 0.35m),
            // atan2(dx, dy) produces a mathematical singularity (0.0 rad / World North)
            // resulting in extreme yaw snapping (e.g. +106 degrees during Helgen cart rides).
            if (horizontal < 25.0f)
            {
                outYawDeg = 0.0f;
                outPitchDeg = 0.0f;
                return;
            }

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
        _tuning.debugGazeRays = cfg.debugGazeRays;

        _tuning.saccadeSpeedMult = cfg.saccadeSpeedMult;
        _tuning.velocitySaturation = cfg.velocitySaturation;
        _tuning.microJitterAmp = cfg.microJitterAmp;
        _tuning.microJitterIntervalMin = cfg.microJitterIntervalMin;
        _tuning.microJitterIntervalMax = cfg.microJitterIntervalMax;
        _tuning.headTrackingSpeed = cfg.headTrackingSpeed;
        _tuning.maxComfortEyeAngle = cfg.maxComfortEyeAngle;
        _tuning.headOnsetDelaySec = cfg.headOnsetDelaySec;

        _tuning.spine2YawWeight = cfg.spine2YawWeight;
        _tuning.neckYawWeight = cfg.neckYawWeight;
        _tuning.neckPitchWeight = cfg.neckPitchWeight;
        _tuning.headYawWeight = cfg.headYawWeight;
        _tuning.headPitchWeight = cfg.headPitchWeight;

        _tuning.enableGazeAversion = cfg.enableGazeAversion;
        _tuning.enableSocialTriangle = cfg.enableSocialTriangle;
        _tuning.triangleFixationDuration = cfg.triangleFixationDuration;
        _tuning.mutualGazeThreshold = cfg.mutualGazeThreshold;

        _tuning.tier1DistanceMeters = cfg.tier1DistanceMeters;
        _tuning.tier2DistanceMeters = cfg.tier2DistanceMeters;

        // Crosshair sweet-spot parameters feed TargetSelector's static frame
        // snapshot. Game-thread only, consistent with the rest of the engine.
        TargetSelector::s_crosshair.enabled = cfg.enableCrosshairGaze;
        TargetSelector::s_crosshair.baseToleranceDeg = cfg.crosshairToleranceDeg;
        TargetSelector::s_crosshair.maxRangeMeters = cfg.crosshairMaxRangeMeters;
        TargetSelector::s_crosshair.pointBlankMeters = cfg.crosshairPointBlankMeters;

        // In-game visual settings are snapshotted alongside the kinematics tuning.
        // The renderer is a pure consumer of the engine's solved state, so its
        // constants travel the same single path as everything else in GazeTuning.
        {
            Visuals::VisualTuning vis{};
            vis.enableInGameVisuals = cfg.enableInGameVisuals;
            vis.gazeRaysEnabled = cfg.gazeRaysEnabled;
            vis.rayRenderMode = cfg.rayRenderMode;
            vis.gazeRayLengthMeters = cfg.gazeRayLengthMeters;
            vis.gazeRayThicknessCm = cfg.gazeRayThicknessCm;
            vis.gazeRayColour = static_cast<uint32_t>(cfg.gazeRayColour) & 0x00FFFFFFu;
            vis.gazeRayOpacity = cfg.gazeRayOpacity;
            vis.gazeRaysOnPlayer = cfg.gazeRaysOnPlayer;
            vis.gazeRaysOnNPCs = cfg.gazeRaysOnNPCs;
            vis.gazeRaysOnCreatures = cfg.gazeRaysOnCreatures;
            vis.gazeRaysAttachHead = cfg.gazeRaysAttachHead;
            vis.gazeRaysTerminus = cfg.gazeRaysTerminus;
            vis.pupilForwardOffsetCm = cfg.pupilForwardOffsetCm;
            vis.pupilUpOffsetCm = cfg.pupilUpOffsetCm;
            vis.pupilGlowIntensity = cfg.pupilGlowIntensity;
            vis.showHcepPanel = cfg.showHcepPanel;
            vis.hcepPanelAllActors = cfg.hcepPanelAllActors;
            vis.hcepPanelScale = cfg.hcepPanelScale;
            vis.hcepPanelForwardOffsetCm = cfg.hcepPanelForwardOffsetCm;
            Visuals::VisualEffectsManager::Get().SetTuning(vis);
        }

        logger::info("[TrueGaze] Tuning refreshed: saccadeMult={:.2f} jitter={:.2f} "
                     "headSpeed={:.2f} eyeMax={:.1f} crosshair={}",
                     _tuning.saccadeSpeedMult, _tuning.microJitterAmp,
                     _tuning.headTrackingSpeed, _tuning.maxComfortEyeAngle,
                     cfg.enableCrosshairGaze ? "on" : "off");
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

        const auto &cfg = ConfigManager::GetSingleton();
        _pipe.Start(cfg.pipeName.c_str(), cfg.autoReconnectIntervalSec);
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
        EyeAimConstraint::Reset();
        Visuals::VisualEffectsManager::Get().Reset();
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
                // Withdraw the applied bone transforms before dropping the state so
                // an evicted actor does not keep its last gaze pose frozen on.
                EyeAimConstraint::WithdrawActor(it->first);
                // The visual emitters are attached to the skeleton, so they must be
                // detached here too or they would outlive the actor they annotate.
                Visuals::VisualEffectsManager::Get().RemoveActor(it->first);
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
        if (_lastFrameUs > 1500)
        {
            static std::chrono::steady_clock::time_point s_lastBudgetWarning{};
            const auto now = std::chrono::steady_clock::now();
            if (now - s_lastBudgetWarning > std::chrono::seconds(10))
            {
                s_lastBudgetWarning = now;
                logger::warn("[TrueGaze] Frame budget exceeded: {} us across {} actors.",
                             _lastFrameUs, _actors.size());
            }
        }

        AdvanceFrameCounter();
    }

    void GazeEngine::ReleaseBones() noexcept
    {
        EyeAimConstraint::Withdraw();
    }

    // ---------------------------------------------------------------------------
    // Per-actor kinematics execution
    // ---------------------------------------------------------------------------

    void GazeEngine::TickActor(RE::Actor *actor, float deltaSeconds) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        ++_tickCalls;
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

        // Eligibility check via version-independent virtual IsDead() and 3D status.
        if (!AnimationHook::IsActorEligibleForGaze(actor))
        {
            return;
        }

        ++_eligibleTicks;

        auto *root = actor->Get3D();
        if (!root)
        {
            return;
        }

        if (_eligibleTicks == 1)
        {
            logger::info("[TrueGaze] Eligible actor tick: form={:08X} player={} humanoid={}",
                         formId,
                         actor->IsPlayerRef() ? "yes" : "no",
                         actor->IsHumanoid() ? "yes" : "no");
        }

        // --- Resolve or create per-actor state ----------------------------------
        auto [it, inserted] = _actors.try_emplace(formId);
        if (inserted && _actors.size() > MAX_TRACKED_ACTORS)
        {
            _actors.erase(it);
            return;
        }

        ActorGazeRuntime &state = it->second;

        // Frame deduplication: prevent double-ticking within the same render frame
        // (e.g. between individual Character::Update and batch ProcessLists::highActorHandles).
        if (state.lastFrameTicked == _frameCounter)
        {
            return;
        }
        state.lastFrameTicked = _frameCounter;
        state.idleSec = 0.0f;

        if (!state.initialised)
        {
            const float startYaw = actor->GetAngleZ() * kRadToDeg;
            state.Reset(0.0f, 0.0f);
            state.rngSeed = formId;
            Kinematics::MicroJitter::Init(state.jitter, formId, _tuning.microJitterAmp);
            (void)startYaw;
        }

        // Keep the drift amplitude in step with configuration changes. The mean
        // reversion rate derives from the configured micro-correction interval:
        // corrections arrive on average every `interval` seconds, so drift is pulled
        // back at theta = 1/mean(interval). (0.2-0.45 s -> theta ~ 3.1/s, within the
        // physiological 2-8/s band for ocular drift correction.)
        state.jitter.amplitudeDeg = _tuning.microJitterAmp;
        {
            const float meanInterval = 0.5f * (_tuning.microJitterIntervalMin +
                                               _tuning.microJitterIntervalMax);
            state.jitter.reversionRate = (meanInterval > 0.0f) ? (1.0f / meanInterval)
                                                               : state.jitter.reversionRate;
        }
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

        const auto tier = LodManager::GetLodTier(distanceMeters,
                                                 _tuning.tier1DistanceMeters,
                                                 _tuning.tier2DistanceMeters);

        if (tier == LodManager::LodTier::Tier3_Culled)
        {
            // Beyond 15 m the eyes are sub-pixel and the engine's own LOD applies.
            ++_culledTicks;
            return;
        }

        // --- Compute and apply ---------------------------------------------------
        float yawDeg = 0.0f;
        float pitchDeg = 0.0f;
        ComputeDeflection(actor, state, deltaSeconds, yawDeg, pitchDeg);

        state.lastYawDeg = yawDeg;
        state.lastPitchDeg = pitchDeg;
        state.gazeRegion = ClassifyRegion(yawDeg, pitchDeg);

        if (_tuning.debugGazeRays)
        {
            state.rayDebugTimerSec += deltaSeconds;
            if (state.rayDebugTimerSec >= 1.0f)
            {
                state.rayDebugTimerSec = 0.0f;
                const auto pos = actor->GetPosition();
                const char *actorType = actor->IsPlayerRef() ? "Player" : "NPC";

                logger::info("[TrueGaze::Ray] Actor {:08X} ({}) at ({:.1f}, {:.1f}, {:.1f}) -> Gaze Yaw={:+.1f}deg Pitch={:+.1f}deg Region={} (HCEP Mode={})",
                             formId,
                             actorType,
                             pos.x, pos.y, pos.z,
                             yawDeg, pitchDeg,
                             state.gazeRegion,
                             state.hcepMode);

                if (auto *console = RE::ConsoleLog::GetSingleton())
                {
                    console->Print("[TrueGaze::Ray] %08X (%s) -> Yaw:%+.1f deg Pitch:%+.1f deg Region:%d (Mode:%d)",
                                   formId, actorType, yawDeg, pitchDeg, state.gazeRegion, state.hcepMode);
                }

                if (actor->IsPlayerRef() && (std::abs(yawDeg) > 1.5f || std::abs(pitchDeg) > 1.5f))
                {
                    char hudBuf[128];
                    std::snprintf(hudBuf, sizeof(hudBuf), "[TrueGaze] 3rd-Person Gaze: Yaw %+.1f deg | Pitch %+.1f deg", yawDeg, pitchDeg);
                    RE::SendHUDMessage::ShowHUDMessage(hudBuf);
                }
            }
        }

        // Only clear Skyrim's native headtracking when TrueGaze has resolved a
        // real social target (trackedTargetFormId != 0, meaning NearbyActor or higher).
        // AmbientInterest always resolves to targetFormId == 0 (a vacant forward point).
        // If we only have ambient, leave vanilla headtracking running — it does a better
        // job than overriding it with an empty stare into space. Without this guard, NPCs
        // with the player outside their forward cone had their native tracking cleared and
        // replaced with nothing, causing them to look away from the player.
        if (!actor->IsPlayerRef() && state.trackedTargetFormId != 0)
        {
            if (auto *high = actor->GetHighProcess())
            {
                for (std::uint32_t i = 0; i < RE::HighProcessData::HEAD_TRACK_TYPE::kTotal; ++i)
                {
                    high->ClearHeadtrackTarget(static_cast<RE::HighProcessData::HEAD_TRACK_TYPE>(i), false);
                }
            }
        }

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
        // --- Player attention input --------------------------------------------
        Bridge::TrueGazeTelemetryPacket hcepPacket{};
        const bool hasHcepTelemetry = _pipe.IsConnected() &&
                                      _pipe.TryGetLatestTelemetry(hcepPacket);
        if (hasHcepTelemetry)
        {
            PlayerGazeResolver::SetHcepTelemetry(hcepPacket);
        }
        else
        {
            PlayerGazeResolver::ClearHcepTelemetry();
        }

        // --- Salience -----------------------------------------------------------
        const auto target = TargetSelector::ResolveTarget(actor->GetFormID(), &state, deltaSeconds);
        ++_targetResolutions;
        _lastTargetPriority = static_cast<uint8_t>(target.priority);
        _lastTargetFormId = target.targetFormId;
        if (target.priority == TargetSelector::TargetPriority::None)
        {
            ++_noTargetResolutions;
        }

        if (_targetResolutions == 1 || (_targetResolutions % 300) == 0)
        {
            logger::info("[TrueGaze] Target trace: resolutions={} priority={} form={:08X} "
                         "distance={:.2f}m actor={:08X}",
                         _targetResolutions,
                         static_cast<unsigned>(target.priority),
                         target.targetFormId,
                         target.distanceMeters,
                         actor->GetFormID());
        }

        float desiredYaw = 0.0f;
        float desiredPitch = 0.0f;

        if (target.priority != TargetSelector::TargetPriority::None)
        {
            const RE::NiPoint3 targetPos{target.worldX, target.worldY, target.worldZ};
            RE::NiPoint3 observerEyePos = actor->GetPosition();
            if (state.cachedHead)
            {
                observerEyePos = state.cachedHead->world.translate;
            }
            else if (auto *root = actor->Get3D())
            {
                observerEyePos = RE::NiPoint3{root->world.translate.x, root->world.translate.y, root->world.translate.z + kEyeHeightUnits};
            }
            else
            {
                observerEyePos.z += kEyeHeightUnits;
            }
            WorldTargetToLocalGaze(observerEyePos, actor->GetAngleZ(),
                                   targetPos, desiredYaw, desiredPitch);
        }

        // --- Cognitive state drives the HCEP mode -------------------------------
        if (state.hasModeOverride)
        {
            state.modeOverrideTimerSec -= deltaSeconds;
            if (state.modeOverrideTimerSec <= 0.0f)
            {
                state.hasModeOverride = false;
                state.modeOverrideTimerSec = 0.0f;
            }
        }
        else
        {
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

            // Mode 2: Stream human player eye fixations and cognitive mode from HCEP Desktop
            if (hasHcepTelemetry)
            {
                state.hcepMode = hcepPacket.hcepMode;
            }
        }

        // --- Mutual gaze (crosshair sweet spot) ----------------------------------
        // The player's crosshair resting on this actor's face is the ground truth
        // for "the player is looking at me". While it holds, the actor holds eye
        // contact and the mutual-gaze timer accumulates; the moment it breaks the
        // timer resets. This is the first production consumer of mutualGazeHoldSec,
        // which previously existed but was never written by anything.
        bool mutualGazeNow = false;
        if (_tuning.enableCrosshairGaze &&
            target.priority == TargetSelector::TargetPriority::CrosshairFocus)
        {
            PlayerGazeResolver::Params gazeParams{};
            gazeParams.baseToleranceDeg = _tuning.crosshairToleranceDeg;
            gazeParams.maxRangeMeters = _tuning.crosshairMaxRangeMeters;
            gazeParams.pointBlankMeters = _tuning.crosshairPointBlankMeters;

            mutualGazeNow = PlayerGazeResolver::IsPlayerLookingAtFace(
                actor->GetFormID(), gazeParams);
        }

        if (mutualGazeNow)
        {
            ++_mutualGazeFrames;
            state.mutualGazeHoldSec += deltaSeconds;
            if (_tuning.debugGazeRays && state.mutualGazeHoldSec >= 0.5f && state.mutualGazeHoldSec - deltaSeconds < 0.5f)
            {
                const char *actorName = actor->GetName();
                char hudBuf[128];
                std::snprintf(hudBuf, sizeof(hudBuf), "[TrueGaze] Eye Contact Held: %s",
                              (actorName && actorName[0]) ? actorName : "Target NPC");
                RE::SendHUDMessage::ShowHUDMessage(hudBuf);
            }
        }
        else
        {
            state.mutualGazeHoldSec = 0.0f;
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
            ++_saccadesTriggered;

            Kinematics::SaccadeGenerator::TriggerSaccade(
                state.saccade, desiredYaw, desiredPitch,
                _tuning.EffectiveVMax(Kinematics::SaccadeGenerator::DEFAULT_VMAX),
                _tuning.velocitySaturation);

            // A large saccade triggers a micro-blink (saccadic suppression).
            const bool wasBlinking = state.blink.isBlinking;
            Integrations::EfmBlinkController::OnSaccadeTriggered(
                state.blink, state.saccade.amplitudeDeg);
            if (!wasBlinking && state.blink.isBlinking)
            {
                ++_blinksTriggered;
            }

            // Biological Latency Gap: Arm head onset delay so eyes lead and head lags
            state.vor.headOnsetDelayTimerSec = _tuning.headOnsetDelaySec;
        }
        else if (!state.saccade.isBallistic)
        {
            // SMOOTH PURSUIT: When continuing to track the same target, smoothly pursue
            // any displacement in desired gaze (e.g. social triangle scanning, actor locomotion)
            // instead of freezing the eye at the old saccade terminus!
            const float diffYaw = desiredYaw - state.saccade.currentYaw;
            const float diffPitch = desiredPitch - state.saccade.currentPitch;
            const float diffDistSq = diffYaw * diffYaw + diffPitch * diffPitch;

            if (diffDistSq > 400.0f) // > 20 degrees sudden jump
            {
                // Target made a major sudden jump while keeping same FormID: trigger catch-up saccade
                ++_saccadesTriggered;
                Kinematics::SaccadeGenerator::TriggerSaccade(
                    state.saccade, desiredYaw, desiredPitch,
                    _tuning.EffectiveVMax(Kinematics::SaccadeGenerator::DEFAULT_VMAX),
                    _tuning.velocitySaturation);

                const bool wasBlinking = state.blink.isBlinking;
                Integrations::EfmBlinkController::OnSaccadeTriggered(
                    state.blink, state.saccade.amplitudeDeg);
                if (!wasBlinking && state.blink.isBlinking)
                {
                    ++_blinksTriggered;
                }

                // Biological Latency Gap on major catch-up saccade
                state.vor.headOnsetDelayTimerSec = _tuning.headOnsetDelaySec;
            }
            else
            {
                // Smooth ocular pursuit: track continuously
                state.saccade.targetYaw = desiredYaw;
                state.saccade.targetPitch = desiredPitch;
                state.saccade.currentYaw = desiredYaw;
                state.saccade.currentPitch = desiredPitch;
            }
        }

        Kinematics::SaccadeGenerator::Update(state.saccade, deltaSeconds);

        // --- Blink lifecycle -----------------------------------------------------
        // Without Update() a triggered blink never clears: isBlinking stays true and
        // ApplyMorphs writes a stale eyelid weight every frame for the rest of the
        // session. Update advances the blink envelope (close -> open -> rest).
        Integrations::EfmBlinkController::Update(state.blink, deltaSeconds);

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

        (void)yawDeg;
        (void)pitchDeg;

        // Distribute the head deflection across spine, neck, and head.
        // The neck and head follow the inertial approach (state.vor.headYaw/Pitch),
        // while the eyes directly receive the biological VOR counter-rotation
        // (state.vor.eyeLocalYaw/Pitch) plus micro-saccadic jitter drift!
        const BoneController::StrainWeights weights{
            _tuning.spine2YawWeight, _tuning.neckYawWeight, _tuning.neckPitchWeight,
            _tuning.headYawWeight, _tuning.headPitchWeight};
        const auto strain = BoneController::CalculateHierarchyStrain(
            state.vor.headYaw, state.vor.headPitch, state.vor.eyeMaxAngle, state.vor.eyeMaxAngle, weights);

        const float jitterYaw = (DistanceMetersForTier(actor) <= _tuning.tier1DistanceMeters)
                                    ? state.jitter.currentYawOffset
                                    : 0.0f;
        const float jitterPitch = (DistanceMetersForTier(actor) <= _tuning.tier1DistanceMeters)
                                      ? state.jitter.currentPitchOffset
                                      : 0.0f;

        const float eyeYaw = std::clamp(
            state.vor.eyeLocalYaw + jitterYaw,
            -_tuning.maxComfortEyeAngle, _tuning.maxComfortEyeAngle);
        const float eyePitch = std::clamp(
            state.vor.eyeLocalPitch + jitterPitch,
            -_tuning.maxComfortEyeAngle, _tuning.maxComfortEyeAngle);

        state.eyeSaturated = (std::abs(eyeYaw) >= _tuning.maxComfortEyeAngle - 0.01f ||
                              std::abs(eyePitch) >= _tuning.maxComfortEyeAngle - 0.01f);

        // Bone Caching: Probe and resolve bone pointers only once per actor / root model.
        // On vanilla skeletons lacking eye bones, FindFirstBone misses for both eyes every frame,
        // which previously triggered 6 recursive scene-graph traversals per actor per frame with
        // string allocations. Caching resolved bones eliminates thousands of traversals per second.
        if (!state.skeletonResolved || state.cachedRoot != root)
        {
            state.cachedRoot = root;
            state.cachedSpine = FindFirstBone(root, kSpineCandidates, std::size(kSpineCandidates));
            state.cachedNeck = FindFirstBone(root, kNeckCandidates, std::size(kNeckCandidates));
            state.cachedHead = FindFirstBone(root, kHeadCandidates, std::size(kHeadCandidates));
            state.cachedEyeL = FindFirstBone(root, kEyeLeftCandidates, std::size(kEyeLeftCandidates));
            state.cachedEyeR = FindFirstBone(root, kEyeRightCandidates, std::size(kEyeRightCandidates));

            if (!state.cachedSpine)
            {
                state.cachedSpine = FindBoneFuzzy(root, "spn2");
                if (!state.cachedSpine)
                    state.cachedSpine = FindBoneFuzzy(root, "spn1");
            }

            if (!state.cachedEyeL)
            {
                state.cachedEyeL = FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "l eye");
                if (!state.cachedEyeL)
                    state.cachedEyeL = FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "eye_l");
                if (!state.cachedEyeL)
                    state.cachedEyeL = FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "eyeleft");
            }

            if (!state.cachedEyeR)
            {
                state.cachedEyeR = FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "r eye");
                if (!state.cachedEyeR)
                    state.cachedEyeR = FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "eye_r");
                if (!state.cachedEyeR)
                    state.cachedEyeR = FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "eyeright");
            }

            state.skeletonResolved = true;
        }

        auto *spine = state.cachedSpine;
        auto *neck = state.cachedNeck;
        auto *head = state.cachedHead;
        auto *eyeL = state.cachedEyeL;
        auto *eyeR = state.cachedEyeR;

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

            // Phase S2 rig capability matrix: classify the visual-origin mode so a
            // reader can tell an eye-node rig from a vanilla FaceGen rig, and so an
            // absent eye node is never mistaken for a defect. The head socket is the
            // documented, first-class fallback when a rig exposes no eye bones.
            const bool hasEyeNode = (eyeL != nullptr || eyeR != nullptr);
            const char *originMode = head ? (hasEyeNode ? "EyeNode" : "GeometricHeadSocket")
                                          : "Unavailable";

            // Record the outcome for the stgstatus rig-capability summary.
            RecordRigProbe(originMode, head != nullptr, hasEyeNode);

            logger::info("[TrueGaze] Skeleton probe for {:08X}: spine={} neck={} head={} "
                         "eyeL={} eyeR={} ({} of 5 resolved) origin={} humanoid={} player={}",
                         actor->GetFormID(),
                         spine ? "yes" : "NO", neck ? "yes" : "NO", head ? "yes" : "NO",
                         eyeL ? "yes" : "NO", eyeR ? "yes" : "NO", found,
                         originMode,
                         actor->IsHumanoid() ? "yes" : "no",
                         actor->IsPlayerRef() ? "yes" : "no");

            // Separate the two failure classes explicitly. "Eye nodes absent" is an
            // expected, supported vanilla-humanoid condition handled by the geometric
            // head socket. "Head anchor absent" is genuinely blocking.
            if (head && !hasEyeNode)
            {
                logger::info("[TrueGaze] Rig {:08X} exposes no eye nodes; using the "
                             "GeometricHeadSocket origin. This is expected on vanilla "
                             "humanoid rigs (eyes are FaceGen morphs), not an error.",
                             actor->GetFormID());
            }

            // The head is the one that absolutely must resolve; without it there is
            // no gaze to see. Be loud rather than let this pass as a quiet zero.
            if (!head)
            {
                logger::warn("[TrueGaze] Head anchor absent for {:08X} (origin=Unavailable). "
                             "Gaze cannot be visible for this actor. The bone-name "
                             "candidates in GazeEngine.cpp need extending for this rig.",
                             actor->GetFormID());
            }

            state.bonesReported = true;
        }

        const bool isPlayer = actor->IsPlayerRef();
        bool allowHeadtrack = !isPlayer;
        if (isPlayer)
        {
            auto *camera = RE::PlayerCamera::GetSingleton();
            allowHeadtrack = camera && camera->IsInThirdPerson();
        }

        if (!isPlayer && spine)
        {
            EyeAimConstraint::Apply(actor->GetFormID(), spine, strain.spineYaw, 0.0f);
        }

        if (allowHeadtrack && neck)
        {
            EyeAimConstraint::Apply(actor->GetFormID(), neck, strain.neckYaw, strain.neckPitch);
        }

        if (allowHeadtrack && head)
        {
            EyeAimConstraint::Apply(actor->GetFormID(), head, strain.headYaw, strain.headPitch);
        }

        // Eyes receive the low-inertia ballistic VOR counter-rotation and micro-jitter.
        if (eyeL)
        {
            EyeAimConstraint::Apply(actor->GetFormID(), eyeL, eyeYaw, eyePitch);
        }

        if (eyeR)
        {
            EyeAimConstraint::Apply(actor->GetFormID(), eyeR, eyeYaw, eyePitch);
        }

        // Eyelid morph writes and biological eye-direction morphs (EFA / EFM / Vanilla)
        Integrations::EfmBlinkController::ApplyGazeMorphs(actor->GetFormID(),
                                                          state.blink.eyelidCloseWeight,
                                                          eyeYaw, eyePitch);

        // In-game 3D visualisation of the solved gaze. Runs after the skeleton is
        // posed so the head bone's world transform is current, and is driven by the
        // *eye residual* - what the eyes carry beyond the already-turned head. Passing
        // the total deflection here would double-count the head's share and the beam
        // would overshoot the true gaze direction.
        try
        {
            Visuals::VisualEffectsManager::Get().UpdateActor(
                actor,
                state.cachedHead,
                state.cachedEyeL,
                state.cachedEyeR,
                eyeYaw,
                eyePitch,
                state.gazeRegion,
                isPlayer,
                actor->IsHumanoid());
        }
        catch (const std::exception &e)
        {
            logger::error("[TrueGaze] VisualEffectsManager::UpdateActor exception: {}", e.what());
        }
        catch (...)
        {
            logger::error("[TrueGaze] VisualEffectsManager::UpdateActor unknown exception.");
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

        if (_pipe.IsConnected())
        {
            Bridge::SkyrimFeedbackPacket feedback{};
            feedback.targetFormId = state.trackedTargetFormId;
            feedback.relationshipRank = 0;
            feedback.combatState = actor->IsInCombat() ? 1 : 0;
            auto *ui = RE::UI::GetSingleton();
            feedback.isDialogueActive = (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) ? 1 : 0;
            feedback.distanceToTarget = DistanceMetersForTier(actor);
            feedback.mutualGazeAngle = state.lastYawDeg;
            feedback.gameFrameNumber = 0;
            _pipe.SendFeedback(feedback);
        }
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
