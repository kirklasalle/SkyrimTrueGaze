# Implementation Plan: SkyrimTrueGaze Project Refactoring, Missing Modules, PRD, Roadmap & Changelog

> [!NOTE]
> **HISTORICAL PLAN — PARTIALLY SUPERSEDED.** This plan was written before the
> engine was implemented. Its **SkyUI / MCM / Papyrus** components were **removed by
> design decision on 2026-09-14**: TrueGaze is a **vanilla-UI** mod with no ESP, no
> `.psc`/`.pex`, no MCM menu, and no SkyUI dependency. Configuration is the INI at
> `Data\SKSE\Plugins\TrueGaze.ini`, edited through the standalone
> **`TrueGazeConfig.html`** page. References below to `TrueGaze_ENGLISH.txt`,
> "MCM localization", and "Phase 5: In-Game MCM Configuration" are therefore
> obsolete. See [`STATUS.md`](STATUS.md) for current status.

This plan details the steps to transition the project from its prior directory designation (`TrueGaze` / `Trugaze`) to `SkyrimTrueGaze`, ensure all required code files and mod artifacts are implemented and present, establish a comprehensive Product Requirements Document (PRD), provide a detailed Roadmap, and generate an updated Changelog.

## User Review Required

> [!IMPORTANT]
> The directory has been renamed from `TrueGaze` to `SkyrimTrueGaze` at `D:\Projects\SkyrimTrueGaze`. All source comments, documentation files, IPC specifications, and build references will be updated to reflect this new directory path while keeping the official product and plugin binary name as **TrueGaze** (`TrueGaze.dll`) to preserve compatibility with Skyrim modding standards and SKSE. *(The MCM reference originally in this sentence is obsolete — the MCM layer was removed 2026-09-14; TrueGaze is vanilla-UI.)*
>
> `Permanent_Active_Directives.txt` contains an explicit `[DO NOT CHANGE / DELETE / REMOVE / ADD TO THIS FILE]` directive and does not contain project paths; it will remain completely untouched.

---

## Proposed Changes

### Component 1: Directory Path Refactoring (`D:\Projects\TrueGaze` $\longrightarrow$ `D:\Projects\SkyrimTrueGaze`)

Update all path references in documentation, headers, and metadata to `D:\Projects\SkyrimTrueGaze`.

#### [MODIFY] [README.md](file:///d:/Projects/SkyrimTrueGaze/README.md)

- Update workspace path references from `D:\Projects\TrueGaze` to `D:\Projects\SkyrimTrueGaze`.
- Update architectural diagrams and scaffolding tree to reflect the new repository root.

#### [MODIFY] [TRUEGAZE_ARCHITECTURE.md](file:///d:/Projects/SkyrimTrueGaze/TRUEGAZE_ARCHITECTURE.md)

- Update repository workspace path and scaffolding blueprint to `D:\Projects\SkyrimTrueGaze`.

#### [MODIFY] [ARCHITECTURE.md](file:///d:/Projects/SkyrimTrueGaze/docs/ARCHITECTURE.md)

- Mirror architecture updates to the `docs/` reference copy.

#### [MODIFY] [HCEP_BRIDGE_SPEC.md](file:///d:/Projects/SkyrimTrueGaze/docs/HCEP_BRIDGE_SPEC.md)

- Update plugin path reference to `D:\Projects\SkyrimTrueGaze`.

#### [MODIFY] [.nexus](file:///d:/Projects/SkyrimTrueGaze/.nexus)

- Update `Project Root` and documentation links to `D:\Projects\SkyrimTrueGaze`.

#### [MODIFY] [TelemetryPacket.h](file:///d:/Projects/SkyrimTrueGaze/src/Bridge/TelemetryPacket.h)

- Update docstring references from `TrueGaze` to `SkyrimTrueGaze`.

#### [MODIFY] [NamedPipeServer.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Bridge/NamedPipeServer.hpp)

- Update docstring references from `TrueGaze` to `SkyrimTrueGaze`.

#### [MODIFY] [vcpkg.json](file:///d:/Projects/SkyrimTrueGaze/vcpkg.json)

- Update repository/homepage link to reflect `SkyrimTrueGaze`.

---

### Component 2: Missing C++ Module Implementations

Currently, several header declarations are not backed by implementation files, which causes linker errors when building `TrueGaze.dll`.

#### [NEW] [NamedPipeServer.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Bridge/NamedPipeServer.cpp)

- Implement `Start()`: Spawns high-priority background worker thread (`std::jthread` or `std::thread`).
- Implement `Stop()`: Gracefully signals shutdown and closes pipe handles.
- Implement Windows Named Pipe duplex listener loop on `\\.\pipe\TrueGazeBridge`.
- Implement lock-free double-buffered telemetry updates for 64-byte `TrueGazeTelemetryPacket`.
- Implement `TryGetLatestTelemetry(TrueGazeTelemetryPacket& outPacket)`: Non-blocking atomic read (< 10 ns).
- Implement `SendFeedback(const SkyrimFeedbackPacket& feedback)`: Non-blocking transmission of in-game state back to HCEP Desktop.
- Implement CRC32 calculation and verification.

#### [NEW] [AnimationHook.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/AnimationHook.cpp)

- Implement `Install()`: Installs post-Havok animation evaluation hook into actor update pipeline.
- Implement `IsActorEligibleForGaze(uint32_t actorFormId)`: Validates actor liveness, consciousness, and absence of ragdoll/paralysis states.
- Implement `GetActorGazeWeight(uint32_t actorFormId)`: Computes context-dependent blend weight (dialogue, combat, idle).

#### [NEW] [TargetSelector.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TargetSelector.cpp)

- Implement `ResolveTarget(uint32_t observerFormId)`:
  - Dialogue partner prioritization (FormID match).
  - Combat adversary acquisition within view frustum.
  - Nearby friendly actor detection.
  - Ambient environmental points of interest (salience fallback).

#### [NEW] [OarConditions.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Integrations/OarConditions.cpp)

- Implement `EvaluateIsMode()`: Matches actor HCEP cognitive mode (LOGIC, AFFECT, SPIRIT, HEART, THINK).
- Implement `EvaluateIsMutualGaze()`: Tests mutual gaze duration against specified threshold.
- Implement `EvaluateGazeRegion()`: Tests current gaze target region ID (0-12).
- Implement `RegisterWithOar()`: Registers custom C++ condition callbacks with Open Animation Replacer API via SKSE messaging interface.

---

### Component 3: Mod Package Assets & Configurations

Ensure all expected mod runtime assets and configuration files are present for deployment into Skyrim mod managers (MO2, Vortex).

#### [NEW] [TrueGaze.ini](file:///d:/Projects/SkyrimTrueGaze/skyrim/SKSE/Plugins/TrueGaze.ini)

- Default configuration file for SKSE plugin:
  - `[General]` (EnableTrueGaze, EnableCreatures)
  - `[Kinematics]` (SaccadeSpeedMult, MicroJitterAmp, HeadTrackingSpeed, MaxEyeAngle)
  - `[Social]` (EnableGazeAversion, EnableSocialTriangle, MutualGazeThreshold)
  - `[Bridge]` (ConnectHcepBridge, PipeName, AutoReconnectIntervalSec)
  - `[LOD]` (Tier1DistanceMeters, Tier2DistanceMeters)
  - `[Debug]` (DebugGazeRays, LogLevel)

#### [REMOVED 2026-09-14] `skyrim/Interface/Translations/TrueGaze_ENGLISH.txt`

- ~~SkyUI / MCM localization file providing English translation strings for MCM options.~~
- **Obsolete.** The MCM layer was removed by design decision; TrueGaze is vanilla-UI.
  No translation files are shipped. Configuration is the INI edited through
  `TrueGazeConfig.html`.

#### [NEW] [oar_config.json](file:///d:/Projects/SkyrimTrueGaze/skyrim/meshes/actors/character/animations/OpenAnimationReplacer/TrueGaze/config.json)

- Sample OAR condition rules demonstrating `TrueGaze_IsMode`, `TrueGaze_IsMutualGaze`, and `TrueGaze_GetGazeRegion` for mod authors.

---

### Component 4: Standalone Mathematical Test Harness

#### [NEW] [KinematicsTests.cpp](file:///d:/Projects/SkyrimTrueGaze/tests/KinematicsTests.cpp)

- Pure C++20 verification suite to validate all mathematical models without requiring the Skyrim engine runtime:
  - Test 1: Saccade Main Sequence velocity and duration scaling.
  - Test 2: VOR eye-head decoupling and counter-rotation compensation.
  - Test 3: Micro-jitter Brownian walk bounding and interval distribution.
  - Test 4: Social Triangle vertex switching and angular offsets.
  - Test 5: BoneController hierarchy strain distribution (Spine2 10%, Neck 25%, Head 65%).
  - Test 6: POD Telemetry packet memory layout (64 bytes & 32 bytes) and CRC32 integrity.

---

### Component 5: Governance, Planning & Product Documentation

#### [NEW] [PRD.md](file:///d:/Projects/SkyrimTrueGaze/PRD.md)

- Complete, authoritative Product Requirements Document covering:
  - Executive summary and problem statement (eliminating dead-eye syndrome).
  - Target user personas & hardware/software support matrix.
  - Functional Requirements (FR-1 through FR-12).
  - Non-Functional Requirements (NFR-1 through NFR-8).
  - System architecture, pipe protocol contract, and skeletal bone hierarchy.
  - Acceptance criteria and verification methodology.

#### [NEW] [ROADMAP.md](file:///d:/Projects/SkyrimTrueGaze/ROADMAP.md)

- Detailed release and development roadmap:
  - Phase 1: Core Mathematical Kinematics & Biology (Complete).
  - Phase 2: Engine Integration, Memory Hooking & CommonLibSSE-NG (In Progress).
  - Phase 3: HCEP Desktop Telemetry Pipe Bridge (In Progress).
  - Phase 4: Animation Integration (OAR Native Conditions & EFM Blinking).
  - ~~Phase 5: In-Game MCM Configuration & Player Customization.~~ **Superseded — MCM removed 2026-09-14; configuration is vanilla-UI (INI + `TrueGazeConfig.html`).**
  - Phase 6: Skyrim VR Support & Performance Tuning (60-144 FPS).
  - Phase 7: Community Release, Documentation & Nexus Distribution.
  - Phase 8: Cross-Engine Expansion (Unreal Engine 5 & Unity HCEP Ecosystem).

#### [NEW] [CHANGELOG.md](file:///d:/Projects/SkyrimTrueGaze/CHANGELOG.md)

- Standard Keep a Changelog / SemVer document detailing:
  - `[Unreleased]` - Upcoming features and milestones.
  - `[1.0.0-rc1]` - Directory refactor to `SkyrimTrueGaze`, full C++ implementation of bridge server, animation hooks, OAR condition handler, target selector, and INI configuration, PRD, and Roadmap. *(MCM translations mentioned in the original entry were removed 2026-09-14.)*
  - `[0.2.0]` - Biomechanical kinematics algorithms, 64-byte IPC bridge specification, and OAR integration design.
  - `[0.1.0]` - Initial repository structure, CMake presets, and architecture blueprints.

---

## Verification Plan

### Automated Verification

1. **Kinematics & Packet Verification**:
   - Compile and execute `tests/KinematicsTests.cpp` using MSVC to verify all mathematical constants, equations, packet sizes, and CRC32 calculations.
2. **CMake Configuration**:
   - Clean stale build files and verify `cmake --preset windows-release` configure phase with the updated directory path `D:/Projects/SkyrimTrueGaze`.

### Manual Verification

- Review all generated documents (`PRD.md`, `ROADMAP.md`, `CHANGELOG.md`) for complete alignment with Kirk LaSalle's HCEP theory, architectural specifications, and Skyrim modding guidelines.
- Confirm that `Permanent_Active_Directives.txt` was not modified.
- Verify that no dangling references to `D:\Projects\TrueGaze` remain in active files.
