# TRUE GAZE™ — HCEP Bridge IPC Specification
### Author: Kirk LaSalle
### Platform: Windows x64 Named Pipes / Zero-Copy Shared Memory
### Connecting: `HCEP.App` (Desktop Perception Suite) $\longleftrightarrow$ `TrueGaze.dll` (Skyrim SE/AE/VR Plugin)

---

## 1. Architectural Architecture & Objective

The **True Gaze™ HCEP Bridge** enables real-time bi-directional telemetry exchange between:
1. **The HCEP Desktop Platform** (`D:\Projects\HCEP`): Capturing real-world user eye movements, facial Action Units, head pose, blink states, and the 5 HCEP cognitive modes via Kinect v1 or standard USB webcams.
2. **The TrueGaze Skyrim Plugin** (`D:\Projects\SkyrimTrueGaze`): Injecting the player's true gaze orientation, determining genuine mutual eye contact with in-game actors, and adjusting NPC behavioral kinematics in real time.

```
┌───────────────────────────────────────────────────┐
│              HCEP Desktop Suite (C#)              │
│    (Kinect / Webcam Sensor ──► PnP Gaze Solver)   │
└─────────────────────────┬─────────────────────────┘
                          │
         [TrueGazeTelemetryPacket (64 bytes)]
                          │
                          ▼
            Windows Asynchronous Named Pipe
              (\\.\pipe\TrueGazeBridge)
                          ▲
                          │
        [SkyrimFeedbackPacket (32 bytes)]
                          │
┌─────────────────────────┴─────────────────────────┐
│              TrueGaze.dll (Native C++)            │
│   (Lock-Free Consumer ──► Actor Kinematics / OAR) │
└───────────────────────────────────────────────────┘
```

---

## 2. Pipe Configuration & Lifetime

* **Pipe Name:** `\\.\pipe\TrueGazeBridge`
* **Direction:** Duplex (`PIPE_ACCESS_DUPLEX`)
* **Mode:** Message-type byte stream (`PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_NOWAIT`)
* **Buffer Size:** 4096 bytes (in / out)
* **Tick Rate:** 60 Hz – 120 Hz (synchronized with render frame or sensor frame)
* **Roundtrip Latency:** $< 0.35 \text{ ms}$ on local IPC.

---

## 3. Wire Protocol Structs

### 3.1. Outbound (HCEP Desktop $\longrightarrow$ TrueGaze Plugin): `TrueGazeTelemetryPacket`
Total size: **64 bytes** (strictly 8-byte aligned, zero padding waste).

```cpp
#pragma pack(push, 1)
struct TrueGazeTelemetryPacket
{
    // --- Header (8 bytes) ---
    uint32_t magic;           // 0x48434550 ("HCEP" in ASCII)
    uint16_t version;         // Protocol version (0x0100 -> v1.0.0)
    uint16_t sequenceId;      // Monotonically increasing frame counter

    // --- High-Precision Timestamp (8 bytes) ---
    uint64_t timestampUs;     // Microseconds since session start

    // --- Player Real-World Gaze Vector (16 bytes) ---
    float gazePitch;          // Vertical look angle in radians (-pi/2 to +pi/2)
    float gazeYaw;            // Horizontal look angle in radians (-pi to +pi)
    float gazeConvergence;    // Estimated focal distance in meters
    float gazeConfidence;     // Tracking confidence: 0.0f (lost) to 1.0f (solid)

    // --- Cognitive & Emotional State (8 bytes) ---
    uint8_t hcepMode;         // 0=LOGIC, 1=AFFECT, 2=SPIRIT, 3=HEART, 4=THINK
    uint8_t cognitiveState;   // 12 classified cognitive states (e.g. Engaged, Distracted)
    int8_t  emotionalValence; // Range -100 (Hostile/Sad) to +100 (Warm/Happy)
    uint8_t blinkBitmask;     // Bit 0 = Left Blink, Bit 1 = Right Blink
    uint8_t socialTriangle;   // 0=None, 1=Left Eye, 2=Right Eye, 3=Mouth
    uint8_t reserved[3];      // Alignment padding (must be 0x00)

    // --- Player Head Pose (12 bytes) ---
    float headPitch;          // Head pitch rotation in radians
    float headYaw;            // Head yaw rotation in radians
    float headRoll;           // Head roll rotation in radians

    // --- Integrity & State (12 bytes) ---
    uint32_t trackedPersonId; // ID of active user tracked by ArcFace
    float mutualGazeHoldSec;  // Sustained mutual gaze duration
    uint32_t crc32;           // CRC-32 checksum of preceding 60 bytes
};
#pragma pack(pop)
```

### 3.2. Inbound Feedback (TrueGaze Plugin $\longrightarrow$ HCEP Desktop): `SkyrimFeedbackPacket`
Total size: **32 bytes**.

```cpp
#pragma pack(push, 1)
struct SkyrimFeedbackPacket
{
    uint32_t magic;           // 0x534B5952 ("SKYR" in ASCII)
    uint16_t version;         // Protocol version (0x0100)
    uint16_t reserved;

    uint32_t targetFormId;    // FormID of the NPC currently centered in screen space
    int16_t  relationshipRank;// Skyrim actor relationship rank (-4 to +4)
    uint8_t  combatState;     // 0=Out of combat, 1=Combat, 2=Searching
    uint8_t  isDialogueActive;// 1 if player is currently in dialogue menu, 0 otherwise

    float    distanceToTarget;// 3D world distance in meters
    float    mutualGazeAngle; // Angle between NPC gaze ray and Player gaze ray in degrees
    uint32_t gameFrameNumber; // Skyrim internal frame counter
    uint32_t crc32;           // CRC-32 checksum
};
#pragma pack(pop)
```

---

## 4. Concurrency & Thread-Safety Model

To guarantee **zero frame drops** in Skyrim:
1. **Background I/O Worker**: The Named Pipe client runs on a dedicated high-priority background thread (`std::jthread`).
2. **Double-Buffered Lock-Free Exchange**:
   * The background thread writes incoming packets into an atomic triple-buffer or double-buffer.
   * The main game thread reads the latest valid packet using `std::atomic<TelemetryPacket*>::load(std::memory_order_acquire)` in under **10 nanoseconds**.
   * The game's render and animation loops **never block or wait for I/O**.
3. **Auto-Fallback / Degraded Mode**:
   * If the pipe is disconnected or `HCEP.App` is closed, `TrueGaze.dll` automatically switches to **Mode 1: Autonomous Edge** within 1 frame.
   * Reconnection attempts occur in the background once every 3.0 seconds without user intervention.
