# TrueGaze Support Knowledge Base

**Status:** Living support document
**Created:** September 19, 2026
**Scope:** Visible in-game development assets, Skyrim resource reuse, runtime attachment, troubleshooting, and release support
**Owner:** Kirk LaSalle

## 1. Purpose

This document converts the current TrueGaze evidence and external Skyrim modding research into a local, repeatable support knowledge base.

It exists to prevent repeated speculative fixes around the visible in-game illustration blocker. Every future asset experiment should be classified as one of:

1. **Resource discovery** - finding the exact NIF, texture, material, and dependency paths.
2. **Resource extraction** - obtaining a local development copy from a BSA or a mod package.
3. **Runtime loading** - proving `BSModelDB::Demand` resolves the resource.
4. **Scene-graph attachment** - proving `NiNode::AttachChild` succeeds on the game thread.
5. **Transform correctness** - proving origin, axis, scale, and orientation.
6. **Material visibility** - proving the NIF and textures render visibly in the target lighting conditions.
7. **Distribution legality** - determining whether the asset may be redistributed.

A successful step never proves the later steps automatically.

## 2. Current TrueGaze Evidence

The latest Skyrim AE runtime evidence proves:

- SKSE plugin loading.
- Actor update hook invocation.
- Eligible actor ticks.
- Target resolution.
- Live spine/neck/head skeleton probing on the tested player rig.
- HCEP mode/state telemetry consumption.
- Visual subsystem updates.
- Two diagnostic `NiPointLight` emitters attached.
- Zero anchor failures.
- Zero light-creation failures.
- Standalone kinematics tests passing.
- HCEP bridge mock passing.

The current visible-geometry evidence is:

```text
beam geometry    0 attached / many attempts
BSResource::ErrorCode::kNotExist
```

This means the current candidate resource lookup fails before `NiNode::AttachChild` can run. It does not prove that `NiNode::AttachChild` is wrong.

## 3. Research Findings

### 3.1 BSA Browser or archive utility

A Skyrim mesh commonly lives in a Bethesda archive rather than as a loose file. A BSA browser/extractor is appropriate for:

- locating the exact folder-qualified path;
- extracting a development copy;
- extracting referenced textures when needed;
- confirming whether a candidate exists in the installed game version.

The filename alone is insufficient. `fxsoulcairnbeam.nif` appearing in a filename table does not prove that the runtime path supplied to `BSModelDB::Demand` is correct, that the asset is a beam suitable for this use, or that its dependent textures are available.

### 3.2 NifSkope

NifSkope is appropriate for inspecting:

- root node names;
- local forward axis;
- transforms and scale;
- shader properties;
- texture paths;
- alpha/emissive material behavior;
- animation controllers;
- child geometry and bounds.

For TrueGaze, the first inspection questions are:

1. Is the model authored along local `+Y`, as the runtime currently assumes?
2. Does it contain visible geometry, or only controllers/particles that require a form/effect system?
3. Are texture paths valid in a normal Skyrim Data layout?
4. Is the model a static beam, a scene-specific prop, a particle attachment, or a form-driven effect?
5. Does its bounding volume survive the intended scale and transform?

### 3.3 CommonLibSSE-NG and `BSModelDB::Demand`

CommonLibSSE-NG exposes Skyrim-facing types and runtime relocation wrappers. The local SDK contains:

- `RE::BSModelDB::Demand(const char*, NiPointer<NiNode>&, ArgsType)`;
- `RE::NiNode::AttachChild(NiAVObject*, bool)`.

The practical runtime contract is:

1. Supply the exact resource path Skyrim's model database accepts.
2. Check the returned `BSResource::ErrorCode`.
3. Check the output `NiPointer` is non-null.
4. Attach on the game thread to a valid `NiNode`.
5. Hold the attached object through a strong `NiPointer`.
6. Detach before releasing or replacing the parent.

`AttachChild` is an attachment operation, not a resource extractor and not a material fixer. If `Demand` returns `kNotExist`, the problem is earlier in the pipeline.

### 3.4 NIF texture paths

A copied NIF can remain invisible or render incorrectly when its texture references point to:

- an absent loose texture;
- a texture only present in an optional DLC archive;
- a path with incorrect case or folder spelling;
- a material intended for an Art Object or particle system rather than a directly attached model.

NifSkope inspection must therefore be followed by a dependency check in the target Skyrim Data installation.

### 3.5 xEdit and Creation Kit

xEdit or Creation Kit becomes relevant when the asset is referenced by a Skyrim form, such as:

- `BGSArtObject`;
- spell or magic effect records;
- projectile records;
- activators or placed references;
- effect shaders.

That route generally requires an ESP/ESL or an existing form identifier. TrueGaze intentionally avoids ESP/ESL, Papyrus, MCM, and SkyUI dependencies. For the current developer illustration, direct NIF attachment is the lower-dependency path.

## 4. Recommended Asset Strategies

### Strategy A: Local extracted vanilla asset

Use for development-only diagnosis.

1. Install a BSA Browser or equivalent extractor locally.
2. Search `Skyrim - Meshes0.bsa`, `Skyrim - Meshes1.bsa`, and relevant DLC archives.
3. Extract a candidate NIF to a scratch directory.
4. Inspect it with NifSkope.
5. Extract or verify dependent textures.
6. Place a local development copy under `skyrim/meshes/effects/`.
7. Request the exact loose path through `BSModelDB::Demand`.
8. Test loading and attachment in-game.
9. Record the path, error code, NIF root, axis, scale, and material result.

Do not ship Bethesda-owned extracted assets in the public TrueGaze package without permission.

### Strategy B: Original TrueGaze development asset

Preferred for publication.

1. Create a minimal beam mesh aligned along local `+Y`.
2. Use an original emissive texture and material.
3. Place the NIF and texture under the TrueGaze mod layout.
4. Load it as a loose asset with an unambiguous path.
5. Validate the complete scene-graph and material path.
6. Package it with a clear license and attribution statement.

This avoids BSA path ambiguity, DLC dependencies, and Bethesda redistribution concerns.

### Strategy C: Existing Skyrim Art Object

Use only if the project later accepts a form/plugin dependency.

1. Identify a known existing Art Object or effect form.
2. Verify its model and texture dependencies.
3. Apply it through the engine-supported reference/effect API.
4. Test lifetime, attachment node, face target, cell transitions, and cleanup.
5. Document the ESP/ESL dependency and licensing implications.

This is not the current preferred path because TrueGaze is designed to remain ESP-free.

## 5. Decision Tree for `kNotExist`

### `BSModelDB::Demand` returns `kNotExist`

Check in this order:

1. Is the game running the current DLL? Compare SHA-256.
2. Is the requested path the exact Skyrim resource path, not a Windows filesystem path?
3. Does the asset exist as a loose file under the active Data directory?
4. If relying on a BSA, does the archive actually contain the asset for this game edition?
5. Is the candidate in a DLC archive that is not loaded?
6. Does the model database accept the selected path spelling and separator form?
7. Is the call occurring after game data is loaded and on the game thread?
8. Does the extracted NIF have valid model data and a supported root type?

Do not change axis, scale, or attachment code until the resource returns `kNone` with a non-null model.

### `Demand` succeeds but nothing is visible

Check:

1. `NiNode::AttachChild` was called.
2. The parent is a live `NiNode` and belongs to the current actor 3D.
3. The attached object is not hidden or culled.
4. The model's local axis and scale are correct.
5. The bounding volume is valid.
6. Texture paths resolve.
7. Shader/material properties support the intended lighting.
8. The model is not a particle/controller-only asset.
9. The camera is in a view where the actor and effect can be seen.
10. The model is not immediately detached by reset, actor rebuild, or a config refresh.

### Lights exist but no beam exists

This is expected when `stgstatus` reports lights but `beam geometry` remains zero. `NiPointLight` illuminates the scene; it is not visible beam geometry.

## 6. TrueGaze Diagnostic Contract

The runtime should report these stages distinctly:

```text
configuration loaded
actor tick entered
actor eligible
skeleton capability resolved
visual update entered
asset lookup attempted
asset lookup result
model pointer valid/invalid
attachment parent valid/invalid
geometry attached/detached
```

Current S1/S2/S5 work provides:

- runtime identity;
- effective config path;
- rig origin classification;
- eye/head absence counters;
- bounded geometry lookup retries;
- `beam geometry` counters.

Future support improvements:

- asset resolver state in `stgstatus`;
- exact resource path in status output;
- parent type and model pointer diagnostics;
- geometry detach count;
- actor-generation identifier;
- a development static-marker mode independent of beam loading.

## 7. Reproducible Development Procedure

### Prepare

1. Exit Skyrim completely.
2. Build/deploy the Release DLL.
3. Verify the live DLL hash.
4. Verify the live INI visual settings.
5. Back up the current log.
6. Prepare a small interior save with one actor.

### Test

1. Launch through SKSE.
2. Load the save.
3. Enter third person.
4. Stand 3-5 meters from a living humanoid.
5. Wait for the skeleton probe.
6. Run `stgstatus`.
7. Record the runtime identity, rig origin, visual counters, and geometry result.
8. Exit normally.
9. Run post-run health analysis.

### Record

For every candidate asset, record:

```text
Candidate path:
Source archive or original asset:
Extraction tool:
NIF root and local axis:
Referenced textures:
BSModelDB result:
Model pointer:
Attachment parent:
Geometry attached:
Visible in-game:
Transform correction:
Redistribution status:
```

## 8. External Research References

These are starting references, not substitutes for validating the current runtime:

- [CommonLibSSE-NG repository and documentation](https://github.com/alandtse/CommonLibVR/tree/ng)
- [CommonLibSSE repository](https://github.com/Ryan-rsm-McKenzie/CommonLibSSE)
- [NifSkope project](https://github.com/NifTools/NifSkope)
- [xEdit/TES5Edit project](https://github.com/TES5Edit/TES5Edit)
- [Skyrim Creation Kit wiki Art Object reference](https://ck.uesp.net/wiki/Art_Object)
- [CommonLib documentation portal](https://commonlib.dev/)

The Creation Kit wiki was protected by a web security challenge during this research session; its page is recorded as a reference but was not treated as verified local evidence.

## 9. Support Triage Template

```text
TrueGaze version/build:
Game runtime:
SKSE runtime:
Address Library:
Live DLL SHA-256:
Effective INI path:
Candidate asset path:
Asset source/archive:
BSA extraction tool:
NifSkope inspection result:
BSModelDB result code:
Model pointer non-null:
Parent node type:
Geometry attached count:
Visible in-game:
Rig origin:
stgstatus output:
TrueGaze.log:
skse64.log:
```

## 10. Current Recommendation

For the immediate development goal, use a **local extracted vanilla asset** only to prove the resource/scene-graph pipeline. In parallel, create an **original TrueGaze beam asset** for any public release. This gives the team a fast diagnostic path without making the final distribution dependent on Bethesda-owned content or uncertain archive paths.

## 11. BSA Investigation Record (2026-09-19)

Direct parsing of the installed `Skyrim - Meshes0.bsa` established:

- Header: version 105, flags `0x87`, 978 folders, 19443 files.
- The folder-name block contains the exact entries `meshes\effects` and `meshes\dlc02\effects`.
- The file-name block contains `fxsoulcairnbeam.nif`, positioned between `powerwordhaas.nif` and `warriorstone.nif`.
- A soulcairn-related folder `meshes\dlc01\lod\soulcairn` also exists.
- Hand-decoding could not reliably map the file index to its owning folder: sequential folder-name decoding misaligns after ~335 entries, and folder-record offsets do not index into the name block in the assumed way.
- A lockstep walk of the file-names block produced a nonsense record (246 MB size), confirming the assumed record/name alignment is wrong for this archive.

**Conclusion:** the exact owning folder of `fxsoulcairnbeam.nif` remained unverified by hand parsing. This is precisely why the recommended workflow uses a purpose-built BSA Browser rather than ad-hoc parsing.

### 11.1 Resolution with BSA Browser (2026-09-19)

BSA Browser (`bsab.exe`, portable, downloaded from the AlexxEG/BSA_Browser GitHub release) resolved the path immediately:

```text
meshes\dlc01\effects\fxsoulcairnbeam.nif
```

The asset is a **Dawnguard (DLC01) effect**, not a base-game `meshes\effects` asset. The earlier `kNotExist` was caused by the wrong folder, not by the loader.

Verified asset facts:

- NIF: Gamebryo File Format, Version 20.2.0.7, root `Static`.
- Referenced textures (all found in `Skyrim - Textures5.bsa`):
  - `textures\effects\cloudtilelight.dds`
  - `textures\effects\gradients\gradambfog01.dds`
  - `textures\effects\glowsoft01.dds`
- All three textures and the NIF were extracted to `scratch/beam_extract/` and deployed as loose files into the game `Data` directory for development testing.
- `VisualTuning::beamModelPath` now defaults to the verified path, so no rebuild is needed to test further candidates.

**Distribution note:** the extracted NIF/DDS files are Bethesda-owned and are deployed locally for development only. They must not be included in the public TrueGaze package; an original asset remains the publication path.

### 11.2 In-game attachment confirmed; tuning paused (2026-09-19, 20:59 session)

The verified DLC01 path loaded and attached in a live Skyrim session:

```text
Visible beam geometry attached from 'meshes\dlc01\effects\fxsoulcairnbeam.nif'
beam geometry    1 attached / 1 attempts
```

The beam is not yet perceptible in-game. Expected causes, in check order:

1. **Scale** — the soulcairn beam is authored for a large scene; at head-anchor scale it may be enormous or sub-pixel. Check the NIF's native bounds in NifSkope and compare with `fGazeRayLengthMeters`.
2. **Alpha/material** — the beam uses `CloudTileLight`, `GradAmbFog01`, and `GlowSoft01` with alpha blending; it may be invisible against bright scenes or require additive blending to read as a beam.
3. **Axis** — confirm the model's local forward axis matches the `BeamRotation` assumption (+Y).
4. **Culling** — verify the attached node is not app-culled and its bounds survive the applied scale.

**Pause decision:** visual tuning is deferred in favour of S4 (HCEP intent fusion) and S3 (perceptual acceptance). The loading/attachment pipeline is complete and documented; resuming requires only transform/material tuning via the configurable `VisualTuning::beamModelPath` and existing visual INI keys.

## 12. Scripted Scenes & Kinematic Coordinate Anomalies (Phase 8 / R16)

When actors are positioned in scripted/directed scenes, vehicles (e.g. `PrisonerCarriage01`), or seated furniture (thrones, benches, chairs), their Havok physics root (`actor->GetAngleZ()`) can diverge by 90° or more from their visible animated torso.

For full architectural details, root cause analyses, and the graceful anomaly handling matrix, see the dedicated reference:
- [`docs/SCRIPTED_SCENE_KINEMATIC_ANOMALIES.md`](SCRIPTED_SCENE_KINEMATIC_ANOMALIES.md)

Key operational rules:
1. **Standing vs Seated Reference Decoupling**: For standing humanoids aligned with their capsule, use `actor->GetAngleZ()` with strictly level pitch `0.0f` (sampling raw spine vectors on standing actors corrupts pitch by +30° to +45° and skews heading). For seated/furniture/vehicle actors (detected via torso divergence `diff > 30°` or `sitState != kNormal` or `GetOccupiedFurniture()`), sample the spine forward vector `std::atan2(forward.x, forward.y)` (or clavicle line `(-dy, dx)` as fallback) with level pitch invariant (`pitchRad = 0.0f`) to decouple cervical tracking from vehicle trajectory.
2. **Never assume vanilla procedure headtrack slots match speech**: active speech (`high->voiceState` & `lastSpokenToArray`) takes precedence over secondary look procedures.
3. **Always enforce biological cervical limits (±70° yaw)** relative to the torso coordinate frame to prevent 180° backward twisting.
4. **Maintain Dialogue Player Hold (3.0s)** and **Fixation Stability (1.5s)** to prevent micro-twitching and target flapping.
5. **Zero Snapping / Discontinuities**: Use Hermite cubic smoothstep for head engagement threshold, and exponential strain/yield blending (rate 8.0/s, ~125ms) across CGA (THINK mode) and scene deferral transitions.
6. **Conversational Eye Majority**: Restrict peripheral aversion regions strictly to deliberate CGA episodes and weight foveal eye contact 4.0x so NPCs look into target eyes 85%+ of the time.

## 13. Tavern Motivators, Focal Point Detection & Binocular Vergence (R17)

### 13.1 Tavern Social Salience Motivators
In a crowded, social environment like a Skyrim tavern (e.g., The Bannered Mare in Whiterun), TrueGaze balances multiple competing motivators to drive authentic, living crowd attention:

1. **Active Dialogue & MenuTopicManager (Priority 4)**:
   - Direct engagement with the player or an NPC dialogue partner locks attention to the speaker's face.
   - Dialogue Player Hold (3.0s) prevents gaze dropping or flickering during mid-sentence audio pauses.
2. **Live Voice Address (`high->voiceState` & `lastSpokenToArray`)**:
   - Whenever an NPC speaks aloud (greetings, tavern banter, barmaid orders, or singing), nearby actors within conversational range prioritize the active speaker as their dialogue partner.
3. **Bards & Musical Performance**:
   - Performing bards (lute, flute, singing) run AI packages using `HEAD_TRACK_TYPE::kProcedure` and `kScript`.
   - Patrons within auditory and conversational radius scan the bard as a prominent social candidate.
4. **Proximity & Visual Field Scanning (Priority 1)**:
   - Humanoid patrons scan other actors within a 6.0m conversational room radius.
   - Strict forward visual cone (65°–75°) ensures patrons do not unnaturally twist their heads backwards to watch people behind them.
   - The Player receives an expanded social peripheral cone (110°), a 12.0m detection radius, and a 1.5m salience bonus (3.0m if addressing the player).
   - Fixation Dwell Hysteresis (1.5s) eliminates rapid target flapping in crowded rooms.
5. **Tavern Brawls & Combat (Priority 2)**:
   - If a brawl or combat erupts (e.g., Uthgerd the Unbroken), combatants transition to LOGIC mode (analytical/locked), tracking combat targets up to 15m.
6. **Social Triangle & HCEP AFFECT Scanpath**:
   - Rather than staring blankly, gaze naturally cycles across `LeftEye` (4x bias), `RightEye` (4x bias), `Mouth` (1x), and `Chest`.
7. **Race Categories & Temperament Profiles (`CharacterProfile.hpp`)**:
   - *Nords*: High confidence, prolonged direct eye contact.
   - *Elves*: Analytical, elevated Third-Eye fixation or status-based dominance.
   - *Khajiit & Argonians*: Heightened peripheral scanning and darting saccades.
   - *Relationship & Aggression*: Friends and lovers hold gaze longer; intimidated NPCs glance down (Lower-Right aversion).

### 13.2 3D Facial Focal Point Detection & Magnetic Eye Contact Lock
- **Physical Focal Detection**: Rather than arbitrary proximity spheres, TrueGaze evaluates the ray–face intersection at distance $D$. If the line of sight falls within the target's eye region ($|\Delta X| \le 3.5\text{ units}$, $|\Delta Z| \le 1.8\text{ units}$ or $\le 2.5^\circ$), an **Eye Contact Hit** (`state.eyeContactHit = true`) is registered.
- **Magnetic Attractor Dynamics ("Like a Magnet")**:
  - Micro-jitter is damped by 70% (`jitterDamp = 0.3f`) during eye contact, preventing physiological tremors from shaking the gaze off the target pupil.
  - An active magnetic restoring force ($\alpha = 0.85$) smoothly draws the ocular lines of sight into tight, stable alignment with the target pupil center.
- **Peer-to-Peer Mutual Gaze Accumulation**:
  - Evaluated bi-directionally across both Player $\leftrightarrow$ NPC and NPC $\leftrightarrow$ NPC interactions.
  - When both actors target each other and both achieve focal eye contact, mutual gaze duration accumulates, driving OAR animation conditions and HUD messages.

### 13.3 Distance Detection & Physiological Binocular Vergence (Dynamic Eye Crossing)
- **Distance Detection**: Euclidean distance in 3D world space $D = \|\vec{P}_{\text{target}} - \vec{P}_{\text{observer}}\|$ is tracked in real-time.
- **Vergence Angle Math**: Based on adult humanoid inter-pupillary distance ($IPD \approx 6.4\text{cm} = 4.48\text{ Skyrim units}$, half-IPD $b = 2.24\text{ units}$):
  $$\theta_{\text{vergence}} = \arctan\left(\frac{b}{\max(D, 3.5\text{ units})}\right) \times \frac{180}{\pi}$$
  - At 1.5m ($105\text{ units}$): $\sim 1.2^\circ$ inward rotation.
  - At 0.5m ($35\text{ units}$): $\sim 3.7^\circ$ inward rotation.
  - At 0.2m ($14\text{ units}$): $\sim 9.1^\circ$ inward rotation.
  - At point-blank ($< 0.1\text{m}$): $\sim 17^\circ\text{--}30^\circ$, causing realistic physical eye crossing.
- **Per-Eye Application**: `leftEyeYaw = eyeYaw + vergenceYawDeg` (+Yaw turns toward nose), `rightEyeYaw = eyeYaw - vergenceYawDeg` (-Yaw turns toward nose).
- **3D Visualizer Focal Depth Convergence**: Real-time target distance $D$ is passed to `VisualEffectsManager::UpdateActor`, causing 3D marker arrows, gaze rays, and terminus lights to physically converge and terminate at the exact depth of the target's face.

## 14. Combat Gaze Architecture: Multi-Target Dynamic Scanning, Tactical Action Glances & Footwork Saccades (R18)

### 14.1 Primary Focus Lock & Magnetic Combat Fixation
- In combat, human combatants, predatory creatures, and martial artists do not randomly wander their eyes; their gaze is predominantly locked onto the opponent's face, eyes, and upper torso to anticipate movement and read intent.
- `state.hcepMode = 0` (LOGIC): analytical focus with full speed scaling (`speedScale = 1.0f`).
- Magnetic attractor dynamics lock the ocular lines of sight onto the active adversary's pupils, with micro-jitter damped by 70% to prevent ocular tremor from breaking the locked stare.

### 14.2 Multi-Target Threat Awareness ("All Targets If Multiple")
- In skirmishes where multiple hostile adversaries engage the observer:
  - **Primary Adversary Focus**: The immediate combat target (`currentCombatTarget` or nearest attacker) retains primary gaze priority.
  - **Tactical Threat Sweeps**: Every 2.5–3.5s, if secondary hostile combatants are present within the visual field (within 80° heading), the observer executes a brief, sharp tactical scan saccade (0.50–0.55s) to evaluate the secondary hostile's position and distance.
  - **Imminent Danger Prioritization**: If any secondary hostile initiates an attack swing (`GetAttackState() != kNone`) within striking distance ($\le 6.0\text{m}$), attention immediately snaps to the attacking threat for 0.50s, regardless of the periodic scan timer.
  - **Guaranteed Return**: Following the secondary threat scan, the gaze immediately returns directly to the primary combat focus lock.

### 14.3 Close Encounter Tactical Action Glances (Strike & Block)
- When engaged in close-quarters melee ($\le 4.5\text{m}$ / ~315 units):
  - **Weapon Strike Glance (`CombatGlanceType::HandsWeapon`)**:
    - Trigger: When the adversary (or observer) initiates a strike, swing, or power attack (`GetAttackState() != kNone`, including `kSwing`, `kHit`, `kBash`, `kDraw`).
    - Focus Point: The opponent's active weapon hand or weapon node (`"WEAPON"`, `"NPC R Hand [RHnd]"`).
    - Duration: 0.40–0.45s glance to track the weapon trajectory.
    - Cooldown: 1.8s cooldown prevents rapid oscillation during multi-hit combos.
  - **Shield Block Glance (`CombatGlanceType::HandsShield`)**:
    - Trigger: When the adversary raises a shield block (`actorState2.wantBlocking` edge) or blocks an incoming strike.
    - Focus Point: The opponent's shield node or offhand guard (`"SHIELD"`, `"NPC L Hand [LHnd]"`).
    - Duration: 0.40s alert glance to assess defensive guarding.
    - Cooldown: 1.8s.
  - **Always Going Back to the Focus Lock**: The instant the glance timer expires, the target coordinate returns to the adversary's eye anchor. The magnetic attractor pulls the gaze smoothly and firmly back into the opponent's pupils.

### 14.4 Movement & Footwork Saccades
- In combat footwork, dodging, and tactical repositioning:
  - **Footwork Glance (`CombatGlanceType::FeetFootwork`)**:
    - Trigger: When the player (or combatant) initiates movement (`movingForward`, `movingBack`, `movingLeft`, `movingRight`, `running`, `sprinting` transition) or during sustained tactical maneuvering within close/medium combat range ($\le 8.0\text{m}$).
    - Focus Point: The opponent's feet or terrain footwork (`"NPC L Foot [Lft ]"`, `"NPC R Foot [Rft ]"` or ground level).
    - Duration: 0.30s crisp glance down to gauge footing and spacing.
    - Cooldown: 3.5s cooldown ensures natural, periodic footwork checks without excessive looking down.
  - **Magnetic Snap-Back**: As soon as the footwork glance ends, the eyes saccade right back up to the adversary's face and eyes, locking on like a magnet.

