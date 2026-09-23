# TrueGaze™ Architectural Remediation Walkthrough

**Product:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine  
**Author & Product Owner:** Kirk LaSalle  
**Execution Date:** September 16, 2026  
**Status:** **Fully Implemented, Verified & Deployed**

---

## 1. Executive Summary of Changes

All 10 critical, major, and moderate issues identified in the September 15, 2026 World-Class Software Application Programming Audit have been fully remediated and validated:

| Subsystem | Audit Finding | Remediation Applied | Status |
| :--- | :--- | :--- | :---: |
| **Bridge & IPC** | C-01: Thread deadlock on exit in `NamedPipeServer::Stop()` | Converted Named Pipe to Win32 Asynchronous Overlapped I/O (`FILE_FLAG_OVERLAPPED`) with manual-reset shutdown event. Instant non-blocking shutdown. | **RESOLVED** |
| **Integrations** | C-02: FaceGen morph inversion (blinks triggering DialogueAnger/Fear) | Switched from `SetExpressionOverride` to `modifierKeyFrame.SetValue(Modifier::BlinkLeft/Right, weight)` and `isUpdated = true`. | **RESOLVED** |
| **Engine / Memory** | C-03: Dangling bone pointer hazard / CTD on cell unload | Added `EyeAimConstraint::Reset()` to clear cached pointers without dereferencing; added actor liveness checks in `WithdrawActor`. | **RESOLVED** |
| **Kinematics / Skeleton** | C-04: VOR kinematic disconnect (eyes stayed at 0°) | Separated head chain from ocular rotation: routed `state.vor.headYaw/Pitch` through spine/neck/head strain, and `state.vor.eyeLocalYaw/Pitch + jitter` directly to eye bones. | **RESOLVED** |
| **Engine / AI** | C-05: Ambient interest facing bug ("all NPCs stare East") | Replaced hardcoded global `+X` forward vector with heading trigonometry `std::sin(actorYaw)` and `std::cos(actorYaw)`. | **RESOLVED** |
| **Engine / Lifecycle** | C-06: Orphaned frame lifecycle (`EndFrame` never called) | Hooked `GazeEngine::Get().EndFrame(a_delta)` into `PlayerCharacter` update hook. Enables 30-second actor eviction and microsecond profiling. | **RESOLVED** |
| **Bridge / Engine** | C-07: Mode 2 HCEP telemetry unconsumed | Hooked `_pipe.TryGetLatestTelemetry(hcepPacket)` into `GazeEngine::ComputeDeflection`, and outbound `_pipe.SendFeedback()` into `PublishState`. | **RESOLVED** |
| **Engine / Scene Graph** | C-08: NetImmerse child transforms out of sync | Added `NiUpdateData` downward pass (`bone->UpdateDownwardPass(updateData, 0)`) in `RefreshWorldTransform` so hair, beards, horns, and helmets track bone motion. | **RESOLVED** |
| **API** | C-09: `TrueGaze_OverrideActorMode` overwritten on next tick | Added `modeOverrideTimerSec` and `hasModeOverride` to `ActorGazeRuntime`. Overrides persist for the requested duration. | **RESOLVED** |
| **Kinematics / Perf** | C-10: Saccade progress quadrature CPU overhead | Replaced 32-step numerical trapezoidal integral calling `std::exp` with compile-time 65-entry LUT and linear interpolation. | **RESOLVED** |

---

## 2. Modified Components & Key Diffs

### 2.1 Bridge & Concurrency Subsystem
- [`NamedPipeServer.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Bridge/NamedPipeServer.hpp): Added Windows headers, `HANDLE _shutdownEvent`, and `OVERLAPPED _connectOverlapped`.
- [`NamedPipeServer.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Bridge/NamedPipeServer.cpp):
  - Created pipe with `FILE_FLAG_OVERLAPPED`.
  - Used `WaitForMultipleObjects` on `_shutdownEvent` and `_connectOverlapped.hEvent` during `ConnectNamedPipe`.
  - In `Stop()`, signals `_shutdownEvent`, issues `CancelIoEx`, and cleanly joins the worker thread without hangs.
  - In `DrainOutboundQueue` and `WorkerLoop`'s read loop, handles overlapped `WriteFile` and `ReadFile`.

### 2.2 Facial Morph Subsystem
- [`EfmBlinkController.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Integrations/EfmBlinkController.cpp):
  - Corrected morph destination from `expressionKeyFrame` to `modifierKeyFrame.SetValue(Modifier::BlinkLeft/Right, clampedWeight)`.
  - Enabled `modifierKeyFrame.isUpdated = true`. Eyelids now close anatomically without grimacing in anger or fear.

### 2.3 Engine, Kinematics & Bone Controller
- [`EyeAimConstraint.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/EyeAimConstraint.hpp) & [`EyeAimConstraint.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/EyeAimConstraint.cpp):
  - Implemented `EyeAimConstraint::Reset()` to reset tracking safely without touching potentially deallocated `NiAVObject` pointers during cell loads.
  - Safeguarded `WithdrawActor` with `TESForm::LookupByID` and `actor->Get3D()` checks.
  - Added `UpdateDownwardPass` with `NiUpdateData::Flag::kDirty` in `RefreshWorldTransform`.
- [`TargetSelector.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TargetSelector.cpp):
  - Corrected ambient forward point calculation using `std::sin(actorYaw)` and `std::cos(actorYaw)`.
- [`ActorGazeRuntime.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ActorGazeRuntime.hpp) & [`TrueGazeAPI.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TrueGazeAPI.cpp):
  - Added `modeOverrideTimerSec` and `hasModeOverride` with active countdown handling.
- [`GazeEngine.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp):
  - `ResetAll` now calls `EyeAimConstraint::Reset()`.
  - `ComputeDeflection` consumes Mode 2 telemetry from `_pipe.TryGetLatestTelemetry` when connected.
  - `ApplyToSkeleton` passes `state.vor.headYaw/Pitch` into the spine/neck/head strain calculation, and passes `state.vor.eyeLocalYaw/Pitch + jitter` directly into `eyeL` and `eyeR`.
  - `PublishState` sends outbound `SkyrimFeedbackPacket` to the desktop client via `_pipe.SendFeedback`.
- [`AnimationHook.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/AnimationHook.cpp):
  - In `PlayerTag` update hook, calls `GazeEngine::Get().EndFrame(a_delta)` once per frame.
- [`SaccadeGenerator.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Kinematics/SaccadeGenerator.hpp):
  - Implemented compile-time `kProgressLut` (65 floats) and constant-time linear interpolation in `ProgressAt(t)`.

---

## 3. Verification & Validation Results

### 3.1 Solution Build
Executed:
```powershell
cmake --build d:\Projects\SkyrimTrueGaze\build\windows-release --config Release
```
**Result:** **0 Errors, 0 Warnings**. Output produced `TrueGaze.dll` (646.5 KB), `KinematicsTests.exe`, and `HcepBridgeClientMock.exe`. Auto-refreshed package in `skyrim/SKSE/Plugins/TrueGaze.dll`.

### 3.2 Biomechanical Kinematics & Main Sequence Test Suite
Executed:
```powershell
d:\Projects\SkyrimTrueGaze\bin\Release\KinematicsTests.exe
```
**Result:**
```
========================================================
  SkyrimTrueGaze Kinematics & Mathematics Test Suite   
  An HCEP Product by Kirk LaSalle                      
========================================================
[TEST] Running SaccadeGenerator verification...
  -> SaccadeGenerator passed.
[TEST] Running VorCoordinator verification...
  -> VorCoordinator passed.
[TEST] Running MicroJitter bounded-drift verification...
  -> MicroJitter bounded-drift passed.
[TEST] Running SocialTriangle verification...
  -> SocialTriangle passed.
[TEST] Running BoneController hierarchy strain verification...
  -> BoneController passed.
[TEST] Running eye residual allocation verification...
  -> Eye residual allocation passed.
[TEST] Running Main Sequence profile fidelity verification...
  -> Main Sequence fidelity passed.
[TEST] Running micro-jitter Brownian/seeding verification...
  -> Micro-jitter Brownian/seeding passed.
[TEST] Running LodManager verification...
  -> LodManager passed.
[TEST] Running EfmBlinkController verification...
  -> EfmBlinkController passed.
[TEST] Running TelemetryPacket layout and CRC32 verification...
  -> TelemetryPackets passed (64-byte & 32-byte layout verified).

[SUCCESS] ALL 11 BIOMECHANICAL KINEMATICS TESTS PASSED!
```

### 3.3 Overlapped IPC Named Pipe Bridge Test
Executed:
```powershell
d:\Projects\SkyrimTrueGaze\bin\Release\HcepBridgeClientMock.exe
```
**Result:**
- Non-blocking server startup confirmed.
- Client connected to `\\.\pipe\TrueGazeBridge`.
- 10 simulated 64-byte telemetry frames successfully streamed and consumed.
- 32-byte outbound `SkyrimFeedbackPacket` successfully written and verified by client.
- Clean, instantaneous server shutdown with 0 deadlocks.

### 3.4 Pre-Flight Diagnostic Health Check & Deployment
Executed:
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File "d:\Projects\SkyrimTrueGaze\scripts\Deploy-TrueGaze.ps1" -NoLaunch -NoBuild
```
**Result:**
- Deployed freshly built `TrueGaze.dll` (646.5 KB) and `TrueGaze.ini` into `Skyrim Special Edition\Data\SKSE\Plugins\`.
- All 14 diagnostic checks PASSED with 0 warnings and 0 failures:
  - Skyrim 1.7.104.0 match: OK
  - `skse64_1_7_104.dll` match: OK
  - `versionlib-1-7-104-0.bin` (2.2 MB) match: OK
  - VC++ runtime: OK
  - All 3 SKSE exports verified: OK
  - Slot `0xAD` driver verified: OK
  - Exception guards: OK
  - **VERDICT: Clear to launch.**
