# SkyrimTrueGaze — Full Code, Documentation, and Project Audit

**Audit date:** 2026-09-19  
**Audited tree:** `D:\Projects\SkyrimTrueGaze` working tree, including uncommitted changes  
**Auditor:** GitHub Copilot  
**Scope:** C++ runtime, HCEP bridge, Skyrim integration, VR readiness, visuals, configuration, tooling, tests, packaging, governance, documentation, and product usefulness.

> **Executive truth:** The project has made a substantial and credible transition from a non-functional SDK-less skeleton to a real SDK-linked, configuration-driven gaze runtime. Controlled Skyrim AE sessions now prove plugin loading, actor-update hooks, eligible ticks, target resolution, skeleton probing, HCEP mode/state consumption, and diagnostic light attachment. The remaining product gap is not basic runtime activation: it is perceptual acceptance, full HCEP intent fusion, visible illustration geometry, rig coverage, OAR/VR validation, and release hardening. The highest-value work is now evidence-led productization rather than more aspirational architecture prose.

---

## 1. Audit method and evidence boundary

The audit used direct inspection of the current working tree, symbol/call-flow tracing by source reading, repository-wide searches, the existing health checker, the existing standalone executables, and the CMake build integration.

### Evidence captured on 2026-09-19

| Check | Result | Meaning |
| --- | ---: | --- |
| `get_errors` over project-owned `src`, `tests`, and `scripts` | No editor diagnostics | No currently surfaced language-service errors; this is not runtime proof. |
| `KinematicsTests.exe` | Exit 0; 11 suites passed | Mathematical/core regression evidence is good. |
| `HcepBridgeClientMock.exe` | Exit 0 | Local named-pipe round-trip and packet exchange pass in the mock scenario. |
| `Test-TrueGazeHealth.ps1 -Quiet` | 15 passed, 0 warnings, 0 failures | Current detected game/SKSE/Address Library/binary/deployment checks are clear. |
| CMake target build `TrueGaze` | **Environment-sensitive** | The repository Release/deploy pipeline has produced matching live DLLs; one VS Code/CMake configure attempt failed because `directxtk` package configuration was missing. Clean release reproducibility remains a release gate. |
| Git worktree | Many modified and untracked files | This report assesses the current tree and does not assume the working tree is release-ready or committed. |

The CMake failure is at `extern/CommonLibSSE-NG/CMakeLists.txt:53`: `directxtkConfig.cmake`/`directxtk-config.cmake` was not found. This is an environment/dependency reproducibility failure, not evidence that the source is intrinsically uncompilable. It must nevertheless be treated as a release blocker until a clean configure/build succeeds.

### Rating scale

- **P0 — release/product blocker:** prevents a trustworthy in-game or distributable result.
- **P1 — high priority:** materially limits the promised HCEP/VR experience or creates serious runtime risk.
- **P2 — important hardening:** should be completed before a public 1.0.
- **P3 — strategic/future:** valuable after the Skyrim reference experience is proven.

---

## 2. Current architecture assessment

### What is real and strong

1. **Build-mode separation is correct in principle.** `CMakeLists.txt` makes CommonLibSSE-NG mandatory for the plugin and provides an explicit standalone mode for platform-agnostic tests. This prevents the former “successful build that silently does nothing” failure mode.
2. **The runtime has a real frame path.** `src/Engine/AnimationHook.cpp` installs vtable hooks for `Actor`, `Character`, and `PlayerCharacter`, calls the original update, then performs TrueGaze work on the game thread.
3. **Pose composition is additive and withdrawable.** `EyeAimConstraint` and the per-actor state attempt to preserve the animated pose, compose gaze, and withdraw it before the next actor update or on reset/eviction/save.
4. **The kinematics core is meaningfully tested.** Main Sequence profile, VOR, OU drift, social triangle, residual eye allocation, LOD, blink state, and packet layout/CRC are covered by the existing 11-suite executable.
5. **The HCEP transport has serious engineering improvements.** The current server uses message-mode overlapped I/O, CRC verification, stale telemetry rejection, user-scoped pipe security when descriptor creation succeeds, triple-buffer publication, and an SPSC outbound ring.
6. **Truthful degradation is now a project strength.** OAR registration explicitly warns that it is not implemented rather than claiming success. Visual geometry mode warns when assets are absent. The health tool distinguishes expected incomplete features from failed prerequisites.
7. **The vanilla-UI decision is coherent.** Removing SkyUI/MCM/Papyrus/ESP dependencies reduces installation surface and keeps configuration in the HTML editor/INI/console path.

### What is not yet proven

- No evidence in this audit proves a live Skyrim session loaded the current DLL, installed the current hooks, resolved skeleton nodes, applied visible transforms, rendered a visual emitter, or survived extended play.
- No evidence proves Skyrim VR HMD pose integration. `src/Engine/VrController.cpp` currently derives an approximate pose from `PlayerCharacter` position/angles; it does not demonstrate an OpenVR/OpenXR eye-tracking feed.
- No evidence proves OAR custom condition registration. `src/Integrations/OarConditions.cpp` intentionally returns false.
- No evidence proves the current release build can be recreated from a clean checkout in this environment; CMake configure currently stops at missing `directxtk`.

---

## 3. Findings — P0 and P1

### P0-1 — The headline feature is still in-engine unverified

**Evidence:** `README.md`, `docs/STATUS.md`, and `docs/TEST_SCENARIO.md` acknowledge that no current run has established visible NPC eye movement. The repository health script can report “Clear to launch,” but that is pre-flight readiness, not behavioral evidence.

**Impact:** The project can correctly compile and pass unit tests while still failing on vtable ABI, node names, transform conventions, actor update order, morph/scene-graph ownership, or runtime version behavior. This is the single largest gap between engineering effort and user impact.

**Required gate:** Run the staged test scenario in a real Skyrim save and capture:

1. plugin load and version/address-library evidence;
2. hook-installed evidence;
3. skeleton probe results for vanilla humanoid, custom humanoid, creature, and player;
4. visible gaze movement with visuals disabled;
5. visual emitter evidence with visuals enabled;
6. first-person, third-person, dialogue, combat, cell transition, save/load, death/ragdoll, and long-session stability;
7. measured frame cost and crash-free duration.

Do not promote any runtime feature to “complete” until the corresponding artifact (log, capture, metrics, or test result) is stored and linked.

### P0-2 — HCEP human gaze is not yet driving the promised interaction

**Evidence:** `src/Engine/GazeEngine.cpp::ComputeDeflection` calls `TryGetLatestTelemetry` and uses `hcepPacket.hcepMode` to overwrite `state.hcepMode`. The current production path does not use the incoming `gazePitch`, `gazeYaw`, `gazeConvergence`, `gazeConfidence`, `headPitch`, `headYaw`, `headRoll`, `blinkBitmask`, `socialTriangle`, `emotionalValence`, `cognitiveState`, `trackedPersonId`, or inbound `mutualGazeHoldSec` to solve the player's gaze or establish mutual eye contact.

**Impact:** “Connected HCEP” currently means “the desktop mode can influence NPC mode,” not “NPCs know where the human is looking.” The strongest HCEP promise—reciprocal, human-feeling attention—is therefore not delivered by the live engine.

**Upgrade:** Introduce an explicit `PlayerIntent/GazeSignal` layer between IPC and targeting:

- validate and confidence-gate gaze/head values;
- transform sensor coordinates into Skyrim camera/HMD coordinates;
- apply timestamp/sequence monotonicity and calibration offsets;
- select whether camera/crosshair, HCEP gaze, or a confidence-weighted fusion is authoritative;
- use blink as an attention/occlusion signal rather than blindly rotating;
- use convergence only when calibrated and physically plausible;
- maintain a short-lived fixation/attention state with hysteresis;
- feed that state into `PlayerGazeResolver`, `TargetSelector`, `mutualGazeHoldSec`, and outbound feedback.

This is the central HCEP engineering milestone.

### P1-1 — HCEP packet validation is incomplete

**Evidence:** `NamedPipeServer.cpp` checks magic, CRC, packet size, and staleness, but does not visibly reject unsupported `version`, invalid angle/range values, NaN/ infinity floats, invalid confidence, invalid mode/state ranges, reserved-byte policy violations, or sequence rollback/wrap semantics.

**Impact:** A malformed or compromised local client can inject impossible gaze and state values. This can cause NaNs, extreme target behavior, or unstable animation. CRC provides integrity against accidental corruption, not authenticity or semantic validity.

**Upgrade:** Add a `ValidateTelemetryPacket()` function with protocol-version negotiation, finite/range checks, sequence policy, confidence policy, and explicit rejection counters. Add fuzz/property tests and malformed-frame tests to the bridge harness.

### P1-2 — The “triple buffer” design needs a formal memory-model proof/test

**Evidence:** `NamedPipeServer.hpp/.cpp` publish a non-atomic `TelemetrySlot` payload and use an epoch/index retry scheme. The approach may be workable for the one-producer/one-consumer case, but the comments call it standard lock-free triple buffering while the code has no dedicated stress test, sanitizer-backed proof, or documented happens-before argument for all payload fields.

**Impact:** A rare torn snapshot in a game thread is difficult to diagnose and can undermine the safety claim.

**Upgrade:** Either document and stress-test the exact algorithm under a producer loop faster than the consumer, or use a well-known seqlock/immutable atomic shared-pointer pattern with explicit C++ memory-order reasoning. Add a test that checks packet self-consistency across millions of publications and runs under ThreadSanitizer where available.

### P1-3 — Stop/shutdown can trade a race for a detached worker

**Evidence:** `NamedPipeServer::Stop()` waits 250 ms and detaches if the worker has not exited. It also closes shutdown/overlapped resources after the detach path. The worker still references the owning object and its events during `WorkerLoop`.

**Impact:** Detaching a thread that retains `this` during plugin/game shutdown risks use-after-free or access to closed event state. The intent is to avoid loader-lock deadlock, but the fallback does not establish object lifetime safety.

**Upgrade:** Make worker lifetime explicit: signal cancellation, cancel I/O, join from a safe lifecycle point, and avoid destroying the server until the worker is proven stopped. If process termination makes joining impossible, use a process-lifetime-owned state block rather than detaching a thread that references a destructible plugin object. Add repeated start/stop and shutdown-race tests.

### P1-4 — Visual effect lifecycle has not been proven against Skyrim scene-graph ownership

**Evidence:** `src/Visuals/VisualEffectsManager.cpp` attaches `NiPointLight` nodes and detaches them during reset/eviction/disable, but this is entirely untested in-engine. It also uses a bounded emitter map (`kMaxEmitterActors=64`), meaning visual output can silently disappear for additional actors.

**Impact:** Wrong attach/detach semantics can crash, leak, or leave lights behind. The cap can make a user believe gaze is broken in a crowded scene.

**Upgrade:** Verify scene-graph ownership and thread affinity in-game; log emitter-capacity events with actor IDs and make the policy configurable or use distance/priority eviction. Add cell unload/rebuild tests and a long-session leak check.

### P1-5 — VR is a product target in documentation but not an implemented HMD gaze path

**Evidence:** `CMakeLists.txt` exposes `BUILD_SKYRIM_VR`, while `VrController.cpp` reports VR through `REL::Module::IsVR()` and derives position/yaw/pitch from the player actor. `GetVrGazeRay()` is a spherical ray from those angles; it is not demonstrated as HMD or eye-tracker data. `BUILD_SKYRIM_VR` defaults OFF.

**Impact:** Skyrim VR users are currently promised more than the code proves. VR is also the strongest experiential opportunity, so an approximate flat-screen pose is not enough for the flagship demo.

**Upgrade:** Define a specific VR support contract: Skyrim VR runtime version, address-library/runtime compatibility, HMD pose source, eye-gaze source (if present), fallback when only head pose exists, comfort limits, and calibration. First ship a head-directed VR pilot, then add eye tracking as an optional adapter. Do not call it eye-tracked VR until measured data reaches the runtime.

---

## 4. Findings — P2 engineering and product hardening

### P2-1 — Performance profiler is design-only and currently loses accumulation semantics

`src/Engine/PerformanceProfiler.hpp` stores each timer result rather than accumulating it, and no production call site was established in the inspected path. The documented 150 µs budget is therefore not a verified performance contract. Instrument the actual actor tick and frame boundary, publish actor count, percentile/max timings, dropped telemetry, and emitter count, then measure populated Whiterun/solitude/combat scenarios at 60/90/120 Hz.

### P2-2 — Outbound feedback contains placeholder fields

`GazeEngine.cpp::PublishState` sets `relationshipRank = 0` and `gameFrameNumber = 0`; `mutualGazeAngle` is populated from `state.lastYawDeg`, which is not necessarily the angle between the NPC gaze ray and the player's gaze ray described by the protocol. This is a truthful transport test but not yet truthful semantic feedback. Either implement the fields or mark them unavailable in the protocol/version and documentation.

### P2-3 — Mutual gaze threshold is not the same as mutual gaze proof

The current crosshair sweet spot is a useful deterministic fallback, but it establishes player attention from Skyrim's crosshair and checks the actor's target selection. It does not yet combine human HCEP gaze, NPC eye direction, head/HMD pose, occlusion, confidence, and angular divergence as described in the PRD. Rename/document it as crosshair-assisted mutual gaze until the HCEP fusion path exists.

### P2-4 — Actor-update hook scope and transform order require in-game confirmation

The code patches three vtables at slot `0xAD`, withdraws before original update, and applies after it. This is a defensible design, but the evidence is SDK/source-based rather than runtime-based. Verify all three concrete vtables, original-call stability, animation graph ordering, third-person rendering, and first-person player behavior on each supported runtime. Add a load-time diagnostic that identifies the selected runtime and hook addresses without claiming success until the write/return path is observed.

### P2-5 — Bone discovery remains a silent functional dependency despite diagnostics

The skeleton probe is an excellent diagnostic improvement, but the implementation still depends on string candidates and fuzzy matching. Vanilla eye nodes may be absent because eye motion can be FaceGen/morph-driven. Build a rig capability matrix and make the fallback explicit: head-only, morph-only, eye-node, or unavailable. Do not interpret “engine ticked” as “eyes moved.”

### P2-6 — OAR integration is not a release feature yet

The evaluator cache is useful and published, but `RegisterWithOar()` intentionally returns false. PRD/README/older walkthrough material still describe OAR as a working integration in places. Keep the honest warning, mark all OAR claims as unavailable until an API contract is pinned and an in-game rule fires, and add a separate adapter test/package only after verification.

### P2-7 — Public API thread/lifecycle contract is unspecified

`TrueGazeAPI.cpp` reads and modifies engine actor state. The API does not document that calls must occur on the game thread, what happens during reset/cell unload, or whether `FindActor` remains stable. Add a versioned API contract, thread assertions/queueing, null/stale actor behavior, and a compatibility test DLL/client.

### P2-8 — Package tooling is not portable or fully reproducible

`scripts/PackageMod.ps1` hardcodes `D:\Projects\SkyrimTrueGaze` and builds without configuring first. It also does not establish a clean release manifest, symbol policy, or artifact hash report. Convert paths to `$PSScriptRoot`, require/select a configure preset, verify the output binary and package contents, emit SHA-256 and version metadata, and fail if the package contains stale/forbidden artifacts.

### P2-9 — Documentation has material drift

Concrete examples found:

- `README.md` advertises C++20 while `CMakeLists.txt` and `CMakePresets.json` require C++23.
- `README.md` still describes a 637 KB DLL/status snapshot while current repository notes and files describe later visual/console work and a larger binary.
- `docs/STATUS.md` is dated September 12 while current changes are dated September 18/19; some “SKSE/Address Library not installed” statements conflict with the current health check’s 15/0/0 result.
- `ROADMAP.md` retains historical claims such as “not consumed by production code,” “VR detection always false,” and “ApplyMorphs commented out” in sections that are not consistently marked as superseded by later work.
- `docs/HCEP_BRIDGE_SPEC.md` describes `PIPE_NOWAIT`, `std::jthread`, `<0.35 ms` latency, and `<10 ns` reads; current code uses `PIPE_WAIT`, `std::thread`, and no benchmark evidence was found for those numeric claims.
- PRD acceptance criteria label several behaviors as passed when only unit tests or source implementation exist; the project’s own four-state vocabulary says “in-engine verified” is required for runtime completion.

Create one current source-of-truth status document and make older audits explicitly historical. Every performance/science claim should carry an evidence type: citation, unit test, benchmark, or in-engine capture.

### P2-10 — License, SDK, and distribution posture need a deliberate decision

`LICENSE` is proprietary and prohibits copying/distribution while the project describes a public modding SDK and packaging for Nexus. That may be intentional, but it is commercially and operationally ambiguous. State whether the release is private evaluation, licensed distribution, or public mod distribution with a separate SDK license. Remove “public SDK” language until a grant exists, or publish a scoped SDK license.

### P2-11 — CI coverage is incomplete even if GitHub Actions is unavailable

The only visible workflow is charter integrity. There is no active build/test workflow in the repository. If private-repository Actions minutes remain unavailable, add a documented local/portable CI path (for example, a self-hosted runner or reproducible PowerShell validation script), and ensure a future CI workflow covers standalone tests, plugin configure/build, package audit, charter verification, and documentation consistency.

### P2-12 — Security has improved but is not complete

The current pipe uses a user-scoped security descriptor when descriptor creation succeeds, but intentionally falls back to the default DACL and logs a warning. The payload is plaintext and CRC is not authentication. The license/governance documents are honest about several gaps. For a local single-user game mod this may be acceptable; for HCEP as a reusable platform bridge it is not sufficient. Make secure descriptor failure a configurable fail-closed option, minimize/zero `trackedPersonId` by default, add semantic validation and connection identity/audit events, and document the threat model rather than implying regulatory compliance.

---

## 5. HCEP usefulness and human-like interaction assessment

The project’s most compelling idea is not “rotate NPC bones.” It is a reciprocal attention loop:

1. the player looks, fixates, blinks, shifts attention, or disengages;
2. the system estimates confidence and intent without overclaiming emotion;
3. Skyrim selects a socially meaningful target;
4. the NPC responds with eye lead, head follow, fixation variation, glance aversion, blink timing, and context-sensitive body/voice/animation cues;
5. the game returns interpretable feedback to HCEP;
6. the loop remains stable, calibrated, private, and believable.

The current project implements much of step 4 for autonomous NPC gaze and a crosshair approximation of step 1, but the connected HCEP path does not yet complete steps 1–5. The next design should therefore separate:

- **Perception:** raw sensor data and confidence;
- **Calibration:** coordinate frames, user/session calibration, timing;
- **Attention inference:** fixation, saccade, blink, disengagement, target confidence;
- **Social interpretation:** conversation/combat/context state, not unsupported mind-reading;
- **Animation response:** gaze, posture, face, voice/animation triggers;
- **Evaluation:** whether the response feels natural to players;
- **Privacy/safety:** opt-in, minimization, local-only processing, clear state.

### High-impact Skyrim experience sequence

Prioritize one demonstrable scenario rather than broad claims:

1. **Conversation:** player looks at an NPC’s eyes; NPC acquires the player’s face, blinks naturally, and holds contact with small fixation variation.
2. **Disengagement:** player looks away; NPC releases contact with a short, non-snapping aversion and returns to environmental attention.
3. **Social triangle:** during dialogue, NPC alternates eye/eye/mouth regions with dwell-time variation, not a rigid loop.
4. **Emotionally neutral baseline:** no inferred emotion is shown unless a game context or explicit HCEP signal authorizes it.
5. **Combat:** gaze prioritizes threat and does not create distracting social behavior.
6. **VR:** head pose and player viewpoint are stable; gaze response respects comfort and avoids visual overdrive.

Measure this with blinded player comparisons: baseline Skyrim, autonomous TrueGaze, and connected HCEP. Record perceived naturalness, eye-contact appropriateness, distraction, nausea/comfort in VR, frame time, and failure rate.

### Robotics and other games

Do not begin with engine adapters. First extract a portable, engine-neutral contract:

- `GazeObservation` (direction, origin, timestamp, confidence, blink, calibration ID);
- `AttentionState` (fixation target, fixation age, saccade phase, disengaged, confidence);
- `SocialContext` (interaction phase, target salience, safety/consent policy);
- `GazeResponse` (eye/head/body targets, timing, limits, priority, explainability);
- deterministic simulation and replay files.

Then implement a Skyrim adapter as the reference renderer and a robotics adapter that maps the same response into actuator limits. This gives HCEP a credible platform story without pretending that a Skyrim bone hook is already a robotics product.

---

## 6. Prioritized upgrade program

### Gate 0 — Reproducible evidence (immediate)

- Fix the current `directxtk`/vcpkg configure failure and record the exact dependency bootstrap.
- Update `PackageMod.ps1` to be workspace-relative and configure-first.
- Refresh `docs/STATUS.md`, `ROADMAP.md`, README badges/version/standard, and HCEP protocol claims.
- Add a machine-readable audit/status manifest with `designed`, `implemented`, `unit-verified`, `in-engine-verified` states.

### Gate 1 — First in-game proof (highest value)

- Build the current DLL from the current tree.
- Run load-only, skeleton probe, visible gaze, visual diagnostics, dialogue, combat, save/load, cell transition, and long-session tests.
- Store logs, screenshots/video, frame metrics, and exact binary hashes.
- Fix the first runtime defect found before adding new features.

### Gate 2 — Make HCEP actually perceptual

- Add validated signal ingestion and coordinate calibration.
- Fuse HCEP gaze with crosshair/camera/HMD using confidence and hysteresis.
- Consume player gaze, head pose, blink, convergence only when valid, and attention timing.
- Publish semantically correct mutual-gaze and feedback fields.
- Add replay-driven integration tests independent of live hardware.

### Gate 3 — Make the response feel human

- Replace rigid social-triangle cycling with fixation dwell distributions, salience, interruption, and recovery.
- Implement explicit eye-lead latency and realistic pursuit/hold/aversion transitions.
- Tune per-rig capability profiles and validate vanilla/custom/VR skeletons.
- Add context policies for dialogue, combat, stealth, crowd scenes, and accessibility.

### Gate 4 — Make VR credible

- Define supported Skyrim VR runtime and test matrix.
- Implement/verify HMD pose; make eye tracking an adapter, not an assumption.
- Add VR comfort limits, calibration UI/workflow, and 90/120 Hz frame-budget evidence.
- Produce a short VR demo showing mutual attention, disengagement, and stable recovery.

### Gate 5 — Ecosystem and platform

- Verify OAR registration against a pinned API/version or remove it from the 1.0 promise.
- Version the C API and publish an integration guide only after thread/lifecycle rules are tested.
- Extract engine-neutral HCEP observation/attention/response contracts.
- Prototype one additional adapter only after Skyrim and VR acceptance gates pass.

---

## 7. Recommended release criteria

A public 1.0 should require all of the following:

- clean configure/build/package from documented prerequisites;
- standalone tests and bridge tests pass;
- plugin load verified on every advertised runtime;
- visible gaze verified on vanilla humanoid and at least one custom/VR-relevant rig;
- no crash/hang in a defined long-session scenario;
- measured frame cost within a stated budget with a specified actor population;
- HCEP connected mode demonstrably changes player/NPC interaction using real or replayed gaze data;
- protocol validation, stale/failure behavior, and privacy settings tested;
- documentation has no known contradictions and no “implemented” labels for source-only features;
- license and distribution terms match the intended audience.

---

## 8. Final assessment

**Engineering:** strong core progress, good corrective culture, and unusually honest diagnostics.  
**Runtime readiness:** not release-ready until the current build is recreated and observed in-game.  
**HCEP readiness:** transport-ready and mode-aware, but not yet a complete human-gaze interaction system.  
**Skyrim usefulness:** high potential; the crosshair-driven reciprocal gaze and autonomous kinematics are the right foundation, but player-perceived naturalness must be measured rather than asserted.  
**Skyrim VR potential:** exceptionally high demonstrator value, currently a validation/integration program rather than a finished feature.  
**Platform/robotics potential:** real strategic opportunity if the team extracts a sensor- and engine-neutral attention contract after proving the Skyrim reference implementation.

The project should now optimize for **evidence density and felt interaction**: one verified, repeatable Skyrim conversation in which a player looks, an NPC notices, responds naturally, and releases attention gracefully is worth more than another unverified subsystem or architectural claim.
