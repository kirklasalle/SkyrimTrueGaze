# Walkthrough — TrueGaze™ Deep Audit Remediation (September 17, 2026)

All 5 immediate remediation actions specified in the **TrueGaze™ Deep Systems, Kinematics, Runtime & Safety Audit** have been surgically implemented, compiled in Release configuration, verified against all 11 biomechanical kinematics tests and the HCEP IPC test harness, and deployed to the live Skyrim game directory.

---

## Remediation Summary

| # | Remediation Action | Root Cause Addressed | Files Modified | Verification Result |
|---|-------------------|----------------------|----------------|---------------------|
| **1** | **Remove `TickAllActors` from `PlayerHook::Hook`** | Solved double-ticking collision where NPCs were ticked during player hook, withdrawn at `Character::Update`, and early-returned due to `lastFrameTicked`, leaving NPCs inert. | [`src/Engine/AnimationHook.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/AnimationHook.cpp) | **PASSED** — Clean single-pass simulation per actor per frame. |
| **2** | **Fix Co-Located Cart Passenger Singularity** | Helgen cart passengers share root coordinates `(16523.4, -82583.0, 8310.3)`. `IsInVisualCone` returned `true` for `<1.0f` dist, causing `atan2(0,0) = 0` yaw jumps (`+106°` neck snaps). | [`src/Engine/TargetSelector.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TargetSelector.cpp)<br>[`src/Engine/GazeEngine.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp) | **PASSED** — Rejects targets `< 0.35m` (25 units) in visual cone, guards `horizontal < 25.0f` in `WorldTargetToLocalGaze`, uses `root->world.translate`. |
| **3** | **Clamp Head Angles in `VorCoordinator::Update`** | Unclamped `state.headPitch` accumulated to steep angles (`+86.6°`). Because `idealEyePitch = targetPitch - headPitch = 0°`, eyes froze forward while neck stopped at physical limit. | [`src/Kinematics/VorCoordinator.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Kinematics/VorCoordinator.hpp)<br>[`tests/KinematicsTests.cpp`](file:///d:/Projects/SkyrimTrueGaze/tests/KinematicsTests.cpp) | **PASSED** — Head yaw clamped to `[-70°, +70°]`, pitch clamped to `[-35°, +45°]`. Ocular counter-rotation verified at steep pitch in unit tests. |
| **4** | **Harden `EyeAimConstraint::Withdraw()`** | Dereferencing `slot.bone` during end-of-frame withdrawal without verifying actor and 3D root liveness risked use-after-free or CTDs on cell changes. | [`src/Engine/EyeAimConstraint.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/EyeAimConstraint.cpp) | **PASSED** — Added `RE::TESForm::LookupByID(slot.actorFormId)` and `actor->Get3D()` verification before dereference. |
| **5** | **Cache Resolved Bones in `ActorGazeRuntime`** | Vanilla skeletons without separate eye bones caused `FindFirstBone` to fail every frame, running 6 full recursive `BSVisit::TraverseScenegraphObjects` passes with string allocations per actor per frame (up to 844µs frame times). | [`src/Engine/ActorGazeRuntime.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ActorGazeRuntime.hpp)<br>[`src/Engine/GazeEngine.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp) | **PASSED** — Resolved bone pointers cached on initial encounter or root change; eliminates redundant traversals. |

---

## Detailed Code Modifications

### 1. Double-Ticking Removal ([`AnimationHook.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/AnimationHook.cpp))
In `PlayerHook::Hook`, the call to `AnimationHook::TickAllActors(a_delta)` was removed:
```cpp
// 3rd Person: Player eyes engage TrueGaze normally.
GazeEngine::Get().TickActor(a_actor, a_delta);
// (TickAllActors removed: NPCs are simulated exclusively within CharacterHook::Hook)
```

### 2. Cart Passenger Singularity Defense ([`TargetSelector.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TargetSelector.cpp) & [`GazeEngine.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp))
1. In `IsInVisualCone`:
```cpp
// Reject co-located or stacked actors (< 25 units / ~0.35m), e.g. during Helgen carriage rides
// or when actors share marker origin. Returning true here caused atan2(0,0) singularity.
if (distSq < 625.0f)
{
    return false;
}
```
2. In `GetActorWorldPosition`:
```cpp
RE::NiPoint3 GetActorWorldPosition(RE::Actor *actor) noexcept
{
    if (!actor) return RE::NiPoint3{0.0f, 0.0f, 0.0f};
    if (auto *root = actor->Get3D())
    {
        return root->world.translate;
    }
    return actor->GetPosition();
}
```
3. In `WorldTargetToLocalGaze`:
```cpp
if (horizontal < 25.0f)
{
    outYawDeg = 0.0f;
    outPitchDeg = 0.0f;
    return;
}
```
4. In `ComputeDeflection`:
```cpp
RE::NiPoint3 actorPos = actor->GetPosition();
if (auto *root = actor->Get3D())
{
    actorPos = root->world.translate;
}
WorldTargetToLocalGaze(actorPos, actor->GetAngleZ(), targetPos, desiredYaw, desiredPitch);
```

### 3. Biomechanical Cervical Clamping ([`VorCoordinator.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Kinematics/VorCoordinator.hpp))
```cpp
constexpr float HEAD_YAW_LIMIT = 70.0f;
constexpr float HEAD_PITCH_LIMIT_DOWN = 35.0f;
constexpr float HEAD_PITCH_LIMIT_UP = 45.0f;

state.headYaw = std::clamp(state.headYaw + deltaYaw, -HEAD_YAW_LIMIT, HEAD_YAW_LIMIT);
state.headPitch = std::clamp(state.headPitch + deltaPitch, -HEAD_PITCH_LIMIT_DOWN, HEAD_PITCH_LIMIT_UP);
```

### 4. Liveness Check in `Withdraw()` ([`EyeAimConstraint.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/EyeAimConstraint.cpp))
```cpp
for (uint32_t i = 0; i < g_touchedCount; ++i)
{
    TouchedBone &slot = g_touched[i];
    if (slot.active && slot.bone)
    {
        auto *form = RE::TESForm::LookupByID(slot.actorFormId);
        auto *actor = form ? form->As<RE::Actor>() : nullptr;
        if (actor && actor->Get3D())
        {
            slot.bone->local.rotate = slot.originalRotate;
            RefreshWorldTransform(slot.bone);
        }
    }
    slot.active = false;
    slot.actorFormId = 0;
    slot.bone = nullptr;
}
```

### 5. Bone Pointer Caching ([`ActorGazeRuntime.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ActorGazeRuntime.hpp) & [`GazeEngine.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp))
Added caching fields to `ActorGazeRuntime`:
```cpp
bool skeletonResolved{false};
RE::NiAVObject *cachedRoot{nullptr};
RE::NiAVObject *cachedSpine{nullptr};
RE::NiAVObject *cachedNeck{nullptr};
RE::NiAVObject *cachedHead{nullptr};
RE::NiAVObject *cachedEyeL{nullptr};
RE::NiAVObject *cachedEyeR{nullptr};
```
In `GazeEngine::ApplyToSkeleton`, probe and fuzzy searches only execute when `!state.skeletonResolved || state.cachedRoot != root`.

---

## Verification & Build Results

### 1. Pinned Release Build
- **Command**: `cmake --build --preset release`
- **Result**: Exit code 0.
- **Artifact**: `bin/Release/TrueGaze.dll` (661.5 KB), `bin/Release/KinematicsTests.exe`.

### 2. Biomechanical Kinematics Test Suite
- **Command**: `.\bin\Release\KinematicsTests.exe`
- **Result**:
  ```
  ========================================================
    SkyrimTrueGaze Kinematics & Mathematics Test Suite   
    An HCEP Product by Kirk LaSalle                      
  ========================================================
  [TEST] Running SaccadeGenerator verification...
    -> SaccadeGenerator passed.
  [TEST] Running VorCoordinator verification...
    -> VorCoordinator passed (including steep cervical clamping).
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

### 3. Real-Time IPC Named Pipe Test Harness
- **Command**: `.\bin\Release\HcepBridgeClientMock.exe`
- **Result**: Connected to `\\.\pipe\TrueGazeBridge`, streamed 10 telemetry frames across all cognitive modes, validated 32-byte `SkyrimFeedbackPacket` response.
- **Status**: **100% Passed**.

### 4. Permanent Active Directives (PAD) Integrity
- **Command**: `python scripts/verify_charter.py --verbose`
- **Result**: All 10 Charter Laws intact and fully compliant.

### 5. Deployment & Pre-Flight Diagnostics
- **Command**: `.\scripts\Deploy-TrueGaze.ps1`
- **Result**:
  - Build: Succeeded
  - Deployed: `TrueGaze.dll` and `TrueGaze.ini` deployed to `G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\`.
  - Diagnostics: **14/14 checks passed**.
  - Verdict: **Clear to launch**.
