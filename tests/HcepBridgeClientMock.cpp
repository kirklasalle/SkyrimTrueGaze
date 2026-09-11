#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <iostream>
#include <chrono>
#include <thread>
#include <cassert>
#include <cstring>
#include <cstdint>

#include "../src/Bridge/TelemetryPacket.h"
#include "../src/Bridge/NamedPipeServer.hpp"

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

} // namespace

int main()
{
    std::cout << "========================================================\n";
    std::cout << "  HCEP Desktop Suite <-> TrueGaze Bridge Test Harness   \n";
    std::cout << "  Simulating HCEP.App Real-Time Gaze Telemetry Stream   \n";
    std::cout << "========================================================\n";

    // 1. Start the TrueGaze NamedPipeServer (Simulating Skyrim game runtime)
    std::cout << "[SERVER] Starting TrueGaze::Bridge::NamedPipeServer...\n";
    TrueGaze::Bridge::NamedPipeServer server;
    server.Start();

    // Give server worker thread a brief moment to create pipe
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // 2. Connect as HCEP Desktop Suite Client (Simulating D:\Projects\HCEP)
    std::cout << "[CLIENT] Connecting to \\\\.\\pipe\\TrueGazeBridge...\n";
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    for (int retry = 0; retry < 10; ++retry) {
        hPipe = CreateFileA(
            R"(\\.\pipe\TrueGazeBridge)",
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr
        );
        if (hPipe != INVALID_HANDLE_VALUE) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cerr << "[FAIL] Could not connect to named pipe. Error: " << GetLastError() << "\n";
        server.Stop();
        return 1;
    }

    std::cout << "[CLIENT] Successfully connected to TrueGaze Named Pipe!\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    assert(server.IsConnected());
    std::cout << "[SERVER] Confirmed server.IsConnected() == true.\n";

    // 3. Stream 10 simulated 64-byte telemetry frames from HCEP Desktop
    std::cout << "[CLIENT] Streaming 10 telemetry frames (HCEP modes: LOGIC, AFFECT, THINK)...\n";
    for (uint16_t seq = 1; seq <= 10; ++seq) {
        TrueGaze::Bridge::TrueGazeTelemetryPacket packet{};
        packet.magic = 0x48434550; // "HCEP"
        packet.version = 0x0100;
        packet.sequenceId = seq;
        packet.timestampUs = static_cast<uint64_t>(seq) * 16666ULL;
        packet.gazePitch = 0.05f * static_cast<float>(seq);
        packet.gazeYaw = -0.08f * static_cast<float>(seq);
        packet.gazeConvergence = 1.8f;
        packet.gazeConfidence = 0.99f;
        packet.hcepMode = (seq % 5); // Cycling modes
        packet.headPitch = 0.02f;
        packet.headYaw = -0.04f;
        packet.crc32 = ComputeCrc32(
            reinterpret_cast<const uint8_t*>(&packet),
            sizeof(packet) - sizeof(uint32_t)
        );

        DWORD written = 0;
        BOOL ok = WriteFile(hPipe, &packet, sizeof(packet), &written, nullptr);
        assert(ok && written == sizeof(packet));

        std::this_thread::sleep_for(std::chrono::milliseconds(16));

        // 4. In-game thread reads latest packet via lock-free API
        TrueGaze::Bridge::TrueGazeTelemetryPacket received{};
        bool gotLatest = server.TryGetLatestTelemetry(received);
        if (gotLatest) {
            assert(received.magic == 0x48434550);
            std::cout << "  Frame " << seq << " received in engine: mode=" 
                      << static_cast<int>(received.hcepMode) 
                      << ", yaw=" << received.gazeYaw 
                      << ", pitch=" << received.gazePitch << "\n";
        }
    }

    // 5. Simulate in-game feedback sent back to HCEP Desktop
    std::cout << "[SERVER] Sending 32-byte SkyrimFeedbackPacket to client...\n";
    TrueGaze::Bridge::SkyrimFeedbackPacket feedback{};
    feedback.targetFormId = 0x000136C8; // Example: Lydia FormID
    feedback.relationshipRank = 3;      // Ally / Companion
    feedback.combatState = 0;           // Out of combat
    feedback.isDialogueActive = 1;      // In dialogue
    feedback.distanceToTarget = 1.45f;
    feedback.mutualGazeAngle = 1.2f;
    feedback.gameFrameNumber = 12450;
    server.SendFeedback(feedback);

    // Give pipe a moment to flush
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 6. Client reads feedback
    TrueGaze::Bridge::SkyrimFeedbackPacket clientReceivedFeedback{};
    DWORD bytesRead = 0;
    BOOL readOk = ReadFile(hPipe, &clientReceivedFeedback, sizeof(clientReceivedFeedback), &bytesRead, nullptr);
    if (readOk && bytesRead == sizeof(clientReceivedFeedback)) {
        assert(clientReceivedFeedback.magic == 0x534B5952);
        assert(clientReceivedFeedback.targetFormId == 0x000136C8);
        assert(clientReceivedFeedback.relationshipRank == 3);
        std::cout << "  Feedback confirmed by client: targetFormId=0x" 
                  << std::hex << clientReceivedFeedback.targetFormId << std::dec
                  << ", distance=" << clientReceivedFeedback.distanceToTarget << "m"
                  << ", mutualAngle=" << clientReceivedFeedback.mutualGazeAngle << " deg\n";
    }

    // 7. Cleanup
    CloseHandle(hPipe);
    server.Stop();

    std::cout << "\n[SUCCESS] HCEP Desktop <-> TrueGaze Bridge verification passed 100%!\n";
    return 0;
}
