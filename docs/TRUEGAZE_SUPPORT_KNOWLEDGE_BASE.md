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

This is expected when `tgstatus` reports lights but `beam geometry` remains zero. `NiPointLight` illuminates the scene; it is not visible beam geometry.

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

- asset resolver state in `tgstatus`;
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
6. Run `tgstatus`.
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
tgstatus output:
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
