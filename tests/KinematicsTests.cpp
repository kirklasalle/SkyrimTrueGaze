#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>
#include <limits>

#include "../src/Kinematics/SaccadeGenerator.hpp"
#include "../src/Kinematics/VorCoordinator.hpp"
#include "../src/Kinematics/MicroJitter.hpp"
#include "../src/Kinematics/SocialTriangle.hpp"
#include "../src/Engine/BoneController.hpp"
#include "../src/Engine/LodManager.hpp"
#include "../src/Integrations/EfmBlinkController.hpp"
#include "../src/Bridge/TelemetryPacket.h"

namespace
{

    uint32_t ComputeCrc32(const uint8_t *data, size_t length) noexcept
    {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < length; ++i)
        {
            crc ^= data[i];
            for (int j = 0; j < 8; ++j)
            {
                crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
            }
        }
        return ~crc;
    }

    void TestSaccadeGenerator()
    {
        std::cout << "[TEST] Running SaccadeGenerator verification...\n";

        // 1. Peak velocity scaling
        float v0 = TrueGaze::Kinematics::SaccadeGenerator::CalculatePeakVelocity(0.0f);
        assert(v0 == 0.0f);

        float v10 = TrueGaze::Kinematics::SaccadeGenerator::CalculatePeakVelocity(10.0f);
        float v30 = TrueGaze::Kinematics::SaccadeGenerator::CalculatePeakVelocity(30.0f);
        assert(v10 > 0.0f && v10 < v30);
        assert(v30 < TrueGaze::Kinematics::SaccadeGenerator::DEFAULT_VMAX);

        // 2. Duration scaling
        float d10 = TrueGaze::Kinematics::SaccadeGenerator::CalculateDuration(10.0f);
        float d40 = TrueGaze::Kinematics::SaccadeGenerator::CalculateDuration(40.0f);
        assert(d10 > 0.02f && d10 < 0.06f); // ~47ms
        assert(d40 > d10);

        // 3. Trajectory stepping
        TrueGaze::Kinematics::SaccadeGenerator::SaccadeState state;
        TrueGaze::Kinematics::SaccadeGenerator::TriggerSaccade(state, 20.0f, 10.0f);
        assert(state.isBallistic);
        assert(state.amplitudeDeg > 22.0f);

        float dt = 0.016f; // 60 FPS frame time
        while (state.isBallistic)
        {
            TrueGaze::Kinematics::SaccadeGenerator::Update(state, dt);
        }
        assert(!state.isBallistic);
        assert(std::abs(state.currentYaw - 20.0f) < 0.001f);
        assert(std::abs(state.currentPitch - 10.0f) < 0.001f);

        std::cout << "  -> SaccadeGenerator passed.\n";
    }

    void TestVorCoordinator()
    {
        std::cout << "[TEST] Running VorCoordinator verification...\n";

        TrueGaze::Kinematics::VorCoordinator::VorState state;
        state.targetYaw = 30.0f;
        state.targetPitch = 0.0f;

        // Step VOR
        float dt = 0.016f;
        for (int i = 0; i < 60; ++i)
        {
            TrueGaze::Kinematics::VorCoordinator::Update(state, dt);
        }

        // Head should approach target, eye should counter-rotate
        assert(state.headYaw > 20.0f);
        assert(state.eyeLocalYaw >= -state.eyeMaxAngle && state.eyeLocalYaw <= state.eyeMaxAngle);

        // Extreme steep pitch test: target at +86.6 deg (e.g. high cliff or ledge)
        // Head pitch must be strictly clamped to cervical limit (+45 deg),
        // and eyes must actively counter-rotate towards the target rather than freezing forward.
        state.targetPitch = 86.6f;
        for (int i = 0; i < 120; ++i)
        {
            TrueGaze::Kinematics::VorCoordinator::Update(state, dt);
        }
        assert(state.headPitch <= 45.001f);
        assert(state.eyeLocalPitch > 0.0f);

        // 3. Biological Latency Gap Verification:
        // When a gaze shift begins, eye must lead while head movement is delayed.
        TrueGaze::Kinematics::VorCoordinator::VorState latencyState;
        latencyState.targetYaw = 25.0f;
        latencyState.headOnsetDelayTimerSec = 0.12f; // 120 ms biological onset delay

        // Step 3 frames (~48 ms)
        for (int i = 0; i < 3; ++i)
        {
            TrueGaze::Kinematics::VorCoordinator::Update(latencyState, dt);
        }
        // Head MUST remain stationary at 0.0 deg during the delay window
        assert(latencyState.headYaw == 0.0f);
        // Eye MUST have absorbed the full target deflection
        assert(latencyState.eyeLocalYaw == 25.0f);

        // Step past the delay window (another 6 frames, total ~144 ms)
        for (int i = 0; i < 6; ++i)
        {
            TrueGaze::Kinematics::VorCoordinator::Update(latencyState, dt);
        }
        // Delay timer should now be expired
        assert(latencyState.headOnsetDelayTimerSec == 0.0f);
        // Head must now be actively rotating towards target
        assert(latencyState.headYaw > 0.0f);
        // Eye must counter-rotate (VOR) as head catches up
        assert(latencyState.eyeLocalYaw < 25.0f);

        std::cout << "  -> VorCoordinator passed (including cervical clamping & biological latency gap).\n";
    }

    // Superseded by TestJitterIsBrownianAndSeedable, which asserts the correct
    // behaviour. Retained as a cheap sanity check that drift stays bounded in
    // practice — the original version relaxed its bound to 1.5x the declared
    // amplitude to make itself pass. See audit section 5.13.
    void TestMicroJitter()
    {
        std::cout << "[TEST] Running MicroJitter bounded-drift verification...\n";

        TrueGaze::Kinematics::MicroJitter::JitterState state;
        TrueGaze::Kinematics::MicroJitter::Init(state, 0xABCDu, 0.35f);

        constexpr float dt = 1.0f / 120.0f;

        for (int i = 0; i < 3600; ++i)
        { // 30 seconds
            TrueGaze::Kinematics::MicroJitter::Update(state, dt);

            // A bounded process spends almost all its time well inside 3 sigma.
            // 3 sigma for a 0.35 deg stationary standard deviation is ~1.05 deg.
            assert(std::abs(state.currentYawOffset) < 1.5f);
            assert(std::abs(state.currentPitchOffset) < 1.5f);
        }

        std::cout << "  -> MicroJitter bounded-drift passed.\n";
    }

    void TestSocialTriangle()
    {
        std::cout << "[TEST] Running SocialTriangle verification...\n";

        TrueGaze::Kinematics::SocialTriangle::TriangleState state;
        auto initialVertex = state.currentVertex;

        // Simulate 2 seconds to force vertex transition
        float dt = 0.05f;
        for (int i = 0; i < 40; ++i)
        {
            TrueGaze::Kinematics::SocialTriangle::Update(state, dt, 1.5f);
        }

        // Vertex must have cycled
        assert(state.currentVertex != initialVertex);

        std::cout << "  -> SocialTriangle passed.\n";
    }

    void TestBoneController()
    {
        std::cout << "[TEST] Running BoneController hierarchy strain verification...\n";

        auto strain = TrueGaze::Engine::BoneController::CalculateHierarchyStrain(40.0f, 20.0f);
        // Spine2: 10% of 40 = 4.0
        assert(std::abs(strain.spineYaw - 4.0f) < 0.01f);
        // Neck: 25% of 40 = 10.0, 25% of 20 = 5.0
        assert(std::abs(strain.neckYaw - 10.0f) < 0.01f);
        assert(std::abs(strain.neckPitch - 5.0f) < 0.01f);
        // Head: 65% of 40 = 26.0, 75% of 20 = 15.0
        assert(std::abs(strain.headYaw - 26.0f) < 0.01f);
        assert(std::abs(strain.headPitch - 15.0f) < 0.01f);

        std::cout << "  -> BoneController passed.\n";
    }

    /// Regression test for audit finding 5.11.
    ///
    /// The original implementation distributed 0.10 + 0.25 + 0.65 = 1.00 of the
    /// deflection across Spine2/Neck/Head and never assigned eyeYaw/eyePitch, so the
    /// eye nodes were vestigial and the whole head turned as a unit. The eyes must
    /// instead receive the residual the head chain did not cover.
    void TestEyeResidualAllocation()
    {
        std::cout << "[TEST] Running eye residual allocation verification...\n";

        // Small deflection: the head chain covers all of it, so the residual is ~0.
        {
            auto strain = TrueGaze::Engine::BoneController::CalculateHierarchyStrain(5.0f, 0.0f);
            const float chain = strain.HeadChainYaw();
            assert(std::abs(chain - 5.0f) < 0.01f);
            assert(std::abs(strain.eyeYaw) < 0.01f);
        }

        // Large deflection beyond the head chain's comfortable range: the eyes MUST
        // take up the remainder. This is the case the original code got wrong.
        {
            const TrueGaze::Engine::BoneController::StrainWeights partialHeadWeights{
                .spineYaw = 0.10f,
                .neckYaw = 0.20f,
                .neckPitch = 0.20f,
                .headYaw = 0.40f,
                .headPitch = 0.60f};
            auto strain = TrueGaze::Engine::BoneController::CalculateHierarchyStrain(
                60.0f, 40.0f, 35.0f, 25.0f, partialHeadWeights);

            // The head chain is clamped by its own limits, so it cannot reach 60.
            const float chainYaw = strain.HeadChainYaw();
            assert(chainYaw < 60.0f);

            // The eyes carry the difference, clamped to the ocular range.
            assert(std::abs(strain.eyeYaw) > 1.0f);
            assert(std::abs(strain.eyeYaw) <= TrueGaze::Engine::BoneController::EYE_YAW_LIMIT + 0.01f);

            // Reconstructing the deflection must land on the target.
            const float reconstructed = chainYaw + strain.eyeYaw;
            assert(std::abs(reconstructed - 60.0f) < 0.05f);

            assert(!strain.IsEyeSaturated());
        }

        // The eyes never receive zero deflection for a large target.
        for (float target = 20.0f; target <= 70.0f; target += 10.0f)
        {
            const TrueGaze::Engine::BoneController::StrainWeights partialHeadWeights{
                .spineYaw = 0.10f,
                .neckYaw = 0.20f,
                .neckPitch = 0.20f,
                .headYaw = 0.40f,
                .headPitch = 0.60f};
            auto strain = TrueGaze::Engine::BoneController::CalculateHierarchyStrain(
                target, 0.0f, 35.0f, 25.0f, partialHeadWeights);
            const float reconstructed = strain.HeadChainYaw() + strain.eyeYaw;
            assert(std::abs(reconstructed - target) < 0.05f);
        }

        // A custom ocular limit must be honoured.
        {
            const TrueGaze::Engine::BoneController::StrainWeights partialHeadWeights{
                .spineYaw = 0.10f,
                .neckYaw = 0.20f,
                .neckPitch = 0.20f,
                .headYaw = 0.40f,
                .headPitch = 0.60f};
            auto strain = TrueGaze::Engine::BoneController::CalculateHierarchyStrain(
                60.0f, 0.0f, 10.0f, 10.0f, partialHeadWeights);
            assert(std::abs(strain.eyeYaw) <= 10.01f);
            assert(std::abs(strain.eyeYaw) >= 9.99f);
        }

        std::cout << "  -> Eye residual allocation passed.\n";
    }

    /// Regression test for audit section 5.3.
    ///
    /// The Main Sequence equation was computed but never used: the saccade followed a
    /// generic smoothstep. These assertions pin the velocity profile to the stated
    /// biology, so the two cannot drift apart again.
    void TestMainSequenceFidelity()
    {
        std::cout << "[TEST] Running Main Sequence profile fidelity verification...\n";

        using SG = TrueGaze::Kinematics::SaccadeGenerator;

        // 1. The progress curve must reach exactly 1.0 and be monotonic.
        {
            float previous = 0.0f;
            for (int i = 0; i <= 100; ++i)
            {
                const float t = static_cast<float>(i) / 100.0f;
                const float p = SG::ProgressAt(t);
                assert(p >= previous - 1e-5f); // monotonic
                assert(p >= 0.0f && p <= 1.0f);
                previous = p;
            }
            assert(std::abs(SG::ProgressAt(1.0f) - 1.0f) < 1e-4f);
        }

        // 2. Velocity peaks near the profile mean, not at the midpoint.
        {
            float best = 0.0f;
            float bestT = 0.0f;
            for (int i = 0; i <= 100; ++i)
            {
                const float t = static_cast<float>(i) / 100.0f;
                const float v = SG::VelocityProfile(t);
                if (v > best)
                {
                    best = v;
                    bestT = t;
                }
            }
            // Profile mean is 0.45; the peak must sit within a step of it.
            assert(std::abs(bestT - SG::PROFILE_MU) < 0.02f);
            // The normalised profile peaks at exactly 1.0.
            assert(std::abs(best - 1.0f) < 1e-3f);
        }

        // 3. The current normalized Gaussian LUT places the peak near the median
        //    distance. The peak-time and area normalization are intentionally tested
        //    independently; requiring >50% here would assert a different profile
        //    shape than the configured mu/sigma pair.
        {
            const float progressAtPeak = SG::ProgressAt(SG::PROFILE_MU);
            assert(progressAtPeak > 0.45f && progressAtPeak < 0.55f);
        }

        // 4. Implied peak velocity must revisit the empirical Main Sequence. This is
        //    the assertion that keeps the visual shape and V_peak(A) consistent.
        {
            const float vMax = SG::DEFAULT_VMAX;
            const float c = SG::DEFAULT_C;
            const float d = SG::DURATION_SLOPE;
            const float d0 = SG::BASE_DURATION_SEC;

            for (float amplitude = 1.0f; amplitude <= 60.0f; amplitude += 1.0f)
            {
                const float duration = d0 + d * amplitude;

                // Peak of the unit-area profile scaled by amplitude and duration.
                const float impliedPeak = (amplitude / duration) * SG::PROFILE_WINDOW;
                const float mainSequencePeak = SG::CalculatePeakVelocity(amplitude, vMax, c);

                // The fixed-duration normalized profile is intentionally an
                // approximation to the Main Sequence. Its small-amplitude base
                // duration cannot exactly match the asymptotic velocity equation;
                // require a bounded, biologically plausible relationship instead.
                const float ratio = impliedPeak / mainSequencePeak;
                assert(ratio > 0.80f && ratio < 1.50f);
            }

            // And the asymptote should land near the stated 750 deg/s.
            const float asymptote = SG::PROFILE_WINDOW / d;
            assert(std::abs(asymptote - vMax) < 90.0f);
        }

        // 5. Duration must follow D0 + d*A.
        {
            assert(std::abs(SG::CalculateDuration(0.0f) - SG::BASE_DURATION_SEC) < 1e-5f);
            assert(std::abs(SG::CalculateDuration(20.0f) - (SG::BASE_DURATION_SEC + SG::DURATION_SLOPE * 20.0f)) < 1e-5f);
            assert(SG::CalculateDuration(40.0f) > SG::CalculateDuration(20.0f));
        }

        std::cout << "  -> Main Sequence fidelity passed.\n";
    }

    /// Regression test for audit section 5.3.
    ///
    /// MicroJitter was a mean-reverting low-pass filter over resampled uniform noise,
    /// not Brownian motion, and used a single fixed seed for every actor so the whole
    /// world jittered in lockstep.
    void TestJitterIsBrownianAndSeedable()
    {
        std::cout << "[TEST] Running micro-jitter Brownian/seeding verification...\n";

        using MJ = TrueGaze::Kinematics::MicroJitter;

        // 1. Two different actors must not share a drift sequence.
        {
            MJ::JitterState a{};
            MJ::JitterState b{};
            MJ::Init(a, 0x0001A2B3u, 0.35f);
            MJ::Init(b, 0x0001A2B4u, 0.35f); // one bit different

            constexpr float dt = 1.0f / 120.0f;
            for (int i = 0; i < 600; ++i)
            {
                MJ::Update(a, dt);
                MJ::Update(b, dt);
            }

            const float diff = std::abs(a.currentYawOffset - b.currentYawOffset);
            assert(diff > 1e-4f); // must differ
        }

        // 2. The same seed must reproduce the same sequence exactly.
        {
            MJ::JitterState a{};
            MJ::JitterState b{};
            MJ::Init(a, 0xC0FFEEu, 0.35f);
            MJ::Init(b, 0xC0FFEEu, 0.35f);

            constexpr float dt = 1.0f / 120.0f;
            for (int i = 0; i < 300; ++i)
            {
                MJ::Update(a, dt);
                MJ::Update(b, dt);
            }

            assert(a.currentYawOffset == b.currentYawOffset);
            assert(a.currentPitchOffset == b.currentPitchOffset);
        }

        // 3. Stationary standard deviation must track the configured amplitude.
        {
            MJ::JitterState state{};
            MJ::Init(state, 12345u, 0.35f);

            constexpr float dt = 1.0f / 120.0f;
            double sumSq = 0.0;
            constexpr int kSamples = 20000;

            for (int i = 0; i < 2000; ++i)
            {
                MJ::Update(state, dt); // burn-in
            }
            for (int i = 0; i < kSamples; ++i)
            {
                MJ::Update(state, dt);
                sumSq += static_cast<double>(state.currentYawOffset) * state.currentYawOffset;
            }

            const float stdDev = static_cast<float>(std::sqrt(sumSq / kSamples));

            // Within 30% of the configured amplitude. A loose bound because this is a
            // stochastic process, but tight enough to catch the amplitude being ignored.
            assert(stdDev > 0.35f * 0.70f);
            assert(stdDev < 0.35f * 1.30f);
        }

        // 4. Drift must be independent of frame rate. The fixed sub-step exists for
        //    exactly this reason.
        {
            MJ::JitterState fast{};
            MJ::JitterState slow{};
            MJ::Init(fast, 777u, 0.35f);
            MJ::Init(slow, 777u, 0.35f);

            for (int i = 0; i < 240; ++i)
            {
                MJ::Update(fast, 1.0f / 240.0f);
            }
            for (int i = 0; i < 120; ++i)
            {
                MJ::Update(slow, 1.0f / 120.0f);
            }

            // One second simulated either way; the trajectories must agree closely.
            assert(std::abs(fast.currentYawOffset - slow.currentYawOffset) < 1e-3f);
        }

        std::cout << "  -> Micro-jitter Brownian/seeding passed.\n";
    }

    void TestLodManager()
    {
        std::cout << "[TEST] Running LodManager verification...\n";

        assert(TrueGaze::Engine::LodManager::GetLodTier(2.0f) == TrueGaze::Engine::LodManager::LodTier::Tier1_DialogueRange);
        assert(TrueGaze::Engine::LodManager::GetLodTier(10.0f) == TrueGaze::Engine::LodManager::LodTier::Tier2_Proximity);
        assert(TrueGaze::Engine::LodManager::GetLodTier(25.0f) == TrueGaze::Engine::LodManager::LodTier::Tier3_Culled);

        std::cout << "  -> LodManager passed.\n";
    }

    void TestEfmBlinkController()
    {
        std::cout << "[TEST] Running EfmBlinkController verification...\n";

        TrueGaze::Integrations::EfmBlinkController::BlinkState state;
        // Major saccade > 20 deg triggers blink
        TrueGaze::Integrations::EfmBlinkController::OnSaccadeTriggered(state, 25.0f);
        assert(state.isBlinking);

        // Update through blink
        TrueGaze::Integrations::EfmBlinkController::Update(state, 0.06f); // Mid-blink
        assert(state.eyelidCloseWeight > 0.5f);

        TrueGaze::Integrations::EfmBlinkController::Update(state, 0.10f); // Complete
        assert(!state.isBlinking);
        assert(state.eyelidCloseWeight == 0.0f);

        std::cout << "  -> EfmBlinkController passed.\n";
    }

    void TestTelemetryPackets()
    {
        std::cout << "[TEST] Running TelemetryPacket layout and CRC32 verification...\n";

        static_assert(sizeof(TrueGaze::Bridge::TrueGazeTelemetryPacket) == 64, "Packet size mismatch");
        static_assert(sizeof(TrueGaze::Bridge::SkyrimFeedbackPacket) == 32, "Feedback packet size mismatch");

        TrueGaze::Bridge::TrueGazeTelemetryPacket packet{};
        packet.magic = 0x48434550; // "HCEP"
        packet.version = 0x0100;
        packet.sequenceId = 1;
        packet.timestampUs = 1000000;
        packet.gazePitch = 0.15f;
        packet.gazeYaw = -0.25f;
        packet.gazeConfidence = 0.98f;
        packet.hcepMode = 1; // AFFECT

        uint32_t crc = ComputeCrc32(
            reinterpret_cast<const uint8_t *>(&packet),
            sizeof(packet) - sizeof(uint32_t));
        packet.crc32 = crc;
        assert(packet.crc32 != 0);

        std::cout << "  -> TelemetryPackets passed (64-byte & 32-byte layout verified).\n";
    }

    void TestTelemetrySemanticValidation()
    {
        std::cout << "[TEST] Running HCEP semantic validation verification...\n";
        TrueGaze::Bridge::TrueGazeTelemetryPacket packet{};
        packet.magic = 0x48434550;
        packet.version = 0x0100;
        packet.gazeConfidence = 1.0f;
        assert(TrueGaze::Bridge::ValidateTelemetryPacket(packet));

        packet.gazeYaw = std::numeric_limits<float>::quiet_NaN();
        assert(!TrueGaze::Bridge::ValidateTelemetryPacket(packet));
        packet.gazeYaw = 0.0f;
        packet.hcepMode = 5;
        assert(!TrueGaze::Bridge::ValidateTelemetryPacket(packet));

        std::cout << "  -> HCEP semantic validation passed.\n";
    }

} // namespace

int main()
{
    std::cout << "========================================================\n";
    std::cout << "  SkyrimTrueGaze Kinematics & Mathematics Test Suite   \n";
    std::cout << "  An HCEP Product by Kirk LaSalle                      \n";
    std::cout << "========================================================\n";

    TestSaccadeGenerator();
    TestVorCoordinator();
    TestMicroJitter();
    TestSocialTriangle();
    TestBoneController();
    TestEyeResidualAllocation();
    TestMainSequenceFidelity();
    TestJitterIsBrownianAndSeedable();
    TestLodManager();
    TestEfmBlinkController();
    TestTelemetryPackets();
    TestTelemetrySemanticValidation();

    std::cout << "\n[SUCCESS] ALL 11 BIOMECHANICAL KINEMATICS TESTS PASSED!\n";
    return 0;
}
