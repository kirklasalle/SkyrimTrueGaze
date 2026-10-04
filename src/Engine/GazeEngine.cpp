#include "GazeEngine.hpp"
#include "BoneController.hpp"
#include "ConfigManager.hpp"
#include "EyeAimConstraint.hpp"
#include "GazeAnchors.hpp"
#include "GazeRegion.hpp"
#include "Integrations/OarConditions.hpp"
#include "LodManager.hpp"
#include "PerformanceProfiler.hpp"
#include "PlayerGazeResolver.hpp"
#include "TargetSelector.hpp"
#include "TraceLogger.hpp"
#include "Visuals/VisualEffectsManager.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/B/BSVisit.h>
#include <RE/C/ConsoleLog.h>
#include <RE/H/HighProcessData.h>
#include <RE/S/SendHUDMessage.h>
#include <RE/Skyrim.h>
#endif

#include <algorithm>
#include <cmath>

namespace TrueGaze::Engine
{

    namespace
    {

        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kRadToDeg = 180.0f / kPi;

        /// Skyrim world units per metre.
        constexpr float kUnitsPerMeter = 70.0f;

        /// Approximate eye height above an actor's origin, in Skyrim units.
        /// R14 E7.5 — canonical value now lives in GazeAnchors.hpp; this alias
        /// keeps the call sites below readable.
        constexpr float kEyeHeightUnits = TrueGaze::Anchors::kEyeHeightUnits;

        /// Skyrim world units per centimetre (70 units == 1 m).
        constexpr float kUnitsPerCm = TrueGaze::Anchors::kUnitsPerCm;

        /// Live eye-anchor offsets (head bone -> eyeline), in centimetres. Kept in
        /// sync with TargetSelector::s_eyeAnchor by GazeEngine::RefreshTuning so the
        /// observer's ray ORIGIN sits on the same eyeline as the target's eye anchor,
        /// making every solve genuinely eye-to-eye.
        float g_eyeAnchorForwardCm = 8.0f;
        float g_eyeAnchorUpCm = 8.5f;

        /// Wrap an angle in radians to (-pi, pi].
        float WrapPi(float radians) noexcept
        {
            while (radians > kPi)
                radians -= 2.0f * kPi;
            while (radians < -kPi)
                radians += 2.0f * kPi;
            return radians;
        }

        struct RegionCalibrationSpec
        {
            uint8_t regionId;
            const char* name;
            float yawDeg;
            float pitchDeg;
        };

        constexpr RegionCalibrationSpec kCalibrationSweepSpecs[] = {
            {0, "LeftEye", -1.8f, 0.5f},
            {1, "RightEye", 1.8f, 0.5f},
            {5, "Torso", 0.0f, -4.5f},
            {2, "Mouth", 5.5f, -4.5f},
            {3, "Forehead", 0.0f, 7.0f},
            {4, "Chin", 0.0f, -9.0f},
            {8, "Ground", 0.0f, -18.0f},
            {9, "ULPeripheral", -15.0f, 18.0f},
            {10, "URPeripheral", 15.0f, 18.0f},
            {11, "LLPeripheral", -15.0f, -18.0f},
            {12, "LRPeripheral", 15.0f, -18.0f}
        };

#if __has_include(<RE/Skyrim.h>)

        /// Bone names to try, in priority order. Different rigs (vanilla, XP32/XPMSSE, custom
        /// creature skeletons) name these differently. Standard Skyrim skeleton uses literal
        /// "NPC L Eye" and "NPC R Eye", and "NPC Spine2 [Spn2]".
        constexpr const char* kSpineCandidates[] = {"NPC Spine2 [Spn2]",
                                                    "NPC Spine1 [Spn1]",
                                                    "NPC Spine [Spn0]",
                                                    "NPC Spine2 [Spine2]",
                                                    "Spine2",
                                                    "NPC Spine1 [Spine1]",
                                                    "Spine"};
        constexpr const char* kNeckCandidates[] = {"NPC Neck [Neck]", "Neck", "Neck1"};
        constexpr const char* kHeadCandidates[] = {"NPC Head [Head]", "Head", "Head1"};
        constexpr const char* kEyeLeftCandidates[] = {
            "NPC L Eye", "NPC L Eye [LEye]", "NPC L Eye [L Eye]", "Eye_L", "EyeLeft", "L Eye",
            "LEye"};
        constexpr const char* kEyeRightCandidates[] = {
            "NPC R Eye", "NPC R Eye [REye]", "NPC R Eye [R Eye]", "Eye_R", "EyeRight", "R Eye",
            "REye"};

        RE::NiAVObject* FindFirstBone(RE::NiAVObject* root, const char* const* candidates,
                                      size_t count) noexcept
        {
            if (!root)
            {
                return nullptr;
            }

            for (size_t i = 0; i < count; ++i)
            {
                if (auto* found = root->GetObjectByName(RE::BSFixedString(candidates[i])))
                {
                    return found;
                }
            }
            return nullptr;
        }

        RE::NiAVObject* FindBoneFuzzy(RE::NiAVObject* root, std::string_view a_needle) noexcept
        {
            if (!root)
            {
                return nullptr;
            }

            RE::NiAVObject* result = nullptr;
            RE::BSVisit::TraverseScenegraphObjects(
                root,
                [&](RE::NiAVObject* obj)
                {
                    if (obj && obj->name.c_str())
                    {
                        std::string s = obj->name.c_str();
                        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c)
                                       { return static_cast<char>(std::tolower(c)); });
                        if (s.find(a_needle) != std::string::npos)
                        {
                            result = obj;
                            return RE::BSVisit::BSVisitControl::kStop;
                        }
                    }
                    return RE::BSVisit::BSVisitControl::kContinue;
                });
            return result;
        }

        /// R14 E7.5 — EyeAnchorFromHeadBone moved to GazeAnchors.hpp (shared with
        /// TargetSelector.cpp, which previously carried a byte-identical copy).
        /// The observer's ray origin and the target's eye point are computed by
        /// the SAME function, so the eyeline is identical by construction.
        RE::NiPoint3 EyeAnchorFromHeadBone(const RE::NiAVObject* headBone) noexcept
        {
            return TrueGaze::Anchors::EyeAnchorFromHeadBone(headBone, g_eyeAnchorForwardCm,
                                                            g_eyeAnchorUpCm);
        }

        /// Convert a world-space target into actor-relative yaw/pitch, in degrees.
        /// observerEyePos is the world position of the observer's head/eyes.
        /// targetPos is the true 3D world position of the target's head/eyes.
        void WorldTargetToLocalGaze(const RE::NiPoint3& observerEyePos, float actorYawRad,
                                    const RE::NiPoint3& targetPos, float& outYawDeg,
                                    float& outPitchDeg) noexcept
        {
            const float dx = targetPos.x - observerEyePos.x;
            const float dy = targetPos.y - observerEyePos.y;
            const float dz = targetPos.z - observerEyePos.z;

            const float horizontal = std::sqrt(dx * dx + dy * dy);

            // R14 E7.4 — point-blank gate aligned with TargetSelector's
            // IsInVisualCone singularity guard (distSq < 16.0f, i.e. 4 units).
            // The old 25-unit gate zeroed gaze for targets up to 0.35 m away,
            // which contradicted the selector treating anything inside 4 units
            // as "always in cone" — an NPC standing 20 units away was selected
            // but then forced to stare dead ahead. 4 units is the smallest
            // distance at which atan2(dx, dy) is still numerically stable.
            if (horizontal < 4.0f)
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
        ///
        /// Region IDs (from HCEP-02 Enhanced Diagram):
        ///  0 = LeftEye       1 = RightEye      2 = Mouth        3 = Forehead
        ///  4 = Chin           5 = Torso          6 = RightHand    7 = LeftHand
        ///  8 = Ground         9 = UpperLeftPeripheral (CGA: positivity/hope)
        /// 10 = UpperRightPeripheral (CGA: memory/constructive thought)
        /// 11 = LowerLeftPeripheral (CGA: tiredness/negativity/sadness)
        /// 12 = LowerRightPeripheral (CGA: shyness/fear/deception)
        ///
        /// Implementation lives in GazeRegion.hpp (shared with the renderer,
        /// the panel hit detector, and tests/GeometryTests.cpp).
        uint8_t ClassifyRegion(float yawDeg, float pitchDeg) noexcept
        {
            return ClassifyGazeRegion(yawDeg, pitchDeg);
        }

#endif // __has_include(<RE/Skyrim.h>)

    } // namespace

    // ---------------------------------------------------------------------------
    // Lifecycle
    // ---------------------------------------------------------------------------

    void GazeEngine::RefreshTuning() noexcept
    {
        const auto& cfg = ConfigManager::GetSingleton();

        // Data minimisation (issue #7). Live: takes effect from the next frame.
        _pipe.SetRetainTrackedPersonId(cfg.retainTrackedPersonId);

        _tuning.enableTrueGaze = cfg.enableTrueGaze;
        _tuning.enableCreatures = cfg.enableCreatures;
        _tuning.debugGazeRays = cfg.debugGazeRays;
        _tuning.enableCharacterProfiles = cfg.enableCharacterProfiles;
        _tuning.enableCategoryProfiles = cfg.enableCategoryProfiles;

        _tuning.saccadeSpeedMult = cfg.saccadeSpeedMult;
        _tuning.velocitySaturation = cfg.velocitySaturation;
        _tuning.microJitterAmp = cfg.microJitterAmp;
        _tuning.microJitterIntervalMin = cfg.microJitterIntervalMin;
        _tuning.microJitterIntervalMax = cfg.microJitterIntervalMax;
        _tuning.headTrackingSpeed = cfg.headTrackingSpeed;
        _tuning.eyePursuitSpeed = cfg.eyePursuitSpeed;
        _tuning.maxComfortEyeAngle = cfg.maxComfortEyeAngle;
        _tuning.headOnsetDelaySec = cfg.headOnsetDelaySec;

        _tuning.spine2YawWeight = cfg.spine2YawWeight;
        _tuning.neckYawWeight = cfg.neckYawWeight;
        _tuning.neckPitchWeight = cfg.neckPitchWeight;
        _tuning.headYawWeight = cfg.headYawWeight;
        _tuning.headPitchWeight = cfg.headPitchWeight;
        _tuning.headEngageThresholdDeg = cfg.headEngageThresholdDeg;

        _tuning.eyeAnchorForwardCm = cfg.eyeAnchorForwardCm;
        _tuning.eyeAnchorUpCm = cfg.eyeAnchorUpCm;
        _tuning.eyeMorphGain = cfg.eyeMorphGain;
        _tuning.eyeMorphFullScaleDeg = cfg.eyeMorphFullScaleDeg;

        // Eye-anchor offsets feed BOTH the target side (TargetSelector's static
        // snapshot) and the observer's ray origin (the file-scope globals used by
        // EyeAnchorFromHeadBone in this translation unit). Keeping them identical is
        // what makes the solve genuinely eye-to-eye.
        TargetSelector::s_eyeAnchor.forwardCm = cfg.eyeAnchorForwardCm;
        TargetSelector::s_eyeAnchor.upCm = cfg.eyeAnchorUpCm;
        g_eyeAnchorForwardCm = cfg.eyeAnchorForwardCm;
        g_eyeAnchorUpCm = cfg.eyeAnchorUpCm;

        _tuning.enableGazeAversion = cfg.enableGazeAversion;
        _tuning.enableSocialTriangle = cfg.enableSocialTriangle;
        _tuning.triangleFixationDuration = cfg.triangleFixationDuration;
        _tuning.mutualGazeThreshold = cfg.mutualGazeThreshold;
        _tuning.trianglePathRandomness = cfg.trianglePathRandomness;
        _tuning.cgaHeadInvolvement = cfg.cgaHeadInvolvement;
        _tuning.dialogueSyncCgaReturn = cfg.dialogueSyncCgaReturn;
        _tuning.cgaDialogueOffsetSec = cfg.cgaDialogueOffsetSec;

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
            vis.arrowCrossSectionMm = cfg.arrowCrossSectionMm;
            vis.showHcepPanel = cfg.showHcepPanel;
            vis.hcepPanelAllActors = cfg.hcepPanelAllActors;
            vis.hcepPanelScale = cfg.hcepPanelScale;
            vis.hcepPanelForwardOffsetCm = cfg.hcepPanelForwardOffsetCm;
            Visuals::VisualEffectsManager::Get().SetTuning(vis);
        }

        logger::info("[TrueGaze] Tuning refreshed: saccadeMult={:.2f} jitter={:.2f} "
                     "headSpeed={:.2f} eyeMax={:.1f} crosshair={}",
                     _tuning.saccadeSpeedMult, _tuning.microJitterAmp, _tuning.headTrackingSpeed,
                     _tuning.maxComfortEyeAngle, cfg.enableCrosshairGaze ? "on" : "off");
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

        const auto& cfg = ConfigManager::GetSingleton();
        _pipe.SetRetainTrackedPersonId(cfg.retainTrackedPersonId);
        _pipe.Start(cfg.pipeName.c_str(), cfg.autoReconnectIntervalSec);
        _bridgeStarted = true;
    }

    void GazeEngine::StopBridge(Bridge::NamedPipeServer::JoinPolicy policy) noexcept
    {
        if (!_bridgeStarted)
        {
            return;
        }

        _pipe.Stop(policy);
        _bridgeStarted = false;
    }

    void GazeEngine::ResetAll() noexcept
    {
        EyeAimConstraint::Reset();
        Visuals::VisualEffectsManager::Get().Reset();
        _actors.clear();
        logger::info("[TrueGaze] Actor gaze state cleared.");
    }

    void GazeEngine::SetCalibrationOverride(bool active, float yawDeg, float pitchDeg,
                                            uint32_t targetActorFormId) noexcept
    {
        _calState.active = active;
        _calState.sweepActive = false;
        _calState.overrideYawDeg = yawDeg;
        _calState.overridePitchDeg = pitchDeg;
        _calState.targetActorFormId = targetActorFormId;
        _calState.sweepTimerSec = 0.0f;
    }

    void GazeEngine::StartCalibrationSweep(uint32_t targetActorFormId) noexcept
    {
        _calState.active = true;
        _calState.sweepActive = true;
        _calState.targetActorFormId = targetActorFormId;
        _calState.currentRegionIndex = 0;
        _calState.sweepTimerSec = 0.0f;
        _calState.sweepSettledFrames = 0;
        _calState.sweepAgreedFrames = 0;
        _calState.sweepMismatchFrames = 0;
        _calState.overrideYawDeg = kCalibrationSweepSpecs[0].yawDeg;
        _calState.overridePitchDeg = kCalibrationSweepSpecs[0].pitchDeg;
        logger::info("[TrueGaze::Cal] Started calibration sweep across {} regions.",
                     std::size(kCalibrationSweepSpecs));
    }

    void GazeEngine::StepCalibrationNext(uint32_t targetActorFormId) noexcept
    {
        if (!_calState.active || _calState.sweepActive)
        {
            _calState.active = true;
            _calState.sweepActive = false;
            _calState.currentRegionIndex = 0;
        }
        else
        {
            _calState.currentRegionIndex =
                (_calState.currentRegionIndex + 1) % static_cast<int>(std::size(kCalibrationSweepSpecs));
        }
        _calState.targetActorFormId = targetActorFormId;
        _calState.overrideYawDeg = kCalibrationSweepSpecs[_calState.currentRegionIndex].yawDeg;
        _calState.overridePitchDeg = kCalibrationSweepSpecs[_calState.currentRegionIndex].pitchDeg;
    }

    void GazeEngine::StopCalibration() noexcept
    {
        _calState.active = false;
        _calState.sweepActive = false;
        _calState.overrideYawDeg = 0.0f;
        _calState.overridePitchDeg = 0.0f;
        _calState.sweepTimerSec = 0.0f;
        logger::info("[TrueGaze::Cal] Calibration override stopped.");
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
        //
        // R14 E2.1: the profiler is now real and this check uses its budget
        // constant. The previous threshold (1500 us) disagreed with the
        // documented 150 us budget by 10x; both now derive from
        // PerformanceProfiler::kFrameBudgetUs so they can never diverge again.
        // The over-budget multiplier (10x) keeps the warn threshold conservative
        // The over-budget multiplier (10x) keeps the warn threshold conservative
        // until E2.4's benchmark artifacts establish the real distribution.
        PerformanceProfiler::RecordFrame(_lastFrameUs);
        if (_lastFrameUs > PerformanceProfiler::kFrameBudgetUs * 10)
        {
            static std::chrono::steady_clock::time_point s_lastBudgetWarning{};
            const auto now = std::chrono::steady_clock::now();
            if (now - s_lastBudgetWarning > std::chrono::seconds(10))
            {
                s_lastBudgetWarning = now;
                logger::warn(
                    "[TrueGaze] Frame budget exceeded: {} us (budget {} us) across {} actors.",
                    _lastFrameUs, PerformanceProfiler::BudgetMicros(), _actors.size());
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

    void GazeEngine::TickActor(RE::Actor* actor, float deltaSeconds) noexcept
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

        auto* root = actor->Get3D();
        if (!root)
        {
            return;
        }

        if (_eligibleTicks == 1)
        {
            logger::info("[TrueGaze] Eligible actor tick: form={:08X} player={} humanoid={}",
                         formId, actor->IsPlayerRef() ? "yes" : "no",
                         actor->IsHumanoid() ? "yes" : "no");
        }

        // --- Resolve or create per-actor state ----------------------------------
        auto [it, inserted] = _actors.try_emplace(formId);
        if (inserted && _actors.size() > MAX_TRACKED_ACTORS)
        {
            _actors.erase(it);
            return;
        }

        ActorGazeRuntime& state = it->second;

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
            state.Reset(0.0f, 0.0f);
            state.rngSeed = formId;
            Kinematics::MicroJitter::Init(state.jitter, formId, _tuning.microJitterAmp);
            // Seed the scanpath's base fixation duration from the INI once.
            // The scanpath owns its durations from here on (it multiplies each
            // draw by cadenceScale); the engine no longer stomps this value
            // every frame — that stomp defeated the calm cadence lever.
            state.triangle.fixationDurationSec = _tuning.triangleFixationDuration;
        }

        // Keep the drift amplitude in step with configuration changes. The mean
        // reversion rate derives from the configured micro-correction interval:
        // corrections arrive on average every `interval` seconds, so drift is pulled
        // back at theta = 1/mean(interval). (0.2-0.45 s -> theta ~ 3.1/s, within the
        // physiological 2-8/s band for ocular drift correction.)
        state.jitter.amplitudeDeg = _tuning.microJitterAmp;
        {
            const float meanInterval =
                0.5f * (_tuning.microJitterIntervalMin + _tuning.microJitterIntervalMax);
            state.jitter.reversionRate =
                (meanInterval > 0.0f) ? (1.0f / meanInterval) : state.jitter.reversionRate;
        }
        // CALM/COMBAT SPEED MODEL (Kirk directive, September 26 2026).
        //
        // "Slowed down by at least 50% as a baseline. Faster speeds are for
        // combat scenarios and any high intense action." — then, after the
        // 15:37 field test: "move the baseline down at least another 50%."
        //
        // The tuned INI speeds are the COMBAT speeds. Away from combat the
        // engine runs the graceful baseline at ONE QUARTER of those speeds:
        // quarter the head-tracking rate, quarter the saccadic peak velocity,
        // quarter the ocular pursuit rate, quarter the cervical slew caps —
        // and (via 1/speedScale) 4x the saccade duration and 4x the scanpath
        // fixation cadence, so the eyes move slowly and dwell long. The moment
        // the actor enters combat, full tuned speed returns — fast eyes read
        // as alert and dangerous in a fight, and as frantic everywhere else.
        // Computed here (TickActor) so every consumer this frame — head
        // tracking sync below, saccades and pursuit in ComputeDeflection —
        // shares one decision. ComputeDeflection receives it as a parameter,
        // so the two sites can never disagree.
        const bool actorInCombat = actor->IsInCombat();
        const float speedScale = actorInCombat ? 1.0f : 0.25f;

        // CALM/COMBAT SPEED MODEL: the head chain tracks at a slightly FASTER
        // rate than the eyes when calm (Kirk fine-tuning, 2026-09-26: "the head
        // moves slow and gracefully but it's a little too slow — the head can
        // catch up to the eyes slightly faster"; then after round 8: "the head
        // still moves a little bit too slow, it could be increased by 10%").
        // 1.54x the calm eye rate (1.4 x 1.10) keeps the graceful S-curve while
        // --- LOD tiering (CULL BEFORE RESOLVE — R14 E2.2) ----------------------
        //
        // The distance check and cull decision happen BEFORE ComputeDeflection.
        // Previously this block sat after the calm/combat speed-model sync but
        // the target resolution happened inside ComputeDeflection — meaning a
        // culled actor still paid the full TargetSelector cost (a full
        // process-list scan with bone lookups per candidate) on the frame it
        // was culled. With N nearby actors that made the per-frame cost
        // O(N^2) in crowd scenes: 50 culled actors each paid a 50-candidate
        // scan they never used. Culling first means culled actors never enter
        // the solve at all.
        const auto* player = RE::PlayerCharacter::GetSingleton();
        float distanceMeters = 0.0f;

        if (player)
        {
            const float units = actor->GetPosition().GetDistance(player->GetPosition());
            distanceMeters = units / kUnitsPerMeter;
        }

        const auto tier = LodManager::GetLodTier(distanceMeters, _tuning.tier1DistanceMeters,
                                                 _tuning.tier2DistanceMeters);

        if (tier == LodManager::LodTier::Tier3_Culled)
        {
            // Beyond 15 m the eyes are sub-pixel and the engine's own LOD applies.
            // Early-out BEFORE any speed-model or target-resolution work.
            ++_culledTicks;
            return;
        }

        // --- Compute and apply ---------------------------------------------------
        float yawDeg = 0.0f;
        float pitchDeg = 0.0f;
        ComputeDeflection(actor, state, deltaSeconds, yawDeg, pitchDeg, speedScale, actorInCombat);

        // --- Calibration Mode Override (A8 / Technical Design §8) ---
        if (_calState.active && (_calState.targetActorFormId == 0 || _calState.targetActorFormId == formId))
        {
            yawDeg = _calState.overrideYawDeg;
            pitchDeg = _calState.overridePitchDeg;
            state.saccade.currentYaw = yawDeg;
            state.saccade.currentPitch = pitchDeg;
            state.saccade.isBallistic = false;
            state.jitter.currentYawOffset = 0.0f;
            state.jitter.currentPitchOffset = 0.0f;

            if (_calState.sweepActive)
            {
                _calState.sweepTimerSec += deltaSeconds;
                if (_calState.sweepTimerSec >= 0.2f)
                {
                    const auto hit = Visuals::VisualEffectsManager::Get().GetActorHitInfo(formId);
                    if (hit.hit)
                    {
                        ++_calState.sweepSettledFrames;
                        if (hit.agreement)
                        {
                            ++_calState.sweepAgreedFrames;
                        }
                        else
                        {
                            ++_calState.sweepMismatchFrames;
                        }
                    }
                }

                if (_calState.sweepTimerSec >= 2.0f)
                {
                    _calState.sweepTimerSec = 0.0f;
                    ++_calState.currentRegionIndex;
                    if (_calState.currentRegionIndex < static_cast<int>(std::size(kCalibrationSweepSpecs)))
                    {
                        _calState.overrideYawDeg = kCalibrationSweepSpecs[_calState.currentRegionIndex].yawDeg;
                        _calState.overridePitchDeg = kCalibrationSweepSpecs[_calState.currentRegionIndex].pitchDeg;
                        logger::info("[TrueGaze::Cal] Sweep advancing to [{}/{}]: {} (yaw={:+.1f}, pitch={:+.1f})",
                                     _calState.currentRegionIndex + 1, std::size(kCalibrationSweepSpecs),
                                     kCalibrationSweepSpecs[_calState.currentRegionIndex].name,
                                     _calState.overrideYawDeg, _calState.overridePitchDeg);
                    }
                    else
                    {
                        const float agreementPct = _calState.sweepSettledFrames > 0
                            ? (static_cast<float>(_calState.sweepAgreedFrames) / _calState.sweepSettledFrames) * 100.0f
                            : 100.0f;
                        logger::info("[TrueGaze::Cal] Sweep COMPLETE: {} settled frames, {} agreed ({:.1f}%), {} mismatches",
                                     _calState.sweepSettledFrames, _calState.sweepAgreedFrames, agreementPct,
                                     _calState.sweepMismatchFrames);
                        StopCalibration();
                    }
                }
            }
        }

        // Gold Standard defer diagnostics: record the defer decision made by
        // this frame's target resolution so stgstatus can show it.
        _lastDeferActive = state.sceneDeferActive;
        if (state.sceneDeferActive)
        {
            ++_deferFrames;
        }

        state.lastYawDeg = yawDeg;
        state.lastPitchDeg = pitchDeg;
        state.gazeRegion = ClassifyRegion(yawDeg, pitchDeg);
        _gameTimeSec += deltaSeconds;

        // Trace Telemetry: Saccade onset and completion
        if (state.saccade.isBallistic && !state.wasInBallisticSaccade)
        {
            state.wasInBallisticSaccade = true;
            state.saccadeFromRegion = state.gazeRegion;
            TraceLogger::Get().LogSaccadeOnset(
                _gameTimeSec, formId, state.gazeRegion,
                static_cast<uint8_t>(state.triangle.currentVertex),
                state.saccade.amplitudeDeg, state.saccade.totalDurationSec);
        }
        else if (!state.saccade.isBallistic && state.wasInBallisticSaccade)
        {
            state.wasInBallisticSaccade = false;
            TraceLogger::Get().LogSaccadeComplete(
                _gameTimeSec, formId, state.gazeRegion,
                state.saccade.elapsedSec, 0.0f);
        }

        // Trace Telemetry: Debounced region transitions
        if (state.lastClassifiedRegion != state.gazeRegion)
        {
            if (state.lastClassifiedRegion != 0xFF)
            {
                TraceLogger::Get().LogRegionChange(
                    _gameTimeSec, formId, state.lastClassifiedRegion, state.gazeRegion,
                    state.currentRegionDwellSec, RegionName(state.lastClassifiedRegion),
                    RegionName(state.gazeRegion));
            }
            state.lastClassifiedRegion = state.gazeRegion;
            state.currentRegionDwellSec = 0.0f;
        }
        else
        {
            state.currentRegionDwellSec += deltaSeconds;
        }

        // Trace Telemetry: Controlled Gaze Aversion (CGA) enter / exit
        if (state.cgaActive && !state.wasInCga)
        {
            state.wasInCga = true;
            state.cgaDurationSec = 0.0f;
            const char* trigger = (state.mutualGazeHoldSec >= _tuning.mutualGazeThreshold)
                                      ? "MUTUAL_THRESHOLD"
                                      : "COGNITIVE_BURDEN";
            const uint8_t vert = static_cast<uint8_t>(state.triangle.currentVertex);
            const uint8_t quadrant = (vert == 5 ? 9 : (vert == 6 ? 10 : (vert == 7 ? 11 : (vert == 8 ? 12 : 9))));
            TraceLogger::Get().LogCgaEnter(_gameTimeSec, formId, trigger, quadrant,
                                           RegionName(quadrant));
        }
        else if (!state.cgaActive && state.wasInCga)
        {
            state.wasInCga = false;
            const char* trigger = _tuning.dialogueSyncCgaReturn ? "DIALOGUE_SYNC" : "TIMEOUT";
            TraceLogger::Get().LogCgaExit(_gameTimeSec, formId, state.gazeRegion,
                                          state.cgaDurationSec, trigger);
        }
        if (state.cgaActive)
        {
            state.cgaDurationSec += deltaSeconds;
        }

        // Trace Telemetry: Blink onset
        if (state.blink.isBlinking && !state.wasBlinking)
        {
            state.wasBlinking = true;
            TraceLogger::Get().LogBlink(_gameTimeSec, formId,
                                        state.blink.blinkTotalDurationSec * 1000.0f,
                                        state.timeSinceLastBlink);
            state.timeSinceLastBlink = 0.0f;
        }
        else if (!state.blink.isBlinking)
        {
            state.wasBlinking = false;
            state.timeSinceLastBlink += deltaSeconds;
        }

        if (_tuning.debugGazeRays)
        {
            state.rayDebugTimerSec += deltaSeconds;
            if (state.rayDebugTimerSec >= 1.0f)
            {
                state.rayDebugTimerSec = 0.0f;
                const auto pos = actor->GetPosition();
                const char* actorType = actor->IsPlayerRef() ? "Player" : "NPC";

                logger::info("[TrueGaze::Ray] Actor {:08X} ({}) at ({:.1f}, {:.1f}, {:.1f}) -> "
                             "Gaze Yaw={:+.1f}deg Pitch={:+.1f}deg Region={} (HCEP Mode={})",
                             formId, actorType, pos.x, pos.y, pos.z, yawDeg, pitchDeg,
                             state.gazeRegion, state.hcepMode);

                if (auto* console = RE::ConsoleLog::GetSingleton())
                {
                    console->Print("[TrueGaze::Ray] %08X (%s) -> Yaw:%+.1f deg Pitch:%+.1f deg "
                                   "Region:%d (Mode:%d)",
                                   formId, actorType, yawDeg, pitchDeg, state.gazeRegion,
                                   state.hcepMode);
                }

                if (actor->IsPlayerRef() && (std::abs(yawDeg) > 1.5f || std::abs(pitchDeg) > 1.5f))
                {
                    char hudBuf[128];
                    std::snprintf(hudBuf, sizeof(hudBuf),
                                  "[TrueGaze] 3rd-Person Gaze: Yaw %+.1f deg | Pitch %+.1f deg",
                                  yawDeg, pitchDeg);
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
        // SCENE DEFER: never cleared while deferring — vanilla scene direction owns
        // the head chain during directed segments.
        if (!actor->IsPlayerRef() && state.trackedTargetFormId != 0 && !state.sceneDeferActive)
        {
            if (auto* high = actor->GetHighProcess())
            {
                for (std::uint32_t i = 0; i < RE::HighProcessData::HEAD_TRACK_TYPE::kTotal; ++i)
                {
                    high->ClearHeadtrackTarget(static_cast<RE::HighProcessData::HEAD_TRACK_TYPE>(i),
                                               false);
                }
            }
        }

        ApplyToSkeleton(actor, state, yawDeg, pitchDeg);

        // Structured JSONL Trace Gaze Tick and Mismatch
        const auto hitInfo = Visuals::VisualEffectsManager::Get().GetActorHitInfo(formId);
        const uint8_t hitRegion = hitInfo.hit ? hitInfo.hitRegion : 255;
        const float eyeYaw = state.saccade.currentYaw + state.jitter.currentYawOffset;
        const float eyePitch = state.saccade.currentPitch + state.jitter.currentPitchOffset;
        const float totalDeflection = std::sqrt(yawDeg * yawDeg + pitchDeg * pitchDeg);
        const float eyeDeflection = std::sqrt(eyeYaw * eyeYaw + eyePitch * eyePitch);
        const float headPct =
            (totalDeflection > 0.001f)
                ? std::clamp(1.0f - (eyeDeflection / totalDeflection), 0.0f, 1.0f)
                : 0.0f;

        TraceLogger::Get().LogGazeTick(
            _gameTimeSec, formId, state.trackedTargetFormId, yawDeg, pitchDeg, eyeYaw,
            eyePitch, state.gazeRegion, hitRegion, ModeName(state.hcepMode),
            actor->IsPlayerRef() ? 0 : 0, state.mutualGazeHoldSec > 0.0f,
            state.mutualGazeHoldSec, headPct, deltaSeconds);

        if (hitInfo.hit && !hitInfo.agreement && !state.saccade.isBallistic &&
            !state.sceneDeferActive)
        {
            TraceLogger::Get().LogHitMismatch(_gameTimeSec, formId, state.gazeRegion,
                                              hitInfo.hitRegion, eyeYaw, eyePitch,
                                              hitInfo.hitX, hitInfo.hitZ, "BOUNDARY");
        }

        if (_calState.active && (_calState.targetActorFormId == 0 || _calState.targetActorFormId == formId))
        {
            TraceLogger::Get().LogCalibration(
                _gameTimeSec, _calState.sweepActive ? "sweep" : "step", state.gazeRegion,
                yawDeg, pitchDeg, state.gazeRegion, hitRegion,
                hitInfo.hit ? hitInfo.agreement : false);
        }

        PublishState(actor, state);
#else
        (void)actor;
        (void)deltaSeconds;
#endif
    }

    void GazeEngine::ComputeDeflection(RE::Actor* actor, ActorGazeRuntime& state,
                                       float deltaSeconds, float& outYaw, float& outPitch,
                                       float speedScale, bool actorInCombat) noexcept
    {
        outYaw = 0.0f;
        outPitch = 0.0f;

#if __has_include(<RE/Skyrim.h>)
        // --- Player attention input --------------------------------------------
        Bridge::TrueGazeTelemetryPacket hcepPacket{};
        const bool hasHcepTelemetry =
            _pipe.IsConnected() && _pipe.TryGetLatestTelemetry(hcepPacket);
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
                         "distance={:.2f}m actor={:08X}{}",
                         _targetResolutions, static_cast<unsigned>(target.priority),
                         target.targetFormId, target.distanceMeters, actor->GetFormID(),
                         state.sceneDeferActive ? " scene=DEFERRED" : "");
        }

        float desiredYaw = 0.0f;
        float desiredPitch = 0.0f;

        // CALM/COMBAT SPEED MODEL: speedScale was computed in TickActor (the
        // single decision point for this frame — see the full rationale there).
        // Consumers below: saccadic peak velocity, ocular pursuit glide rate,
        // and the catch-up saccade threshold.

        // EYES NEVER YIELD (Kirk directive, September 25 2026).
        //
        // During scene defer the HEAD CHAIN yields to vanilla scene direction, but
        // the EYES remain fully TrueGaze-controlled. A scene-directed NPC still
        // glances with living eyes even while the game directs their head.
        //
        // DEFER PLAYER GLANCE (Kirk observation, September 26 2026 — "directed
        // and scripted NPCs ignore the Player during the opening cart scene"):
        // while deferring, if the player is within social range and inside the
        // actor's wide visual cone, the EYES glance at the PLAYER rather than
        // freezing on the last resolved target. The head chain stays yielded —
        // vanilla scene direction still owns the head — but a person standing
        // an arm's length away is never ignored. This is the social minimum:
        // you may be directed at someone else, but your eyes still flick to
        // whoever is that close to you.
        if (target.priority == TargetSelector::TargetPriority::None && state.sceneDeferActive)
        {
            bool glancedAtPlayer = false;

            const auto* player = RE::PlayerCharacter::GetSingleton();
            if (player && player != actor && !actor->IsPlayerRef())
            {
                // Social glance window: 0.4 m .. 4 m (28..280 units). Closer than
                // 0.4 m the co-location singularity would zero the solve anyway;
                // beyond 4 m a glance would read as staring across the room.
                const float units = actor->GetPosition().GetDistance(player->GetPosition());
                if (units > 28.0f && units <= 280.0f)
                {
                    RE::NiPoint3 observerEyePos = actor->GetPosition();
                    if (state.cachedHead)
                    {
                        observerEyePos = EyeAnchorFromHeadBone(state.cachedHead);
                    }
                    else
                    {
                        observerEyePos.z += kEyeHeightUnits;
                    }

                    // The player's eye line, not their origin: the glance lands
                    // on the face, the same eye-to-eye standard as every other
                    // target in the engine.
                    RE::NiPoint3 playerEye = player->GetPosition();
                    if (auto* playerRoot = player->Get3D())
                    {
                        playerEye = RE::NiPoint3{playerRoot->world.translate.x,
                                                 playerRoot->world.translate.y,
                                                 playerRoot->world.translate.z + kEyeHeightUnits};
                    }
                    else
                    {
                        playerEye.z += kEyeHeightUnits;
                    }

                    float glanceYaw = 0.0f;
                    float glancePitch = 0.0f;
                    WorldTargetToLocalGaze(observerEyePos, actor->GetAngleZ(), playerEye, glanceYaw,
                                           glancePitch);

                    // Wide social cone (150 deg): the player slightly off to the
                    // side — or seated sideways in the Helgen cart, where body
                    // yaw points away from the player — still gets the glance.
                    // This is an EYES-ONLY movement (the head chain stays yielded
                    // to scene direction), so there is no neck fight to guard
                    // against; only someone fully behind the actor is excluded.
                    if (std::abs(glanceYaw) <= 150.0f)
                    {
                        desiredYaw = glanceYaw;
                        desiredPitch = glancePitch;
                        glancedAtPlayer = true;
                    }
                }
            }

            if (!glancedAtPlayer)
            {
                desiredYaw = state.lastYawDeg;
                desiredPitch = state.lastPitchDeg;
            }
        }
        else if (target.priority != TargetSelector::TargetPriority::None)
        {
            const RE::NiPoint3 targetPos{target.worldX, target.worldY, target.worldZ};
            RE::NiPoint3 observerEyePos = actor->GetPosition();
            if (state.cachedHead)
            {
                // Eye anchor, NOT the raw head-bone origin: the observer looks FROM
                // its own eyeline so the yaw/pitch solve is eye-to-eye rather than
                // throat-to-forehead. Uses the same projection as the target side.
                observerEyePos = EyeAnchorFromHeadBone(state.cachedHead);
            }
            else if (auto* root = actor->Get3D())
            {
                observerEyePos = RE::NiPoint3{root->world.translate.x, root->world.translate.y,
                                              root->world.translate.z + kEyeHeightUnits};
            }
            else
            {
                observerEyePos.z += kEyeHeightUnits;
            }
            WorldTargetToLocalGaze(observerEyePos, actor->GetAngleZ(), targetPos, desiredYaw,
                                   desiredPitch);
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
            auto* ui = RE::UI::GetSingleton();
            const bool inDialogue = ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);

            if (inDialogue)
            {
                // AFFECT during player dialogue: social triangle scanning.
                state.hcepMode = 1;
            }
            else if (actor->IsInCombat())
            {
                state.hcepMode = 0; // LOGIC: locked, analytical
            }
            else if (target.priority == TargetSelector::TargetPriority::DialoguePartner ||
                     target.priority == TargetSelector::TargetPriority::NearbyActor)
            {
                // AFFECT for ALL social interactions: NPC-to-NPC dialogue, scene
                // conversations, idle chatter, tavern mutual gaze, and any time an
                // actor is actively looking at another actor. This engages the social
                // triangle eye movement so NPCs look alive during conversation and
                // proximity encounters — not just during the player's DialogueMenu.
                state.hcepMode = 1;
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

            mutualGazeNow =
                PlayerGazeResolver::IsPlayerLookingAtFace(actor->GetFormID(), gazeParams);
        }

        if (mutualGazeNow)
        {
            ++_mutualGazeFrames;
            state.mutualGazeHoldSec += deltaSeconds;
            if (_tuning.debugGazeRays && state.mutualGazeHoldSec >= 0.5f &&
                state.mutualGazeHoldSec - deltaSeconds < 0.5f)
            {
                const char* actorName = actor->GetName();
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

        // --- Dialogue-Synced CGA Return -------------------------------------------
        //
        // Kirk LaSalle's insight: the TIMING of CGA return is the sweet spot for
        // human-like interaction. Two modes:
        //
        //   1. PRECISE: CGA aversion ends and gaze snaps to the speaker's face at
        //      the exact moment dialogue begins — the "oh, they said something"
        //      natural attention capture.
        //
        //   2. OFFSET: a per-actor random ±1-3 second offset so NPCs don't all
        //      react identically. Some return slightly before (anticipatory — they
        //      sensed the speaker was about to talk), some after (delayed cognitive
        //      processing — they were deep in thought).
        //
        // The offset is randomised once per CGA episode (when the NPC enters THINK
        // mode), not every frame, so each NPC has a consistent personality.
        {
            auto* ui2 = RE::UI::GetSingleton();
            const bool dialogueNow = ui2 && ui2->IsMenuOpen(RE::DialogueMenu::MENU_NAME);

            // Also detect NPC-to-NPC dialogue via the target selector.
            const bool actorInDialogueScene =
                target.priority == TargetSelector::TargetPriority::DialoguePartner;

            const bool dialogueActive = dialogueNow || actorInDialogueScene;

            if (_tuning.dialogueSyncCgaReturn && state.cgaActive && dialogueActive)
            {
                // Dialogue just started (or is active) while NPC is in CGA aversion.
                // Should we return now, or is the offset timer still counting?
                //
                // If offset is <= 0, return immediately (precise sync or timer expired).
                // If offset > 0, count down the offset before returning (delayed).
                // If offset < 0, the NPC already returned anticipatorily before dialogue
                // began — the negative offset was consumed when it first ticked negative.
                if (state.cgaDialogueReturnOffsetSec <= 0.0f)
                {
                    const float faceDist = std::max(0.5f, target.distanceMeters);
                    const bool wasAverting =
                        Kinematics::SocialTriangle::ReturnToFace(state.triangle, faceDist);
                    state.cgaActive = false;

                    if (wasAverting)
                    {
                        logger::info("[TrueGaze] CGA dialogue-sync return: actor {:08X} "
                                     "snapped gaze to speaker (offset={:.2f}s).",
                                     actor->GetFormID(), state.cgaDialogueReturnOffsetSec);
                    }
                }
                else
                {
                    // Counting down the positive offset (delayed return).
                    state.cgaDialogueReturnOffsetSec -= deltaSeconds;
                }
            }

            // Anticipatory return: negative offset means the NPC returns BEFORE
            // dialogue starts. Count the offset toward zero even without dialogue.
            if (_tuning.dialogueSyncCgaReturn && state.cgaActive &&
                state.cgaDialogueReturnOffsetSec < 0.0f && !dialogueActive)
            {
                state.cgaDialogueReturnOffsetSec += deltaSeconds;
                if (state.cgaDialogueReturnOffsetSec >= 0.0f)
                {
                    // Anticipatory timer expired — return to face now.
                    const float faceDist = std::max(0.5f, target.distanceMeters);
                    Kinematics::SocialTriangle::ReturnToFace(state.triangle, faceDist);
                    state.cgaActive = false;
                    state.cgaDialogueReturnOffsetSec = 0.0f;

                    logger::info("[TrueGaze] CGA anticipatory return: actor {:08X} "
                                 "returned gaze before dialogue.",
                                 actor->GetFormID());
                }
            }

            // Edge detect: when an NPC enters CGA (cgaActive transitions to true),
            // randomise the dialogue return offset for this episode.
            if (state.cgaActive && state.wasNotInDialogue == dialogueActive)
            {
                // Not an edge — no action needed.
            }
            state.wasNotInDialogue = !dialogueActive;
        }

        // --- Character Gaze Profile (temperament-driven gaze) --------------------
        //
        // WHO is looking: Bethesda's own characterization (Confidence, Aggression,
        // relationship, archetype flags, combat state) projected onto the HCEP-02
        // diagram as behavioural multipliers. Cached per actor; refreshed only on
        // combat edges (the cheapest reliable change signal) — AV/relationship
        // reads are not free. Default profile = all 1.0 multipliers = exact parity
        // with the pre-profile engine (additive-only contract).
        {
            const bool combatNow = actor->IsInCombat();

            // PLAYER BEHAVIOURAL PROFILE: the player's own attention feeds their
            // profile. When the player's crosshair holds on a face (they are
            // attending to someone), the accumulator rises; it decays otherwise.
            // An attentive player's 3rd-person gaze reads steadier and warmer —
            // the character reflects the player's behaviour. (1st person is
            // unaffected: eyes-only control, sacred invariant.)
            if (actor->IsPlayerRef())
            {
                const bool attending =
                    (target.priority == TargetSelector::TargetPriority::CrosshairFocus ||
                     target.priority == TargetSelector::TargetPriority::DialoguePartner);
                if (attending)
                {
                    state.playerAttentionSec =
                        std::min(30.0f, state.playerAttentionSec + deltaSeconds);
                }
                else
                {
                    state.playerAttentionSec =
                        std::max(0.0f, state.playerAttentionSec - deltaSeconds * 0.5f);
                }
            }

            if (!state.profileValid || state.profileWasInCombat != combatNow)
            {
                if (_tuning.enableCharacterProfiles)
                {
                    const auto* player = RE::PlayerCharacter::GetSingleton();
                    const auto input = GatherTemperament(actor, player);
                    // R15: resolve the category bundle from the live config and
                    // pass it into Classify. bEnableCategoryProfiles=false yields
                    // an all-neutral bundle = exact pre-R15 parity.
                    const auto bundle = CategoryBundleFor(input.category);
                    state.profile = CharacterProfile::Classify(input, true, bundle);
                }
                else
                {
                    // Profiles disabled: never touch the SDK adapter at all.
                    // Exact pre-profile behaviour (all 1.0 multipliers).
                    state.profile = CharacterProfile::Default();
                }
                state.profileValid = true;
                state.profileWasInCombat = combatNow;

                // Blend the player's behavioural attention into their own profile:
                // sustained face-attention (>= 5s accumulated) gradually warms the
                // player's gaze — longer mutual-gaze holds, slower scanning — the
                // same direction as a positive relationship, earned by behaviour.
                if (actor->IsPlayerRef() && _tuning.enableCharacterProfiles)
                {
                    const float warmth = std::clamp(state.playerAttentionSec / 30.0f, 0.0f, 1.0f);
                    if (warmth > 0.0f)
                    {
                        state.profile.mutualGazeThresholdMult *= (1.0f + 0.4f * warmth);
                        state.profile.fixationScaleMult *= (1.0f + 0.25f * warmth);
                        state.profile.modeBiasAffect += 0.3f * warmth;
                    }
                }
            }
            const auto& profile = state.profile;

            // Apply the profile multipliers to this frame's parameters.
            // (Fixation duration and triangle weights are applied below where the
            // scanpath runs; aversion rate scales the CGA entry probability.)
            _frame.fixationScale = profile.fixationScaleMult;
            _frame.triangleEnabled = profile.triangleEnabled;
            _frame.vertexWeights = profile.vertexWeights.data();
            _frame.cgaRoll = std::uniform_real_distribution<float>(0.0f, 1.0f)(state.triangle.rng);
        }

        // --- Social Triangle & Extended HCEP Diagram Scanpath --------------------
        //
        // Eyes must ALWAYS be moving when looking at another actor, regardless of
        // preset or HCEP mode. The scanpath varies by cognitive mode:
        //
        //   LOGIC (0):  Core social triangle (LeftEye ↔ RightEye ↔ Mouth)
        //   AFFECT (1): Core social triangle with empathetic dwell weighting
        //   SPIRIT (2): Extended diagram — triangle + Third-Eye (forehead) fixation
        //   HEART (3):  Extended diagram — triangle + Chest/Heart/Sternum fixation
        //   THINK (4):  Cognitive Gaze Aversion (CGA) — peripheral region scanning
        //               with brief face returns (HCEP-02 enhanced diagram pattern)
        //
        // Seed the triangle RNG from the actor's FormID once so each NPC has a
        // unique, stable scanpath that does not jitter in lockstep with others.
        if (!state.triangle.rngSeeded)
        {
            Kinematics::SocialTriangle::SeedRng(state.triangle, state.rngSeed);
        }

        if (target.priority >= TargetSelector::TargetPriority::NearbyActor)
        {
            const float faceDist = std::max(0.5f, target.distanceMeters);
            // CALM/COMBAT SPEED MODEL + CHARACTER PROFILE: the scanpath's
            // fixation cadence. cadenceScale multiplies every fixation-duration
            // draw inside SocialTriangle (1/speedScale: calm 0.5 -> 2.0x dwell,
            // combat 1.0 -> 1.0x; profile fixation scale folds in here too).
            //
            // FIXATION STOMP REMOVED (2026-09-26, "eyes are still too fast"):
            // this block previously ALSO overwrote state.triangle.
            // fixationDurationSec with the base value EVERY FRAME — destroying
            // the cadence-scaled duration the scanpath had just drawn and
            // resetting the countdown each frame. The cadence lever never got
            // to tick: the eyes re-jumped at the base rate no matter what the
            // speed model said. The scanpath now owns its durations; the base
            // value is seeded once at actor init (see the !initialised block
            // in TickActor).
            //
            // DWELL DIRECTIVE (Kirk, 2026-09-26): "how often the eyes look
            // around — they should look around LESS OFTEN so they can be
            // focused on the target eyes longer and more often before looking
            // around." Movement speed is now good (Kirk-verified), so the
            // remaining lever is DWELL: an extra calm-only 2x on top of the
            // speed model's 4x. At the base draw range 0.20-0.55 s the eyes
            // now hold each region ~1.6-4.4 s before moving on — roughly an
            // EIGHTH of the original look-around frequency. Combat restores
            // the biological cadence exactly.
            state.triangle.cadenceScale =
                (1.0f / speedScale) * (actorInCombat ? 1.0f : 2.0f) * _frame.fixationScale;

            // EYE-TO-EYE DOMINANCE (Kirk directive, 2026-09-26): "looking into
            // character eyes is important and must last longer before shifting,
            // and it MUST happen when dialogue occurs."
            //
            // Two tiers:
            //   Baseline 2.5x — a hold on LeftEye/RightEye outlasts every other
            //     region by that margin, everywhere, always. Eye contact is the
            //     socially meaningful state; mouth/chest/forehead visits are
            //     glances BETWEEN eye holds, not equals.
            //   Dialogue 4.0x — during the player's DialogueMenu OR an NPC-to-NPC
            //     conversation (DialoguePartner target), eye holds stretch
            //     further: sustained eye contact IS the dialogue contract.
            //     Combined with the calm cadence this yields ~6-17 s eye holds
            //     in a calm conversation — the eyes live on the speaker's eyes,
            //     visiting other regions only briefly between long eye holds.
            {
                auto* ui = RE::UI::GetSingleton();
                const bool inDialogue =
                    (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) ||
                    target.priority == TargetSelector::TargetPriority::DialoguePartner;

                state.triangle.eyeDwellScale = inDialogue ? 4.0f : 2.5f;
            }

            if (_tuning.enableGazeAversion && state.hcepMode == 4)
            {
                // THINK mode: Cognitive Gaze Aversion (CGA) using the full HCEP-02
                // enhanced diagram. Gaze breaks away to peripheral aversion regions
                // (upper-left, upper-right, lower-left, lower-right) with brief face
                // returns, modelling cognitive processing and the VOR counter-rotation
                // arc / saccade vectors from the diagram.
                // Character profile: aversion rate scales how often CGA engages —
                // cowardly NPCs avert far more than foolhardy guards.
                const bool cgaEngages = (_frame.cgaRoll < 0.0f) ||
                                        (_frame.cgaRoll < state.profile.aversionRateMult * 0.5f);
                if (cgaEngages)
                {
                    // On CGA activation edge, randomise the dialogue return offset.
                    if (!state.cgaActive)
                    {
                        // First frame of CGA — pick this NPC's dialogue-sync personality.
                        // Uniform distribution over [-offset, +offset] seconds:
                        //   negative = anticipatory (returns BEFORE dialogue)
                        //   positive = delayed (returns AFTER dialogue onset)
                        //   zero     = precise sync
                        const float maxOff = _tuning.cgaDialogueOffsetSec;
                        if (maxOff > 0.0f)
                        {
                            std::uniform_real_distribution<float> offDist(-maxOff, maxOff);
                            state.cgaDialogueReturnOffsetSec = offDist(state.triangle.rng);
                        }
                        else
                        {
                            state.cgaDialogueReturnOffsetSec = 0.0f;
                        }
                    }
                    state.cgaActive = true;
                    Kinematics::SocialTriangle::UpdateCGA(state.triangle, deltaSeconds, faceDist);
                }
                else
                {
                    // Profile suppressed CGA this cycle (low aversion rate):
                    // fall through to core triangle so eyes still live.
                    state.cgaActive = false;
                    if (_tuning.enableSocialTriangle && _frame.triangleEnabled)
                    {
                        Kinematics::SocialTriangle::Update(state.triangle, deltaSeconds, faceDist,
                                                           _tuning.trianglePathRandomness);
                    }
                }
            }
            else if (state.hcepMode == 2 || state.hcepMode == 3)
            {
                // SPIRIT / HEART: Extended diagram — social triangle interleaved with
                // Third-Eye (forehead / spiritual focus) or Chest (empathic resonance).
                state.cgaActive = false;
                Kinematics::SocialTriangle::UpdateExtended(state.triangle, deltaSeconds, faceDist,
                                                           state.hcepMode);
            }
            else
            {
                // LOGIC / AFFECT / any other mode: Core social triangle scanning.
                // Eyes cycle LeftEye ↔ RightEye ↔ Mouth continuously — the biological
                // baseline that makes NPCs look alive rather than staring with dead eyes.
                // Character profile: creatures (triangleEnabled=false) skip the
                // scanpath entirely — fixation-dominant animal attention.
                state.cgaActive = false;
                if (_tuning.enableSocialTriangle && _frame.triangleEnabled)
                {
                    // Profile-weighted vertex selection when a profile is active:
                    // weights shift visit probabilities across the HCEP-02 diagram
                    // (shy -> LowerRight, lover -> Chest, scholar -> ThirdEye).
                    if (_frame.vertexWeights)
                    {
                        Kinematics::SocialTriangle::UpdateWeighted(
                            state.triangle, deltaSeconds, faceDist, _tuning.trianglePathRandomness,
                            _frame.vertexWeights);
                    }
                    else
                    {
                        Kinematics::SocialTriangle::Update(state.triangle, deltaSeconds, faceDist,
                                                           _tuning.trianglePathRandomness);
                    }
                }
            }

            desiredYaw += state.triangle.vertexOffsetXDeg;
            desiredPitch += state.triangle.vertexOffsetYDeg;
        }

        // --- Target classification & saccade ------------------------------------
        const uint32_t targetFormId = target.targetFormId;
        if (state.trackedTargetFormId != targetFormId)
        {
            // Salience changed: commit to a new ballistic saccade.
            // CALM/COMBAT SPEED MODEL: saccadic peak velocity is halved away
            // from combat — a calm glance, not a whip — and full in combat.
            // DURATION is the real speed lever (the profile normalises to unit
            // area, so travel time = duration): calm stretches duration 2x =
            // half angular speed. 1/speedScale, NOT 2*speedScale — the latter
            // evaluates to 1.0 when calm and was the 15:37 build's no-op bug.
            // See SaccadeGenerator::CalculateDuration.
            state.trackedTargetFormId = targetFormId;
            ++_saccadesTriggered;

            Kinematics::SaccadeGenerator::TriggerSaccade(
                state.saccade, desiredYaw, desiredPitch,
                _tuning.EffectiveVMax(Kinematics::SaccadeGenerator::DEFAULT_VMAX) * speedScale,
                _tuning.velocitySaturation, 1.0f / speedScale);

            // A large saccade triggers a micro-blink (saccadic suppression).
            const bool wasBlinking = state.blink.isBlinking;
            Integrations::EfmBlinkController::OnSaccadeTriggered(state.blink,
                                                                 state.saccade.amplitudeDeg);
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

            // GRACEFUL REGION TRANSITIONS (Kirk observation, September 26 2026 —
            // "NPCs seem to have a snapping motion moving through regions").
            //
            // The old catch-up threshold (>20 deg) fired a BALLISTIC saccade for
            // large scanpath transitions — Chest→LeftEye at conversation range
            // spans ~15-25 deg, so every extended-diagram weave snapped. The
            // threshold is raised to 45 deg: only a genuine target teleport
            // (actor crossed the room) justifies a ballistic jump. Everything
            // inside 45 deg — every social-triangle vertex, every CGA aversion
            // region, every extended-diagram point — now flows through the
            // smooth-pursuit glide below: an exponential approach with zero
            // velocity discontinuity. In combat the threshold returns to 20 deg
            // so combat target acquisition stays sharp.
            const float catchUpThresholdSq = actorInCombat ? 400.0f : 2025.0f; // 20 / 45 deg

            if (diffDistSq > catchUpThresholdSq)
            {
                // Target made a major sudden jump while keeping same FormID: trigger catch-up
                // saccade CALM/COMBAT SPEED MODEL: same halving as the primary saccade.
                ++_saccadesTriggered;
                Kinematics::SaccadeGenerator::TriggerSaccade(
                    state.saccade, desiredYaw, desiredPitch,
                    _tuning.EffectiveVMax(Kinematics::SaccadeGenerator::DEFAULT_VMAX) * speedScale,
                    _tuning.velocitySaturation, 1.0f / speedScale);

                const bool wasBlinking = state.blink.isBlinking;
                Integrations::EfmBlinkController::OnSaccadeTriggered(state.blink,
                                                                     state.saccade.amplitudeDeg);
                if (!wasBlinking && state.blink.isBlinking)
                {
                    ++_blinksTriggered;
                }

                // Biological Latency Gap on major catch-up saccade
                state.vor.headOnsetDelayTimerSec = _tuning.headOnsetDelaySec;
            }
            else
            {
                // Smooth ocular pursuit: GLIDE toward the desired gaze instead of
                // teleporting. The hard assignment here (current = desired in one
                // frame) was the visible snap between social triangle fixations —
                // every new vertex repositioned the eyes instantly. An exponential
                // approach gives a fast but graceful glide, and because the head's
                // VOR target IS this eye angle, the head inherits the same smoothness.
                state.saccade.targetYaw = desiredYaw;
                state.saccade.targetPitch = desiredPitch;

                const float glideYaw = desiredYaw - state.saccade.currentYaw;
                const float glidePitch = desiredPitch - state.saccade.currentPitch;

                if (std::abs(glideYaw) < 0.02f && std::abs(glidePitch) < 0.02f)
                {
                    // Sub-perceptual remainder: settle exactly to kill endless crawling.
                    state.saccade.currentYaw = desiredYaw;
                    state.saccade.currentPitch = desiredPitch;
                }
                else
                {
                    // CALM/COMBAT SPEED MODEL: ocular pursuit glides at HALF the
                    // tuned rate away from combat — the region-to-region motion
                    // reads as a graceful drift, not a snap.
                    const float alpha =
                        1.0f - std::exp(-_tuning.eyePursuitSpeed * speedScale * deltaSeconds);
                    state.saccade.currentYaw += glideYaw * alpha;
                    state.saccade.currentPitch += glidePitch * alpha;
                }
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

    void GazeEngine::ApplyToSkeleton(RE::Actor* actor, ActorGazeRuntime& state, float yawDeg,
                                     float pitchDeg) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        auto* root = actor->Get3D();
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
        //
        // CGA EYE-DOMINANT MODE: when the actor is in CGA aversion (THINK mode,
        // gaze directed to peripheral regions), the head chain gets near-zero
        // involvement so the aversion is carried almost entirely by the eyes.
        // This prevents the grotesque Embry-style neck twist.
        const bool isCgaAversion = state.cgaActive && Kinematics::SocialTriangle::IsAversionVertex(
                                                          state.triangle.currentVertex);

        BoneController::StrainDistribution strain;
        if (isCgaAversion)
        {
            // Eyes-dominant aversion: head barely moves, eyes dart to peripheral region.
            strain = BoneController::CalculateCgaStrain(
                state.vor.headYaw, state.vor.headPitch, state.vor.eyeMaxAngle,
                state.vor.eyeMaxAngle, _tuning.cgaHeadInvolvement);
        }
        else
        {
            const BoneController::StrainWeights weights{
                _tuning.spine2YawWeight, _tuning.neckYawWeight, _tuning.neckPitchWeight,
                _tuning.headYawWeight, _tuning.headPitchWeight};
            strain = BoneController::CalculateHierarchyStrain(
                state.vor.headYaw, state.vor.headPitch, state.vor.eyeMaxAngle,
                state.vor.eyeMaxAngle, weights, _tuning.headEngageThresholdDeg);
        }

        const float jitterYaw = (DistanceMetersForTier(actor) <= _tuning.tier1DistanceMeters)
                                    ? state.jitter.currentYawOffset
                                    : 0.0f;
        const float jitterPitch = (DistanceMetersForTier(actor) <= _tuning.tier1DistanceMeters)
                                      ? state.jitter.currentPitchOffset
                                      : 0.0f;

        const float eyeYaw = std::clamp(state.vor.eyeLocalYaw + jitterYaw,
                                        -_tuning.maxComfortEyeAngle, _tuning.maxComfortEyeAngle);
        const float eyePitch = std::clamp(state.vor.eyeLocalPitch + jitterPitch,
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
            if (!state.cachedHead)
            {
                state.cachedHead = FindBoneFuzzy(root, "head");
            }
            state.cachedEyeL =
                FindFirstBone(root, kEyeLeftCandidates, std::size(kEyeLeftCandidates));
            state.cachedEyeR =
                FindFirstBone(root, kEyeRightCandidates, std::size(kEyeRightCandidates));

            if (!state.cachedSpine)
            {
                state.cachedSpine = FindBoneFuzzy(root, "spn2");
                if (!state.cachedSpine)
                    state.cachedSpine = FindBoneFuzzy(root, "spn1");
            }

            if (!state.cachedEyeL)
            {
                state.cachedEyeL =
                    FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "l eye");
                if (!state.cachedEyeL)
                    state.cachedEyeL =
                        FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "eye_l");
                if (!state.cachedEyeL)
                    state.cachedEyeL =
                        FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "eyeleft");
            }

            if (!state.cachedEyeR)
            {
                state.cachedEyeR =
                    FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "r eye");
                if (!state.cachedEyeR)
                    state.cachedEyeR =
                        FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "eye_r");
                if (!state.cachedEyeR)
                    state.cachedEyeR =
                        FindBoneFuzzy(state.cachedHead ? state.cachedHead : root, "eyeright");
            }

            state.skeletonResolved = true;
        }

        auto* spine = state.cachedSpine;
        auto* neck = state.cachedNeck;
        auto* head = state.cachedHead;
        auto* eyeL = state.cachedEyeL;
        auto* eyeR = state.cachedEyeR;

        // Report the skeleton probe once per actor.
        //
        // Bone names are matched by string, and these candidate lists have never
        // been confirmed against a live rig. If every name misses, the engine runs
        // perfectly and rotates nothing - a silent no-op, which is precisely the
        // failure this project exists to stop repeating. One log line per actor
        // converts that into an answerable question: did the bones resolve?
        if (!state.bonesReported)
        {
            const int found =
                (spine ? 1 : 0) + (neck ? 1 : 0) + (head ? 1 : 0) + (eyeL ? 1 : 0) + (eyeR ? 1 : 0);

            // Phase S2 rig capability matrix: classify the visual-origin mode so a
            // reader can tell an eye-node rig from a vanilla FaceGen rig, and so an
            // absent eye node is never mistaken for a defect. The head socket is the
            // documented, first-class fallback when a rig exposes no eye bones.
            const bool hasEyeNode = (eyeL != nullptr || eyeR != nullptr);
            const char* originMode =
                head ? (hasEyeNode ? "EyeNode" : "GeometricHeadSocket") : "Unavailable";

            // Record the outcome for the stgstatus rig-capability summary.
            RecordRigProbe(originMode, head != nullptr, hasEyeNode);

            logger::info("[TrueGaze] Skeleton probe for {:08X}: spine={} neck={} head={} "
                         "eyeL={} eyeR={} ({} of 5 resolved) origin={} humanoid={} player={}",
                         actor->GetFormID(), spine ? "yes" : "NO", neck ? "yes" : "NO",
                         head ? "yes" : "NO", eyeL ? "yes" : "NO", eyeR ? "yes" : "NO", found,
                         originMode, actor->IsHumanoid() ? "yes" : "no",
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

        // PLAYER CONTROL MODEL (Kirk LaSalle directive, September 25 2026):
        //
        //   3rd person: TrueGaze FULLY controls the player — head, neck, spine,
        //               eye bones, AND FaceGen eye morphs. The player character
        //               headtracks and engages nearby actors just like any NPC.
        //
        //   1st person: ONLY THE EYES are controlled by TrueGaze. Head/neck/spine
        //               bones are NEVER touched — that would rotate the camera and
        //               cause motion sickness. Eye bones and FaceGen Look* morphs
        //               still fire, so the player's eyeballs track the target even
        //               though the player's own view doesn't shift.
        //
        // This is a SACRED invariant. Do NOT add head bone writes when
        // `isPlayer && !allowHeadtrack`. The eyes-only path is intentional.
        //
        // NPCs always see the player as a valid gaze TARGET regardless of camera
        // mode — the player's eye anchor is always computed and published.
        const bool isPlayer = actor->IsPlayerRef();
        bool allowHeadtrack = !isPlayer;
        if (isPlayer)
        {
            auto* camera = RE::PlayerCamera::GetSingleton();
            allowHeadtrack = camera && camera->IsInThirdPerson();
        }

        // SCENE DEFER: while active, the head chain (spine/neck/head) yields to
        // vanilla scene direction — TrueGaze writes nothing to those bones. The
        // EYES below are NOT gated by this: eyes never yield.
        const bool headChainYields = (!isPlayer && state.sceneDeferActive);

        if (!isPlayer && spine && !headChainYields)
        {
            EyeAimConstraint::Apply(actor->GetFormID(), spine, strain.spineYaw, 0.0f);
        }

        if (allowHeadtrack && neck && !headChainYields)
        {
            EyeAimConstraint::Apply(actor->GetFormID(), neck, strain.neckYaw, strain.neckPitch);
        }

        if (allowHeadtrack && head && !headChainYields)
        {
            EyeAimConstraint::Apply(actor->GetFormID(), head, strain.headYaw, strain.headPitch);
        }

        // Eyes receive the low-inertia ballistic VOR counter-rotation and micro-jitter.
        // IMPORTANT: eye bones and FaceGen morphs are ALWAYS written, even when
        // allowHeadtrack is false (player in 1st person). This is the "only the eyes
        // are fully controlled" contract — the eyes track the target regardless of
        // camera mode. On vanilla rigs (no eye bones) the FaceGen path below is the
        // sole eye-movement channel.
        if (eyeL)
        {
            EyeAimConstraint::Apply(actor->GetFormID(), eyeL, eyeYaw, eyePitch);
        }

        if (eyeR)
        {
            EyeAimConstraint::Apply(actor->GetFormID(), eyeR, eyeYaw, eyePitch);
        }

        // Eyelid morph writes and biological eye-direction morphs (EFA / EFM / Vanilla).
        // On vanilla rigs this is the ONLY thing that moves the eyes, so the eye-lead
        // gain / full-scale are what make "the eyes are the target, and they lead the
        // head" actually visible on the Helgen-cart NPCs and every other stock NPC.
        Integrations::EfmBlinkController::ApplyGazeMorphs(
            actor->GetFormID(), state.blink.eyelidCloseWeight, eyeYaw, eyePitch,
            _tuning.eyeMorphFullScaleDeg, _tuning.eyeMorphGain);

        // In-game 3D visualisation of the solved gaze. Runs after the skeleton is
        // posed so the head bone's world transform is current, and is driven by the
        // *eye residual* - what the eyes carry beyond the already-turned head. Passing
        // the total deflection here would double-count the head's share and the beam
        // would overshoot the true gaze direction.
        try
        {
            Visuals::VisualEffectsManager::Get().UpdateActor(
                actor, state.cachedHead, state.cachedEyeL, state.cachedEyeR, eyeYaw, eyePitch,
                state.gazeRegion, isPlayer, actor->IsHumanoid());
        }
        catch (const std::exception& e)
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

    void GazeEngine::PublishState(RE::Actor* actor, const ActorGazeRuntime& state) noexcept
    {
        Integrations::OarConditions::PublishActorState(actor->GetFormID(), state.hcepMode,
                                                       state.gazeRegion, state.mutualGazeHoldSec);

        if (_pipe.IsConnected())
        {
            Bridge::SkyrimFeedbackPacket feedback{};
            feedback.targetFormId = state.trackedTargetFormId;

            // R14 E7.6 — real relationship rank. The packet contract documents
            // "Relationship rank (-4 to +4)"; the previous hard-coded 0 told
            // HCEP every NPC was neutral. Skyrim's own scale is
            // BGSRelationship::RELATIONSHIP_LEVEL (0=Lover .. 8=Archnemesis),
            // which we map onto the documented -4..+4 axis (Lover=+4,
            // Archnemesis=-4). No relationship record -> 0 (neutral), which is
            // also the honest answer for creatures (they have no TESNPC).
            feedback.relationshipRank = RelationshipRankForActor(actor);

            feedback.combatState = actor->IsInCombat() ? 1 : 0;

            // R14 E7.8 — cache the dialogue-menu lookup once per frame. The
            // singleton pointer and menu-open state cannot change mid-frame on
            // the game thread, so re-resolving RE::UI::GetSingleton() per actor
            // (hundreds of times per frame in a dense cell) was pure overhead.
            static uint64_t s_dialogueFrameTag = 0;
            static bool s_dialogueOpen = false;
            if (s_dialogueFrameTag != _frameCounter)
            {
                s_dialogueFrameTag = _frameCounter;
                auto* ui = RE::UI::GetSingleton();
                s_dialogueOpen = ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME);
            }
            feedback.isDialogueActive = s_dialogueOpen ? 1 : 0;
            feedback.distanceToTarget = DistanceMetersForTier(actor);
            feedback.mutualGazeAngle = state.lastYawDeg;

            // R14 E7.6 — real frame number. The contract documents "Skyrim
            // internal frame counter"; 0 told HCEP every feedback packet came
            // from frame zero, making staleness detection on the desktop side
            // impossible. The engine's monotonic frame counter is the closest
            // available analogue and is exactly what orders our own packets.
            feedback.gameFrameNumber = static_cast<uint32_t>(_frameCounter & 0xFFFFFFFFu);

            _pipe.SendFeedback(feedback);
        }
    }

    int16_t GazeEngine::RelationshipRankForActor(RE::Actor* actor) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        if (!actor)
        {
            return 0;
        }

        auto* npc = actor->GetActorBase();
        if (!npc || !npc->relationships)
        {
            return 0;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        if (!player)
        {
            return 0;
        }

        auto* playerNpc = player->GetActorBase();
        if (!playerNpc)
        {
            return 0;
        }

        auto* rel = RE::BGSRelationship::GetRelationship(npc, playerNpc);
        if (!rel)
        {
            return 0;
        }

        // Map Skyrim's 0..8 relationship ladder onto the packet's -4..+4 axis.
        // 0=Lover -> +4 ... 4=Acquaintance -> 0 ... 8=Archnemesis -> -4.
        const auto level = rel->level.get();
        return static_cast<int16_t>(4 - static_cast<int>(level));
#else
        (void)actor;
        return 0;
#endif
    }

    CharacterProfile::CategoryProfileBundle
    GazeEngine::CategoryBundleFor(CharacterProfile::GazeCategory category) noexcept
    {
        // R15: single bundle authority. Compiled defaults come from
        // DefaultBundleFor (the stereotype); the INI [Profiles] section overrides
        // the exposed high-signal fields. Category profiles disabled = neutral.
        if (!ConfigManager::GetSingleton().enableCategoryProfiles)
        {
            return {};
        }

        auto b = CharacterProfile::DefaultBundleFor(category);
        const auto& cfg = ConfigManager::GetSingleton();

        using C = CharacterProfile::GazeCategory;
        switch (category)
        {
        case C::Guard:
            b.aversionRateMult = cfg.guardAversionMult;
            b.fixationScaleMult = cfg.guardFixationMult;
            break;
        case C::Child:
            b.fixationScaleMult = cfg.childFixationMult;
            b.aversionRateMult = cfg.childAversionMult;
            b.pathRandomnessMult = cfg.childPathRandomnessMult;
            break;
        case C::Vampire:
            b.aversionRateMult = cfg.vampireAversionMult;
            b.fixationScaleMult = cfg.vampireFixationMult;
            break;
        case C::Werewolf:
            b.fixationScaleMult = cfg.werewolfFixationMult;
            b.pathRandomnessMult = cfg.werewolfPathRandomnessMult;
            break;
        case C::Khajiit:
            b.fixationScaleMult = cfg.khajiitFixationMult;
            b.pathRandomnessMult = cfg.khajiitPathRandomnessMult;
            break;
        case C::Argonian:
            b.fixationScaleMult = cfg.argonianFixationMult;
            b.aversionRateMult = cfg.argonianAversionMult;
            break;
        case C::Elf:
            b.fixationScaleMult = cfg.elfFixationMult;
            b.aversionRateMult = cfg.elfAversionMult;
            break;
        case C::Orc:
            b.aversionRateMult = cfg.orcAversionMult;
            b.fixationScaleMult = cfg.orcFixationMult;
            break;
        case C::Creature_Predator:
            b.fixationScaleMult = cfg.predatorFixationMult;
            b.aversionRateMult = cfg.predatorAversionMult;
            break;
        case C::Creature_Prey:
            b.fixationScaleMult = cfg.preyFixationMult;
            b.aversionRateMult = cfg.preyAversionMult;
            break;
        case C::Creature_Dragon:
            b.fixationScaleMult = cfg.dragonFixationMult;
            b.aversionRateMult = cfg.dragonAversionMult;
            break;
        case C::Undead:
            b.fixationScaleMult = cfg.undeadFixationMult;
            b.aversionRateMult = cfg.undeadAversionMult;
            break;
        case C::Daedra:
            b.aversionRateMult = cfg.daedraAversionMult;
            b.fixationScaleMult = cfg.daedraFixationMult;
            break;
        case C::Construct:
            b.fixationScaleMult = cfg.constructFixationMult;
            b.aversionRateMult = cfg.constructAversionMult;
            break;
        case C::Player:
        case C::HumanoidNPC:
        case C::OtherHumanoid:
        case C::OtherCreature:
        default:
            // Neutral categories: the temperament axes carry the whole profile.
            break;
        }

        return b;
    }

    float GazeEngine::DistanceMetersForTier(RE::Actor* actor) noexcept
    {
#if __has_include(<RE/Skyrim.h>)
        const auto* player = RE::PlayerCharacter::GetSingleton();
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

    ActorGazeRuntime* GazeEngine::FindActor(uint32_t formId) noexcept
    {
        auto it = _actors.find(formId);
        return it != _actors.end() ? &it->second : nullptr;
    }

    const char* GazeEngine::ModeName(uint8_t mode) noexcept
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

    const char* GazeEngine::RegionName(uint8_t region) noexcept
    {
        return GazeRegionName(region);
    }

} // namespace TrueGaze::Engine
