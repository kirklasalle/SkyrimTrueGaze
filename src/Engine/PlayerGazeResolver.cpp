#include "PlayerGazeResolver.hpp"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#endif

#include <cmath>
#include <algorithm>
#include <chrono>

namespace TrueGaze::Engine
{

    namespace
    {

        constexpr float kPi = 3.14159265358979323846f;
        constexpr float kRadToDeg = 180.0f / kPi;

        /// Skyrim world units per metre.
        constexpr float kUnitsPerMeter = 70.0f;

        /// Approximate eye height above an actor's origin, in Skyrim units.
        /// Used only when the head bone cannot be resolved.
        constexpr float kEyeHeightUnits = 160.0f;

        /// Physical radius of a human head and collar focal area, in metres.
        /// Expanded to 0.28m so crosshair gaze comfortably covers the head, hair,
        /// and upper neckline without edge-flicker or knife-edge boundaries.
        constexpr float kHeadRadiusMeters = 0.28f;
        constexpr float kHcepMinimumConfidence = 0.50f;

        /// Phase S4 fusion policy.
        /// Telemetry older than this is rejected: a stalled HCEP Desktop must not
        /// drive NPCs from frozen data (documented stale-rejection contract).
        constexpr uint64_t kHcepStaleMs = 500;
        /// During a blink the eyes are closed; the reported gaze vector is a
        /// prediction, not an observation. Suppress fusion rather than rotate on
        /// stale eye data (S4: blink as attention/occlusion signal).
        constexpr uint8_t kBothEyesBlinkMask = 0x03;
        /// Convergence is only trusted within this physical range; outside it the
        /// sensor value is not a plausible focal distance for a seated player.
        constexpr float kConvergenceMinMeters = 0.3f;
        constexpr float kConvergenceMaxMeters = 6.0f;

        PlayerGazeResolver::HcepSignal g_hcepSignal{};
        PlayerGazeResolver::IntentDiagnostic g_lastIntent{};

        /// Angular size of an object of physical radius r at distance d:
        /// theta = 2 * atan(r / d). Returns degrees.
        float AngularRadiusDeg(float radiusMeters, float distanceMeters) noexcept
        {
            if (distanceMeters <= 0.001f)
            {
                // Effectively point-blank: the object fills the view.
                return 89.0f;
            }
            return 2.0f * std::atan(radiusMeters / distanceMeters) * kRadToDeg;
        }

        /// Angle between two unit vectors, in degrees.
        float AngleBetweenDeg(const RE::NiPoint3 &a, const RE::NiPoint3 &b) noexcept
        {
            const float dot = std::clamp(a.Dot(b), -1.0f, 1.0f);
            return std::acos(dot) * kRadToDeg;
        }

#if __has_include(<RE/Skyrim.h>)

        /// Bone candidates for the head, mirroring GazeEngine's lists. Kept local
        /// so this module stays independent of GazeEngine internals.
        constexpr const char *kHeadCandidates[] = {
            "NPC Head [Head]", "Head", "Head1"};

        RE::NiAVObject *FindHeadBone(RE::NiAVObject *root) noexcept
        {
            if (!root)
            {
                return nullptr;
            }
            for (const char *name : kHeadCandidates)
            {
                if (auto *found = root->GetObjectByName(RE::BSFixedString(name)))
                {
                    return found;
                }
            }
            return nullptr;
        }

        /// The camera's world-space position and forward direction.
        ///
        /// Position comes from PlayerCamera::GetActiveCameraPosition(), which is
        /// the cached camera-anchor position (first-person head, or the third-
        /// person boom). Direction comes from the live NiCamera's world transform:
        /// NiCamera is an NiAVObject, and its world.rotate columns are the camera
        /// basis. In NetImmerse the camera looks down -Y, so forward = -GetVectorY().
        /// This is verified against the vendored SDK's own usage of the transform
        /// (NiCamera::worldToCam is built from world.rotate) rather than assumed.
        struct CameraFrame
        {
            RE::NiPoint3 pos{0.0f, 0.0f, 0.0f};
            RE::NiPoint3 forward{0.0f, 1.0f, 0.0f};
            bool valid{false};
        };

        CameraFrame GetCameraFrame() noexcept
        {
            CameraFrame frame;

            auto *camera = RE::Main::WorldRootCamera();
            if (!camera)
            {
                return frame;
            }

            frame.pos = RE::PlayerCamera::GetActiveCameraPosition();

            // Camera forward: NetImmerse cameras look down their local -Y axis.
            // world.rotate.GetVectorY() is the +Y basis column; negate it.
            const RE::NiPoint3 basisY = camera->world.rotate.GetVectorY();
            const float len = basisY.Length();
            if (len > 0.0001f)
            {
                frame.forward = RE::NiPoint3{-basisY.x / len, -basisY.y / len, -basisY.z / len};
                frame.valid = true;
            }
            return frame;
        }

        RE::NiPoint3 FusedForward(const CameraFrame &frame) noexcept
        {
            // Phase S4: staleness gate. A signal older than the freshness window
            // is treated as absent, exactly like a low-confidence one.
            const uint64_t nowMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                             std::chrono::steady_clock::now().time_since_epoch())
                                                             .count());
            const bool stale = g_hcepSignal.valid &&
                               (nowMs - g_hcepSignal.receivedSteadyMs) > kHcepStaleMs;
            if (!g_hcepSignal.valid || stale ||
                g_hcepSignal.confidence < kHcepMinimumConfidence)
            {
                return frame.forward;
            }

            // Phase S4: blink suppression. With both eyes closed the vector is a
            // prediction; fusing it would rotate the world on closed eyes.
            if ((g_hcepSignal.blinkBitmask & kBothEyesBlinkMask) == kBothEyesBlinkMask)
            {
                return frame.forward;
            }

            const float weight = std::clamp(g_hcepSignal.confidence, 0.0f, 1.0f);
            RE::NiPoint3 fused{
                frame.forward.x * (1.0f - weight) + g_hcepSignal.directionX * weight,
                frame.forward.y * (1.0f - weight) + g_hcepSignal.directionY * weight,
                frame.forward.z * (1.0f - weight) + g_hcepSignal.directionZ * weight};
            const float length = fused.Length();
            if (length <= 0.0001f)
            {
                return frame.forward;
            }
            fused /= length;
            return fused;
        }

        /// The world position of an actor's face. Prefers the head bone's world
        /// transform; falls back to origin + eye height when the bone is missing
        /// (creature rigs, unloaded head parts).
        bool GetFacePosition(RE::Actor *actor, RE::NiPoint3 &out) noexcept
        {
            auto *root = actor ? actor->Get3D() : nullptr;
            if (!root)
            {
                return false;
            }

            if (auto *head = FindHeadBone(root))
            {
                out = head->world.translate;
                return true;
            }

            const auto pos = actor->GetPosition();
            out = RE::NiPoint3{pos.x, pos.y, pos.z + kEyeHeightUnits};
            return true;
        }

#endif // __has_include(<RE/Skyrim.h>)

    } // namespace

    PlayerGazeResolver::PlayerGaze PlayerGazeResolver::Resolve(const Params &params) noexcept
    {
        PlayerGaze result{};

#if __has_include(<RE/Skyrim.h>)
        // 1. The game's own crosshair pick. This is the same data the HUD uses
        //    for the activation prompt, so "the crosshair is on X" is the game's
        //    own answer, not our approximation of it.
        auto *pick = RE::CrosshairPickData::GetSingleton();
        if (!pick)
        {
            return result;
        }

        const auto handle = pick->GetActiveTarget();
        auto *target = handle.get().get();
        if (!target)
        {
            return result;
        }

        auto *actor = target->As<RE::Actor>();
        if (!actor || actor->IsDead())
        {
            return result;
        }

        // 2. The camera ray. Without a valid camera frame there is no "looking".
        const CameraFrame frame = GetCameraFrame();
        if (!frame.valid)
        {
            return result;
        }

        // 3. The target's face position and the ray to it.
        RE::NiPoint3 facePos{0.0f, 0.0f, 0.0f};
        if (!GetFacePosition(actor, facePos))
        {
            return result;
        }

        const RE::NiPoint3 toFace = facePos - frame.pos;
        const float distanceUnits = toFace.Length();
        const float distanceMeters = distanceUnits / kUnitsPerMeter;

        if (distanceMeters > params.maxRangeMeters)
        {
            // Beyond social range the crosshair is an aiming tool, not a gaze.
            return result;
        }

        const float distanceMetersSafe = std::max(distanceMeters, 0.05f);
        const RE::NiPoint3 toFaceDir{
            toFace.x / distanceUnits, toFace.y / distanceUnits, toFace.z / distanceUnits};

        // 4. The sweet-spot test: angular error vs the face's angular size.
        const float faceAngleDeg = AngleBetweenDeg(FusedForward(frame), toFaceDir);
        const float headAngularRadiusDeg =
            AngularRadiusDeg(kHeadRadiusMeters, distanceMetersSafe);

        const bool pointBlank = distanceMeters <= params.pointBlankMeters;
        const float toleranceDeg = params.baseToleranceDeg +
                                   (pointBlank ? 89.0f : headAngularRadiusDeg);

        result.targetFormId = actor->GetFormID();
        result.faceAngleDeg = faceAngleDeg;
        result.headAngularRadiusDeg = headAngularRadiusDeg;
        result.faceX = facePos.x;
        result.faceY = facePos.y;
        result.faceZ = facePos.z;
        result.onFace = (faceAngleDeg <= toleranceDeg) || (distanceMeters <= 4.0f);
        return result;
#else
        (void)params;
        return result;
#endif
    }

    bool PlayerGazeResolver::IsPlayerLookingAtFace(uint32_t actorFormId,
                                                   const Params &params) noexcept
    {
        if (actorFormId == 0)
        {
            return false;
        }
        const PlayerGaze gaze = Resolve(params);
        return gaze.onFace && gaze.targetFormId == actorFormId;
    }

    void PlayerGazeResolver::SetHcepTelemetry(
        const Bridge::TrueGazeTelemetryPacket &packet) noexcept
    {
        g_hcepSignal = {};
        g_lastIntent = {};

        const uint64_t nowMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                         std::chrono::steady_clock::now().time_since_epoch())
                                                         .count());

        // Record the diagnostic snapshot even on rejection, so tgstatus can answer
        // WHY fusion is inactive (invalid packet vs low confidence vs stale).
        g_lastIntent.sequenceId = packet.sequenceId;
        g_lastIntent.confidence = packet.gazeConfidence;
        g_lastIntent.headYawDeg = packet.headYaw * kRadToDeg;
        g_lastIntent.headPitchDeg = packet.headPitch * kRadToDeg;
        g_lastIntent.convergenceMeters = packet.gazeConvergence;
        g_lastIntent.convergencePlausible =
            packet.gazeConvergence >= kConvergenceMinMeters &&
            packet.gazeConvergence <= kConvergenceMaxMeters;

        if (!Bridge::ValidateTelemetryPacket(packet) || packet.gazeConfidence < kHcepMinimumConfidence)
        {
            return;
        }

#if __has_include(<RE/Skyrim.h>)
        auto *camera = RE::Main::WorldRootCamera();
        if (!camera)
        {
            return;
        }

        const float cy = std::cos(packet.gazeYaw);
        const float sy = std::sin(packet.gazeYaw);
        const float cp = std::cos(packet.gazePitch);
        const float sp = std::sin(packet.gazePitch);
        const auto right = camera->world.rotate.GetVectorX();
        const auto basisY = camera->world.rotate.GetVectorY();
        const auto up = camera->world.rotate.GetVectorZ();
        const RE::NiPoint3 forward{-basisY.x, -basisY.y, -basisY.z};
        RE::NiPoint3 direction{
            forward.x * cy * cp + right.x * sy * cp + up.x * sp,
            forward.y * cy * cp + right.y * sy * cp + up.y * sp,
            forward.z * cy * cp + right.z * sy * cp + up.z * sp};
        const float length = direction.Length();
        if (length <= 0.0001f)
        {
            return;
        }
        direction /= length;
        g_hcepSignal.directionX = direction.x;
        g_hcepSignal.directionY = direction.y;
        g_hcepSignal.directionZ = direction.z;
        g_hcepSignal.confidence = packet.gazeConfidence;
        g_hcepSignal.sequenceId = packet.sequenceId;
        g_hcepSignal.valid = true;

        // Phase S4: full intent context for fusion and diagnostics.
        g_hcepSignal.headYawRad = packet.headYaw;
        g_hcepSignal.headPitchRad = packet.headPitch;
        g_hcepSignal.headRollRad = packet.headRoll;
        g_hcepSignal.convergenceMeters = packet.gazeConvergence;
        g_hcepSignal.blinkBitmask = packet.blinkBitmask;
        g_hcepSignal.timestampUs = packet.timestampUs;
        g_hcepSignal.receivedSteadyMs = nowMs;

        g_lastIntent.valid = true;
        g_lastIntent.effectiveConfidence = packet.gazeConfidence;
        g_lastIntent.ageMs = 0;
        g_lastIntent.stale = false;
        g_lastIntent.blinkSuppressed =
            (packet.blinkBitmask & kBothEyesBlinkMask) == kBothEyesBlinkMask;
#else
        (void)packet;
        (void)nowMs;
#endif
    }

    void PlayerGazeResolver::ClearHcepTelemetry() noexcept
    {
        g_hcepSignal = {};
        g_lastIntent = {};
    }

    PlayerGazeResolver::IntentDiagnostic PlayerGazeResolver::LastIntent() noexcept
    {
        IntentDiagnostic snapshot = g_lastIntent;
        if (snapshot.valid)
        {
            const uint64_t nowMs = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                             std::chrono::steady_clock::now().time_since_epoch())
                                                             .count());
            snapshot.ageMs = nowMs - g_hcepSignal.receivedSteadyMs;
            snapshot.stale = snapshot.ageMs > kHcepStaleMs;
        }
        return snapshot;
    }

} // namespace TrueGaze::Engine
