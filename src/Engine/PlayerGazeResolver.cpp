#include "PlayerGazeResolver.hpp"

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

        /// Approximate eye height above an actor's origin, in Skyrim units.
        /// Used only when the head bone cannot be resolved.
        constexpr float kEyeHeightUnits = 160.0f;

        /// Physical radius of a human head, in metres. A real head is ~0.09 m
        /// half-width; 0.12 m is used so the sweet spot covers hair/hood edges
        /// without counting a miss by a full body-width.
        constexpr float kHeadRadiusMeters = 0.12f;

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
        const float faceAngleDeg = AngleBetweenDeg(frame.forward, toFaceDir);
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
        result.distanceMeters = distanceMeters;
        result.onFace = faceAngleDeg <= toleranceDeg;
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

} // namespace TrueGaze::Engine
