# Verification & Test Plan — Gaze Arrow & HCEP Panel Calibration

**Project:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine  
**Author:** Antigravity (Engineering), Kirk LaSalle (Director & Approver)  
**Date:** 2026-10-03  
**Status:** ✅ Approved — Ready for Development  
**Companion:** [Technical Design](Technical%20Design%20%E2%80%94%20Gaze%20Arrow%20&%20HCEP%20Panel%20Calibration%20and%20Hit%20Detection.md) · [Region Map Specification](Region%20Map%20Specification%20%E2%80%94%20HCEP-02%20Gaze%20Regions.md)

---

## 1. Scope

This plan covers every testable claim made by the Technical Design and Region
Map Specification. Tests are split into three tiers:

| Tier | Where | Requires Game? | Automated? |
|:-----|:------|:---------------|:-----------|
| **Unit** | `tests/` (C++ catch2 / doctest) | No | Yes |
| **Calibration** | In-game console (`stgcal`) | Yes | Semi (logs parsed) |
| **Integration** | Live gameplay screenshots + trace logs | Yes | No |

---

## 2. Acceptance Criteria (Quantitative)

These thresholds define **pass/fail** for the calibration work as a whole.

| Criterion | Target | Measurement Method |
|:----------|:-------|:-------------------|
| Arrow base position | Within **0.5 Skyrim units** of the pupil origin | `stgcal 0 0` + screenshot overlay |
| Arrow length | Within **±1%** of `tuning.LengthUnits()` | Unit test: compare `bounds × scale` to config |
| Arrow cross-section width | Within **±5%** of the eye diameter | Unit test: compare `crossSectionScale × nativeWidth × uniformScale` to `eyeDiameterUnits` |
| Hit region == Classified region | **≥ 98%** of settled frames | Three-way agreement counters (`stgstatus`) |
| Intended region == Classified region | **≥ 90%** of settled frames | Agreement counters (lower because saccade transit is expected) |
| `stgcal sweep` all-pass | **11/11 regions** hit correctly | Console summary output |
| No renderer crash | **0 SEH faults** across 30-minute soak test | `TrueGaze.log` scan for `SEH` or `0xC0000005` |

---

## 3. Unit Tests

All unit tests live in `tests/` and compile without Skyrim headers
(`__has_include(<RE/Skyrim.h>)` is false). They test pure maths only.

### 3.1 Matrix Column Scaling

**File:** `tests/test_matrix_scaling.cpp`

| Test Case | Input | Expected |
|:----------|:------|:---------|
| Identity matrix, scale columns 0 and 2 by 0.5 | `M = I` | `M[r][0] = 0.5` for r∈{0,1,2}; `M[r][1]` unchanged; `M[r][2] = 0.5` |
| Rotated basis, scale column 0 only | 45° yaw rotation | Column 0 halved, columns 1 and 2 unchanged |
| Scale = 1.0 (no-op) | Any basis | Matrix unchanged |
| Scale = 0.0 (degenerate) | Any basis | Columns 0, 2 zeroed; column 1 preserved |

**Verifies A1 fix:** confirms that the new loop `entry[row][col]` scales columns,
not rows.

### 3.2 AlignBeamOrientation

**File:** `tests/test_align_beam.cpp`

| Test Case | Target Dir | Authored Along | Expected Column 1 (or 2) |
|:----------|:-----------|:---------------|:--------------------------|
| +Y forward | (0, 1, 0) | Z = false | Column 1 = (0, 1, 0) |
| +Y forward | (0, 1, 0) | Z = true | Column 2 = (0, 1, 0) |
| +X right | (1, 0, 0) | Z = false | Column 1 = (1, 0, 0) |
| +Z up | (0, 0, 1) | Z = false | Column 1 = (0, 0, 1) |
| Diagonal (1,1,0) normalised | (0.707, 0.707, 0) | Z = false | Column 1 ≈ (0.707, 0.707, 0) |
| Zero vector (degenerate) | (0, 0, 0) | either | Returns identity |

**Verifies:** the basis vectors end up in columns (not rows), and the forward
axis points where expected.

### 3.3 Ray–Plane Intersection

**File:** `tests/test_ray_panel_hit.cpp`

| Test Case | Ray Origin | Ray Dir | Panel Y | Expected Hit |
|:----------|:-----------|:--------|:--------|:-------------|
| Straight ahead | (0, 0, 0) | (0, 1, 0) | 49.0 | (0, 49, 0) — centre |
| 15° yaw right | (0, 0, 0) | (sin15°, cos15°, 0) | 49.0 | (x ≈ 13.13, 49, 0) |
| 18° pitch up | (0, 0, 0) | (0, cos18°, sin18°) | 49.0 | (0, 49, z ≈ 15.92) |
| Parallel to panel (D.y = 0) | (0, 0, 0) | (1, 0, 0) | 49.0 | NoHit (0xFF) |
| Behind panel (D.y < 0) | (0, 0, 0) | (0, -1, 0) | 49.0 | NoHit (0xFF) |
| Off-centre origin | (2, 5, 3) | (0, 1, 0) | 49.0 | (2, 49, 3) |

### 3.4 ClassifyRegion Boundary Cases

**File:** `tests/test_classify_region.cpp`

| Test Case | Yaw° | Pitch° | Expected Region |
|:----------|:-----|:-------|:----------------|
| Dead centre | 0.0 | 0.0 | LeftEye (0) |
| Slight right | +1.5 | 0.0 | RightEye (1) |
| Slight left | −1.5 | 0.0 | LeftEye (0) |
| Mouth | 0.0 | −4.0 | Mouth (2) |
| Forehead | 0.0 | +5.0 | Forehead (3) |
| Chin | 0.0 | −8.0 | Chin (4) |
| Torso | 0.0 | −4.0 | Torso (5) — priority: |yaw| < 5 and pitch ∈ (−6, −3) |
| Ground | 0.0 | −15.0 | Ground (8) |
| UL Peripheral | −10.0 | +20.0 | ULPeripheral (9) |
| UR Peripheral | +10.0 | +20.0 | URPeripheral (10) |
| LL Peripheral | −10.0 | −20.0 | LLPeripheral (11) |
| LR Peripheral | +10.0 | −20.0 | LRPeripheral (12) |
| Boundary: pitch = +12 exactly | 0.0 | +12.0 | Forehead (3) — upper CGA requires > 12 |
| Boundary: yaw = −5 exactly | −5.0 | +15.0 | Forehead (3) — UL requires < −5 |
| Boundary: yaw = −5.01 | −5.01 | +15.0 | ULPeripheral (9) |

**Verifies:** every boundary from the Region Map Specification §5.1 is exercised
with exact boundary values.

### 3.5 GazeRegion ↔ SocialTriangle::Vertex Mapping

**File:** `tests/test_region_mapping.cpp`

| Vertex | Expected Region ID |
|:-------|:-------------------|
| `LeftEye` (0) | 0 |
| `RightEye` (1) | 1 |
| `Mouth` (2) | 2 |
| `ThirdEye` (3) | 3 |
| `Chest` (4) | 5 |
| `UpperLeftAversion` (5) | 9 |
| `UpperRightAversion` (6) | 10 |
| `LowerLeftAversion` (7) | 11 |
| `LowerRightAversion` (8) | 12 |

Round-trip test: for each mapping, verify that `ClassifyRegion` at the region's
representative point returns the same ID.

### 3.6 Eyeball Diameter Computation

**File:** `tests/test_eyeball_scale.cpp`

| Test Case | Input | Expected Width (Skyrim units) |
|:----------|:------|:------------------------------|
| Default human, scale 1.0 | 24 mm, scale 1.0 | 1.68 |
| Giant, scale 2.0 | 24 mm, scale 2.0 | 3.36 |
| Child, scale 0.85 | 24 mm, scale 0.85 | 1.428 |
| Override 12 mm (iris only) | 12 mm, scale 1.0 | 0.84 |

### 3.7 Per-Eye Convergence

**File:** `tests/test_convergence.cpp`

| Test Case | IPD (units) | Target Distance | Expected Convergence |
|:----------|:------------|:----------------|:--------------------|
| 1 m target | 4.4 | 70.0 | ~1.8° per eye |
| 0.5 m target | 4.4 | 35.0 | ~3.6° per eye |
| 10 m target | 4.4 | 700.0 | ~0.18° per eye |
| Infinite (parallel) | 4.4 | ∞ | 0° (both arrows parallel) |

---

## 4. Calibration Tests (In-Game)

### 4.1 `stgcal axes` — Axis Verification (A8)

**Procedure:**
1. Enable visuals: `stgv`.
2. Run `stgcal axes`.
3. Screenshot the three coloured lines extending from the player's head bone.

**Pass criteria:**
- Red line extends to the player's **right** (X axis).
- Green line extends **forward** from the face (Y axis).
- Blue line extends **upward** (Z axis).

If any axis is wrong, the head-bone convention needs correction before
proceeding with further calibration.

### 4.2 `stgcal <yaw> <pitch>` — Known-Angle Verification

**Procedure:** For each row in the representative-point table (Region Map §5.3):
1. `stgcal <yaw> <pitch>`.
2. Verify the arrow visually points in the expected direction.
3. Verify the panel highlights the correct region.
4. Verify `stgstatus` reports the matching classified and hit regions.
5. Screenshot for the record.

**Pass criteria:** all 11 active regions match.

### 4.3 `stgcal sweep` — Automated Sweep

**Procedure:**
1. `stgcal sweep`.
2. Wait 22 seconds (11 regions × 2 seconds each).
3. Read the console summary table.

**Pass criteria:**
```
stgcal sweep results:
  Region  0 (LeftEye)       : Classified=0  Hit=0  ✅
  Region  1 (RightEye)      : Classified=1  Hit=1  ✅
  Region  2 (Mouth)         : Classified=2  Hit=2  ✅
  ...
  Region 12 (LRPeripheral)  : Classified=12 Hit=12 ✅
  PASS: 11/11 regions correct.
```

### 4.4 CGA Handedness Verification (A5)

**Procedure:**
1. `stgcal -15 18` (upper-left, gazer's own left).
2. Verify the panel highlights the **Positivity/Hope** region (region 9, cyan).
3. `stgcal 15 18` (upper-right, gazer's own right).
4. Verify the panel highlights the **Memory** region (region 10, bright cyan).

**Pass criteria:** labels match the NLP/CGA convention as defined in Region Map §3.

---

## 5. Integration Test Scenarios

### 5.1 Conversation Scenario

**Setup:** Player approaches an NPC (e.g., Orgnar in Sleeping Giant Inn) and
initiates dialogue with `stgv` on.

**Observations:**
- Arrows originate from the NPC's eye sockets at eyeball scale.
- Arrows change colour as the NPC's gaze moves between social triangle vertices.
- The panel's highlighted region matches the arrow intersection point.
- The trace log shows dwell times consistent with conversation (eyes > 60%,
  mouth ~20–30%, peripheral < 10%).

**Pass criteria:**
- Arrow base visually on the pupil (not forehead, not chin).
- Arrow width approximately matches the eye.
- No arrow going through the NPC's head (backwards).
- Agreement counter ≥ 98% at end of conversation.

### 5.2 CGA Aversion Scenario

**Setup:** Player stands near an NPC in THINK mode (CGA active). `stgv` on.

**Observations:**
- Eyes dart to peripheral regions.
- Arrows follow the eyes, not the head (head barely moves in CGA).
- Panel lights up the correct CGA quadrant (cyan/silver/slate).
- Trace log records aversion entries with correct region IDs.

### 5.3 Creature Scenario

**Setup:** Player faces a dog, wolf, or dragon with `stgv` on.

**Observations:**
- Arrow cross-section scales to the creature's actual eye size (larger for
  dragons, smaller for dogs).
- Panel is visible and oriented correctly.
- No renderer crash.

### 5.4 Distance / LOD Scenario

**Setup:** Player stands at 3 m, 8 m, and 14 m from an NPC with `stgv` on.

**Observations:**
- At 3 m (Tier 1): full visuals, arrows, panel, hit detection all active.
- At 8 m (Tier 2): visuals still render (may be simplified).
- At 14 m: approaching Tier 3 cull. Verify graceful removal, no dangling geometry.

### 5.5 30-Minute Soak Test

**Setup:** Walk through Whiterun with `stgv` on. Enter and exit buildings.
Talk to multiple NPCs. Wait in the tavern.

**Pass criteria:**
- No CTD.
- No `SEH` entries in `TrueGaze.log`.
- Memory stable (no unbounded growth in emitter count — check `stgstatus`
  `ActiveActorCount` stays bounded).
- Agreement counter stays ≥ 98% across the session.

---

## 6. Regression Tests

After each phase of implementation, re-run:

1. All unit tests (`ctest` or equivalent).
2. `stgcal sweep` (must remain 11/11).
3. The conversation scenario with Orgnar (visual sanity check).

Any regression blocks the next phase.

---

## 7. Test Artifacts & Evidence

| Artifact | Location | Purpose |
|:---------|:---------|:--------|
| Unit test source | `tests/test_matrix_scaling.cpp`, etc. | Automated regression |
| `stgcal sweep` log | `TrueGaze.log` | Calibration evidence |
| Trace log (JSONL) | `TrueGaze_GazeTrace.jsonl` | Dwell/accuracy analysis |
| Screenshots | `docs/screengrabs/calibration/` | Visual evidence archive |
| Agreement stats | `stgstatus` console output | Session accuracy report |

---

## 8. Test Implementation Priority

| Priority | Tests | Blocks |
|:---------|:------|:-------|
| **P0** | §3.1 (matrix scaling), §3.4 (ClassifyRegion boundaries) | Phase 1 development |
| **P0** | §4.1 (`stgcal axes`) | All other calibration tests |
| **P1** | §3.2 (AlignBeamOrientation), §3.3 (ray–plane) | Phase 2 development |
| **P1** | §4.2, §4.3 (known-angle, sweep) | Phase 2 acceptance |
| **P2** | §3.5 (mapping), §3.6 (eyeball), §3.7 (convergence) | Phase 1 polish |
| **P2** | §5.1–§5.5 (integration scenarios) | Release candidate |

---

*Approved by Kirk LaSalle, 2026-10-03.*
