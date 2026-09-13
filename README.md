# TRUE GAZE™ (`TrueGaze`)

## Biological NPC Gaze & Biomechanical Kinematics Engine

### A First-Party Product of the Human Communication Eye Protocol (HCEP) Architecture

[![Status](https://img.shields.io/badge/status-alpha%20%C2%B7%20in%20development-orange)](#-current-project-status)
[![Version](https://img.shields.io/badge/version-1.0.0--rc1-blue)](#)
[![Platform](https://img.shields.io/badge/platform-Skyrim%20SE%20%7C%20AE%20%7C%20VR-4b5563)](#)
[![C++](https://img.shields.io/badge/C%2B%2B-20-00599C)](#)
[![SDK](https://img.shields.io/badge/SDK-CommonLibSSE--NG-8b5cf6)](#)
[![License](https://img.shields.io/badge/license-Proprietary-red)](LICENSE)

**Architect & Product Owner:** Kirk LaSalle  
**Repository:** `https://github.com/kirklasalle/SkyrimTrueGaze`  
**Native Binary:** `TrueGaze.dll` (SKSE64 / CommonLibSSE-NG)  
**Target Platform:** The Elder Scrolls V: Skyrim (SE 1.5.97, AE 1.6.640+, AE 1.6.1170+, Skyrim VR) & Modern Creation Engine

> [!IMPORTANT]
> **Documentation vs. Implementation.** The architecture sections below describe the *designed* TrueGaze system — the target. The current build implements the gaze engine and compiles against the real SDK, but **has not yet been verified inside a running Skyrim instance.** For an honest, verified breakdown of what actually works today, see **[`docs/STATUS.md`](docs/STATUS.md)** and the independent **[`docs/AUDIT_REPORT_2026-09-11.md`](docs/AUDIT_REPORT_2026-09-11.md)**.

---

## 📌 Current Project Status

| | |
| :--- | :--- |
| **Maturity** | 🟡 **~65%** of a shippable 1.0.0 |
| **Builds & links the SDK?** | ✅ Yes — DLL is 637 KB and imports `CommonLibSSE`, `spdlog`, `fmt` |
| **Drives bones?** | ✅ Yes — implemented and compiled |
| **Verified in-game?** | ❌ **Not yet** — nobody has watched an NPC's eyes move |
| **Hard blocker** | None. The former blocker (SDK not vendored) is resolved. |

**Verified working:** SDK linkage · the gaze engine and bone application · per-actor simulation state · configuration reaching the simulation · Main Sequence velocity profile · Ornstein-Uhlenbeck drift · triple-buffered IPC with a user-scoped pipe ACL · 10 registered Papyrus functions with script parity · a public C API that returns live state · 11 passing test suites · a reproducible pinned build.

**Not yet verified:** everything above works in a *running game*. That is the next milestone and the only thing that can promote the headline feature from 🔨 Implemented to ✅ In-engine verified.

**Still open:** OAR condition registration (#6) · the MCM has no backing plugin form (#2) · `.pdb` files are not packaged · the tick is not yet wrapped in `try/catch` (NFR-4) · GitHub Actions does not run on this account for private repositories (#9).

➡️ **Full capability matrix and remediation plan: [`docs/STATUS.md`](docs/STATUS.md)**

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

## 8. Mod Configuration Menu (SkyUI MCM)

Players and modders have full control over the engine:

* **Master Toggles**: Enable/Disable Player Tracking, NPC Gaze, Creature Gaze.
* **Saccade Dynamics**: Adjust saccade velocity, fixation duration, and micro-jitter amplitude.
* **Social Parameters**: Set Social Triangle cycling speed and Cognitive Gaze Aversion frequency.
* **Connected Mode**: Toggle HCEP Desktop sync, Named Pipe status indicator, and mutual gaze sensitivity.
* **Diagnostic Visualizer**: In-game 3D debug rays showing NPC gaze vectors and target focus cones.

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
│   │   ├── EfmBlinkController.cpp    # Expressive Facegen Morphs eyelid sync
│   │   └── PapyrusInterface.cpp      # Script bindings for modders & quests
│   │
│   └── Bridge/                       # HCEP Desktop connectivity
│       ├── NamedPipeServer.cpp       # Asynchronous low-latency IPC listener
│       └── TelemetryPacket.h         # Shared 64-byte POD struct
│
└── skyrim/                           # Game assets & configuration
    └── Interface/
        └── MCM/
            └── Config/
                └── TrueGaze/
                    └── config.json   # SkyUI Mod Configuration Menu definition
```

---

### Summary of the Product Vision

With **`TrueGaze`**, Kirk LaSalle's HCEP moves from an analytical perception platform into an **embodied biological execution engine**. Characters in Skyrim will no longer merely exist as static 3D puppets—they will look, listen, hesitate, scan, and connect with the physiological fidelity of real living beings.

---

## 10. Documentation Index

| Document | Purpose |
| :--- | :--- |
| [`docs/STATUS.md`](docs/STATUS.md) | ⭐ **Start here.** Verified capability matrix — what actually works today |
| [`docs/TEST_SCENARIO.md`](docs/TEST_SCENARIO.md) | Staged in-game test protocol and troubleshooting |
| [`docs/AUDIT_REPORT_2026-09-11.md`](docs/AUDIT_REPORT_2026-09-11.md) | Independent technical audit, build forensics, and market assessment |
| [`PRD.md`](PRD.md) | Product Requirements Document — FR/NFR specification |
| [`ROADMAP.md`](ROADMAP.md) | Development roadmap, verified status, and the remediation plan |
| [`CHANGELOG.md`](CHANGELOG.md) | Version history and defect log |
| [`TRUEGAZE_ARCHITECTURE.md`](TRUEGAZE_ARCHITECTURE.md) | Full architecture reference |
| [`docs/HCEP_BRIDGE_SPEC.md`](docs/HCEP_BRIDGE_SPEC.md) | Named-pipe wire protocol specification |
| [`docs/OAR_INTEGRATION.md`](docs/OAR_INTEGRATION.md) | Open Animation Replacer integration guide |
| [`include/TrueGazeAPI.h`](include/TrueGazeAPI.h) | Public C/C++ modding API |
| [`GOVERNANCE.md`](GOVERNANCE.md) | Charter enforcement map — what is enforced, what is a gap, what is not applicable |
| [`AGENTIC_PRIME_DIRECTIVE.md`](AGENTIC_PRIME_DIRECTIVE.md) | Agentic Prime Directive |
| [`AGENTIC_SACRED_COVENANT.md`](AGENTIC_SACRED_COVENANT.md) | PRISM Sacred Covenant |
| [`Permanent_Active_Directives.txt`](Permanent_Active_Directives.txt) | Canonical charter — the 10 Laws and 4 Core Tenets |

---

## 11. Governance

This project operates under the **Permanent Active Directives** — the 10 Laws and
4 Core Tenets authored by Kirk LaSalle. The charter is pinned by SHA-256 digest and
verified at commit time and in CI.

```bash
# Enable the pre-commit gate (one-time, per clone)
powershell -ExecutionPolicy Bypass -File scripts/install-hooks.ps1

# Verify manually
python scripts/verify_charter.py            # verify
python scripts/verify_charter.py --verbose  # per-Law report
python scripts/verify_charter.py --json     # machine-readable
python scripts/verify_charter.py --strict   # treat recorded divergences as failures
```

> ⚠️ **Enforcement is currently local only.** The pre-commit hook is active and has
> been proven to block a reworded Law. The CI workflow is committed and its YAML
> validates, but GitHub Actions runs in this account currently terminate with
> `startup_failure` before creating any jobs — an account-level availability issue,
> not a workflow defect. Until that is resolved, a fresh clone has no gate until
> `install-hooks.ps1` is run, and `--no-verify` bypasses it.

**Honest enforcement status** — full detail in [`GOVERNANCE.md`](GOVERNANCE.md):

| Control | Status |
| :--- | :--- |
| `LAW10-CHARTER` — Laws pinned by digest, verified locally + CI | ✅ **Implemented** (local hook active; CI committed, not executing) |
| `LAW7-TRUTHFUL-LOG` — no log may report success for work not performed | ✅ **Implemented** (review-enforced) |
| `LAW9-AUDIT` — auditable record of reasoning | 🟡 **Partial** |
| `LAW6-BIOMETRIC` — protection of personal/biometric data | 🔴 **Gap** — pipe has no access control, encryption, or consent capture |
| `LAW10-APPROVAL` — cryptographic approval for directive changes | 🔴 **Gap** — regeneration is a plain file write |
| `LAW1–5, 8` | ⛔ **No runtime control** — governing principles for human conduct, not plugin constraints |

Two of the four Core Tenets in the markdown charter documents are **paraphrases**
of the canonical text rather than reproductions. These are recorded in the manifest
as disclosed divergences pending Governance Council review. The 10 Laws themselves
are byte-identical across all three charter documents.

> The biometric gap (Law 6) is the most significant governance item in this
> repository. The HCEP bridge transmits gaze vectors, head pose, blink state, and
> cognitive classification over a named pipe created with **no security descriptor**,
> while `LICENSE` asserts GDPR/CCPA/BIPA compliance. See [`GOVERNANCE.md`](GOVERNANCE.md#law-6--biometric-data-protection-) for the gap analysis and remediation plan.

---

## 12. Building, Deploying and Testing

> ✅ **The build now produces a real plugin.** CommonLibSSE-NG v7.5.4 is vendored as a submodule, the CMake build **fails hard** if it is absent, and the resulting DLL links the SDK. See [`docs/STATUS.md`](docs/STATUS.md) for exactly what is and is not verified.

**Prerequisites:** Visual Studio 2022 (MSVC 19.44+), CMake ≥ 3.23, vcpkg at `D:\vcpkg`, and the SDK submodule initialized:

```powershell
git submodule update --init --recursive
```

### One-click workflow

**`TrueGaze.cmd`** is a single self-contained batch file. It needs nothing beyond what the build already needs (CMake, MSVC, vcpkg). It finds the game, reads its version, derives the exact Address Library filename that version requires, builds, deploys, verifies the deployed binary, and only then launches.

```powershell
.\TrueGaze.cmd              # interactive menu
```

Or drive it directly:

| Command | Does |
| :--- | :--- |
| `TrueGaze.cmd all` | build → deploy → verify → launch |
| `TrueGaze.cmd loadonly` | deploy with the engine **off**, then launch — the safe first run |
| `TrueGaze.cmd build` | build and verify, no launch |
| `TrueGaze.cmd deploy` | deploy an existing build, then verify |
| `TrueGaze.cmd verify` | check everything, no build, no launch |
| `TrueGaze.cmd postrun` | report what happened on the last run |
| `TrueGaze.cmd status` | show the detected configuration |

Options: `/game "path"` to override detection, `/force` to launch despite failures, `/nopause` for automation.

Exit code is `0` only when nothing failed, so it composes in scripts.

> **It refuses to launch when verification fails.** The check is cheap; a two-minute game launch that crashes on load is not.

To additionally get clickable shortcuts in the project root and on the Desktop:

```powershell
.\scripts\Install-OneClick.ps1
```

### The automated health check

The same verification is available on its own:

```powershell
.\scripts\Test-TrueGazeHealth.ps1           # pre-flight
.\scripts\Test-TrueGazeHealth.ps1 -PostRun  # what happened on the last run
```

Pre-flight verifies, in about a second, everything that is expensive to discover later:

* the game version found, and the exact Address Library filename it implies
* that SKSE is installed **and that its build matches this game version**
* that the Address Library is present **and version-matched** — a library for the *wrong* version is worse than none
* the VC++ runtime dependencies
* that the DLL exports the full SKSE loader contract (`SKSEPlugin_Load`, `SKSEPlugin_Query`, `SKSEPlugin_Version`)
* that the compiled binary targets `Actor::Update` slot `0xAD` and carries the tick exception guard
* that the deployed copy **hashes identically** to the build — a stale deployed binary has shipped twice

### Manual build

```powershell
$env:VCPKG_ROOT = "D:\vcpkg"
cmake --preset windows-release
cmake --build --preset release          # -> build/windows-release/Release/TrueGaze.dll

cmake --preset standalone
cmake --build --preset standalone       # -> bin/Release/KinematicsTests.exe (+ bridge mock)
```

A successful build automatically refreshes the packaged copy at `skyrim/SKSE/Plugins/TrueGaze.dll` via a CMake post-build step, so the distributed artifact can no longer drift from the build.

### Testing in game

The full protocol is in **[`docs/TEST_SCENARIO.md`](docs/TEST_SCENARIO.md)**. It is staged deliberately — each stage isolates one failure mode, and a later stage cannot be interpreted if an earlier one is broken:

| Stage | Proves |
| :--- | :--- |
| 0 — Load-only | The plugin loads and installs its hook, with simulation disabled |
| 1 — Skeleton probe | The engine finds the bone nodes it needs *(the critical unknown)* |
| 2 — Visible gaze | An NPC actually looks at you |
| 3 — Social triangle | Eye-scan cycling during dialogue |
| 4 — Stability | No crashes or frame-budget overruns in a crowded scene |

> ⚠️ **Nothing has been verified in a running game yet.** Every claim in `docs/STATUS.md` is currently build-time, unit-test or static-analysis evidence. No row is marked **✅ In-engine verified**. The protocol above is what changes that.

### Required game-side dependencies

| Requirement | Why |
| :--- | :--- |
| **SKSE64**, AE build matching your `SkyrimSE.exe` | Nothing loads without it. Launch via `skse64_loader.exe`, never `SkyrimSE.exe`. |
| **Address Library for SKSE Plugins** | TrueGaze resolves game offsets through `Data/SKSE/Plugins/versionlib-<version>.bin`. |
| Visual C++ 2015–2022 x64 Redistributable | `MSVCP140` / `VCRUNTIME140` runtime dependencies. |

The health check verifies all three and names the exact filename each one needs.

---

## 13. Contributing

Contributions are welcome, particularly in the areas the audit identified as blocking. The highest-value contributions right now are:

1. **Vendoring CommonLibSSE-NG** and making the CMake guard a hard failure (Phase R1)
2. **Implementing the bone-application tick** (Phase R2) — see `src/Engine/AnimationHook.cpp`
3. **Fixing the `BoneController` eye-residual allocation bug** — see the note in [`docs/STATUS.md`](docs/STATUS.md)
4. **Fixing the named-pipe data race** — triple-buffer or seqlock
5. **Animation content** for the OAR condition set (for animators)

**Before contributing, please read [`docs/STATUS.md`](docs/STATUS.md).** All documentation and code comments in this project are expected to use the four-state vocabulary (📐 Designed / 🔨 Implemented / 🧪 Unit-verified / ✅ In-engine verified), and no log message may report success for an operation that was not performed.

---

## 14. License & Attribution

**Copyright © 2026 Kirk LaSalle. All rights reserved.** See [`LICENSE`](LICENSE).

**Proprietary:** the HCEP (Human Communication Eye Protocol) theory, the 5-mode cognitive-emotional classification system, the HCEP engineering implementation, the Body Language Protocols, and the Permanent Active Directives.

**Public science cited by this project:** the Main Sequence saccade equation (Bahill, Clark & Stark, 1975; Baloh et al., 1975), gaze aversion under cognitive load (Glenberg et al., 1998), and Social Triangle scanpaths (Argyle & Cook, 1976; Ingham et al., 1973). These are published academic findings and are not claimed as trade secrets.

> ⚠️ **Licensing conflict to resolve.** `LICENSE` states "No license is granted... copying, distribution... prohibited", which is irreconcilable with publishing a public modding SDK or distributing this mod. This must be resolved before any public release. See `ROADMAP.md` Phase R0.

---

<p align="center">
  <em>TrueGaze™ — An HCEP Product by Kirk LaSalle</em><br/>
  <sub>"When you look in their eyes, they know."</sub>
</p>
