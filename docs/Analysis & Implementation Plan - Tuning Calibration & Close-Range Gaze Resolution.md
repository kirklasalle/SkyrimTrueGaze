# Analysis & Implementation Plan: Tuning Calibration & Close-Range Gaze Resolution

## Analysis of Screenshots 05–09 & Timestamp Logs

### 1. The Delphine Averted-Gaze Bug (`ingame_ScreenShot_09.png`)
In **Screenshot 09** (Subtle preset, 1:15:52 PM), the player is point-blank (< 0.65m) looking directly at Delphine's face at the Sleeping Giant Inn alchemy bench. The HUD reports `[TrueGaze] Eye Contact Held: Delphine`, but Delphine's head is turned ~40°–45° to her left (screen right, towards the wall), completely looking away from the player.

#### Root Causes Identified from Code & Telemetry:
1. **Double-Rotation Stacking with Vanilla Skyrim Headtracking**:
   - In `EyeAimConstraint.cpp`:
     ```cpp
     slot->bone->local.rotate = MakeGazeRotation(yawDeg, pitchDeg) * slot->originalRotate;
     ```
     `slot->originalRotate` is the bone rotation from Skyrim's engine before TrueGaze runs.
   - When an NPC is near the player, Skyrim's native AI (`HighProcessData`) **already rotates the head toward the player** via vanilla headtracking.
   - TrueGaze then measures the target angle relative to the actor's root body orientation (`actor->GetAngleZ()`), calculating ~+41.4° (Delphine facing the alchemy table while player is to her right).
   - TrueGaze then applies that full +41.4° **on top of** the already-turned head bone, rotating the head to ~80° (completely past the player and into the wall)!
2. **Close-Proximity Visual Cone Drop**:
   - In `TargetSelector.cpp`, `IsInVisualCone` was capped at `kMaxHoldVisualConeAngleDeg = 75.0f`.
   - Because Delphine is locked to the alchemy table facing East/SE, when the player stood beside her at ~80°, Delphine's `crosshairHoldTimerSec` abruptly dropped to `0.0f` and fell through to `TargetPriority::AmbientInterest` (which points straight ahead along her body orientation).
3. **Preset Hierarchy Weights "Too Strong"**:
   - In both "Subtle" and "Social" presets, the bone hierarchy strain weights were set to default:
     `fSpine2YawWeight = 0.10`, `fNeckYawWeight = 0.25`, `fHeadYawWeight = 0.65` (Sum = 1.00 = 100%).
   - On vanilla humanoids (which have no eye bones; eyes are driven by FaceGen morphs), the head chain takes 100% of the yaw. This causes the head to whip and over-rotate aggressively.
   - `bEnableGazeAversion = true` was active, which periodically injects a +/-22° yaw jump.
   - `fHeadTrackingSpeed` in Subtle was `4.5` (too fast) and in Social was `5.5`.
4. **False Mutual Gaze in Screenshot 08 (`[TrueGaze] Eye Contact Held: Sven` while looking at Orgnar)**:
   - In `PlayerGazeResolver.cpp` line 263:
     `result.onFace = (faceAngleDeg <= toleranceDeg) || (distanceMeters <= 4.0f);`
     The clause `|| (distanceMeters <= 4.0f)` forced `onFace = true` for *any* actor within 4 meters regardless of where the crosshair was aimed, causing Sven to retain mutual gaze while the player was looking at Orgnar.

---

## Proposed Technical Changes

### 1. Gaze Engine & Kinematics (`src/Engine/GazeEngine.cpp` & `src/Engine/AnimationHook.cpp`)
- **Suppress Vanilla Headtracking for Active TrueGaze Actors**:
  - In `GazeEngine::TickActor`, when TrueGaze is actively solving gaze for an NPC (`state.active`), clear Skyrim's vanilla headtracking targets on that actor (`actor->GetcurrentProcess()->ClearActionHeadtrackTarget(false)` or `highProcess->ClearHeadtrackTarget(...)`).
  - This ensures `slot->originalRotate` remains at the actor's natural baseline animated posture without double-rotation stacking.
- **Dampen Head Movement in Subtle & Social Presets**:
  - Distribute hierarchy strain so the head chain provides gentle, organic orientation support without snapping.

### 2. Target Selector (`src/Engine/TargetSelector.cpp`)
- **Expand Point-Blank & Hold Visual Cones**:
  - Increase `kMaxHoldVisualConeAngleDeg` from `75.0f` to `95.0f`.
  - At point-blank range (< 1.5m), allow natural head turning up to `95.0f` degrees so an NPC standing beside the player doesn't abruptly drop eye contact.

### 3. Player Gaze Resolver (`src/Engine/PlayerGazeResolver.cpp`)
- **Remove False-Positive Distance Override**:
  - Remove `|| (distanceMeters <= 4.0f)` from `result.onFace`.
  - Keep `pointBlank` angular tolerance (89°) for close range (< `pointBlankMeters`), so looking directly at the face/head remains reliable without falsely triggering across the entire room.

### 4. Presets Recalibration (`TrueGazeConfig.html`, `skyrim/SKSE/Plugins/TrueGaze.ini`, `src/Engine/ConfigManager.cpp`)
- **Subtle & Natural**:
  - `fHeadTrackingSpeed`: `4.5` -> **`2.5`** (calm, gradual tracking)
  - `fMicroJitterAmp`: `0.20` -> **`0.09`** (subtle biological tremor, no shaking)
  - `fMaxComfortEyeAngle`: `30.0` -> **`22.0`**
  - `fSpine2YawWeight`: `0.10` -> **`0.03`**
  - `fNeckYawWeight`: `0.25` -> **`0.12`**
  - `fHeadYawWeight`: `0.65` -> **`0.35`** (Total head yaw deflection = `0.50` instead of `1.00`)
  - `bEnableGazeAversion`: `true` -> **`false`**
- **Social & Dialogue**:
  - `fHeadTrackingSpeed`: `5.5` -> **`3.2`**
  - `fMicroJitterAmp`: `0.30` -> **`0.14`**
  - `fMaxComfortEyeAngle`: `35.0` -> **`25.0`**
  - `fSpine2YawWeight`: `0.10` -> **`0.05`**
  - `fNeckYawWeight`: `0.25` -> **`0.18`**
  - `fHeadYawWeight`: `0.65` -> **`0.45`** (Total head yaw deflection = `0.68`)
  - `bEnableGazeAversion`: `true` -> **`false`**
- **Vanilla Balanced**:
  - `fHeadTrackingSpeed`: `6.0` -> **`4.0`**
  - `fMicroJitterAmp`: `0.35` -> **`0.18`**
  - `fSpine2YawWeight`: `0.06`, `fNeckYawWeight`: `0.20`, `fHeadYawWeight`: `0.55`

---

## Verification Plan
1. **Automated Tests**:
   - Run `bin/KinematicsTests.exe` to ensure mathematical validity of saccades, VOR coordinator, and strain calculations.
2. **Build & Deploy**:
   - Run `cmake --build --preset release`
   - Deploy fresh DLL & INI using `scripts/Deploy-TrueGaze.ps1` to the game's SKSE plugin directory.
3. **In-Game Playtest Verification**:
   - Test "Subtle & Natural" and "Social" presets in Sleeping Giant Inn with Delphine at the alchemy table, Sven playing flute, and Orgnar at the counter.
   - Confirm Delphine turns her head to directly face the player when approached point-blank, without overshooting or turning away.
