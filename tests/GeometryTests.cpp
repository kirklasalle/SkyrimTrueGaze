// GeometryTests — SDK-free verification of the gaze-arrow / HCEP-panel math.
//
// Covers PLAN_Verification_and_Testing.md Tier 1 (unit): column scaling (A1),
// eyeball-sized arrow scales, base placement, convergence, ray-plane
// intersection, angle<->panel projection round-trip, and classifier parity.

#include <cassert>
#include <cmath>
#include <iostream>

#include "../src/Engine/GazeRegion.hpp"
#include "../src/Visuals/GazeGeometry.hpp"

namespace
{
    namespace G = TrueGaze::Visuals::Geometry;
    using TrueGaze::Engine::ClassifyGazeRegion;
    using TrueGaze::Engine::GazeRegion;

    bool Near(float a, float b, float eps = 1e-4f) noexcept { return std::abs(a - b) <= eps; }

    // Multiply m (row-major m[row][col]) by v.
    G::Vec3 Mul(const float (&m)[3][3], const G::Vec3& v) noexcept
    {
        return {m[0][0] * v.x + m[0][1] * v.y + m[0][2] * v.z,
                m[1][0] * v.x + m[1][1] * v.y + m[1][2] * v.z,
                m[2][0] * v.x + m[2][1] * v.y + m[2][2] * v.z};
    }

    // Build a rotation whose COLUMN 1 is `dir` (same construction as
    // AlignBeamOrientation's +Y path).
    void BasisForY(const G::Vec3& dirIn, float (&m)[3][3]) noexcept
    {
        const G::Vec3 dir = G::Normalized(dirIn);
        const G::Vec3 ref = std::abs(dir.z) < 0.9f ? G::Vec3{0, 0, 1} : G::Vec3{0, 1, 0};
        const auto cross = [](const G::Vec3& a, const G::Vec3& b) {
            return G::Vec3{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
        };
        const G::Vec3 right = G::Normalized(cross(dir, ref));
        const G::Vec3 ortho = G::Normalized(cross(right, dir));
        const G::Vec3 cols[3] = {right, dir, ortho};
        for (int c = 0; c < 3; ++c)
        {
            m[0][c] = cols[c].x;
            m[1][c] = cols[c].y;
            m[2][c] = cols[c].z;
        }
    }

    void TestColumnScalingPreservesDirection()
    {
        std::cout << "[TEST] Column scaling keeps the arrow pointing along the gaze (A1)...\n";
        const G::Vec3 gaze = G::Normalized({0.5f, 0.8f, 0.33f}); // right, forward, up

        float m[3][3];
        BasisForY(gaze, m);
        G::ScaleBasisColumns(m, 0.05f, 1.0f, 0.05f);

        // Mesh +Y (arrow length axis) must still map exactly onto the gaze.
        const G::Vec3 y = Mul(m, {0, 1, 0});
        assert(Near(y.x, gaze.x) && Near(y.y, gaze.y) && Near(y.z, gaze.z));
        // Cross-section axes shrink but stay perpendicular to the gaze.
        const G::Vec3 x = Mul(m, {1, 0, 0});
        const G::Vec3 z = Mul(m, {0, 0, 1});
        assert(Near(G::Length(x), 0.05f) && Near(G::Length(z), 0.05f));
        assert(Near(G::Dot(x, gaze), 0.0f) && Near(G::Dot(z, gaze), 0.0f));

        // Regression guard: the OLD row scaling bends the arrow toward +Y.
        float bad[3][3];
        BasisForY(gaze, bad);
        for (int col = 0; col < 3; ++col)
        {
            bad[0][col] *= 0.09f;
            bad[2][col] *= 0.09f;
        }
        const G::Vec3 badY = G::Normalized(Mul(bad, {0, 1, 0}));
        assert(G::Dot(badY, gaze) < 0.95f); // visibly wrong direction
        std::cout << "  -> passed (old row-scaling deviation: "
                  << std::acos(G::Dot(badY, gaze)) * 57.29578f << " deg).\n";
    }

    void TestArrowScales()
    {
        std::cout << "[TEST] Arrow length = reach, cross-section = eyeball...\n";
        const float reach = 2.5f * 70.0f;    // 175 units
        const float eye = 24.0f / 1000.0f * 70.0f; // 1.68 units
        const G::ArrowScales s = G::ComputeMarkerArrowScales(reach, eye);

        assert(Near(G::MarkerArrowMesh::kLengthY * s.uniform, reach, 1e-3f));
        const float widest = G::MarkerArrowMesh::kMaxCrossSection * s.uniform * s.crossSection;
        assert(Near(widest, eye, 1e-4f));
        (void)widest;
        const float thickness = G::MarkerArrowMesh::kWidthX * s.uniform * s.crossSection;
        assert(Near(thickness, eye * 64.0f / 160.0f, 1e-4f));
        (void)thickness;
        // Rear edge on the pupil: centre is half a length ahead.
        assert(Near(s.baseOffset, reach * 0.5f, 1e-3f));

        // Cross-section is independent of reach.
        const G::ArrowScales s2 = G::ComputeMarkerArrowScales(700.0f, eye);
        assert(Near(G::MarkerArrowMesh::kMaxCrossSection * s2.uniform * s2.crossSection, eye, 1e-4f));
        std::cout << "  -> passed.\n";
    }

    void TestConvergence()
    {
        std::cout << "[TEST] Binocular convergence...\n";
        const float halfIpd = 2.2f;
        const G::Vec3 mid{0, 0, 0};
        const G::Vec3 eyeL{-halfIpd, 0, 0};
        const G::Vec3 eyeR{halfIpd, 0, 0};
        const G::Vec3 fwd{0, 1, 0};
        const float d = 175.0f;

        const G::Vec3 dl = G::ConvergedEyeDirection(eyeL, mid, fwd, d);
        const G::Vec3 dr = G::ConvergedEyeDirection(eyeR, mid, fwd, d);
        // Both rays reach the same fixation point.
        const G::Vec3 fix{0, d, 0};
        const G::Vec3 hitL = eyeL + dl * G::Length(fix - eyeL);
        const G::Vec3 hitR = eyeR + dr * G::Length(fix - eyeR);
        assert(Near(hitL.x, 0.0f, 1e-3f) && Near(hitR.x, 0.0f, 1e-3f));
        // Left eye turns right (+x), right eye turns left.
        assert(dl.x > 0.0f && dr.x < 0.0f);
        // Expected vergence half-angle ~0.72 deg.
        assert(Near(std::atan2(halfIpd, d) * 57.29578f, std::asin(dl.x) * 57.29578f, 1e-2f));
        // Degenerate distance falls back to the cyclopean direction.
        const G::Vec3 dz = G::ConvergedEyeDirection(eyeL, mid, fwd, 0.0f);
        assert(Near(dz.y, 1.0f));
        std::cout << "  -> passed.\n";
    }

    void TestRayPlane()
    {
        std::cout << "[TEST] Ray-plane intersection...\n";
        const G::Vec3 n{0, -1, 0}; // panel faces the eye
        const G::Vec3 p{0, 49, 0};
        const G::RayHit h = G::IntersectRayPlane({0, 0, 0}, G::Normalized({0.2f, 1, 0.1f}), p, n);
        assert(h.hit && Near(h.point.y, 49.0f, 1e-3f));
        assert(!G::IntersectRayPlane({0, 0, 0}, {0, -1, 0}, p, n).hit); // away
        assert(!G::IntersectRayPlane({0, 0, 0}, {1, 0, 0}, p, n).hit);  // parallel
        std::cout << "  -> passed.\n";
    }

    void TestPanelProjectionRoundTrip()
    {
        std::cout << "[TEST] Angle <-> panel projection round trip...\n";
        const float d = 49.0f;
        for (float yaw = -30.0f; yaw <= 30.0f; yaw += 7.5f)
        {
            for (float pitch = -25.0f; pitch <= 25.0f; pitch += 5.0f)
            {
                const G::PanelPoint pt = G::ProjectAnglesToPanel(yaw, pitch, d);
                // Matches the closed form in SPEC_Region_Map.md.
                const float r = 3.14159265f / 180.0f;
                assert(Near(pt.x, d * std::tan(yaw * r), 1e-3f));
                assert(Near(pt.z, d * std::tan(pitch * r) / std::cos(yaw * r), 1e-3f));

                // And agrees with an actual ray-plane hit along the gaze vector.
                const float cy = std::cos(yaw * r), sy = std::sin(yaw * r);
                const float cp = std::cos(pitch * r), sp = std::sin(pitch * r);
                const G::RayHit h = G::IntersectRayPlane({0, 0, 0}, {sy * cp, cy * cp, sp},
                                                         {0, d, 0}, {0, -1, 0});
                assert(h.hit && Near(h.point.x, pt.x, 1e-3f) && Near(h.point.z, pt.z, 1e-3f));

                float yaw2 = 0, pitch2 = 0;
                G::PanelToAngles(pt, d, yaw2, pitch2);
                assert(Near(yaw2, yaw, 1e-3f) && Near(pitch2, pitch, 1e-3f));
            }
        }
        std::cout << "  -> passed.\n";
    }

    void TestClassifierParity()
    {
        std::cout << "[TEST] Region classifier (parity with pre-move behaviour)...\n";
        const auto R = [](GazeRegion g) { return static_cast<uint8_t>(g); };
        assert(ClassifyGazeRegion(-2.0f, 0.0f) == R(GazeRegion::LeftEye));
        assert(ClassifyGazeRegion(2.0f, 0.0f) == R(GazeRegion::RightEye));
        assert(ClassifyGazeRegion(0.0f, 0.0f) == R(GazeRegion::LeftEye)); // centre tie -> left
        assert(ClassifyGazeRegion(0.0f, 6.0f) == R(GazeRegion::Forehead));
        assert(ClassifyGazeRegion(-8.0f, 18.0f) == R(GazeRegion::UpperLeftPeripheral));
        assert(ClassifyGazeRegion(8.0f, 18.0f) == R(GazeRegion::UpperRightPeripheral));
        assert(ClassifyGazeRegion(-8.0f, -18.0f) == R(GazeRegion::LowerLeftPeripheral));
        assert(ClassifyGazeRegion(8.0f, -18.0f) == R(GazeRegion::LowerRightPeripheral));
        assert(ClassifyGazeRegion(0.0f, -18.0f) == R(GazeRegion::Ground));
        assert(ClassifyGazeRegion(0.0f, -8.0f) == R(GazeRegion::Chin));
        assert(ClassifyGazeRegion(0.0f, -4.5f) == R(GazeRegion::Torso));
        // Known issue (flagged for Kirk): central Mouth is shadowed by Torso;
        // Mouth is only produced off-centre.
        assert(ClassifyGazeRegion(6.0f, -4.5f) == R(GazeRegion::Mouth));
        std::cout << "  -> passed.\n";
    }

    void TestRayPanelHitDetection()
    {
        std::cout << "[TEST] Ray-panel hit detection (Phase 2 A7)...\n";
        const float dist = 49.0f; // 70cm = 49 units
        const G::Vec3 origin{0.0f, 0.0f, 0.0f}; // cyclopean eye
        const auto R = [](GazeRegion g) { return static_cast<uint8_t>(g); };

        // 1. Straight-ahead gaze -> hits centre, LeftEye default
        {
            const G::Vec3 dir{0.0f, 1.0f, 0.0f};
            const G::PanelHitResult res = G::IntersectGazeWithPanel(origin, dir, dist);
            assert(res.hit);
            assert(Near(res.hitX, 0.0f, 1e-4f));
            assert(Near(res.hitZ, 0.0f, 1e-4f));
            assert(res.hitRegion == R(GazeRegion::LeftEye));
        }

        // 2. Rightward gaze (yaw +5 deg) -> hits RightEye
        {
            constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
            const float y = 5.0f * kDegToRad;
            const G::Vec3 dir = G::Normalized({std::sin(y), std::cos(y), 0.0f});
            const G::PanelHitResult res = G::IntersectGazeWithPanel(origin, dir, dist);
            assert(res.hit);
            assert(res.hitX > 0.0f);
            assert(Near(res.hitYawDeg, 5.0f, 1e-3f));
            assert(res.hitRegion == R(GazeRegion::RightEye));
        }

        // 3. Forehead gaze (pitch +6 deg) -> hits Forehead
        {
            constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
            const float p = 6.0f * kDegToRad;
            const G::Vec3 dir = G::Normalized({0.0f, std::cos(p), std::sin(p)});
            const G::PanelHitResult res = G::IntersectGazeWithPanel(origin, dir, dist);
            assert(res.hit);
            assert(res.hitZ > 0.0f);
            assert(Near(res.hitPitchDeg, 6.0f, 1e-3f));
            assert(res.hitRegion == R(GazeRegion::Forehead));
        }

        // 4. Backward or parallel ray -> no hit
        {
            const G::Vec3 backDir{0.0f, -1.0f, 0.0f};
            const G::PanelHitResult res = G::IntersectGazeWithPanel(origin, backDir, dist);
            assert(!res.hit);
            assert(res.hitRegion == 0xFF);
        }

        // 5. Bounded quad test -> wide gaze out of bounds
        {
            constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;
            const float y = 45.0f * kDegToRad;
            const G::Vec3 dir = G::Normalized({std::sin(y), std::cos(y), 0.0f});
            const G::PanelHitResult res = G::IntersectGazeWithPanel(origin, dir, dist, /*halfWidth=*/10.0f, /*halfHeight=*/10.0f);
            assert(!res.hit);
            assert(res.hitRegion == 0xFF);
        }

        std::cout << "  -> passed.\n";
    }

    void TestCalibrationRegionRepresentativeAngles()
    {
        std::cout << "[TEST] 11-region calibration representative angles verification...\n";
        struct CalCheck
        {
            uint8_t expectedRegion;
            const char* name;
            float yawDeg;
            float pitchDeg;
        };

        const CalCheck checks[] = {
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

        const G::Vec3 origin{0.0f, 0.0f, 0.0f};
        const float dist = 70.0f / 100.0f * 70.0f; // 49.0 units
        constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

        for (const auto& c : checks)
        {
            // 1. Classifier test
            const uint8_t classified = ClassifyGazeRegion(c.yawDeg, c.pitchDeg);
            assert(classified == c.expectedRegion);
            (void)classified;

            // 2. Physical ray-panel hit test
            const float y = c.yawDeg * kDegToRad;
            const float p = c.pitchDeg * kDegToRad;
            const G::Vec3 dir = G::Normalized({std::sin(y), std::cos(y) * std::cos(p), std::sin(p)});
            const G::PanelHitResult res = G::IntersectGazeWithPanel(origin, dir, dist);

            assert(res.hit);
            assert(res.hitRegion == c.expectedRegion);
            assert(res.agreement);
        }

        std::cout << "  -> passed (11/11 calibration regions verified with 100% agreement).\n";
    }
} // namespace

int main()
{
    TestColumnScalingPreservesDirection();
    TestArrowScales();
    TestConvergence();
    TestRayPlane();
    TestPanelProjectionRoundTrip();
    TestClassifierParity();
    TestRayPanelHitDetection();
    TestCalibrationRegionRepresentativeAngles();
    std::cout << "[GeometryTests] All tests passed.\n";
    return 0;
}
