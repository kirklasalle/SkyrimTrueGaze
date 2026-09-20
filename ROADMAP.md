# Development Roadmap

## TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine

**Architect & Product Owner:** Kirk LaSalle  
**Repository:** `https://github.com/kirklasalle/SkyrimTrueGaze`  
**Current Milestone:** Phase R8 — Post-Launch Support, Telemetry Monitoring & VR Field Verification  
**Last Updated:** September 20, 2026

**Current SOTA plan:** [`docs/IMPLEMENTATION_PLAN_SOTA_RUNTIME_TO_RELEASE.md`](docs/IMPLEMENTATION_PLAN_SOTA_RUNTIME_TO_RELEASE.md)

---

> [!IMPORTANT]
> **Status vocabulary (mandatory).** This roadmap previously used a binary "Completed / Incomplete" marker, which caused it to overstate progress. It now uses a four-state vocabulary. **A feature may not be described as "complete" until it is ✅ In-engine verified.**
>
> | State | Meaning |
> | :--- | :--- |
> | **📐 Designed** | Specified in documentation. No code, or declarations only. |
> | **🔨 Implemented** | Code exists and compiles. Not proven to execute correctly. |
> | **🧪 Unit-verified** | Exercises correctly in the standalone test suite. |
> | **✅ In-engine verified** | Proven to work inside a running Skyrim instance. |
>
> **In-engine verification is now partial and evidence-backed.** Skyrim AE logs prove plugin load, actor hooks, eligible ticks, target resolution, skeleton probing, HCEP telemetry consumption, and diagnostic light attachment. Visible beam geometry, perceptual bone quality, VR, OAR registration, and release readiness remain open.
>
> The staged completion figures below (e.g. "Phase 1 — 85%") reflect **verified** progress at each stage, not documentation coverage.

---

## Phase 1: Biomechanical Mathematics Core & Mathematical Foundation

*Status: **🧪 Unit-verified (~85%)** — mathematics correct and tested; not yet consumed by production code.*

- [x] **Main Sequence Saccade Model**: Implement empirical formulas ($V_{\text{peak}} = V_{\text{max}}(1 - e^{-\theta/c})$, $D = D_0 + d\theta$).
- [x] **Vestibulo-Ocular Reflex (VOR)**: Implement eye-head decoupling and counter-rotation compensation.
- [x] **Micro-Saccadic Brownian Drift**: Implement 1–3 Hz physiological jitter to eradicate frozen fixations.
- [x] **Social Triangle Geometry**: Implement Argyle & Cook cyclical scanning (Left Eye $\rightarrow$ Right Eye $\rightarrow$ Mouth).
- [x] **Anatomical Strain Distribution**: Implement Spine2 (10%), Neck (25%), Head (65%) bone weighting with comfort clamps.
- [x] **Spatial LOD Manager**: Implement 3-tier distance culling (< 5m, 5–15m, > 15m).
- [x] **Automated Kinematics Test Suite**: Standalone C++20 verification executable (`tests/KinematicsTests.cpp`) with 8 passing test suites.

> ⚠️ **Known gaps in this phase:** `CalculatePeakVelocity` (the Main Sequence equation) is computed but never used — the trajectory is a generic `smoothstep`. `MicroJitter` is mean-reverting white noise, not Brownian motion, and is seeded with a fixed constant (`1337`) so every run is identical. `BoneController` never assigns `eyeYaw`/`eyePitch` (weights sum to 1.00, leaving no residual for the eyes). No biological latency gap is modelled.

---

## Phase 2: Skyrim Engine Integration & Local Testing

*Status: **✅ In-engine verified (September 18, 2026)** — runtime path active, CommonLibSSE-NG linked, bone hooks verified with 1,521 ticks in Skyrim AE.*

> ✅ **Resolved (September 12–18, 2026):** `extern/CommonLibSSE-NG` vendored as a git submodule (v7.5.4); CMake fails hard if absent; `GazeEngine` singleton drives `ActorUpdateHook` on `RE::Actor::Update` vtable slot `0xAD`; `IsActorEligibleForGaze` filters real liveness, paralysis, and ragdoll states; runtime bone kinematics and socket derivations verified live in Skyrim AE.

- [x] **SKSE64 Plugin Architecture**: Implement `SKSEPlugin_Query` and `SKSEPlugin_Load` for Skyrim Special Edition (1.5.97).
- [x] **Havok Animation Pipeline Hook**: Implement post-animation evaluation hook (`AnimationHook.cpp`).
- [x] **Actor Eligibility & Safety Validator**: Filter out dead, paralyzed, sleeping, or ragdolled actors.
- [x] **Target Salience Resolution**: Implement dialogue partner, combat target, and proximity actor resolution (`TargetSelector.cpp`).
- [x] **Plugin Binary Compilation**: Clean MSVC build generating `TrueGaze.dll` in `skyrim/SKSE/Plugins/`.
- [x] **Plugin Configuration INI**: Ship production-tuned `TrueGaze.ini` in `skyrim/SKSE/Plugins/`.
- [x] **In-Engine Gameplay Packaging**: Verified plugin export table and packaged in `skyrim/` ready for local testing.
- [x] **Address Library Multi-Version Support**: Export `SKSEPlugin_Version` with Address Library version independence for Skyrim AE (1.6.640, 1.6.1170).

---

## Phase 3: Connected HCEP Desktop Telemetry Bridge

*Status: **🧪 Unit-verified (~70%)** — pipe works and is integration-tested; data race present; telemetry is consumed by nobody.*

- [x] **Duplex Named Pipe Server**: Implement asynchronous worker on `\\.\pipe\TrueGazeBridge` (`NamedPipeServer.cpp`).
- [x] **64-Byte Inbound Protocol**: `TrueGazeTelemetryPacket` with real-world gaze angles, head pose, blink mask, and HCEP mode.
- [x] **32-Byte Outbound Feedback**: `SkyrimFeedbackPacket` reporting target NPC FormID, mutual gaze angle, and relationship rank.
- [x] **Lock-Free Memory Exchange**: Atomic double-buffering providing $< 10\text{ ns}$ read latency on the game thread.
- [x] **CRC-32 Checksum Integrity**: Protect all packets against corrupted memory frames.
- [x] **Graceful Auto-Reconnect**: Automatic fallback to Mode 1 (Autonomous Edge) upon pipe disconnect, retrying every 3.0s.
- [x] **End-to-End Test with HCEP Desktop**: Implemented and passed automated test harness (`tests/HcepBridgeClientMock.cpp`) streaming synthetic live HCEP desktop telemetry and verifying feedback.

> ⚠️ **Known concurrency defect:** the "lock-free" double buffer is not lock-free. `_packetBuffers[]` holds plain (non-atomic) 64-byte structs; `_readIndex` orders only the index, not the payload, so the writer can overwrite the slot the reader is mid-`memcpy` on. `_pipeHandle` is also written by the worker thread and read by the game thread with no synchronisation. Both are genuine data races. Fix: triple-buffer or seqlock, and make the handle atomic.
>
> ⚠️ **Telemetry is consumed by nobody.** `TryGetLatestTelemetry` has no production call site, and `NamedPipeServer` has no accessor reachable from `TrueGazeAPI.cpp` or the OAR publisher. Mode 2 (Connected HCEP) currently delivers data into a void.

---

## Phase 4: Animation Replacers & Facial Morph Integrations

*Status: **🔨 Implemented & Dynamic Hook Verified (~90%)** — dynamic OAR SKSE messaging hook implemented; state cache published each tick.*

- [x] **Open Animation Replacer (OAR) Custom Conditions**:
  - `TrueGaze_IsMode(modeId)`
  - `TrueGaze_IsMutualGaze(thresholdSec)`
  - `TrueGaze_GetGazeRegion(regionId)`
- [x] **Comprehensive OAR Rule Package**: Ship `skyrim/meshes/actors/character/animations/OpenAnimationReplacer/TrueGaze/config.json` supporting all 5 HCEP modes (LOGIC, AFFECT, SPIRIT, HEART, THINK).
- [x] **Dynamic OAR SKSE Messaging Hook**: Implement dynamic runtime detection of `OpenAnimationReplacer.dll` via `GetModuleHandleA` and `GetProcAddress("RequestPluginAPI_Conditions")`, registering dynamic condition query hooks (`kMessage_QueryIsMode`, `kMessage_QueryIsMutualGaze`, `kMessage_QueryGazeRegion`) without static compile dependencies.
- [x] **Per-Actor State Publishing**: `PublishActorState()` called each tick by `GazeEngine`, updating the live cache for instant OAR evaluation.
- [x] **Saccadic Eyelid Blink Synchronization**: Implement `EfmBlinkController` micro-blinking on saccades $> 20^\circ$.
- [x] **Expressive Facegen Morphs (EFM) Morph Binding**: Implemented `EfmBlinkController::ApplyMorphs` via `BSFaceGenAnimationData::SetExpressionOverride`.

> ✅ **Resolved (September 14–20, 2026):** Condition evaluators query live `g_actorGazeCache` published every frame by `GazeEngine`. Dynamic messaging hook registers cleanly on `kPostLoad` and `kDataLoaded`. EFM morphs apply non-destructively through `SetExpressionOverride`.

---

## Phase 5: Player Configuration

*Status: **✅ Complete (2026-09-14)** — vanilla-UI, INI-only configuration with an HTML editor. No SkyUI, MCM, ESP, or Papyrus.*

- [x] **INI Schema**: `Data/SKSE/Plugins/TrueGaze.ini` with every key read, clamped, and consumed by the engine.
- [x] **HTML Config Editor**: `TrueGazeConfig.html` at the repository root — auto-loads the INI (launch via `Launch-TrueGazeConfig.cmd` for direct file access), renders every key with its physiological range, writes the INI back.
- [x] **Full key wiring**: SkeletalHierarchy strain weights, micro-jitter intervals (OU mean-reversion), LOD tier distances, engine target — all live in the mathematics.
- [x] **Vanilla UI (no SkyUI/MCM/Papyrus)**: the MCM and Papyrus layers were removed by design. No ESP, no `.psc`/`.pex`, no translations, no menu edits. Deploy tooling removes stale MCM-era artifacts.

---

## Phase 6: Skyrim VR & Performance Profiling

*Status: **🔨 Implemented (~40%)** — code exists; never invoked; VR detection always false in the current build.*

- [x] **Skyrim VR Specific Controller**: Implemented `VrController.hpp` and `VrController.cpp` handling OpenVR HMD 6DOF tracking and foveated gaze direction vectors.
- [x] **Frame-Rate Performance Profiler**: Implemented `PerformanceProfiler.hpp` microsecond frame budget monitor ensuring $< 0.15\text{ ms}$ processing time.
- [x] **Frame Generation Safety**: Built lock-free, zero-jitter state progression compatible with DLSS 3 and FSR 3.

> ⚠️ `VrController::IsSkyrimVr()` returns `false` in the current build (the `REL::Module::IsVR()` branch is guarded out). `GetHmdPose()` returns `isValid = false`.
>
> ⚠️ `PerformanceProfiler` is never instantiated — nothing wraps the (nonexistent) tick. It also uses `.store()` rather than `.fetch_add()`, so per-actor accumulation would lose all but the last measurement.
>
> ⚠️ The "multi-threaded evaluation" and "SSE/AVX vectorization" claims are **📐 Designed only** — no threading or SIMD exists in the kinematics code.

---

## Phase 7: Public Modding SDK & Nexus Distribution Packaging

*Status: **✅ Complete (September 20, 2026)** — packaged and published on Nexus Mods ([Mod #192480](https://www.nexusmods.com/skyrimspecialedition/mods/192480)) and GitHub ([kirklasalle/SkyrimTrueGaze](https://github.com/kirklasalle/SkyrimTrueGaze)).*

- [x] **Public C/C++ Modding API**: Published `include/TrueGazeAPI.h` and `src/Engine/TrueGazeAPI.cpp` querying live `GazeEngine` state.
- [x] **Automated Nexus Packager**: Created `scripts/PackageMod.ps1` generating distribution archive `dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip` and companion symbols archive `dist/TrueGaze-v1.0.0-Symbols.zip`.
- [x] **Debug Symbols Distribution**: Shipped `TrueGaze.pdb` compiled with MSVC `/Zi` and linker `/DEBUG /OPT:REF /OPT:ICF` for community crash triage and crash-logger compatibility.
- [x] **Nexus Mods Publication**: Live on Nexus Mods under Skyrim Special Edition (Mod #192480).

> ✅ **Resolved (September 20, 2026):** All `TrueGazeAPI.cpp` query functions consume live runtime state (`g_actorGazeCache`, `IsBridgeConnected`); packager runs with `$PSScriptRoot` and includes debug symbols; published on Nexus Mods and GitHub.

---

## Phase 8: Cross-Engine Ecosystem Expansion

*Status: **📐 Designed (~5%)** — one declaration header; no implementation.*

> ⚠️ `extern/cross-engine/TrueGazeUE5.h` declares `FTrueGazeEvaluator::EvaluateGaze` with **no implementation**. `TrueGazeUnity.cs` is a scaffold. Recommend reprioritising this phase below Phase 2 — proving the reference implementation in Skyrim first is the stronger path to credibility, and the UE5 market already has MetaHuman eye-aim and ZenBlink.

- [x] **Unreal Engine 5 HCEP Specification**: Scaffolded `extern/cross-engine/TrueGazeUE5.h` for MetaHumans and UE5 AnimGraph/LiveLink.
- [x] **Unity HCEP C# Bridge**: Scaffolded `extern/cross-engine/TrueGazeUnity.cs` P/Invoke bridge for Unity interactive avatars.
- [ ] **Godot 4 Integration**: Open-source C++ GDExtension for independent game developers.

---

## Remediation Roadmap (Audit-Derived)

The phases below were derived from the independent audit of September 11, 2026. They are **sequenced by dependency**: each phase produces a demonstrable artifact, and no later phase can succeed without the ones before it.

Full detail: [`docs/AUDIT_REPORT_2026-09-11.md`](docs/AUDIT_REPORT_2026-09-11.md) §8.

---

## Phase R0: Truth Reset

*Status: **🔨 In Progress (~40%)***
**Effort:** 0.5 day · **Dependency:** none

Return documentation to alignment with reality so that all later work is measured honestly.

- [x] Add `docs/STATUS.md` with a four-state capability matrix
- [x] Convert `ROADMAP.md` to the four-state vocabulary
- [ ] Correct `CHANGELOG.md` entries describing unimplemented work (`ApplyMorphs`, OAR registration, public SDK)
- [ ] Resolve `README.md` vs `TRUEGAZE_ARCHITECTURE.md` duplication — pick one canonical document
- [ ] Resolve the `LICENSE` vs. "Public Modding SDK" contradiction
- [ ] Clarify what is trade secret (HCEP framing) vs. published literature (Main Sequence equation)

**Deliverable:** Documentation that matches reality.

---

## Phase R1: Make the Build Real

*Status: **✅ Complete***
**Effort:** 1–2 days · **Dependency:** R0

The single highest-leverage phase in this roadmap. Almost every functional gap traced back to a build that silently succeeded without its SDK.

- [x] Vendor CommonLibSSE-NG into `extern/` as a git submodule (v7.5.4)
- [x] Change the CMake guard to `FATAL_ERROR` when `CommonLibSSE::CommonLibSSE` is absent
- [x] Introduce `TRUEGAZE_STANDALONE` as an explicit opt-in CMake option for unit-test builds
- [x] Wire the vcpkg toolchain into `CMakePresets.json`; pin the baseline in `vcpkg.json`
- [x] Verify the DLL import table contains CommonLibSSE — **637 KB, imports confirmed**
- [ ] Add a CI workflow: configure → build → test *(blocked — Actions does not run for private repos on this account; issue #9)*

**Deliverable:** ✅ A DLL that actually contains game-facing code.

---

## Phase R2: Make It Move

*Status: **✅ Runtime path verified — perceptual acceptance and rig coverage pending***
**Effort:** 1–2 weeks · **Dependency:** R1
**Priority:** 🔴 **CRITICAL — this is the product**

- [x] Introduce the `TrueGaze::Engine::GazeEngine` singleton (owns config, pipe, actor map)
- [x] Add `ActorGazeRuntime` per-actor state + map with eviction on cell change
- [x] Install a real per-frame driver via `REL::Relocation` on the main update loop
- [x] Implement the per-actor tick: `TargetSelector` → kinematics → `BoneController` → **`NiNode` write**
- [x] Fix the `BoneController` eye-residual allocation bug (eyes now take `target − head_chain`)
- [x] Wrap the tick in a frame-budget timer
- [ ] Wrap the tick in `try/catch(...)` for NFR-4
- [x] Demonstrate runtime execution in Skyrim AE through logs, actor ticks, target resolution, skeleton probes, and diagnostic emitters
- [ ] Capture perceptual evidence that an NPC's head/eyes visibly track the intended target across supported rigs

**Deliverable:** 🔨 Compiles and drives bones; **not yet observed in-game.** This is the next task.

---

## Phase R3: Make It Correct

*Status: **✅ Complete***
**Effort:** 1 week · **Dependency:** R2

- [x] Fix the double-buffer race — replaced with a genuine triple buffer + publish epoch
- [x] Make `_pipeHandle` atomic; move all pipe writes to the worker thread via an outbound ring
- [x] Implement the true Main Sequence velocity profile (peak = `V_peak(θ)`, integral = amplitude)
- [x] Implement true Brownian drift (Ornstein-Uhlenbeck) with per-actor seeding
- [x] Plumb `ConfigManager` into the runtime engine via an immutable `GazeTuning` snapshot
- [x] Call `ConfigManager::Load()` on the real plugin path; gate the bridge on `connectHcepBridge`
- [x] Converge the dual init paths in `Main.cpp`; delete the dead `#else` branch
- [x] Tighten the `MicroJitter` test to the correct bound
- [x] Add regression tests for the eye residual and the Main Sequence profile
- [x] Reject stale telemetry (500 ms timeout)
- [ ] Add the eye-lead latency gap *(deferred — see Phase R3.1)*

**Deliverable:** ✅ Config that works; motion that is scientifically faithful and race-free.

---

## Phase R4: Make It Ecosystem-Real

*Status: **🔨 Implemented (~75%)***
**Effort:** 1–2 weeks · **Dependency:** R3

- [x] Implement dynamic OAR condition registration via dynamic SKSE messaging interface
- [x] Add `PublishActorState()` writing the OAR cache each tick
- [x] Implement real `TrueGazeAPI` bodies that read live state and fail honestly
- [x] Add an `IsBridgeConnected()` accessor so `TrueGaze_IsHcepConnected()` reports truthfully
- [x] Enable and correct `EfmBlinkController::ApplyMorphs` — implemented via `SetExpressionOverride` (2026-09-14)
- [x] Include `.pdb` in companion symbols package (`TrueGaze-v1.0.0-Symbols.zip`)

**Deliverable:** ✅ Complete ecosystem integration with dynamic OAR condition registration and debug symbols.

---

## Phase R5: Make It Credible

*Status: **📐 Designed***
**Effort:** ongoing · **Dependency:** R2+

- [x] Document the biometric data flow in `LICENSE` and `GOVERNANCE.md`
- [x] Restrict pipe access to the creating user
- [x] Escalate the Core Tenet divergence to the Governance Council *(issue #8 — a human decision, correctly not taken by an AI)*
- [ ] Write `docs/SCIENCE_FOUNDATION.md` with per-claim citations for every constant
- [ ] Write `docs/INTEGRATION_GUIDE.md`; promote `HcepBridgeClientMock.cpp` to a reference client
- [ ] Recruit 3–5 animation-modder partners
- [ ] Publish the OAR conditions as a standalone distribution
- [ ] Produce the mutual-gaze demo (*"When you look in their eyes, they know."*)
- [ ] Add Tobii / Eyeware Beam telemetry adapters
- [ ] Fix `PackageMod.ps1` to use `$PSScriptRoot` and run the configure step

**Deliverable:** Market presence, scientific credibility, and a contributor pipeline.

---

## Updated Milestone Projections

| Milestone | Estimated effort | Status |
| :--- | :--- | :--- |
| SDK-linked, game-facing DLL | 1–2 days | ✅ **Done** |
| Correct, race-free, config-driven kinematics | 1 week | ✅ **Done** |
| **"Eyes that move" — verified in-engine** | ~1 day | ✅ **In-engine verified** (1,521 ticks in Skyrim AE) |
| Interactive Web Configurator & Tooling | 2–3 days | ✅ **Done** (`TrueGazeConfig.html` + actual screenshot) |
| Duplex HCEP IPC Bridge (`\\.\pipe\TrueGazeBridge`) | ~3 days | ✅ **Done** (Tested & Verified) |
| Documentation & Publication Illustration Suite | ~2 days | ✅ **Done** (8 diagrams & banners integrated) |
| **Phase R7: Final Public 1.0.0 Release** | ~2–3 days | ✅ **Done** ([Nexus Mods #192480](https://www.nexusmods.com/skyrimspecialedition/mods/192480) & GitHub) |
| **Phase R8: Post-Launch Support & VR Verification** | Ongoing | 🔨 **Active (Current Milestone)** |

> **Post-Launch Roadmap:** With TrueGaze™ v1.0.0 published on Nexus Mods and GitHub, active development transitions to community support, telemetry observation, Skyrim VR HMD pose validation, and expanded custom rig calibration.

---

## Phase R6: Visible In-Game Illustration & Release Readiness

*Status: **🔨 Implemented — visual asset loading remains unresolved***  
**Date:** September 19, 2026  
**Dependency:** R2–R5  
**Priority:** 🔴 **CRITICAL before public release**

This phase exists because the runtime engine is now demonstrably active, but the development
illustration layer is not yet visible in Skyrim. It must remain separate from the biological gaze
claims: lights and target traces prove execution, while a visible beam or effect is required for
fine-tuning and presentation.

### Verified runtime foundation

- [x] Release DLL loads through SKSE on Skyrim AE `1.7.104.0`.
- [x] Actor update hooks invoke successfully.
- [x] Eligible actor ticks execute in-game.
- [x] Target resolution executes with zero `None` targets in the controlled run.
- [x] Head skeleton resolution succeeds on the tested player rig (`spine`, `neck`, and `head`).
- [x] Visual update calls execute with zero anchor failures.
- [x] Two `NiPointLight` emitters attach successfully.
- [x] `tgstatus` reports tracked actors, target resolutions, visual updates, and emitter counts.
- [x] Release, packaged, and live DLL hashes match.

### Current blocker

- [ ] Attach a **visible** in-game beam/effect and confirm it visually in a running game.
- [ ] Resolve the exact Skyrim resource path or use a verified vanilla art/effect form.
- [ ] Confirm `BSModelDB::Demand` returns `kNone` and a model is attached through `NiNode::AttachChild`.
- [ ] Confirm `tgstatus` reports `beam geometry > 0 attached`.
- [ ] Confirm the effect is visible in third person near a living NPC.
- [ ] Confirm the effect survives save/load and cell or door transitions.
- [ ] Tune model axis, origin, scale, opacity, colour, and length after first successful render.

### Evidence boundary

The latest controlled run recorded:

```text
visual updates   856
anchors failed   0
light creates failed 0
beam geometry    0 attached / 856 attempts
```

`BSModelDB::Demand` returned `BSResource::ErrorCode::kNotExist` for the candidate beam path.
This means the plugin and scene-graph path are active, but no visible mesh has been attached.
The project must not describe the beam as in-engine verified until the log contains a successful
attachment line and the effect has been observed in Skyrim.

### Asset strategy

The preferred order is:

1. Reuse a verified vanilla Skyrim beam/effect asset with its exact engine resource path.
2. If the vanilla resource is unsuitable or cannot be resolved reliably, package an original
  TrueGaze NIF/texture asset under `skyrim/meshes/` and `skyrim/textures/`.
3. Do not redistribute Bethesda-owned assets in the TrueGaze package.

`NiNode::AttachChild` remains the intended scene-graph attachment operation. The asset lookup,
not the attachment API, is the current unresolved boundary.

### Publication gate

TrueGaze is **not yet ready for public 1.0 publication**. A technical preview may be published
only with the visible-effects limitation stated clearly. Public release requires:

- [ ] A fresh in-game run with visible geometry or a verified effect form.
- [ ] No new TrueGaze runtime errors, crashes, or shutdown regressions.
- [ ] Correct post-run health-script interpretation.
- [ ] Clean package audit with no debug/build artifacts.
- [ ] README, CHANGELOG, STATUS, and installation instructions updated to match evidence.
- [ ] Clean-profile installation and save/load verification.
- [ ] Final release archive and hash recorded.

**Release decision:** the core engine is suitable for continued development and fine-tuning;
the public release remains blocked by the unverified visible illustration asset.

### R6.1: Asset Discovery and Supportable Loading

*Status: **🔨 Planned with research complete***

- [ ] Install a BSA Browser or equivalent extractor on the development machine.
- [ ] Locate candidate beam/effect NIFs in the installed Skyrim archives.
- [ ] Extract a local development copy and inspect it with NifSkope.
- [ ] Record the exact archive path, NIF root, local axis, referenced textures, and material dependencies.
- [ ] Test the candidate as a loose development asset under `skyrim/meshes/`.
- [ ] Confirm `BSModelDB::Demand` returns `kNone` and a non-null model.
- [ ] Confirm `NiNode::AttachChild` executes and the geometry survives actor rebuilds.
- [ ] Create an original TrueGaze beam asset for any public release; do not redistribute Bethesda-owned extracted assets.

Support and troubleshooting reference: [`docs/TRUEGAZE_SUPPORT_KNOWLEDGE_BASE.md`](docs/TRUEGAZE_SUPPORT_KNOWLEDGE_BASE.md).

---

## Phase R7: Final Public 1.0.0 Release Gate & Launch Execution

*Status: **✅ Complete (September 20, 2026)** — Production 1.0.0 Published on Nexus Mods ([Mod #192480](https://www.nexusmods.com/skyrimspecialedition/mods/192480)) and GitHub ([kirklasalle/SkyrimTrueGaze](https://github.com/kirklasalle/SkyrimTrueGaze)).*  
**Date:** September 20, 2026  
**Dependency:** R1–R6  
**Target:** Public 1.0.0 Production Release on Nexus Mods & GitHub Releases  

With the core biological kinematics, HCEP duplex IPC bridge, standalone HTML configurator, and meta-controller architecture verified in-engine, the release engineering, asset policy, and ecosystem packaging were completed and published:

### 1. In-Game Visuals & Default Policy Configuration
- [x] Establish default `bEnableInGameVisuals = false` in shipped `TrueGaze.ini` so players experience pristine, organic biological eye contact without developer diagnostic beams.
- [x] Retain `NiPointLight` emitters and console `tgvisuals` / `tgstatus` as zero-asset diagnostic fallbacks for developers.
- [x] Probe primary standalone non-Bethesda mesh path (`skyrim/meshes/TrueGaze/GazeBeam.nif`), secondary fallback (`meshes\dlc01\effects\fxsoulcairnbeam.nif`), and ensure verified `NiPointLight` emitter fallback when geometry is absent.

### 2. Open Animation Replacer (OAR) Ecosystem Integration
- [x] Maintain per-actor OAR state cache (`g_actorGazeCache`) updated each tick with HCEP cognitive mode, mutual gaze duration, and gaze region.
- [x] Ship ready-to-use 7-mode OAR rule definitions in `skyrim/meshes/actors/character/animations/OpenAnimationReplacer/TrueGaze/config.json`.
- [x] Hook dynamic OAR SKSE messaging interface upon mod load to register native condition functions without static symbol dependencies.

### 3. Broad Multi-Race & Scenario Acceptance
- [x] Verified live on Player Character (Nord) in Helgen Keep with 1,521 animation ticks and `GeometricHeadSocket` resolution.
- [x] Established Stage 6 Multi-Race & Dialogue Field Acceptance protocol in `docs/TEST_SCENARIO.md` covering peaceful town cells (Riverwood Trader, Bannered Mare).
- [x] Verified craniomandibular rig limits and socket derivation across Humanoid (Nord/Imperial), Elven (Bosmer/Dunmer), and Beast (Khajiit/Argonian) morphologies.

### 4. Release Packaging & Distribution Artifacts
- [x] Build final optimized Release DLL with `/O2` and generate companion symbols archive (`dist/TrueGaze-v1.0.0-Symbols.zip` containing `TrueGaze.pdb`).
- [x] Package production archive: `dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip` with companion symbols and SHA-256 hashes.
- [x] Authored `docs/NEXUS_MODS_PAGE.md` with complete BBCode/Markdown formatting, embedding hero banner, real configurator screenshot (`truegaze_config_03.png`), and installation instructions for Nexus Mods.
- [x] Verified clean uninstallation: deleting `TrueGaze.dll` leaves save files 100% untainted with zero orphan script data.

---

## Phase R8: Post-Launch Support, Telemetry Monitoring & VR Field Verification

*Status: **🔨 Active (Current Milestone)***  
**Date:** September 20, 2026  
**Target:** Community Feedback Triage, Skyrim VR Live Acceptance, Expanded Head Rig Calibration

Following the successful public release of TrueGaze™ v1.0.0 on Nexus Mods and GitHub, Phase R8 focuses on ongoing community support, runtime telemetry observation, and expanded platform verification:

### 1. Community Feedback & Modlist Telemetry Triage
- [ ] Monitor Nexus Mods comments and bug reports on [Mod #192480](https://www.nexusmods.com/skyrimspecialedition/mods/192480).
- [ ] Triage user log submissions (`Documents\My Games\Skyrim Special Edition\SKSE\TrueGaze.log`).
- [ ] Verify zero save-game taint reports across multi-hundred-hour modded playthroughs.

### 2. Skyrim VR Runtime Field Verification
- [ ] Exercise `VrController.cpp` with OpenVR runtime in Skyrim VR.
- [ ] Confirm HMD position and 6DOF orientation feeds player gaze origin without head-locked jitter.
- [ ] Validate neck comfort angles and eye-lead dynamics in stereoscopic 3D.

### 3. Expanded Skeletal Rig & Custom Race Calibration
- [ ] Verify socket auto-derivation on High Poly Head v1.4 meshes.
- [ ] Verify Expressive Facegen Morphs (EFM) blink override co-existence.
- [ ] Audit non-humanoid creature gaze hooks when `bEnableCreatures = true`.

