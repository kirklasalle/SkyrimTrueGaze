# TrueGaze User Guide

**Product:** TrueGaze - Biological NPC Gaze and Biomechanical Kinematics Engine  
**Audience:** Skyrim SE/AE/VR players, mod testers, and development partners  
**Status date:** September 23, 2026  
**Current release line:** 1.0.3 Production Release ([Nexus Mods #192480](https://www.nexusmods.com/skyrimspecialedition/mods/192480))  

> TrueGaze is an active production runtime engine. The plugin loads natively through SKSE across Skyrim SE, AE, and Skyrim VR (including "Mad God VR"). It drives biological oculomotor kinematics (VOR decoupling, Main Sequence ballistic saccades, Brownian micro-drift, Argyle & Cook social triangle cycling), dynamically resolves true 3D head-height elevation targeting (eliminating seated/crouched chest aiming), enables 3rd-person player conversational gaze engagement, and offers two optional in-engine 3D visual diagnostics: Option 1 (discreet ~2mm hair-thin laser rays originating from pupils tracking line of sight) and Option 2 (head-anchored HCEP floating diagram panel with real-time region highlight).

## 1. What TrueGaze Does

TrueGaze is an SKSE plugin that drives live actor gaze kinematics from Skyrim's actor update path. It combines target salience, saccadic movement, eye/head coordination, micro-saccadic drift, social attention, and optional HCEP telemetry.

The current runtime can:

- Track eligible living actors across SE, AE, and VR.
- Resolve a target from dialogue, combat, proximity, crosshair attention, or ambient interest.
- Apply hierarchical gaze motion through the available skeleton nodes.
- Use configurable saccade, VOR, comfort, jitter, social, and LOD parameters.
- Provide Option 1 (Superman laser eyes) and Option 2 (HCEP floating ocular diagram panel) developer visual overlays.
- Receive optional HCEP telemetry through a Windows named pipe.
- Report actor, target, skeleton, visual, and performance diagnostics through the Skyrim log and `tgstatus`.

The optional Visuals subsystem is an opt-in developer diagnostic. It is not required for the gaze kinematics engine itself.

<p align="center">
  <img src="images/target_salience_perception_cones.jpg" alt="In-Engine Gaze Perception & Target Salience Tracking" width="100%">
  <br>
  <em>Figure 1: In-engine target salience and perception field-of-view cones tracking environmental focal points and conversational targets in third-person view.</em>
</p>

## 2. Requirements

### Skyrim and runtime

- **Skyrim Special Edition (SE):** 1.5.97
- **Skyrim Anniversary Edition (AE):** 1.6.318 through 1.6.1170+ (including 1.7.104.0+)
- **Skyrim VR:** 1.4.15 (including Wabbajack modlists like "Mad God VR")
- Matching SKSE runtime (`skse64_loader.exe` / `sksevr_loader.exe`)
- Matching Address Library for SKSE Plugins (`.bin` for SE/AE, `.csv` for VR)
- Windows x64

### Plugin prerequisites

- SKSE64 / SKSEVR.
- Address Library for SKSE Plugins.
- Microsoft Visual C++ 2015-2022 x64 runtime.
- A working Skyrim Data directory.

TrueGaze does not require SkyUI, MCM Helper, an ESP/ESL plugin, Papyrus scripts, or an in-game MCM menu. Configuration is intentionally vanilla-UI and INI-based.

## 3. Installation Layout

A normal installation (via MO2, Vortex, or manual) places files at:

```text
Data\SKSE\Plugins\TrueGaze.dll
Data\SKSE\Plugins\TrueGaze.ini
Data\meshes\TrueGaze\GazeBeam.nif
Data\meshes\TrueGaze\GazeRegionPanel.nif
Data\textures\TrueGaze\GazeRegionPanel.dds
Data\meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze\config.json
```

The release package also contains the standalone configurator suite and guide:

```text
TrueGazeConfig.html
Launch-TrueGazeConfig.cmd
TrueGaze_Configurator_Guide.txt
tools\TrueGazeConfig\
```

Do not place the DLL beside `SkyrimSE.exe`. SKSE plugins belong under `Data\SKSE\Plugins`.

## 4. First Launch

1. Install SKSE and the matching Address Library.
2. Install `TrueGaze.dll` and `TrueGaze.ini`.
3. Start Skyrim through the SKSE loader.
4. Load an existing save or start a new game. A new game is not required.
5. Load into a cell containing a living humanoid NPC.
6. Switch to third-person view when testing player-attached visuals.
7. Wait several seconds for actor updates.
8. Open the vanilla console with `~`.
9. Run `tgstatus` if console commands are enabled.

A save reload or cell transition can rebuild actor 3D nodes, but neither is normally required. A new game does not solve a missing asset, a bad deployment, or an incorrect resource path.

## 5. Configurator Workflow

Launch `Launch-TrueGazeConfig.cmd` or open `TrueGazeConfig.html` through the repository's launcher.

The configurator can:

- Detect the Skyrim installation.
- Inspect saves and the current hero.
- Read and edit `TrueGaze.ini`.
- Apply quick presets.
- Show the selected preset with a silver active state.
- Display the current deployment and engine status.
- Save changes back to the live INI through the local automation bridge.

The top status cards use a classic Skyrim silver treatment. This is visual configurator styling only; it does not alter in-game rendering.

<p align="center">
  <img src="screengrabs/truegaze_config_03.png" alt="TrueGaze Configurator & Launcher (Live Execution Screenshot)" width="100%">
  <br>
  <em>Figure 2: Live screenshot of the standalone TrueGaze Configurator & Launcher—featuring automated Skyrim AE / SE installation detection, save game inspector, hero profile tracking, silver active presets, and real-time biological eye kinematics preview.</em>
</p>

### Presets

The built-in presets are tuning starting points, not separate engines:

- **Vanilla Balanced:** baseline saccade, jitter, head speed, social triangle, and crosshair gaze.
- **Subtle and Natural:** lower movement amplitude and slower head response.
- **Intense and Responsive:** faster saccades and head response with reduced aversion.
- **Social and Dialogue Focus:** dialogue-friendly tuning with a wider crosshair tolerance.

After selecting a preset, save the INI and restart Skyrim or use the supported reload path. The selected-preset indicator only identifies the last preset applied in the configurator; it does not prove that the game has loaded unsaved changes.

## 6. Important INI Settings

### General

```ini
[General]
bEnableTrueGaze=true
bEnableCreatures=true
sEngineTarget=Auto
```

`bEnableTrueGaze` is the master kinematics engine switch. `bEnableCreatures` determines whether non-humanoid actors may enter the biological gaze path.

### Kinematics

```ini
[Kinematics]
fSaccadeSpeedMult=1.0
fVelocitySaturation=14.0
fMicroJitterAmp=0.35
fMicroJitterIntervalMin=0.20
fMicroJitterIntervalMax=0.45
fHeadTrackingSpeed=6.0
fMaxComfortEyeAngle=35.0
```

Higher saccade speed produces faster ballistic eye transitions. Higher jitter is more visibly active but can become distracting. The comfort angle controls when the head/neck chain carries more of the turn.

### Social and crosshair attention

```ini
[Social]
bEnableGazeAversion=true
bEnableSocialTriangle=true
fTriangleFixationDuration=0.35
fMutualGazeThreshold=2.0

[Crosshair]
bEnableCrosshairGaze=true
fCrosshairToleranceDeg=4.0
fCrosshairMaxRangeMeters=25.0
fCrosshairPointBlankMeters=1.5
```

Crosshair gaze is the player-attention signal used for mutual-gaze behavior. It does not mean that every NPC is always selected as a target.

<p align="center">
  <img src="images/kinematics_social_triangle.jpg" alt="Biomechanical Oculomotor Diagnostic & Social Triangle Overlay" width="100%">
  <br>
  <em>Figure 3: Biomechanical diagnostic overlay illustrating the facial Social Triangle scanning pattern and VOR counter-rotation arc.</em>
</p>

### HCEP bridge

The HCEP Desktop bridge client is now part of the HCEP application. See [`HCEP_TRUEGAZE_BRIDGE_CLIENT.md`](HCEP_TRUEGAZE_BRIDGE_CLIENT.md) for its build, publish, connection, and troubleshooting procedure.

```ini
[Bridge]
bConnectHcepBridge=true
sPipeName=\\.\pipe\TrueGazeBridge
fAutoReconnectIntervalSec=3.0
```

The bridge is optional. When unavailable, TrueGaze uses autonomous Skyrim-side attention. When enabled, HCEP telemetry is received locally through a named pipe. Biometric data should only be collected with informed consent from everyone in front of the sensor.

<p align="center">
  <img src="images/hcep_bridge_architecture.jpg" alt="HCEP Bridge Architecture: Real-World Tracking to Skyrim Kinematics" width="100%">
  <br>
  <em>Figure 4: Real-world face/eye tracking streamed through the local named pipe into Skyrim's skeletal transform pipeline.</em>
</p>

### LOD

```ini
[LOD]
fTier1DistanceMeters=5.0
fTier2DistanceMeters=15.0
```

Tier 1 is the full close-range biological kinematics. Tier 2 reduces fine detail at distance. Actors beyond the configured range may be culled from the expensive gaze path.

### Developer visuals (Option 1 & Option 2)

TrueGaze includes two in-engine developer visual diagnostic systems (opt-in, disabled by default in production):

```ini
[Visuals]
bEnableInGameVisuals=true
bGazeRaysEnabled=true
iRayRenderMode=0
fGazeRayLengthMeters=10.0
fGazeRayOpacity=0.85
bGazeRaysOnPlayer=true
bGazeRaysOnNPCs=true
bGazeRaysOnCreatures=true
bGazeRaysAttachHead=true
bGazeRaysTerminus=true
fPupilForwardOffsetCm=7.0
fPupilUpOffsetCm=1.5
fPupilGlowIntensity=0.5

; Option 2: HCEP Floating Ocular Diagram Panel
bShowHcepPanel=true
bHcepPanelAllActors=true
fHcepPanelScale=0.35
fHcepPanelForwardOffsetCm=35.0
```

Visual subsystems:

- **Option 1: Superman Laser Eyes**:
  - Gaze beams render as pencil-thin (~8mm) laser rays projecting directly from anatomical pupil socket anchors.
  - Dynamically scaled in length based on actual target distance or `fGazeRayLengthMeters`.
  - Oriented dynamically to track computed saccadic/fixation eye line of sight (not head rotation).
- **Option 2: HCEP Floating Diagram Panel**:
  - A 3D planar quad rendering the chroma-keyed HCEP-02 ocular diagram (`GazeRegionPanel.nif` / `GazeRegionPanel.dds`).
  - Head-anchored and floating ~35cm in front of actor eyes, oriented toward the camera.
  - Highlights active gaze regions in real time with an emissive glow (Social Triangle, Mutual Gaze, Intimate, Avoidance, Target).
- **Visual Modes (`iRayRenderMode`)**:
  - `0`: Geometry plus point light emitters (`TrueGaze_PupilLight`, `TrueGaze_TerminusLight`).
  - `1`: Light emitters only (subtle gold ambient eye/terminus glow without geometry).
  - `2`: Geometry only (pure 3D mesh rays and panel quad).

You can toggle visuals live in-game at any time using the console command `stgvisuals` (or `stgv`).

### Console commands

```ini
[Console]
bEnableConsoleCommands=true
```

Console commands are enabled when `bEnableConsoleCommands = true` in `TrueGaze.ini`. To avoid name collisions with vanilla Skyrim commands or other mods, TrueGaze registers commands with the `stg` prefix:

## 7. Console Commands

The current registered in-game console commands are:

| Command | Purpose |
| --- | --- |
| `stg` | Toggle the master gaze kinematics engine on/off. |
| `stgvisuals` | Toggle all in-game visuals (master visual switch) on/off. |
| `stgv` | Toggle gaze-ray emitters (subtle laser rays) on/off. |
| `stgon` | Turn every in-game visual diagnostic on. |
| `stgoff` | Turn every in-game visual diagnostic off. |
| `stgmode` | Cycle render mode: `0` (Both) / `1` (Light only) / `2` (Geometry only). |
| `stgradius` | Toggle the gaze terminus landing glow. |
| `stgverbose` | Dynamically switch logging threshold between `Debug` and `Info` live without restarting. |
| `stgstatus` | Print effective runtime state, tracked actors, ray deflections, and telemetry counters. |

### Reading `stgstatus`

The most important fields are:

- `tracked actors`: actors with runtime gaze state.
- `tick calls`: actor update hook calls received by TrueGaze.
- `eligible ticks`: calls that passed actor eligibility checks.
- `LOD culled`: eligible calls stopped by distance LOD.
- `target resolves`: target selector executions.
- `visual emitters`: actors and attached light count.
- `visual updates`: visual subsystem calls.
- `anchors failed`: inability to find an attachment anchor.
- `light creates failed`: failed `NiPointLight` allocation.
- `beam geometry`: visible geometry attachment count and load attempts.

A healthy diagnostic example is:

```text
tracked actors   1
target resolves  119 (none 0)
visual emitters  1 actors, 2 lights
visual updates   119, anchors failed 0, light creates failed 0
beam geometry    0 attached / 119 attempts
```

The example proves execution and lights, but not a visible beam. For a visible geometry test, `beam geometry` must report an attached count greater than zero and the log must contain a successful attachment message.

## 8. Log Locations

At startup, the log now includes a runtime identity block containing the plugin build timestamp, detected Skyrim runtime, and effective INI path. Include this block in support reports.

TrueGaze log:

```text
Documents\My Games\Skyrim Special Edition\SKSE\TrueGaze.log
```

Companion logs:

```text
Documents\My Games\Skyrim Special Edition\SKSE\skse64.log
Documents\My Games\Skyrim Special Edition\SKSE\skse64_loader.log
Documents\My Games\Skyrim Special Edition\SKSE\MCMHelper.log
```

The MCMHelper log may contain unrelated warnings. Those warnings do not automatically indicate a TrueGaze failure.

## 9. Troubleshooting

### The plugin does not load

Check:

1. The DLL is under `Data\SKSE\Plugins`.
2. SKSE matches the Skyrim runtime.
3. Address Library matches the runtime.
4. The VC++ runtime is installed.
5. `skse64.log` contains `plugin TrueGaze.dll ... loaded correctly`.
6. The DLL hash matches the intended build.

### `tgstatus` is not recognized

Check `bEnableConsoleCommands=true` in the live deployed INI, then fully restart Skyrim. The command table is modified during plugin initialization; changing the INI while the game is open does not retroactively register commands.

### Actors are tracked but no visible effect appears

Read the visual counters:

- `2 lights` and `beam geometry 0 attached`: the light path is active, but no visible beam mesh was attached.
- `visual updates 0`: the gaze tick has not reached the visual subsystem.
- `anchors failed > 0`: the actor 3D or attachment anchor is unavailable.
- `light creates failed > 0`: scene-graph light allocation failed.

Do not start a new game to solve an asset lookup failure. Existing saves are supported.

### No target is reported

Check whether actor ticks and target resolutions are increasing. A zero target count can mean no active tick, eligibility rejection, LOD culling, or an actual selector result. The counters distinguish these cases.

### The configuration appears ignored

Confirm the path printed in `TrueGaze.log` after `Configuration loaded`. Deployment intentionally preserves the existing live INI unless explicitly forced to replace it.

### The game crashes

Stop testing and collect:

- `TrueGaze.log`
- `skse64.log`
- `skse64_loader.log`
- the exact DLL hash
- the exact save/cell and camera mode
- the last command entered

Do not continue changing multiple INI values before preserving the evidence.

## 10. Safe Test Procedure

For a repeatable test:

1. Exit Skyrim completely.
2. Deploy and verify the intended DLL.
3. Confirm the live INI values.
4. Launch through SKSE.
5. Load an existing save.
6. Enter third person.
7. Stand near a living humanoid NPC.
8. Wait 10 seconds.
9. Run `tgstatus`.
10. Exit Skyrim normally.
11. Save the complete logs before the next change.

## 11. Current Limitations

- The visible beam geometry asset is not yet successfully loading in the validated installation.
- Vanilla humanoid rigs commonly expose no separate eye bones; TrueGaze uses a geometric pupil origin from the head transform in that case.
- OAR condition registration is not complete.
- The HCEP bridge is local and optional; it is not an encrypted network transport.
- Skyrim VR-specific HMD behavior remains a separate validation target.
- The public release package and documentation are not a final 1.0 release until visible-effects verification and clean-profile testing are complete.

## 12. Support Report Template

For visible asset and beam troubleshooting, use the dedicated [TrueGaze Support Knowledge Base](TRUEGAZE_SUPPORT_KNOWLEDGE_BASE.md). It includes the BSA extraction, NifSkope inspection, runtime loading, and redistribution decision tree.

When reporting a problem, include:

```text
Skyrim runtime:
SKSE runtime:
Address Library version:
TrueGaze DLL SHA-256:
Save/new game:
Cell/location:
First- or third-person:
Live INI visual settings:
Exact console command:
TrueGaze tgstatus output:
TrueGaze.log:
skse64.log:
```

This information makes the issue reproducible and prevents configuration, deployment, and runtime symptoms from being conflated.
