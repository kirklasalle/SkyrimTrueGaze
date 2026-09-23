# Product Requirements Document (PRD)

## TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine

**Product Name:** TrueGaze™ (`TrueGaze.dll`)  
**Workspace:** `D:\Projects\SkyrimTrueGaze`  
**Parent Ecosystem:** Kirk LaSalle's Human Communication Eye Protocol (HCEP) (`D:\Projects\HCEP`)  
**Author & Product Owner:** Kirk LaSalle  
**Version:** 1.0.3  
**Status:** Production Release ([Nexus Mods #192480](https://www.nexusmods.com/skyrimspecialedition/mods/192480))  
**Target Games:** The Elder Scrolls V: Skyrim (Special Edition 1.5.97, Anniversary Edition 1.6.318–1.6.1170+, Skyrim VR 1.4.15)

---

## 1. Executive Summary & Vision

### 1.1 Problem Statement

For over two decades, real-time 3D game engines have suffered from what cognitive psychologists and animators term **"Dead-Eye Syndrome."** In Bethesda's Creation Engine (and games like *Skyrim*), character head-tracking behaves like an unweighted robotic surveillance camera:

1. Neck and head bones linearly interpolate toward the target at fixed angular velocity.
2. The eyes remain static or locked dead-center in the skull sockets.
3. Fixations are unnaturally frozen without micro-movements, triggering the uncanny valley.
4. Characters maintain uncomfortable, psychotic 100% unbroken eye contact during conversation.
5. Eye movements are disconnected from cognitive load, emotional valence, and mutual reciprocity.

### 1.2 Product Vision

**TrueGaze™** is the first true biological oculomotor and head-kinematics engine for video games. Developed as a first-party gaming product derived from Kirk LaSalle's proprietary **Human Communication Eye Protocol (HCEP)**, TrueGaze replaces hardcoded neck snapping with empirical neuroscience formulas modeling:

- **Ballistic Saccades** obeying the biological Main Sequence equation.
- **Vestibulo-Ocular Reflex (VOR)**: Low-inertia eyes lead target acquisition in 20–30ms; higher-inertia head/neck follow in 120–250ms while eyes counter-rotate to preserve foveal gaze lock.
- **Micro-Saccadic Brownian Drift (1–3 Hz)** preventing fixation freeze.
- **Cognitive Gaze Aversion (HCEP THINK Mode)** where characters look away during cognitive recall or processing.
- **Social Triangle Scanning (HCEP AFFECT Mode)** where gaze naturally circulates between Left Eye, Right Eye, and Mouth.
- **Dual-Mode Operation**:
  - *Mode 1 (Autonomous Edge)*: Runs entirely in-engine at 60–144+ FPS for all NPCs and creatures.
  - *Mode 2 (Connected HCEP)*: Connects via low-latency Windows Named Pipe (`\\.\pipe\TrueGazeBridge`) to Kirk LaSalle's HCEP Desktop Suite (`D:\Projects\HCEP`) to stream the human player's real webcam/Kinect gaze into Skyrim, achieving genuine bi-directional mutual eye contact.

---

## 2. Stakeholders & User Personas

| Persona | Description | Needs & Desired Outcomes |
| :--- | :--- | :--- |
| **Skyrim Modder / Player** | Enthusiast playing heavily modded SE/AE/VR Skyrim. | Drop-in SKSE plugin, zero configuration required, seamless performance (< 0.15ms per frame), INI tuning, zero CTDs. |
| **Immersive Roleplayer** | Player seeking deep character intimacy and realism. | Believable eye contact, NPCs that blush or glance away during intense conversations, mutual eye contact rewards. |
| **Animation Modder** | Creator building custom animations using Open Animation Replacer (OAR). | Native condition functions (`TrueGaze_IsMode`, `TrueGaze_IsMutualGaze`, `TrueGaze_GetGazeRegion`) to trigger custom gestures. |
| **HCEP Hardware User** | User running the HCEP Desktop perception platform with webcam/Kinect. | High-speed telemetry bridge streaming player eye fixations and cognitive modes into Skyrim with sub-millisecond IPC latency. |

---

## 3. Supported Environment & Platforms

- **Game Versions:**
  - Skyrim Special Edition (SE) 1.5.97
  - Skyrim Anniversary Edition (AE) 1.6.640, 1.6.1130, 1.6.1170
  - Skyrim VR 1.4.15
- **Runtime Dependencies:**
  - SKSE64 (Skyrim Script Extender 64-bit)
  - Address Library for SKSE Plugins (for multi-version memory offsets)
  - *Optional:* Open Animation Replacer (OAR) >= 2.0.0
  - *Optional:* Expressive Facegen Morphs (EFM) / Expressive Facial Animation (EFA)
- **Explicit non-dependencies (vanilla-UI design):**
  - **SkyUI is NOT required.** TrueGaze ships no MCM menu, no ESP/ESL, no Papyrus
    (`.psc`/`.pex`) script, and no translation files. It does not modify any game
    menu. All configuration is the INI at `Data\SKSE\Plugins\TrueGaze.ini`, edited
    through the standalone `TrueGazeConfig.html` page.
- **Host OS:** Windows 10 / Windows 11 (x64)
- **Compiler / Toolchain:** MSVC 19.40+ (Visual Studio 2022 / 2026), C++20 standard, CMake >= 3.23.

---

## 4. Functional Requirements (FR)

### 4.1 Kinematics & Biological Oculomotor Engine

- **[FR-1] Saccade Main Sequence Equation:**
  Peak saccadic velocity and total duration must be computed dynamically as a function of angular amplitude:
  $$V_{\text{peak}}(\theta) = V_{\text{max}} \cdot \left(1 - e^{-\theta / c}\right)$$
  $$D(\theta) = D_0 + d \cdot \theta$$
  Where $V_{\text{max}} = 750^\circ/\text{s}$, $c = 14^\circ$, $D_0 = 22\text{ ms}$, and $d = 2.5\text{ ms}/^\circ$.
  
- **[FR-2] Vestibulo-Ocular Reflex (VOR) & Eye-Head Decoupling:**
  - Eyes must rotate ballistically toward the new target within 20–30ms.
  - Head and neck must follow exponentially with inertial damping ($\alpha = 1 - e^{-k \cdot \Delta t}$, where $k \approx 6.0$).
  - As the head rotates toward the target, the ocular nodes must counter-rotate by an equal and opposite angular velocity relative to the head to maintain continuous foveal lock.
  
- **[FR-3] Micro-Saccadic Brownian Drift (Jitter):**
  When fixating on an object, eyes must execute continuous bounded random-walk drift within a sub-degree cone ($\pm 0.35^\circ$) updating at 2–4 Hz, preventing perceptual freezing.

- **[FR-4] Skeletal Strain Distribution:**
  Total gaze deflection must be anatomically distributed across the NetImmerse bone hierarchy:
  - `NPC Spine2`: 10% total yaw (clamped to $\pm 12^\circ$).
  - `NPC Neck [Neck]`: 25% total yaw/pitch (clamped to $\pm 20^\circ$).
  - `NPC Head [Head]`: 65% total yaw, 75% pitch (clamped to $\pm 45^\circ$).
  - `Eye Nodes`: 100% instant ballistic saccade with VOR compensation (clamped to $\pm 35^\circ$).

### 4.2 HCEP Cognitive & Social Modes

- **[FR-5] Social Triangle Scanning (AFFECT Mode):**
  During conversational dialogue, gaze must cyclically transition between three facial vertices:
  $$\text{Left Eye} \longrightarrow \text{Right Eye} \longrightarrow \text{Mouth} \longrightarrow \text{Left Eye}$$
  Fixation duration at each vertex must be randomized between 250ms and 450ms. Interpupillary and eye-mouth distances must scale inversely with 3D camera distance.

- **[FR-6] Cognitive Gaze Aversion (THINK Mode):**
  When NPCs evaluate complex questions, recite memories, or change topics, they must execute an avert-saccade away from the player's face toward the upper-left or upper-right peripheral quadrant for 1.2–2.5 seconds before returning to foveal contact.

- **[FR-7] Saccadic Blink Coupling (EFM Integration):**
  Large-amplitude saccades ($> 20^\circ$) must automatically trigger a 120ms micro-blink curve applied to Expressive Facegen Morphs or eyelid bone scales, reproducing physiological saccadic suppression.

### 4.3 Connected HCEP Desktop Telemetry Bridge

- **[FR-8] Asynchronous Named Pipe IPC:**
  The plugin must host an asynchronous duplex Windows Named Pipe server at `\\.\pipe\TrueGazeBridge`.
  - Inbound: 64-byte `TrueGazeTelemetryPacket` containing real-world player gaze vector, head pose, blink bitmask, and active HCEP mode (LOGIC, AFFECT, SPIRIT, HEART, THINK).
  - Outbound: 32-byte `SkyrimFeedbackPacket` providing target NPC FormID, relationship rank, combat state, distance, and mutual gaze angle.
  - Performance: Zero game-thread blocking. Non-blocking double-buffered memory exchange taking $< 10\text{ ns}$ on the game thread.

- **[FR-9] Mutual Gaze Detection:**
  When the player's real-world gaze ray intersects the NPC's ocular zone and the NPC's gaze ray intersects the player's camera position (angular divergence $< 4.5^\circ$), a mutual gaze timer increments. Once the timer exceeds `fMutualGazeThreshold` (default 2.0s), mutual gaze reactions trigger.

### 4.4 Engine & Modding Ecosystem Integration

- **[FR-10] Open Animation Replacer (OAR) Custom Conditions:**
  Expose native C++ condition callbacks:
  - `TrueGaze_IsMode(modeId)`
  - `TrueGaze_IsMutualGaze(thresholdSec)`
  - `TrueGaze_GetGazeRegion(regionId)`
  - `TrueGaze_IsSaccadeActive()`

- **[FR-11] Configuration (INI + HTML Editor):**
  All tuning exposed through `Data\SKSE\Plugins\TrueGaze.ini`, edited via the self-contained `TrueGazeConfig.html` page (auto-loads the INI; launch via `Launch-TrueGazeConfig.cmd`). Covers saccade speed, jitter amplitude and interval, skeletal strain shares, gaze aversion, HCEP desktop sync, mutual gaze threshold, LOD distances, and debug gaze rays.

- **[FR-12] Dynamic Level-of-Detail (LOD):**
  - *Tier 1 (< 5m):* Full biological kinematics (eyes, head, neck, micro-jitter, blinks, triangle).
  - *Tier 2 (5m–15m):* Head and neck kinematics; eye drift culled.
  - *Tier 3 (> 15m):* Engine bypassed entirely; 0.000ms overhead.

- **[FR-13] Dynamic 3D Head-Height Elevation Targeting:**
  Target vectors must evaluate dynamic 3D head bone transforms (`NPC Head [Head]`) for both observer and target, correctly calculating vertical pitch deflection $dz = \text{targetHead.z} - \text{observerHead.z}$. Seated, leaning, or crouching actors must naturally align eye-to-eye rather than aiming horizontally straight into chests or down at furniture.

- **[FR-14] 3rd-Person Player Conversational Gaze & Headtracking:**
  In 3rd-person camera perspective, the player character must identify nearby conversational candidates ($\le 4.5\text{m}$) within the forward visual cone and apply biomechanical head and neck tracking toward dialogue partners, while strictly isolating 1st-person camera and torso spine bones.

---

## 5. Non-Functional Requirements (NFR)

- **[NFR-1] Performance Budget:**
  The total processing time per frame across all active scene actors must not exceed **0.15 ms** at 60 FPS (less than 1% of a 16.6ms frame budget).
- **[NFR-2] Thread Safety & Non-Blocking Execution:**
  All IPC I/O must run on dedicated background threads. Under no circumstances may a pending pipe connection or lost client cause a frame stutter or freeze in Skyrim.
- **[NFR-3] Memory Footprint:**
  The plugin's memory overhead must remain strictly under **12 MB** regardless of world cell density.
- **[NFR-4] Crash & Exception Safety:**
  All bone lookups, node transformations, and memory access must be guarded against null references, deleted actors, and cell transitions. No unhandled C++ exceptions may reach Skyrim's main loop.
- **[NFR-5] High-Refresh Rate & Frame Generation Compatibility:**
  Kinematics calculations must integrate delta-time properly and support variable refresh rates (60, 120, 144, 165, 240 Hz) and frame generation technologies (DLSS 3, FSR 3).

---

## 6. Architecture & Data Flow

```
┌────────────────────────────────────────────────────────────────────────┐
│                        HCEP Desktop Perception                         │
│             (Kinect / Webcam ──► PnP Solver ──► 64B Packet)            │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │  \\.\pipe\TrueGazeBridge
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                         TrueGaze.dll (SKSE64)                          │
│                                                                        │
│   ┌────────────────────────┐         ┌─────────────────────────────┐   │
│   │   NamedPipeServer      │ ◄─────► │     Double-Buffer Cache     │   │
│   │  (Background Worker)   │         │ (Lock-Free Read < 10ns)     │   │
│   └────────────────────────┘         └──────────────┬──────────────┘   │
│                                                     │                  │
│   ┌─────────────────────────────────────────────────▼──────────────┐   │
│   │             Kinematics & Biological Math Pipeline              │   │
│   │  - SaccadeGenerator (Main Sequence Bell Curve)                 │   │
│   │  - VorCoordinator (Eye-Lead / Damped Head / Counter-Rotation)  │   │
│   │  - MicroJitter (Brownian Random Walk Fixation Drift)           │   │
│   │  - SocialTriangle (Argyle & Cook Eye-Mouth Cycling)            │   │
│   │  - BoneController (Spine 10% / Neck 25% / Head 65% / Eye 100%)│   │
│   └─────────────────────────────────┬──────────────────────────────┘   │
│                                     │                                  │
│   ┌─────────────────────────────────▼──────────────────────────────┐   │
│   │                      Skyrim Engine Hooks                       │   │
│   │  - Post-Havok Animation Evaluation Hook                        │   │
│   │  - Open Animation Replacer (OAR) Native Condition Dispatches   │   │
│   │  - Expressive Facegen Morphs (EFM) Saccadic Blink Sync         │   │
│   └────────────────────────────────────────────────────────────────┘   │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Acceptance Criteria & Test Matrix

| Test ID | Area | Acceptance Criterion | Status |
| :--- | :--- | :--- | :--- |
| **AC-1** | Saccade Math | Peak velocity reaches asymptotically toward 750 deg/s; duration scales linearly ($D_0 + d \cdot \theta$). | **Passed** (Unit Test) |
| **AC-2** | VOR Decoupling | Eye rotates instantly; head lags by ~150ms; eye local counter-rotation maintains foveal focus. | **Passed** (Unit Test) |
| **AC-3** | Micro-Jitter | Sub-degree Brownian drift stays within $\pm 0.35^\circ$ without frozen eye sockets. | **Passed** (Unit Test) |
| **AC-4** | Bone Hierarchy | Gaze angle distributes 10% Spine2, 25% Neck, 65% Head with physiological angle clamping. | **Passed** (Unit Test) |
| **AC-5** | Wire Protocol | `TrueGazeTelemetryPacket` is exactly 64 bytes; `SkyrimFeedbackPacket` is exactly 32 bytes. CRC32 validated. | **Passed** (Unit Test) |
| **AC-6** | Plugin Build | `TrueGaze.dll` compiles for MSVC x64 with `SKSEPlugin_Query` and `SKSEPlugin_Load` exports. | **Passed** (Binary Built) |
| **AC-7** | Mod Packaging | Mod directory contains valid `SKSE/Plugins/` (DLL + INI) and `OAR` structures. | **Passed** (Verified) |
| **AC-8** | Skyrim SE Load | SKSE64 SE 1.5.97 successfully loads `TrueGaze.dll` from plugins directory without crashing. | **Ready for Local Test** |
