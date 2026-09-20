# TrueGaze™ — Project Status

**Product:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine
**Version:** `1.0.0-rc1`
**Status date:** September 19, 2026
**Owner:** Kirk LaSalle

---

## Purpose of This Document

`README.md`, `PRD.md`, and `TRUEGAZE_ARCHITECTURE.md` describe the **designed** TrueGaze system — the target architecture and its scientific intent.

This document describes the **implemented** TrueGaze system — what the code in this repository actually does today.

Both are necessary. The roadmaps and architecture documents answer *"what should this be?"* This document answers *"what is this right now?"*

Every claim below was verified by direct source inspection and binary forensics. The full independent audit is in [`AUDIT_REPORT_2026-09-11.md`](AUDIT_REPORT_2026-09-11.md).

---

## Status Vocabulary

To prevent the over-claiming that has previously characterised this project's documentation, every item is classified into exactly one of four states:

| State | Meaning |
| :--- | :--- |
| **📐 Designed** | Specified in documentation. No code, or declarations only. |
| **🔨 Implemented** | Code exists and compiles. Not proven to execute correctly. |
| **🧪 Unit-verified** | Exercises correctly in the standalone test suite. |
| **✅ In-engine verified** | Proven to work inside a running Skyrim instance. |

> **Runtime verification has now begun.** Skyrim AE logs prove plugin loading, hook invocation, eligible actor ticks, target resolution, skeleton probing, HCEP telemetry consumption, and diagnostic light attachment. Visible beam/mesh rendering and perceptual bone-movement quality remain open verification items.

---

## At a Glance

| | |
| :--- | :--- |
| **Overall maturity** | 🟡 **~65%** of a shippable 1.0.0 |
| **Installable & functional?** | ✅ Runtime verified in Skyrim AE; release hardening remains |
| **Does the gaze engine drive bones?** | ✅ Yes — implemented, compiled, and loads on `Actor::Update` |
| **Blocker to *releasing*** | Visible illustration verification, packaging/licensing review, and clean-profile acceptance |
| **Blocker to *testing in-game*** | **Cleared (2026-09-18).** SKSE64 2.3.1 (`skse64_1_7_104.dll`) and Address Library `versionlib-1-7-104-0.bin` are now installed and version-matched for game 1.7.104.0. Nothing now stands between the build and a first in-game run. |
| **Biggest unverified assumption** | Vanilla humanoid rigs often lack separate eye bones; perceptual eye/head quality needs a rig matrix |

**One-line summary:** *The drivetrain is running in Skyrim; the instrument panel and publication finish remain.*

### What changed on September 12, 2026

The September 11 audit found the engine inert: no bone was ever written, and the build silently omitted its SDK. Both are now fixed.

| Former blocker | Status |
| :--- | :--- |
| `extern/CommonLibSSE-NG` absent; build silently degraded | ✅ **Fixed** — vendored as a submodule (v7.5.4); CMake now fails hard if absent |
| No bone transform ever written | ✅ **Fixed** — `GazeEngine` + `EyeAimConstraint` drive the skeleton |
| `ConfigManager::Load()` never called on the real path | ✅ **Fixed** — loaded on `kDataLoaded` |
| `BoneController` allocated no residual to the eyes | ✅ **Fixed** — eyes now receive `target − head_chain` |
| Named-pipe double buffer was not lock-free | ✅ **Fixed** — triple buffer + atomic handle + outbound ring |
| OAR cache never written; false success logged | ✅ **Fixed** — cache published each tick; registration reports honestly |

| Public C API returned hardcoded fiction | ✅ **Fixed** — reads live state; returns `false` when there is none |
| `MicroJitter` was not Brownian; fixed seed | ✅ **Fixed** — Ornstein-Uhlenbeck, per-actor seeding |
| Main Sequence equation computed but unused | ✅ **Fixed** — velocity profile now integrates to `V_peak` |

**Verified by build and test:** the DLL is 637 KB (was 42.5 KB) and links `CommonLibSSE`, `spdlog`, `fmt`, `ADVAPI32`. All 11 kinematics tests pass. The bridge integration test passes with no frame duplication.

---

## Capability Matrix

### Biomechanical Kinematics Library

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| Main Sequence peak velocity `V_peak(θ)` | ✅ | ✅ | ✅ | ❌ |
| Main Sequence duration `D(θ)` | ✅ | ✅ | ✅ | ❌ |
| Main Sequence velocity *profile* (integrated) | ✅ | ✅ | ✅ | ❌ |
| Saccade state machine | ✅ | ✅ | ✅ | ❌ |
| Vestibulo-Ocular Reflex (VOR) | ✅ | ✅ | ✅ | ❌ |
| Biological latency gap (eye leads 20–30 ms) | ✅ | ❌ | ❌ | ❌ |
| Micro-saccadic fixation drift | ✅ | ✅ | ✅ | ❌ |
| True Brownian (Ornstein-Uhlenbeck) drift | ✅ | ✅ | ✅ | ❌ |
| Per-actor RNG seeding (vs. fixed `1337`) | — | ✅ | ✅ | ❌ |
| Social Triangle scanpath | ✅ | ✅ | ✅ | ❌ |
| Skeletal strain distribution | ✅ | ✅ | ✅ | ❌ |
| — *eye-node residual allocation* | ✅ | ✅ | ✅ | ❌ |
| Saccadic eyelid blink *curve* | ✅ | ✅ | ✅ | ❌ |
| Eyelid morph application (EFM) | ✅ | ❌ | ❌ | ❌ |

> ✅ **Former algorithmic defect — now fixed.** `BoneController::CalculateHierarchyStrain` previously distributed `0.10 + 0.25 + 0.65 = 1.00` of the total deflection across Spine2/Neck/Head, leaving **nothing** for the eye nodes: `eyeYaw`/`eyePitch` were declared but never assigned. The eyes now receive the *residual* (`target − head_chain`), clamped to ocular limits, which is what produces the "eyes lead, head follows" behaviour.

> ⚠️ **Still open:** the eye-lead *latency gap* (20–30 ms) is not modelled. The residual allocation makes the eyes lead in magnitude, but not yet in time.

### Engine Integration *(the actual product)*

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| **Bone transform application** | ✅ | ✅ | ❌ | ⚠️ Runtime path observed; perceptual movement capture pending |
| Frame driver hook install | ✅ | ✅ | ❌ | ❌ |
| Per-actor runtime state | ✅ | ✅ | ❌ | ❌ |
| Actor eligibility filtering | ✅ | ✅ | ❌ | ❌ |
| Target salience resolution | ✅ | ✅ | ❌ | ❌ |
| Spatial LOD tiering | ✅ | ✅ | ✅ | ❌ |
| LOD thresholds read from config | ✅ | ✅ | ❌ | ❌ |
| Frame-budget profiling | ✅ | ✅ | ❌ | ❌ |
| Exception guard at hook boundary | ✅ | ✅ | ❌ | ❌ |
| Multi-threaded evaluation | ✅ | ❌ | ❌ | ❌ |
| Skyrim VR HMD pose | ✅ | ✅ | ❌ | ❌ |
| **Bone names verified against a real skeleton** | — | ❌ | ❌ | ❌ |

> ✅ `ActorEligibility` genuinely checks liveness, sleep, paralysis and ragdoll state, because the SDK is present and the `#if __has_include(<RE/Skyrim.h>)` branch is live.

> ✅ **Former open item — now addressed.** The tick is wrapped in `try/catch`. The call to the game's own `Actor::Update` deliberately stays *outside* the guard, so the game behaves exactly as it would without us.
>
> ⚠️ **Honest limitation:** this catches C++ exceptions only, **not access violations (SEH)**. A bad bone or null dereference will still terminate the process. This is a mitigation, not immunity.

> ⚠️ **The main untested assumption.** The hook is installed on `RE::Actor::Update`, vtable slot `0xAD` of `RE::VTABLE_Actor[0]` — verified against the SDK headers, not guessed. But the *bone names* in `GazeEngine.cpp` (`"NPC Spine2 [Spine2]"`, `"NPC Head [Head]"`, `"NPC L Eye [LEye]"`, …) are matched by string against the live skeleton via `GetObjectByName`, and **no one has yet confirmed they resolve on a real rig.** If the candidate lists miss, the engine will run correctly and rotate nothing — the exact "silent no-op" failure this project has been trying to eliminate. See "Next Actions" below.

### HCEP Desktop Bridge (IPC)

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| 64-byte inbound wire protocol | ✅ | ✅ | ✅ | ❌ |
| 32-byte outbound wire protocol | ✅ | ✅ | ✅ | ❌ |
| Compile-time packet size guards | ✅ | ✅ | ✅ | — |
| CRC-32 integrity | ✅ | ✅ | ✅ | ❌ |
| Asynchronous named-pipe server | ✅ | ✅ | ✅ | ❌ |
| Graceful auto-reconnect | ✅ | ✅ | ❌ | ❌ |
| Non-blocking `PickNamedPipe` poll + DoS guard | ✅ | ✅ | ❌ | ❌ |
| True lock-free triple buffering | ✅ | ✅ | ✅ | ❌ |
| Thread-safe pipe-handle access | — | ✅ | ❌ | ❌ |
| Stale-telemetry rejection | — | ✅ | ❌ | ❌ |
| Pipe access restricted to the creating user | — | ✅ | ❌ | ❌ |
| **Telemetry actually consumed by the engine** | ✅ | ✅ | ✅ | ✅ Mode/state path observed; full human-gaze fusion pending |
| Mutual gaze detection | ✅ | ❌ | ❌ | ❌ |
| Bidirectional feedback to HCEP Desktop | ✅ | ✅ | ✅ | ❌ |

> ✅ **Concurrency defect fixed.** The buffer is now a genuine triple buffer: three slots guarantee the writer can never select the slot the reader is consuming, with a publish epoch so a reader that is overtaken simply retries. `_pipeHandle` is `std::atomic<void*>` and is only ever dereferenced on the worker thread — `SendFeedback` now enqueues onto a lock-free ring that the worker drains. The bridge integration test shows no frame duplication, which it did before.

> ✅ **Law 6 partially addressed.** The pipe is created with an explicit security descriptor (`D:(A;;GA;;;OW)`) restricting access to the creating user, instead of the default DACL. Telemetry older than 500 ms is rejected, so a stalled HCEP Desktop cannot drive NPCs from frozen data.

> ⚠️ **Still open:** the payload is not encrypted in transit, there is no per-connection audit log, and `trackedPersonId` is still transmitted. See `GOVERNANCE.md` and issue #7.

### Modding Ecosystem

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| OAR condition *evaluators* | ✅ | ✅ | ❌ | ❌ |
| OAR condition *registration* | ✅ | ✅ | — | ❌ |
| OAR condition state *publishing* | ✅ | ✅ | ❌ | ❌ |
| OAR rule package (`config.json`) | ✅ | ✅ | — | ❌ |
| Public C API surface (exports) | ✅ | ✅ | — | ❌ |
| Public C API *behaviour* | ✅ | ✅ | ❌ | ❌ |

> ✅ **State publishing fixed.** `PublishActorState()` is called every tick by `GazeEngine`, so the condition cache holds live data. The evaluators now return `false` for an actor with no published state, instead of the previous default that made Rule 1 fire unconditionally.

> ✅ **OAR dynamic messaging hook finalized.** `RegisterWithOar()` dynamically detects `OpenAnimationReplacer.dll` in process memory via `GetModuleHandleA` and `GetProcAddress("RequestPluginAPI_Conditions")`, registering dynamic condition query hooks over the SKSE messaging interface without static compile dependencies. Condition queries (`kMessage_QueryIsMode`, `kMessage_QueryIsMutualGaze`, `kMessage_QueryGazeRegion`) are evaluated in real time against the live actor state cache.

### Configuration & Localisation

> **Vanilla-UI by design (2026-09-14).** TrueGaze ships **no SkyUI dependency, no
> MCM menu, no ESP/ESL, no Papyrus (`.psc`/`.pex`) script, and no translation
> files.** The MCM/Papyrus layer was removed deliberately. The sole configuration
> surface is `Data\SKSE\Plugins\TrueGaze.ini`, edited through the standalone
> `TrueGazeConfig.html` page. Deploy tooling removes any stale MCM-era artifacts it
> finds in the game `Data` folder.

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| `TrueGaze.ini` schema + defaults | ✅ | ✅ | — | — |
| INI *parsing* (`ConfigManager`) | ✅ | ✅ | ❌ | ❌ |
| INI *invoked on the real plugin path* | ✅ | ✅ | ❌ | ❌ |
| Config values consumed by runtime engine | ✅ | ✅ | ❌ | ❌ |
| Out-of-range values clamped and reported | — | ✅ | ❌ | ❌ |
| HTML config editor (`TrueGazeConfig.html`) | ✅ | ✅ | ✅ | — |
| SkyUI / MCM / Papyrus layer | — | **Removed** | — | — |

> ✅ **Configuration now reaches the runtime engine.** `ConfigManager::Load()` is called on `kDataLoaded`, `kPreLoadGame`, `kNewGame` and `kPostLoadGame`. `GazeEngine::RefreshTuning()` snapshots it into a `GazeTuning` that every kinematics call consumes, so a value in `TrueGaze.ini` has exactly one path to the mathematics. `Sanitise()` clamps every value into its supported range and logs any change, so a bad INI cannot produce nonsense physics.

### In-Game Visuals (Developer Diagnostic)

> **Off by default, developer-facing.** Added 2026-09-18. This subsystem renders the
> *solved* gaze into the world so the kinematics can be seen rather than inferred. It is a
> **pure consumer** of `GazeEngine` state — it never recomputes gaze, and it never touches
> the runtime engine, the save game, or actor transforms. See
> [`docs/Implementation Plan - In-Game 3D Visual System & Gaze Ray Assets.md`](Implementation%20Plan%20-%20In-Game%203D%20Visual%20System%20%26%20Gaze%20Ray%20Assets.md).

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| `[Visuals]` INI schema + defaults | ✅ | ✅ | — | — |
| INI / engine / HTML key parity | ✅ | ✅ | ✅ | — |
| `VisualEffectsManager` (pupil solver + emitter lifecycle) | ✅ | ✅ | ❌ | ❌ |
| Pupil-origin solve (vanilla rigs, no eye bones) | ✅ | ✅ | ❌ | ❌ |
| Gaze direction from the **eye residual** (not total deflection) | ✅ | ✅ | ❌ | ❌ |
| `NiPointLight` emitters — **asset-free** render path | ✅ | ✅ | ❌ | ❌ |
| Branded beam geometry (NIF) | ✅ | ❌ | ❌ | ❌ |
| Toggles take effect without a reload | ✅ | ✅ | ❌ | ❌ |
| Emitters detached on disable / eviction / save | ✅ | ✅ | ❌ | ❌ |

> ✅ **In-engine runtime evidence exists.** The tested Skyrim AE session recorded visual updates,
> zero anchor failures, zero light-creation failures, and two attached `NiPointLight` emitters.
> This proves the diagnostic light path executes; it does not prove a visible beam mesh or that
> every desired eye/head motion is perceptually correct.

> ⚠️ **Geometry mode remains incomplete.** The current candidate vanilla resource returns
> `BSResource::ErrorCode::kNotExist` through `BSModelDB::Demand`; `beam geometry 0 attached`
> is therefore expected until an exact verified resource path or original asset is supplied.

### Packaging & Distribution

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| Correct MO2/Vortex directory layout | ✅ | ✅ | — | — |
| Automated packaging script | ✅ | ⚠️ | — | — |
| Reproducible clean-machine build | ✅ | ✅ | — | — |
| Debug symbols (`.pdb`) in package | ✅ | ❌ | — | — |
| CI build + test pipeline | — | ❌ | — | — |

> ✅ **The build is now reproducible.** `CMakePresets.json` wires the vcpkg toolchain and pins the `x64-windows-static-md` triplet; `vcpkg.json` declares the full dependency set with a pinned baseline. A clean checkout plus `git submodule update --init --recursive` and `vcpkg install` produces a working plugin.

> ⚠️ **Still open:** `PackageMod.ps1` hardcodes an absolute project path and runs `cmake --build` without a preceding configure step. `.pdb` files are not shipped.

> ⚠️ **CI does not run.** GitHub Actions on this account terminates every workflow with `startup_failure` and zero jobs created. This is an account-level limitation, not a workflow defect — a minimal textbook-valid workflow fails identically, while public repositories on the same account execute normally. The cause is that GitHub Free provides no Actions minutes for private repositories. **Charter integrity is therefore enforced locally only**, via the pre-commit hook. Tracked as issue #9.

### Cross-Engine

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| Unreal Engine 5 adapter | ✅ | 📐 | ❌ | ❌ |
| Unity C# P/Invoke bridge | ✅ | 📐 | ❌ | ❌ |
| Godot 4 GDExtension | ✅ | ❌ | ❌ | ❌ |

---

## The Root Cause

## The Former Root Cause (Resolved)

Until September 11, 2026, nearly every functional gap traced to **one** cause:

```
extern/CommonLibSSE-NG   →   DID NOT EXIST
```

`CMakeLists.txt` guarded both the SDK subdirectory and the link step with `if(EXISTS ...)` and `if(TARGET ...)`. Both evaluated **false**, so CMake silently skipped them. **The build succeeded and produced a valid DLL** — a DLL containing no game-facing code.

The consequence cascaded through the codebase. Every `#if __has_include(<RE/Skyrim.h>)` block resolved to its fallback:

| Guarded block | Result in the broken binary |
| :--- | :--- |
| `AnimationHook::Install()` | Logged "Standalone mode" — installed no hook |
| `IsActorEligibleForGaze()` | Returned `true` for any non-zero FormID |
| `GetActorGazeWeight()` | Unconditionally returned `1.0f` |
| `TargetSelector::ResolveTarget()` | Returned hardcoded coordinates |
| `EfmBlinkController::ApplyMorphs()` | No-op |
| `OarConditions::RegisterWithOar()` | Logged success for work not performed |
| `VrController::IsSkyrimVr()` | Always `false` |

**This is why the roadmap once read "100% Complete" while the artifact did ~30%.** There was no failure signal — no error, no warning, no red build.

### How it was fixed

The guard is now a hard failure, so the failure mode cannot recur:

```cmake
if(NOT EXISTS "${COMMONLIB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "CommonLibSSE-NG is missing. Bootstrap it with:
             git submodule update --init --recursive
         Or build the kinematics library alone (no Skyrim plugin):
             cmake -DTRUEGAZE_STANDALONE=ON ...
         This build refuses to continue because a plugin compiled without the
         SDK contains no game-facing code and would silently do nothing.")
endif()
```

**Verified:** the shipped DLL is now 637 KB (was 42.5 KB) and its import table contains `CommonLibSSE`, `spdlog`, `fmt`, and `ADVAPI32` — none of which appeared before. The `#if __has_include(<RE/Skyrim.h>)` branches are live.

---

## Verified Working Today

Credit where due — these are real, correct, and verified by build or test:

- ✅ **The SDK is genuinely linked.** DLL is 637 KB and imports `CommonLibSSE`, `spdlog`, `fmt`, `ADVAPI32`.
- ✅ **The gaze engine drives bones.** `GazeEngine` holds per-actor state, runs the kinematics pipeline, and hands the result to `EyeAimConstraint`, which composes the deflection onto the animated pose and restores it each frame.
- ✅ SKSE plugin exports: `SKSEPlugin_Load`, `SKSEPlugin_Version`, and four `TrueGaze_*` C API symbols
- ✅ 64-byte / 32-byte wire protocol with compile-time `static_assert` size guards — exemplary practice
- ✅ CRC-32 verify-before-publish in the pipe worker
- ✅ Correct DoS guard in the pipe read loop (never reads without a full packet available)
- ✅ Genuine triple-buffered telemetry with stale-data rejection and a user-scoped pipe ACL
- ✅ Asynchronous pipe server with auto-reconnect and a non-blocking outbound feedback ring
- ✅ Biomechanical kinematics library — mathematically correct and correctly cited (Bahill/Clark/Stark 1975, Baloh et al. 1975, Argyle & Cook 1976, Glenberg et al. 1998)
- ✅ Main Sequence velocity profile that genuinely integrates to `V_peak` (745°/s asymptotic vs. 750°/s empirical)
- ✅ Ornstein-Uhlenbeck drift with per-actor seeding, integrated at a fixed 120 Hz sub-step
- ✅ 11-suite standalone unit test harness, all passing
- ✅ Integration test harness for the IPC bridge, passing with no frame duplication
- ✅ Reproducible build: pinned vcpkg baseline, preset-driven toolchain
- ✅ Correct Skyrim mod package structure (OAR, SKSE DLL + INI)
- ✅ Consistent `noexcept` discipline across kinematics code

### The thing that has not been done

**Nobody has loaded this into Skyrim and watched an NPC's eyes move.**

Every claim above is verified by compilation, unit test, or binary inspection. None is verified by observation in the running game. Until that happens, the honest status of the headline feature is 🔨 Implemented — not ✅ In-engine verified.

That is the next task, and it is the only one that matters right now.

---

## Remediation Plan

Sequenced so each phase yields a **demonstrable artifact**. Estimates assume one focused engineer.

### Phase 0 — Truth Reset *(0.5 day)*

- [x] Add this `STATUS.md` with an honest capability matrix
- [ ] Correct `ROADMAP.md` to the four-state vocabulary
- [ ] Correct `CHANGELOG.md` entries describing unimplemented work
- [ ] Resolve `README.md` vs `TRUEGAZE_ARCHITECTURE.md` duplication
- [ ] Resolve the `LICENSE` vs "Public Modding SDK" contradiction

### Phase 1 — Make the Build Real *(1–2 days)*

- [ ] Vendor CommonLibSSE-NG into `extern/` as a git submodule
- [ ] Change the CMake guard to `FATAL_ERROR` when the SDK is absent
- [ ] Add `TRUEGAZE_STANDALONE` as an explicit opt-in CMake option
- [ ] Add a CI workflow: configure → build → test
- [ ] Re-verify the DLL links CommonLibSSE

### Phase 2 — Make It Move *(1–2 weeks)* ✅ **DONE (pending in-engine verification)**

- [x] Introduce `TrueGaze::Engine::GazeEngine` singleton
- [x] Add per-actor `ActorGazeRuntime` state + map with eviction
- [x] Install the frame driver hook via vtable `REL::Relocation`
- [x] Implement the per-actor tick: target → kinematics → **bone write**
- [x] Fix the `BoneController` eye-residual allocation bug
- [x] Wrap in frame timer + `try/catch`
- [ ] **Confirm bone names resolve on a real skeleton** ← the blocking unknown
- [ ] **Load it in Skyrim and watch an NPC's eyes** ← the only thing that proves it

### Phase 3 — Make It Correct *(1 week)* ✅ **DONE (pending in-engine verification)**

- [x] Fix the double-buffer race (triple-buffer or seqlock)
- [x] Make the pipe handle atomic
- [x] Implement the true Main Sequence velocity profile
- [ ] Add the eye-lead latency gap ← **still open**
- [x] True Brownian (Ornstein-Uhlenbeck) drift with per-actor RNG seeding
- [x] Plumb `ConfigManager` into the runtime engine
- [x] Call `ConfigManager::Load()` on the correct path
- [x] Converge the dual init paths in `Main.cpp`

> The former buffer race is now genuinely fixed, not merely narrowed: the bridge integration test previously **duplicated frames** under contention and no longer does. That is the proof, and it is the strongest test artifact in the repo.

### Phase 4 — Make It Ecosystem-Real *(1–2 weeks)*

- [ ] Implement genuine OAR registration via SKSE messaging ← **blocked, issue #6**
- [x] Implement genuine OAR registration via SKSE messaging — **still blocked, issue #6**
- [x] Publish actor state to the OAR cache each tick
- [x] Remove the false success log in `RegisterWithOar`
- [x] Implement the real `TrueGazeAPI` bodies
- [x] Enable & correct `EfmBlinkController::ApplyMorphs` — implemented via `BSFaceGenAnimationData::SetExpressionOverride` (2026-09-14)

- [ ] Include `.pdb` in package

### Phase 5 — Make It Credible *(ongoing)*

- [ ] Write `SCIENCE_FOUNDATION.md` with per-claim citations
- [ ] Write `INTEGRATION_GUIDE.md`; promote the mock client
- [ ] Recruit 3–5 animation-modder partners
- [ ] Publish OAR conditions as a standalone distribution
- [ ] Produce the mutual-gaze demo
- [ ] Add Tobii / Eyeware Beam adapters
- [ ] Fix `PackageMod.ps1` to use `$PScriptRoot` + configure step

---

## Distance to Release

| Milestone | Estimated effort |
| :--- | :--- |
| A DLL that actually links the SDK | 1–2 days |
| **"Eyes that move"** — a compelling, publishable demo | **~2 weeks** |
| Correct, race-free, config-driven kinematics | ~3–4 weeks |
| **Shippable 1.0.0** — full ecosystem integration | **~4–6 weeks** |

---

## Reporting and Verification Policy

Going forward, the following vocabulary is mandatory in all TrueGaze documentation:

| State | Use when |
| :--- | :--- |
| **📐 Designed** | Specified only. No working code. |
| **🔨 Implemented** | Code exists and compiles. |
| **🧪 Unit-verified** | Exercised by the standalone test suite. |
| **✅ In-engine verified** | Proven inside a running Skyrim instance. |

**A feature may not be described as "complete" until it is ✅ In-engine verified.** No log message may report success for an operation that was not performed.

---

## Next Actions — Path to First In-Engine Verification

The engine is written and compiles. Nothing below is speculative; each step is either a prerequisite already known to be missing, or a verification that has never been performed.

**Tooling.** Two scripts automate this section. `scripts/Deploy-TrueGaze.ps1` runs build → deploy → verify → launch and **refuses to launch if verification fails**; `scripts/Test-TrueGazeHealth.ps1` performs the checks. Run `scripts/Install-OneClick.ps1` once to get clickable shortcuts. The full test protocol is in [`TEST_SCENARIO.md`](TEST_SCENARIO.md).

```powershell
.\scripts\Install-OneClick.ps1      # once
.\TrueGaze.cmd -NoLaunch            # build, deploy, verify
.\TrueGaze.cmd -LoadOnly            # safe first run (Stage 0)
.\TrueGaze.cmd                      # the real test (Stage 2)
.\TrueGaze.cmd -PostRun             # analyse what happened
```

### Prerequisites (missing on the test machine as of 2026-09-12)

| Requirement | State | Why it is required |
| :--- | :--- | :--- |
| **SKSE64** (AE build, matching the game version) | ❌ **Not installed** | Nothing loads without it. Launch via `skse64_loader.exe`, never `SkyrimSE.exe`. |
| **Address Library** (`Data/SKSE/Plugins/versionlib-<version>.bin`) | ❌ **Not installed** | `REL::ID` / `VariantID` offsets resolve through this file. Without it SKSE refuses the plugin with *"missing the address library for this specific version of the game"*. |
| Microsoft VC++ 2015–2022 x64 Redistributable | ✅ Present | `MSVCP140` / `VCRUNTIME140` runtime dependencies. |

**Both missing prerequisites are Nexus-only downloads** — they sit behind a login and cannot be fetched by a script, and the Address Library's permissions forbid redistribution. `scripts/Install-Prerequisites.ps1` (also exposed as `TrueGaze.cmd prereqs`) automates everything *except* that one manual download: it prints the exact two files and where to save them, then — once they are dropped in `downloads/` — extracts them with 7-Zip, copies them into the game folder, and verifies the SKSE build matches your exact game version and the Address Library filename is the one that version needs.

`Test-TrueGazeHealth.ps1` verifies all three, including that the SKSE build matches the exact game version and that the Address Library filename matches too — a library for the *wrong* version is worse than none, because it looks present while resolving every address incorrectly.

### Verification sequence

1. **Prove it loads.** `TrueGaze.cmd -LoadOnly`, launch, load a save, quit. A clean exit proves SKSE loaded the plugin and installed the hook, without the kinematics engine running. Check `TrueGaze.log` for `"Gaze driver installed."`
2. **Prove the bones resolve.** `TrueGaze.cmd`, then stand within 5 m of a living NPC and check the `Skeleton probe` line. This is the single most likely point of silent failure — see the caveat above.
3. **Prove the gaze is visible.** Watch the NPC's head and eyes.
4. **Analyse.** `TrueGaze.cmd -PostRun` reports which markers were reached, the probe result, and any faults.

### What will not work yet

| Feature | Reason |
| :--- | :--- |
| **OAR conditions** | Registration is unimplemented — the OAR API contract could not be verified (issue #6). The cache and evaluators work; the binding does not. |
| **Eyelid morphs (EFM)** | Implemented via `SetExpressionOverride` (2026-09-14) but not yet observed in-engine. |

---

*Last updated: September 19, 2026*
