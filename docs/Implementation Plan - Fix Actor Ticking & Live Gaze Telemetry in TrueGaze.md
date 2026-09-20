# Implementation Plan: Fix Actor Ticking & Live Gaze Telemetry in TrueGaze™

## Problem Diagnosis & Root Cause Analysis

During Kirk's play-test:
- NPCs turned to look at the player using vanilla Skyrim static head-tracking and locked on without eye drift, saccades, or biological latency gap.
- Running `tgstatus` in the console returned `0` for all counters (`tracked actors 0`, `tick calls 0`, `eligible ticks 0`, `target resolves 0`, etc.).

### Root Causes Identified in SKSE Logs (`TrueGaze.log`):
1. **Uncalled Batch Ticking (`TickAllActors`)**:
   `AnimationHook::TickAllActors(float deltaSeconds)`—the function responsible for iterating `RE::ProcessLists::GetSingleton()->highActorHandles` and `middleHighActorHandles` to tick all active NPCs—was **never called anywhere in the codebase**.
2. **First-Person Camera Filter**:
   In `PlayerTag` (`PlayerCharacter::Update`), `GazeEngine::Get().TickActor` was only executed if `isThirdPerson == true`. When playing in 1st person, the player was withdrawn and no other actors were ticked.
3. **Zero Delta on Havok Worker Threads**:
   In Skyrim SE/AE, `Actor::Update` / `Character::Update` (slot `0xAD`) is dispatched across background Havok worker threads (e.g. Thread `13716` in `TrueGaze.log`) with **`a_delta = 0.0000`**. The guard `if (a_delta <= 0.0f) return;` caused an immediate exit for every NPC on every frame.
4. **Thread Safety**:
   NetImmerse scene graph manipulations (`NiNode::UpdateWorldData`, bone rotations) and singletons (`PlayerCharacter`, `ProcessLists`) must only run on the main game thread (Thread `8224`).

---

## Proposed Changes

### Component: Engine Hook & Frame Orchestration

#### [MODIFY] [`src/Engine/AnimationHook.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/AnimationHook.cpp)
- **In `PlayerTag` (`PlayerCharacter::Update` on Main Game Thread)**:
  1. Call `EyeAimConstraint::BeginFrame()` to restore all touched bones from the previous frame before computing new deflections.
  2. If in 3rd person, tick the player (`GazeEngine::Get().TickActor(a_actor, a_delta)`). If in 1st person, withdraw procedural gaze from the player.
  3. **Call `AnimationHook::TickAllActors(a_delta)`** every frame when `a_delta > 0.0f && a_delta < 0.5f`. This guarantees that all nearby NPCs and creatures in `highActorHandles` and `middleHighActorHandles` are evaluated and posed on the main thread with the game's actual render frame delta.
  4. Call `GazeEngine::Get().EndFrame(a_delta)`.
- **In `ActorTag` / `CharacterTag`**:
  - Worker thread calls are safely bypassed after `_original(a_actor, a_delta)` to avoid multi-threaded race conditions on the NetImmerse scene graph.
- **In `TickAllActors` / `TickActorList`**:
  - Add robust index-based iteration across `highActorHandles` and `middleHighActorHandles`.
  - Add initial diagnostic log reporting active actor count and delta time.

---

## Verification Plan

### Automated Tests
- Build `windows-release` with MSVC.
- Run `bin/KinematicsTests.exe` to verify 100% pass on all 8 biomechanical kinematics test suites.
- Run `verify_charter.py` to ensure charter compliance.
- Run `scripts/Test-TrueGazeHealth.ps1` to verify binary exports, slot `0xAD` signature, and exception guards.

### In-Game Verification
- Deploy `TrueGaze.dll` and `TrueGaze.ini` to `G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\`.
- Launch Skyrim and load a save in Riverwood Trader or Whiterun.
- Stand near Lucan, Camilla, or town guards.
- Open console (`~`) and execute `tgstatus`:
  - `tracked actors` > 0
  - `tick calls` > 0
  - `eligible ticks` > 0
  - `target resolves` > 0
  - `saccades/blinks` > 0
- Observe NPCs:
  - Eyes drift naturally with micro-jitter mean reversion.
  - Saccades jump smoothly between eyes and mouth (social triangle).
  - Head moves after eyes with 120ms biological latency gap.
