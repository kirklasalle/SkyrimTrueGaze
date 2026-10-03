#pragma once

// ---------------------------------------------------------------------------
// GazeGeometry — SDK-free math shared by the renderer, the (Phase 2) panel hit
// detector, and the unit tests.
//
// Everything here is pure float math with no CommonLibSSE dependency so it can
// be verified in the standalone test build (tests/GeometryTests.cpp). The
// renderer converts to/from RE::NiPoint3 / RE::NiMatrix3 at the call site.
//
// Matrix convention (matches RE::NiMatrix3 as used by AlignBeamOrientation):
//   m[row][col]; a node's rotation maps mesh-local axes through its COLUMNS,
//   i.e. M * unitX = column 0, M * unitY = column 1, M * unitZ = column 2.
//
// Spec: DESIGN_Arrow_Panel_Calibration_HitDetection.md (approved 2026-10-03).
// ---------------------------------------------------------------------------

#include <algorithm>
#include <cmath>
#include "../Engine/GazeRegion.hpp"

namespace TrueGaze::Visuals::Geometry
{

    struct Vec3
    {
        float x{0.0f};
        float y{0.0f};
        float z{0.0f};
    };

    [[nodiscard]] constexpr Vec3 operator+(const Vec3& a, const Vec3& b) noexcept
    {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }
    [[nodiscard]] constexpr Vec3 operator-(const Vec3& a, const Vec3& b) noexcept
    {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }
    [[nodiscard]] constexpr Vec3 operator*(const Vec3& a, float s) noexcept
    {
        return {a.x * s, a.y * s, a.z * s};
    }
    [[nodiscard]] constexpr float Dot(const Vec3& a, const Vec3& b) noexcept
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }
    [[nodiscard]] inline float Length(const Vec3& a) noexcept
    {
        return std::sqrt(Dot(a, a));
    }
    [[nodiscard]] inline Vec3 Normalized(const Vec3& a) noexcept
    {
        const float len = Length(a);
        return len > 1e-6f ? a * (1.0f / len) : Vec3{};
    }

    // -----------------------------------------------------------------------
    // Native mesh dimensions (measured with scripts/measure_nif.py, 2026-10-03)
    // -----------------------------------------------------------------------
    // marker_arrow.nif: one BSTriShape, 44 verts, centred on its origin.
    //   X -32..+32 (64)   — arrow thickness
    //   Y -120..+120 (240) — arrow length, authored along +Y
    //   Z -80..+80 (160)  — arrowhead span (the widest cross-section extent)
    struct MarkerArrowMesh
    {
        static constexpr float kWidthX = 64.0f;
        static constexpr float kLengthY = 240.0f;
        static constexpr float kHeightZ = 160.0f;
        static constexpr float kHalfLengthY = kLengthY * 0.5f;
        /// Largest cross-section extent; this is what is mapped to the eyeball.
        static constexpr float kMaxCrossSection = kHeightZ > kWidthX ? kHeightZ : kWidthX;
    };

    /// Human eyeball axial diameter (adult average ~24 mm). Default visible
    /// cross-section of the gaze arrow (Kirk directive 2026-10-03).
    inline constexpr float kDefaultEyeDiameterMm = 24.0f;

    // -----------------------------------------------------------------------
    // Arrow scaling
    // -----------------------------------------------------------------------
    struct ArrowScales
    {
        /// NiTransform uniform scale. Sets the arrow LENGTH to exactly the
        /// configured reach: kLengthY * uniform == lengthUnits.
        float uniform{1.0f};
        /// Extra multiplier applied to rotation COLUMNS 0 and 2 (mesh-local X
        /// and Z) so the widest cross-section equals the eyeball diameter while
        /// column 1 (length) is untouched.
        float crossSection{1.0f};
        /// Distance from the eye to the arrow's centre along the gaze ray, so
        /// the arrow's rear edge sits exactly at the pupil.
        float baseOffset{0.0f};
    };

    /// @param lengthUnits       configured reach in (anchor-local) Skyrim units
    /// @param crossSectionUnits desired widest cross-section, e.g. eye diameter
    [[nodiscard]] inline ArrowScales ComputeMarkerArrowScales(float lengthUnits,
                                                              float crossSectionUnits) noexcept
    {
        ArrowScales s{};
        const float length = std::clamp(lengthUnits, 1.0f, 7000.0f);
        s.uniform = length / MarkerArrowMesh::kLengthY;
        const float nativeCross = MarkerArrowMesh::kMaxCrossSection * s.uniform;
        s.crossSection =
            nativeCross > 1e-6f ? std::clamp(crossSectionUnits, 0.01f, 100.0f) / nativeCross : 1.0f;
        s.baseOffset = MarkerArrowMesh::kHalfLengthY * s.uniform;
        return s;
    }

    /// Scale mesh-local axes of a rotation basis. Writes COLUMNS (m[row][col]),
    /// i.e. computes M * diag(sx, sy, sz). Scaling ROWS instead computes
    /// diag(...) * M — a parent-space squash that also bends the direction
    /// column toward parent +Y (the pre-2026-10-03 arrow bug).
    inline void ScaleBasisColumns(float (&m)[3][3], float sx, float sy, float sz) noexcept
    {
        for (int row = 0; row < 3; ++row)
        {
            m[row][0] *= sx;
            m[row][1] *= sy;
            m[row][2] *= sz;
        }
    }

    // -----------------------------------------------------------------------
    // Binocular convergence
    // -----------------------------------------------------------------------
    /// Per-eye direction that converges on the fixation point
    /// `cyclopeanOrigin + cyclopeanDir * fixationDistance`. Falls back to the
    /// cyclopean direction for degenerate inputs.
    [[nodiscard]] inline Vec3 ConvergedEyeDirection(const Vec3& eyeOrigin, const Vec3& cyclopeanOrigin,
                                                    const Vec3& cyclopeanDir,
                                                    float fixationDistance) noexcept
    {
        if (fixationDistance <= 1e-3f)
        {
            return Normalized(cyclopeanDir);
        }
        const Vec3 fixation = cyclopeanOrigin + Normalized(cyclopeanDir) * fixationDistance;
        const Vec3 d = Normalized(fixation - eyeOrigin);
        return Length(d) > 0.5f ? d : Normalized(cyclopeanDir);
    }

    // -----------------------------------------------------------------------
    // Ray–plane intersection and panel projection (Phase 2 hit detection)
    // -----------------------------------------------------------------------
    struct RayHit
    {
        bool hit{false};
        float t{0.0f}; // distance along the (unit) ray
        Vec3 point{};
    };

    /// Intersect ray (origin, unit dir) with plane (point, unit normal).
    /// Rays parallel to, or pointing away from, the plane report no hit.
    [[nodiscard]] inline RayHit IntersectRayPlane(const Vec3& origin, const Vec3& dir,
                                                  const Vec3& planePoint,
                                                  const Vec3& planeNormal) noexcept
    {
        RayHit r{};
        const float denom = Dot(dir, planeNormal);
        if (std::abs(denom) < 1e-6f)
        {
            return r;
        }
        const float t = Dot(planePoint - origin, planeNormal) / denom;
        if (t < 0.0f)
        {
            return r;
        }
        r.hit = true;
        r.t = t;
        r.point = origin + dir * t;
        return r;
    }

    /// Where a gaze at (yaw, pitch) lands on a fronto-parallel plane `distance`
    /// in front of the eye, in plane-local units (x = right, z = up).
    /// Uses the GazeDirection basis: dir = (sin y cos p, cos y cos p, sin p).
    struct PanelPoint
    {
        float x{0.0f};
        float z{0.0f};
    };

    [[nodiscard]] inline PanelPoint ProjectAnglesToPanel(float yawDeg, float pitchDeg,
                                                         float distance) noexcept
    {
        constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
        const float y = yawDeg * kDegToRad;
        const float p = pitchDeg * kDegToRad;
        const float forward = std::cos(y) * std::cos(p);
        if (forward <= 1e-4f)
        {
            return {};
        }
        const float t = distance / forward;
        return {std::sin(y) * std::cos(p) * t, std::sin(p) * t};
    }

    /// Inverse of ProjectAnglesToPanel.
    inline void PanelToAngles(const PanelPoint& pt, float distance, float& outYawDeg,
                              float& outPitchDeg) noexcept
    {
        constexpr float kRadToDeg = 180.0f / 3.14159265358979323846f;
        outYawDeg = std::atan2(pt.x, distance) * kRadToDeg;
        outPitchDeg = std::atan2(pt.z, std::sqrt(pt.x * pt.x + distance * distance)) * kRadToDeg;
    }

    // -----------------------------------------------------------------------
    // Ray–panel hit detection (Phase 2)
    // -----------------------------------------------------------------------
    struct PanelHitResult
    {
        bool hit{false};
        float t{0.0f};          // ray parameter (units)
        float hitX{0.0f};       // panel-relative X (right, units)
        float hitZ{0.0f};       // panel-relative Z (up, units)
        float hitYawDeg{0.0f};  // equivalent gaze yaw at hit point
        float hitPitchDeg{0.0f};// equivalent gaze pitch at hit point
        uint8_t hitRegion{0xFF};// region ID from ClassifyGazeRegion, or 0xFF
    };

    /// Calculate where a gaze ray (origin O, direction D relative to cyclopean eye)
    /// hits the floating HCEP panel at forward distance `panelForwardOffsetUnits`.
    /// Also checks quad bounds if halfWidth/halfHeight > 0.
    [[nodiscard]] inline PanelHitResult IntersectGazeWithPanel(
        const Vec3& rayOriginRelEyeMid,
        const Vec3& rayDir,
        float panelForwardOffsetUnits,
        float halfWidthUnits = 0.0f,
        float halfHeightUnits = 0.0f) noexcept
    {
        PanelHitResult res{};
        if (rayDir.y <= 1e-4f || panelForwardOffsetUnits <= 0.0f)
        {
            return res;
        }

        const float t = (panelForwardOffsetUnits - rayOriginRelEyeMid.y) / rayDir.y;
        if (t <= 0.0f)
        {
            return res;
        }

        res.t = t;
        res.hitX = rayOriginRelEyeMid.x + rayDir.x * t;
        res.hitZ = rayOriginRelEyeMid.z + rayDir.z * t;

        if (halfWidthUnits > 0.0f && std::abs(res.hitX) > halfWidthUnits)
        {
            res.hit = false;
            res.hitRegion = 0xFF;
            return res;
        }
        if (halfHeightUnits > 0.0f && std::abs(res.hitZ) > halfHeightUnits)
        {
            res.hit = false;
            res.hitRegion = 0xFF;
            return res;
        }

        res.hit = true;
        PanelToAngles({res.hitX, res.hitZ}, panelForwardOffsetUnits, res.hitYawDeg, res.hitPitchDeg);
        res.hitRegion = TrueGaze::Engine::ClassifyGazeRegion(res.hitYawDeg, res.hitPitchDeg);
        return res;
    }

} // namespace TrueGaze::Visuals::Geometry
