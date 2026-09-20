# TrueGaze™ — Independent Technical Audit & Market Assessment

> [!NOTE]
> **HISTORICAL DOCUMENT — RETAINED FOR EVIDENCE.**
>
> This audit records the state of the project on **September 11, 2026**, before the
> engine was implemented. Findings below are preserved verbatim as the historical
> record; several have since been fixed or made moot.
>
> **Most relevant here:** the entire **SkyUI / Papyrus / MCM layer was removed by
> design decision on 2026-09-14.** TrueGaze is a **vanilla-UI** mod: configuration
> lives in `Data\SKSE\Plugins\TrueGaze.ini` and is edited through the standalone
> **`TrueGazeConfig.html`** page (launched via `Launch-TrueGazeConfig.cmd`). There
> is **no ESP, no `.psc`/`.pex`, no MCM menu, no SkyUI dependency, and no in-game
> settings menu.** Every finding in this document that concerns MCM, Papyrus,
> `SKI_ConfigBase`, `sourceForm`, translations, or the ESP (notably **C-4**,
> **C-9**, **C-11**, and the Phase 4 checklist items) is therefore **MOOT** —
> superseded by the vanilla-UI architecture, not merely unfixed.
>
> **Current status:** see [`STATUS.md`](STATUS.md) and the later audits
> ([`AUDIT_REPORT_2026-09-15.md`](AUDIT_REPORT_2026-09-15.md),
> [`AUDIT_REPORT_2026-09-17.md`](AUDIT_REPORT_2026-09-17.md)).

**Project:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine
**Repository:** `D:\Projects\SkyrimTrueGaze`
**Auditor:** GitHub Copilot (DeepSeek V4.1 Flash)
**Audit Date:** September 11, 2026
**Artifact Version Audited:** `1.0.0-rc1`
**Audit Type:** Full-documentation review, full-source review, build/artifact forensics, competitive market analysis

---

## 0. How This Audit Was Conducted

| Phase | Method | Scope |
| :--- | :--- | :--- |
| Documentation review | Full read | 11 documents: `README.md`, `PRD.md`, `ROADMAP.md`, `CHANGELOG.md`, `TRUEGAZE_ARCHITECTURE.md`, `docs/*` (4 files), `Permanent_Active_Directives.txt`, `.nexus`, `LICENSE` |
| Source review | Full read | 18 source files across `src/Kinematics`, `src/Engine`, `src/Integrations`, `src/Bridge`, plus `tests/` |
| Asset review | Full read | `skyrim/SKSE/Plugins/TrueGaze.ini`, OAR `config.json`, `TrueGazeConfig.html` *(MCM `config.json`, translations and Papyrus scripts were reviewed at the time but have since been removed — see the note at the top of this document)* |
| Build forensics | Binary inspection | `TrueGaze.dll` PE header/export string scan, `CMakeCache.txt` inspection, `.obj` inventory, distribution `.zip` entry listing |
| Market research | Web research | Nexus Mods landscape, UE5/MetaHuman gaze ecosystem, commercial eye-tracking (Tobii, Eyeware Beam), academic saccade-modelling literature, creator-platform tooling |

**Verification note:** Every claim in this report marked **[VERIFIED]** was independently confirmed by direct inspection of the artifact during this audit. Claims marked **[CLAIMED]** come from project documentation and could not be substantiated. This distinction is the single most important lens for reading this report.

---

## 1. Executive Summary

### 1.1 What Was Found

TrueGaze is an **excellent piece of technical design work attached to an over-claimed delivery narrative.**

The scientific architecture is genuinely strong and, in several respects, more sophisticated than anything found in the public Skyrim modding ecosystem. The kinematics mathematics is correct, well-cited, and defensible. The wire protocol is well-specified. The documentation is unusually thorough.

However, the project is **nowhere near the "100% Complete / 100% Verified" state** that `ROADMAP.md` and `docs/SkyrimTrueGaze - Complete System Walkthrough & Audit Report` assert. The core product — an engine that makes NPCs' eyes move — **does not yet exist as executable behaviour.** Every simulation module is written, tested, and correct; but the module that would *use* them, the module that would *load configuration*, and the module that would *write to a bone* are absent.

The project has built an impeccable engine block and a beautiful dashboard. There is no drivetrain.

### 1.2 The Headline Finding

> **`LodManager.hpp` is the only one of the six core simulation modules that is referenced anywhere outside the test suite. The other five — `SaccadeGenerator`, `VorCoordinator`, `MicroJitter`, `SocialTriangle`, `BoneController` — are never invoked by production code. Even `LodManager` is only nominally referenced, never actually called.**

In plain terms: you can install `TrueGaze.dll` in Skyrim today. It will load. It will write a log file. It will host a named pipe. **No NPC's eyes will move.**

### 1.3 Maturity Scorecard

| Dimension | Assessed Level | Notes |
| :--- | :---: | :--- |
| Scientific / algorithmic foundation | **90%** | Genuinely strong; correct equations, good citations |
| Documentation & specification | **85%** | Thorough and well-structured; but materially over-claims status |
| Mod-packaging & configuration assets | **80%** | Complete and valid; minor correctness defects |
| Native build & toolchain | **65%** | Compiles *without* its stated SDK; skeleton-only binary |
| Engine integration (the actual product) | **5%** | Hooks declared but not installed; no bone writes |
| Runtime wiring / data flow | **8%** | Modules exist; nothing connects them |
| Test coverage of *delivered* behaviour | **15%** | Tests pass, but test only unused code |
| Release readiness | **10%** | Not installable-and-functional by any definition |

**Overall assessed maturity: ~30% of a shippable 1.0.0.** The documentation asserts 100%.

### 1.4 The Three Findings That Matter Most

1. **The engine is inert.** No bone is ever written. This is the product. It is missing. (§4.1)
2. **`TrueGaze.dll` is built without CommonLibSSE-NG.** The SDK directory does not exist. The shipping binary is a standalone skeleton that has never touched Skyrim's memory. (§4.2)
3. **Configuration is never loaded in the real plugin path, and every INI setting is inert.** *(Historical — since fixed. The MCM/`sourceForm` half of this finding is **moot**: the MCM layer was removed 2026-09-14 and TrueGaze is now vanilla-UI, configured through the INI and `TrueGazeConfig.html`.)* (§4.4, §4.5)

### 1.5 What This Report Recommends

The good news is that this is a **much better position than it looks.** The hard, creative, research-intensive work — the biology, the maths, the protocol design, the ecosystem strategy — is done and done well. What remains is engineering: connecting written components, making some architectural decisions that need making, and executing a verification loop. That is a focused, tractable body of work.

A five-phase remediation plan is proposed in §8, sequenced so that each phase produces a **demonstrable, testable artifact**.

---

## 2. Project Inventory — What Actually Exists

### 2.1 Source Tree (18 files)

**`src/Kinematics/` — 4 headers, header-only**

| File | Lines | Purpose | Invoked by production code? |
| :--- | ---: | :--- | :---: |
| `SaccadeGenerator.hpp` | ~100 | Main Sequence velocity/duration equations, ballistic state machine | ❌ **No** |
| `VorCoordinator.hpp` | ~75 | Eye-head decoupling, VOR counter-rotation | ❌ **No** |
| `MicroJitter.hpp` | ~60 | Brownian fixation drift | ❌ **No** |
| `SocialTriangle.hpp` | ~90 | Argyle & Cook eye-mouth-eye cycling | ❌ **No** |

**`src/Engine/` — 8 files**

| File | Lines | Purpose | Status |
| :--- | ---: | :--- | :--- |
| `BoneController.hpp` | ~45 | Skeletal strain distribution | ⚠️ **Never invoked** |
| `LodManager.hpp` | ~30 | 3-tier distance culling | ⚠️ **Never invoked** |
| `PerformanceProfiler.hpp` | ~48 | Frame budget monitor | ⚠️ **Never invoked** |
| `AnimationHook.hpp` / `.cpp` | ~35 / ~105 | Post-Havok hook + eligibility | 🔴 **Hook not installed** |
| `TargetSelector.hpp` / `.cpp` | ~40 / ~95 | Salience resolution | 🔴 **Never invoked** |
| `ConfigManager.hpp` / `.cpp` | ~60 / ~110 | INI loader | 🟡 **Works — but only in fallback path** |
| `TrueGazeAPI.cpp` | ~45 | Public C API | 🔴 **All stubs, hardcoded values** |
| `VrController.hpp` / `.cpp` | ~45 / ~60 | VR HMD pose | 🔴 **Never invoked** |

**`src/Integrations/` — 4 files**

| File | Lines | Purpose | Status |
| :--- | ---: | :--- | :--- |
| `OarConditions.hpp` / `.cpp` | ~40 / ~65 | OAR condition callbacks | 🔴 **Registration not implemented; cache never populated** |
| `EfmBlinkController.hpp` / `.cpp` | ~55 / ~30 | Saccadic eyelid sync | 🔴 **Morph writes commented out** |

**`src/Bridge/` — 3 files**

| File | Lines | Purpose | Status |
| :--- | ---: | :--- | :--- |
| `TelemetryPacket.h` | ~65 | Wire protocol | ✅ **Correct — static_asserts pass** |
| `NamedPipeServer.hpp` / `.cpp` | ~48 / ~190 | Async IPC server | 🟡 **Functional; never instantiated in real path; racy design** |

**`src/` root**

| File | Lines | Purpose | Status |
| :--- | ---: | :--- | :--- |
| `Main.cpp` | ~185 | Entry point | ⚠️ **Two divergent init paths; see §4.4** |
| `PCH.h` | ~40 | Precompiled header | 🟡 **Contains `#include <numbers>` etc. that must be pre-included — see §5.2** |

### 2.2 Assets — All Present and Valid

| Asset | Status | Notes |
| :--- | :---: | :--- |
| `skyrim/SKSE/Plugins/TrueGaze.ini` | ✅ | Well-commented, sensible defaults. **The sole configuration surface (vanilla UI).** |
| `TrueGazeConfig.html` (repo root) | ✅ | Standalone vanilla-UI configurator for the INI; launch via `Launch-TrueGazeConfig.cmd` |
| `skyrim/Interface/MCM/Config/TrueGaze/config.json` | ⛔ | **REMOVED 2026-09-14** — MCM layer deleted (vanilla UI) |
| `skyrim/Interface/Translations/*.txt` (6 lang) | ⛔ | **REMOVED 2026-09-14** — MCM-only translations, no longer shipped |
| `skyrim/scripts/source/TrueGaze.psc` | ⛔ | **REMOVED 2026-09-14** — Papyrus layer deleted |
| `skyrim/scripts/source/TrueGaze_MCM.psc` | ⛔ | **REMOVED 2026-09-14** — Papyrus/MCM layer deleted |
| `skyrim/meshes/.../OAR/TrueGaze/config.json` | ✅ | 7 well-designed rules |
| `include/TrueGazeAPI.h` | ✅ | Clean public API surface |
| `extern/cross-engine/TrueGazeUE5.h` | 🟡 | Declaration only |
| `extern/cross-engine/TrueGazeUnity.cs` | 🟡 | Present (not reviewed in depth) |

### 2.3 Build Artifacts

```
build/windows-release/TrueGaze.dir/Release/
  AnimationHook.obj  ConfigManager.obj  EfmBlinkController.obj  Main.obj
  NamedPipeServer.obj  OarConditions.obj  TargetSelector.obj
  TrueGazeAPI.obj  VrController.obj  cmake_pch.obj / .pch

skyrim/SKSE/Plugins/TrueGaze.dll   42.5 KB   (built 2026-09-11 17:13)
dist/TrueGaze-v1.0.0-rc1-SkyrimSE-AE-VR.zip
```

**Observation:** Exactly 8 translation units compiled. There is **no `.obj` for any Kinematics module** — confirming they are header-only and, more importantly, that **they contribute zero code to the DLL.**

---

## 3. Verified Binary Forensics — The Decisive Evidence

This section contains findings obtained by directly inspecting the shipped binary. These are not opinions.

### 3.1 Export Table — Claims Substantiated ✅

String-scanning `skyrim/SKSE/Plugins/TrueGaze.dll` (42.5 KB, 2026-09-11 17:13):

| Export | Found? |
| :--- | :---: |
| `SKSEPlugin_Load` | ✅ |
| `SKSEPlugin_Query` | ✅ |
| `SKSEPlugin_Version` | ✅ |
| `TrueGaze_GetVersion` | ✅ |
| `TrueGaze_IsHcepConnected` | ✅ |
| `TrueGaze_GetActorGaze` | ✅ |
| `TrueGaze_OverrideActorMode` | ✅ |

**The audit report's "7 Exports Verified" claim is accurate.** This is real, working export plumbing and deserves credit.

### 3.2 Dependency Forensics — A Critical Problem 🔴

| Symbol searched | Result | Interpretation |
| :--- | :---: | :--- |
| `CommonLibSSE` | **MISSING** | **SDK is not linked into the binary** |
| `SKSE` | FOUND | Matches own struct names / log strings |
| `spdlog` | **MISSING** | Logging is compiled out (fallback stub) |
| `fmt` | **MISSING** | vcpkg deps not linked |
| `VCRUNTIME` | FOUND | Standard MSVC runtime |
| `MSVCP` | FOUND | Standard MSVC C++ runtime |
| `KERNEL32` | FOUND | Win32 kernel |
| `ADVAPI32` | **MISSING** | Not statically named |

Additionally:

```
extern/CommonLibSSE-NG  →  NO - CommonLibSSE-NG NOT PRESENT
```

And in `CMakeLists.txt`:

```cmake
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibSSE-NG/CMakeLists.txt")
    add_subdirectory(extern/CommonLibSSE-NG)
endif()
...
if(TARGET CommonLibSSE::CommonLibSSE)
    target_link_libraries(${PROJECT_NAME} PRIVATE CommonLibSSE::CommonLibSSE)
endif()
```

**Both guards evaluate false.** The build silently degrades to a standalone skeleton.

**Consequence:** Every `#if __has_include(<RE/Skyrim.h>)` in the codebase resolves to `false`. The entire game-facing half of the code compiles to fallback stubs. This means:

- `AnimationHook::Install()` executes only `spdlog::info("[TrueGaze] Standalone mode...")`
- `IsActorEligibleForGaze()` returns `true` for *any* non-zero FormID
- `GetActorGazeWeight()` unconditionally returns `1.0f`
- `TargetSelector::ResolveTarget()` returns hardcoded coordinates `(0, 150, 160)`
- `EfmBlinkController::ApplyMorphs()` does nothing
- `OarConditions::RegisterWithOar()` logs a *success message* for work it did not do
- `VrController::IsSkyrimVr()` returns `false`; `GetHmdPose()` returns `isValid = false`

**This is the root cause of nearly every functional gap in this report.** It is also the most fixable finding: the architecture is right, the include guards are right, the code is right. The SDK just needs to actually be there.

### 3.3 Distribution Archive — Correct Structure ✅

`dist/TrueGaze-v1.0.0-rc1-SkyrimSE-AE-VR.zip` entry listing **at the time of this
audit** confirmed a correctly laid out Skyrim mod package:

```
Interface\MCM\Config\TrueGaze\config.json
Interface\Translations\TrueGaze_{CHINESE,ENGLISH,FRENCH,GERMAN,JAPANESE,SPANISH}.txt
meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze\config.json
scripts\source\TrueGaze.psc
scripts\source\TrueGaze_MCM.psc
SKSE\Plugins\TrueGaze.dll
SKSE\Plugins\TrueGaze.ini
Source\Scripts\TrueGaze.psc          ← duplicate
Source\Scripts\TrueGaze_MCM.psc      ← duplicate
```

> **⚠️ SUPERSEDED (2026-09-14):** the MCM/Papyrus/translation entries above were
> removed with the MCM layer. The package now contains only
> `SKSE\Plugins\TrueGaze.dll`, `SKSE\Plugins\TrueGaze.ini`, and the OAR
> `meshes\...\OpenAnimationReplacer\TrueGaze\config.json`. The findings below are
> retained as the historical record; the `.psc`/`.pex`/`TrueGaze.esp` items are
> **moot**.

**Findings (historical):**

- ✅ Directory structure was correct for MO2/Vortex
- ⚠️ `Source\Scripts\` duplicated `scripts\source\` — harmless but unnecessary bloat *(both removed)*
- ⛔ ~~🔴 **No `.pex` compiled scripts** — the Papyrus scripts ship as `.psc` source only, so **SkyUI will find no MCM script to run**~~ — **MOOT (2026-09-14):** Papyrus removed entirely; TrueGaze is vanilla-UI.
- ⛔ ~~🔴 **No `TrueGaze.esp`**~~ — **MOOT (2026-09-14):** no ESP is needed or wanted; configuration is INI-only via `TrueGazeConfig.html`.
- 🔴 **No `SKSE\Plugins\TrueGaze.pdb`** — no crash symbolication for users *(still open)*

---

## 4. Critical Findings

Each finding is rated by severity. **Severity reflects impact on the product's stated purpose**, not implementation effort.

---

### 🔴 C-1 — The Core Product Does Not Exist: No Bone Is Ever Written

**Severity: BLOCKER**

**Evidence.** A workspace-wide search for invocations of the kinematics modules returns:

```
src/Kinematics/SaccadeGenerator.hpp : class SaccadeGenerator       ← definition only
src/Kinematics/VorCoordinator.hpp   : class VorCoordinator         ← definition only
src/Kinematics/MicroJitter.hpp      : class MicroJitter            ← definition only
src/Kinematics/SocialTriangle.hpp   : class SocialTriangle         ← definition only
src/Engine/BoneController.hpp       : CalculateHierarchyStrain     ← definition only
```

The only files referencing them are `tests/KinematicsTests.cpp`.

The single place where bone rotation would be applied is `AnimationHook.cpp`:

```cpp
static void Hook(RE::Actor* a_actor, float a_delta)
{
    _original(a_actor, a_delta);
    if (!a_actor || !AnimationHook::IsActorEligibleForGaze(a_actor->GetFormID())) {
        return;
    }
    // Procedural additive bone rotation would be applied here after Havok evaluation
}
```

The comment literally states the product is unimplemented. Further, this `Hook` function cannot even be reached, because `Install()` is:

```cpp
void AnimationHook::Install() noexcept
{
#if __has_include(<RE/Skyrim.h>)
    logger::info("[TrueGaze] Installing post-Havok animation hooks...");
    // Future: Relocation installation when running within Skyrim process address space
#else
    spdlog::info("[TrueGaze] Standalone mode: AnimationHook compiled with abstract engine interface.");
#endif
}
```

**No `REL::Relocation` is constructed. No trampoline is written. No hook is installed.**

**Impact.** `TrueGaze.dll` can be installed into Skyrim today. It will load, log, and host a pipe. **Every NPC in the game will behave exactly as vanilla.** The entire value proposition — "eradicating dead-eye zombie syndrome" — is absent from the shipped artifact.

**This finding alone invalidates the "Phase 2: Completed (100%)" claim** in `ROADMAP.md`, along with the audit matrix's "Implemented & Built" status for `AnimationHook.cpp`.

**Remediation.** See §8 Phase 2. Requires: (a) CommonLibSSE-NG present, (b) correct hook target identified, (c) `BoneController` output written to `NiNode` local transforms, (d) a per-actor state cache to hold saccade/VOR/jitter state across frames.

---

### 🔴 C-2 — Built Without Its Stated SDK; Entire Game-Facing Half Is Dead Code

**Severity: BLOCKER**

Fully evidenced in §3.2 above.

**The subtlety that makes this dangerous:** the build *succeeds*. `CMakeLists.txt` guards both the `add_subdirectory` and the `target_link_libraries` with `if(EXISTS)` / `if(TARGET)`. When the SDK is absent, CMake quietly skips both and produces a valid DLL. There is no error, no warning, no failed build. A CI pipeline would report green.

Every failure in this audit that is described as "compiles but does nothing" traces directly back to this.

**Remediation.** Make the SDK mandatory:

```cmake
if(NOT TARGET CommonLibSSE::CommonLibSSE)
    message(FATAL_ERROR "CommonLibSSE-NG not found. Run: git submodule update --init --recursive")
endif()
```

Then add it as a proper git submodule and document the bootstrap step. Consider also a `#error` in `PCH.h` when `<RE/Skyrim.h>` is unavailable *and* `TRUEGAZE_STANDALONE` is not defined — so a misconfigured build can never silently ship a stub binary again.

---

### 🔴 C-3 — `ConfigManager::Load()` Is Never Called in the Real Plugin Path

**Severity: HIGH**

`Main.cpp` contains **two mutually exclusive initialisation paths**, and the configuration load exists in only one of them.

**Path A — the `SKSEPluginLoad` macro path** (used when CommonLibSSE-NG is present — i.e. the *real* plugin path):

```cpp
SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    InitializeLogging();
    SKSE::Init(a_skse);
    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging || !messaging->RegisterListener(MessageHandler)) { ... }
    return true;
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
    case SKSE::MessagingInterface::kDataLoaded:
        TrueGaze::Engine::AnimationHook::Install();
        TrueGaze::Integrations::OarConditions::RegisterWithOar();
        g_pipeServer = std::make_unique<TrueGaze::Bridge::NamedPipeServer>();
        g_pipeServer->Start();
        // ⚠️ ConfigManager::GetSingleton().Load()  ← ABSENT
}
```

**`ConfigManager::Load()` is not called here.** Not at all. Not anywhere in this path.

**Path B — the fallback `SKSEPlugin_Load` in the `#else` branch:**

```cpp
extern "C" __declspec(dllexport) bool SKSEPlugin_Load(const SKSEInterface*)
{
    InitializeLogging();
    TrueGaze::Engine::ConfigManager::GetSingleton().Load();   // ← called here
    ...
}
```

**Configuration is loaded only in the path that can never run inside real Skyrim.**

**Compounding consequence:** In Path A, `connectHcepBridge` is never consulted, so `Start()` is called unconditionally. Conversely, in Path B, the pipe is gated on a config value that *was* loaded. The two paths have opposite behaviour.

**Impact.** All 20+ settings in `TrueGaze.ini` — saccade speed, jitter amplitude, gaze aversion, mutual gaze threshold, LOD distances, debug rays — are **inert**. Users editing the INI will observe zero effect.

**Remediation.** Call `Load()` in `SKSEPluginLoad` before `SKSE::Init`, load it in `MessageHandler` on `kDataLoaded`, and gate `Start()` on `connectHcepBridge`. Then converge the two paths — the `#else` branch should be deleted entirely, or explicitly marked `TRUEGAZE_STANDALONE` and excluded from release builds.

---

### 🔴 C-4 — Papyrus "Native" Functions Are Not Registered — **MOOT (2026-09-14)**

> **Resolution:** The **entire Papyrus layer was removed by design decision on
> 2026-09-14.** TrueGaze is a vanilla-UI mod. `src/Integrations/PapyrusInterface.*`
> was deleted, `Main.cpp` no longer calls `RegisterFunctions`, and every `.psc`
> script (`TrueGaze.psc`, `TrueGaze_MCM.psc`, the `SKI_*` stubs) was removed from
> the repository. The modder-facing integration surface is now the **C/C++ public
> API** (`include/TrueGazeAPI.h`) and the **OAR condition** functions — not
> Papyrus. This finding is therefore **moot**: there is nothing left to register.

**Severity: HIGH** *(historical)*

Two Papyrus scripts declare `global native` functions, and the packaged export table confirms the DllExports exist:

`skyrim/scripts/source/TrueGaze.psc`:

```papyrus
int  function GetVersion() global native
bool function IsHcepConnected() global native
bool function IsMutualGaze(Actor akActor, float afDurationThreshold = 1.5) global native
int  function GetActorMode(Actor akActor) global native
ObjectReference function GetGazeTarget(Actor akActor) global native
function OverrideActorMode(Actor akActor, int aiMode, float afDurationSec) global native
```

However, a search across `src/` for `RegisterFunctions`, `Papyrus`, or `GetFormFromFile` returns:

```
Found 2 matches — both in src/Bridge/TelemetryPacket.h (the static_asserts)
```

**There is no `SKSE::GetPapyrusInterface()->Register(...)` call anywhere.**

Furthermore, the **signatures do not match**. `TrueGaze.psc` declares:

| Papyrus declaration | Native export in `TrueGazeAPI.cpp` | Match? |
| :--- | :--- | :---: |
| `IsMutualGaze(Actor, float)` → `bool` | *(no equivalent export)* | ❌ |
| `GetActorMode(Actor)` → `int` | *(no equivalent export)* | ❌ |
| `GetGazeTarget(Actor)` → `ObjectReference` | *(no equivalent export)* | ❌ |
| `GetVersion()` → `int` | `TrueGaze_GetVersion()` → `uint32_t` | ⚠️ name/sig differ |

**Impact.** Any mod author following your own documentation and calling `TrueGaze.IsMutualGaze(akActor)` will get a **Papyrus VM runtime error**. The entire modder-facing SDK promise (FR-10 adjacent) is non-functional.

**Remediation.** Implement `PapyrusInterface::RegisterFunctions()` with exact signature matching, register it in `SKSEPluginLoad`, and add a Papyrus-side integration test. Also add the missing binding for `TrueGaze_IsHcepConnected` (currently returns hardcoded `false` — see C-6).

---

### 🟠 C-5 — OAR Conditions Are Never Registered and Its Cache Is Never Populated

**Severity: HIGH**

`OarConditions.cpp`:

```cpp
bool OarConditions::RegisterWithOar() noexcept
{
#if __has_include(<SKSE/SKSE.h>)
    logger::info("[TrueGaze] Requesting Open Animation Replacer (OAR) API interface...");
    // Future: Dispatch OAR condition registrations via OAR_API interface
    logger::info("[TrueGaze] Registered TrueGaze_IsMode, ... conditions with OAR.");
    return true;
#endif
}
```

Two problems in one function:

1. **No registration occurs.** The registration is a `// Future:` comment, yet the very next line **logs a success message** and returns `true`. This is a correctness hazard: the log actively misleads anyone diagnosing why their OAR rules don't fire.

2. **`g_actorGazeCache` is never written to.** It is declared:

```cpp
std::shared_mutex g_cacheMutex;
std::unordered_map<uint32_t, ActorGazeInfo> g_actorGazeCache;
```

A search for any insertion or mutation of this map returns **nothing**. There is no `SetActorGazeInfo`, no writer, no exporter.

**Impact.** All three evaluators (`EvaluateIsMode`, `EvaluateIsMutualGaze`, `EvaluateGazeRegion`) always take their default path:

| Condition | Always returns |
| :--- | :--- |
| `EvaluateIsMode(id, 0)` | `true` |
| `EvaluateIsMode(id, 1..4)` | `false` |
| `EvaluateIsMutualGaze(id, any)` | `false` |
| `EvaluateGazeRegion(id, 0)` | `true` |
| `EvaluateGazeRegion(id, 1..12)` | `false` |

**Therefore your well-designed 7-rule OAR `config.json` will fire Rule 1 ("LOGIC mode") unconditionally whenever `IsTalking` is true, and Rules 2–7 will never fire.** The 5-mode HCEP behaviour system — a headline feature — is non-operational.

**Remediation.** Add a `PublishActorState()` writer called from the animation tick, and implement genuine OAR API registration via SKSE messaging. Critically: **remove the false success log** and replace it with `logger::warn` when OAR is absent.

---

### 🟠 C-6 — Public C API Returns Hardcoded Fiction

**Severity: MEDIUM-HIGH**

`TrueGazeAPI.cpp` — cited in `CHANGELOG.md` as delivering "Public Modding SDK":

```cpp
TRUEGAZE_API bool TrueGaze_IsHcepConnected() noexcept
{
    // Check pipe server status if active
    return false;                                              // ← always false
}

TRUEGAZE_API bool TrueGaze_GetActorGaze(uint32_t actorFormId, ActorGazeTelemetry* outTelemetry) noexcept
{
    if (!outTelemetry || actorFormId == 0) return false;
    outTelemetry->actorFormId = actorFormId;
    outTelemetry->targetFormId = 0x14;              // hardcoded player FormID
    outTelemetry->gazePitchDeg = 0.0f;              // hardcoded
    outTelemetry->gazeYawDeg = 0.0f;                // hardcoded
    outTelemetry->mutualGazeDurationSec = 0.0f;     // hardcoded
    outTelemetry->activeMode = HcepCognitiveMode::LOGIC;  // hardcoded
    outTelemetry->isMutualGaze = 0;                 // hardcoded
    outTelemetry->isBlinking = 0;                   // hardcoded
    outTelemetry->lodTier = 0;                      // hardcoded
    return true;   // ← reports success while returning no real data
}

TRUEGAZE_API void TrueGaze_OverrideActorMode(...) noexcept
{
    // Mode override logic for dialogue scripting
}
```

**Impact.** A third-party mod author integrating against your published SDK receives `true` (success) alongside entirely fabricated telemetry. They cannot distinguish "TrueGaze is working and the NPC is steadily fixating" from "TrueGaze is inert." Silent false-success is considerably worse than an honest failure.

**Also:** `g_pipeServer` lives in an anonymous namespace in `Main.cpp` and is unreachable from `TrueGazeAPI.cpp`, so even the *intent* of `IsHcepConnected` cannot be satisfied without an accessor.

**Remediation.** Introduce a singleton `TrueGaze::Core` (or `GazeEngine`) owning per-actor state and the pipe server, exposed via an accessor. Have this API read real state. Until then, return `false`/fail rather than fabricated success.

---

### 🟠 C-7 — Named Pipe Architecture Does Not Match Its Own Specification

**Severity: MEDIUM-HIGH**

Three discrepancies between `docs/HCEP_BRIDGE_SPEC.md` and `NamedPipeServer.cpp`:

| Spec (`HCEP_BRIDGE_SPEC.md` §2) | Implementation | Issue |
| :--- | :--- | :--- |
| `PIPE_TYPE_MESSAGE \| PIPE_READMODE_MESSAGE \| PIPE_NOWAIT` | `... \| PIPE_WAIT` | **Non-blocking mode not used** |
| Background I/O worker: "high-priority" | default priority | Priority never raised |
| Direction labelled "Server ↔ Client" with plugin as listener | `FILE_FLAG_FIRST_PIPE_INSTANCE` + max 1 instance | Cannot coexist with another TrueGaze instance; no instance reuse |

**More importantly, the actual concurrency model is not lock-free.** Despite `_readIndex` being `std::atomic` and the spec claiming "lock-free double buffering, < 10 ns":

```cpp
// Writer (worker thread):
_packetBuffers[writeIndex] = incoming;              // plain array write
_readIndex.store(writeIndex, std::memory_order_release);

// Reader (game thread):
std::memcpy(&outPacket, &_packetBuffers[index], sizeof(...));  // plain array read
```

The array element itself is a **plain, non-atomic** 64-byte struct. With only two buffers and a single reader, the writer can overwrite the buffer the reader is *currently memcpy-ing from* (two writes to the same slot before the reader reads). The release/acquire pair on `_readIndex` orders *the index*, not *the payload*. This is a genuine data race — subtle, frame-timing dependent, and exactly the class of bug that produces unreproducible "why did the NPC's head snap to a random angle" reports.

**A second race exists on `_pipeHandle`:** written by the worker thread (`_pipeHandle = hPipe;`) and read by the game thread in `SendFeedback()` (`if (... || !_pipeHandle)`) with **no synchronisation and not even atomicity**. `SendFeedback` is defined and documented as the game-thread path.

**Remediation.**

- Use a 3-buffer (triple-buffer) scheme, or make each slot atomic with a seqlock, or gate with a lightweight spinlock. Recommendation: triple buffer — simplest correct lock-free option for one-writer/one-reader.
- Make `_pipeHandle` a `std::atomic<void*>`; or better, move all pipe writes onto the worker thread via an outbound queue.
- Reconcile the document with reality: either implement `PIPE_NOWAIT` or update the spec. **Do not ship a spec that describes code you don't have.**

---

### 🟠 C-8 — Every Biological Parameter Is a Compile-Time Constant; Config Is Cosmetic

**Severity: MEDIUM-HIGH**

`SaccadeGenerator` is invoked with defaults everywhere:

```cpp
static float CalculatePeakVelocity(float amplitudeDeg,
                                   float vMax = DEFAULT_VMAX,   // 750.0f
                                   float c    = DEFAULT_C);     // 14.0f
```

`ConfigManager` *does* parse `fSaccadeSpeedMult` and `fVelocitySaturation` — but **nothing consumes them.** The tests call `CalculatePeakVelocity(30.0f)`, taking defaults. There is no call site passing configured values, because there is no call site at all (C-1).

The same applies to `microJitterAmp`, `headTrackingSpeed`, `maxComfortEyeAngle`, `triangleFixationDuration`, `tier1DistanceMeters`, `tier2DistanceMeters`.

**Even `LodManager`** — the one nominally-referenced engine module — hardcodes its thresholds:

```cpp
static LodTier GetLodTier(float distanceMeters) noexcept
{
    if (distanceMeters <= 5.0f) return LodTier::Tier1_DialogueRange;
    else if (distanceMeters <= 15.0f) return LodTier::Tier2_Proximity;
    return LodTier::Tier3_Culled;
}
```

`fTier1DistanceMeters` and `fTier2DistanceMeters` are parsed and discarded.

**Impact.** The MCM's "Saccade Velocity Multiplier" and "Micro-Saccadic Jitter Amplitude" sliders control nothing. Users who tune and observe no change will reasonably conclude the mod is broken — and they will be right.

**Remediation.** Plumb `ConfigManager` into the simulation by passing an immutable per-frame `GazeTuning` struct into the update functions, rather than relying on static defaults. Keep the static functions for testability, but add overloads taking explicit parameters and have the production path use those.

---

### ✅ C-9 — MCM Cannot Bind: No Plugin Form Exists — **MOOT (2026-09-14)**

> **Resolution:** The **entire MCM layer was removed by design decision on
> 2026-09-14.** TrueGaze is a **vanilla-UI** mod. There is no MCM menu, no MCM
> Helper config, no `sourceForm`, no ESP, and no SkyUI dependency. Configuration is
> the INI at `Data\SKSE\Plugins\TrueGaze.ini`, edited through the standalone
> **`TrueGazeConfig.html`** page. `ConfigManager` reads that INI directly. This
> finding is **moot**: there is no MCM left to bind.

**Original severity: MEDIUM-HIGH**

`skyrim/Interface/MCM/Config/TrueGaze/config.json` declares on **every** entry:

```json
"sourceType": "ModSettingBool",
"sourceForm": "TrueGaze.esp",
"setting": "bEnableTrueGaze:General"
```

A workspace file search for `**/*.esp` returns **"No files found."**

**Impact.** MCM Helper resolves `sourceForm: "TrueGaze.esp"` → `Game.GetFormFromFile(..., "TrueGaze.esp")`. With no such plugin, the lookup fails. The MCM page will render with options that cannot read or persist values.

**Related:** even with the ESP supplied, note that `TrueGaze.ini` keys use the `b`/`f`/`i` Hungarian prefix (`bEnableTrueGaze`, `fSaccadeSpeedMult`), and the MCM `setting` strings match — good. But these are **INI settings, not game settings**. Mod Setting records live on a `TESGlobal`-backed plugin form, not in the plugin's own INI. As written, MCM Helper will look for globals named `bEnableTrueGaze` in `TrueGaze.esp`, which is a separate mechanism from parsing `TrueGaze.ini` via `GetPrivateProfileString`. **The two configuration systems are parallel and unconnected.**

**Remediation (historical).** Decide on **one** authoring path. The cleanest for this project: create a minimal `TrueGaze.esp` (or an ESL-flagged ESP) containing the required globals for MCM Helper, and have `ConfigManager` read *those* — or alternatively drop the MCM Helper JSON route in favour of the pure-Papyrus `SKI_ConfigBase` script that already exists and write straight to INI. Splitting the difference, as now, guarantees both halves are broken.

> **✅ RESOLVED (2026-09-14) — the "one authoring path" chosen was: neither.**
> The MCM Helper JSON route and the Papyrus route were **both removed**. The single
> authoring path is now the **INI** (`Data\SKSE\Plugins\TrueGaze.ini`), read directly
> by `ConfigManager` and edited through the vanilla-UI **`TrueGazeConfig.html`** page.
> No ESP, no globals, no `sourceForm`, no Papyrus. The recommendation above is
> retained only as the historical record.

---

### 🟡 C-10 — `NamedPipeServer` Is Not `final`/`sealed` and Is Constructed Nowhere in the Real Path

**Severity: LOW-MEDIUM**

In `Main.cpp` Path A, `g_pipeServer` is created on `kDataLoaded` **unconditionally** — the `connectHcepBridge` config gate exists only in Path B (see C-3). A user who sets `bConnectHcepBridge=false` will still get a pipe server started.

Additionally, `NamedPipeServer` has **no accessor** available to `TrueGazeAPI.cpp` or the OAR condition publisher, so the inbound telemetry sitting in `_packetBuffers` is read by literally nobody. Combined with C-1, this means the entire **Mode 2 (Connected HCEP)** architecture — half the product's positioning — delivers data into a void.

---

### 🟡 C-11 — `TrueGaze_GetActorGaze` and Papyrus `GetGazeTarget` Refer to a Target Type That Has No Mapping — **MOOT (2026-09-14)**

> **Resolution:** The Papyrus `GetGazeTarget` declaration was removed with the rest
> of the Papyrus layer (see C-4). The native C API in `include/TrueGazeAPI.h`
> returns the target **FormID** directly, so no position→`ObjectReference` mapping
> is required. This finding is **moot**.

**Severity: LOW-MEDIUM** *(historical)*

`TargetSelector::GazeTarget` carries `worldX/worldY/worldZ` floats. `TrueGaze.psc` promises:

```papyrus
ObjectReference function GetGazeTarget(Actor akActor) global native
```

There is **no mechanism** converting a `GazeTarget` position into an `ObjectReference` (or `FormID`). `TargetSelector` does populate `targetFormId` internally, but **nothing exposes it**, and nothing bridges native ↔ Papyrus. The declared API cannot be implemented as specified without either an `ObjectReference` lookup by position (expensive, ambiguous) or a signature change to return `FormID`.

**Remediation.** Change the Papyrus signature to return `Form` or `int` (FormID) and map via `Game.GetFormFromFile`/`TESForm::LookupByID`. Update the docs to match.

---

### 🟡 C-12 — `TrueGaze.ini` Contains a Setting Nothing Reads

**Severity: LOW**

```ini
[General]
sEngineTarget=Auto
```

`ConfigManager` never reads `sEngineTarget`. Dead configuration is a documentation defect — it implies engine auto-detection exists.

Likewise `[Debug] iLogLevel` is parsed but never applied to the logger (which is set to `spdlog::level::info` unconditionally in `Main.cpp`).

---

## 5. Code-Quality & Correctness Findings

### 5.1 ✅ What Is Genuinely Well Done

Credit where due — these are real strengths:

1. **`TelemetryPacket.h` is exemplary.** `#pragma pack(push, 1)` with compile-time `static_assert` guards on both packet sizes. This is exactly the discipline binary protocols need. **Verified**: both asserts would hold.
2. **DoS guard in the pipe read loop** is correct and thoughtful:

   ```cpp
   DWORD bytesAvail = 0;
   if (!PeekNamedPipe(hPipe, nullptr, 0, nullptr, &bytesAvail, nullptr)) { ... }
   if (bytesAvail < sizeof(TrueGazeTelemetryPacket)) { sleep; continue; }
   ```

   Never reads without confirming a full packet is available. Correct.
3. **CRC-32 verify-before-publish.** The worker validates magic *and* checksum before writing to the buffer. Good hygiene.
4. **`noexcept` discipline** is consistent across the simulation modules — appropriate for code that must never propagate an exception into the game loop (NFR-4).
5. **`constexpr` biological constants** with attribution to Bahill/Clark/Stark (1975) and Baloh et al. (1975). This is scientific good practice.
6. **`ScopedTimer` RAII** in `PerformanceProfiler.hpp` is idiomatic and correct.
7. **Documentation quality** is far above typical modding-project standard. The `PRD.md` FR/NFR structure and `HCEP_BRIDGE_SPEC.md` wire tables are professional.

### 5.2 🟡 The `PCH.h` Hidden-Dependency Hazard

`PCH.h` includes `<cmath>` and `<numbers>`. But `src/Kinematics/*.hpp` are **header-only files that are not part of the PCH-consuming translation-unit set in a standalone context**, and `tests/KinematicsTests.cpp` includes them directly:

```cpp
#include "../src/Kinematics/SaccadeGenerator.hpp"
```

`SaccadeGenerator.hpp` itself includes `<cmath>`, `<numbers>`, `<algorithm>` — **good**. But `SocialTriangle.hpp` uses `std::max` and includes only `<cstdint>` and `<random>`:

```cpp
#include <cstdint>
#include <random>
...
float dist = std::max(0.5f, faceDistanceMeters);   // std::max needs <algorithm>
```

This compiles only because some earlier include dragged in `<algorithm>`. Same class of latent fragility in `EfmBlinkController.hpp` and others. **Every header must include what it uses (IWYU).** Add `<algorithm>` to `SocialTriangle.hpp`.

### 5.3 🟡 Kinematics Module API Design

Three design observations, in decreasing importance:

**(a) `TriggerSaccade` reads `state.currentYaw` before it is ever set.** In `SaccadeGenerator::TriggerSaccade`, the update path uses `state.currentYaw`, which is only ever set by `Update()`. A freshly default-constructed `SaccadeState` has it as `0.0f`, which happens to be correct — but only by accident. There is no initialisation function and no documented invariant. Add `void Reset(SaccadeState&, float yaw, float pitch)`.

**(b) The Main Sequence equation is computed and then discarded.** `CalculatePeakVelocity` is called in tests and **nowhere else**. The actual trajectory is a `smoothstep` (`t*t*(3-2t)`) — a cubic ease, not a Main Sequence velocity profile. The docs claim `V_peak = V_max(1 - e^(-θ/c))` drives the motion. It does not; the velocity profile is smoothstep. The `V_peak` function is decorative.

To actually honour the science: the ballistic phase should integrate a velocity profile whose peak equals `V_peak(θ)`, and whose **duration** equals `D_0 + d·θ`. Currently `totalDurationSec` is set from `CalculateDuration` (correct!) but the *shape* within it is a generic ease. This is a **fidelity gap between the documentation's central scientific claim and the implementation.** For a project whose entire differentiator is scientific grounding, this matters.

**(c) `MicroJitter` is not Brownian.** The docs (and the HCEP handover) describe "Brownian random-walk drift." The implementation is:

```cpp
if (timer >= nextInterval) {
    targetYawOffset = uniform(-max, +max);     // independent resample, not a walk
    nextIntervalSec = uniform(0.2, 0.45);
}
currentYawOffset += (target - current) * alpha;   // exponential smoothing
```

This is **white noise passed through a low-pass filter** — a *mean-reverting* process, not Brownian motion (which is a cumulative random walk with unbounded variance). These are meaningfully different: true Brownian drift would drift and be corrected by fixation control; this jitters around a fixed point.

Also, `static thread_local std::mt19937 rng{ 1337 }` is seeded with a **fixed constant**, so every run produces **identical jitter sequences**. For a system whose stated purpose is to look organic and non-synthetic, identical-per-run randomness is a subtle but real tell (and visibly so in recordings). Seed from `std::random_device` or the actor's FormID.

### 5.4 🟡 `TargetSelector` Unit-Conversion Magic Number

```cpp
target.distanceMeters = observer->GetPosition().GetDistance(playerPos) * 0.01428f; // ~70 units/meter
```

`0.01428` is `1/70.03`, i.e. Skyrim's ~70 units-per-metre. This appears **six times** as a bare literal. It should be a named constant:

```cpp
static constexpr float SKYRIM_UNITS_PER_METER = 70.0f;
static constexpr float UNITS_TO_METERS = 1.0f / SKYRIM_UNITS_PER_METER;
```

Also `160.0f` (eye height offset) is repeated five times and should be a named constant. These are exactly the kind of magic numbers that produce off-by-a-bit bugs when someone later "fixes" one occurrence.

### 5.5 🟡 Verbatim Duplication Between `README.md` and `TRUEGAZE_ARCHITECTURE.md`

`README.md` and `docs/ARCHITECTURE.md` are **byte-for-byte identical** for the sections I compared (Executive Summary through the scaffolding blueprint). In a project already suffering from documentation that over-claims, duplicated authoritative documents guarantee drift. Choose one canonical source; have the other link to it.

### 5.6 🟡 `LICENSE` vs. `README.md` Contradiction

- `LICENSE`: "**No license is granted under this file. All rights are reserved by the author.**" Also prohibits copying, modification, and distribution.
- `README.md` §9 scaffolding blueprint comments: `LICENSE  # Dual-license / MIT integration`

These are irreconcilable. The `LICENSE` file is also proprietary-and-closed while the project ships a "**Public C/C++ Modding API**". **You cannot ship a public modding SDK under a license that forbids copying.** *(The SkyUI MCM this finding originally cited was removed on 2026-09-14; the license/API tension remains.)*

Also worth noting: the `LICENSE` covers HCEP theory/maths as trade secrets — but `SaccadeGenerator.hpp` cites published academic literature (Bahill et al. 1975) for the Main Sequence equation. The *equation* is public science. Only your particular HCEP framing is proprietary. **Clarify what is trade secret versus what is published literature**, or the notice is unenforceable as written.

### 5.7 🟡 `.gitignore` Excludes the Shipping Artifacts

```
*.dll
*.zip
build/
```

The `.gitignore` excludes `build/`, `*.dll`, and `*.zip`. Yet `skyrim/SKSE/Plugins/TrueGaze.dll` **is committed** (it exists on disk and is referenced by `PackageMod.ps1`), and `dist/TrueGaze-v1.0.0-rc1-...zip` exists. Either these are force-added (fragile) or they are untracked (in which case a fresh clone cannot package). Add explicit negations:

```gitignore
!skyrim/SKSE/Plugins/TrueGaze.dll
!dist/
```

...or better, **do not commit binaries at all** and make `PackageMod.ps1` build from source on a clean checkout. Committing binaries alongside source guarantees version skew.

### 5.8 🟡 `PackageMod.ps1` Fragility

```powershell
$projectRoot = "D:\Projects\SkyrimTrueGaze"
$releaseDll = Join-Path $projectRoot "build\windows-release\Release\TrueGaze.dll"
```

Hardcoded absolute path — the script cannot run on any other machine or drive. Use `$PSScriptRoot`:

```powershell
$projectRoot = Split-Path -Parent $PSScriptRoot
```

Also: the script calls `& cmake --build --preset release` but never runs the **configure** step. On a clean machine, `--build` alone fails. Add configure-if-needed. Finally, the script does not copy `.pdb` files (see C-3).

### 5.9 🟡 `PerformanceProfiler` Stores Instead of Accumulates

```cpp
~ScopedTimer() {
    _accumulator.store(static_cast<uint64_t>(durationUs), std::memory_order_relaxed);
}
```

`.store()` **overwrites**. If the profiler is used per-actor in a loop (which is the stated design — `s_activeActorCount` implies per-actor accounting), only the **last** actor's timing survives. This should be `fetch_add` for accumulation, or the class should be documented as "last measurement only."

### 5.10 🟡 `VorCoordinator` Does Not Implement VOR

The class is named `VorCoordinator` and the docs describe VOR counter-rotation as a headline feature. The implementation:

```cpp
state.headYaw += headErrorYaw * alpha;                    // damped approach
float idealEyeYaw = state.targetYaw - state.headYaw;      // eye points at target
state.eyeLocalYaw = clamp(idealEyeYaw, ...);
```

This computes the eye's **gaze-maintenance** angle — which is *functionally* equivalent to VOR stabilisation during head movement, and the in-file comment correctly notes:

```cpp
// VOR effect: As head rotates by +headDelta, eye must compensate by -headDelta
// (Automatically accounted for by computing (target - head) above)
```

That reasoning is **sound**. But three doc-claimed behaviours are absent:

- **No latency gap.** Docs state eyes lead by 20–30 ms and head follows 120–180 ms. Here both update in the same frame from the same target, with only a damping constant differentiating them. `headTrackingSpeed = 6.0` gives a ~167 ms time constant, which approximates the *head* delay — but the eye has **zero** delay, so the eye-lead relationship is emergent-at-best, not modelled.
- **`headDeltaYaw`/`headDeltaPitch` are computed then discarded** — `[[maybe_unused]]`. The actual counter-rotation velocity term `ω_eye = -ω_head` is never used. It is argued away rather than implemented.
- **No `eyeMaxAngle` → head-handoff logic.** Docs (README §4) claim that exceeding comfort thresholds commands root navigation to step and turn. Not implemented.

**Verdict: functionally reasonable, scientifically under-delivered.** The gap between the documented claim and the code is the recurring theme of this audit.

### 5.11 🟡 `BoneController` Allocates the Full 100% to Head/Neck/Spine, Eyes Get 0%

```cpp
dist.spineYaw = clampedYaw * 0.10f;
dist.neckYaw  = clampedYaw * 0.25f;
dist.headYaw  = clampedYaw * 0.65f;
// dist.eyeYaw is never assigned — defaults to 0.0f
```

`StrainDistribution` declares `eyeYaw` and `eyePitch` fields, but `CalculateHierarchyStrain` **never writes them.** 0.10 + 0.25 + 0.65 = 1.00 exactly, leaving nothing for the eyes. By the project's own architecture (README §4: "Eye Nodes: 100% instant ballistic saccade"), the eyes should receive the *residual* (target minus head), not a fraction of the total.

**Correct formulation:**

```
head_total  = spine + neck + head     (the 100% distribution is the HEAD'S share of the gaze)
eye_local   = target_gaze - head_total (what the eyes must add on top)
```

As written, the head absorbs the entire deflection and the eyes are vestigial. Combined with C-1 (no bone writes), this has never manifested — but it would produce exactly the "robotic whole-body turning" the project exists to eliminate.

**This is a genuine algorithmic bug in the project's signature feature.**

### 5.12 🟡 `SocialTriangle` Distance Scaling Is Inverted in Magnitude

```cpp
float eyeSeparationDeg = (0.065f / dist) * (180.0f / 3.14159f);
```

With `dist = 1.5 m`: `(0.065 / 1.5) * 57.3 ≈ 2.48°`. At 0.5 m: `≈ 7.45°`. The direction (closer → larger angle) is **correct**.

But `vertexOffsetXDeg = -eyeSeparationDeg * 0.5f` at 1.5 m gives `-1.24°` from face centre, so the two eyes sit `2.48°` apart. For a real face at 1.5 m, interpupillary 65 mm subtends `2.48°` — **correct**. Good.

The issue is **naming/semantics**: `eyeSeparationDeg` is used as though it were a half-separation in one place (`* 0.5f`) and full separation conceptually. It works numerically, but the variable name will mislead a future maintainer. Rename to `interpupillaryDeg` and halve explicitly at the call site, or introduce `halfEyeSeparationDeg`.

Also `3.14159f` is a bare literal — use `std::numbers::pi_v<float>` (already available in `PCH.h`).

### 5.13 🟡 `MicroJitter` Bounds Are Not Guaranteed

The test asserts:

```cpp
assert(std::abs(state.currentYawOffset) <= state.maxAmplitudeDeg * 1.5f);
```

The `1.5×` fudge factor reveals the author knew the bound isn't honoured. Since `targetYawOffset ∈ [-max, +max]` and `current` exponentially approaches `target`, `current` **does** stay within `[-max, +max]` — so the true bound is `1.0×`. The test should assert `<= maxAmplitudeDeg` exactly, and any observed violation would indicate a real bug. **Relaxing an assertion to make it pass is a smell**; tighten it.

### 5.14 🟡 `EfmBlinkController`: Morph Writes Commented Out

```cpp
auto* faceGenData = actor->GetFaceGenAnimationData();
if (faceGenData) {
    // Morph indexes for EFM: Left/Right blink
    // faceGenData->exprOverrides[RE::FaceGen::Expression::BlinkLeft] = clampedWeight;
    // faceGenData->exprOverrides[RE::FaceGen::Expression::BlinkRight] = clampedWeight;
}
```

The only substantive logic is commented out. The `#else` branch is `(void)clampedWeight;`. So `ApplyMorphs` is a no-op on both paths. The `CHANGELOG.md` entry *"Implemented `EfmBlinkController::ApplyMorphs` for face morph target weight calculations"* is **not accurate**.

Additionally the `exprOverrides` array index — `RE::FaceGen::Expression::BlinkLeft` — is used as an array subscript, but in CommonLibSSE these are **named morph keys**, not a dense `Expression` enum indexing `exprOverrides`. Using a non-existent enum as an index would be a compile error once the SDK is present. This code has **never been compiled with the SDK** and will not compile without revision.

### 5.15 🟡 Test Suite Tests Only Dead Code

All 8 test suites pass — genuinely, and the assertions are mostly well-chosen. But:

- The tests exercise 8 modules.
- **None of those 8 modules is reachable from the shipped binary.**

So a green test run conveys **zero information about whether the product works.** In fact the test suite's success is actively misleading in the context of the audit report's "ALL 8 TESTS PASSED" framing.

**This is not a criticism of the tests' quality — it is a criticism of the coverage boundary.** The tests are a solid **unit** suite. There is no **integration** test, no **hook** test, and no **in-game** test. The HCEP bridge test (`HcepBridgeClientMock.cpp`) *is* a genuine integration test of the pipe and is the strongest verification artifact in the repo — but it tests the pipe in isolation from the game, with a mock on both ends.

Also: `HcepBridgeClientMock.cpp` contains a race — the client `WriteFile`s then immediately the test calls `TryGetLatestTelemetry` after only a 16 ms sleep, with no guarantee the worker has processed it. The observed output shows *duplicated* frames (`mode=2` twice, `mode=1` twice, `mode=4` twice) which is exactly the symptom of the reader outrunning the writer. The test tolerates this because it only asserts `magic`, but a stricter test would flake.

### 5.16 🟡 ROADMAP / CHANGELOG Accuracy

`ROADMAP.md` marks **Phases 1–7 as "Completed (100%)"** and only Phase 8 as partial. Based on this audit:

| Phase | Claimed | Assessed | Basis |
| :--- | :---: | :---: | :--- |
| 1 — Kinematics Core | 100% | **~85%** | Maths correct; `V_peak` never used; `eyeYaw` unwritten |
| 2 — Engine Integration | 100% | **~10%** | 🔴 No hook installed; no bone writes; SDK absent |
| 3 — HCEP Bridge | 100% | **~70%** | Pipe works; data race; nobody reads telemetry |
| 4 — OAR / EFM | 100% | **~15%** | 🔴 No registration; cache never written; morphs commented out |
| 5 — MCM | 100% | **N/A** | **MOOT — MCM layer removed 2026-09-14. Vanilla-UI: INI + `TrueGazeConfig.html`.** |
| 6 — VR & Profiling | 100% | **~40%** | Code exists; never invoked; `IsSkyrimVr()` always false in build |
| 7 — SDK & Packaging | 100% | **~50%** | Package builds; all API bodies are stubs; **Papyrus layer removed (C-4 moot)** |
| 8 — Cross-Engine | Partial | **~5%** | One header with a declaration; no implementation |

The ROADMAP is a **design document presented as a status report.** Recommend renaming the completed markers to distinguish *"Designed"* from *"Implemented"* from *"Verified in-engine."* A three-state vocabulary would have prevented this entire class of discrepancy.

---

## 6. Market Research & Competitive Landscape

### 6.1 The Core Question: "Is Anyone Doing This?"

**Answer: partially, in fragments — never in this combination, and never with this scientific depth.**

No commercial or open-source product was found that combines, for a shipping game:

1. biological oculomotor modelling (Main Sequence + VOR + microsaccadic drift), with
2. cognitive-state gaze behaviour (aversion, social triangle), with
3. an external real-world eye-tracker bridge driving NPC reactions, with
4. a modder-facing condition API.

Each pillar exists somewhere. **The integration is genuinely novel.**

### 6.2 Competitor Matrix — In-Game Character Gaze

| Product | Platform | What it does | Scientific depth | Gap vs. TrueGaze |
| :--- | :--- | :--- | :--- | :--- |
| **PC Head Tracking and Voice Type SE** | Skyrim SE (SKSE) | Player-character head/eye tracking to nearby actors; voice-type selection | **None** — priority heuristics only | No NPC→player gaze, no saccades, no VOR, no cognition. Known to *break* NPC eye tracking when enabled ([Reddit r/skyrimmods](https://www.reddit.com/r/skyrimmods/comments/wno92l/npcs_eyes_tracking_broken_with_pc_head_tracking/)) |
| **Player Headtracking** (Nexus #23600) | Skyrim LE/SE | Player head turns automatically toward NPCs | None | Player-only; no eye nodes |
| **Skyrim SE Head Tracking** (itsloopyo) | Skyrim SE | Webcam head tracking → player head, *no VR headset* | None | Player-only; head only; **no eye tracking** |
| **Reduced NPC Head Tracking** (Nexus #58361) | Skyrim SE | *Reduces* NPC head tracking | None | Reaction to the problem, not a solution |
| **EFM / Expressive Facial Animation** | Skyrim SE | Facial morph rigs & expressions | None | **Complementary** — a dependency candidate, not a competitor |
| **Open Animation Replacer (OAR)** | Skyrim SE | Conditional animation replacement | None | **Complementary** — TrueGaze's stated integration target |
| **ZenBlink** (Fab, UE5) | Unreal Engine 5 | Procedural blinking, pupil response, random/targeted eye/head/neck movement | **Low** — procedural, not modelled | Closest commercial analogue. No cognition, no VOR, no external tracker, no saccade dynamics |
| **MetaHuman Runtime Eyes Aim** (Epic, built-in) | UE5 | Eye bone aims at a target actor/camera | None | Simple aim constraint. No saccades, no drift, no psychology |
| **Runtime MetaHuman Lip Sync** (georgy.dev) | UE5 | Blink helpers alongside lip sync | Low | Lip-sync-adjacent, not gaze-intelligent |

**Reading of the matrix:** the Skyrim modding ecosystem's ceiling is **"head turns toward target."** Nobody in the ecosystem is modelling *how eyes actually move*. TrueGaze's scientific model would be, on arrival, **the most sophisticated character gaze system in the Skyrim ecosystem by a wide margin** — and arguably ahead of most commercial engine defaults.

### 6.3 Competitor Matrix — Real-World Eye Tracking / Gaze Input

| Product | Type | Purpose | Relevance |
| :--- | :--- | :--- | :--- |
| **Tobii** (Eye Tracker 5, Nexus) | Dedicated HW | Gaze-driven aiming/UI | Assistive input, not NPC behaviour |
| **Tobii Nexus** | Software SDK | Webcam→gaze for integration | **Best candidate bridge partner.** Has an SDK; would be an excellent alternative HCEP source |
| **Eyeware Beam** | Software | Webcam/phone→eye tracking | Same — viable telemetry source |
| **FaceTrackNoIR** | Open source | Webcam→head pose for games | Head only; older |
| **GazePlay** | Open source | Gaze-driven games for accessibility | Different domain; validates the tech, not a competitor |

**Key strategic insight:** TrueGaze's `NamedPipeServer` is **vendor-agnostic**. Anything that can write a 64-byte packet can drive it. That means Tobii, Eyeware Beam, or a MediaPipe pipeline can all be HCEP sources. **The pipe protocol is the real IP asset here** — more so than the Skyrim plugin, because it is engine-independent. This directly supports the "cross-engine expansion" thesis in `ROADMAP.md` Phase 8.

### 6.4 Academic Landscape — The Science Is Real, the Gap Is Application

Published work covers each element of TrueGaze's model in isolation:

| Topic | Representative work |
| :--- | :--- |
| Main Sequence (V_peak, duration vs. amplitude) | **Bahill, Clark & Stark (1975)** — the canonical source, correctly cited in `SaccadeGenerator.hpp` |
| Saccade velocity saturation asymptote | **Baloh et al. (1975)** — correctly cited |
| Physics-based saccade animation for characters | *"Physics-based modelling and animation of saccadic eye movement"* (Academia.edu) — models extraocular muscles |
| 3D saccade generation, optimal control | *"Modelling 3D saccade generation by feedforward optimal control"* (PMC8177626) |
| Eyelid kinematics for virtual characters | *"Eyelid kinematics for virtual characters"* (ResearchGate) |
| Saccade prediction for redirected walking | Wiley CAV 10.1002/cav.2167 — real-time saccade prediction |
| Gaze aversion during cognitive load | **Glenberg et al. (1998)** — cited in project docs; genuine published finding |
| Social triangle (eye-mouth-eye scanning) | **Argyle & Cook (1976)**, Ingham et al. (1973) — cited; genuine |

**Critical assessment:**

- ✅ **The citations are correct and real.** The project has not invented science. `V_peak = V_max(1 − e^(−θ/c))` with `V_max ≈ 750°/s, c ≈ 14°` matches the literature closely.
- ✅ **Applying these to *game NPCs* at runtime is genuinely under-explored.** The academic work targets *animation production* (offline, for film/characters) or *VR research*, not *runtime NPC behaviour in a shipping game*.
- ⚠️ **Glenberg's finding is more nuanced than "look away when thinking."** The published result concerns gaze aversion during *specific* cognitive tasks (particularly those with visual-spatial loading), and the effect varies by task type and individual. Claiming a universal "30–45% aversion rate" for all NPC dialogue (README §5) overstates a real but conditional finding.
- ⚠️ **"50 years of psycholinguistic and neuroscience research"** (README §1) is marketing register. Argyle (1976) and Glenberg (1998) were selected from a large literature; the framing implies a synthesis that isn't documented.

**Recommendation:** Add `docs/SCIENCE_FOUNDATION.md` giving the precise, per-claim citation for every constant — amplitude ranges, fixation durations, aversion rates, latency gaps. This converts marketing language into a **verifiable scientific position**, which is a genuine competitive moat. It also directly supports the empirical-validation track that `HCEP-SDK/docs/empirical_validation_protocol.md` already gestures at.

### 6.5 Market Verdict

**Is the moat real?** Yes, with a caveat.

**The defensible assets are:**

1. **The integrated model** — nobody combines oculomotor science + cognitive gaze + external hardware bridge + modder API.
2. **The pipe protocol** — engine-agnostic, vendor-agnostic, and the true bridge to the cross-engine thesis.
3. **The scientific grounding** — if documented rigorously, this is hard to copy and gives an authoritative voice.

**The caveat:** the moat is **in the design, not the delivery.** None of it currently runs. A competitor with the same research papers and three focused engineers would need perhaps 3–5 months to reach the state TrueGaze *claims* to be in — and could arrive while TrueGaze is still refining its documentation.

**Market risk is therefore schedule risk, not concept risk.** The concept is sound. The clock is the problem.

### 6.6 Positioning Recommendation

The Skyrim modding audience will not buy "oculomotor kinematics." It will buy **"NPCs who actually look at you."** Position accordingly:

| Technical framing | Market framing |
| :--- | :--- |
| Main Sequence saccade dynamics | *"Their eyes move like real eyes, not like security cameras"* |
| VOR counter-rotation | *"They glance first, then turn — like a person"* |
| Micro-saccadic Brownian drift | *"No more dead-eyed stares"* |
| Social triangle scanning | *"They look at your eyes *and* your mouth when you talk"* |
| Cognitive gaze aversion | *"They look away when they're thinking"* |
| HCEP Desktop bridge | *"When you look in their eyes, they know."* |

The last one is the killer feature. **Nothing else on the market delivers bi-directional mutual gaze.** Lead with it.

---

## 7. Considerations, Enhancements & Suggestions

### 7.1 Architecture

**A-1 — Introduce a `TrueGaze::Core` / `GazeEngine` singleton. (HIGH PRIORITY)**
Currently state is scattered: per-actor saccade/VOR/jitter state has **no home at all**, `g_pipeServer` is trapped in an anonymous namespace in `Main.cpp`, and the OAR cache is a file-static in `OarConditions.cpp`. Introduce one owner:

```cpp
namespace TrueGaze::Core {
    class GazeEngine {
    public:
        static GazeEngine& Get() noexcept;
        void LoadConfig(const ConfigManager&) noexcept;
        ActorGazeRuntime& GetOrCreate(uint32_t formId) noexcept;   // per-actor state
        void TickActor(RE::Actor*, float dt) noexcept;             // the missing loop
        void PublishToOar(uint32_t formId, const ActorGazeState&) noexcept;
        Bridge::NamedPipeServer& Bridge() noexcept;
        const Kinematics::Tuning& Tuning() const noexcept;
    };
}
```

This single change unblocks C-1, C-5, C-6, C-8, and C-10 simultaneously. **It is the highest-leverage refactor in this report.**

**A-2 — Add a per-actor runtime state struct.**
`SaccadeState`, `VorState`, `JitterState`, `TriangleState`, `BlinkState` all need to persist per NPC across frames. Nothing currently owns them. Add:

```cpp
struct ActorGazeRuntime {
    Kinematics::SaccadeGenerator::SaccadeState saccade{};
    Kinematics::VorCoordinator::VorState       vor{};
    Kinematics::MicroJitter::JitterState       jitter{};
    Kinematics::SocialTriangle::TriangleState  triangle{};
    Integrations::EfmBlinkController::BlinkState blink{};
    uint8_t  hcepMode{0};
    uint8_t  gazeRegion{0};
    float    mutualGazeHoldSec{0.0f};
    float    lastUpdateSec{0.0f};
};
```

Backed by a `std::unordered_map<uint32_t, ActorGazeRuntime>` with periodic eviction (unloaded actors would otherwise leak — this is also an **NFR-3 memory-footprint risk**).

**A-3 — Separate `TRUEGAZE_STANDALONE` as an explicit build mode.**
Rather than relying on `__has_include` to silently fork behaviour, make it a deliberate CMake option. Release builds must **fail** without the SDK. Standalone builds must be explicitly requested (for unit tests).

**A-4 — Consider a shared-memory fast path instead of a named pipe.**
The named pipe is correct for an external process, but for latency (<10 ns claim) and to avoid the pipe-handle race, a `CreateFileMapping` double-buffer with a seqlock would be simpler and genuinely lock-free. Keep the pipe as the *transport* for the external HCEP app; use the mapping for the in-process handoff. Consider it — not urgent.

### 7.2 Algorithmic Fidelity (Where the Project's Soul Lives)

**B-1 — Make the Main Sequence equation actually drive the trajectory. (HIGH PRIORITY)**
Replace `smoothstep` with a velocity profile that honours `V_peak(θ)` and `D(θ)`. The canonical form is a skewed-Gaussian or a minimum-jerk profile *normalised* so peak velocity matches `V_peak`. Suggested:

```cpp
// Normalise a Gaussian velocity profile so its peak == V_peak(theta)
// and its integral over [0, D] == amplitude. Then integrate.
```

This is the difference between "we cite Bahill" and "we implement Bahill." For this project it is the whole point.

**B-2 — Model the eye-lead latency gap explicitly.**
Add a `latencyTimerSec` to `VorState`. Eyes respond at `t=0`; head engages when `latencyTimer > 0.120f`. This is a small change producing a large perceptual payoff — it is *the* signature of biological gaze.

**B-3 — Implement true Brownian drift, or rename it.**
Choose:

- *True Brownian:* `current += gaussian(0, σ) * sqrt(dt)`, with an Ornstein-Uhlenbeck mean-reverting term to bound it.
- *Or rename to "low-pass filtered white noise"* and update the docs.

Also: **seed the RNG per-actor from `random_device` + FormID.** Identical-per-run jitter is a synthetic tell.

**B-4 — Fix the `BoneController` residual allocation. (§5.11)**
Eyes must receive `target − head_total`. This is a **correctness bug in the signature feature**.

**B-5 — Add microsaccade inhibition of return & fixation drift interaction.**
Advanced, but: real microsaccades occur *during* fixation and are inhibited immediately after a large saccade (~150 ms). Modelling this would be a genuine differentiator.

**B-6 — Add a blink-rate model, not just saccade-coupled blinks.**
Spontaneous blink rate is 15–20/min, modulated by cognitive load (higher during conversation, lower during visual attention). Currently blinks only fire on saccades >20°. Adding a spontaneous-blink Poisson process is cheap and high-value.

### 7.3 Engineering Robustness

**C-1 — Fix the double-buffer race. (§C-7)**
Triple-buffer, or seqlock. Non-negotiable before any release.

**C-2 — Make `_pipeHandle` atomic, or move writes to the worker thread.**

**C-3 — Add frame-time guardrails.**
`PerformanceProfiler` is never invoked (C-1). Once the tick exists, wrap it:

```cpp
Engine::PerformanceProfiler::ScopedTimer t(Engine::PerformanceProfiler::s_lastFrameTimeUs);
```

and — critically — **degrade gracefully** when over budget:

```cpp
if (!PerformanceProfiler::IsWithinBudget()) { skipMicroJitter(); skipSocialTriangle(); }
```

An adaptive LOD that responds to *measured* cost, not just distance, is far more robust.

**C-4 — Exception safety at the hook boundary.** NFR-4 requires no exception reaches the game loop. Wrap the hook body in `try/catch(...)` as a last resort. Non-negotiable for a SKSE plugin.

**C-5 — Thread-safe logging.** The pipe worker and the game thread both call `logger::info`. `spdlog` sinks are thread-safe by default with `_mt` variants, but the fallback stub is not and the pattern isn't configured for multi-thread. Verify.

**C-6 — Add `PDB` to the package** for crash symbolication (C-3, §3.3).

**C-7 — Bound the actor map** with an LRU/eviction sweep on cell change to satisfy NFR-3.

### 7.4 Testing Strategy

**D-1 — Add an integration test that proves bone writes occur.** Mock `NiNode` and assert a transform is written. Without this, C-1 can silently regress.

**D-2 — Tighten `MicroJitter` assertion** to `<= maxAmplitudeDeg` (§5.13).

**D-3 — De-flake `HcepBridgeClientMock`** with a condition variable or a polling loop instead of `sleep_for`. (§5.15)

**D-4 — Add property-based tests for the kinematics.** e.g. for 1000 random amplitudes, assert monotonicity of `V_peak` and `D`, and that `D ∈ [22ms, 120ms]`.

**D-5 — Add a golden-file test for the wire protocol** — a byte-exact hex dump of a known packet — to catch accidental struct changes.

**D-6 — Add an in-game smoke test checklist** with screenshots/GIFs. This is what users actually need.

**D-7 — Add CI.** A GitHub Actions workflow running configure + build + `KinematicsTests` + `HcepBridgeClientMock` would have caught C-2 on day one. **This is the single highest-value process improvement available.**

### 7.5 Product & Go-to-Market

**E-1 — Reframe the ROADMAP around demonstrable artifacts.**
Replace `[x] Completed` with `[x] Designed / [ ] Implemented / [ ] Verified in-game`. This prevents the exact over-claiming this audit found.

**E-2 — Ship a "dead-eye fix" minimal viable feature first.**
Don't wait for the full 5-mode cognition system. **Head + eye tracking with saccades and jitter** alone would be a compelling, independently-publishable Nexus mod. The cognitive modes become the v2 headline.

**E-3 — Publish the OAR conditions as their own distribution.**
`TrueGaze_IsMode`, `TrueGaze_IsMutualGaze`, `TrueGaze_GetGazeRegion` are independently valuable to animators **even if the kinematics are minimal.** This seeds ecosystem adoption before the engine is complete.

**E-4 — Cultivate 3–5 animation-modder partners early.**
They will build the gestures that make your kinematic states *visible*. Their mods become your marketing. (See §6.5 — the schedule clock is the real risk.)

**E-5 — Lead with mutual gaze.**
*"When you look in their eyes, they know."* Nothing else on the market does this. It is the demo that will get shared.

**E-6 — Consider a companion-mod collaboration.** The intimacy/companion modding community is large, highly engaged, and would pay disproportionate attention to mutual-gaze reactions. The OAR `SPIRIT`-mode rule is a ready-made hook.

**E-7 — Clarify the licensing contradiction before publishing. (§5.6)** You cannot ship a "Public Modding SDK" under "no license is granted; copying and distribution prohibited."

### 7.6 Documentation

**F-1 — Resolve the duplicate authoritative documents. (§5.5)** Pick one canonical `ARCHITECTURE.md`; make the other a link.

**F-2 — Add `docs/SCIENCE_FOUNDATION.md`** with per-claim citations. (§6.4) This is your credibility moat.

**F-3 — Add `docs/STATUS.md`** — an honest capability matrix (Designed / Implemented / Verified). Over-claiming in the ROADMAP actively damages trust when users install the mod and see no change.

**F-4 — Add `docs/INTEGRATION_GUIDE.md`** for HCEP Desktop implementers — the 64-byte layout with byte offsets annotated, plus a working reference client. (The existing `HcepBridgeClientMock.cpp` is an excellent start and should be promoted, not left in `tests/`.)

**F-5 — Correct the CHANGELOG entries that describe unimplemented work** (`ApplyMorphs`, OAR registration, public SDK). (§5.14, §C-5, §C-6)

**F-6 — Document the Skyrim build bootstrap** (`git submodule update --init --recursive`, vcpkg setup, MSVC version). Currently a fresh clone cannot reproduce the build.

### 7.7 Strategic / Cross-Engine

**G-1 — Treat the pipe protocol, not the Skyrim plugin, as the primary IP.** It is engine-independent and vendor-agnostic. (§6.3) It is also the thing a competitor would have to re-derive.

**G-2 — Add Tobii / Eyeware Beam adapters.** Both have SDKs. Each adapter is a small translation layer producing a 64-byte packet. This turns "requires Kirk's HCEP Desktop" into "works with what you already own" — **a massive TAM expansion** for minimal engineering.

**G-3 — Reprioritize Phase 8 (cross-engine).** UE5/Unity headers are *declarations only*. Meanwhile the UE5 ecosystem already has MetaHuman eye-aim and ZenBlink. Entering UE5 without the Skyrim reference implementation proven would be premature. **Prove it in Skyrim first.** Skyrim is the ideal proving ground: passionate, technical audience, low build cost, real feedback.

**G-4 — Explore the "NPCs that notice you" gameplay thesis.** Mutual gaze is a *mechanic*, not just a cosmetic. Stealth detection, intimidation, romance, dialogue gating. This is a product direction beyond "more realistic eyes" and is where the commercial value likely lives.

---

## 8. Remediation Plan

Sequenced so each phase produces a **demonstrable artifact**. Effort estimates assume one focused engineer.

### Phase 0 — Truth Reset *(0.5 day)*

- [ ] Correct `ROADMAP.md` to a three-state vocabulary (Designed / Implemented / Verified).
- [ ] Correct `CHANGELOG.md` entries for unimplemented work.
- [ ] Add `docs/STATUS.md` with the honest capability matrix (§7.6 F-3).
- [ ] Resolve the `README` vs `TRUEGAZE_ARCHITECTURE` duplication.
- [ ] Resolve the `LICENSE` vs "Public Modding SDK" contradiction.

**Deliverable:** Documentation that matches reality.

---

### Phase 1 — Make the Build Real *(1–2 days)*

- [ ] Add CommonLibSSE-NG as a proper git submodule in `extern/`.
- [ ] Change the CMake guard to `FATAL_ERROR` when absent.
- [ ] Add `TRUEGAZE_STANDALONE` as an explicit opt-in CMake option.
- [ ] Add a CI workflow: configure → build → test. **This catches C-2 permanently.**
- [ ] Verify the DLL now links CommonLibSSE; re-run the import scan from §3.2.

**Deliverable:** A DLL that actually contains game-facing code. *Everything downstream depends on this.*

---

### Phase 2 — Make It Move *(1–2 weeks)* 🔴 **THE CRITICAL PHASE**

- [ ] Implement `TrueGaze::Core::GazeEngine` (§7.1 A-1).
- [ ] Add `ActorGazeRuntime` per-actor state + map with eviction (§7.1 A-2).
- [ ] Identify and install the real animation hook target via `REL::Relocation`.
- [ ] Implement the per-actor tick: `TargetSelector` → modules → `BoneController` → **`NiNode` write**.
- [ ] Fix the `BoneController` residual-allocation bug (§5.11 / §7.2 B-4).
- [ ] Wrap in `ScopedTimer` + `try/catch` (§7.3 C-1, C-4).

**Deliverable:** **A video of an NPC whose eyes visibly move.** This is the moment the product becomes real.

---

### Phase 3 — Make It Correct *(1 week)*

- [ ] Fix the double-buffer race — triple buffer or seqlock (§C-7).
- [ ] Make `_pipeHandle` atomic (§C-7).
- [ ] Implement the Main Sequence velocity profile (§7.2 B-1).
- [ ] Add the eye-lead latency gap (§7.2 B-2).
- [ ] True Brownian drift with `random_device` seeding (§7.2 B-3).
- [ ] Plumb `ConfigManager` into the simulation via a `Tuning` struct (§C-8).
- [ ] Call `ConfigManager::Load()` in the correct path; gate `Start()` on config (§C-3).
- [ ] Converge/delete the dual init paths in `Main.cpp` (§C-3).
- [ ] Tighten tests; add the integration test (§7.4 D-1, D-2, D-3).

**Deliverable:** Config that works; motion that is scientifically faithful and race-free.

---

### Phase 4 — Make It Ecosystem-Real *(1–2 weeks)*

- [ ] Implement genuine OAR condition registration via SKSE messaging (§C-5).
- [ ] Add `PublishActorState()` writing `g_actorGazeCache` each tick (§C-5).
- [ ] **Remove the false success log** in `RegisterWithOar` (§C-5).
- [x] ~~Implement `PapyrusInterface::RegisterFunctions()` with correct signatures (§C-4).~~ — **MOOT: the Papyrus layer was removed 2026-09-14; TrueGaze is vanilla-UI.**
- [ ] Implement real `TrueGazeAPI` bodies reading live state (§C-6).
- [ ] Expose the pipe accessor for `IsHcepConnected` (§C-6).
- [ ] Uncomment & correct `EfmBlinkController::ApplyMorphs` (§5.14).
- [x] ~~Create the `TrueGaze.esp` (or ESL) with MCM globals~~ — **MOOT: no ESP, no MCM. Configuration is the INI edited via `TrueGazeConfig.html`.**
- [x] ~~Compile Papyrus scripts to `.pex`; include in the package (§C-9).~~ — **MOOT: no Papyrus scripts exist.**
- [ ] Include `.pdb` in the package (§7.3 C-6).

**Deliverable:** A package that installs, configures, and drives OAR rules — a complete mod.

---

### Phase 5 — Make It Credible *(ongoing)*

- [ ] Write `docs/SCIENCE_FOUNDATION.md` (§7.6 F-2).
- [ ] Write `docs/INTEGRATION_GUIDE.md`; promote the mock client (§7.6 F-4).
- [ ] Recruit 3–5 animation-modder partners (§7.5 E-4).
- [ ] Publish the OAR conditions standalone (§7.5 E-3).
- [ ] Produce the mutual-gaze demo (§7.5 E-5).
- [ ] Add Tobii / Eyeware Beam adapters (§7.7 G-2).
- [ ] Fix `PackageMod.ps1` to use `$PSScriptRoot` + configure step (§5.8).

**Deliverable:** Market presence, scientific credibility, and a contributor pipeline.

---

## 9. Closing Assessment

### What You Have Built

Let me be direct about the quality of what exists, because the findings above are numerous and it would be easy to read them as a verdict on the work. **They are not.**

The scientific architecture is **excellent**. The Main Sequence implementation is correct and correctly cited. The wire protocol is professionally specified with compile-time layout guards. The documentation is better than most commercial projects produce. The OAR rule set shows real empathy for the modder experience. The strategic instinct — that the *pipe protocol* is the durable asset, not the Skyrim plugin — is right.

Most importantly: **you identified a real problem that nobody has solved, and you designed a genuinely novel solution to it.** The market research in §6 confirms this. "Dead-eye syndrome" is real, universally recognised by players, and nobody is fixing it properly.

### What You Have Not Yet Built

The product does not run. Not because the design is wrong, but because the last mile — connecting the parts — is unbuilt, and because a silently-degrading build has been masking that fact.

The single most damaging pattern in this repository is **`#if __has_include(<RE/Skyrim.h>)` silently forking to stubs.** The intent was noble (testability without the SDK). The consequence is that a build with no SDK produces a DLL that *looks* complete, logs *success* messages for work it did not do, and exports *all* the right symbols — while doing nothing. That is why the ROADMAP says 100% and the artifact says otherwise, and it is why an audit was needed at all.

**Fix that one pattern and most of this report resolves itself.**

### The Honest Framing

| | |
| :--- | :--- |
| **Concept** | 🟢 Genuinely novel. Confirmed against the market. |
| **Design** | 🟢 Strong. Scientific, well-specified, modder-friendly. |
| **Implementation** | 🔴 ~30%. The engine is inert. |
| **Documentation** | 🟡 Good quality, materially over-claiming status. |
| **Distance to a shippable 1.0** | 🟡 ~4–6 focused weeks. |
| **Distance to a compelling Nexus release** | 🟢 **~2 weeks** (Phase 1 + Phase 2 = "eyes that move"). |

The 2-week figure is the important one. You are **much closer to something publishable than the gap between claim and reality suggests** — because the hard parts (the science, the design, the ecosystem strategy) are genuinely finished. What remains is engineering labour, and it is well-scoped in §8.

### The One Thing To Do Next

Add CommonLibSSE-NG to `extern/`. Change the CMake guard to `FATAL_ERROR`. Then make one NPC's eyes move.

Everything else in this report is downstream of that. **Build the drivetrain. The engine and dashboard are already beautiful.**

---

*Audit conducted by direct inspection of all documentation, all source, and all build artifacts, supplemented by public market and academic research. Findings marked [VERIFIED] were independently reproduced during this audit.*

*Kirk — the design is sound and the idea is real. Now go make it move.*
