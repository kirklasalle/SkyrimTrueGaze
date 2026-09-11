# TrueGaze™ — Project Status

**Product:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine
**Version:** `1.0.0-rc1`
**Status date:** September 11, 2026
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

> **Every item in this project currently maxes out at 🧪 Unit-verified. Nothing has yet reached ✅ In-engine verified.**

---

## At a Glance

| | |
| :--- | :--- |
| **Overall maturity** | 🔴 **~30%** of a shippable 1.0.0 |
| **Installable & functional?** | ❌ Not yet |
| **Does the gaze engine drive bones?** | ❌ No |
| **Hard blocker** | CommonLibSSE-NG is not vendored; the build silently degrades to a standalone skeleton |

**One-line summary:** *The engine block and dashboard are built and beautiful. There is no drivetrain.*

---

## Capability Matrix

### Biomechanical Kinematics Library

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| Main Sequence peak velocity `V_peak(θ)` | ✅ | ✅ | ✅ | ❌ |
| Main Sequence duration `D(θ)` | ✅ | ✅ | ✅ | ❌ |
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
| Actor eligibility filtering | ✅ | ⚠️ | ❌ | ❌ |
| Target salience resolution | ✅ | ✅ | ❌ | ❌ |
| Spatial LOD tiering | ✅ | ✅ | ✅ | ❌ |
| LOD thresholds read from config | ✅ | ❌ | ❌ | ❌ |
| Frame-budget profiling | ✅ | ⚠️ | ❌ | ❌ |
| Exception guard at hook boundary | ✅ | ❌ | ❌ | ❌ |
| Multi-threaded evaluation | ✅ | ❌ | ❌ | ❌ |
| Skyrim VR HMD pose | ✅ | ✅ | ❌ | ❌ |

> ⚠️ `ActorEligibility` compiles but in the current (SDK-less) build returns `true` for any non-zero FormID — it never actually checks liveness, ragdoll, or paralysis.

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
| True lock-free double buffering | ✅ | ❌ | ❌ | ❌ |
| Thread-safe pipe-handle access | — | ❌ | ❌ | ❌ |
| **Telemetry actually consumed by the engine** | ✅ | ❌ | ❌ | ❌ |
| Mutual gaze detection | ✅ | ❌ | ❌ | ❌ |
| Bidirectional feedback to HCEP Desktop | ✅ | ⚠️ | ✅ | ❌ |

> ⚠️ **Known concurrency defect:** the "lock-free" double buffer is not lock-free. `_packetBuffers[]` holds plain (non-atomic) 64-byte structs and `_readIndex` orders only the *index*, not the *payload* — so the writer can overwrite the slot the reader is mid-`memcpy` on. `_pipeHandle` is additionally written by the worker thread and read by the game thread with no synchronisation at all. Both are genuine data races. Fix: triple-buffer or seqlock, and make the handle atomic.

### Modding Ecosystem

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| OAR condition *evaluators* | ✅ | ✅ | ❌ | ❌ |
| OAR condition *registration* | ✅ | ❌ | ❌ | ❌ |
| OAR condition state *publishing* | ✅ | ❌ | ❌ | ❌ |
| OAR rule package (`config.json`) | ✅ | ✅ | — | ❌ |
| Papyrus native function registration | ✅ | ❌ | ❌ | ❌ |
| Papyrus ↔ native signature match | ✅ | ❌ | ❌ | ❌ |
| Public C API surface (exports) | ✅ | ✅ | — | ❌ |
| Public C API *behaviour* | ✅ | ❌ | ❌ | ❌ |
| Papyrus MODE / REGION constants | ✅ | ✅ | — | — |

> ⚠️ `OarConditions::RegisterWithOar()` currently logs a **success message for work it does not do**, and the state cache it reads from (`g_actorGazeCache`) is **never written to**. The result: the 7-rule OAR package fires Rule 1 unconditionally and Rules 2–7 never fire.

> ⚠️ `TrueGaze.psc` declares six `global native` functions, but no `SKSE::GetPapyrusInterface()->Register(...)` call exists anywhere, and the declared signatures do not match the exported `TrueGazeAPI.cpp` symbols. Calling these from Papyrus will raise a VM error.

### Configuration & Localisation

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| `TrueGaze.ini` schema + defaults | ✅ | ✅ | — | — |
| INI *parsing* (`ConfigManager`) | ✅ | ✅ | ❌ | ❌ |
| INI *invoked on the real plugin path* | ✅ | ❌ | ❌ | ❌ |
| Config values consumed by simulation | ✅ | ❌ | ❌ | ❌ |
| MCM Helper JSON schema | ✅ | ✅ | — | — |
| MCM backing plugin form (`TrueGaze.esp`) | ✅ | ❌ | ❌ | ❌ |
| SkyUI `SKI_ConfigBase` Papyrus script | ✅ | ✅ | — | ❌ |
| Compiled Papyrus (`.pex`) distribution | ✅ | ❌ | ❌ | ❌ |
| 6-language MCM localisation | ✅ | ✅ | — | — |

> ⚠️ `ConfigManager::Load()` is called **only in the `#else` fallback path** in `Main.cpp` — the path that cannot execute inside real Skyrim. On the actual plugin path, **no configuration is ever loaded**, so every INI setting and every MCM slider is inert.

> ⚠️ The MCM schema declares `"sourceForm": "TrueGaze.esp"` on every entry, but **no such plugin exists** in this repository. The MCM has nothing to bind to.

### Packaging & Distribution

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| Correct MO2/Vortex directory layout | ✅ | ✅ | — | — |
| Automated packaging script | ✅ | ⚠️ | — | — |
| Reproducible clean-machine build | ✅ | ❌ | ❌ | — |
| Debug symbols (`.pdb`) in package | ✅ | ❌ | — | — |
| CI build + test pipeline | — | ❌ | — | — |

> ⚠️ `PackageMod.ps1` hardcodes `$projectRoot = "D:\Projects\SkyrimTrueGaze"`, so it cannot run on any other machine, and it invokes `cmake --build` without first running the configure step.

### Cross-Engine

| Capability | Designed | Implemented | Unit-verified | In-engine |
| :--- | :---: | :---: | :---: | :---: |
| Unreal Engine 5 adapter | ✅ | 📐 | ❌ | ❌ |
| Unity C# P/Invoke bridge | ✅ | 📐 | ❌ | ❌ |
| Godot 4 GDExtension | ✅ | ❌ | ❌ | ❌ |

---

## The Root Cause

Nearly every functional gap above traces to **one** cause:

```
extern/CommonLibSSE-NG   →   DOES NOT EXIST
```

`CMakeLists.txt` guards both the SDK subdirectory and the link step:

```cmake
if(EXISTS ".../extern/CommonLibSSE-NG/CMakeLists.txt")
    add_subdirectory(extern/CommonLibSSE-NG)
endif()
if(TARGET CommonLibSSE::CommonLibSSE)
    target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE)
endif()
```

Both guards evaluate **false**, and CMake silently skips them. **The build succeeds and produces a valid DLL** — a DLL that contains no game-facing code.

The consequence cascades through the codebase. Every `#if __has_include(<RE/Skyrim.h>)` block resolves to its fallback:

| Guarded block | Result in the shipped binary |
| :--- | :--- |
| `AnimationHook::Install()` | Logs "Standalone mode" — installs no hook |
| `IsActorEligibleForGaze()` | Returns `true` for any non-zero FormID |
| `GetActorGazeWeight()` | Unconditionally returns `1.0f` |
| `TargetSelector::ResolveTarget()` | Returns hardcoded coordinates |
| `EfmBlinkController::ApplyMorphs()` | No-op |
| `OarConditions::RegisterWithOar()` | Logs success for work not performed |
| `VrController::IsSkyrimVr()` | Always `false` |

**This is why the previous roadmap read "100% Complete" while the artifact did 30%.** There was no failure signal — no error, no warning, no red build. A CI pipeline would have reported green.

---

## Verified Working Today

Credit where due — these are real and correct:

- ✅ SKSE plugin exports: `SKSEPlugin_Load`, `SKSEPlugin_Query`, `SKSEPlugin_Version`, and four `TrueGaze_*` C API symbols
- ✅ 64-byte / 32-byte wire protocol with compile-time `static_assert` size guards — exemplary practice
- ✅ CRC-32 verify-before-publish in the pipe worker
- ✅ Correct DoS guard in the pipe read loop (never reads without a full packet available)
- ✅ Asynchronous pipe server with auto-reconnect
- ✅ Biomechanical kinematics library — mathematically correct and correctly cited (Bahill/Clark/Stark 1975, Baloh et al. 1975, Argyle & Cook 1976, Glenberg et al. 1998)
- ✅ 8-suite standalone unit test harness, all passing
- ✅ Integration test harness for the IPC bridge
- ✅ Correct Skyrim mod package structure (MCM, translations, OAR, SKSE)
- ✅ Well-designed 7-rule OAR condition package
- ✅ Consistent `noexcept` discipline across simulation code

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
