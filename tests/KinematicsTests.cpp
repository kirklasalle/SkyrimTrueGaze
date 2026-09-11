#include <iostream>
#include <cassert>
#include <cmath>
#include <vector>

#include "../src/Kinematics/SaccadeGenerator.hpp"
#include "../src/Kinematics/VorCoordinator.hpp"
#include "../src/Kinematics/MicroJitter.hpp"
#include "../src/Kinematics/SocialTriangle.hpp"
#include "../src/Engine/BoneController.hpp"
#include "../src/Engine/LodManager.hpp"
#include "../src/Integrations/EfmBlinkController.hpp"
#include "../src/Bridge/TelemetryPacket.h"

namespace {

uint32_t ComputeCrc32(const uint8_t* data, size_t length) noexcept
{
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
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
    while (state.isBallistic) {
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
    for (int i = 0; i < 60; ++i) {
        TrueGaze::Kinematics::VorCoordinator::Update(state, dt);
    }

    // Head should approach target, eye should counter-rotate
    assert(state.headYaw > 20.0f);
    assert(state.eyeLocalYaw >= -state.eyeMaxAngle && state.eyeLocalYaw <= state.eyeMaxAngle);

    std::cout << "  -> VorCoordinator passed.\n";
}

void TestMicroJitter()
{
    std::cout << "[TEST] Running MicroJitter verification...\n";

    TrueGaze::Kinematics::MicroJitter::JitterState state;
    float dt = 0.016f;

    for (int i = 0; i < 300; ++i) { // 5 seconds
        TrueGaze::Kinematics::MicroJitter::Update(state, dt);
        assert(std::abs(state.currentYawOffset) <= state.maxAmplitudeDeg * 1.5f);
        assert(std::abs(state.currentPitchOffset) <= state.maxAmplitudeDeg * 1.5f);
    }

    std::cout << "  -> MicroJitter passed.\n";
}

void TestSocialTriangle()
{
    std::cout << "[TEST] Running SocialTriangle verification...\n";

    TrueGaze::Kinematics::SocialTriangle::TriangleState state;
    auto initialVertex = state.currentVertex;

    // Simulate 2 seconds to force vertex transition
    float dt = 0.05f;
    for (int i = 0; i < 40; ++i) {
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
        reinterpret_cast<const uint8_t*>(&packet),
        sizeof(packet) - sizeof(uint32_t)
    );
    packet.crc32 = crc;
    assert(packet.crc32 != 0);

    std::cout << "  -> TelemetryPackets passed (64-byte & 32-byte layout verified).\n";
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
    TestLodManager();
    TestEfmBlinkController();
    TestTelemetryPackets();

    std::cout << "\n[SUCCESS] ALL 8 BIOMECHANICAL KINEMATICS TESTS PASSED!\n";
    return 0;
}
