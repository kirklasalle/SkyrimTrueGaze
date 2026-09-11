# Development Roadmap

## TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine

**Architect & Product Owner:** Kirk LaSalle  
**Repository:** `https://github.com/kirklasalle/SkyrimTrueGaze`  
**Current Milestone:** Phase 0 — Truth Reset → Phase 1 (Make the Build Real)  
**Last Updated:** September 11, 2026

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
> **Nothing in this project has yet reached ✅ In-engine verified.** See [`docs/STATUS.md`](docs/STATUS.md) for the full capability matrix and [`docs/AUDIT_REPORT_2026-09-11.md`](docs/AUDIT_REPORT_2026-09-11.md) for the independent audit.
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

*Status: **🔨 Implemented (~10%)** — 🔴 **BLOCKER: the engine is inert. No bone is ever written.***

> 🔴 **This phase was previously marked "Completed (100%)". It is not.** The animation hook is declared but never installed (`Install()` contains no `REL::Relocation`), and the hook body that would apply bone rotation is a comment. Additionally, `extern/CommonLibSSE-NG` does not exist, so the build silently degrades to a standalone skeleton and every `#if __has_include(<RE/Skyrim.h>)` block resolves to a stub. **Installing `TrueGaze.dll` today will not move any NPC's eyes.**

- [x] **SKSE64 Plugin Architecture**: Implement `SKSEPlugin_Query` and `SKSEPlugin_Load` for Skyrim Special Edition (1.5.97).
- [x] **Havok Animation Pipeline Hook**: Implement post-animation evaluation hook (`AnimationHook.cpp`).
- [x] **Actor Eligibility & Safety Validator**: Filter out dead, paralyzed, sleeping, or ragdolled actors.
- [x] **Target Salience Resolution**: Implement dialogue partner, combat target, and proximity actor resolution (`TargetSelector.cpp`).
- [x] **Plugin Binary Compilation**: Clean MSVC build generating `TrueGaze.dll` in `skyrim/SKSE/Plugins/`.
- [x] **Plugin Configuration INI**: Ship production-tuned `TrueGaze.ini` in `skyrim/SKSE/Plugins/`.
- [x] **In-Engine Gameplay Packaging**: Verified plugin export table and packaged in `skyrim/` ready for local testing.
- [x] **Address Library Multi-Version Support**: Export `SKSEPlugin_Version` with Address Library version independence for Skyrim AE (1.6.640, 1.6.1170).

> ⚠️ **Verified:** all 7 exports are present in the shipped DLL. **Not verified:** that any of them do anything beyond loading. `IsActorEligibleForGaze` returns `true` for any non-zero FormID in the current build; `GetActorGazeWeight` unconditionally returns `1.0f`.

---

## Phase 3: Connected HCEP Desktop Telemetry Bridge

*Status: **🧪 Unit-verified (~70%)** — pipe works and is integration-tested; data race present; telemetry is consumed by nobody.*

- [x] **Duplex Named Pipe Server**: Implement asynchronous worker on `\\.\pipe\TrueGazeBridge` (`NamedPipeServer.cpp`).
- [x] **64-Byte Inbound Protocol**: `TrueGazeTelemetryPacket` with real-world gaze angles, head pose, blink mask, and HCEP mode.
- [x] **32-Byte Outbound Feedback**: `SkyrimFeedbackPacket` reporting target NPC FormID, mutual gaze angle, and relationship rank.
- [x] **Lock-Free Memory Exchange**: Atomic double-buffering providing $< 10\text{ ns}$ read latency on the game thread.
- [x] **CRC-32 Checksum Integrity**: Protect all packets against corrupted memory frames.
- [x] **Graceful Auto-Reconnect**: Automatic fallback to Mode 1 (Autonomous Edge) upon pipe disconnect, retrying every 3.0s.
- [x] **End-to-End Test with HCEP Desktop**: Implemented and passed automated test harness (`tests/HcepBridgeClientMock.cpp`) simulating live HCEP desktop telemetry streaming and feedback verification.

> ⚠️ **Known concurrency defect:** the "lock-free" double buffer is not lock-free. `_packetBuffers[]` holds plain (non-atomic) 64-byte structs; `_readIndex` orders only the index, not the payload, so the writer can overwrite the slot the reader is mid-`memcpy` on. `_pipeHandle` is also written by the worker thread and read by the game thread with no synchronisation. Both are genuine data races. Fix: triple-buffer or seqlock, and make the handle atomic.
>
> ⚠️ **Telemetry is consumed by nobody.** `TryGetLatestTelemetry` has no production call site, and `NamedPipeServer` has no accessor reachable from `TrueGazeAPI.cpp` or the OAR publisher. Mode 2 (Connected HCEP) currently delivers data into a void.

---

## Phase 4: Animation Replacers & Facial Morph Integrations

*Status: **📐 Designed (~15%)** — 🔴 condition registration and morph application are not implemented.*

- [x] **Open Animation Replacer (OAR) Custom Conditions**:
  - `TrueGaze_IsMode(modeId)`
  - `TrueGaze_IsMutualGaze(thresholdSec)`
  - `TrueGaze_GetGazeRegion(regionId)`
- [x] **Comprehensive OAR Rule Package**: Ship `skyrim/meshes/actors/character/animations/OpenAnimationReplacer/TrueGaze/config.json` supporting all 5 HCEP modes (LOGIC, AFFECT, SPIRIT, HEART, THINK).
- [x] **Saccadic Eyelid Blink Synchronization**: Implement `EfmBlinkController` micro-blinking on saccades $> 20^\circ$.
- [x] **Expressive Facegen Morphs (EFM) Morph Binding**: Implemented `EfmBlinkController::ApplyMorphs` for face morph target weight calculations.

> ⚠️ **Not implemented as described.** `OarConditions::RegisterWithOar()` contains a `// Future:` comment where the registration should be, yet logs a **success message** and returns `true`. The state cache it reads (`g_actorGazeCache`) is never written to by any code. Net effect: the 7-rule OAR package fires Rule 1 unconditionally and Rules 2–7 never fire.
>
> ⚠️ `EfmBlinkController::ApplyMorphs` has its only substantive logic **commented out** in both branches — it is a no-op. The cited `RE::FaceGen::Expression::BlinkLeft` used as an array subscript will not compile once the SDK is present; this code has never been compiled against CommonLibSSE.
>
> ⚠️ The success log in `RegisterWithOar` must be removed — reporting success for unperformed work actively misleads diagnosis.

---

## Phase 5: Player Configuration & SkyUI MCM Interface

*Status: **🔨 Implemented (~45%)** — assets complete; no binding, no persistence, no compiled scripts.*

- [x] **MCM JSON Schema**: Implement `skyrim/Interface/MCM/Config/TrueGaze/config.json` with sliders, toggles, and help texts.
- [x] **SkyUI Localization**: Implemented 6 localization files:
  - English (`TrueGaze_ENGLISH.txt`)
  - French (`TrueGaze_FRENCH.txt`)
  - German (`TrueGaze_GERMAN.txt`)
  - Spanish (`TrueGaze_SPANISH.txt`)
  - Japanese (`TrueGaze_JAPANESE.txt`)
  - Chinese (`TrueGaze_CHINESE.txt`)
- [x] **Papyrus MCM Script**: Implemented `skyrim/scripts/source/TrueGaze_MCM.psc` with slider/toggle callbacks.

> ⚠️ **The MCM cannot bind.** `Interface/MCM/Config/TrueGaze/config.json` declares `"sourceForm": "TrueGaze.esp"` on every entry, but **no such plugin exists** in this repository. MCM Helper's `GetFormFromFile` lookup will fail.
>
> ⚠️ **Configuration is never loaded on the real plugin path.** `ConfigManager::Load()` is called only in the `#else` fallback branch of `Main.cpp` — the branch that cannot execute inside real Skyrim. Every INI setting and every MCM slider is therefore inert.
>
> ⚠️ **No compiled `.pex` scripts ship.** The package contains `.psc` source only, so SkyUI finds no MCM script to run.
>
> ⚠️ The INI keys and the MCM `setting` strings are parallel, unconnected configuration systems — INI parsing via `GetPrivateProfileString` does not read MCM Helper globals. One authoring path must be chosen.

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

*Status: **🔨 Implemented (~50%)** — package builds; API surface is stub-only; no Papyrus binding.*

- [x] **Public C/C++ Modding API**: Published `include/TrueGazeAPI.h` and `src/Engine/TrueGazeAPI.cpp` exporting query and mode override functions.
- [x] **Papyrus Script API**: Published `skyrim/scripts/source/TrueGaze.psc` exposing native functions for quest/follower mod authors.
- [x] **Automated Nexus Packager**: Created `scripts/PackageMod.ps1` and generated distribution zip archive `dist/TrueGaze-v1.0.0-rc1-SkyrimSE-AE-VR.zip`.

> ⚠️ **All `TrueGazeAPI.cpp` bodies are stubs.** `TrueGaze_IsHcepConnected()` returns a hardcoded `false`; `TrueGaze_GetActorGaze()` returns hardcoded zeroes and a hardcoded `0x14` target FormID while returning `true` (reporting success with fabricated data); `TrueGaze_OverrideActorMode()` is an empty body. A third-party integrator cannot distinguish real telemetry from the stub.
>
> ⚠️ **No Papyrus registration exists.** `TrueGaze.psc` declares six `global native` functions but there is no `SKSE::GetPapyrusInterface()->Register(...)` call anywhere, and the declared signatures do not match the exported symbols. Calling these will raise a Papyrus VM error.
>
> ⚠️ `PackageMod.ps1` hardcodes an absolute project path and runs `cmake --build` without a preceding configure step. `.pdb` files are not shipped.
>
> ⚠️ **Licensing conflict:** `LICENSE` states "No license is granted... copying, distribution... prohibited", which is irreconcilable with publishing a "Public Modding SDK".

---

## Phase 8: Cross-Engine Ecosystem Expansion

*Status: **📐 Designed (~5%)** — one declaration header; no implementation.*

> ⚠️ `extern/cross-engine/TrueGazeUE5.h` declares `FTrueGazeEvaluator::EvaluateGaze` with **no implementation**. `TrueGazeUnity.cs` is a scaffold. Recommend reprioritising this phase below Phase 2 — proving the reference implementation in Skyrim first is the stronger path to credibility, and the UE5 market already has MetaHuman eye-aim and ZenBlink.

- [x] **Unreal Engine 5 HCEP Specification**: Scaffolded `extern/cross-engine/TrueGazeUE5.h` for MetaHumans and UE5 AnimGraph/LiveLink.
- [x] **Unity HCEP C# Bridge**: Scaffolded `extern/cross-engine/TrueGazeUnity.cs` P/Invoke bridge for Unity interactive avatars.
- [ ] **Godot 4 Integration**: Open-source C++ GDExtension for independent game developers.

---

# Remediation Roadmap (Audit-Derived)

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

*Status: **📐 Designed***
**Effort:** 1–2 days · **Dependency:** R0

The single highest-leverage phase in this roadmap. Almost every functional gap traces back to a build that silently succeeds without its SDK.

- [ ] Vendor CommonLibSSE-NG into `extern/` as a git submodule
- [ ] Change the CMake guard to `FATAL_ERROR` when `CommonLibSSE::CommonLibSSE` is absent
- [ ] Introduce `TRUEGAZE_STANDALONE` as an explicit opt-in CMake option for unit-test builds
- [ ] Add a `#error` when `<RE/Skyrim.h>` is unavailable and `TRUEGAZE_STANDALONE` is not defined
- [ ] Add a CI workflow: configure → build → test
- [ ] Re-verify the DLL import table contains CommonLibSSE

**Deliverable:** A DLL that actually contains game-facing code. *Every later phase depends on this.*

---

## Phase R2: Make It Move

*Status: **📐 Designed***
**Effort:** 1–2 weeks · **Dependency:** R1
**Priority:** 🔴 **CRITICAL — this is the product**

- [ ] Introduce the `TrueGaze::Core::GazeEngine` singleton (owns config, pipe, actor map)
- [ ] Add `ActorGazeRuntime` per-actor state + map with eviction on cell change
- [ ] Identify and install the real animation hook target via `REL::Relocation`
- [ ] Implement the per-actor tick: `TargetSelector` → kinematics → `BoneController` → **`NiNode` write**
- [ ] Fix the `BoneController` eye-residual allocation bug (`eyeYaw = target − head_total`)
- [ ] Wrap the tick in `ScopedTimer` + `try/catch(...)`

**Deliverable:** **A video of an NPC whose eyes visibly move.** This is the moment the product becomes real.

---

## Phase R3: Make It Correct

*Status: **📐 Designed***
**Effort:** 1 week · **Dependency:** R2

- [ ] Fix the double-buffer race — triple-buffer or seqlock
- [ ] Make `_pipeHandle` atomic, or move all pipe writes to the worker thread
- [ ] Implement the true Main Sequence velocity profile (peak = `V_peak(θ)`, integral = amplitude)
- [ ] Add the eye-lead latency gap (eyes at `t=0`, head at `t≈120 ms`)
- [ ] Implement true Brownian / Ornstein-Uhlenbeck drift with `random_device` + per-actor seeding
- [ ] Plumb `ConfigManager` into the simulation via an immutable `Tuning` struct
- [ ] Call `ConfigManager::Load()` on the real plugin path; gate `Start()` on `connectHcepBridge`
- [ ] Converge or delete the dual init paths in `Main.cpp`
- [ ] Tighten the `MicroJitter` assertion from `1.5×` to `1.0×` the declared bound
- [ ] Add an integration test proving a bone write occurs
- [ ] De-flake `HcepBridgeClientMock` with a condition variable

**Deliverable:** Config that works; motion that is scientifically faithful and race-free.

---

## Phase R4: Make It Ecosystem-Real

*Status: **📐 Designed***
**Effort:** 1–2 weeks · **Dependency:** R3

- [ ] Implement genuine OAR condition registration via SKSE messaging
- [ ] Add `PublishActorState()` writing the OAR cache each tick
- [ ] **Remove the false success log** in `RegisterWithOar()`
- [ ] Implement `PapyrusInterface::RegisterFunctions()` with signatures matching `TrueGaze.psc`
- [ ] Implement real `TrueGazeAPI` bodies that read live state
- [ ] Expose a pipe accessor so `TrueGaze_IsHcepConnected()` can report truthfully
- [ ] Enable and correct `EfmBlinkController::ApplyMorphs`
- [ ] Create `TrueGaze.esp` (or ESL) with MCM globals — or drop MCM Helper in favour of INI
- [ ] Compile Papyrus scripts to `.pex` and include them in the package
- [ ] Include `.pdb` in the package

**Deliverable:** A package that installs, configures, and drives OAR rules — a complete mod.

---

## Phase R5: Make It Credible

*Status: **📐 Designed***
**Effort:** ongoing · **Dependency:** R2+

- [ ] Write `docs/SCIENCE_FOUNDATION.md` with per-claim citations for every constant
- [ ] Write `docs/INTEGRATION_GUIDE.md`; promote `HcepBridgeClientMock.cpp` to a reference client
- [ ] Recruit 3–5 animation-modder partners
- [ ] Publish the OAR conditions as a standalone distribution
- [ ] Produce the mutual-gaze demo (*"When you look in their eyes, they know."*)
- [ ] Add Tobii / Eyeware Beam telemetry adapters
- [ ] Fix `PackageMod.ps1` to use `$PSScriptRoot` and run the configure step
- [ ] Add `.gitignore` negations, or stop committing binaries entirely

**Deliverable:** Market presence, scientific credibility, and a contributor pipeline.

---

## Milestone Projections

| Milestone | Estimated effort | Cumulative |
| :--- | :--- | :--- |
| SDK-linked, game-facing DLL | 1–2 days | ~2 days |
| **"Eyes that move"** — publishable demo | ~2 weeks | **~2 weeks** |
| Correct, race-free, config-driven kinematics | ~1 week | ~3–4 weeks |
| **Shippable 1.0.0** — full ecosystem integration | ~1–2 weeks | **~4–6 weeks** |

> **Note:** the ~2-week figure is significant. The research-intensive work — the biology, the mathematics, the protocol design, the ecosystem strategy — is genuinely complete. What remains is engineering labour, and it is well-scoped above.
