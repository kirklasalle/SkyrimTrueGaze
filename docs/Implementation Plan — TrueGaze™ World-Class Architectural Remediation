# Implementation Plan — TrueGaze™ World-Class Architectural Remediation

Remediate all critical, major, and moderate issues identified in the September 15, 2026 World-Class Software Application Programming Audit to achieve rock-solid stability, correct biological oculomotor kinematics, and complete drivetrain wiring.

## User Review Required

> [!IMPORTANT]
> **Key Architecture Decisions:**
> 1. **Named Pipe Concurrency**: Upgraded to Win32 Asynchronous Overlapped I/O with manual-reset shutdown event to guarantee instant, deadlock-free game exit even when no HCEP client is connected.
> 2. **FaceGen Morph Keyframes**: Corrected from `SetExpressionOverride` (which was triggering dialogue anger/fear on blink) to `modifierKeyFrame.SetValue(Modifier::BlinkLeft/Right)`, restoring anatomical eyelid closure.
> 3. **VOR Biomechanical Wiring**: Routed `state.vor.headYaw/Pitch` to the spine/neck/head chain and `state.vor.eyeLocalYaw/Pitch + jitter` directly to the left/right eye bones. This activates authentic Vestibulo-Ocular Reflex and fixes the "head snaps at 750 deg/s with stationary eyes" defect.
> 4. **Frame Lifecycle Anchor**: Hooked `PlayerCharacter::Update` to invoke `GazeEngine::EndFrame` once per frame, enabling actor eviction and real microsecond frame profiling.

---

## Proposed Changes

Grouped by component and layer:

### Bridge & IPC Subsystem

#### [MODIFY] [NamedPipeServer.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Bridge/NamedPipeServer.hpp)
- Add `HANDLE _shutdownEvent{nullptr}` and `OVERLAPPED _connectOverlapped{}`.
- Update `Start` and `Stop` signatures and private helpers for Overlapped I/O.

#### [MODIFY] [NamedPipeServer.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Bridge/NamedPipeServer.cpp)
- In `CreateNamedPipeA`, add `FILE_FLAG_OVERLAPPED`.
- In `WorkerLoop`: Call `ConnectNamedPipe` asynchronously; wait on `_shutdownEvent` and `_connectOverlapped.hEvent` with `WaitForMultipleObjects`.
- In `Stop`: Signal `_shutdownEvent` and cancel pending I/O before calling `_workerThread.join()`, eliminating process hangs on quit.

---

### Integrations Subsystem

#### [MODIFY] [EfmBlinkController.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Integrations/EfmBlinkController.cpp)
- Replace calls to `faceGenData->SetExpressionOverride` with `faceGenData->modifierKeyFrame.SetValue(Modifier::BlinkLeft/Right, clampedWeight)`.
- Set `faceGenData->modifierKeyFrame.isUpdated = true`.

---

### Engine & Skeleton Subsystem

#### [MODIFY] [EyeAimConstraint.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/EyeAimConstraint.hpp)
#### [MODIFY] [EyeAimConstraint.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/EyeAimConstraint.cpp)
- Add `EyeAimConstraint::Reset()` to safely reset `g_touchedCount = 0` and `g_frameOpen = false` on cell transitions and game loads without dereferencing dangling pointers.
- In `WithdrawActor`: verify actor validity before dereferencing `slot.bone`.
- In `RefreshWorldTransform`: call NetImmerse `UpdateDownwardPass` with `NiUpdateData` so child meshes (hair, beard, horns, helmets) follow bone rotations.

#### [MODIFY] [BoneController.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/BoneController.hpp)
- Update `CalculateHierarchyStrain` documentation and defaults to clarify that head chain distributes `headYaw/headPitch`, while eyes directly receive VOR counter-rotation + jitter.

#### [MODIFY] [TargetSelector.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TargetSelector.cpp)
- Fix Priority 4 (AmbientInterest): rotate `kAmbientForwardUnits` by the actor's orientation `std::sin(yaw)` / `std::cos(yaw)` instead of hardcoding global `+X` (East).

#### [MODIFY] [ActorGazeRuntime.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ActorGazeRuntime.hpp)
- Add `float modeOverrideTimerSec{0.0f}` and `bool hasModeOverride{false}` to support timed API mode overrides.

#### [MODIFY] [TrueGazeAPI.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TrueGazeAPI.cpp)
- In `TrueGaze_OverrideActorMode`: initialize `modeOverrideTimerSec = durationSec` and `hasModeOverride = true`.

#### [MODIFY] [GazeEngine.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.hpp)
#### [MODIFY] [GazeEngine.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp)
- In `ResetAll`: call `EyeAimConstraint::Reset()` to prevent use-after-free crashes during save/load.
- In `ComputeDeflection`:
  - Honor API mode override timer.
  - If `_pipe.IsConnected()` and `_pipe.TryGetLatestTelemetry(packet)`, consume Mode 2 real-time gaze telemetry.
- In `ApplyToSkeleton`:
  - Route `state.vor.headYaw` and `state.vor.headPitch` through the spine/neck/head strain hierarchy.
  - Route `state.vor.eyeLocalYaw + jitterYaw` and `state.vor.eyeLocalPitch + jitterPitch` directly to `eyeL` and `eyeR` bones!
- In `PublishState`: If `_pipe.IsConnected()`, send `SkyrimFeedbackPacket` back to HCEP Desktop.

#### [MODIFY] [AnimationHook.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/AnimationHook.cpp)
- In `ActorUpdateHook<PlayerTag>::Hook`: after calling `_original`, call `GazeEngine::Get().EndFrame(a_delta)` once per frame.

---

### Kinematics Optimization

#### [MODIFY] [SaccadeGenerator.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Kinematics/SaccadeGenerator.hpp)
- Replace 32-step numerical trapezoidal integral calling `std::exp` in `ProgressAt(t)` with a compile-time precomputed 65-entry lookup table with linear interpolation.

---

## Verification Plan

### Automated Tests
1. **Build Verification**:
   ```powershell
   cmake --build d:\Projects\SkyrimTrueGaze\build\windows-release --config Release
   ```
2. **Kinematics & Mathematics Tests**:
   ```powershell
   d:\Projects\SkyrimTrueGaze\bin\KinematicsTests.exe
   ```
3. **Named Pipe Bridge Mock Test**:
   ```powershell
   d:\Projects\SkyrimTrueGaze\bin\HcepBridgeClientMock.exe
   ```
4. **Pre-Flight Health Verification**:
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File "d:\Projects\SkyrimTrueGaze\scripts\Test-TrueGazeHealth.ps1"
   ```

### Manual Verification
1. **Deployment Pipeline**:
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File "d:\Projects\SkyrimTrueGaze\scripts\Deploy-TrueGaze.ps1" -NoLaunch -NoBuild
   ```
2. **Post-Run Log Inspection**:
   ```powershell
   powershell -NoProfile -ExecutionPolicy Bypass -File "d:\Projects\SkyrimTrueGaze\scripts\Deploy-TrueGaze.ps1" -PostRun
   ```
