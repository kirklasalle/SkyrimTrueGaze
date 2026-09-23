# Implementation Plan: Game Shutdown Hang Fix, Debug Gaze Rays & 3rd-Person Player Gaze

Address three key runtime engine behaviors reported by Kirk:
1. **Shutdown Hang / Delay**: Eliminate thread-joining hangs during game exit by introducing safe join timeouts, `std::atexit` cleanup, and non-blocking overlapped pipe writes.
2. **Debug Gaze Rays & Log Level**: Connect `iLogLevel` and `bDebugGazeRays` from `TrueGaze.ini` directly to `spdlog`, and implement 3D line-of-sight ray tracing diagnostics in the log.
3. **Player Gaze in 1st vs 3rd Person**: Enable TrueGaze biological eye engagement on the player character when in 3rd person (tracking dialogue partners, crosshair targets, and nearby NPCs), while keeping player eyes/head aligned to mouse controls in 1st person.

---

## User Review Required

> [!IMPORTANT]
> **iLogLevel Setting Clarification**:
> In Skyrim/spdlog standards:
> - `0` = Trace (maximum verbosity)
> - `1` = Debug (detailed diagnostic info & rays)
> - `2` = Info (standard gameplay logging, default)
> - `3` = Warn (warnings and errors only)
> - `4` = Error (suppresses everything except fatal errors)
>
> Setting `iLogLevel = 4` suppressed all normal engine logs. We will ensure `bDebugGazeRays=true` logs ray tracing diagnostics regardless, and update the HTML configurator with clear explanatory labels for the logging levels so players know `0`/`1` enables full verbose diagnostics.

---

## Proposed Changes

### Bridge & Shutdown Subsystem

#### [MODIFY] [`NamedPipeServer.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Bridge/NamedPipeServer.cpp)
- **Deadlock-Free `Stop()`**: When Skyrim exits, Windows acquires the OS Loader Lock before static destructors run. Calling `_workerThread.join()` unconditionally causes a hang. We will:
  - Disconnect the pipe and cancel pending I/O immediately.
  - Wait up to 300ms on `_workerThread.native_handle()`.
  - If the thread terminates, join cleanly; if not, safely `detach()` so Skyrim exits instantly without hanging.
- **Non-Blocking `DrainOutboundQueue`**: Replace the blocking `GetOverlappedResult(..., TRUE)` with an overlapped wait that respects `_shutdownEvent` and timeouts.

#### [MODIFY] [`Main.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Main.cpp)
- Register `std::atexit([]() { TrueGaze::Engine::GazeEngine::Get().StopBridge(); });` so the bridge server shuts down cleanly before `ExitProcess` initiates loader-lock termination.
- Apply `config.logLevel` directly to `spdlog::default_logger()->set_level(...)` whenever the configuration is loaded or reloaded.

---

### Player Gaze & Camera Modes (1st Person vs 3rd Person)

#### [MODIFY] [`AnimationHook.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/AnimationHook.cpp)
- In `PlayerHook` (`PlayerCharacter::Update`):
  - Check `RE::PlayerCamera::GetSingleton()->IsInThirdPerson()`.
  - **In 3rd Person**: Allow `GazeEngine::Get().TickActor(a_actor, a_delta)` to run for the player.
  - **In 1st Person**: Ensure any deflections are withdrawn (`EyeAimConstraint::WithdrawActor(playerFormId)`), letting the player's mouse crosshair and view directly guide looking.

#### [MODIFY] [`GazeEngine.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp)
- In `ApplyToSkeleton`:
  - When `actor->IsPlayerRef()` is true, **only apply deflection to `eyeL` and `eyeR` (and eyelid blink morphs)**.
  - Do NOT deflect the player's spine, neck, or head. This preserves 100% of the player's third-person movement, aiming, attacking, and camera control while giving the player character living, responsive biological eyes.

#### [MODIFY] [`TargetSelector.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TargetSelector.cpp)
- For the player character in 3rd person:
  - If Dialogue Menu is open, look at the speaking NPC (`MenuTopicManager::GetSingleton()->speaker`).
  - If player is looking at an NPC / actor, look at their face.
  - If in combat, look at the combat target (`currentCombatTarget`).
  - If near speaking NPCs, look at the active speaker.
  - Otherwise, maintain ambient focus along the player's facing direction.

---

### Diagnostic Gaze Rays

#### [MODIFY] [`GazeEngine.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp)
- In `TickActor`:
  - When `_tuning.debugGazeRays` is enabled (or debug log level is active), log periodic 3D gaze ray diagnostics (ray origin, target coordinates, calculated yaw/pitch, target distance and priority) so the player can verify gaze tracking in `TrueGaze.log`.

---

## Verification Plan

### Automated Tests
1. Build the updated DLL using CMake:
   ```cmd
   cmake --build d:\Projects\SkyrimTrueGaze\build\windows-release --config Release
   ```
2. Run kinematics regression tests:
   ```cmd
   d:\Projects\SkyrimTrueGaze\bin\Release\KinematicsTests.exe
   ```
3. Run pipe server mock test to verify overlapped non-blocking shutdown:
   ```cmd
   d:\Projects\SkyrimTrueGaze\bin\Release\HcepBridgeClientMock.exe
   ```
4. Deploy the updated `TrueGaze.dll` to `G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\`.

### Manual In-Game Verification
- Launch Skyrim via `skse64_loader.exe` (or the HTML configurator's **Launch Skyrim** button).
- Test switching between 1st person and 3rd person:
  - In 1st person: mouse crosshair directs looking normally.
  - In 3rd person: player's eyes engage TrueGaze, tracking NPCs and targets.
- Test quitting the game: confirm instant, clean shutdown with no hangs.
- Inspect `SKSE\TrueGaze.log` to confirm log level and debug gaze ray outputs.
