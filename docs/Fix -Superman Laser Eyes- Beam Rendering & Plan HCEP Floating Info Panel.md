# Fix "Superman Laser Eyes" Beam Rendering & Plan HCEP Floating Info Panel

## Problem Summary

**Option 1 (Fix Now):** The in-game gaze effects are rendering as massive, opaque, translucent **triangular cones/wedges** filling the screen (see [ingame_ScreenShot_11.png](file:///d:/Projects/SkyrimTrueGaze/docs/screengrabs/ingame_ScreenShot_11.png)), instead of thin, focused "Superman laser eye" beams originating from the NPC pupils. The beams should be narrow, directed lines from each eye — not broad floodlights.

**Option 2 (Plan Only — after Option 1 succeeds):** The HCEP information diagram from [hcep-02_enhanced.png](file:///d:/Projects/SkyrimTrueGaze/docs/images/hcep-02_enhanced.png) should float as a textured plane in front of NPCs, with the laser eye beams naturally sweeping through the labelled gaze regions (Third-eye, Upper, Lower, Far Upper, Far Lower, Chest, etc.).

---

## Option 1: Fix Laser Eye Beams

### Root Cause Analysis

After thorough code inspection, I've identified **four compounding bugs** that together produce the massive translucent wedge shapes visible in the screenshot:

#### Bug 1: Beam geometry rotation is never applied

In [VisualEffectsManager.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualEffectsManager.cpp#L321-L339), the `BeamRotation()` helper function (lines 40–54) is **defined but never called**. The geometry update block computes `localDirection` but never uses it to set `emitters.geometry->local.rotate`. The rotation matrix remains at identity, so the beam always points along its authored axis regardless of where the NPC is actually looking.

```cpp
// Line 325: direction is computed...
const RE::NiPoint3 localDirection = anchorInverse.rotate * dirWorld;
// Line 326: position is set...
emitters.geometry->local.translate = localPupil;
// ...but local.rotate is NEVER set! The beam keeps its default orientation.
```

#### Bug 2: `marker_arrow.nif` fallback is a large conical mesh

The custom [GazeBeam.nif](file:///d:/Projects/SkyrimTrueGaze/skyrim/meshes/TrueGaze/GazeBeam.nif) (1,735 bytes) has a `BSFadeNode` root — which is the wrong root type for an attached child node. `BSFadeNode` participates in the engine's actor fading system and gets alpha-blended at close range, which produces the ghostly translucent wedge. The fallback mesh (`meshes\marker_arrow.nif`) is even worse — it's a visible directional arrow with significant geometry.

The [GenerateGazeBeamNif.py](file:///d:/Projects/SkyrimTrueGaze/scripts/GenerateGazeBeamNif.py) script was designed to use `NiNode` as root (line 147), but the deployed NIF on disk uses `BSFadeNode` — likely a stale build or manual edit. This mismatch means the NIF was regenerated incorrectly at some point.

#### Bug 3: Uniform scale inflates the beam cross-section

In [VisualEffectsManager.cpp line 332](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualEffectsManager.cpp#L330-L332), the beam is scaled uniformly:

```cpp
emitters.geometry->local.scale = targetScale;  // uniform scale!
```

With `gazeRayLengthMeters = 10.0` and `kNaturalLengthUnits = 240.0`, `targetScale` computes to `700/240 ≈ 2.9`. This scales X, Y, **and Z** equally — turning a thin cylinder into a fat cone 2.9× wider than intended. For a "laser beam" effect, the beam should remain pencil-thin regardless of length.

#### Bug 4: NiPointLight radius is proportional to beam length

In [VisualEffectsManager.cpp line 388-389](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualEffectsManager.cpp#L388-L389):

```cpp
const float radius = std::max(kMinBeamRadius,
                              _tuning.LengthUnits() * kBeamRadiusFraction * radiusScale);
```

With `LengthUnits() = 700` and `kBeamRadiusFraction = 0.04`, the light radius is `700 × 0.04 = 28` Skyrim units (~40cm). This is an enormous point light — it illuminates the entire face and nearby walls with the gold colour, adding to the "floodlight" appearance. A pupil glow should be ~4–8 units maximum.

---

### Proposed Changes

#### [MODIFY] [VisualEffectsManager.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualEffectsManager.cpp)

**Change 1: Apply beam rotation every frame (fix Bug 1)**

After computing `localDirection`, call `BeamRotation()` to build a proper rotation matrix and assign it to `emitters.geometry->local.rotate`:

```diff
         if (emitters.geometry)
         {
             // The beam model is authored along local +Y. Reposition and
             // orient its root every update so it visibly follows the solved gaze.
             const RE::NiPoint3 localDirection = anchorInverse.rotate * dirWorld;
             emitters.geometry->local.translate = localPupil;
-            // Natural length of marker_arrow.nif is ~240 Skyrim units (~3.4m).
-            // Scale proportionally to gazeRayLengthMeters, with safety clamp to avoid
-            // triggering BSPortalGraph room culling in interior cells.
-            constexpr float kNaturalLengthUnits = 240.0f;
-            const float targetScale = std::clamp(_tuning.LengthUnits() / kNaturalLengthUnits, 0.5f, 3.0f);
-            emitters.geometry->local.scale = targetScale;
+            emitters.geometry->local.rotate = BeamRotation(localDirection);
+            // Scale uniformly to 1.0 — the beam NIF is authored as a unit-length
+            // cylinder; actual reach is handled by the geometry's own vertex data
+            // which is regenerated at the configured length. Keep scale at 1.0
+            // so the pencil-thin cross-section is not inflated.
+            emitters.geometry->local.scale = 1.0f;
```

**Change 2: Fix NiPointLight radius (fix Bug 4)**

Replace the length-proportional radius with a small fixed value appropriate for a pupil glow:

```diff
-        /// Beam radius as a fraction of its length. A fixed radius would be invisible
-        /// on a long beam and a blob on a short one, so the light scales with reach.
-        constexpr float kBeamRadiusFraction = 0.04f;
-        constexpr float kMinBeamRadius = 4.0f; // Skyrim units
+        /// Fixed pupil glow radius. The light marks the beam origin, not the beam
+        /// itself, so a small constant produces the correct "laser source" look.
+        constexpr float kPupilGlowRadius = 6.0f;   // Skyrim units (~8.6 cm)
+        constexpr float kTerminusGlowRadius = 4.0f; // slightly smaller landing dot
```

And update `EnsureLight`:

```diff
-        const float radiusScale = a_pupil ? 1.0f : 0.6f;
-        const float radius = std::max(kMinBeamRadius,
-                                      _tuning.LengthUnits() * kBeamRadiusFraction * radiusScale);
+        const float radius = a_pupil ? kPupilGlowRadius : kTerminusGlowRadius;
```

---

#### [MODIFY] [GenerateGazeBeamNif.py](file:///d:/Projects/SkyrimTrueGaze/scripts/GenerateGazeBeamNif.py)

Regenerate the beam mesh with corrected properties:

1. **Use `NiNode` root** (not `BSFadeNode`) — prevents actor fading interference
2. **Set `radius = 0.3` Skyrim units** (~0.43 cm) — pencil-thin laser beam
3. **Set `length` to match `LengthUnits()`** (default 700 units = 10m) — the mesh itself defines the beam length at full scale (1.0), eliminating the need for uniform scaling that inflates the cross-section
4. **Increase `sides` to 6** — hexagonal cross-section is fine for a thin beam (saves tris vs. 8)
5. **Use proper additive blending flags** in `BSEffectShaderProperty` for the glowing laser look

The script default call will become:
```python
generate_gaze_beam_nif(dest, radius=0.3, length=700.0, sides=6)
```

After running the updated script, the regenerated [GazeBeam.nif](file:///d:/Projects/SkyrimTrueGaze/skyrim/meshes/TrueGaze/GazeBeam.nif) will be a proper thin cylinder along +Y.

---

#### [MODIFY] [VisualTuning.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualTuning.hpp)

Add a configurable beam thickness parameter so the cross-section can be tuned without rebuilding the NIF:

```diff
         float gazeRayLengthMeters{10.0f};
+        float gazeRayThicknessCm{0.8f};   // beam cross-section diameter; default ~8mm pencil beam
```

---

#### [MODIFY] [ConfigManager.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ConfigManager.hpp) / [ConfigManager.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ConfigManager.cpp)

Add the new `fGazeRayThicknessCm` INI key to the `[Visuals]` config block so modders can tune beam width.

---

### Summary of What Changes

| File | Change | Fixes |
|------|--------|-------|
| `VisualEffectsManager.cpp` | Apply `BeamRotation(localDirection)` to `local.rotate` | Bug 1: Beam now points along gaze |
| `VisualEffectsManager.cpp` | Use fixed small radius for pupil/terminus lights | Bug 4: No more floodlight glow |
| `VisualEffectsManager.cpp` | Set `local.scale = 1.0f` (NIF is authored at full length) | Bug 3: No cross-section inflation |
| `GenerateGazeBeamNif.py` | Correct root type to `NiNode`, thin radius, full-length cylinder | Bug 2: Proper thin beam mesh |
| `VisualTuning.hpp` | Add `gazeRayThicknessCm` parameter | Future tuning knob |
| `ConfigManager.hpp/.cpp` | Expose `fGazeRayThicknessCm` in INI | Config completeness |

### Expected Visual Result

After these fixes, each NPC will show:
- **Two narrow, gold, emissive beams** (one per actor — the "midpoint pupil" origin) projecting outward from the eye area along the solved gaze direction
- **A tiny gold glow** at the pupil origin (≈8.6cm radius)
- Beams that **rotate and track** the gaze target in real-time, proving the kinematics solver is working
- Similar to "Superman laser eyes" — thin, bright, directed

---

## Option 2: HCEP Floating Info Panel (Plan Only — Deferred)

> [!IMPORTANT]
> This section is **saved for planning and implementation after Option 1 is completed successfully**. No code changes will be made for Option 2 until the laser eyes are confirmed working in-game.

### Vision

Based on [hcep-02_enhanced.png](file:///d:/Projects/SkyrimTrueGaze/docs/images/hcep-02_enhanced.png), the goal is to display the HCEP gaze-region diagram as a **floating textured plane** in front of each NPC in 3D space, with:

- The diagram (Third-eye, Upper/Lower regions, Far Upper/Lower, Chest/Heart, Right/Left eye, Mouth) rendered as a billboard or head-anchored quad
- The laser eye beams naturally sweeping through and pointing at the labelled regions as the gaze moves
- The plane tracking the NPC's head position/orientation
- Configurable visibility (developer diagnostic, not player-facing)

### Preliminary Architecture

1. **New NIF asset:** `meshes\TrueGaze\GazeRegionPanel.nif` — a flat quad (billboard) with the HCEP diagram as a DDS texture
2. **New DDS texture:** `textures\TrueGaze\GazeRegionPanel.dds` — the HCEP-02 diagram, cleaned up for in-game use (remove the person, keep only the labelled regions and arrows)
3. **Attachment:** Under the head bone, offset forward by ~40–60cm so it floats in front of the NPC's face
4. **Billboard behaviour:** Either camera-facing billboard (NiBillboardNode) or head-anchored with manual orientation
5. **Integration with existing beam:** The laser beams (from Option 1) will naturally point through the panel since both are driven by the same solved gaze vector
6. **Config key:** `bShowHcepPanel` in `[Visuals]` section, default off

### Open Questions for Option 2

- Should the panel be camera-facing (billboard) or anchored to the NPC's head orientation?
- Should the currently-active gaze region be highlighted (e.g., glow the "Upper region" circle when gaze is in that zone)?
- What panel size in-world? (~50cm × 30cm seems reasonable for readability)
- Should the panel show for all NPCs or only the one the player is currently interacting with?

---

## Verification Plan

### Automated Tests
- Build verification: `cmake --build build --config Release` — confirms compilation with all changes

### Manual Verification (Option 1)
1. Run `python scripts/GenerateGazeBeamNif.py` to regenerate the beam NIF
2. Deploy to Skyrim Data folder via `scripts/Deploy-TrueGaze.ps1`
3. Launch Skyrim SE, enable visuals via console (`tgvisuals`) or TrueGaze.ini
4. Verify in a tavern scene (multiple NPCs):
   - Beams originate from NPC eye area (not chest or feet)
   - Beams are **thin** (pencil-width, not conical wedges)
   - Beams **track** with gaze direction (follow the gaze target)
   - Pupil glow is small and localised
   - No screen-filling translucent overlay
5. Take comparison screenshot to confirm the fix against [ingame_ScreenShot_11.png](file:///d:/Projects/SkyrimTrueGaze/docs/screengrabs/ingame_ScreenShot_11.png)
