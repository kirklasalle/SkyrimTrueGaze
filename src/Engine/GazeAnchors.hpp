#pragma once

/// Shared eye-anchor geometry (R14 E7.5).
///
/// EyeAnchorFromHeadBone was previously duplicated in GazeEngine.cpp and
/// TargetSelector.cpp with a "keep them identical" comment — the classic
/// copy-drift trap. The fallback eye height also disagreed across modules
/// (125 units here, 160 in PlayerGazeResolver.cpp) for the same physical
/// quantity. Both now live here, once.
///
/// Game-thread only: these helpers read the NetImmerse scene graph and must
/// only be called from the main game thread (see AnimationHook's threading
/// contract).

#include "PCH.h"

#if __has_include(<RE/Skyrim.h>)
#include <RE/Skyrim.h>
#include <RE/B/BSLightingShaderMaterialEye.h>
#include <RE/N/NiSkinData.h>
#endif

namespace TrueGaze::Anchors
{
    /// Skyrim world units per metre.
    inline constexpr float kUnitsPerMeter = 70.0f;

    /// Skyrim world units per centimetre.
    inline constexpr float kUnitsPerCm = kUnitsPerMeter / 100.0f;

    /// Approximate eye height above an actor's origin, in Skyrim units.
    ///
    /// CANONICAL VALUE: 125 units (~1.79 m at 70 units/m) — the standing eye
    /// height for a 1.0-scale humanoid measured from the actor origin, which
    /// sits at the feet. 125 is retained as the canonical fallback because
    /// every module previously agreed on it EXCEPT PlayerGazeResolver (160),
    /// whose fallback only fired for creature rigs without head bones. 125
    /// keeps NPC-to-NPC geometry unchanged; the 160 outlier is corrected to
    /// match.
    inline constexpr float kEyeHeightUnits = 125.0f;

    /// Measured from vanilla eyesmale.nif / eyesfemale.nif (head-bone-local units):
    /// shader globe centres sit at x=+/-2.4, y=7.57, z=3.75 and the visible cornea
    /// front reaches y=9.11, so the pupil lies ~1.54 units ahead of the globe centre.
    inline constexpr float kEyeGlobeRadiusUnits = 1.54f;
    inline constexpr float kHalfIpdUnits = 2.4f;

    /// Project an eye anchor onto the eyeline from a head bone's world transform.
    ///
    /// The head bone origin sits at the base of the skull; the eyeballs are up and
    /// forward of it. We move along the bone's OWN world basis so the anchor tracks
    /// seated/leaning/crouched poses and race scales. In the Skyrim skeleton the
    /// head bone's local forward is +Y and local up is +Z; NiMatrix3 stores each
    /// local axis as a COLUMN of world.rotate, so column 1 = forward, column 2 = up.
#if __has_include(<RE/Skyrim.h>)
    struct EyePair
    {
        RE::NiPoint3 left{};
        RE::NiPoint3 right{};
    };

    /// True when `actor` is the player currently viewing through the 1st-person camera.
    /// In 1st person, `player->Get3D()` is the floating arms rig (`skeleton_1stperson.nif`),
    /// whose origin/head bone does not coincide with the active viewpoint. NPCs and creatures
    /// looking at the player in 1st person must aim directly into the 1st-person camera lens.
    inline bool IsFirstPersonPlayer(const RE::Actor* actor) noexcept
    {
        if (!actor || !actor->IsPlayerRef())
        {
            return false;
        }
        auto* camera = RE::PlayerCamera::GetSingleton();
        return camera && camera->IsInFirstPerson();
    }

    /// World position of the player's active 1st-person camera viewpoint.
    inline bool ResolveFirstPersonCameraPoint(RE::NiPoint3& out) noexcept
    {
        RE::NiPoint3 pos = RE::PlayerCamera::GetActiveCameraPosition();
        const bool activeValid =
            std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z) &&
            (pos.x * pos.x + pos.y * pos.y + pos.z * pos.z) > 1.0f;
        if (activeValid)
        {
            out = pos;
            return true;
        }
        if (auto* worldCam = RE::Main::WorldRootCamera())
        {
            pos = worldCam->world.translate;
            if (std::isfinite(pos.x) && std::isfinite(pos.y) && std::isfinite(pos.z) &&
                (pos.x * pos.x + pos.y * pos.y + pos.z * pos.z) > 1.0f)
            {
                out = pos;
                return true;
            }
        }
        return false;
    }

    /// Self-verified world-space camera frame (`pos`, `forward`, `right`, `up`).
    ///
    /// In NetImmerse / Gamebryo (`RE::NiCamera`), `world.rotate` stores:
    ///   col 0 (`GetVectorX`) = Forward (look direction)
    ///   col 1 (`GetVectorY`) = Up
    ///   col 2 (`GetVectorZ`) = Right
    /// Previously several call sites assumed `-GetVectorY()` was forward, which pointed
    /// straight down into the floor (~90 deg off) and broke crosshair gaze & region aiming.
    /// This helper self-verifies the exact axis mapping once via `WorldPtToScreenPt3` so
    /// the camera frame is mathematically guaranteed exact on SE, AE, 1.7.104, and VR.
    struct CameraBasis
    {
        RE::NiPoint3 pos{0.0f, 0.0f, 0.0f};
        RE::NiPoint3 forward{0.0f, 1.0f, 0.0f};
        RE::NiPoint3 right{1.0f, 0.0f, 0.0f};
        RE::NiPoint3 up{0.0f, 0.0f, 1.0f};
        bool valid{false};
    };

    inline bool ResolveCameraBasis(CameraBasis& out) noexcept
    {
        out = {};
        auto* worldCam = RE::Main::WorldRootCamera();
        if (!worldCam)
        {
            return false;
        }

        RE::NiPoint3 camPos = RE::PlayerCamera::GetActiveCameraPosition();
        if (!std::isfinite(camPos.x) || !std::isfinite(camPos.y) || !std::isfinite(camPos.z) ||
            (camPos.x * camPos.x + camPos.y * camPos.y + camPos.z * camPos.z) <= 1.0f)
        {
            camPos = worldCam->world.translate;
        }
        out.pos = camPos;

        const RE::NiPoint3 cols[3] = {
            worldCam->world.rotate.GetVectorX(),
            worldCam->world.rotate.GetVectorY(),
            worldCam->world.rotate.GetVectorZ()
        };
        auto unitize = [](RE::NiPoint3 v) noexcept {
            const float l = v.Length();
            return (l > 1e-4f) ? (v / l) : RE::NiPoint3{0.0f, 0.0f, 0.0f};
        };
        const RE::NiPoint3 ucols[3] = {unitize(cols[0]), unitize(cols[1]), unitize(cols[2])};

        // Cached axis indices + signs verified once via WorldPtToScreenPt3.
        // Defaults to standard NetImmerse NiCamera: fwd=+col0, up=+col1, right=+col2.
        static bool s_verified = false;
        static int s_fwdIdx = 0;
        static float s_fwdSign = 1.0f;
        static int s_rightIdx = 2;
        static float s_rightSign = 1.0f;
        static int s_upIdx = 1;
        static float s_upSign = 1.0f;

        if (!s_verified)
        {
            const RE::NiPoint3 origin = worldCam->world.translate;
            float bestCenterErr = 1e9f;
            int foundFwdIdx = -1;
            float foundFwdSign = 1.0f;
            for (int c = 0; c < 3; ++c)
            {
                if (ucols[c].SqrLength() < 0.5f)
                    continue;
                for (float sgn : {1.0f, -1.0f})
                {
                    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
                    const RE::NiPoint3 pt = origin + ucols[c] * (100.0f * sgn);
                    if (worldCam->WorldPtToScreenPt3(pt, sx, sy, sz, 0.0001f) &&
                        std::isfinite(sx) && std::isfinite(sy) && std::isfinite(sz) && sz > 0.0f)
                    {
                        const float err = std::hypot(sx - 0.5f, sy - 0.5f);
                        if (err < bestCenterErr)
                        {
                            bestCenterErr = err;
                            foundFwdIdx = c;
                            foundFwdSign = sgn;
                        }
                    }
                }
            }

            if (foundFwdIdx >= 0 && bestCenterErr < 0.05f)
            {
                s_fwdIdx = foundFwdIdx;
                s_fwdSign = foundFwdSign;
                const RE::NiPoint3 fwdVec = ucols[s_fwdIdx] * s_fwdSign;

                // Identify Right (+sx) and Up (+sy) among the other two orthogonal columns
                for (int c = 0; c < 3; ++c)
                {
                    if (c == s_fwdIdx)
                        continue;
                    float sx = 0.0f, sy = 0.0f, sz = 0.0f;
                    const RE::NiPoint3 pt = origin + fwdVec * 100.0f + ucols[c] * 20.0f;
                    if (worldCam->WorldPtToScreenPt3(pt, sx, sy, sz, 0.0001f) &&
                        std::isfinite(sx) && std::isfinite(sy) && sz > 0.0f)
                    {
                        const float dx = sx - 0.5f;
                        const float dy = sy - 0.5f;
                        if (std::abs(dx) >= std::abs(dy))
                        {
                            s_rightIdx = c;
                            s_rightSign = (dx >= 0.0f) ? 1.0f : -1.0f;
                        }
                        else
                        {
                            s_upIdx = c;
                            s_upSign = (dy >= 0.0f) ? 1.0f : -1.0f;
                        }
                    }
                }
                s_verified = true;
                logger::info("[TrueGaze] Verified NiCamera basis: fwd=(col{},{:+.0f}) "
                             "right=(col{},{:+.0f}) up=(col{},{:+.0f})",
                             s_fwdIdx, s_fwdSign, s_rightIdx, s_rightSign, s_upIdx, s_upSign);
            }
        }

        out.forward = ucols[s_fwdIdx] * s_fwdSign;
        out.right = ucols[s_rightIdx] * s_rightSign;
        out.up = ucols[s_upIdx] * s_upSign;
        out.valid = (out.forward.SqrLength() > 0.5f);
        return out.valid;
    }

    /// Synthesise binocular left/right eye anchors around the 1st-person camera lens
    /// using the verified camera right axis and canonical half-IPD (±2.4 units).
    inline bool ResolveFirstPersonCameraEyes(EyePair& out) noexcept
    {
        CameraBasis cam{};
        if (ResolveCameraBasis(cam))
        {
            out.left = cam.pos - cam.right * kHalfIpdUnits;
            out.right = cam.pos + cam.right * kHalfIpdUnits;
            return true;
        }
        RE::NiPoint3 camPos{};
        if (!ResolveFirstPersonCameraPoint(camPos))
        {
            return false;
        }
        out.left = camPos - RE::NiPoint3{kHalfIpdUnits, 0.0f, 0.0f};
        out.right = camPos + RE::NiPoint3{kHalfIpdUnits, 0.0f, 0.0f};
        return true;
    }

    /// Read the eye shader's globe centres in the head-part skin coordinate frame.
    /// Vanilla eyesmale.nif stores these at (+/-2.397, 7.572, 3.751), not at
    /// either of the independently guessed targeting/visual socket offsets.
    inline bool FaceGenEyeCenters(RE::Actor* actor, const RE::NiAVObject* head,
                                  EyePair& out) noexcept
    {
        if (!actor || !head)
            return false;
        auto* part = actor->GetHeadPartObject(RE::BGSHeadPart::HeadPartType::kEyes);
        auto* geometry = part ? part->GetFirstGeometryOfShaderType(
                                    RE::BSShaderMaterial::Feature::kEye) : nullptr;
        if (!geometry)
            return false;
        auto* shader = netimmerse_cast<RE::BSLightingShaderProperty*>(
            geometry->GetGeometryRuntimeData().shaderProperty.get());
        if (!shader || !shader->material ||
            shader->material->GetFeature() != RE::BSShaderMaterial::Feature::kEye)
            return false;
        const auto* material = static_cast<const RE::BSLightingShaderMaterialEye*>(shader->material);
        auto* skin = geometry->GetGeometryRuntimeData().skinInstance.get();
        if (!skin || !skin->skinData || !skin->bones || !skin->skinData->GetBoneData())
            return false;

        const auto count = std::min(skin->numMatrices, skin->skinData->GetBoneCount());
        for (std::uint32_t i = 0; i < count; ++i)
        {
            if (skin->bones[i] != head)
                continue;
            const auto& bind = skin->skinData->GetBoneDataSkinToBone(i);
            const RE::NiPoint3 a = bind * material->eyeCenter[0];
            const RE::NiPoint3 b = bind * material->eyeCenter[1];
            const float separation = a.GetDistance(b);
            if (!std::isfinite(separation) || separation < 1.0f || separation > 14.0f ||
                !std::isfinite(a.Length()) || !std::isfinite(b.Length()) ||
                a.Length() > 40.0f || b.Length() > 40.0f)
                return false;
            out.left = head->world * (a.x <= b.x ? a : b);
            out.right = head->world * (a.x <= b.x ? b : a);
            return true;
        }
        return false;
    }

    inline RE::NiPoint3 EyeAnchorFromHeadBone(const RE::NiAVObject* headBone, float forwardCm,
                                              float upCm) noexcept
    {
        const auto& m = headBone->world.rotate;
        const auto& o = headBone->world.translate;
        const float scale = headBone->world.scale > 0.0f ? headBone->world.scale : 1.0f;

        // The head bone's own axes in world space: +Y = forward (toward the face),
        // +Z = up (toward the crown). GetVectorY/Z return exactly these columns.
        const RE::NiPoint3 forward = m.GetVectorY();
        const RE::NiPoint3 up = m.GetVectorZ();

        const float fwd = forwardCm * kUnitsPerCm * scale;
        const float upl = upCm * kUnitsPerCm * scale;

        return RE::NiPoint3{o.x + forward.x * fwd + up.x * upl, o.y + forward.y * fwd + up.y * upl,
                            o.z + forward.z * fwd + up.z * upl};
    }

    inline EyePair ResolveEyeCenters(RE::Actor* actor, const RE::NiAVObject* head,
                                      const RE::NiAVObject* eyeL, const RE::NiAVObject* eyeR,
                                      float forwardCm, float upCm) noexcept
    {
        EyePair eyes{};
        if (IsFirstPersonPlayer(actor) && ResolveFirstPersonCameraEyes(eyes))
            return eyes;
        if (FaceGenEyeCenters(actor, head, eyes))
            return eyes;
        const float scale = head && head->world.scale > 0.0f ? head->world.scale : 1.0f;
        const RE::NiPoint3 right = head ? head->world.rotate.GetVectorX() : RE::NiPoint3{1, 0, 0};
        const auto centre = head ? EyeAnchorFromHeadBone(head, forwardCm, upCm)
                                 : actor->GetPosition() + RE::NiPoint3{0, 0, kEyeHeightUnits};
        eyes.left = eyeL ? eyeL->world.translate : centre - right * (kHalfIpdUnits * scale);
        eyes.right = eyeR ? eyeR->world.translate : centre + right * (kHalfIpdUnits * scale);
        return eyes;
    }

    /// Move an eye-globe centre out to the visible pupil on the cornea surface
    /// along the (unit) direction that eye is looking.
    inline RE::NiPoint3 PupilOnCornea(const RE::NiPoint3& globeCentre, const RE::NiPoint3& lookDir,
                                      const RE::NiAVObject* head) noexcept
    {
        const float scale = head && head->world.scale > 0.0f ? head->world.scale : 1.0f;
        return globeCentre + lookDir * (kEyeGlobeRadiusUnits * scale);
    }

    /// Visible pupils of an actor looking straight along its head-forward axis.
    /// Used as the aim point when another actor looks INTO these eyes.
    inline EyePair ResolvePupilsForward(RE::Actor* actor, const RE::NiAVObject* head,
                                        float forwardCm, float upCm) noexcept
    {
        EyePair eyes{};
        if (IsFirstPersonPlayer(actor) && ResolveFirstPersonCameraEyes(eyes))
            return eyes;
        eyes = ResolveEyeCenters(actor, head, nullptr, nullptr, forwardCm, upCm);
        if (head)
        {
            const RE::NiPoint3 forward = head->world.rotate.GetVectorY();
            eyes.left = PupilOnCornea(eyes.left, forward, head);
            eyes.right = PupilOnCornea(eyes.right, forward, head);
        }
        return eyes;
    }

    inline RE::NiPoint3 EyeMidpoint(RE::Actor* actor, const RE::NiAVObject* head,
                                    float forwardCm, float upCm) noexcept
    {
        if (IsFirstPersonPlayer(actor))
        {
            RE::NiPoint3 camPos{};
            if (ResolveFirstPersonCameraPoint(camPos))
                return camPos;
        }
        const auto eyes = ResolveEyeCenters(actor, head, nullptr, nullptr, forwardCm, upCm);
        return (eyes.left + eyes.right) * 0.5f;
    }
#endif
} // namespace TrueGaze::Anchors
