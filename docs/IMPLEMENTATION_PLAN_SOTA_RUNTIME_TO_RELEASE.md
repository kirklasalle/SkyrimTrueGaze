# TrueGaze SOTA Implementation Plan: Runtime to Release

**Date:** September 19, 2026
**Owner:** Kirk LaSalle
**Scope:** Upgrade the working Skyrim/HCEP runtime into an evidence-led, perceptually validated, publishable software product
**Primary references:** `docs/STATUS.md`, `ROADMAP.md`, `docs/USER_GUIDE.md`, `docs/DEVELOPER_GUIDE.md`

## Executive Assessment

TrueGaze is now a working Skyrim AE runtime implementation of the HCEP gaze-execution layer. The current evidence proves:

- SKSE plugin loading.
- Actor update hook invocation.
- Eligible actor ticking.
- Target resolution.
- Live skeleton probing.
- HCEP mode/state telemetry consumption.
- Visual subsystem execution.
- Two diagnostic `NiPointLight` emitters attached with zero anchor or light-creation failures.
- Successful standalone kinematics and HCEP bridge tests.

The product is not yet SOTA or ready for a final public release because the evidence boundary stops short of perceptual acceptance and release hardening. The highest-value work is now validation and productization, not more speculative architecture.

## Current Open Items

1. **Vanilla eye-bone availability:** many vanilla humanoid rigs do not expose separate eye nodes. The head, neck, and spine path can work while eye-node lead cannot. The geometric pupil-origin fallback must be treated as a first-class rig capability, not as an error.
2. **Perceptual correctness:** logs prove execution but cannot prove that a human sees natural gaze, correct timing, stable attention, or acceptable head/eye coupling.
3. **Visible development illustration:** the light path attaches, but the current beam geometry lookup returns `BSResource::ErrorCode::kNotExist`. A visible asset is required for tuning and demonstrations.
4. **OAR registration:** evaluators and state publishing exist, but registration against the OAR API is intentionally not implemented.
5. **Skyrim VR:** VR is not a release-proven target. Head-directed VR and optional eye-tracking input need separate contracts and tests.
6. **HCEP depth:** the runtime consumes HCEP telemetry, but full gaze-vector, head-pose, blink, convergence, confidence, and social-signal fusion needs a formal intent layer and calibration contract.
7. **Release readiness:** packaging, licensing, clean-profile installation, symbols, support policy, and reproducible release evidence remain incomplete.

## SOTA Definition

For this project, SOTA means:

- Evidence-backed behavior rather than compile-time claims.
- A measurable separation between target selection, intent fusion, kinematics, pose application, and visualization.
- Rig-aware degradation with explicit capability reporting.
- Deterministic diagnostics that explain absence of behavior.
- Safe game-thread ownership and bounded runtime cost.
- Versioned HCEP contracts with semantic validation and malformed-input tests.
- Reproducible release artifacts with legal and dependency clarity.
- Human perceptual evaluation in addition to automated tests.

SOTA does not mean adding complexity indiscriminately. Every new abstraction must remove ambiguity, improve observability, or protect a public contract.

## Phase S1: Establish the Evidence Baseline

**Goal:** turn the existing Skyrim run into a reproducible acceptance artifact.

**Status:** 🔨 Implemented and deployed; one fresh Skyrim run is still required to capture the new identity and rig-origin markers.

Completed implementation slices: runtime identity logging, effective INI-path logging, corrected post-run health markers, and rig-origin diagnostics are deployed. The next Skyrim session is the acceptance artifact.

### S1 Implementation

- Add a session identifier and runtime build identifier to `TrueGaze.log`.
- Log the game runtime, SKSE runtime, Address Library version, DLL hash, and effective INI path at startup.
- Add a structured run summary at shutdown or explicit `tgstatus` capture.
- Extend the health script to parse current markers rather than legacy phrases.
- Store one sanitized evidence bundle under `docs/evidence/` or an external release record:
  - log excerpts;
  - DLL hash;
  - `tgstatus` output;
  - runtime and dependency versions;
  - test results;
  - known limitations.

### S1 Acceptance criteria

- A contributor can reproduce the same runtime state from a clean deploy.
- A log can unambiguously distinguish plugin loading, actor ticking, target resolution, skeleton resolution, visual updates, and geometry attachment.
- No health script result contradicts the raw log.

## Phase S2: Build a Rig Capability Matrix

**Goal:** make eye-bone absence an explicit supported capability rather than a silent limitation.

**Status:** 🔨 Implemented and deployed; the current player rig will be the first acceptance sample.

Completed implementation slices: `EyeNode` / `GeometricHeadSocket` / `Unavailable` classification, eye-node-absent and head-anchor-absent counters, skeleton probe logging, and `tgstatus` output.

### Rig classes

1. Vanilla humanoid NPC.
2. Vanilla player in third person.
3. Custom humanoid with XP32/XPMSSE-style eye nodes.
4. Creature with explicit eye nodes.
5. Creature without eye nodes.
6. Skyrim VR player body.

### S2 Implementation

- Add a `RigCapability` record to runtime diagnostics:
  - spine resolved;
  - neck resolved;
  - head resolved;
  - left/right eye resolved;
  - geometric pupil fallback active;
  - skeleton generation or 3D rebuild identity.
- Report capabilities through `tgstatus` and the log once per actor-generation.
- Keep separate counters for `eye nodes absent` and `head anchor absent`.
- Add an explicit visual-origin mode:
  - `EyeNode`
  - `GeometricHeadSocket`
  - `Unavailable`
- Add per-rig test notes to `docs/TEST_SCENARIO.md`.

### S2 Acceptance criteria

- A vanilla humanoid reports `head=yes`, `eyeL/eyeR=NO`, and `origin=GeometricHeadSocket` without being mislabeled broken.
- A custom rig with eye nodes reports `origin=EyeNode`.
- A missing head reports a blocking capability failure.
- No actor can silently appear healthy when the required anchor is absent.

## Phase S3: Perceptual Gaze Acceptance

**Goal:** prove that the runtime output looks and feels correct to a human observer.

### S3 Test design

For each rig class, record:

- Static frontal attention.
- Slow player movement left and right.
- Sudden target movement.
- Dialogue eye contact.
- Crosshair face focus.
- Combat target selection.
- Target leaving the visual cone.
- Close-range micro-jitter.
- LOD transition at 5 m and 15 m.
- Save/load and cell transition.

### S3 Measurements

- Time from target change to visible head response.
- Overshoot and oscillation.
- Maximum yaw/pitch before clamping.
- Eye/head contribution where eye nodes exist.
- Stability while target remains fixed.
- Frame cost at 1, 10, 25, and 50 active actors.
- Human rating for naturalness, attention, jitter, and distraction on a five-point scale.

### S3 Acceptance criteria

- No visible snapping caused by target hysteresis at cone boundaries.
- No head twitch during crosshair latch or target hold.
- No persistent pose after actor reset, save, load, death, ragdoll, or cell transition.
- Head movement is visibly attributable to the selected target.
- Eye-leading claims are made only for rigs where eye nodes or a validated visual proxy exist.

## Phase S4: HCEP Intent and Fusion Layer

**Goal:** upgrade HCEP from mode/state input to a trustworthy human-intent signal.

**Status:** 🔨 Core fusion implemented and deployed 2026-09-19; HCEP-side bridge client implemented and published; joint acceptance pending one live session with both apps running.

Implemented in `PlayerGazeResolver`:

- Full intent context on the signal: head yaw/pitch/roll, convergence, blink bitmask, packet timestamp, and receive time.
- Staleness gate: telemetry older than 500 ms is treated as absent, matching the documented stale-rejection contract.
- Blink suppression: with both eyes closed the gaze vector is a prediction, so fusion is suppressed rather than rotating on closed-eye data.
- Convergence plausibility: focal distance outside 0.3-6.0 m is flagged as implausible in diagnostics.
- `LastIntent()` diagnostic snapshot: validity, confidence, sequence, age, stale/blink flags, head pose, and convergence state.
- `tgstatus` now reports the intent state and WHY fusion is inactive (no telemetry, low confidence, stale, or blink).

### S4 Design

Introduce a `PlayerGazeIntent` structure between the bridge and target selection:

- timestamp and sequence;
- finite, range-checked yaw/pitch;
- confidence;
- convergence validity;
- head pose;
- blink state;
- social triangle state;
- cognitive mode;
- calibration generation;
- source status.

Use explicit source arbitration:

```text
camera/crosshair intent + HCEP intent -> confidence-weighted fusion -> PlayerGazeResolver
```

### S4 Implementation

- Validate protocol version, finite floats, ranges, sequence monotonicity, and reserved fields.
- Reject malformed packets with reason counters.
- Add calibration offsets and coordinate-system documentation.
- Use confidence thresholds and stale-data expiry.
- Treat blink as temporary visual-attention suppression, not as a random gaze command.
- Use convergence only when calibrated and physically plausible.
- Feed fused intent into crosshair focus, mutual gaze hold, target selection, and feedback.
- Add golden packet fixtures and fuzz tests.

### S4 Acceptance criteria

- HCEP gaze yaw/pitch changes player-intent diagnostics in-game.
- Low-confidence or stale telemetry falls back cleanly to camera/crosshair behavior.
- Malformed packets never produce NaNs or impossible target directions.
- Feedback reports the same target and mutual-gaze state that the engine used.

## Phase S5: Visible Development Illustration

**Goal:** provide an unmistakable, tunable, legally distributable in-game visual.

**Status:** ✅ Loading/attachment milestone achieved 2026-09-19; visual tuning deliberately paused in favour of S4/S3.

Runtime evidence from the 2026-09-19 20:59 session:

```text
Visible beam geometry attached from 'meshes\dlc01\effects\fxsoulcairnbeam.nif'
beam geometry    1 attached / 1 attempts
visual updates   550, anchors failed 0, light creates failed 0
```

The full pipeline (resource discovery → BSA extraction → `BSModelDB::Demand` → `NiNode::AttachChild`) is proven end-to-end. The beam attaches but is not yet perceptible at the head anchor; the remaining work is art tuning (scale, opacity, axis, material) on a scene-specific vanilla effect. This is deliberately deferred so S4 (HCEP intent fusion) and S3 (perceptual acceptance) can proceed first. Resuming requires only a tuning change — no rebuild of the loading path.

### S5 Preferred strategy

Use an original TrueGaze asset rather than depending on a guessed Bethesda archive path. A minimal original asset can be:

- a thin emissive beam mesh aligned along local `+Y`;
- a small emissive pupil marker;
- an optional terminus marker;
- an original texture and material;
- packaged under `skyrim/meshes/` and `skyrim/textures/`.

If a vanilla asset is used for local development, resolve it from verified form/model data or a proper BSA extraction tool and never redistribute it.

### S5 Implementation

- Add a `VisualAssetResolver` with explicit states: `Unknown`, `Pending`, `Loaded`, `Missing`, `Invalid`.
- Make the beam resource path configurable without a rebuild. **Implemented:** `VisualTuning::beamModelPath` now drives `BSModelDB::Demand`, so an extracted or original asset can be tested by editing the tuning snapshot rather than recompiling.
- Log resource path, result code, model pointer state, parent type, and attachment result.
- Add an explicit geometry count and `visible geometry candidate` status to `tgstatus`.
- Verify local/world transform conventions with a known test mesh.
- Add configurable beam thickness, opacity, colour, origin offset, target-follow mode, and terminus visibility.
- Add a development-only `tgvdebug` state that forces a static, high-contrast marker at the head anchor to separate asset loading from gaze math.

### S5 Acceptance criteria

- A visible marker appears in third person on a tested vanilla humanoid.
- The marker follows the solved gaze direction.
- Its origin is in the expected pupil/head-socket location.
- It can be disabled and detached without reload.
- Geometry remains stable through save/load, cell transition, and 10-minute sessions.
- The package contains only original assets or no redistributed Bethesda assets.

### S5.1 Research-backed asset workflow

The current web and local research supports a two-track approach:

1. Extract a vanilla candidate locally with a BSA Browser or equivalent archive utility, inspect it with NifSkope, and use it only to diagnose the runtime loading and scene-graph path.
2. Create or commission an original TrueGaze NIF/texture for public distribution.

The external tool roles are distinct:

- BSA Browser/archive utility: discover and extract archive contents.
- NifSkope: inspect NIF hierarchy, axis, shader properties, texture paths, bounds, and controllers.
- CommonLibSSE-NG: load a model through `BSModelDB::Demand` and attach it through `NiNode::AttachChild`.
- xEdit/Creation Kit: only for form-backed Art Objects, spell effects, projectiles, or ESP/ESL workflows.

The detailed research record and support decision tree are maintained in [`docs/TRUEGAZE_SUPPORT_KNOWLEDGE_BASE.md`](TRUEGAZE_SUPPORT_KNOWLEDGE_BASE.md).

### S5.2 Candidate acceptance gate

- [ ] Exact full resource path recorded.
- [ ] Local extracted candidate opens in NifSkope.
- [ ] All referenced textures/material dependencies are identified.
- [ ] `BSModelDB::Demand` returns `kNone`.
- [ ] Returned `NiPointer<NiNode>` is non-null.
- [ ] Parent `NiNode` is valid on the game thread.
- [ ] Geometry attaches and remains attached for one actor-generation.
- [ ] Geometry is visible in third person.
- [ ] Transform axis, origin, scale, and material are tuned.
- [ ] Redistribution status is documented before packaging.

## Phase S6: OAR Integration

**Goal:** make the existing condition evaluators available to OAR through a verified public contract.

### S6 Implementation

- Pin the OAR API version and obtain its official registration interface from a known-good SDK/example.
- Add a narrow adapter module rather than coupling OAR headers throughout the engine.
- Register:
  - mode condition;
  - mutual-gaze threshold;
  - gaze-region condition.
- Report registration state and API version in `tgstatus`.
- Keep evaluators usable in standalone mode for tests.
- Add a mock registration test for the adapter and an in-game OAR acceptance scenario.

### S6 Acceptance criteria

- Registration succeeds only when OAR is present and the API accepts the callbacks.
- No false success log is possible.
- OAR rules change only when published live state changes.
- OAR absence does not affect the core gaze engine.

## Phase S7: Skyrim VR Contract

**Goal:** define and validate VR instead of treating VR as a badge claim.

### S7 Releases

1. **VR head-directed pilot:** HMD/head pose drives player attention with comfort limits.
2. **Optional eye-tracking adapter:** a separate provider supplies calibrated eye vectors.
3. **HCEP fusion:** eye tracker, HMD, crosshair, and HCEP signals are fused with confidence.

### S7 Implementation

- Pin Skyrim VR runtime, SKSE VR, and Address Library versions.
- Document whether the build supports head pose only or eye tracking.
- Add provider interfaces so VR hardware is optional.
- Add calibration state and recentering.
- Add comfort clamps, smoothing, and fallback behavior.
- Run a 30-minute VR stability test with logging and frame-time capture.

### S7 Acceptance criteria

- Documentation never calls head direction eye tracking.
- HMD-only fallback works without an eye tracker.
- Eye-tracking providers can disconnect without crashes or frozen gaze.
- Frame time remains within the published budget.

## Phase S8: Release Engineering and Legal Readiness

**Goal:** make publication repeatable and defensible.

### S8 Implementation

- Resolve the proprietary license versus public SDK/mod distribution contradiction.
- Define whether the release is:
  - private technical preview;
  - source-available development build;
  - public mod distribution;
  - public SDK.
- Make `PackageMod.ps1` the one release pipeline:
  - configure;
  - build;
  - test;
  - deploy to a staging layout;
  - verify hashes;
  - audit contents;
  - generate archive and manifest.
- Include PDBs only in a developer symbols package, not necessarily the player archive.
- Generate a machine-readable release manifest with versions, hashes, supported runtimes, and known limitations.
- Add clean-profile installation testing for manual, MO2, and Vortex layouts.
- Add rollback instructions and a support-report template.
- Replace stale health-script markers and eliminate false plugin-load failures.

### S8 Acceptance criteria

- Clean checkout builds from documented prerequisites.
- Release archive contains only intended files.
- License and redistribution terms are internally consistent.
- A fresh profile installs and launches without manual file surgery beyond documented dependencies.
- Every public feature claim links to an acceptance artifact.

## S9: Quality and Observability Upgrade

**Goal:** make failures diagnosable without source-level investigation.

### S9 Implementation

- Add event counters for every major stage: hook, eligibility, LOD, target, skeleton, pose, visual, bridge, OAR.
- Add reason-coded rejection counters.
- Add per-session correlation IDs.
- Add bounded event sampling rather than per-frame log floods.
- Add a JSON or CSV diagnostic export through the configurator bridge.
- Add automated log parsing for startup, runtime, visual, and shutdown acceptance.
- Add stress tests for bridge start/stop, save/load reset, actor eviction, and scene-graph detach.
- Replace detached worker shutdown with an ownership-safe lifecycle design.

### S9 Acceptance criteria

- `tgstatus` answers whether the system is active and why a visual or target is absent.
- The health script produces no known false positives.
- Shutdown is race-free under repeated start/stop tests.
- A support report can be triaged from logs without a debugger.

## Delivery Order

The recommended order is:

1. S1 evidence baseline.
2. S2 rig capability matrix.
3. S3 perceptual acceptance.
4. S5 visible developer illustration.
5. S4 full HCEP intent fusion.
6. S9 observability and lifecycle hardening.
7. S6 OAR registration.
8. S7 VR pilot and adapter contract.
9. S8 packaging, licensing, and publication.

This order keeps the core Skyrim experience measurable before expanding ecosystem surface area.

## Release Tiers

### Technical preview

Requires S1 through the current runtime baseline, clear limitations, and no claim of visible beam geometry or VR eye tracking.

### Development release

Requires S1-S5, perceptual evidence on at least one vanilla humanoid and one custom/alternate rig, stable visual toggles, and a clean staging package.

### Public 1.0

Requires S1-S9 acceptance, resolved licensing, documented supported runtimes, clean-profile installation, no known shutdown or bridge lifecycle defects, and public artifacts for every headline claim.

## Definition of Done

TrueGaze reaches the SOTA target when a new contributor can:

1. Build the plugin from a clean checkout.
2. Deploy it to a supported Skyrim runtime.
3. Confirm the exact DLL and configuration in logs.
4. Observe and measure actor attention on documented rig classes.
5. Enable a visible development illustration and tune it safely.
6. Connect HCEP telemetry with confidence-aware fallback.
7. Validate OAR and VR only when those adapters are present.
8. Generate a release archive whose contents, license, hashes, and support claims are internally consistent.
