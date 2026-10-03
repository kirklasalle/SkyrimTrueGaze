# TrueGaze Visual System — Crash & Missing Visuals Root Cause Analysis

## The Crash

**Sequence**: `stgv` OFF → `stgv` ON → **SEH fault 0xC0000005** (access violation)

### Log Evidence
```
[14:07:57.637] In-game visual emitters cleared.           ← stgv OFF: Reset() called
[14:07:57.637] Visual tuning: visuals=off rays=off        ← emitter map cleared

[14:08:03.302] Visual tuning: visuals=on rays=on          ← stgv ON
[14:08:04.662] EnsureBeamGeometry: 'fxsoulcairnbeam.nif' attempt 1   ← beam loads OK
[14:08:04.664] Visible beam geometry attached (fallback)  ← beam attached
[14:08:04.664] EnsureHcepPanel: 'GazeRegionPanel.nif'    ← panel load attempt
[14:08:04.666] SEH fault 0xC0000005 at 0x7ff7712ab564    ← CRASH HERE
```

### Root Cause: `GazeRegionPanel.nif` is a Corrupt/Malformed Binary NIF

The file is only **678 bytes**. For reference, `marker_arrow.nif` (~2KB) and `fxsoulcairnbeam.nif` (~4KB) are already minimal meshes. 678 bytes is **not** enough for a valid Skyrim NIF with proper BSTriShape geometry, shader properties, and vertex data. The header says "Gamebryo File Format, Version 20.2.0.7" and mentions `NiNode` and `BSTri...` (truncated) — this is a Python-generated stub that lacks complete vertex buffers and shader data.

**What happens**: `BSModelDB::Demand()` successfully parses the header (returns `kNone`), so the code proceeds to `model->Clone()`. The cloned node is attached to the skeleton's head bone. On the next render pass, Skyrim's renderer walks the geometry, finds malformed vertex/index buffers or a null shader property, and faults with 0xC0000005.

This is the **exact same failure mode** as the custom `GazeBeam.nif` that was abandoned on 2026-09-22/23 — Python-generated binary NIFs pass BSModelDB::Demand but crash the renderer.

### Why the Beam Works Fine (the "Fallback Difference")

The beam uses `meshes\dlc01\effects\fxsoulcairnbeam.nif` — a **Bethesda-authored asset** from the Dawnguard DLC BSA. It has proper vertex buffers, proper BSEffectShaderProperty, proper NiAlphaProperty. It was authored in Bethesda's toolchain and is renderer-safe.

The only thing ever successfully visualized in-game was this Dawnguard beam + the NiPointLight emitters. Both are engine-native objects that don't depend on any custom art.

## The Fix Strategy

### 1. Eliminate the custom GazeRegionPanel.nif entirely
Use **NiPointLight emitters** for the panel visualization — the same proven-stable approach as the gaze rays. Instead of loading a custom mesh that crashes the renderer, create a grid of positioned NiPointLights that form a visible "panel" of colored dots.

### 2. Fix the toggle crash (defensive null-geometry guard)
Even after fixing the panel, the code must never let a `Clone()` result of a malformed NIF reach the renderer. Add validation that the cloned geometry has non-zero child geometry before attaching.

### 3. Fix the Reset→Re-create lifecycle
The `Reset()` path clears the emitter map but `_geometryModeReported` is never reset, causing diagnostic logging to go silent on re-enable. More critically, `DetachAll` must safely handle the case where the parent node was destroyed between toggle-off and toggle-on (cell change, actor 3D rebuild).

## Crash-Safe Architecture: Light-Based Panel

Instead of loading any custom NIF for the panel, create a constellation of **9 positioned NiPointLights** (one per HCEP region: LeftEye, RightEye, Mouth, Forehead, Chin, Torso, and 4 CGA peripherals). Each light:
- Sits at its geometric position relative to the head bone
- Uses the region's diagnostic colour
- The active region gets full brightness; inactive regions get dim
- No mesh, no shader, no BSA dependency, no crash risk

This is the "proven fallback" approach that has **always worked** in-game.
