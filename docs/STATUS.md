# TrueGaze™ — Project Status

**Product:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine
**Version:** `1.0.0-rc1`
**Status date:** September 12, 2026
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

> **Nothing has yet reached ✅ In-engine verified.** The engine now compiles against the real SDK and drives bones, but no one has yet loaded it into Skyrim and watched an NPC's eyes move. That is the next milestone, and it is the only thing that can promote any row below to ✅.

---

## At a Glance

| | |
| :--- | :--- |
| **Overall maturity** | 🟡 **~65%** of a shippable 1.0.0 |
| **Installable & functional?** | 🟡 Builds and links the SDK; **not yet verified in-game** |
| **Does the gaze engine drive bones?** | ✅ Yes — implemented and compiled |
| **Hard blocker** | None. The former blocker (SDK not vendored) is resolved. |

**One-line summary:** *The drivetrain is built and turns. It has not yet been driven on a road.*

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
| Papyrus functions never registered | ✅ **Fixed** — 10 functions registered, 10-for-10 parity with `TrueGaze.psc` |
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
| True Main Sequence velocity *profile* (vs. smoothstep) | ✅ | ❌ | ❌ | ❌ |
| Micro-saccadic fixation drift | ✅ | ✅ | ✅ | ❌ |
| True Brownian (vs. mean-reverting) drift | ✅ | ❌ | ❌ | ❌ |
| Per-actor RNG seeding (vs. fixed `1337`) | — | ❌ | ❌ | ❌ |
| Social Triangle scanpath | ✅ | ✅ | ✅ | ❌ |
| Skeletal strain distribution | ✅ | ⚠️ | ⚠️ | ❌ |
| — *eye-node residual allocation* | ✅ | ❌ | ❌ | ❌ |
| Saccadic eyelid blink *curve* | ✅ | ❌ | ❌ | ❌ |
| Eyelid morph application (EFM) | ✅ | ❌ | ❌ | ❌ |

> ⚠️ **Known algorithmic defect:** `BoneController::CalculateHierarchyStrain` distributes `0.10 + 0.25 + 0.65 = 1.00` of the total deflection across Spine2/Neck/Head, leaving **nothing** for the eye nodes — `eyeYaw`/`eyePitch` are declared but never assigned. The eyes should receive the *residual* (`target − head_total`). This is a correctness bug in the project's signature feature.

### Engine Integration *(the actual product)*

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| **Bone transform application** | ✅ | ❌ | ❌ | ❌ |
| Post-Havok animation hook install | ✅ | ❌ | ❌ | ❌ |
| Per-actor runtime state | ✅ | ❌ | ❌ | ❌ |
| Actor eligibility filtering | ✅ | ✅ | ❌ | ❌ |
| Target salience resolution | ✅ | ✅ | ❌ | ❌ |
| Spatial LOD tiering | ✅ | ✅ | ✅ | ❌ |
| LOD thresholds read from config | ✅ | ✅ | ❌ | ❌ |
| Frame-budget profiling | ✅ | ✅ | ❌ | ❌ |
| Exception guard at hook boundary | ✅ | ❌ | ❌ | ❌ |
| Multi-threaded evaluation | ✅ | ❌ | ❌ | ❌ |
| Skyrim VR HMD pose | ✅ | ✅ | ❌ | ❌ |

> ✅ `ActorEligibility` now genuinely checks liveness, sleep, paralysis and ragdoll state, because the SDK is present and the `#if __has_include(<RE/Skyrim.h>)` branch is live.

> ⚠️ **Not yet done:** the tick is not wrapped in `try/catch`. NFR-4 requires that no exception reaches the game loop. This is a small, high-value change and is tracked as an open item.

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
| **Telemetry actually consumed by the engine** | ✅ | ⚠️ | ❌ | ❌ |
| Mutual gaze detection | ✅ | ❌ | ❌ | ❌ |
| Bidirectional feedback to HCEP Desktop | ✅ | ✅ | ✅ | ❌ |

> ✅ **Concurrency defect fixed.** The buffer is now a genuine triple buffer: three slots guarantee the writer can never select the slot the reader is consuming, with a publish epoch so a reader that is overtaken simply retries. `_pipeHandle` is `std::atomic<void*>` and is only ever dereferenced on the worker thread — `SendFeedback` now enqueues onto a lock-free ring that the worker drains. The bridge integration test shows no frame duplication, which it did before.

> ✅ **Law 6 partially addressed.** The pipe is created with an explicit security descriptor (`D:(A;;GA;;;OW)`) restricting access to the creating user, instead of the default DACL. Telemetry older than 500 ms is rejected, so a stalled HCEP Desktop cannot drive NPCs from frozen data.

> ⚠️ **Still open:** the payload is not encrypted in transit, there is no per-connection audit log, and `trackedPersonId` is still transmitted. See `GOVERNANCE.md` and issue #7.

### Modding Ecosystem

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| OAR condition *evaluators* | ✅ | ✅ | ❌ | ❌ |
| OAR condition *registration* | ✅ | ❌ | ❌ | ❌ |
| OAR condition state *publishing* | ✅ | ✅ | ❌ | ❌ |
| OAR rule package (`config.json`) | ✅ | ✅ | — | ❌ |
| Papyrus native function registration | ✅ | ✅ | ❌ | ❌ |
| Papyrus ↔ native signature match | ✅ | ✅ | — | — |
| Public C API surface (exports) | ✅ | ✅ | — | ❌ |
| Public C API *behaviour* | ✅ | ✅ | ❌ | ❌ |
| Papyrus MODE / REGION constants | ✅ | ✅ | — | — |

> ✅ **State publishing fixed.** `PublishActorState()` is called every tick by `GazeEngine`, so the condition cache holds live data. The evaluators now return `false` for an actor with no published state, instead of the previous default that made Rule 1 fire unconditionally.

> ⚠️ **OAR registration is still not implemented, and now says so.** `RegisterWithOar()` logs a `warn` stating that no OAR API binding exists, and returns `false`. The previous implementation logged success for work it did not perform — a Law 7 violation. The exact OAR plugin API contract could not be verified from available sources, and guessing it would repeat the original mistake. Tracked as issue #6.

> ✅ **Papyrus fixed.** 10 functions are registered via `SKSE::GetPapyrusInterface()->Register(...)`, with **10-for-10 name parity** against `TrueGaze.psc`. The previous mismatch (6 declared, 0 registered, signatures disagreeing) would have raised a VM error at every call site.

### Configuration & Localisation

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| `TrueGaze.ini` schema + defaults | ✅ | ✅ | — | — |
| INI *parsing* (`ConfigManager`) | ✅ | ✅ | ❌ | ❌ |
| INI *invoked on the real plugin path* | ✅ | ✅ | ❌ | ❌ |
| Config values consumed by simulation | ✅ | ✅ | ❌ | ❌ |
| Out-of-range values clamped and reported | — | ✅ | ❌ | ❌ |
| MCM Helper JSON schema | ✅ | ✅ | — | — |
| MCM backing plugin form (`TrueGaze.esp`) | ✅ | ❌ | ❌ | ❌ |
| SkyUI `SKI_ConfigBase` Papyrus script | ✅ | ✅ | — | ❌ |
| Compiled Papyrus (`.pex`) distribution | ✅ | ❌ | ❌ | ❌ |
| 6-language MCM localisation | ✅ | ✅ | — | — |

> ✅ **Configuration now reaches the simulation.** `ConfigManager::Load()` is called on `kDataLoaded`, `kPreLoadGame`, `kNewGame` and `kPostLoadGame`. `GazeEngine::RefreshTuning()` snapshots it into a `GazeTuning` that every kinematics call consumes, so a value in `TrueGaze.ini` has exactly one path to the mathematics. `Sanitise()` clamps every value into its supported range and logs any change, so a bad INI cannot produce nonsense physics.

> ⚠️ **Still open:** the MCM schema declares `"sourceForm": "TrueGaze.esp"` on every entry, but no such plugin exists in this repository, and no `.pex` scripts ship. The MCM has nothing to bind to. Tracked as issue #2.

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
- ✅ 10 Papyrus functions registered with 10-for-10 name parity against `TrueGaze.psc`
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
- ✅ Correct Skyrim mod package structure (MCM, translations, OAR, SKSE)
- ✅ Consistent `noexcept` discipline across simulation code

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

### Phase 2 — Make It Move *(1–2 weeks)* 🔴 **CRITICAL**

- [ ] Introduce `TrueGaze::Core::GazeEngine` singleton
- [ ] Add per-actor `ActorGazeRuntime` state + map with eviction
- [ ] Install the real animation hook via `REL::Relocation`
- [ ] Implement the per-actor tick: target → kinematics → **bone write**
- [ ] Fix the `BoneController` eye-residual allocation bug
- [ ] Wrap in frame timer + `try/catch`

### Phase 3 — Make It Correct *(1 week)*

- [ ] Fix the double-buffer race (triple-buffer or seqlock)
- [ ] Make the pipe handle atomic
- [ ] Implement the true Main Sequence velocity profile
- [ ] Add the eye-lead latency gap
- [ ] True Brownian drift with per-actor RNG seeding
- [ ] Plumb `ConfigManager` into the simulation
- [ ] Call `ConfigManager::Load()` on the correct path
- [ ] Converge the dual init paths in `Main.cpp`

### Phase 4 — Make It Ecosystem-Real *(1–2 weeks)*

- [ ] Implement genuine OAR registration via SKSE messaging
- [ ] Publish actor state to the OAR cache each tick
- [ ] Remove the false success log in `RegisterWithOar`
- [ ] Implement Papyrus registration with matching signatures
- [ ] Implement the real `TrueGazeAPI` bodies
- [ ] Enable & correct `EfmBlinkController::ApplyMorphs`
- [ ] Create `TrueGaze.esp` with MCM globals — or drop MCM Helper for INI
- [ ] Compile Papyrus to `.pex`; include in package
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

*Last updated: September 11, 2026*
