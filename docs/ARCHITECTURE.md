# TRUE GAZE™ (`TrueGaze`)

## Architecture Reference

> [!NOTE]
> **This document duplicates the root [`README.md`](../README.md) and [`TRUEGAZE_ARCHITECTURE.md`](../TRUEGAZE_ARCHITECTURE.md).**
>
> The independent audit of September 11, 2026 identified this triplication as a documentation-drift risk: three copies of the same authoritative content will inevitably diverge. **`README.md` is the canonical source for the product overview.** This file is retained for historical reference; prefer the root documents.
>
> For verified implementation status, see [`STATUS.md`](STATUS.md).
> For the independent audit, see [`AUDIT_REPORT_2026-09-11.md`](AUDIT_REPORT_2026-09-11.md).
> **Current runtime note:** Skyrim AE execution is now verified for plugin load, actor updates, target resolution, skeleton probing, HCEP state consumption, and diagnostic light attachment. Visible geometry, broad rig coverage, OAR registration, and Skyrim VR remain open work.

---

## 1. Executive Summary & Brand Positioning

**True Gaze™** is an advanced biomechanical perception and kinematics engine designed to eradicate the "dead-eye zombie syndrome" pervasive in game engines. Developed as a specialized gaming product of Kirk LaSalle's **Human Communication Eye Protocol (HCEP)**, `TrueGaze` bridges 50 years of psycholinguistic and neuroscience research (Argyle & Cook, Kendon, Glenberg) into real-time 3D game characters.

Unlike traditional head-tracking mods that apply rigid spherical interpolation directly to the neck bone, `TrueGaze` models the **human oculomotor system**:

* **Eyes lead, head follows** via the Vestibulo-Ocular Reflex (VOR).
* **Ballistic Saccades** calculated via the empirical Main Sequence equation.
* **Micro-saccadic Brownian drift** preventing visual freezing.
* **Cognitive Gaze Aversion (THINK Mode)** and **Social Triangle Cycling (AFFECT Mode)**.
* **Deep Open Animation Replacer (OAR) integration**, allowing modders to trigger bespoke body gestures based on real-time gaze and cognitive states.

---

## 2. Biomechanical Gaze Architecture

```
                    ┌──────────────────────────────────────────┐
                    │      Environmental & Actor Salience      │
                    │   (Dialogue Target, Combat, Proximity)   │
                    └────────────────────┬─────────────────────┘
                                         │ Target Vector
                                         ▼
┌───────────────────────────────────────────────────────────────────────────────┐
│                           TRUE GAZE RUNTIME ENGINE                            │
│                                                                               │
│  ┌─────────────────────────┐              ┌────────────────────────────────┐  │
│  │    HCEP 5-Mode State    │              │       Saccade Generator        │  │
│  │ (LOGIC, AFFECT, SPIRIT, ├─────────────►│    (Main Sequence Dynamics:    │  │
│  │      HEART, THINK)      │              │     Vpeak = Vmax*(1-e^(-A/C))  │  │
│  └───────────┬─────────────┘              └───────────────┬────────────────┘  │
│              │                                            │                   │
│              ▼                                            ▼                   │
│  ┌─────────────────────────┐              ┌────────────────────────────────┐  │
│  │   Cognitive Aversion    │              │   VOR & Kinetic Coordinator    │  │
│  │  (Glenberg Off-Target)  │              │ (Eyes snap 30ms, Head dampens) │  │
│  └───────────┬─────────────┘              └───────────────┬────────────────┘  │
│              │                                            │                   │
│              ▼                                            ▼                   │
│  ┌─────────────────────────┐              ┌────────────────────────────────┐  │
│  │  Micro-Saccadic Jitter  │              │ Saccadic Eyelid Suppression    │  │
│  │ (1-3Hz Brownian Drift)  │              │   (Blink coupling with EFM)    │  │
│  └─────────────────────────┘              └────────────────────────────────┘  │
└───────────────────────────────────────┬───────────────────────────────────────┘
                                        │
             ┌──────────────────────────┴──────────────────────────┐
             ▼                                                     ▼
┌───────────────────────────────┐               ┌────────────────────────────────┐
│   Additive Bone Overrides     │               │   OAR Custom Condition Bus     │
│ (Spine2 10%, Neck 25%, Head   │               │ (Triggers body animations when │
│  65%, Eye Nodes 100% Saccade) │               │  NPC thinks, resonates, stares)│
└───────────────────────────────┘               └────────────────────────────────┘
```

### 2.1. The Main Sequence Saccade Dynamic

Living eyes do not rotate with smooth linear damping. They jump ballistically:

* **Duration ($\mathbf{D}$)**: $D = D_0 + d \cdot \theta$ (typically 20ms to 50ms depending on angular amplitude $\theta$).
* **Peak Angular Velocity ($\mathbf{V_{peak}}$)**:
  $$V_{peak} = V_{max} \cdot \left(1 - e^{-\frac{\theta}{C}}\right)$$
  *(where $V_{max} \approx 700^\circ/\text{sec}$ to $900^\circ/\text{sec}$, and $C \approx 14^\circ$).*
* The eye holds fixation on an interest point for **200ms to 600ms**, then jumps ballistically in under 40ms to the next salient landmark.

### 2.2. The Vestibulo-Ocular Reflex (VOR) & Head-Eye Decoupling

* **Latency Gap**: The eye begins moving within **20ms–30ms** of a new target being selected. The heavy head and neck bones do not begin significant inertial movement until **120ms–180ms**.
* **Counter-Rotation**: Once the eyes have snapped to the target, the head begins swinging toward the target. During this head rotation, the eyes must counter-rotate backwards in the head frame at the exact opposite angular velocity:
  $$\vec{\omega}_{\text{eye}} = -\vec{\omega}_{\text{head}}$$
  This keeps the image locked onto the fovea without visual slipping.

### 2.3. Micro-Saccadic Brownian Drift (Fixation Jitter)

If a 3D model looks at an object with zero movement, the viewer's brain recognizes it as synthetic or dead. `TrueGaze` injects an organic, sub-conscious physiological drift:

* **Frequency**: 1.5 Hz to 3.0 Hz.
* **Amplitude**: $0.15^\circ$ to $0.45^\circ$.
* Implemented as damped 2D Brownian motion across the ocular yaw/pitch plane.

### 2.4. Saccadic Suppression & Eyelid Blink Coupling

Human eyes suppress visual perception during large saccades, and **large saccades (>20° amplitude) trigger synchronous micro-blinks**.

* When `TrueGaze` detects a major gaze transition, it signals the character's eyelid morphs (`EyelidUpper_Down`, `EyelidLower_Up`) to execute a subtle 120ms dip-and-open, eliminating static, staring eyes.

---

## 3. Deep Integration with Open Animation Replacer (OAR)

One of the most powerful architectural enhancements is making `TrueGaze` a first-class citizen within the **Open Animation Replacer (OAR)** ecosystem.

```
┌────────────────────────────────────────────────────────────────────────┐
│                        OAR Animation Pipeline                          │
├────────────────────────────────────────────────────────────────────────┤
│ 1. OAR evaluates conditions (Weather, Armor, Combat, Location)         │
│ 2. *NEW*: TrueGaze exposes custom native C++ OAR conditions            │
│ 3. If condition passes: Actor triggers custom body/gesture animation   │
└────────────────────────────────────────────────────────────────────────┘
```

### Custom OAR Conditions Exposed by TrueGaze

`TrueGaze` registers custom condition functions directly with OAR's native API:

1. `TrueGaze_IsMode(mode_id)`:
   * **`THINK` (Mode 4)**: Modders can configure NPCs to touch their chin, look upward, or shift weight when deep in thought.
   * **`AFFECT` (Mode 1)**: NPCs play subtle nodding, smiling, or expressive conversational idles during Social Triangle scanning.
   * **`SPIRIT` (Mode 2)**: Triggers intense intimacy or mutual locked-gaze stances (perfect for companion mods).
2. `TrueGaze_IsMutualGaze(duration_threshold)`:
   * Returns true if the player and the NPC have maintained direct mutual eye contact for more than $X$ seconds (e.g., triggering a blush, a smile, or an intimidated combat posture).
3. `TrueGaze_GetGazeRegion()`:
   * Returns the classified 13-region gaze target (e.g., looking at player's eyes, looking at player's drawn weapon, looking at the ground in shame).

---

## 4. Skeletal & Node Kinematics Distribution

`TrueGaze` applies biomechanically sound hierarchical strain distribution to prevent distorted necks:

| Bone Node | Strain % | Maximum Angle Constraint | Purpose |
| --- | --- | --- | --- |
| **`NPC Spine2`** | 10% | $\pm 15^\circ$ Yaw | Base upper-torso lead |
| **`NPC Neck [Neck]`** | 25% | $\pm 25^\circ$ Yaw, $\pm 20^\circ$ Pitch | Cervical curvature distribution |
| **`NPC Head [Head]`** | 65% | $\pm 55^\circ$ Yaw, $\pm 45^\circ$ Pitch | Primary head orientation |
| **`NPC L/R Eye [Eye]`** | Instant | $\pm 35^\circ$ Yaw, $\pm 25^\circ$ Pitch | Ballistic Saccade & VOR target lock |

* **Exceeding Comfort Thresholds ($> 70^\circ$)**: If the target moves past the head's physiological limit, `TrueGaze` commands the actor's root navigation to step and rotate the character's feet, rather than twisting the neck past anatomical limits.

---

## 5. Non-Humanoid & Creature Behavioral Profiles

`TrueGaze` applies customized cognitive profiles to Skyrim's diverse bestiary:

```
┌─────────────────┬───────────────────┬──────────────────┬───────────────────────────────┐
│ Species / Rig   │ Saccade Frequency │ Aversion Rate    │ Unique Behavioral Trait       │
├─────────────────┼───────────────────┼──────────────────┼───────────────────────────────┤
│ **Humanoid**    │ 2.0 – 3.5 Hz      │ 30% – 45%        │ Social Triangle & Micro-drift │
│ **Predator**    │ 0.5 – 1.0 Hz      │ 0% (Relentless)  │ Threat-locked binocular stare │
│ (Wolf, Sabre)   │                   │                  │                               │
│ **Prey (Deer)** │ 4.0 – 6.0 Hz      │ 85%              │ Panoramic horizon scanning    │
│ **Dragon**      │ 0.8 – 1.5 Hz      │ 5%               │ Spline wave neck distribution │
│ **Automaton**   │ Fixed tick (10Hz) │ 0%               │ Pure linear stepper panning   │
│ **Undead**      │ Irregular / Slow  │ 10%              │ Asymmetrical ocular drift     │
└─────────────────┴───────────────────┴──────────────────┴───────────────────────────────┘
```

---

## 6. Dual-Mode Operational Architecture

`TrueGaze` functions in two flexible configurations:

```
┌────────────────────────────────────────────────────────────────────────┐
│                        MODE 1: AUTONOMOUS EDGE                         │
│  • 100% self-contained inside Skyrim SE/AE (TrueGaze.dll).             │
│  • High-performance C++20 running at 60–144+ FPS.                      │
│  • Controls all world NPCs, companions, and creatures.                │
│  • Zero external apps or hardware required.                            │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │
           (Optional Windows Named Pipe: \\.\pipe\TrueGazeBridge)
                                    │
┌───────────────────────────────────▼────────────────────────────────────┐
│                        MODE 2: CONNECTED HCEP                          │
│  • Interlinks with the HCEP Desktop Suite (D:\Projects\HCEP).          │
│  • Real-world webcam/Kinect streams the player's true gaze vector,     │
│    blink state, and HCEP mode into Skyrim.                             │
│  • Enables true bi-directional mutual eye contact:                     │
│    NPCs react instantly when YOU look directly into their eyes.        │
└────────────────────────────────────────────────────────────────────────┘
```

### IPC Telemetry Specification (`HcepGazeTelemetryPacket`)

* **Transport**: Windows Asynchronous Named Pipe (`\\.\pipe\TrueGazeBridge`).
* **Format**: 64-byte aligned binary POD struct (zero JSON overhead).
* **Payload**: Microsecond timestamp, Player Gaze Pitch/Yaw, HCEP Mode (0–4), Cognitive State, Blink Bitmask, and Active Target FormID.
* **Latency**: $< 0.4 \text{ ms}$ roundtrip.

```cpp
// 64-byte aligned binary packet sent from HCEP Core -> Skyrim Plugin
#pragma pack(push, 1)
struct HcepGazeTelemetryPacket
{
    // Header (8 bytes)
    uint32_t magic;          // 0x48434550 ("HCEP" ASCII)
    uint16_t version;        // Protocol version (e.g., 0x0100 -> v1.0)
    uint16_t packetSequence; // Monotonically increasing frame counter

    // Timestamp (8 bytes)
    uint64_t timestampUs;    // Microseconds since session epoch

    // Player Real-World Gaze Vector (16 bytes)
    float gazePitch;         // Look angle up/down in radians (-pi/2 to +pi/2)
    float gazeYaw;           // Look angle left/right in radians (-pi to +pi)
    float gazeConvergence;   // Estimated focus distance in meters
    float gazeConfidence;    // 0.0f (lost) to 1.0f (high confidence tracking)

    // Player Cognitive & HCEP Mode State (8 bytes)
    uint8_t hcepMode;        // 0=LOGIC, 1=AFFECT, 2=SPIRIT, 3=HEART, 4=THINK
    uint8_t cognitiveState;  // 12 cognitive classifications (e.g., Confused, Engaged)
    int8_t  emotionalValence;// -100 (hostile/sad) to +100 (friendly/happy)
    uint8_t blinkState;      // Bitmask: Bit 0 = Left Eye Blink, Bit 1 = Right Eye Blink
    uint8_t socialTriangle;  // 0=None, 1=Left Eye, 2=Right Eye, 3=Mouth Vertex
    uint8_t reserved[3];     // Padding

    // Player Head Pose (12 bytes)
    float headPitch;         // Head tilt forward/backward
    float headYaw;           // Head turn left/right
    float headRoll;          // Head tilt shoulder-to-shoulder

    // Target Synchronization Feedback (12 bytes)
    uint32_t activeTargetFormId; // FormID of Skyrim NPC the player is currently viewing (0 if none)
    float mutualGazeDuration;    // Continuous seconds of sustained mutual eye contact
    uint32_t checksum;           // CRC32 verification
};
#pragma pack(pop)
```

---

## 7. Performance & Spatial LOD (Zero Frame Drops)

To guarantee flawless performance even in heavy combat or crowded cities (Whiterun, Solitude):

1. **Distance LOD Tiering**:
   * **Tier 1 ($< 5\text{m}$ / Dialogue Range)**: Full simulation (Eyes, Saccades, VOR, Social Triangle, Micro-drift, Eyelid Blinks).
   * **Tier 2 ($5\text{m} - 15\text{m}$ / Proximity Range)**: Head & Neck kinematics active; Eye nodes use simplified tracking; Micro-drift disabled.
   * **Tier 3 ($> 15\text{m}$)**: Standard game engine LOD; processing completely bypassed.
2. **Multi-Threaded Evaluation**:
   * Mathematical calculations (quaternions, Main Sequence equations, spline calculations) execute in parallel worker tasks using SSE/AVX vectorization before being applied to the bone hierarchy during the game's animation tick.

---

## 8. Configuration (INI + HTML Editor)

Players and modders have full control over the engine through a single INI file:

* **File**: `Data\SKSE\Plugins\TrueGaze.ini` — the sole configuration surface.
* **Editor**: `TrueGazeConfig.html` at the repository root — a self-contained page that
  auto-loads the INI (launch via `Launch-TrueGazeConfig.cmd` for direct file access),
  renders every key with its physiological range, and writes the INI back.
* **Master Toggles**: Enable/Disable the engine, creature kinematics.
* **Saccade Dynamics**: Saccade velocity multiplier, saturation constant, micro-jitter
  amplitude and correction interval, VOR head damping, ocular comfort angle.
* **Skeletal Hierarchy**: Per-joint strain shares (spine/neck/head yaw and pitch).
* **Social Parameters**: Social Triangle cycling, gaze aversion, mutual-gaze threshold.
* **Connected Mode**: HCEP Desktop sync toggle, pipe name, reconnect interval.
* **LOD**: Tier 1 / Tier 2 distance thresholds.
* **Diagnostics**: Debug gaze rays, log level.

---

## 9. Scaffolding Blueprint for `D:\Projects\SkyrimTrueGaze`

Here is the clean, modular directory structure ready to be initialized for the `SkyrimTrueGaze` repository:

```
D:\Projects\SkyrimTrueGaze/
├── CMakeLists.txt                    # Modern CMake build script
├── CMakePresets.json                 # MSVC C++20 build presets (SE, AE, VR)
├── vcpkg.json                        # Dependency management
├── README.md                         # Product overview & installation guide
├── LICENSE                           # Dual-license / MIT integration
│
├── docs/
│   ├── ARCHITECTURE.md               # In-depth kinematics & Havok hook specs
│   ├── OAR_INTEGRATION.md            # Guide for animation modders using OAR
│   └── HCEP_BRIDGE_SPEC.md           # Named Pipe binary protocol documentation
│
├── extern/
│   ├── CommonLibSSE-NG/              # Multi-target Skyrim SE/AE/VR SDK
│   └── SKSE64/                       # SKSE core headers
│
├── src/
│   ├── Main.cpp                      # SKSE entry point (SKSEPlugin_Load)
│   ├── PCH.h                         # Precompiled header
│   │
│   ├── Kinematics/                   # Platform-agnostic biological math
│   │   ├── SaccadeGenerator.hpp      # Main Sequence velocity & duration equations
│   │   ├── VorCoordinator.hpp        # Eye-Head decoupling & counter-rotation
│   │   ├── MicroJitter.hpp           # Brownian drift fixation generator
│   │   └── SocialTriangle.hpp        # AFFECT mode eye-mouth-eye cycler
│   │
│   ├── Engine/                       # Creation Engine hooks & memory manipulation
│   │   ├── BoneController.cpp        # NetImmerse NiNode transform manipulation
│   │   ├── AnimationHook.cpp         # ModifyAnimationUpdateData post-Havok hook
│   │   ├── TargetSelector.cpp        # 3D spatial salience & raycasting
│   │   └── LodManager.cpp            # Distance & visibility performance culling
│   │
│   ├── Integrations/                 # Community ecosystem connectors
│   │   ├── OarConditions.cpp         # Custom OAR condition registry
│   │   └── EfmBlinkController.cpp    # Expressive Facegen Morphs eyelid sync
│   │
│   ├── Visuals/                      # In-game 3D representation of the solved gaze
│   │   ├── VisualTuning.hpp          # Immutable per-frame visual config snapshot
│   │   └── VisualEffectsManager.cpp  # Pupil/terminus emitters, actor-agnostic
│   │
│   └── Bridge/                       # HCEP Desktop connectivity
│       ├── NamedPipeServer.cpp       # Asynchronous low-latency IPC listener
│       └── TelemetryPacket.h         # Shared 64-byte POD struct
│
└── skyrim/                           # Game assets & configuration
    └── SKSE/
        └── Plugins/
            ├── TrueGaze.dll          # The engine
            └── TrueGaze.ini          # Sole configuration surface
```

---

### Summary of the Product Vision

With **`TrueGaze`**, Kirk LaSalle's HCEP moves from an analytical perception platform into an **embodied biological execution engine**. Characters in Skyrim will no longer merely exist as static 3D puppets—they will look, listen, hesitate, scan, and connect with the physiological fidelity of real living beings.
