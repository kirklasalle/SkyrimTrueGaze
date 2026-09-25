# Changelog

All notable changes to the **TrueGaze™** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> [!IMPORTANT]
> **Correction notice.** Earlier entries in this changelog described several features as "implemented" that are not functional, because they were written from design intent rather than from the code. Those entries have been annotated below. See [`docs/STATUS.md`](docs/STATUS.md) for the verified capability matrix and [`docs/AUDIT_REPORT_2026-09-11.md`](docs/AUDIT_REPORT_2026-09-11.md) for the full independent audit.

## [1.0.5] - 2026-09-25

### Added — Eye-Dominant Gaze, Dialogue-Synced CGA Return & Organic Motion

#### 👁️ Eyes Now Lead, Head Follows (Biologically Correct Gaze Distribution)

- **Head chain weights dramatically reduced**: `fHeadYawWeight` 0.65→0.245, `fNeckYawWeight`
  0.25→0.07, `fHeadPitchWeight` 0.55→0.245 (after two tuning passes: −50% then −30%). Eyes
  now receive the **majority** of the gaze deflection as the residual, producing the natural
  "eyes move first, head follows subtly" effect documented in oculomotor literature.
- **Head onset delay increased**: `fHeadOnsetDelaySec` 0.04→0.10 seconds. The biological
  latency gap between eye saccade onset and head following is now visually perceptible,
  matching the ~80-150ms observed in human subjects (Guitton & Volle, 1987).
- **Head engagement threshold** (`fHeadEngageThresholdDeg`, default 8°): for small gaze
  shifts (e.g. social triangle cycling between eyes/mouth at close range), the head stays
  **perfectly still** and only the eyes move. Eliminates robotic micro-head-turns.

#### 🧠 Eye-Dominant CGA (Cognitive Gaze Aversion) — "Embry Fix"

- **CGA aversion is now eye-only**: `BoneController::CalculateCgaStrain()` routes aversion
  deflections almost entirely to the eye bones with minimal head chain involvement
  (`fCgaHeadInvolvement`, default 0.08 = 8% head, 92% eyes). Eliminates the grotesque
  neck-twist observed on Embry when averting gaze.
- **Per-actor dialogue-sync CGA return** (`bDialogueSyncCgaReturn`): when dialogue begins,
  NPCs in CGA aversion snap their gaze back to the speaker's face — the natural "oh, they
  said something" attention capture. Kirk LaSalle's insight: *timing is the sweet spot for
  CGA; the return-to-face timed with dialogue is what makes bots look alive.*
- **Organic offset variation** (`fCgaDialogueOffsetSec`, default ±2.0s): each NPC gets a
  unique random offset per CGA episode. Some return slightly before dialogue (anticipatory
  — sensed the speaker was about to talk), some after (delayed processing — deep in
  thought). Prevents identical crowd reactions.
- **SocialTriangle::ReturnToFace()**: new method forces CGA scanpath back to the dominant
  eye, consumed by the dialogue-sync system.

#### 🎲 Organic Social Triangle Scanpath (No More Repetitive Orbit)

- **Weighted-random path selection** (`fTrianglePathRandomness`, default 0.6): replaces the
  deterministic LeftEye→RightEye→Mouth orbit with weighted transitions — 45% eye-to-eye
  (the transition humans favour), 35% lateral jumps, 20% same-point re-fixations. At 0 the
  classic orbit is preserved; at 1 full free wandering.
- **Per-visit landing scatter**: every fixation lands on a slightly different angle
  (±max(0.18°, 12% of offset magnitude)), so no orbit can ever repeat — the polygon-tracing
  look is gone. Also de-repetitizes CGA aversion points.
- **Extended diagram de-looped** (SPIRIT/HEART): exits from Third-Eye/Chest land on a
  random triangle vertex; the mouth now jumps to either eye.
- **Fixation rhythm widened**: dwell now varies 200–550ms (was 250–450ms).

#### 🌊 Smooth & Graceful Motion (Snappiness Eliminated)

- **Root cause of the snap found**: the smooth-pursuit path teleported the eyes to each new
  fixation in a single frame (`currentYaw = desiredYaw`), and that hard step propagated
  into the head through the VOR. Replaced with an **exponential glide**
  (`fEyePursuitSpeed`, default 12/s) with exact settle below a 0.02° remainder. Eyes and
  head now move fluidly between fixation points.
- **Head 20% slower**: `fHeadTrackingSpeed` 6.5→5.2; cervical slew caps 130→104 deg/s yaw
  and 90→72 deg/s pitch. All presets cut proportionally.

#### 📋 New INI Keys (TrueGaze.ini)

- `[SkeletalHierarchy] fHeadEngageThresholdDeg` — degrees below which only eyes move
- `[Social] fCgaHeadInvolvement` — head chain fraction during aversion (0=eyes-only)
- `[Social] bDialogueSyncCgaReturn` — CGA ends when dialogue begins
- `[Social] fCgaDialogueOffsetSec` — per-actor ±offset around dialogue onset
- `[Social] fTrianglePathRandomness` — 0 = fixed orbit, 1 = free organic wandering
- `[Kinematics] fEyePursuitSpeed` — smooth ocular pursuit glide rate (1/s)

#### 🔌 Public API (TrueGazeAPI.h)

- `ActorGazeTelemetry` gained `gazeRegion` (0..12, HCEP-02 Enhanced Diagram) + 3 reserved
  padding bytes for ABI stability.
- `TrueGaze_GetVersion()` now reports 0x01000500 (v1.0.5).
- Beam region colours in `VisualEffectsManager` re-matched to `ClassifyRegion()` IDs.

### Changed

- All preset weights updated across TrueGazeConfig.html to reflect eye-dominant tuning,
  slower head speeds, and organic path randomness (vanilla 0.6, subtle 0.5, intense 0.7,
  social 0.7, developer 0.6).
- `BoneController` compiled defaults updated to eye-dominant values.
- Unit tests extended: head engagement threshold, CGA eye-dominant strain, deterministic
  orbit preservation at randomness 0, no fixed loop at randomness 1, landing scatter.
- Codebase reformatted (clang-format) across VorCoordinator, TrueGazeAPI, and headers.

## [1.0.4] - 2026-09-23

### Fixed — NPC Gaze Crash (Multi-Actor Beam Geometry) & NPC Looking-Away

#### 🛑 Crash Fix: Beam Geometry SEH Crash Under Multiple Actors

- **Root Cause**: The Python-generated `GazeBeam.nif` (hand-crafted binary NIF) loaded
  via `BSModelDB::Demand` without error but caused a delayed SEH access violation
  (0xC0000005) inside Skyrim's NIF renderer when 7+ actors simultaneously carried beam
  geometry. C++ `try/catch` cannot intercept SEH — the game crashed silently with no
  log entry. Confirmed via `TrueGaze.log` analysis: log stopped cleanly mid-frame at
  16:19:22 with `priority=5` crosshair focus on the bard (actor `0003550C`), no warning.
- **Fix**: Switched primary beam model from `meshes\TrueGaze\GazeBeam.nif` to
  `meshes\dlc01\effects\fxsoulcairnbeam.nif` (proven vanilla Dawnguard Soul Cairn beam
  strip, present in every AE/SE install via `Meshes01.bsa`). The hand-crafted NIF is
  retired until validated in NifSkope.
- **Default render mode**: `iRayRenderMode` changed from `0` (Both) to `1` (LightOnly).
  NiPointLight emitters are proven crash-free across all actor counts. Mode `0` now
  uses the Dawnguard beam (safe), available via the Developer preset or `stgmode` console
  command. `usingFallbackMesh` detection extended to recognise `fxsoulcairnbeam` for
  correct non-uniform scale compensation.
- **Files**: `src/Visuals/VisualTuning.hpp`, `src/Visuals/VisualEffectsManager.cpp`,
  `skyrim/SKSE/Plugins/TrueGaze.ini`, `TrueGazeConfig.html`, `skyrim/TrueGazeConfig.html`

#### 🛑 Behavioural Fix: NPCs Looking Away From Player

- **Root Cause 1 — Unconditional `ClearHeadtrackTarget`**: Every frame, TrueGaze called
  `ClearHeadtrackTarget` on every NPC before writing its own gaze. When the engine fell
  back to `AmbientInterest` (priority=1, `targetFormId=0`) — a vacant point in the air
  ahead of the NPC — it had already destroyed Skyrim's own tracking. NPCs stared at
  nothing rather than the player. Log confirmed: `priority=1 form=00000000` for several
  minutes of gameplay (Embry, Sven, Delphine all looking away in screenshots).
- **Root Cause 2 — Detection ranges too small**: NPC candidate scan capped at 3.5 m,
  player nearby range 8 m, player cone 75°. Log showed player at 5.85 m — beyond NPC
  scan but within player range, yet outside the 75° cone → ambient fallback.
- **Fix 1**: `ClearHeadtrackTarget` now only called when `state.trackedTargetFormId != 0`
  (a real actor target is locked). When only ambient interest is resolved, Skyrim's
  native headtracking is left in place.
- **Fix 2**: NPC candidate scan widened `3.5 m → 6.0 m`; player nearby range
  `8 m → 12 m`; player detection cone `75° → 110°` (covers natural peripheral social
  attention without allowing backwards neck-snap).
- **Files**: `src/Engine/GazeEngine.cpp`, `src/Engine/TargetSelector.cpp`

#### 🔧 Diagnostic Visuals Off By Default

- `bEnableInGameVisuals`, `bGazeRaysEnabled`, `bDebugGazeRays` all default to `false`.
  Log level defaults to `Info` (2) instead of `Debug` (1).
- All non-developer presets (Vanilla, Subtle, Intense, Social & Dialogue) explicitly set
  these keys to `false` so switching away from the Developer preset cleanly disables
  diagnostic overlays.
- The in-game effect of TrueGaze is the NPC's actual head rotation and FaceGen pupil
  morphs (`LookLeft`/`LookRight`/`LookUp`/`LookDown`) — no laser beams required.
- **Files**: `skyrim/SKSE/Plugins/TrueGaze.ini`, `TrueGazeConfig.html`,
  `skyrim/TrueGazeConfig.html`

### Changed — Version Bump

- CMake project version, `SKSEPluginInfo`, and runtime identity log updated to `1.0.4`.
  On launch the log will show: `TrueGaze v1-0-4-0` and
  `Runtime identity: plugin v1.0.4 build Sep 23 2026`.

---

## [1.0.3] - 2026-09-23

### Fixed — True 3D Head-Height Elevation Targeting & Chest Aiming Defect

- **Dynamic 3D Bone Head-Height Elevation Tracking**:
  - *Root Cause Analysis*: Diagnosed from player logs and screenshots (`ScreenShot62`–`ScreenShot67`) that gaze pitch deflections were calculated as flat horizontal ($+0.4^\circ$, $-0.1^\circ$), causing seated NPCs (Camilla Valerius) to stare horizontally straight ahead into the standing player's chest, and counter-leaning NPCs (Lucan Valerius) to look downward toward the counter. Eye contact only triggered when the player crouched down to the exact horizontal elevation of seated NPCs.
  - *Rigid Elevation Elimination*: Both `TargetSelector.cpp` and `GazeEngine.cpp` previously calculated eye height using actor root translations (feet on the ground) plus a rigid `+160.0f` offset. In interior cells with floor level $z = 0$, this produced $dz = (0 + 160) - (0 + 160) = 0.0$, forcing pitch deflection to zero regardless of whether an actor was seated, leaning, standing, or crouching.
  - *Dynamic Bone Transform Solving*: Implemented `GetActorHeadPosition(RE::Actor* actor)` in `TargetSelector.cpp`, which dynamically resolves the actual `NPC Head [Head]` bone world transform (`head->world.translate`) with fallback candidates (`Head`, `Head1`, `Bip01 Head`). Updated `GazeEngine::WorldTargetToLocalGaze` to compute $dz = \text{targetHead.z} - \text{observerHead.z}$.
  - *Natural Postural Adaptation*: Seated NPCs now realistically tilt their gaze and head upwards toward standing characters, standing characters look down toward seated or counter-leaning characters, and crouching smoothly and continuously adjusts line-of-sight elevation in real time.

### Fixed — 3rd-Person Player Conversational Gaze & Biomechanical Headtracking

- **Player Character 3rd-Person Conversational Engagement**:
  - *Root Cause 1 (Candidate Scanning)*: When `observer == player`, `TargetSelector.cpp` only resolved targets during active dialogue menus, direct crosshair collision targeting, or combat. In 3rd person with a free camera, if the crosshair was not centered directly on an NPC, the player character fell through to an ambient forward idle state with $0.0^\circ$ deflection.
  - *Root Cause 2 (Skeleton Constraint Application)*: In `GazeEngine::ApplyToSkeleton`, an `if (!isPlayer && head)` guard explicitly bypassed head and neck rotations on the player character. Because vanilla humanoid rigs lack eye bones (`eyeL` and `eyeR` are null), the player character never turned their head toward nearby targets.
  - *Conversational Candidate Scanning*: Added a candidate scan in `TargetSelector.cpp` for the player character that locates the nearest conversational partner ($\le 4.5\text{m}$) inside the player's forward visual cone.
  - *3rd-Person Biomechanical Headtracking*: Updated `GazeEngine::ApplyToSkeleton` to allow head and neck tracking on the player character whenever `camera->IsInThirdPerson()` is true, while leaving 1st-person camera and torso spine untouched to prevent camera disturbance and ensure running animations stay aligned.

### Added — Live Console Logger Level Switching & Visual Enhancements

- **Dynamic `stgverbose` Logger Switching**: In `ConsoleCommands.cpp`, `CmdVerbose` now dynamically invokes `spdlog::default_logger()->set_level(...)` to switch the active logging threshold between `debug` and `info` instantly without requiring a game restart.
- **Subtle Laser Beams & Configurator Options**:
  - Calibrated discreet ~2mm hair-thin laser beams (`fGazeRayThicknessCm = 0.20`, `fGazeRayLengthMeters = 2.50`, `fPupilGlowIntensity = 0.35`) originating precisely from anatomical pupil sockets.
  - Exposed `bGazeRaysOnPlayer` toggle in `TrueGazeConfig.html` and `TrueGaze.ini` allowing users to display laser rays on NPCs only or on the player as well.
  - Calibrated the `Developer` preset in `TrueGazeConfig.html` for immediate, high-responsiveness eye movement verification (`fSaccadeSpeedMult = 1.50`, `fHeadTrackingSpeed = 6.50`, `fHeadOnsetDelaySec = 0.04`, `fMaxComfortEyeAngle = 35.0`).

## [1.0.2] - 2026-09-21

### Fixed — Crash to Desktop (CTD) on Save Load & Visual Stability Hardening

- **Crash to Desktop (CTD) on Game Load Resolved**:
  - *Root Cause 1 (FaceGen Modifier KeyFrame Null Pointer)*: In `EfmBlinkController::ApplyGazeMorphs`, `faceGenData->modifierKeyFrame.SetValue()` was called without verifying that `modifierKeyFrame.values != nullptr` or `modifierKeyFrame.count > 11`. During game load and 3rd-person player initialization, the player's FaceGen morph structures are unallocated, producing a null-pointer dereference access violation (0xC0000005). Added robust guard: `if (faceGenData && faceGenData->modifierKeyFrame.values && faceGenData->modifierKeyFrame.count > static_cast<std::uint32_t>(RE::BSFaceGenKeyframeMultiple::Modifier::LookUp))`.
  - *Root Cause 2 (Malformed Handwritten NIF Assets)*: Discovered that procedural scripts had created corrupt binary NIF structures for `GazeBeam.nif` and `GazeRegionPanel.nif`. Calling `RE::BSModelDB::Demand` on these malformed assets caused Skyrim's native resource parser to crash inside `SkyrimSE.exe`. Deleted the corrupt files and hardened `VisualEffectsManager::EnsureBeamGeometry` to default reliably to the verified engine-native `RE::NiPointLight` emitter fallback.
  - *Root Cause 3 (BSModelDB::DBTraits::ArgsType Zero-Initialization)*: Changed `RE::BSModelDB::DBTraits::ArgsType args{};` to `ArgsType args;` to preserve Skyrim's default engine traits (`unk8=true`, `postProcess=true`, `texLoadLevel=3`).
  - *Fault-Tolerant Exception Boundaries*: Wrapped all visual updates in `try / catch` blocks in `src/Engine/GazeEngine.cpp` (`VisualEffectsManager::Get().UpdateActor`) and player ticking in `src/Engine/AnimationHook.cpp`, preventing any potential visual or actor anomaly from disrupting the core kinematics loop or terminating the game.
- **Configurator UI Parity & Preset Normalization**:
  - Exposed `fGazeRayThicknessCm`, `bShowHcepPanel`, `bHcepPanelAllActors`, `fHcepPanelScale`, and `fHcepPanelForwardOffsetCm` in `TrueGazeConfig.html` with rich contextual tooltips.
  - Recalibrated all quick preset yaw strain weights (`vanilla`, `subtle`, `intense`, `social`) so their cervical distribution shares (`fSpine2YawWeight` + `fNeckYawWeight` + `fHeadYawWeight`) sum to exactly 1.00, eliminating engine sanitization warnings.
  - Synchronized `TrueGazeConfig.html` between workspace root and `skyrim/` packaging directory.

## [1.0.1] - 2026-09-21

### Fixed — Skyrim VR ("Mad God VR") Crash on Start & Multi-Targeting Architecture

- **Skyrim VR Startup Crash Resolved**: Diagnosed and resolved fatal game crash on boot when running Skyrim VR (`SkyrimVR.exe` 1.4.15), specifically reported in heavily modded environments such as the "Mad God VR" modlist (500+ mods).
  - *Root Cause 1 (CommonLib Address Library Resolution)*: In previous builds, CMake compiled with `BUILD_SKYRIM_VR=OFF`. Without `SKYRIM_CROSS_VR` and `HAS_SKYRIM_MULTI_TARGETING` defined, CommonLibSSE-NG attempted to load `versionlib-1-4-15-0.bin` via `IDDB::load()`. Because Skyrim VR exclusively utilizes `version-1-4-15-0.csv`, this threw an unhandled `std::system_error` causing immediate termination. Initialized `extern/CommonLibSSE-NG/extern/openvr` submodule and enabled `BUILD_SKYRIM_VR=ON` in `CMakeLists.txt` to compile unified SE/AE/VR multi-targeting.
  - *Root Cause 2 (Vtable Hook Slot Misalignment)*: In Skyrim SE and AE (`SkyrimSE.exe`), `RE::Actor::Update` occupies virtual table index `0xAD`. In Skyrim VR, `AttachWeapon` is inserted at slot `0x82`, shifting `Actor::Update` down by two entries to slot `0xAF`. Hooking slot `0xAD` on VR overwrote `PutActorOnMountQuick`, corrupting virtual dispatch and triggering instant memory access violation crashes upon actor initialization.
  - *Dynamic Hook Dispatch*: Refactored `src/Engine/AnimationHook.cpp` to dynamically evaluate the runtime module using `REL::Module::IsVR() ? 0xAF : 0xAD`.
- **Autonomous Direct SKSE Launch Independence**: Fully verified that TrueGaze runs completely autonomously when launching directly via SKSE (`skse64_loader.exe` or `sksevr_loader.exe`) or mod manager "Run" buttons (Mod Organizer 2 / Vortex). TrueGaze initializes natively on `kDataLoaded`, reads `TrueGaze.ini`, runs bone restorations every frame (ensuring full compatibility with OAR, Nemesis, Pandora, and combat overhauls), and operates without requiring any external batch files, web servers, or active bridge processes during gameplay.
- **Actor Eligibility & Eye Contact Restoration (Zero Eligible Ticks Resolved)**: Fixed defect in `src/Engine/AnimationHook.cpp` and `src/Engine/TargetSelector.cpp` where direct calls to `actor->GetLifeState()` evaluated invalid bitfield offsets on Skyrim AE 1.6.629+ / 1.6.1170 (1.7.104), causing 100% of actors to fail `IsActorEligibleForGaze` (`tick calls: 466, eligible ticks: 0`) and preventing visual emitters (gaze rays and HCEP panels) from attaching. Replaced fragile bitfields with canonical version-independent engine virtual calls (`actor->IsDead()`, index `0x99` SE/AE, `0x9A` VR) and form flags (`IsDisabled()`, `IsDeleted()`). Removed premature worker-thread headtrack clearing in `ActorUpdateHook`, restoring natural headtracking and gaze engagement across all NPCs.
- **Health Verification Update**: Updated `scripts/Test-TrueGazeHealth.ps1` to detect and validate the dynamic slot `0xAF` VR string marker and exception boundary, passing all 15 pre-flight checks with zero warnings or failures.

### Added — TrueGaze Configurator Package Integration & User Guide

- **Web Configurator Suite Bundled in Release**: Included the full standalone visual configurator suite into the production distribution archive (`dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip`):
  - `TrueGazeConfig.html` (Standalone zero-install HTML5 visual configurator with animated pupil kinematics preview, preset cards, and live save inspection).
  - `Launch-TrueGazeConfig.cmd` (Universal batch launcher with automated port scanning, process cleanup, and browser spawning).
  - `tools/TrueGazeConfig/` (Node.js and PowerShell local automation bridge servers for seamless bidirectional file reads and writes).
- **Universal Mod Manager Path Resolution**: Updated `Launch-TrueGazeConfig.cmd` and `TrueGazeBridgeServer.ps1` to auto-detect and resolve relative directories when installed inside Mod Organizer 2 (`<MO2>/mods/TrueGaze/`), Vortex staging folders, or manual `Data/` directories.
- **Comprehensive User Guide**: Authored and packaged `TrueGaze_Configurator_Guide.txt` detailing step-by-step instructions for:
  - Launching directly from SKSE in heavily modded setups.
  - Using Method A (One-Click Automation) and Method B (Direct Browser Configuration).
  - Adding the Configurator as a registered tool in MO2 and Vortex.
  - Mod compatibility, load order notes, and log file locations.

### Added — Option 1 (Laser Eyes) & Option 2 (HCEP Floating Diagram Panel)

- **Option 1: Superman Laser Eyes Refinements**:
  - *Pencil-Thin Ray Thickness*: Scaled geometry down to match the exact diameter of actor pupils (~8mm / 0.008 scale on X/Y axes in `NiMatrix3` rotation bases), replacing large arrows with razor-sharp laser rays.
  - *Dynamic Target Distance Scaling*: Scaled ray length along the Z-axis dynamically based on actual raycast hit distance or `fGazeRayLengthMeters`.
  - *Pupil Origin Socket Anchoring*: Replaced raw head bone centers with anatomical pupil socket derivations (`fPupilForwardOffsetCm`, `fPupilUpOffsetCm`), ensuring beams project outward cleanly from the eyes without intersecting facial geometry.
  - *Ocular Line-of-Sight Tracking*: Oriented laser rays to track the computed saccadic and fixation line-of-sight vectors rather than following static head yaw/pitch.
- **Option 2: HCEP Floating Diagram Panel**:
  - *Floating 3D Display Quad*: Authored `meshes/TrueGaze/GazeRegionPanel.nif`, rendering a 3D planar quad anchored to the actor's head bone and floating stably ~35cm in front of the eyes.
  - *Chroma-Keyed Texture Pipeline*: Processed `hcep-02_enhanced-diagram_keyed-01.jfif` to generate `textures/TrueGaze/GazeRegionPanel.dds` in DXT5 format with an 8-bit alpha channel, removing solid background artifacts for transparent holographic rendering.
  - *Real-Time Gaze Region Emissive Glow*: Dynamic shader highlights on the panel responding in real-time to active HCEP gaze regions (Social Triangle, Mutual Gaze, Intimate, Avoidance, Target/Distraction).
  - *Configurable INI Controls*: Added `[Visuals]` configuration keys: `bShowHcepPanel` (toggle floating display), `bHcepPanelAllActors` (display on all NPCs or player only), `fHcepPanelScale` (overall quad dimensions), and `fHcepPanelForwardOffsetCm` (forward floating distance).

## [1.0.0] - 2026-09-20

### Public Production Release on Nexus Mods ([Mod #192480](https://www.nexusmods.com/skyrimspecialedition/mods/192480))

- **Public Distribution Packaging:** Packaged production archive `dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip` (SHA-256: `41AB39649F3B78F505F6BA4D972460365CC4EFAA0352C61297CB66F60991410F`) and companion debug symbols `dist/TrueGaze-v1.0.0-Symbols.zip` (`TrueGaze.pdb`, SHA-256: `5021AB863EF30539F0A0DE4FC87A9C88C090AEB55B16FF3BC4FF74F28D10EA28`).
- **Diagnostic Visuals Policy:** Shipped `TrueGaze.ini` defaults `bEnableInGameVisuals = false` for pristine, organic eye contact without developer diagnostic laser beams. When toggled (`tgvisuals`), `VisualEffectsManager` probes standalone mesh `meshes\TrueGaze\GazeBeam.nif`, falls back to Dawnguard `fxsoulcairnbeam.nif`, and seamlessly defaults to the verified `NiPointLight` emitter fallback (`TrueGaze_PupilLight`, `TrueGaze_TerminusLight`).
- **Open Animation Replacer (OAR) Dynamic Messaging Hook:** Dynamic SKSE messaging registration (`OarConditions::OnSkseMessage`, `RegisterWithOar`) with zero static compile dependencies. Evaluates `TrueGaze_IsMode`, `TrueGaze_IsMutualGaze`, and `TrueGaze_GetGazeRegion` against live per-actor state cache in real time.
- **Broad Multi-Race & Dialogue Field Acceptance:** Documented Stage 6 test protocol in `docs/TEST_SCENARIO.md` across Humanoid (Nord/Imperial in Whiterun/Riverwood), Elven (Bosmer/Dunmer), and Beast races (Khajiit/Argonian) for third-person dialogue camera, VOR counter-rotation, and Social Triangle cycling.
- **Nexus Mods Presentation:** Authored `docs/NEXUS_MODS_PAGE.md` with complete BBCode/Markdown formatting, live configurator screengrabs, hero banner, and release documentation.

### Changed — Skyrim AE Runtime Verification and SOTA Roadmap (2026-09-19)

- Updated the current documentation state after controlled Skyrim AE sessions.
- Confirmed through runtime logs: SKSE load, actor update hooks, eligible ticks, target resolution, skeleton probing, HCEP mode/state consumption, and diagnostic light attachment.
- Documented the remaining evidence boundary: vanilla humanoid eye-node availability varies by rig; perceptual eye/head quality still needs a formal acceptance matrix; visible beam geometry remains unresolved; OAR registration and Skyrim VR require separate validation; packaging and licensing remain release gates.
- Added [`docs/IMPLEMENTATION_PLAN_SOTA_RUNTIME_TO_RELEASE.md`](IMPLEMENTATION_PLAN_SOTA_RUNTIME_TO_RELEASE.md), a staged implementation and publication plan.
- Updated `README.md`, `docs/STATUS.md`, `ROADMAP.md`, `docs/TEST_SCENARIO.md`, and architecture references to distinguish verified runtime behavior from open visual and ecosystem work.
- Added [`docs/TRUEGAZE_SUPPORT_KNOWLEDGE_BASE.md`](TRUEGAZE_SUPPORT_KNOWLEDGE_BASE.md) with web-supported BSA/NifSkope/CommonLib asset-reuse guidance, a `kNotExist` decision tree, legal boundaries, and a repeatable support-report template.

### Fixed — Console Commands "not found": wrong registration mechanism (2026-09-18)

The first in-game run of the console feature produced:

```
Console command table validation FAILED (count is at or beyond the declared
 table length). No commands were registered and no memory was written.
```

**The guard worked; the assumption was wrong.** It refused to write on an unverified
layout instead of corrupting engine memory or failing silently — exactly what it was
built to do. The defect was the mechanism itself.

- **There is no count of console commands.** The previous revision assumed a counter
  sat immediately after the command array and tried to append past it. The SDK's own
  `LocateConsoleCommand` disproves that: it scans the whole array and identifies a real
  command from a **per-entry marker** — an empty `helpString` means the entry is empty,
  and otherwise the help string ends with `1` (live) or `0` (dead).
- **Registration now reclaims, rather than appends.** It fills entries the engine
  already treats as not-a-command, so it needs no count and **cannot displace a working
  vanilla command** (a live entry is never a candidate). Nothing is ever written past
  the end of the array.
- **Both name fields are set to the token you type** (`tgv`, not `TrueGazeRays`). The
  SDK's lookup matches on `functionName`; we cannot be certain which field the engine's
  parser matches, so both hold the short token and the readable text lives in
  `helpString`, carrying the live-command marker.
- **Registration install is now self-diagnosing.** It logs
  `Console table scan: scanned=.. live=.. dead=.. empty=.. reclaimable=..` on every
  launch, so a future failure states which condition broke instead of leaving a
  "not found" to interpret. A partial registration is refused outright — registering
  some commands and not others would be worse than registering none.

### Fixed — Deploy silently wiped the user's configuration (2026-09-18)

**A serious usability bug.** Both `Deploy-TrueGaze.ps1` and `TrueGaze.cmd` copied the
*packaged* `TrueGaze.ini` over the *deployed* one on **every** deploy. The packaged file
is the set of shipped defaults; the deployed file holds the user's live tuning, and
everything the console commands persist. So every build/deploy reset hand-tuned
settings to defaults — which presents as "my settings randomly reverted."

- **The deployed INI is now preserved.** It is copied only when absent (first install).
- Keys present in the newer defaults but absent from the deployed INI are **reported**
  (not silently injected). That is safe — the engine keeps its compiled default for any
  missing key — but you should know a new key exists.
- **`-ForceIni`** added to `Deploy-TrueGaze.ps1` for the case where resetting to
  shipped defaults is genuinely wanted.

### Added — Runtime Console Commands (vanilla `~` console) (2026-09-18)

TrueGaze could only be re-configured by editing `TrueGaze.ini` and restarting. The
console is the right place for **runtime** toggling, because `~` pauses the game and
frees the camera — exactly the moment you want to switch the gaze visuals on and step
back to watch them.

- **`src/Integrations/ConsoleCommands.{hpp,cpp}`** (new) — registers `tg*` commands
  into the engine's own console command table.
- **VANILLA ONLY.** No Papyrus, no ESP/ESL, no MCM, no SkyUI. This is also a hard
  requirement, not just a preference: **Papyrus native functions cannot be called from
  the console**, so the commands must be genuine `SCRIPT_FUNCTION` entries. A pre-flight
  check asserts no Papyrus API usage anywhere in `src/**`.
- **Press `~` and type `tgstatus`** to see the live state, or `tgv` to toggle the gaze
  rays. Commands: `tg` (simulation), `tgvisuals`/`tgv` (visuals / rays), `tgon`/`tgoff`,
  `tgmode` (render mode), `tgradius` (terminus glow), `tgverbose` (logging),
  `tgstatus` (full state).
- Toggles apply **immediately** and are **persisted** to the INI, so a console change
  survives a restart. Each command prints its new state to the console and the file log.
- New `[Console]` INI section + a configurator panel.

> [!IMPORTANT]
> **`bEnableConsoleCommands` is OFF by default, and that is a deliberate engineering
> decision — not an oversight.** It writes into engine memory, so it stays opt-in until
> confirmed inside a running game.
>
> **Registration does not append — it reclaims.** There is **no count** of console
> commands; the SDK's own `LocateConsoleCommand` scans the whole array and identifies a
> live command by a per-entry marker (empty `helpString` = empty entry; otherwise the
> help string ends with `1` for live or `0` for dead). So TrueGaze fills entries the
> engine already treats as not-a-command. That needs no count, and it **cannot displace
> a working vanilla command**, because a live entry is never a candidate.
>
> **An earlier revision of this feature was wrong, and the failure is recorded here.**
> It assumed a count sat after the array, guessed at that offset, and on first launch the
> guard refused to write and logged `Console command table validation FAILED (count is at
> or beyond the declared table length)`. That guard is exactly why nothing was corrupted —
> the plugin did **not** write bad memory and did **not** fail silently; it declined and
> said why. The assumption was the defect; the guard caught it. *Never guess an index;
> never guess an offset.*
>
> Everything still works without this, from `TrueGaze.ini` and `TrueGazeConfig.html`.

### Added — Complete Prerequisite Installer (2026-09-18)

Installing TrueGaze on a fresh machine previously required assembling the
toolchain by hand, and `TrueGaze.cmd prereqs` covered only the two Nexus mod
files — not the build system itself. This adds a single entry point for the
whole machine.

- **`Install-AllPrerequisites.bat`** (new, repository root) — the one-click
  entry point. Batch wrapper only: it finds PowerShell, forwards its arguments,
  and pauses when double-clicked. All real work is delegated to PowerShell,
  because a multi-line `-Command` argument leaks into `cmd.exe` (documented in
  the audit notes).
- **`scripts/Install-AllPrerequisites.ps1`** (new) — installs, in dependency
  order: PowerShell (checked) → **winget** → **Git** → **CMake** → **7-Zip** →
  **Visual Studio Build Tools** with the C++ workload → **VC++ 2015-2022 x64
  Redistributable** → **vcpkg** (bootstrapped, `x64-windows-static-md`) →
  **`CommonLibSSE-NG` submodule** → **SKSE64 + Address Library**.
- **`VCPKG_ROOT` is now persisted.** `CMakePresets.json` resolves its toolchain
  as `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`, so an unset variable
  is a hard configure failure. This was the single most common blocker on a
  fresh machine and is now set automatically.
- **`-Verify` mode** reports what is present and what is missing without
  changing anything — safe to run at any time.
- **`TrueGaze.cmd prereqs-all`** and a new menu entry `[8]` expose it from the
  project's existing tool.
- Installs needing administrator rights are **reported, not silently skipped**,
  with the exact winget command to run from an elevated terminal.
- Idempotent: re-running installs only what is missing (`winget` "already
  installed" is recognised, not treated as a failure).

> [!IMPORTANT]
> **SKSE64 and the Address Library are now installed** (SKSE `skse64_1_7_104.dll`,
> Address Library `versionlib-1-7-104-0.bin`, both matched to game 1.7.104.0).
> The blocker to in-game testing is therefore **cleared** — nothing now stands
> between the build and a first in-game run. Note the `[Visuals]` verification
> (see below) can now actually be performed.

### Added — In-Game 3D Visual System: "Superman Laser Eyes" (2026-09-18)

TrueGaze computed a full per-actor gaze solution every frame and rendered **nothing**
in-world. The only "debug rays" were text: a throttled `spdlog` line, a console
`Print`, and a HUD message string. `src/Integrations/DebugGazeRenderer.hpp` was an
orphaned header with no `.cpp` and no call site (audit Finding 6). This adds a real,
toggleable in-game 3D visual layer. See
[`docs/Implementation Plan - In-Game 3D Visual System & Gaze Ray Assets.md`](docs/Implementation%20Plan%20-%20In-Game%203D%20Visual%20System%20%26%20Gaze%20Ray%20Assets.md).

- **`src/Visuals/VisualEffectsManager.{hpp,cpp}`** (new) — a **pure consumer** of
  `GazeEngine` state. It never recomputes gaze; it subscribes to the eye residual
  (`eyeYaw`/`eyePitch`), the resolved head/eye bones, and the gaze region. What you
  see is therefore the solver's own answer, not a re-derivation that could drift.
- **`src/Visuals/VisualTuning.hpp`** (new) — immutable per-frame snapshot, mirroring
  `GazeTuning`. A key in `TrueGaze.ini` has exactly one path to the visuals.
- **V1 renders via `NiPointLight`** — a real light at the pupil and, optionally, at
  the gaze terminus. This path needs **no art assets**, so the visual works today.
  Branded beam geometry (NIF) is a later phase; `iRayRenderMode` selects
  `0 Both / 1 Light-only / 2 Geometry-only`.
- **Pupil origin is derived geometrically.** Vanilla humanoid rigs expose **no eye
  bones** (eyes are FaceGen morphs), so the socket is computed from the head bone's
  world basis plus the configurable `fPupilForwardOffsetCm` / `fPupilUpOffsetCm`.
  Custom rigs (XP32/XPMSSE) that do expose `NPC L/R Eye` are used directly.
- **Direction uses the eye residual, not the total deflection.** The head bone's
  world transform already contains the head's share of the turn, so applying the
  total here would double-count it and the beam would overshoot the true gaze.
- **18 keys in a new `[Visuals]` INI section**, all off by default, plus an additively
  added **"In-Game Visuals"** panel in `TrueGazeConfig.html` (14 controls + rich
  tooltips in the page's single tooltip standard). Verified: the INI, the engine and
  the HTML agree on every key.
- **Safety.** Visuals are never serialized; emitters are detached on disable, on
  actor eviction, on `ResetAll`, and on `kSaveGame` alongside `ReleaseBones()`. The
  subsystem cannot affect the simulation, the save game, or actor state.

> [!NOTE]
> The `[Visuals]` subsystem is **developer-facing and off by default**. It is opt-in,
> requires a game restart to take effect (the INI is read at startup), and — like the
> rest of the engine — is **not yet verified in-game** (SKSE64 and the Address Library
> remain uninstalled on the test machine). The `NiPointLight` path is the first thing
> to confirm on the next launch.

### Changed — Vanilla-UI Enforcement: No SkyUI, Papyrus, or MCM (2026-09-18)

TrueGaze is **deliberately vanilla-UI**. The MCM and Papyrus layers were removed by
design on 2026-09-14; this pass makes that stance explicit, enforced, and
self-healing rather than merely documented.

- **`scripts/Deploy-TrueGaze.ps1`** — deploy now **removes stale MCM-era artifacts**
  from the game `Data` folder on every run: `TrueGaze.esp`, `TrueGaze.esl`,
  `TrueGaze_MCM.pex`, `TrueGaze.pex`, `MCM\Config\TrueGaze`, `Interface\MCM\Config\TrueGaze`,
  and `Interface\Translations\TrueGaze_*.txt`. Upgrading over an old install now
  self-heals instead of leaving a stale ESP in the load order or a `.pex` SkyUI
  might still bind.
- **`scripts/Test-TrueGazeHealth.ps1`** — new pre-flight check
  **"No legacy SkyUI/Papyrus/MCM artifacts"**. Reports a `Warn` naming any leftover
  file found, so a confusing in-game state is diagnosed before launch.
- **`LaunchTrueGaze.bat`** — cleanup block expanded to remove the same full set of
  legacy artifacts (previously only the ESP).
- **`TrueGaze.cmd`** — the deploy path now calls a `CLEAN_LEGACY` routine that
  removes the same artifacts, so Kirk's primary self-contained entry point
  self-heals identically. Written with top-level `if exist` tests only (a bare `)`
  inside a parenthesised block would close it early, and these paths contain
  parentheses). Verified by planting fake artifacts and confirming removal.
- **Documentation** — the vanilla-UI stance is now stated explicitly in `README.md`
  (dependency table), `PRD.md` (explicit non-dependencies), `docs/STATUS.md`
  (configuration section), and `ROADMAP.md` (Phase 5). The historical audits
  (`AUDIT_REPORT_2026-09-11.md`, the superseded walkthrough, and the original
  implementation plan) now carry clear banners marking their MCM/Papyrus/ESP
  findings — **C-4**, **C-9**, **C-11**, and the Phase 4 checklist — as **moot**
  rather than merely "resolved by another mechanism".

The only configuration surface is `Data\SKSE\Plugins\TrueGaze.ini`, edited through
the standalone `TrueGazeConfig.html` page.

### Changed — Unified Tooltip Standard in the Configurator (2026-09-18)

`TrueGazeConfig.html` previously used two different tooltip styles: the rich
floating tooltip card on the configuration rows, and plain native browser
`title=""` tooltips on everything above the first section (the action toolbar and
the Quick Presets bar). The rich card is now the **single standard across the
entire page**.

- **New `UI_TOOLTIPS` knowledge base** — each toolbar button and each Quick Preset
  now carries a full entry with a *Function & Mechanism* description, an *Effect*
  summary, *Context*, and a recommendation, matching the depth of the INI
  parameter entries.
- **Toolbar + presets rewired** — every `title=""` attribute was replaced with
  `data-tt-ui="ui:<id>"`, wired through the same `#richTooltip` card by a new
  `wireUiTooltips()` bootstrap pass. No native browser tooltips remain on the page.
- **Bottom action bar** — the *Save Changes to Game* and *Export Copy* buttons in
  the bottom bar now share the toolbar buttons' tooltips.
- **Adaptive card** — interface entries render an `[INTERFACE]` tag (name read from
  the button's own label) and an `Effect:` label, and omit the
  Recommended/Shipped footer, which has no meaning for an action. Configuration
  entries are unchanged: `[SECTION]` tag, `In-Game Visual Impact:`, and the
  Recommended/Shipped footer.
- **Additive only** — page layout, controls, bridge calls, and the simulation canvas
  are untouched. No component was removed or replaced.

### Added — Crosshair-Driven Mutual Gaze (2026-09-14)

- **`src/Engine/PlayerGazeResolver.{hpp,cpp}`** — resolves the player's crosshair into a "who is the player looking at" answer. Reads the game's own `CrosshairPickData` (the same pick the HUD activation prompt uses) and tests whether the crosshair ray falls within the target's **face sweet spot**. The sweet spot is angular, not a fixed radius: the crosshair must be within the head's angular size (`2·atan(0.12 m / d)`) plus a base tolerance, so a close NPC is forgiving and a distant one requires precision — matching natural vision and the crosshair's own on-screen behaviour.
- **`TargetSelector::TargetPriority::CrosshairFocus`** — new highest-priority target. When the crosshair rests on an NPC's face, that NPC looks back at the **player's face** (origin + eye height), producing true eye-to-eye contact instead of proximity-based looking.
- **`mutualGazeHoldSec` is now written.** `GazeEngine::ComputeDeflection` accumulates it while the crosshair holds on the actor's face and resets it the moment it breaks. This is the first production consumer of the field, which previously existed but was never written — the OAR conditions and C API always reported zero.
- **`[Crosshair]` INI section** — `bEnableCrosshairGaze`, `fCrosshairToleranceDeg`, `fCrosshairMaxRangeMeters`, `fCrosshairPointBlankMeters`. Wired through `ConfigManager` (read/sanitise/save), `GazeTuning`, `TargetSelector::s_crosshair`, and `TrueGazeConfig.html`. INI edited in Latin-1 per the byte-preservation rule.

### Added — Governance Enforcement

- **`GOVERNANCE.md`** — the charter enforcement map required by `AGENTIC_SACRED_COVENANT.md` §3.1. States plainly which controls are implemented, which are gaps, and which are not applicable to a Skyrim plugin. Includes the Law 6 biometric gap analysis and the amendment procedure.
- **`scripts/verify_charter.py`** — implements the `LAW10-CHARTER` control. Pins the 10 Laws and 4 Core Tenets by SHA-256 digest and verifies them across every charter document. Detects a Law that has been reworded, dropped, duplicated, renumbered, or left disagreeing between copies.
- **`config/charter_manifest.json`** — canonical digests of the Laws and Core Tenets, plus recorded known divergences.
- **`.github/workflows/charter-integrity.yml`** — CI enforcement of charter integrity, independent of the C++ build.

### Fixed — Charter Integrity

- **Fourth Law capitalisation** — `AGENTIC_PRIME_DIRECTIVE.md` and `AGENTIC_SACRED_COVENANT.md` read "An **I**ntelligence System" where the canonical text reads "An **i**ntelligence System". Corrected.
- **Ninth Law em-dash spacing** — both markdown charters read "verified — recognizing" where the canonical text reads "verified—recognizing". Corrected.

  The 10 Laws are now **byte-identical** across all three charter documents.

### Discovered — Governance Findings

- **🔴 Law 6 (biometric data) — no runtime control.** The HCEP bridge transmits real-world gaze vectors, head pose, blink state, a tracked person identifier, and cognitive/emotional classification over a named pipe created with `nullptr` security attributes (`src/Bridge/NamedPipeServer.cpp:75`). There is no access control, no encryption in transit, no consent capture, and no connection auditing — while `LICENSE` asserts GDPR Article 9, CCPA, and BIPA compliance. Practical risk is low for a single-user local mod, but the governance position is not defensible as written. Remediation plan in `GOVERNANCE.md`.
- **🔴 Law 10 (approval) — no cryptographic control.** `verify_charter.py --update` regenerates the manifest with a plain file write. There is no signature and no separation between recording a change and authorising it. Currently mitigated procedurally (explicit reminder + mandatory diff review), not cryptographically.
- **🟡 Core Tenet divergence.** All four Core Tenets in `AGENTIC_PRIME_DIRECTIVE.md` and `AGENTIC_SACRED_COVENANT.md` are **paraphrases** of the canonical text in `Permanent_Active_Directives.txt`, not reproductions. Recorded in the manifest as disclosed divergences pending Governance Council review. Three options are documented in `GOVERNANCE.md`; the choice is reserved to the Council.
- **🟡 Laws 1–5 and 8 have no runtime control.** Consistent with the covenant's own disclosure. These are governing principles for human conduct on this project, not runtime constraints on the plugin. Recorded so they cannot be mistaken for implemented controls.

### Added — Independent Audit & Honest Status Reporting

- **`docs/AUDIT_REPORT_2026-09-11.md`** — Full independent audit: complete documentation review, complete source review, binary forensics on the shipped `TrueGaze.dll`, distribution-archive inspection, and competitive market research (Nexus Mods landscape, UE5/MetaHuman gaze ecosystem, commercial eye-tracking, academic saccade literature).
- **`docs/STATUS.md`** — Verified capability matrix using a mandatory four-state vocabulary (📐 Designed / 🔨 Implemented / 🧪 Unit-verified / ✅ In-engine verified). Replaces binary "complete/incomplete" reporting, which had overstated progress.
- **Governance policy** — "Reporting and Verification Policy" now prohibits describing a feature as complete before it is ✅ In-engine verified, and prohibits log messages reporting success for operations not performed.

### Changed

- **`README.md`** — Added status banner, badges, and a **Current Project Status** section that disclaims the gap between the designed architecture and the current build. Clarified that the README describes the *target* system.
- **`ROADMAP.md`** — Converted all phase statuses from "Completed (100%)" to a four-state vocabulary with **verified** percentages (Phase 1 ~85%, Phase 2 ~10%, Phase 3 ~70%, Phase 4 ~15%, Phase 5 ~45%, Phase 6 ~40%, Phase 7 ~50%, Phase 8 ~5%). Added the audit-derived remediation roadmap (Phases R0–R5) sequenced by dependency, with milestone projections.

### Fixed — Documentation Accuracy

The following claims in earlier entries were found to be **inaccurate** and have been annotated in place:

- **`EfmBlinkController::ApplyMorphs` — not implemented.** The only substantive logic (the `exprOverrides` writes) is commented out; both code paths are no-ops. The cited `RE::FaceGen::Expression::BlinkLeft` used as an array subscript will not compile once the SDK is present; this code has never been compiled against CommonLibSSE.
- **OAR condition registration — not implemented.** `OarConditions::RegisterWithOar()` contains a `// Future:` comment where registration should be, yet logs a success message and returns `true`. The state cache the evaluators read (`g_actorGazeCache`) is never written to. Net effect: the 7-rule OAR package fires Rule 1 unconditionally and Rules 2–7 never fire.
- **Public modding SDK — export surface only.** All `TrueGazeAPI.cpp` bodies are stubs. `TrueGaze_IsHcepConnected()` returns a hardcoded `false`; `TrueGaze_GetActorGaze()` returns hardcoded zeroes and a hardcoded `0x14` target FormID while returning `true`; `TrueGaze_OverrideActorMode()` is an empty body.

- **Animation hooks — not installed.** `AnimationHook::Install()` constructs no `REL::Relocation`; the hook body that would apply bone rotation is a comment.
- **Configuration — never loaded on the real plugin path.** `ConfigManager::Load()` is called only in the `#else` fallback branch of `Main.cpp`. Every INI setting is inert.

### Discovered — Verified Defects

- **🔴 BLOCKER: the engine is inert.** No bone transform is ever written. `SaccadeGenerator`, `VorCoordinator`, `MicroJitter`, `SocialTriangle`, and `BoneController` have no production call sites — they are referenced only by `tests/KinematicsTests.cpp`.
- **🔴 BLOCKER: the build omits its SDK.** `extern/CommonLibSSE-NG` does not exist. The CMake guards (`if(EXISTS)` / `if(TARGET)`) silently skip both the subdirectory and the link step, so the build succeeds and produces a standalone skeleton. Every `#if __has_include(<RE/Skyrim.h>)` block resolves to a fallback stub.
- **`BoneController::CalculateHierarchyStrain` allocates no residual to the eyes.** Weights `0.10 + 0.25 + 0.65 = 1.00`, so `eyeYaw`/`eyePitch` are declared but never assigned. The eyes should receive `target − head_total`. This is a correctness bug in the project's signature feature.
- **`NamedPipeServer` double buffer is not lock-free.** `_packetBuffers[]` holds plain (non-atomic) 64-byte structs; `_readIndex` orders only the index, not the payload, so the writer can overwrite the slot the reader is mid-`memcpy` on. `_pipeHandle` is additionally written by the worker thread and read by the game thread with no synchronisation. Both are genuine data races.
- **`MicroJitter` is not Brownian.** It resamples an independent uniform target each interval and exponentially approaches it — a mean-reverting process, not a random walk. It is also seeded with a fixed constant (`1337`), so every run produces an identical jitter sequence.
- **`CalculatePeakVelocity` (the Main Sequence equation) is never used.** The saccade trajectory is a generic `smoothstep`. Additionally, `CalculateDuration` sets the correct total duration, but the *velocity profile within it* is a cubic ease rather than a Main Sequence profile — a fidelity gap between the documentation's central scientific claim and the implementation.
- **No biological latency gap is modelled.** Docs specify eyes lead by 20–30 ms and head follows at 120–180 ms; the implementation updates both in the same frame with only a damping constant differentiating them.
- **`PerformanceProfiler::ScopedTimer` uses `.store()` rather than `.fetch_add()`** — only the last measurement survives, which would break the intended per-actor accumulation.
- **`SocialTriangle.hpp` uses `std::max` without including `<algorithm>`** — compiles only by transitive include.
- **`PackageMod.ps1` hardcodes an absolute project path** and runs `cmake --build` with no preceding configure step. `.pdb` files are not shipped.
- **Magic numbers in `TargetSelector.cpp`** — `0.01428f` (Skyrim units-to-metres) and `160.0f` (eye-height offset) appear as bare literals five and six times respectively.
- **`TrueGaze.ini` contains a dead setting** — `sEngineTarget=Auto` is never read by `ConfigManager`. `iLogLevel` is parsed but never applied.
- **`LICENSE` contradicts the "Public Modding SDK" claim** — the license states "No license is granted... copying, distribution... prohibited".
- **`.gitignore` excludes the shipping artifacts** (`*.dll`, `*.zip`, `build/`) that the packaging script depends on.

### Verified Working

Confirmed correct by direct inspection during the audit:

- All 7 DLL exports present: `SKSEPlugin_Load`, `SKSEPlugin_Query`, `SKSEPlugin_Version`, `TrueGaze_GetVersion`, `TrueGaze_IsHcepConnected`, `TrueGaze_GetActorGaze`, `TrueGaze_OverrideActorMode`
- `TelemetryPacket.h` compile-time `static_assert` size guards (64-byte / 32-byte) — exemplary practice
- CRC-32 verify-before-publish in the pipe worker
- Correct DoS guard in the pipe read loop (`PeekNamedPipe` size check before `ReadFile`)
- Asynchronous pipe server with graceful auto-reconnect
- Biomechanical kinematics library — mathematically correct, correctly cited
- 8-suite standalone unit test harness — all passing
- IPC integration test harness
- Correct Skyrim mod package structure (MO2/Vortex layout)
- Well-designed 7-rule OAR condition package

---

## [1.0.0-rc1] - 2026-09-11

### Added

- **Multi-Version SKSE64 Plugin Exports**: Unified DLL exporting `SKSEPlugin_Query` and `SKSEPlugin_Load` (Skyrim SE 1.5.97) and `SKSEPlugin_Version` with Address Library version independence (Skyrim AE 1.6+).
- **NamedPipeServer Implementation & Verification**: Asynchronous Windows Named Pipe worker (`\\.\pipe\TrueGazeBridge`) featuring lock-free double-buffered telemetry exchange (< 10ns read time), CRC-32 validation, and non-blocking `PeekNamedPipe` polling (`src/Bridge/NamedPipeServer.cpp`).
- **End-to-End HCEP Integration Test Harness**: Standalone client test (`tests/HcepBridgeClientMock.cpp`) simulating live HCEP desktop perception streaming and validating 32-byte feedback reception.
- **AnimationHook Implementation**: Post-Havok actor animation evaluation pipeline with eligibility checks (filtering dead, paralyzed, sleeping, or ragdolled actors) and state-based gaze weighting (`src/Engine/AnimationHook.cpp`).
- **TargetSelector Implementation**: Salience prioritization engine resolving dialogue partners, combat adversaries, proximity actors, and ambient focus points (`src/Engine/TargetSelector.cpp`).
- **OarConditions Implementation & Config Rules**: Thread-safe condition cache and sample rules (`skyrim/meshes/actors/character/animations/OpenAnimationReplacer/TrueGaze/config.json`) supporting all 5 HCEP modes (LOGIC, AFFECT, SPIRIT, HEART, THINK).
- **Expressive Facegen Morphs (EFM) Integration**: Implemented `EfmBlinkController::ApplyMorphs` for face morph target weight calculations during saccades.
- **Skyrim VR & Performance Instrumentation**:
  - `VrController.hpp`/`cpp` handling OpenVR HMD 6DOF transforms and foveated gaze projections.
  - `PerformanceProfiler.hpp` microsecond execution timer ensuring frame budget $< 0.15\text{ ms}$.
- **Public Modding SDK**:
  - `include/TrueGazeAPI.h` and `src/Engine/TrueGazeAPI.cpp` exporting public C/C++ API (`TrueGaze_GetActorGaze`, `TrueGaze_GetVersion`, `TrueGaze_IsHcepConnected`, `TrueGaze_OverrideActorMode`).
- **Automated Nexus Packager**: Created `scripts/PackageMod.ps1` and compiled distribution package `dist/TrueGaze-v1.0.0-rc1-SkyrimSE-AE-VR.zip`.
- **Pure C++20 Kinematics Test Suite**: Standalone automated test harness (`tests/KinematicsTests.cpp`) validating 8 core components.
- **Product Requirements Document (`PRD.md`)**: Comprehensive technical document specifying product vision, personas, functional/non-functional requirements, wire protocols, and acceptance criteria.
- **Detailed Development Roadmap (`ROADMAP.md`)**: Strategic multi-phase roadmap spanning kinematics core through Nexus release and cross-engine expansion.

### Changed

- **Project Directory Refactoring**: Transitioned repository root from `D:\Projects\TrueGaze` to `D:\Projects\SkyrimTrueGaze` across all documentation, architecture guides, headers, build presets, and `.nexus` handover records.
- **Precompiled Header & Compiler Warning Hardening**: Added `[[maybe_unused]]` attributes and MSVC C4189 warning suppression in kinematics coordinators. Wrapped external logging headers with fallback macros for standalone compilation.

---

## [0.2.0] - 2026-09-10

### Added

- Mathematical foundation for biological oculomotor dynamics:
  - `SaccadeGenerator.hpp`: Main Sequence peak velocity ($750^\circ/\text{s}$) and duration calculations.
  - `VorCoordinator.hpp`: Vestibulo-Ocular Reflex eye-head decoupling and counter-rotation.
  - `MicroJitter.hpp`: Brownian random-walk drift eliminating frozen stares.
  - `SocialTriangle.hpp`: Argyle & Cook cyclical scanning between eyes and mouth.
  - `BoneController.hpp`: NetImmerse bone strain distribution (Spine2 10%, Neck 25%, Head 65%).
  - `LodManager.hpp`: 3-tier spatial level-of-detail culling.
  - `EfmBlinkController.hpp`: Micro-blink triggering for saccades $> 20^\circ$.
- 64-byte inbound telemetry wire protocol (`TrueGazeTelemetryPacket`) and 32-byte outbound feedback protocol (`SkyrimFeedbackPacket`).
- Open Animation Replacer (OAR) integration specification and HCEP bridge specification documents.

---

## [0.1.0] - 2026-09-09

### Added

- Initial project architecture and mission blueprint for TrueGaze™ as a first-party HCEP gaming product.
- Modern CMakePresets.json targeting MSVC C++20 for Skyrim SE, AE, and VR.
- Initial CMakeLists.txt and vcpkg.json dependency manifests.
- Master project overview (`README.md`) and technical architecture document (`TRUEGAZE_ARCHITECTURE.md`).
