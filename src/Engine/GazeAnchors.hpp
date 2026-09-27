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

    /// Project an eye anchor onto the eyeline from a head bone's world transform.
    ///
    /// The head bone origin sits at the base of the skull; the eyeballs are up and
    /// forward of it. We move along the bone's OWN world basis so the anchor tracks
    /// seated/leaning/crouched poses and race scales. In the Skyrim skeleton the
    /// head bone's local forward is +Y and local up is +Z; NiMatrix3 stores each
    /// local axis as a COLUMN of world.rotate, so column 1 = forward, column 2 = up.
#if __has_include(<RE/Skyrim.h>)
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
#endif
} // namespace TrueGaze::Anchors
