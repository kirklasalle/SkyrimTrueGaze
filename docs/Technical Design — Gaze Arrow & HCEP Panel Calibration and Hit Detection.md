# Technical Design — Gaze Arrow & HCEP Panel Calibration and Hit Detection

**Project:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine  
**Author:** Antigravity (Engineering), Kirk LaSalle (Director & Approver)  
**Date:** 2026-10-03  
**Status:** ✅ Approved — Ready for Development  
**Tracks:** Audit findings A1–A9 from the 2026-10-03 visual review

---

## 1. Purpose

This document defines the engineering changes required to make the Superman Laser
Eyes arrows and the HCEP Floating Diagram Panel into an **accurate, calibrated
development instrument** rather than a rough visual indicator. Specifically:

1. Scale the arrow cross-section to the actor's eyeball diameter.
2. Fix the matrix axis bug that shears the arrow direction.
3. Re-anchor the panel so it aligns with the gaze classifier's coordinate frame.
4. Add ray–panel hit detection so the arrow physically intersects the correct
   region on the panel.
5. Produce three-way agreement data (intended → classified → hit) every frame.
6. Generate structured trace logs for debugging and tuning.

---

## 2. Audit Findings & Resolutions

| ID | Finding | Resolution | Section |
|:---|:--------|:-----------|:--------|
| A1 | Cross-section scaling writes rows instead of columns | Scale rotation matrix **columns** 0 and 2 | §3.2 |
| A2 | Arrow, classifier, panel use different reference frames | Unify on head-local angles | §4 |
| A3 | Region labels are body-relative, not partner-relative | Intentional — document clearly | §5.2 |
| A4 | Panel layout doesn't match classifier's angular thresholds | Generate panel art from the angular region spec | §5 |
| A5 | CGA quadrant left/right may be mirrored | Use gazer's own left = Upper-Left Peripheral | §5.1 |
| A6 | Panel not centred on eye height | Offset panel origin to eye anchor | §4.3 |
| A7 | Both arrows share one direction vector | Separate per-eye aim toward shared target point | §3.4 |
| A8 | Head-bone axis convention assumed, not verified | Add `stgcal` calibration command | §8 |
| A9 | Housekeeping: duplicate enums, magic numbers, stale data | Clean up in implementation | §9 |

---

## 3. Arrow Geometry — Eyeball-Scale Cross-Section

### 3.1 Target Dimensions

| Parameter | Real-world | Skyrim units (70 u/m) | Source |
|:----------|:-----------|:----------------------|:-------|
| Human eyeball diameter | 24 mm | 1.68 | Anatomical average |
| Human iris diameter | 12 mm | 0.84 | — |
| Human pupil diameter (light-adapted) | 3 mm | 0.21 | — |

**Decision (Kirk-approved):** arrow cross-section = **full eyeball diameter** (24 mm × world scale).
The arrowhead widens to 1.5× the shaft width. This is large enough to see from
a few metres but small enough that it reads as a precise pointer rather than a
broad beam.

### 3.2 Scaling Method — Fix A1

The current code ([VisualEffectsManager.cpp:L558–563](../src/Visuals/VisualEffectsManager.cpp#L558-L563)):

```cpp
// BROKEN — scales rows, not columns
for (int row = 0; row < 3; ++row) {
    geometryRot.entry[0][row] *= kCrossSectionMult;  // row 0
    geometryRot.entry[2][row] *= kCrossSectionMult;  // row 2
}
```

The matrix stores basis vectors as **columns** (verified at [L131–154](../src/Visuals/VisualEffectsManager.cpp#L131-L154)):
`entry[row][col]`, column 0 = X (right), column 1 = Y (forward), column 2 = Z (up).

Scaling `entry[0][row]` for all `row` scales **row 0**, which mixes parts of
every column and shears the entire basis.

**Correct approach:** scale columns 0 and 2 (the width and height axes), leaving
column 1 (the forward/length axis) untouched:

```cpp
// Scale columns 0 (X = width) and 2 (Z = height)
for (int row = 0; row < 3; ++row) {
    geometryRot.entry[row][0] *= crossSectionScale;  // column 0
    geometryRot.entry[row][2] *= crossSectionScale;  // column 2
}
```

### 3.3 Scale Computation

When a beam mesh is first loaded, measure its bounding box from the cloned
`NiNode`:

```
nativeLength = bounds.max.y - bounds.min.y   // along +Y (authored forward)
nativeWidth  = bounds.max.x - bounds.min.x   // cross-section X
nativeHeight = bounds.max.z - bounds.min.z   // cross-section Z
```

Then each frame:

```
targetLength = tuning.LengthUnits()
targetWidth  = eyeDiameterUnits  // 1.68 × headBone.world.scale

uniformScale = targetLength / nativeLength
crossSectionScaleX = targetWidth / (nativeWidth × uniformScale)
crossSectionScaleZ = targetWidth / (nativeHeight × uniformScale)
```

Set `local.scale = uniformScale`, and apply `crossSectionScaleX` / `crossSectionScaleZ` to
columns 0 and 2 of the rotation matrix as shown above.

### 3.4 Per-Eye Convergence — Fix A7

Currently both arrows share one `dirWorld`. For anatomical accuracy, each eye
should aim at the shared gaze terminus:

```
terminusWorld = midpointPupils + gazeDir × lengthUnits
dirL = normalize(terminusWorld - originLWorld)
dirR = normalize(terminusWorld - originRWorld)
```

At 1 m this gives about 1.8° inward convergence per eye — visible and correct.

### 3.5 Base Placement

The arrow's local-space minimum Y (`bounds.min.y`) is offset so the visible base
sits exactly on the pupil origin:

```
baseOffset = -bounds.min.y × uniformScale
translate = localPupil + localDir × baseOffset
```

This replaces the current `kMarkerArrowNativeHalfLengthUnits` magic number.

### 3.6 Eye Diameter Source

1. **Primary:** Walk the FaceGen `BSFaceGenNiNode` children and read the bounding
   sphere of the eye mesh tri-shape (names containing `Eye` or `_eye_`). Double
   the radius gives the visible eye diameter. Cache per actor on skeleton resolve.
2. **Fallback:** `24 mm × headBone.world.scale × 70 / 1000`.

### 3.7 INI Keys

| Key | Section | Type | Default | Description |
|:----|:--------|:-----|:--------|:------------|
| `fArrowCrossSectionSource` | `[Visuals]` | int | `0` | 0 = auto (mesh probe), 1 = fixed 24 mm |
| `fArrowCrossSectionOverrideMm` | `[Visuals]` | float | `24.0` | Manual override when source = 1 |
| `fArrowMinScreenWidthPx` | `[Visuals]` | float | `2.0` | Minimum on-screen width (prevents invisible lines at distance) |

---

## 4. Reference Frame Unification — Fix A2

### 4.1 The Problem

Three subsystems each use a different angular frame for "where is the actor looking":

| Subsystem | Frame | Variable |
|:----------|:------|:---------|
| `ClassifyRegion` | Body-relative total deflection | `yawDeg`, `pitchDeg` from `ComputeDeflection` |
| Arrow direction | Head-bone-relative eye residual | `eyeYaw`, `eyePitch` (clamped, jittered) |
| Panel position | Head-bone-local, fixed offset | `HcepPanelForwardOffsetUnits()` |

The arrow follows the eye residual, but the classifier labels the total body
deflection. When the head has turned 15° right toward a target, the classifier
reports region 1 (RightEye) while the eye residual is close to 0° and the arrow
points straight ahead.

### 4.2 Solution

**For the hit-detection system only** (the existing rendering remains correct):
reconstruct the total gaze direction in head-local space.

```
totalGazeWorld = gazeDirection(headBasis, totalYawRad, totalPitchRad)
totalGazeHeadLocal = headWorldInverse.rotate × totalGazeWorld
```

This `totalGazeHeadLocal` is used for ray–panel intersection and for the new
reticle overlay. The arrows continue to render from the eye residual (which is
what the skeleton actually shows), and the three-way diagnostic reports when the
arrow's rendered direction diverges from the classified total direction.

### 4.3 Panel Origin Correction — Fix A6

Move the panel's local origin from `(0, forward, 0)` to the eye anchor point in
head-local space:

```
panelOrigin = (0, HcepPanelForwardOffsetUnits(),
               eyeAnchorUpOffsetUnits - headBoneToEyeUp)
```

Where `eyeAnchorUpOffsetUnits` is derived from `pupilUpOffsetCm`. This centres
the panel on the eyeline so a zero-deflection gaze hits the panel's centre.

---

## 5. Region Map Specification

### 5.1 Handedness Convention

**Gazer's own left = "Upper-Left Peripheral" (Positivity/Hope, region 9).**

This follows NLP/CGA literature where the subject's own left corresponds to
"visual recall / positivity" aversion. The HCEP-02 diagram is drawn from the
observer's point of view (facing the subject), so the diagram label "Upper
region (Positivity, happiness, hope)" appears on the **observer's left** =
**gazer's right** in the diagram image. The panel will be re-authored so the
gazer's left maps to negative yaw (consistent with `ClassifyRegion`).

### 5.2 Angular Region Definitions

All angles are gazer-body-relative. Positive yaw = gazer's right. Positive
pitch = up. These boundaries drive the classifier, the panel art layout, and the
hit-detection lookup.

| ID | Name | Yaw Range (°) | Pitch Range (°) | Panel Art Position |
|:---|:-----|:--------------|:-----------------|:-------------------|
| 0 | LeftEye | [−∞, −1) | [−3, +3] | Projected from (−1.8°, +0.5°) |
| 1 | RightEye | (+1, +∞] | [−3, +3] | Projected from (+1.8°, +0.5°) |
| 2 | Mouth | [−5, +5] | [−6, −3) | Projected from (0°, −4.5°) |
| 3 | Forehead/ThirdEye | [−5, +5] | (+3, +12] | Projected from (0°, +7°) |
| 4 | Chin | [−5, +5] | [−12, −6) | Projected from (0°, −9°) |
| 5 | Torso/Chest | [−5, +5] | (−6, −3) ∩ central | Projected from (0°, −4.5°) below mouth |
| 8 | Ground | [−5, +5] | (−∞, −12) | Projected from (0°, −18°) |
| 9 | UL Peripheral | (−∞, −5) | (+12, +∞) | Projected from (−15°, +18°) |
| 10 | UR Peripheral | (+5, +∞) | (+12, +∞) | Projected from (+15°, +18°) |
| 11 | LL Peripheral | (−∞, −5) | (−∞, −12) | Projected from (−15°, −18°) |
| 12 | LR Peripheral | (+5, +∞) | (−∞, −12) | Projected from (+15°, −18°) |

> [!NOTE]
> Regions 6 (RightHand) and 7 (LeftHand) are defined in the HCEP-02 spec but
> not yet classified by the engine. They are reserved and the panel leaves space
> for them.

### 5.3 Panel Projection Formula

Each region's representative angle `(yaw°, pitch°)` projects onto the panel
plane at distance `d` (in Skyrim units) from the eye anchor:

```
panelX = d × tan(yaw × π/180)
panelZ = d × tan(pitch × π/180) / cos(yaw × π/180)
```

The region boundary shapes (polygons or circles) use the same projection on the
boundary angles. This guarantees that **if the arrow is aimed at a boundary
angle, its intersection with the panel lies exactly on the boundary line**.

---

## 6. Ray–Panel Hit Detection

### 6.1 Geometry

All computation is in **head-local space** (the panel's parent frame).

```
Ray origin  O = eye anchor in head-local (midpoint of left/right pupil locals)
Ray direction D = totalGazeHeadLocal (§4.2)
Panel plane: Y = panelForwardOffset (a constant)
```

Intersection parameter:

```
t = (panelForwardOffset - O.y) / D.y
```

If `t ≤ 0` or `D.y ≈ 0`, the gaze is behind or parallel to the panel — report
`hitRegion = 0xFF` (no hit).

Hit point on the panel:

```
hitX = O.x + D.x × t
hitZ = O.z + D.z × t
```

### 6.2 Region Lookup

Convert `(hitX, hitZ)` to angles:

```
hitYawDeg  = atan2(hitX, panelForwardOffset) × 180/π
hitPitchDeg = atan2(hitZ, panelForwardOffset) × 180/π
```

Then pass `(hitYawDeg, hitPitchDeg)` through the **same `ClassifyRegion`
function** used by the engine. This guarantees the lookup and the classifier can
never disagree on boundary cases.

### 6.3 Performance Budget

Per actor per frame: 1 division, 2 multiplies, 2 additions, 2 `atan2` calls, 1
branch ladder. Total: well under 100 ns on any modern CPU. No engine raycasts.

### 6.4 Reticle Overlay (Optional, Phase 2)

A small crosshair mesh (reusing `marker_arrow.nif` at 0.3 units, flat in XZ)
placed at the hit point on the panel's near side. If the reticle sits on the
arrow where it crosses the panel, the maths matches the render. Toggled by
`stgv reticle`.

---

## 7. Three-Way Agreement Diagnostics

Each frame, for each actor with visuals enabled, compute:

| Channel | Source | Variable |
|:--------|:-------|:---------|
| **Intended** | Social triangle / CGA vertex | `state.triangle.currentVertex` mapped to region ID |
| **Classified** | `ClassifyRegion(totalYaw, totalPitch)` | `state.gazeRegion` |
| **Hit** | Ray–panel intersection | `hitRegion` (§6.2) |

### 7.1 Agreement Rules

- **Settling window:** ignore mismatches for 150 ms after a saccade onset (the
  eye and head are in transit).
- **Head-chain yield:** when `sceneDeferActive`, the head is controlled by
  vanilla animation — the arrow won't match the classified total, and this is
  expected. Flag it as `DEFERRED` rather than a mismatch.
- **Classified vs Hit** should agree in **≥ 98% of settled frames** once A1 and
  A6 are fixed.

### 7.2 Counters (Exposed via `stgstatus`)

```
Gaze Accuracy:
  Settled frames:       12,450
  Classified==Hit:      12,311  (98.9%)
  Mismatches:              139  ( 1.1%)
  Deferred (ignored):    1,204
  Intended!=Classified:    387  (saccade in progress or CGA → face return)
```

---

## 8. Calibration Commands

### 8.1 `stgcal <yaw> <pitch>`

Forces the gaze engine to override the next actor's deflection to the given
angles (body-relative degrees). Useful to verify:

- The arrow points in the expected direction.
- The panel displays the correct region highlighted.
- The hit-detection reports the correct region.
- The sign conventions (left/right, up/down) are correct.

### 8.2 `stgcal sweep`

Steps through each region's representative angle (from §5.2), holding each for
2 seconds, and logs the classified region, the hit region, and whether they
agree. Prints a summary table at the end.

### 8.3 `stgcal axes`

Draws three thin coloured lines along the head bone's local X (red), Y (green),
Z (blue) axes for 10 seconds. Verifies the assumption that X = right, Y =
forward, Z = up. This settles finding A8.

### 8.4 INI Key

| Key | Section | Type | Default |
|:----|:--------|:-----|:--------|
| `bEnableCalibrationCommands` | `[Debug]` | bool | `false` |

---

## 9. Housekeeping — Fix A9

### 9.1 Unify Region ID Enums

`SocialTriangle::Vertex` (0–8) and `gazeRegion` (0–12) overlap but differ. Add a
shared `enum class GazeRegion : uint8_t` in a new header
`src/Engine/GazeRegion.hpp`. Both `SocialTriangle` and `ClassifyRegion` use it.
The public API (`TrueGazeAPI.h`) documents the mapping.

### 9.2 Eliminate Magic Numbers

| Current Magic | Replacement |
|:-------------|:------------|
| `kMarkerArrowScaleMultiplier = 0.30f` | Computed from mesh bounds |
| `kMarkerArrowCrossSectionMultiplier = 0.09f` | `eyeDiameterUnits / (nativeWidth × uniformScale)` |
| `kMarkerArrowNativeHalfLengthUnits = 120.0f` | `(bounds.max.y - bounds.min.y) / 2` |
| `kPanelSourceUnits = 512.0f` | Read from the panel mesh bounds |
| `kDiagramAspect = 2760.0f / 1504.0f` | Read from the texture dimensions or a constant derived from the region spec |

### 9.3 Initial Tint on Creation

Currently, tinting only happens when `lastGazeRegion != gazeRegion`. On creation,
`lastGazeRegion = 0xFF`, so the first frame always triggers a tint. This is
correct. Verify this path is exercised in the unit tests.

### 9.4 Stale `kPanelRegions` Data

The `kPanelRegions[11]` array holds coordinates for the old light-constellation
approach. Once the panel uses the angular projection (§5.3), this array is
replaced by the region spec lookup.

---

## 10. Implementation Phases

### Phase 1 — Correctness (High Priority)

1. Fix A1: column scaling in `VisualEffectsManager.cpp`.
2. Fix A6: panel origin offset to eye anchor.
3. Fix A5: verify CGA handedness with `stgcal axes` and `stgcal sweep`.
4. Implement per-eye convergence (A7).
5. Measure mesh bounds on load, eliminate magic numbers (A9).
6. Scale arrow cross-section to eyeball diameter.

### Phase 2 — Detection & Diagnostics

7. Implement ray–panel hit detection (§6).
8. Add three-way agreement counters (§7).
9. Add `stgcal`, `stgcal sweep`, `stgcal axes` (§8).
10. Expose agreement stats in `stgstatus`.

### Phase 3 — Trace Logging

11. Implement `TrueGaze_GazeTrace.jsonl` (see Trace Log Schema document).
12. Add `stgtrace on|off` command.

### Phase 4 — Panel Art Regeneration

13. Generate panel texture from angular spec (§5.3).
14. Replace `kPanelRegions` with spec-driven data.
15. Optional reticle overlay.

---

## 11. Files Changed (Estimated)

| File | Nature of Change |
|:-----|:-----------------|
| `src/Visuals/VisualEffectsManager.cpp` | A1 fix, eyeball scaling, convergence, hit detection, reticle |
| `src/Visuals/VisualEffectsManager.hpp` | New fields: mesh bounds, hit result, agreement counters |
| `src/Visuals/VisualTuning.hpp` | New INI-driven fields for arrow sizing, trace toggle |
| `src/Engine/GazeRegion.hpp` | **New file** — shared region enum |
| `src/Engine/GazeEngine.cpp` | Pass total deflection to visual manager, calibration override |
| `src/Engine/ConfigManager.hpp` | New INI keys |
| `src/Engine/ConfigManager.cpp` | Load/save new keys |
| `src/Integrations/ConsoleCommands.cpp` | `stgcal`, `stgtrace` commands |
| `include/TrueGazeAPI.h` | Document `GazeRegion` enum values |
| `scripts/GenerateHcepPanelTexture.py` | Spec-driven projection layout |
| `tests/` | Unit tests for matrix, ray–plane, region lookup |

---

*Approved by Kirk LaSalle, 2026-10-03. Development may begin.*
