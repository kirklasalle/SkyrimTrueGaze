#pragma once
// ─────────────────────────────────────────────────────────────────────────────
// TRUEGAZE™ — HCEP Bridge Binary Telemetry Protocol Schema
// Copyright © 2026 Kirk LaSalle. All rights reserved.
//
// Platform: Windows x64 Named Pipes (\\.\pipe\TrueGazeBridge)
// Connecting: HCEP.App (C# WPF Desktop) <--> TrueGaze.dll (Native C++23)
// ─────────────────────────────────────────────────────────────────────────────

#include <cstdint>
#include <type_traits>

namespace TrueGaze::Bridge
{
    inline constexpr uint32_t HCEP_MAGIC = 0x48434550; // "HCEP" in ASCII
    inline constexpr uint32_t SKYR_MAGIC = 0x534B5952; // "SKYR" in ASCII
    inline constexpr uint16_t PROTOCOL_VERSION = 0x0100; // v1.0.0
    inline constexpr const char* DEFAULT_PIPE_NAME = "\\\\.\\pipe\\TrueGazeBridge";

#pragma pack(push, 1)

    // Outbound from HCEP Desktop Sensor to TrueGaze Skyrim Plugin (64 Bytes)
    struct TrueGazeTelemetryPacket
    {
        // --- Header (8 bytes) ---
        uint32_t magic;          // 0x48434550 ("HCEP")
        uint16_t version;        // 0x0100
        uint16_t sequenceId;     // Monotonic frame counter

        // --- High-Precision Timestamp (8 bytes) ---
        uint64_t timestampUs;    // Microseconds since session start

        // --- Real-World Gaze Vector (16 bytes) ---
        float gazePitch;         // Look angle in radians (-pi/2 to +pi/2)
        float gazeYaw;           // Look angle in radians (-pi to +pi)
        float gazeConvergence;   // Estimated focal distance in meters
        float gazeConfidence;    // 0.0f (lost) to 1.0f (solid tracking lock)

        // --- Cognitive & Emotional State (8 bytes) ---
        uint8_t hcepMode;        // 0=LOGIC, 1=AFFECT, 2=SPIRIT, 3=HEART, 4=THINK
        uint8_t cognitiveState;  // 12 classified states (Engaged, Distracted, etc.)
        int8_t  emotionalValence;// Range -100 to +100
        uint8_t blinkBitmask;    // Bit 0 = Left Eye, Bit 1 = Right Eye
        uint8_t fixatedQuadrant; // 0=Top-Left, 1=Top-Right, 2=Bot-Left, 3=Bot-Right
        uint8_t reserved1;
        uint8_t reserved2;
        uint8_t reserved3;

        // --- Head Pose & Translation (24 bytes) ---
        float headPitch;         // Head tilt in radians
        float headYaw;           // Head pan in radians
        float headRoll;          // Head tilt roll in radians
        float headPosX;          // Sensor X in meters
        float headPosY;          // Sensor Y in meters
        float headPosZ;          // Sensor Z in meters

        // Verification utility
        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return magic == HCEP_MAGIC && version == PROTOCOL_VERSION;
        }
    };
    static_assert(sizeof(TrueGazeTelemetryPacket) == 64, "TrueGazeTelemetryPacket must be exactly 64 bytes");
    static_assert(std::is_trivially_copyable_v<TrueGazeTelemetryPacket>, "Packet must be trivially copyable for zero-copy IPC");

    // Inbound Feedback from TrueGaze Skyrim Plugin to HCEP Desktop (32 Bytes)
    struct SkyrimFeedbackPacket
    {
        // --- Header (8 bytes) ---
        uint32_t magic;                 // 0x534B5952 ("SKYR")
        uint16_t version;               // 0x0100
        uint16_t reservedHeader;

        // --- Engine Frame & Target (8 bytes) ---
        uint32_t frameId;               // Skyrim engine frame index
        uint32_t focusedActorFormId;    // 32-bit FormID of targeted NPC (0 = None)

        // --- Eye Contact Metrics (12 bytes) ---
        float eyeContactDurationSec;    // Continuous mutual gaze time
        float mutualGazeAlignment;      // Dot product (1.0f = perfect mutual eye lock)
        float targetDistanceMeters;     // Distance from player eyes to NPC eyes

        // --- Reaction & State (4 bytes) ---
        uint8_t npcReactionState;       // 0=Neutral, 1=Attentive, 2=Intimidated, 3=Flattered
        uint8_t dialogueActive;         // 1 if player is actively talking to NPC
        uint8_t reserved1;
        uint8_t reserved2;

        // Verification utility
        [[nodiscard]] constexpr bool isValid() const noexcept
        {
            return magic == SKYR_MAGIC && version == PROTOCOL_VERSION;
        }
    };
    static_assert(sizeof(SkyrimFeedbackPacket) == 32, "SkyrimFeedbackPacket must be exactly 32 bytes");
    static_assert(std::is_trivially_copyable_v<SkyrimFeedbackPacket>, "Feedback packet must be trivially copyable for zero-copy IPC");

#pragma pack(pop)

} // namespace TrueGaze::Bridge
