# SkyrimTrueGaze — Full World-Class Codebase Audit

**Audit date:** 2026-09-26
**Auditor:** GitHub Copilot (Z.ai: GLM 5.3 Flash)
**Audited tree:** `D:\Projects\SkyrimTrueGaze` working tree (HEAD `cdbb435`, 31 dirty files)
**Scope:** C++ runtime (`src/`, `include/`, `tests/`), build system, tooling, configuration surface, packaging, documentation, governance, and product posture.
**Supersedes:** `docs/AUDIT_REPORT_2026-09-19.md` (kept as historical record). Prior audits: 2026-09-11, 2026-09-15, 2026-09-17.

---

## 0. Executive Summary

TrueGaze has completed a genuine and rare transition: from a **non-functional skeleton** (2026-09-11: SDK absent, no bone ever written, all APIs stubs) to a **real, in-engine-verified biomechanical gaze engine** (2026-09-26: eye-to-eye targeting, scene defer, SEH fault tolerance, calm/combat speed model — all field-verified by Kirk LaSalle across nine Helgen-cart test rounds, verdict "things look good").

The engineering culture is now a strength: honest four-state status vocabulary, regression tests pinning every past defect, self-diagnosing logs, and guards that refuse to write on unverified assumptions (console-table reclaim, vtable slot verification).

**This audit's verdict:** the *product* is ahead of the *engineering infrastructure*. The remaining risk is concentrated in five areas:

1. **Engineering infrastructure debt** — no CI, no formatter/linter config, no static analysis, no sanitizers, no fuzzing of the IPC boundary, tests use raw `assert` with no runner.
2. **Concurrency claims without proof** — the triple-buffer and SPSC ring are well-commented but have no TSan/stress evidence; `Stop()` still detaches a worker that references `this`.
3. **Documentation drift at scale** — 14 concrete contradictions found between ROADMAP, STATUS, README, PRD, TEST_SCENARIO, and the code (§8). The project's own truth-first charter is violated by its own docs.
4. **Hot-path performance unmeasured** — the 150 µs frame budget is asserted, never measured; `PerformanceProfiler` is dead code with a known accumulation bug; `TargetSelector::ResolveTarget` does full process-list scans per actor per frame (O(N²) in crowd scenes).
5. **Legal/identity ambiguity** — LICENSE is GPLv3, README badge says Proprietary, distribution posture undecided; the retired crash-causing `GazeBeam.nif` still ships in the tree.

**Overall grade: B+ engineering on an A- product.** The gap to A is closed by the implementation plan accompanying this audit (`docs/IMPLEMENTATION_PLAN_2026-09-26_ENGINEERING_EXCELLENCE.md`).

---

## 1. Codebase Inventory (measured 2026-09-26)

| File | Lines | Role |
| --- | ---: | --- |
| `src/Engine/GazeEngine.cpp` | 1,554 | Core orchestrator: per-actor tick, deflection solve, skeleton apply, publishing |
| `src/Engine/TargetSelector.cpp` | 889 | Salience resolution: crosshair, dialogue, combat, social scan, defer |
| `src/Visuals/VisualEffectsManager.cpp` | 928 | NiPointLight + NIF beam + HCEP panel lifecycle |
| `src/Integrations/ConsoleCommands.cpp` | 544 | stg* console commands via slot-reclaim |
| `src/Bridge/NamedPipeServer.cpp` | 455 | Overlapped duplex pipe, triple buffer, SPSC ring |
| `src/Engine/AnimationHook.cpp` | 425 | VTable hooks (Actor/Character/PlayerCharacter), SEH frame guard |
| `src/Engine/ConfigManager.cpp` | 425 | INI load/sanitise/save (all keys) |
| `src/Engine/PlayerGazeResolver.cpp` | 329 | Crosshair pick + HCEP fusion (S4) |
| `src/Main.cpp` | 299 | SKSE load, logging ownership, message handler |
| `src/Engine/CharacterProfile.hpp` | 250 | Temperament → gaze profile classification |
| `src/Engine/EyeAimConstraint.cpp` | 237 | Additive bone rotation compose/withdraw |
| `src/Kinematics/SocialTriangle.hpp` | 475 | Scanpath: triangle, extended, CGA, weighted |
| `src/Kinematics/SaccadeGenerator.hpp` | 219 | Main Sequence saccades with LUT |
| `src/Kinematics/MicroJitter.hpp` | 124 | Ornstein-Uhlenbeck drift |
| `src/Kinematics/VorCoordinator.hpp` | 126 | VOR decoupling, SmoothDamp head curve |
| `src/Engine/BoneController.hpp` | 174 | Strain distribution + CGA strain |
| `src/Engine/TrueGazeAPI.cpp` | 84 | Public C API |
| `src/Integrations/OarConditions.cpp` | 181 | Condition cache + OAR messaging registration |
| `src/Integrations/EfmBlinkController.cpp` | 61 | Blink + FaceGen Look* morphs |
| `src/Engine/VrController.cpp` | 52 | VR detection + approximate HMD pose |
| `tests/KinematicsTests.cpp` | 673 | 12 suites, raw assert |
| `tests/HcepBridgeClientMock.cpp` | 138 | Pipe round-trip harness |
| **Total project-owned C++** | **~7,900** | |

**Assessment:** the size is appropriate for the feature set. The two hotspots — `GazeEngine.cpp` (1,554) and `TargetSelector.cpp` (889) — carry too many responsibilities each and are the natural first refactor targets (§4.1).

---

## 2. Architecture Assessment

### 2.1 What is strong

1. **Clean layering.** `Kinematics/` is SDK-free and unit-testable; `Engine/` owns orchestration; `Bridge/` owns IPC; `Integrations/` owns game-facing adapters; `Visuals/` is a pure consumer of solved state. This is textbook and it works.
2. **Immutable tuning snapshots.** `GazeTuning`/`VisualTuning` are rebuilt on refresh and passed by const-ref — no torn config reads, one path from INI key to mathematics.
3. **Additive-only pose contract.** `EyeAimConstraint` caches pristine rotations, composes, withdraws — idempotent per frame, withdrawable on eviction/save/reset. The frozen-pose leak class is closed.
4. **Honest degradation everywhere.** Missing INI → compiled defaults (logged); missing bones → loud probe log + rig-capability counters; missing beam asset → light fallback + bounded retry; OAR absent → cache-only mode. Nothing fails silently.
5. **SEH fault tolerance (R13/G3).** The `__try/__except` frame guard with rate-limited fault reporting is the single most valuable robustness feature in the codebase — every historical crash class is now survivable and localisable.
6. **Logging ownership (R13/G4).** Single sink, SKSE `InitInfo{log=false}` — the invisible-crash class is closed permanently.
7. **Self-diagnosing runtime.** `stgstatus` answers "is it on, why not, what did the rig report, what did the defer contract do" without a debugger.

### 2.2 Architectural risks

| # | Risk | Evidence | Severity |
| --- | --- | --- | --- |
| A-1 | **God-object drift in `GazeEngine`** | `ComputeDeflection` alone spans ~700 lines and handles: HCEP ingestion, target resolution, defer glance, mode arbitration, mutual gaze, CGA dialogue sync, character profiles, scanpath stepping, saccades, blinks, VOR, jitter. Every new feature lands here. | Medium (maintainability) |
| A-2 | **`TargetSelector::ResolveTarget` is O(N) per actor per frame** | Full `highActorHandles` scan inside a per-actor call → O(N²) with N nearby actors. At 50 actors this is 2,500 position/cone evaluations per frame, each with `Get3D()` + bone lookups. | Medium-High (crowd perf) |
| A-3 | **Static mutable snapshots on singletons** | `TargetSelector::s_crosshair`, `s_eyeAnchor`, `PlayerGazeResolver::g_hcepSignal`, `g_lastIntent` — game-thread-only by convention, unenforced. A second caller thread would race silently. | Medium |
| A-4 | **`EyeAimConstraint` global fixed array** | `MAX_TOUCHED_BONES = 512` caps ~85 actors × 6 bones. Exceeding it silently drops bone application (`FindOrCreate` returns nullptr, no counter). | Medium |
| A-5 | **`VisualEffectsManager` emitter cap 64** | `kMaxEmitterActors=64` — visual output silently disappears for actor 65+ in a crowded cell. No log when the cap is hit. | Medium |
| A-6 | **Per-frame `std::string` in `FindBoneFuzzy`** | Allocates a lowercase copy of every bone name during traversal — only on first resolve (cached), but the fuzzy path runs per unresolved eye per actor. | Low |
| A-7 | **No serialization of engine state** | Deliberate (procedural gaze must not bake into saves) and correctly handled via `kSaveGame` → `ReleaseBones`. Good. | None |

---

## 3. Concurrency & Thread-Safety Audit

### 3.1 NamedPipeServer — much improved, two residual defects

**Fixed and verified good:**

- Triple-buffer telemetry publication with epoch/index ordering (release/acquire).
- Atomic `_pipeHandle`; game thread never dereferences it.
- SPSC feedback ring; full-ring drops the new packet (correct choice for telemetry).
- User-scoped security descriptor (`D:(A;;GA;;;OW)`) with honest fallback logging — Law 6 posture.
- Semantic validation (`ValidateTelemetryPacket`): magic, version, NaN/Inf, mode/state ranges, reserved bytes, valence range. Closes audit-0919 P1-1.

**Residual defects:**

| # | Finding | Location | Severity |
| --- | --- | --- | --- |
| C-1 | **`Stop()` detach path still risks use-after-free.** After 250 ms the worker is detached while it still references `this` (`_pipeName`, `_shutdownEvent`, `_slots`, `_feedbackRing`). The subsequent `CloseHandle(_shutdownEvent)` races the detached worker's `WaitForSingleObject(_shutdownEvent, ...)`. | `NamedPipeServer.cpp:100-135` | **High** (shutdown crash class) |
| C-2 | **Triple buffer lacks a happens-before proof for payload fields.** `_slots[writeIdx].packet = incoming;` (plain store) is ordered by `_publishEpoch.fetch_add(release)` → `_readyIndex.store(release)`; the reader's acquire on `_readyIndex` orders the payload read after publication — this is *probably* correct, but the epoch retry path (`TryGetLatestTelemetry`) must be shown to never read a slot mid-overwrite. With 3 slots and writer advancing `writeIdx+1`, the writer's next slot is the one being read only if the reader holds a slot for 2+ publications — the epoch bump handles that, but no stress test proves it. | `NamedPipeServer.cpp:330-345`, `NamedPipeServer.hpp:118-135` | **Medium** |
| C-3 | **`DrainOutboundQueue` creates/destroys an event per call** (every 2 ms while connected). Minor churn; use a member event. | `NamedPipeServer.cpp:395` | Low |
| C-4 | **CRC is integrity, not authentication** — documented, acceptable for local single-user, but the threat model is not written down anywhere. | — | Low (documentation) |

### 3.2 Game-thread state — sound by convention, unenforced

- `GazeEngine::_actors`, `EyeAimConstraint::g_touched`, `VisualEffectsManager::_emitters` are game-thread-only. The hook architecture (NPC ticks only via `TickAllActors` from the PlayerTag hook) enforces this structurally — good design.
- **Gap:** no debug assertion anywhere records the thread id and fails loudly if a game-thread-only API is called off-thread. `s_mainThreadId` is stored but never checked. Cheap to add, high value (§5, E-3).

### 3.3 OarConditions — correct

`std::shared_mutex` cache with clamped writes and clean reads. `RegisterWithOar` dispatches the SKSE message and reports honestly. The remaining gap is contractual (S6: pin the OAR API version and prove a rule fires), not correctness.

---

## 4. Code-Quality Findings (file:line specific)

### 4.1 Structural

| # | Finding | Location | Severity |
| --- | --- | --- | --- |
| Q-1 | `GazeEngine::ComputeDeflection` is ~700 lines with 12 responsibilities. Extract: `ResolveIntent()`, `RunScanpath()`, `CommitSaccadeOrPursuit()`, `ApplyProfile()`. | `GazeEngine.cpp:560-1250` | Medium |
| Q-2 | `TargetSelector::ResolveTarget` is ~640 lines of sequential priority blocks with heavy duplication (target-population boilerplate repeated 10×). A `SetTarget(priority, actor, pos)` helper would cut ~200 lines. | `TargetSelector.cpp:113-889` | Medium |
| Q-3 | `VisualEffectsManager.cpp` mixes lifecycle, geometry math, shader tinting, and panel logic in one TU. `TintBeamGeometry` + `BeamTransform` belong in a `VisualAssets` helper. | `VisualEffectsManager.cpp:60-230` | Low |
| Q-4 | Dead code: `PerformanceProfiler` is included by `GazeEngine.cpp` but **never instantiated anywhere** — the 150 µs budget is enforced only by `EndFrame`'s ad-hoc `_lastFrameUs > 1500` warning (which is 1,500 µs, not 150 µs — the budget comment and the check disagree by 10×). | `PerformanceProfiler.hpp`, `GazeEngine.cpp:418-432` | **Medium** |
| Q-5 | `AnimationHook.cpp` retains `CharacterHook`/`ActorHook` template instantiations that install vtable patches whose Hook bodies do nothing for non-player tags (they return immediately after `_original`). Three vtable patches where one (PlayerCharacter) does all the work. Either document why the other two patches exist (safety net for runtimes where PlayerCharacter::Update doesn't fire?) or remove them. | `AnimationHook.cpp:180-215, 320-330` | Medium (needs in-engine evidence to decide) |
| Q-6 | `GazeEngine::TickActor` computes `startYaw` then discards it (`(void)startYaw`) — leftover. | `GazeEngine.cpp:545-556` | Trivial |
| Q-7 | `TrueGazeAPI.cpp` `lodTier` derivation is a lie: `out->lodTier = state->eyeSaturated ? 1 : 0;` — eye saturation is not an LOD tier. Document as placeholder or compute the real tier. | `TrueGazeAPI.cpp:44` | Low |
| Q-8 | `PublishState` sends `relationshipRank = 0` and `gameFrameNumber = 0` placeholders (audit-0919 P2-2, still open). `mutualGazeAngle` is populated from `lastYawDeg` — not the protocol's "angle between NPC and player gaze rays". | `GazeEngine.cpp:1600-1615` | Medium |
| Q-9 | `ConfigManager::Save()` writes `fGazeRayThicknessCm` but `Sanitise()` never clamps it (all other Visuals floats are clamped). | `ConfigManager.cpp:390-400` | Trivial |
| Q-10 | `EyeAnchorFromHeadBone` is duplicated in three places (GazeEngine.cpp anon-ns, TargetSelector.cpp anon-ns, PlayerGazeResolver uses its own head lookup with a *different* eye height fallback: 160 vs 125 units). The 125/160 inconsistency means the crosshair face-position fallback and the gaze-solve fallback disagree by 35 units. | `GazeEngine.cpp:88`, `TargetSelector.cpp:60`, `PlayerGazeResolver.cpp:24` | **Medium** |
| Q-11 | Magic numbers persist despite the constants effort: `28.0f`/`280.0f` (defer glance window), `150.0f` (glance cone), `1.54f` (head catch-up), `2.5f`/`4.0f` (eye dwell), `0.25f` (calm speed), `45.0f`/`400.0f`/`2025.0f` (catch-up thresholds) are inline literals in `ComputeDeflection`. They are well-commented but not tunable and not named. | `GazeEngine.cpp:640-1250` | Low-Medium |
| Q-12 | `FindBoneFuzzy` lowercases via `std::transform` + lambda per bone — fine (cached), but `std::tolower` on `unsigned char` is correct; no defect. Noted for completeness. | `GazeEngine.cpp:105` | None |

### 4.2 Correctness risks (potential bugs)

| # | Finding | Location | Severity |
| --- | --- | --- | --- |
| B-1 | **`WorldTargetToLocalGaze` singularity gate uses horizontal distance < 25 units** — but `IsInVisualCone` was specifically relaxed to 16 units² (≈4 units) for the Helgen cart. A seated actor at 20 units horizontal distance passes the cone check but then gets yaw=0/pitch=0 from the solve gate → stares forward. The two thresholds disagree; the solve gate should match the cone's 16-unit² logic. | `GazeEngine.cpp:130-140` vs `TargetSelector.cpp:70-90` | **Medium** (cart-class regressions) |
| B-2 | **`state.cachedRoot != root` invalidation misses 3D rebuild identity.** Comment says "per actor / root model" but the cache key is the root pointer; a cell change that rebuilds 3D at the *same* address (allocator reuse) would keep stale bone pointers → writes into freed/reused nodes. Low probability, high impact. Mitigation: also compare `world.translate` sanity or re-probe on `kCellChange`-adjacent events. | `GazeEngine.cpp:1420-1425` | Low-Medium |
| B-3 | **`EyeAimConstraint::WithdrawActor` sets `g_frameOpen = g_touchedCount != 0`** — if the player's withdraw (1st-person path, every frame) empties the array mid-frame, a subsequent NPC `Apply` re-opens the frame and re-caches `originalRotate` from an *already-modified* bone if `BeginFrame` ran earlier and the bone was touched by another actor... Actually safe: `BeginFrame` zeroes the table once per frame and `Apply` composes from the cached pristine pose. But the `g_frameOpen` toggle means `BeginFrame`'s fail-safe `Withdraw()` can fire mid-frame after a player withdraw emptied the table — restoring bones the NPC path already re-cached. Trace carefully; add a frame-generation counter instead of a bool. | `EyeAimConstraint.cpp:120-135, 175-180` | Medium (subtle) |
| B-4 | **`RefreshTuning` is called from console commands (`ApplyAndReport`) mid-frame** — it mutates `TargetSelector::s_crosshair`/`s_eyeAnchor` and the Visuals tuning while `TickAllActors` may be mid-iteration. Game-thread-only so no data race, but a mid-frame tuning swap can produce one inconsistent frame (e.g. new cone params with old cached bones). Acceptable; document. | `ConsoleCommands.cpp:75-80` | Low |
| B-5 | **`ComputeDeflection` reads `RE::UI::GetSingleton()` up to 3× per actor per frame** (mode arbitration, CGA dialogue sync, eye-dwell scale). Each call is a singleton lookup + menu-name compare. Cache once per frame in `EndFrame`/frame scope. | `GazeEngine.cpp:745, 905, 1080` | Low (perf) |
| B-6 | **`_frameFixationScale`/`_frameTriangleEnabled`/`_frameVertexWeights`/`_frameCgaRoll` are member "frame-scope" variables** — they are per-`ComputeDeflection`-call scope masquerading as frame scope. If `ComputeDeflection` were ever re-entered (it isn't today), these would corrupt. Pass them as a small struct instead. | `GazeEngine.hpp:170-180` | Low |
| B-7 | **`ClassifyRegion` returns 0 (LeftEye) for the centered dead zone** (absolute yaw ≤ 1° and absolute pitch ≤ 3°) — a centered gaze classifies as LeftEye. Harmless for visuals (region colour), but OAR conditions keyed on LeftEye fire for center gaze. Consider a `Center`/`Face` region or return the last region. | `GazeEngine.cpp:175-180` | Low |
| B-8 | **`MicroJitter::Update` catch-up path halves offsets but discards accumulated time** — after a stall the drift snaps toward mean instantly (visible micro-jump after every load screen). A gentler decay (e.g. ×0.9 per stall) or a re-seed would be smoother. | `MicroJitter.hpp:95-105` | Low |
| B-9 | **`SaccadeGenerator::Update` tail not read in this audit** — the LUT interpolation and terminus handling are unit-tested (TestMainSequenceFidelity), so risk is low. | — | None |
| B-10 | **`NamedPipeServer::WorkerLoop` creates `UserOnlySecurityDescriptor` per reconnect iteration** — fine (cheap), but on descriptor failure it logs a warn **every reconnect cycle** (every 3 s while no client). Rate-limit. | `NamedPipeServer.cpp:150-160` | Trivial |

### 4.3 Error handling & safety

- **SEH frame guard (G3)** is exemplary — rate-limited, address-logging, game-surviving.
- **C++ try/catch** wraps `VisualEffectsManager::UpdateActor` and both `BSModelDB::Demand` sites. Good.
- **Gap:** `GazeEngine::TickActor` itself is *not* individually wrapped — it relies on the outer SEH frame. That is sufficient (the SEH frame covers the whole body), but a fault anywhere in the tick abandons **all** actors' frames, not just the faulting actor. Per-actor isolation (SEH inside `TickActorList`'s loop) would degrade one actor instead of all. **Recommend for R14.**
- **Gap:** `ConsoleCommands::Install` writes into the engine's command table with a preserved opcode — the design is sound and field-proven, but `g_helpStrings` persistence is the only thing keeping `helpString` alive; a static_assert or comment cross-reference exists. Acceptable.

---

## 5. Test Coverage Audit

### 5.1 What exists

`tests/KinematicsTests.cpp` — 12 suites, all passing:
SaccadeGenerator, VorCoordinator (incl. latency gap + cervical clamp), MicroJitter (bounded drift), SocialTriangle (organic path + landing scatter), BoneController, EyeResidualAllocation, MainSequenceFidelity, JitterBrownianSeeding, LodManager, EfmBlinkController, TelemetryPackets (layout+CRC), TelemetrySemanticValidation, CharacterProfile (parity/ordering/clamps/weighted vertices).

`tests/HcepBridgeClientMock.cpp` — pipe round-trip, 10 frames, feedback path.

### 5.2 What is NOT tested (priority order)

| Module | Gap | Why it matters |
| --- | --- | --- |
| `TargetSelector` | **Zero tests.** The most logic-dense module (889 lines: priorities, cones, hysteresis, defer, voice-address, salience bias) is pure game-thread code with no harness. Every cart-scene bug of the last week was found by hand in this file. | Highest |
| `GazeEngine::ComputeDeflection` | Zero tests (SDK-bound, but the mode arbitration / CGA sync / speed-model logic is extractable and testable). | High |
| `BoneController::CalculateCgaStrain` | Tested only via one assertion in TestBoneController; no boundary tests. | Medium |
| `NamedPipeServer` triple buffer | No stress test (audit-0919 P1-2 still open). No TSan run. | High |
| `EyeAimConstraint` | No test of compose/withdraw idempotency or the 512-cap overflow path. | Medium |
| `CharacterProfile::GatherTemperament` | SDK adapter untested (crashed once in-engine, 2026-09-25 — fixed by base-form reads, but no regression test possible without a mock). | Medium |
| `ConfigManager::Sanitise` | No unit tests for clamp/renormalise logic (pure, easily testable). | Medium |
| `ValidateTelemetryPacket` | Tested for 3 cases; no fuzz/property testing. | Medium |
| Console slot-reclaim logic | `FindReclaimableSlots` classification is pure-ish but untested (needs a mock table). | Low |

### 5.3 Test infrastructure gaps

- Raw `assert` + `main()` — no framework, no failure isolation (first assert aborts the whole suite), no CI wiring, asserts compile out in Release (the suite must be built Debug — nothing enforces this).
- No test runner config (no CTest registration — `add_executable` only; `enable_testing()` absent).
- No fuzzing anywhere despite a wire protocol.
- No sanitizer builds (ASan/UBSan/TSan presets absent from `CMakePresets.json`).

---

## 6. Build System Audit

### 6.1 Current state (good bones)

- C++23, `/W4 /permissive- /Zc:preprocessor /Zc:__cplusplus /EHsc` — solid warning posture.
- Hard-fail on missing SDK (the C-2 lesson is permanently encoded).
- Post-build DLL refresh to `skyrim/SKSE/Plugins/` (stale-copy class closed).
- Standalone mode for SDK-free test builds.
- vcpkg manifest with pinned baseline.

### 6.2 Gaps

| # | Gap | Impact |
| --- | --- | --- |
| K-1 | **No CI whatsoever** (only charter-integrity workflow, which cannot run — private-repo Actions minutes, issue #9). No build/test gate exists anywhere automated. | High |
| K-2 | **No `.clang-format`** — CHANGELOG 1.0.5 claims "codebase reformatted (clang-format)" but no config file exists in the repo; formatting is ad-hoc per-session. | Medium |
| K-3 | **No `.clang-tidy` / static analysis** — no `analysis` preset, no `/analyze`, no clang-tidy runner. | Medium |
| K-4 | **No sanitizer presets** (ASan/UBSan for the standalone tests; TSan unavailable on MSVC but the bridge mock could run under clang/WSL). | Medium |
| K-5 | **`file(GLOB_RECURSE ... CONFIGURE_DEPENDS)`** for sources — acceptable with CONFIGURE_DEPENDS, but explicit lists are the world-class norm (better error messages, no accidental glob of scratch files). | Low |
| K-6 | **`enable_testing()`/CTest absent** — tests build but no `ctest` integration; CI-ready test invocation doesn't exist. | Medium |
| K-7 | **vcpkg.json `version-string: 1.0.0`** vs project VERSION 1.0.5 — drift. | Trivial |
| K-8 | **No PDB-to-symbols-package automation in CMake** (packaging scripts handle it, but the build doesn't emit a hash manifest). | Low |
| K-9 | **Warnings-as-errors not set** (`/WX` absent) — with /W4 clean, /WX is nearly free and prevents regression. | Low |
| K-10 | **No `CMakeUserPresets` guidance / toolchain validation** — `VCPKG_ROOT` absence fails late with an obscure error; a configure-time check with a clear message exists only in the prerequisite installer. | Low |

---

## 7. Performance Audit

### 7.1 Measured vs claimed

- **Claimed budget:** 150 µs frame time (PerformanceProfiler comment, PRD).
- **Enforced check:** `_lastFrameUs > 1500` (1.5 ms) warning in `EndFrame` — 10× the claimed budget, and `_lastFrameUs` measures only the hook-frame window, not the full engine cost.
- **Measured:** nothing. No benchmark artifacts exist. The 150 µs claim is unevidenced (audit-0919 P2-1, still open).

### 7.2 Hot-path cost analysis (per frame, N nearby actors)

| Operation | Cost | Frequency | Risk |
| --- | --- | --- | --- |
| `TargetSelector::ResolveTarget` full process-list scan | O(N) per actor → **O(N²)** total; each iteration: `handle.get()` (refcount), `Get3D()`, `GetWorldPosition`, `IsInVisualCone` (atan2), `DistanceMeters` | Every actor, every frame | **High at N≥25** |
| `GetActorHeadPosition` bone lookup | `GetObjectByName` × 4 candidates (string compares) — **not cached** in TargetSelector (unlike GazeEngine's cache) | Every candidate evaluation | **High** — this is the hidden multiplier on the O(N²) |
| `RE::UI::GetSingleton()` + menu compare | ~3× per actor per frame | Every actor | Low-Medium |
| `PlayerGazeResolver::Resolve` (crosshair) | Camera frame + face bone lookup per call | Per actor (crosshair block) | Medium |
| `EyeAimConstraint::FindOrCreate` | Linear scan of 512-entry array | 6× per actor | Low (512 is small) |
| `RefreshWorldTransform` + `UpdateDownwardPass` | Scene-graph update per touched bone | 6× per actor | Medium (inherent) |
| spdlog formatted logging | Throttled (1 Hz debug rays, 1/300 target trace) | Bounded | Low |
| `std::uniform_real_distribution` constructions | Per call (no thread_local reuse) | Several per actor | Trivial |

**Verdict:** the engine is comfortably fast for ≤10 actors (the tested scenario). At tavern/city density (25-50 actors) the O(N²) scan + uncached bone lookups are the dominant cost and unmeasured. The LOD tier-3 cull happens *after* the target resolution — culled actors still pay the full scan. **Reorder: cull before resolve** (cheap win, §plan P-2).

### 7.3 Memory

- Per-actor state: `ActorGazeRuntime` ≈ 1-2 KB × 512 max ≈ <1 MB. Fine.
- `EyeAimConstraint` 512 × ~48 B = 24 KB static. Fine.
- Emitter map ≤ 64 actors. Fine.
- No per-frame heap allocation found in the tick path (the `FindBoneFuzzy` string is init-only). Good.

---

## 8. Documentation Drift Audit (14 contradictions)

The project's own charter (truth-first, four-state vocabulary) is violated by its own documents. Each item below is a factual contradiction between two project documents or between a document and the code:

| # | Contradiction | Documents | Resolution needed |
| --- | --- | --- | --- |
| D-1 | R6 publication gate says "not yet ready for public 1.0" (all boxes open) vs R7 "✅ Complete — 1.0.0 Published" | ROADMAP R6 vs R7 | Close or annotate the R6 gate |
| D-2 | R2 says "not yet observed in-game — this is the next task" vs R11/R13/STATUS "in-engine verified" | ROADMAP R2 vs R11/R13 | Update R2 status line |
| D-3 | Phase 3 "telemetry consumed by nobody" vs S4 fusion implemented + STATUS "consumed" | ROADMAP Ph3 vs SOTA S4 | Update Ph3 |
| D-4 | Phase 1 "known gaps" (smoothstep, fixed seed, no residual) — all fixed since R3 | ROADMAP Ph1 vs R3/STATUS | Delete the stale warning block |
| D-5 | OAR: ROADMAP Ph4 ~90% + STATUS "hook registered" vs audit-0919 P2-6 "intentionally returns false" + S6 "pin API version" | Multiple | One truthful OAR paragraph everywhere |
| D-6 | VR: ROADMAP Ph6/R9 "✅ In-Engine Verified" vs audit-0919 P1-5 "approximate pose, no OpenVR feed" + S7 "future work" | Multiple | Downgrade VR claims to head-directed pilot status |
| D-7 | LICENSE = GPLv3 vs README badge "Proprietary" vs R0/P2-10 "proprietary, prohibits distribution" | LICENSE vs README vs ROADMAP | **Decide the license. One badge.** |
| D-8 | README/PRD say C++20 vs CMake `CMAKE_CXX_STANDARD 23` | README/PRD vs CMakeLists | Fix README/PRD |
| D-9 | PRD v1.0.3 vs README/CMake v1.0.5 vs CHANGELOG [Unreleased] | PRD vs README vs CHANGELOG | Sync versions |
| D-10 | STATUS.md header "September 25" vs footer "September 21" vs Sept-26 content; tier percentages sum to **110%** (50+55+5) | STATUS.md internal | Fix dates + percentages |
| D-11 | TEST_SCENARIO Stage 2 "NPCs do not gaze at each other — not implemented" vs R11 verified NPC-to-NPC eye contact | TEST_SCENARIO vs R11 | Update Stage 2 |
| D-12 | Console commands `tg*` in older docs vs `stg*` in newer (tg = vanilla grass collision) | Multiple | Global find/replace in stale docs |
| D-13 | `docs/AUDIT_REPORT_2026-09-15.md` and `docs/TrueGaze™ — Deep World-Class Technical…Audit.md` are **byte-identical duplicates**; README ↔ TRUEGAZE_ARCHITECTURE near-duplicate (R0 open item) | docs/ | Deduplicate |
| D-14 | R5 lists "Fix PackageMod.ps1 $PSScriptRoot" as open but the script already uses it | ROADMAP R5 vs scripts | Close the checkbox |

**Also stale:** `docs/STATUS.md` Phase 6/7 checklists reference `tgstatus` (old name) and a "Phase 7 SIMD" that is designed-only; the footer date is 5 days behind the header.

---

## 9. Security & Privacy Audit

| Area | State | Notes |
| --- | --- | --- |
| Pipe ACL | ✅ User-scoped SDDL descriptor, honest fallback | Law 6 posture good |
| Payload encryption | ❌ Plaintext | Acceptable for local single-user; document threat model |
| Authentication | ❌ CRC only | Same-session malicious process could inject telemetry; semantic validation bounds the damage |
| Data minimization | ⚠️ `trackedPersonId` forwarded unvalidated | Zero it by default or validate |
| Consent/audit | ❌ No connection audit log beyond connect/disconnect lines | Add connection identity event (S9) |
| Regulatory claims | ⚠️ LICENSE/GOVERNANCE mention GDPR/CCPA/BIPA posture; no DPIA exists | Keep honest; do not claim compliance |
| Console command table writes | ✅ Guarded, field-proven, opcode-preserving | Exemplary defensive engineering |
| INI handling | ✅ Byte-safe Latin-1 tooling, user-INI preservation on deploy | Deploy-wipe bug fixed 2026-09-18 |

---

## 10. Tooling & Packaging Audit

**Strong:** `TrueGaze.cmd` (10 modes, batch-trap-hardened), `Test-TrueGazeHealth.ps1` (15 checks), `Deploy-TrueGaze.ps1` (INI-preserving, stale-artifact cleanup), `Install-AllPrerequisites` (idempotent, VCPKG_ROOT persistence), charter pre-commit hook.

**Gaps:**

- `LaunchTrueGaze.bat` hardcodes `G:\Program Files (x86)\Steam\...` and `D:\Projects\SkyrimTrueGaze` — machine-specific (the .cmd is portable; the .bat is not).
- Three copies of `TrueGazeConfig.html` (root, `skyrim/`, `dist/`) manually synchronized — drift risk (2,362 lines each).
- `GazeBeam.nif` (the retired, crash-causing Python-generated NIF) **still ships** in `skyrim/meshes/TrueGaze/` and the v1.0.5 package tree. It is disabled by config but present. Remove from the shipping tree or move to a clearly-labelled `dev-assets/` folder.
- No SHA-256 manifest generation in packaging (Package-Release.ps1 does hashes? — verify; the S8 gate requires it).
- No automated doc-consistency checker (the D-1..D-14 list above is greppable — a script could catch most of it).

---

## 11. Governance Audit

- Charter enforcement: local pre-commit hook active and tamper-tested; CI blocked by GitHub Free/private (issue #9, root-caused — not a project defect).
- GOVERNANCE.md honest map: 2 implemented, 1 partial, 2 gaps (Law 6 runtime controls, Law 10 cryptographic approval), 5 N/A.
- Open governance items: Core Tenet paraphrase divergences (issue #8, Council decision pending); Law 6 biometric runtime controls partially implemented (pipe ACL done; consent/audit/minimization open).
- **New finding:** the GPLv3 license change (recent commit) was not propagated to GOVERNANCE.md, README badge, or the R0 license-contradiction item — governance docs now lag the license decision they are supposed to record.

---

## 12. Prioritized Findings Register

### P0 — Product blockers (none currently)

None. The product works in-engine and is field-verified. The 2026-09-19 P0-1 (headline unverified) and P0-2 (HCEP unused) are closed by R11/R13 and S4 respectively.

### P1 — High priority (fix before next release)

| ID | Finding | Source § |
| --- | --- | --- |
| P1-1 | `NamedPipeServer::Stop()` detach path use-after-free risk | §3.1 C-1 |
| P1-2 | Triple buffer: no stress test / memory-model proof | §3.1 C-2 |
| P1-3 | O(N²) target scan + uncached bone lookups in `TargetSelector`; cull-before-resolve missing | §7.2, A-2 |
| P1-4 | Eye-height fallback inconsistency (125 vs 160 units) + triple-duplicated `EyeAnchorFromHeadBone` | §4.1 Q-10 |
| P1-5 | Solve-gate vs cone-gate singularity threshold mismatch (25 units vs 16 units²) | §4.2 B-1 |
| P1-6 | Documentation drift: 14 contradictions incl. license identity (GPLv3 vs Proprietary) | §8 |
| P1-7 | No CI / no automated build+test gate | §6.2 K-1 |
| P1-8 | Retired crash-causing `GazeBeam.nif` still in shipping tree | §10 |

### P2 — Important hardening

| ID | Finding | Source § |
| --- | --- | --- |
| P2-1 | `PerformanceProfiler` dead code; budget check 10× off claim; no measurements | §4.1 Q-4, §7.1 |
| P2-2 | Feedback packet placeholder fields (relationshipRank, gameFrameNumber, mutualGazeAngle semantics) | §4.1 Q-8 |
| P2-3 | `TargetSelector` has zero tests — extract pure logic, harness it | §5.2 |
| P2-4 | No clang-format/clang-tidy/editorconfig; no /WX; no CTest; no sanitizer presets | §6.2 K-2..K-6, K-9 |
| P2-5 | `EyeAimConstraint` 512-cap silent overflow; add counter + log | §2.2 A-4 |
| P2-6 | Emitter cap 64 silent; add log-on-cap | §2.2 A-5 |
| P2-7 | Per-actor SEH isolation (fault in one actor shouldn't abandon all) | §4.3 |
| P2-8 | `EyeAimConstraint` frame-open bool → frame-generation counter | §4.2 B-3 |
| P2-9 | Public API contract unspecified (thread, lifecycle, lodTier placeholder) | §4.1 Q-7, audit-0919 P2-7 |
| P2-10 | Configurator HTML triple-copy drift risk | §10 |
| P2-11 | Threat model document for the pipe (plaintext, CRC-not-auth) | §9 |
| P2-12 | `lodTier` API placeholder; `Sanitise` missing thickness clamp; trivial cleanups (Q-6, Q-9, B-10) | §4.1 |

### P3 — Strategic / future

| ID | Finding |
| --- | --- |
| P3-1 | VR: real HMD pose path (OpenVR feed) — S7 contract |
| P3-2 | OAR: pin API version, prove a rule fires — S6 |
| P3-3 | Engine-neutral HCEP contract extraction (robotics/platform story) |
| P3-4 | Multi-threaded SIMD evaluation for crowds (designed, 0%) |
| P3-5 | Fuzz harness for telemetry packets |
| P3-6 | Godot/UE5 adapters (Phase 8) |

---

## 13. What "World-Class" Looks Like for This Codebase (gap summary)

| Dimension | Today | World-class target |
| --- | --- | --- |
| Correctness culture | ✅ Exemplary (regression tests, guards, honest logs) | Maintain |
| In-engine verification | ✅ Core verified; VR/OAR/visuals pending | Close S5.2/S6/S7 gates |
| Concurrency | 🟡 Well-designed, unproven | Stress tests + documented memory model + shutdown-safe lifecycle |
| Performance | 🔴 Unmeasured, O(N²) risk | Measured budget, cull-first, cached lookups, benchmark artifacts |
| Testing | 🟡 12 math suites; 0 logic-module tests | TargetSelector/ConfigManager/pipe harnesses + CTest + fuzz |
| Build/CI | 🔴 No CI, no formatter/linter config | GitHub Actions (or self-hosted), clang-format/tidy, /WX, CTest |
| Documentation | 🔴 14 contradictions | Single source of truth + consistency checker script |
| Security | 🟡 Good ACL, undocumented threat model | Threat model doc + minimization defaults |
| Legal | 🔴 Contradictory identity | One license decision, propagated everywhere |

---

## 14. Verdict

TrueGaze is a **credible, working, honestly-documented biomechanical gaze engine** — a genuinely novel product with no competitor equivalent (oculomotor science + cognitive gaze + external tracker bridge + modder API). The September engineering sprint (R11→R13) converted it from "compiles" to "verified in a live game with a human in the loop."

The remaining distance to world-class is **not more features** — it is the unglamorous infrastructure: concurrency proof, performance measurement, logic-module tests, CI, formatter/linter, documentation truth, and one license decision. All of it is scoped in the companion plan.

**Recommended next action:** execute `docs/IMPLEMENTATION_PLAN_2026-09-26_ENGINEERING_EXCELLENCE.md` Phase E1 (concurrency + shutdown safety) and E2 (performance measurement) before any new gameplay feature.
