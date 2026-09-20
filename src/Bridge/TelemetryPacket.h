#pragma once

#include <cmath>
#include <cstdint>

namespace TrueGaze::Bridge
{

#pragma pack(push, 1)

    /// @brief 64-byte aligned real-time telemetry packet received from HCEP Desktop Suite (D:\Projects\HCEP).
    struct TrueGazeTelemetryPacket
    {
        // --- Header (8 bytes) ---
        uint32_t magic;      // 0x48434550 ("HCEP" ASCII)
        uint16_t version;    // Protocol version (e.g. 0x0100 -> v1.0)
        uint16_t sequenceId; // Monotonically increasing frame index

        // --- Timestamp (8 bytes) ---
        uint64_t timestampUs; // Microseconds since session start

        // --- Player Real-World Gaze Vector (16 bytes) ---
        float gazePitch;       // Look angle up/down in radians (-pi/2 to +pi/2)
        float gazeYaw;         // Look angle left/right in radians (-pi to +pi)
        float gazeConvergence; // Estimated focal distance in meters
        float gazeConfidence;  // 0.0f (lost) to 1.0f (solid tracking)

        // --- Cognitive & Emotional State (8 bytes) ---
        uint8_t hcepMode;        // 0=LOGIC, 1=AFFECT, 2=SPIRIT, 3=HEART, 4=THINK
        uint8_t cognitiveState;  // 12 classified cognitive states
        int8_t emotionalValence; // Range: -100 to +100
        uint8_t blinkBitmask;    // Bit 0 = Left Eye Blink, Bit 1 = Right Eye Blink
        uint8_t socialTriangle;  // 0=None, 1=Left Eye, 2=Right Eye, 3=Mouth
        uint8_t reserved[3];     // Padding (0x00)

        // --- Player Head Pose (12 bytes) ---
        float headPitch; // Head pitch rotation in radians
        float headYaw;   // Head yaw rotation in radians
        float headRoll;  // Head roll rotation in radians

        // --- Synchronization & Integrity (12 bytes) ---
        uint32_t trackedPersonId; // Active tracked person ID
        float mutualGazeHoldSec;  // Sustained mutual gaze duration
        uint32_t crc32;           // CRC32 checksum
    };
    static_assert(sizeof(TrueGazeTelemetryPacket) == 64, "TrueGazeTelemetryPacket must be exactly 64 bytes");

    /// @brief 32-byte feedback packet sent from TrueGaze.dll back to HCEP Desktop Suite.
    struct SkyrimFeedbackPacket
    {
        uint32_t magic;   // 0x534B5952 ("SKYR" ASCII)
        uint16_t version; // Protocol version (0x0100)
        uint16_t reserved;

        uint32_t targetFormId;    // FormID of the targeted NPC
        int16_t relationshipRank; // Relationship rank (-4 to +4)
        uint8_t combatState;      // 0=Peace, 1=Combat, 2=Searching
        uint8_t isDialogueActive; // 1 if dialogue menu is open

        float distanceToTarget;   // World distance in meters
        float mutualGazeAngle;    // Degrees between NPC gaze ray and Player gaze ray
        uint32_t gameFrameNumber; // Skyrim internal frame counter
        uint32_t crc32;           // CRC32 checksum
    };
    static_assert(sizeof(SkyrimFeedbackPacket) == 32, "SkyrimFeedbackPacket must be exactly 32 bytes");

    /// @brief Reject malformed or semantically impossible HCEP input before it reaches
    /// the game-thread gaze solver. CRC protects transport integrity; this protects the
    /// solver from NaN, infinity, unsupported modes, and impossible sensor values.
    inline bool ValidateTelemetryPacket(const TrueGazeTelemetryPacket &packet) noexcept
    {
        constexpr uint32_t kHcepMagic = 0x48434550; // "HCEP"
        constexpr uint16_t kProtocolVersion = 0x0100;
        constexpr float kPi = 3.14159265358979323846f;

        if (packet.magic != kHcepMagic || packet.version != kProtocolVersion)
        {
            return false;
        }
        if (packet.hcepMode > 4 || packet.cognitiveState > 11 || packet.socialTriangle > 3 ||
            (packet.blinkBitmask & 0xFCu) != 0 || packet.reserved[0] != 0 ||
            packet.reserved[1] != 0 || packet.reserved[2] != 0)
        {
            return false;
        }
        if (packet.emotionalValence < -100 || packet.emotionalValence > 100)
        {
            return false;
        }
        if (!std::isfinite(packet.gazePitch) || !std::isfinite(packet.gazeYaw) ||
            !std::isfinite(packet.gazeConvergence) || !std::isfinite(packet.gazeConfidence) ||
            !std::isfinite(packet.headPitch) || !std::isfinite(packet.headYaw) ||
            !std::isfinite(packet.headRoll) || !std::isfinite(packet.mutualGazeHoldSec))
        {
            return false;
        }
        return packet.gazePitch >= -kPi * 0.5f && packet.gazePitch <= kPi * 0.5f &&
               packet.gazeYaw >= -kPi && packet.gazeYaw <= kPi &&
               packet.gazeConvergence >= 0.0f && packet.gazeConvergence <= 100.0f &&
               packet.gazeConfidence >= 0.0f && packet.gazeConfidence <= 1.0f &&
               std::abs(packet.headPitch) <= kPi * 0.5f && std::abs(packet.headYaw) <= kPi &&
               std::abs(packet.headRoll) <= kPi && packet.mutualGazeHoldSec >= 0.0f &&
               packet.mutualGazeHoldSec <= 3600.0f;
    }

#pragma pack(pop)

} // namespace TrueGaze::Bridge
