# Changelog

All notable changes to the **TrueGaze™** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> [!IMPORTANT]
> **Correction notice.** Earlier entries in this changelog described several features as "implemented" that are not functional, because they were written from design intent rather than from the code. Those entries have been annotated below. See [`docs/STATUS.md`](docs/STATUS.md) for the verified capability matrix and [`docs/AUDIT_REPORT_2026-09-11.md`](docs/AUDIT_REPORT_2026-09-11.md) for the full independent audit.

---

## [Unreleased]

### Added — Governance Enforcement

- **`GOVERNANCE.md`** — the charter enforcement map required by `AGENTIC_SACRED_COVENANT.md` §3.1. States plainly which controls are implemented, which are gaps, and which are not applicable to a Skyrim plugin. Includes the Law 6 biometric gap analysis and the amendment procedure.
- **`scripts/verify_charter.py`** — implements the `LAW10-CHARTER` control. Pins the 10 Laws and 4 Core Tenets by SHA-256 digest and verifies them across every charter document. Detects a Law that has been reworded, dropped, duplicated, renumbered, or left disagreeing between copies.
- **`config/charter_manifest.json`** — canonical digests of the Laws and Core Tenets, plus recorded known divergences.
- **`.github/workflows/charter-integrity.yml`** — CI enforcement of charter integrity, independent of the C++ build.

### Fixed — Charter Integrity

- **Fourth Law capitalisation** — `AGENTIC_PRIME_DIRECTIVE.md` and `AGENTIC_SACRED_COVENANT.md` read "An **I**ntelligence System" where the canonical text reads "An **i**ntelligence System". Corrected.
- **Ninth Law em-dash spacing** — both markdown charters read "verified — recognizing" where the canonical text reads "verified—recognizing". Corrected.

  The 10 Laws are now **byte-identical** across all three charter documents.

### Discovered — Governance Findings

- **🔴 Law 6 (biometric data) — no runtime control.** The HCEP bridge transmits real-world gaze vectors, head pose, blink state, a tracked person identifier, and cognitive/emotional classification over a named pipe created with `nullptr` security attributes (`src/Bridge/NamedPipeServer.cpp:75`). There is no access control, no encryption in transit, no consent capture, and no connection auditing — while `LICENSE` asserts GDPR Article 9, CCPA, and BIPA compliance. Practical risk is low for a single-user local mod, but the governance position is not defensible as written. Remediation plan in `GOVERNANCE.md`.
- **🔴 Law 10 (approval) — no cryptographic control.** `verify_charter.py --update` regenerates the manifest with a plain file write. There is no signature and no separation between recording a change and authorising it. Currently mitigated procedurally (explicit reminder + mandatory diff review), not cryptographically.
- **🟡 Core Tenet divergence.** All four Core Tenets in `AGENTIC_PRIME_DIRECTIVE.md` and `AGENTIC_SACRED_COVENANT.md` are **paraphrases** of the canonical text in `Permanent_Active_Directives.txt`, not reproductions. Recorded in the manifest as disclosed divergences pending Governance Council review. Three options are documented in `GOVERNANCE.md`; the choice is reserved to the Council.
- **🟡 Laws 1–5 and 8 have no runtime control.** Consistent with the covenant's own disclosure. These are governing principles for human conduct on this project, not runtime constraints on the plugin. Recorded so they cannot be mistaken for implemented controls.

### Added — Independent Audit & Honest Status Reporting

- **`docs/AUDIT_REPORT_2026-09-11.md`** — Full independent audit: complete documentation review, complete source review, binary forensics on the shipped `TrueGaze.dll`, distribution-archive inspection, and competitive market research (Nexus Mods landscape, UE5/MetaHuman gaze ecosystem, commercial eye-tracking, academic saccade literature).
- **`docs/STATUS.md`** — Verified capability matrix using a mandatory four-state vocabulary (📐 Designed / 🔨 Implemented / 🧪 Unit-verified / ✅ In-engine verified). Replaces binary "complete/incomplete" reporting, which had overstated progress.
- **Governance policy** — "Reporting and Verification Policy" now prohibits describing a feature as complete before it is ✅ In-engine verified, and prohibits log messages reporting success for operations not performed.

### Changed

- **`README.md`** — Added status banner, badges, and a **Current Project Status** section that disclaims the gap between the designed architecture and the current build. Clarified that the README describes the *target* system.
- **`ROADMAP.md`** — Converted all phase statuses from "Completed (100%)" to a four-state vocabulary with **verified** percentages (Phase 1 ~85%, Phase 2 ~10%, Phase 3 ~70%, Phase 4 ~15%, Phase 5 ~45%, Phase 6 ~40%, Phase 7 ~50%, Phase 8 ~5%). Added the audit-derived remediation roadmap (Phases R0–R5) sequenced by dependency, with milestone projections.

### Fixed — Documentation Accuracy

The following claims in earlier entries were found to be **inaccurate** and have been annotated in place:

- **`EfmBlinkController::ApplyMorphs` — not implemented.** The only substantive logic (the `exprOverrides` writes) is commented out; both code paths are no-ops. The cited `RE::FaceGen::Expression::BlinkLeft` used as an array subscript will not compile once the SDK is present; this code has never been compiled against CommonLibSSE.
- **OAR condition registration — not implemented.** `OarConditions::RegisterWithOar()` contains a `// Future:` comment where registration should be, yet logs a success message and returns `true`. The state cache the evaluators read (`g_actorGazeCache`) is never written to. Net effect: the 7-rule OAR package fires Rule 1 unconditionally and Rules 2–7 never fire.
- **Public modding SDK — export surface only.** All `TrueGazeAPI.cpp` bodies are stubs. `TrueGaze_IsHcepConnected()` returns a hardcoded `false`; `TrueGaze_GetActorGaze()` returns hardcoded zeroes and a hardcoded `0x14` target FormID while returning `true`; `TrueGaze_OverrideActorMode()` is an empty body.
- **Papyrus bindings — not registered.** No `SKSE::GetPapyrusInterface()->Register(...)` call exists anywhere, and the declared signatures in `TrueGaze.psc` do not match the exported symbols.
- **Animation hooks — not installed.** `AnimationHook::Install()` constructs no `REL::Relocation`; the hook body that would apply bone rotation is a comment.
- **MCM — cannot bind.** The MCM schema declares `"sourceForm": "TrueGaze.esp"` on every entry, but no such plugin exists in the repository. Additionally, no compiled `.pex` scripts ship, so SkyUI finds no script to run.
- **Configuration — never loaded on the real plugin path.** `ConfigManager::Load()` is called only in the `#else` fallback branch of `Main.cpp`. Every INI setting and every MCM slider is inert.

### Discovered — Verified Defects

- **🔴 BLOCKER: the engine is inert.** No bone transform is ever written. `SaccadeGenerator`, `VorCoordinator`, `MicroJitter`, `SocialTriangle`, and `BoneController` have no production call sites — they are referenced only by `tests/KinematicsTests.cpp`.
- **🔴 BLOCKER: the build omits its SDK.** `extern/CommonLibSSE-NG` does not exist. The CMake guards (`if(EXISTS)` / `if(TARGET)`) silently skip both the subdirectory and the link step, so the build succeeds and produces a standalone skeleton. Every `#if __has_include(<RE/Skyrim.h>)` block resolves to a fallback stub.
- **`BoneController::CalculateHierarchyStrain` allocates no residual to the eyes.** Weights `0.10 + 0.25 + 0.65 = 1.00`, so `eyeYaw`/`eyePitch` are declared but never assigned. The eyes should receive `target − head_total`. This is a correctness bug in the project's signature feature.
- **`NamedPipeServer` double buffer is not lock-free.** `_packetBuffers[]` holds plain (non-atomic) 64-byte structs; `_readIndex` orders only the index, not the payload, so the writer can overwrite the slot the reader is mid-`memcpy` on. `_pipeHandle` is additionally written by the worker thread and read by the game thread with no synchronisation. Both are genuine data races.
- **`MicroJitter` is not Brownian.** It resamples an independent uniform target each interval and exponentially approaches it — a mean-reverting process, not a random walk. It is also seeded with a fixed constant (`1337`), so every run produces an identical jitter sequence.
- **`CalculatePeakVelocity` (the Main Sequence equation) is never used.** The saccade trajectory is a generic `smoothstep`. Additionally, `CalculateDuration` sets the correct total duration, but the *velocity profile within it* is a cubic ease rather than a Main Sequence profile — a fidelity gap between the documentation's central scientific claim and the implementation.
- **No biological latency gap is modelled.** Docs specify eyes lead by 20–30 ms and head follows at 120–180 ms; the implementation updates both in the same frame with only a damping constant differentiating them.
- **`PerformanceProfiler::ScopedTimer` uses `.store()` rather than `.fetch_add()`** — only the last measurement survives, which would break the intended per-actor accumulation.
- **`SocialTriangle.hpp` uses `std::max` without including `<algorithm>`** — compiles only by transitive include.
- **`PackageMod.ps1` hardcodes an absolute project path** and runs `cmake --build` with no preceding configure step. `.pdb` files are not shipped.
- **Magic numbers in `TargetSelector.cpp`** — `0.01428f` (Skyrim units-to-metres) and `160.0f` (eye-height offset) appear as bare literals five and six times respectively.
- **`TrueGaze.ini` contains a dead setting** — `sEngineTarget=Auto` is never read by `ConfigManager`. `iLogLevel` is parsed but never applied.
- **`LICENSE` contradicts the "Public Modding SDK" claim** — the license states "No license is granted... copying, distribution... prohibited".
- **`.gitignore` excludes the shipping artifacts** (`*.dll`, `*.zip`, `build/`) that the packaging script depends on.

### Verified Working

Confirmed correct by direct inspection during the audit:

- All 7 DLL exports present: `SKSEPlugin_Load`, `SKSEPlugin_Query`, `SKSEPlugin_Version`, `TrueGaze_GetVersion`, `TrueGaze_IsHcepConnected`, `TrueGaze_GetActorGaze`, `TrueGaze_OverrideActorMode`
- `TelemetryPacket.h` compile-time `static_assert` size guards (64-byte / 32-byte) — exemplary practice
- CRC-32 verify-before-publish in the pipe worker
- Correct DoS guard in the pipe read loop (`PeekNamedPipe` size check before `ReadFile`)
- Asynchronous pipe server with graceful auto-reconnect
- Biomechanical kinematics library — mathematically correct, correctly cited
- 8-suite standalone unit test harness — all passing
- IPC integration test harness
- Correct Skyrim mod package structure (MO2/Vortex layout)
- Well-designed 7-rule OAR condition package
- 6-language MCM localisation

---

## [1.0.0-rc1] - 2026-09-11

### Added

- **Multi-Version SKSE64 Plugin Exports**: Unified DLL exporting `SKSEPlugin_Query` and `SKSEPlugin_Load` (Skyrim SE 1.5.97) and `SKSEPlugin_Version` with Address Library version independence (Skyrim AE 1.6+).
- **NamedPipeServer Implementation & Verification**: Asynchronous Windows Named Pipe worker (`\\.\pipe\TrueGazeBridge`) featuring lock-free double-buffered telemetry exchange (< 10ns read time), CRC-32 validation, and non-blocking `PeekNamedPipe` polling (`src/Bridge/NamedPipeServer.cpp`).
- **End-to-End HCEP Integration Test Harness**: Standalone client test (`tests/HcepBridgeClientMock.cpp`) simulating live HCEP desktop perception streaming and validating 32-byte feedback reception.
- **AnimationHook Implementation**: Post-Havok actor animation evaluation pipeline with eligibility checks (filtering dead, paralyzed, sleeping, or ragdolled actors) and state-based gaze weighting (`src/Engine/AnimationHook.cpp`).
- **TargetSelector Implementation**: Salience prioritization engine resolving dialogue partners, combat adversaries, proximity actors, and ambient focus points (`src/Engine/TargetSelector.cpp`).
- **OarConditions Implementation & Config Rules**: Thread-safe condition cache and sample rules (`skyrim/meshes/actors/character/animations/OpenAnimationReplacer/TrueGaze/config.json`) supporting all 5 HCEP modes (LOGIC, AFFECT, SPIRIT, HEART, THINK).
- **Expressive Facegen Morphs (EFM) Integration**: Implemented `EfmBlinkController::ApplyMorphs` for face morph target weight calculations during saccades.
- **Mod Configuration Menu (MCM) & Localization**:
  - SkyUI Papyrus script `TrueGaze_MCM.psc` with toggle and slider event handlers.
  - 6 MCM translation languages: English, French, German, Spanish, Japanese, and Chinese.
- **Skyrim VR & Performance Instrumentation**:
  - `VrController.hpp`/`cpp` handling OpenVR HMD 6DOF transforms and foveated gaze projections.
  - `PerformanceProfiler.hpp` microsecond execution timer ensuring frame budget $< 0.15\text{ ms}$.
- **Public Modding SDK & Native Papyrus Bindings**:
  - `include/TrueGazeAPI.h` and `src/Engine/TrueGazeAPI.cpp` exporting public C/C++ API (`TrueGaze_GetActorGaze`, `TrueGaze_GetVersion`, `TrueGaze_IsHcepConnected`, `TrueGaze_OverrideActorMode`).
  - `skyrim/scripts/source/TrueGaze.psc` exposing native Papyrus functions.
- **Automated Nexus Packager**: Created `scripts/PackageMod.ps1` and compiled distribution package `dist/TrueGaze-v1.0.0-rc1-SkyrimSE-AE-VR.zip`.
- **Pure C++20 Kinematics Test Suite**: Standalone automated test harness (`tests/KinematicsTests.cpp`) validating 8 core components.
- **Product Requirements Document (`PRD.md`)**: Comprehensive technical document specifying product vision, personas, functional/non-functional requirements, wire protocols, and acceptance criteria.
- **Detailed Development Roadmap (`ROADMAP.md`)**: Strategic multi-phase roadmap spanning kinematics core through Nexus release and cross-engine expansion.

### Changed

- **Project Directory Refactoring**: Transitioned repository root from `D:\Projects\TrueGaze` to `D:\Projects\SkyrimTrueGaze` across all documentation, architecture guides, headers, build presets, and `.nexus` handover records.
- **Precompiled Header & Compiler Warning Hardening**: Added `[[maybe_unused]]` attributes and MSVC C4189 warning suppression in kinematics coordinators. Wrapped external logging headers with fallback macros for standalone compilation.

---

## [0.2.0] - 2026-09-10

### Added

- Mathematical foundation for biological oculomotor dynamics:
  - `SaccadeGenerator.hpp`: Main Sequence peak velocity ($750^\circ/\text{s}$) and duration calculations.
  - `VorCoordinator.hpp`: Vestibulo-Ocular Reflex eye-head decoupling and counter-rotation.
  - `MicroJitter.hpp`: Brownian random-walk drift eliminating frozen stares.
  - `SocialTriangle.hpp`: Argyle & Cook cyclical scanning between eyes and mouth.
  - `BoneController.hpp`: NetImmerse bone strain distribution (Spine2 10%, Neck 25%, Head 65%).
  - `LodManager.hpp`: 3-tier spatial level-of-detail culling.
  - `EfmBlinkController.hpp`: Micro-blink triggering for saccades $> 20^\circ$.
- 64-byte inbound telemetry wire protocol (`TrueGazeTelemetryPacket`) and 32-byte outbound feedback protocol (`SkyrimFeedbackPacket`).
- Open Animation Replacer (OAR) integration specification and HCEP bridge specification documents.
- In-game Mod Configuration Menu schema (`skyrim/Interface/MCM/Config/TrueGaze/config.json`).

---

## [0.1.0] - 2026-09-09

### Added

- Initial project architecture and mission blueprint for TrueGaze™ as a first-party HCEP gaming product.
- Modern CMakePresets.json targeting MSVC C++20 for Skyrim SE, AE, and VR.
- Initial CMakeLists.txt and vcpkg.json dependency manifests.
- Master project overview (`README.md`) and technical architecture document (`TRUEGAZE_ARCHITECTURE.md`).
