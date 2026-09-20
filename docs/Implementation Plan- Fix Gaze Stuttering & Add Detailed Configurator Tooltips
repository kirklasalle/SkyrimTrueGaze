# Implementation Plan: Fix Gaze Stuttering & Add Detailed Configurator Tooltips

## Problem Statement & Root Cause Analysis
Kirk LaSalle reported:
1. *"The faces are now stuttering left/right or to just the shoulder and back, it looks like a horror effect for the head to stutter like that. Please fix."*
2. *"In addition please add detailed mouse over tips for the HTML TrueGaze Config."*
3. Provided reference context regarding Expressive Facial Animation (EFA/EFM) and eye morph replacement mods.

### Detailed Root Cause of the "Horror Stutter":
1. **Missing Visual Cone (Field of Regard) Filter in TargetSelector**:
   - In `TargetSelector.cpp`, nearby NPC selection was purely distance-based (`d < closestDistMeters`). If an NPC or companion was standing *behind* the observer (e.g. 1.5m behind their back or at their flank), the observer selected them as their gaze target.
   - The computed local yaw angle was between $90^\circ$ and $180^\circ$ (confirmed in Kirk's log: `Gaze Yaw=+121.5deg`).
   - `BoneController` clamped this angle to the maximum cervical limit (`CHAIN_YAW_LIMIT = 70.0f`), forcing the actor's head and neck to twist violently backwards all the way to their shoulder.
   - When the target or observer moved slightly across the $\pm 180^\circ$ boundary or distance threshold, the target angle flipped from $+70^\circ$ to $-70^\circ$, or snapped from $+70^\circ$ to ambient $0^\circ$ and back, producing an unnatural, rapid head-snapping "horror effect."
2. **Zero Fixation Hysteresis (Target Flapping)**:
   - When multiple NPCs (or player and NPC) were at similar distances (e.g. 2.9m and 3.1m), floating point jitter caused `ResolveTarget` to alternate targets frame-by-frame. Every flip triggered `SaccadeGenerator::TriggerSaccade()`, throwing the head back and forth continuously.
3. **Double-Tick per Frame**:
   - `CharacterHook::Hook` ticked humanoid actors on their update, and `PlayerTag` called `AnimationHook::TickAllActors`, causing NPCs in `highActorHandles` to be ticked *twice* per frame with double delta-time integration and conflicting bone states.

---

## Proposed Changes

### 1. Target Selection: Visual Cone Constraint & Fixation Hysteresis
#### [MODIFY] [TargetSelector.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/TargetSelector.cpp)
- **Natural Visual Cone Check**:
  - Calculate relative local yaw for all candidate targets: `localYawDeg = WrapPi(bearing - observerFacingRad) * kRadToDeg;`
  - Enforce comfortable humanoid visual cone: If $|localYawDeg| > 65.0^\circ$, the candidate is outside the actor's field of regard. Reject the candidate so the actor does not twist their neck backwards.
- **Target Fixation Hysteresis**:
  - Add a 1.5-second minimum fixation dwell time and a distance advantage buffer ($0.8\text{m}$) for the currently locked target.
  - An actor will stay locked on their current target smoothly rather than flapping between nearby characters.
- **Smooth Return to Forward Gaze**:
  - When no valid targets exist within the forward $65^\circ$ cone, default smoothly to `AmbientInterest` directly ahead of the actor ($localYaw = 0^\circ$).

### 2. Engine & Animation: Frame Deduplication & Eye Morph Integration
#### [MODIFY] [ActorGazeRuntime.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ActorGazeRuntime.hpp)
- Add `uint64_t lastFrameTicked{0};` to track the engine frame index.
- Add `float fixationHoldSec{0.0f};` and `uint32_t activeTargetId{0};` to support fixation dwell time.

#### [MODIFY] [GazeEngine.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.hpp) & [GazeEngine.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp)
- Add atomic monotonic `_globalFrameIndex` incremented once per frame in `EndFrame()`.
- In `GazeEngine::TickActor`:
  - If `state.lastFrameTicked == _globalFrameIndex`, return immediately. This eliminates double-ticking between `CharacterHook` and `TickAllActors`.
  - Mark `state.lastFrameTicked = _globalFrameIndex`.

#### [MODIFY] [EfmBlinkController.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Integrations/EfmBlinkController.hpp) & [EfmBlinkController.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Integrations/EfmBlinkController.cpp)
- Extend `EfmBlinkController` to support **FaceGen Eye Direction Modifiers** (`LookLeft`, `LookRight`, `LookDown`, `LookUp` in `BSFaceGenKeyframeMultiple::Modifier`).
- In `ApplyMorphs(actorFormId, eyelidWeight, eyeYawDeg, eyePitchDeg)`:
  - Normalize `eyeYaw` and `eyePitch` to $[0.0, 1.0]$ modifier weights:
    - If `eyeYaw < 0.0f`: `LookLeft = std::clamp(-eyeYaw / 30.0f, 0.0f, 1.0f);`
    - If `eyeYaw > 0.0f`: `LookRight = std::clamp(eyeYaw / 30.0f, 0.0f, 1.0f);`
    - If `eyePitch < 0.0f`: `LookDown = std::clamp(-eyePitch / 25.0f, 0.0f, 1.0f);`
    - If `eyePitch > 0.0f`: `LookUp = std::clamp(eyePitch / 25.0f, 0.0f, 1.0f);`
  - This natively powers Expressive Facial Animation (EFA - Female & Male Edition) and vanilla eye tracking morphs!

### 3. HTML TrueGaze Configurator: Rich Interactive Mouse-Over Tooltips
#### [MODIFY] [TrueGazeConfig.html](file:///d:/Projects/SkyrimTrueGaze/TrueGazeConfig.html)
- Create a comprehensive dictionary `TOOLTIP_GUIDE` with detailed, user-friendly explanations for every parameter across all sections (`General`, `Kinematics`, `SkeletalHierarchy`, `Social`, `Crosshair`, `Bridge`, `LOD`, `Debug`).
  - **Biomechanical Meaning**: Deep explanation of the biological mechanism (Main Sequence saccades, VOR decoupling, Brownian micro-jitter drift, Argyle & Cook social triangle, cognitive aversion).
  - **In-Game Visual Impact**: Clear description of what the player will witness on NPCs and character faces.
  - **Recommended Values**: Specific guidance for different playstyles (Realistic, Subtle, Cinematic).
- Add CSS styling for floating, rich interactive tooltip cards (`.gaze-tooltip`) featuring:
  - Gold accent header with parameter name and INI key.
  - Badge with default value and unit.
  - Rich body text with bulleted insights.
  - Micro-animation fade and positioning attached to cursor / control row.

---

## Verification Plan

### Automated Verification
1. Rebuild the SKSE plugin in Release x64:
   `cmake --build d:\Projects\SkyrimTrueGaze\build\windows-release --config Release`
2. Run standalone kinematics test suite:
   `d:\Projects\SkyrimTrueGaze\bin\Release\KinematicsTests.exe`
3. Deploy new `TrueGaze.dll` to:
   `G:\Program Files (x86)\Steam\steamapps\common\Skyrim Special Edition\Data\SKSE\Plugins\TrueGaze.dll`

### Visual & Browser Verification
1. Inspect `TrueGazeConfig.html` in browser using `browser_subagent`.
2. Hover over various settings (e.g. `fSaccadeSpeedMult`, `fHeadTrackingSpeed`, `bEnableCrosshairGaze`, `fMicroJitterAmp`).
3. Take high-resolution screenshot artifacts of the new tooltips in action to verify aesthetics and typography.
