# TrueGaze™ — Engine Actor Ticking Remediation & Live Status Walkthrough

**Architect & Owner:** Kirk LaSalle  
**Status Date:** September 20, 2026  
**Milestone:** Engine Hook Actor Ticking Remediation, Automated Verification, and Skyrim Deployment  

---

## 🔍 Investigation & Root Cause Forensics

### The Problem Observed During Play-Testing:
1. NPCs and creatures turned their heads to face the player, but locked on rigidly like mannequins.
2. There was no ocular drift, no look-away saccade, no social triangle, and no return to eye contact.
3. Running `tgstatus` in the console printed `0` across every operational metric (`tracked actors 0`, `tick calls 0`, `eligible ticks 0`, etc.).

### Findings from `G:\Users\kirkl\Documents\My Games\Skyrim Special Edition\SKSE\TrueGaze.log`:
```
[12:02:44.942] [8224 ] [I] [TrueGaze] Actor update hook invoked: form=00000014 delta=0.0167
[12:02:44.945] [13716] [I] [TrueGaze] Actor update hook invoked: form=0009B0DC delta=0.0000
[12:04:56.368] [8224 ] [I] [TrueGaze] TrueGaze - status
  tracked actors   0
  tick calls       0
  eligible ticks   0
```

1. **Uncalled Batch Ticking**:
   The engine method [`AnimationHook::TickAllActors(float deltaSeconds)`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/AnimationHook.cpp), which iterates `RE::ProcessLists::GetSingleton()->highActorHandles` and `middleHighActorHandles` to tick all loaded NPCs, was **never called** in the runtime loop.
2. **First-Person Camera Filter**:
   In `PlayerTag` (`PlayerCharacter::Update` on main thread `8224`), `TickActor` was only executed if `isThirdPerson == true`. When playing in 1st person, the player's procedural gaze was withdrawn (allowing crosshair aim), but no other actors were ticked.
3. **Zero Delta on Havok Background Worker Threads**:
   In Skyrim SE/AE, `Actor::Update` / `Character::Update` (slot `0xAD`) for NPCs is invoked on Havok job worker threads (thread `13716`) with `a_delta = 0.0000`. The code guard `if (a_delta <= 0.0f) return;` caused an immediate early return on line 150 for every NPC on every frame.
4. **Vanilla Head-Tracking**:
   Because TrueGaze was not ticking, vanilla Skyrim head-tracking took over and aimed the NPC's head rigidly at the player with zero ocular drift, zero saccades, and zero biological latency gap.

---

## 🛠️ Code Changes Implemented

### Engine Hook & Main-Thread Orchestration ([`src/Engine/AnimationHook.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/AnimationHook.cpp))

1. **Orchestrated `TickAllActors` on Main Game Thread**:
   In `PlayerTag` (`PlayerCharacter::Update`, which runs reliably every render frame on thread `8224` with the real frame delta `a_delta = 0.0167s`):
   - Prepares bone constraints for the frame with `EyeAimConstraint::BeginFrame()`, restoring previously deflected bones to their pristine baseline so deflections never compound.
   - If the player is in 3rd person, ticks the player. If in 1st person, withdraws the player.
   - **Calls `AnimationHook::TickAllActors(a_delta);`**: Iterates all active NPCs and creatures in `highActorHandles` and `middleHighActorHandles` on the main game thread, perfectly synchronized with the game's display rate.
   - Calls `GazeEngine::Get().EndFrame(a_delta)`.
2. **Safely Bypassed Worker Threads**:
   Non-player calls to slot `0xAD` on Havok background worker threads return immediately after calling `_original(a_actor, a_delta)`. This prevents multi-threaded race conditions and heap corruption on NetImmerse scene graph nodes (`NiNode`) and singletons.
3. **Robust Index-Based Actor Iteration**:
   Updated `TickActorList` to use index-based iteration across `list.size()` with null and eligibility checks.
4. **Diagnostic Telemetry**:
   Added a one-time log to `TickAllActors` confirming the number of high and middle-high actors and frame delta.

---

## 🧪 Verification & Health Check Results

### 1. Standalone Kinematics & Mathematics Test Suite
```powershell
.\bin\Release\KinematicsTests.exe
```
**Result:** 100% Pass across all 11 test suites:
- `SaccadeGenerator` (velocity and duration scaling, ballistic stepping)
- `VorCoordinator` (including steep cervical clamping & biological latency gap verification)
- `MicroJitter` bounded-drift & Brownian/seeding
- `SocialTriangle` scanpath
- `BoneController` hierarchy strain & eye residual allocation
- `Main Sequence` profile fidelity
- `LodManager` spatial degradation
- `EfmBlinkController` parabolic dip
- `TelemetryPacket` layout (64-byte / 32-byte wire protocol & CRC32)
- `HCEP` semantic validation

### 2. Charter Law Verification
```powershell
python scripts/verify_charter.py
```
**Result:** 0 unrecorded charter divergences. Charter Laws intact.

### 3. Pre-Flight Health Check & Game Deployment
```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Deploy-TrueGaze.ps1 -GamePath "G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition" -NoLaunch
```
**Result:** 15 Passed, 0 Warnings, 0 Failed:
- `[ OK ]` Skyrim Special Edition located (`1.7.104.0`)
- `[ OK ]` skse64_loader.exe & Address Library (`versionlib-1-7-104-0.bin`) verified
- `[ OK ]` SKSE loader contract satisfied (all 3 exports present)
- `[ OK ]` Gaze driver targets Actor::Update (slot 0xAD)
- `[ OK ]` Tick exception guard compiled in
- `[ OK ]` Deployed DLL matches build (`TrueGaze.dll` -> `Data\SKSE\Plugins\TrueGaze.dll`, 712.5 KB)
- `[ OK ]` TrueGaze.ini deployed and parseable
- `VERDICT: Clear to launch.`

### 4. Production Release Archives Rebuilt
Executed [`scripts/PackageMod.ps1`](file:///d:/Projects/SkyrimTrueGaze/scripts/PackageMod.ps1):
- **Mod Package:** `dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip` (305.38 KB)  
  *SHA-256:* `98D9654F807D1530B6619321C6FE9D97E0618AF2E6CD58DA20DE4AF3A19065DF`
- **Symbols Package:** `dist/TrueGaze-v1.0.0-Symbols.zip` (5.37 MB)  
  *SHA-256:* `FFCC6680C5EE03F1E47A94098412582BBC7C21A592D2E17C79F2F60D3426BA94`

---

## 🎮 How to Verify in Game

1. Start Skyrim using `skse64_loader.exe` (or launch via your mod manager).
2. Load any save near NPCs (e.g. Riverwood Trader or Whiterun).
3. Look at an NPC (Lucan, Camilla, town guard, etc.):
   - Observe their eyes: they will make eye contact, subtly drift with micro-jitter (they are alive!), execute social triangle saccades between eyes and mouth, look away thoughtfully, and return.
   - Notice the **Biological Latency Gap**: when shifting attention, the NPC's eyes snap first, and the head smoothly follows ~120 ms later.
4. Open the console (`~`) and enter:
   ```
   tgstatus
   ```
   Verify that:
   - `tracked actors` > 0
   - `tick calls` > 0 (will be actively accumulating)
   - `eligible ticks` > 0
   - `target resolves` > 0
   - `saccades/blinks` > 0
   - `bio latency` shows `0.120 s`
