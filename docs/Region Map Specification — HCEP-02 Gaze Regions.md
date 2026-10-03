# Region Map Specification — HCEP-02 Gaze Regions

**Project:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine  
**Author:** Antigravity (Engineering), Kirk LaSalle (Director & Approver)  
**Date:** 2026-10-03  
**Status:** ✅ Approved — Ready for Development  
**Companion:** [Technical Design — Arrow & Panel Calibration](Technical%20Design%20%E2%80%94%20Gaze%20Arrow%20&%20HCEP%20Panel%20Calibration%20and%20Hit%20Detection.md)

---

## 1. Purpose

This document is the **single source of truth** for TrueGaze's gaze region
system. It defines:

- The unified `GazeRegion` enum shared by every subsystem.
- The angular boundaries that `ClassifyRegion` uses.
- The projection formula that places each region on the HCEP panel.
- The mapping from `SocialTriangle::Vertex` to `GazeRegion`.

Any change to a region boundary, a region ID, or the panel layout **must** start
here. Code, art, and tests are all derived from this specification.

---

## 2. Coordinate Convention

| Axis | Positive Direction | Zero |
|:-----|:-------------------|:-----|
| **Yaw** | Gazer's own right | Dead ahead (actor forward) |
| **Pitch** | Up | Horizontal plane |

All angles are **body-relative total deflection** — the full angle from the
actor's forward vector to the gaze target, regardless of how the deflection is
distributed across spine, neck, head, and eyes.

---

## 3. Handedness

**Gazer's own left = negative yaw.**

The HCEP-02 diagram is drawn from the **observer's** point of view (facing the
subject). In that diagram, "Upper region (Positivity)" appears on the
observer's left, which is the **gazer's right**. NLP/CGA literature defines
aversion quadrants from the **subject's own perspective**, and the classifier
(`ClassifyRegion`) uses the gazer's body-relative yaw. Therefore:

| Diagram Label (observer's view) | Gazer's Own Side | Yaw Sign | Region ID |
|:-------------------------------|:-----------------|:---------|:----------|
| Upper region (Positivity) | Gazer's **left** | yaw < −5° | 9 |
| Far Upper regions (Memory) | Gazer's **right** | yaw > +5° | 10 |
| Lower region (Tiredness) | Gazer's **left** | yaw < −5° | 11 |
| Far Lower regions (Shyness) | Gazer's **right** | yaw > +5° | 12 |

> [!IMPORTANT]
> The panel art must be re-authored or flipped so that negative yaw (gazer's
> left) corresponds to the Positivity quadrant. The current panel texture has
> this mirrored because it was placed with a 180° Y-flip to face the camera.

---

## 4. Unified GazeRegion Enum

This enum replaces both the raw `uint8_t gazeRegion` field and the
`SocialTriangle::Vertex` enum for cross-subsystem communication.

```cpp
// src/Engine/GazeRegion.hpp
#pragma once
#include <cstdint>

namespace TrueGaze
{
    /// Unified gaze region IDs from the HCEP-02 Enhanced Diagram.
    /// Used by: ClassifyRegion, VisualEffectsManager, OarConditions,
    ///          TrueGazeAPI, SocialTriangle, trace logging.
    enum class GazeRegion : uint8_t
    {
        LeftEye             = 0,   // Social triangle vertex
        RightEye            = 1,   // Social triangle vertex
        Mouth               = 2,   // Social triangle vertex
        Forehead            = 3,   // Third-Eye / spiritual focus
        Chin                = 4,   // Below-face transition
        Torso               = 5,   // Chest / heart / sternum (HEART mode)
        RightHand           = 6,   // Reserved — not yet classified
        LeftHand            = 7,   // Reserved — not yet classified
        Ground              = 8,   // Floor / shame / submission
        ULPeripheral        = 9,   // CGA: positivity, happiness, hope
        URPeripheral        = 10,  // CGA: memory, constructive thought
        LLPeripheral        = 11,  // CGA: tiredness, negativity, sadness
        LRPeripheral        = 12,  // CGA: shyness, fear, deception

        NoHit               = 0xFF // Ray missed the panel (behind/parallel)
    };
}
```

### 4.1 SocialTriangle::Vertex Mapping

| `SocialTriangle::Vertex` | Value | `GazeRegion` | Value |
|:--------------------------|:------|:-------------|:------|
| `LeftEye` | 0 | `LeftEye` | 0 |
| `RightEye` | 1 | `RightEye` | 1 |
| `Mouth` | 2 | `Mouth` | 2 |
| `ThirdEye` | 3 | `Forehead` | 3 |
| `Chest` | 4 | `Torso` | 5 |
| `UpperLeftAversion` | 5 | `ULPeripheral` | 9 |
| `UpperRightAversion` | 6 | `URPeripheral` | 10 |
| `LowerLeftAversion` | 7 | `LLPeripheral` | 11 |
| `LowerRightAversion` | 8 | `LRPeripheral` | 12 |

A static `constexpr` mapping function `VertexToRegion(Vertex v)` will be added
to `SocialTriangle` or `GazeRegion.hpp`.

---

## 5. Angular Boundary Definitions

These boundaries define the **classifier's decision function**. They are
evaluated in priority order (top to bottom). The first matching rule wins.

### 5.1 Priority Order

```
1. CGA Upper    pitch > +12°
     yaw < −5°  → ULPeripheral (9)
     yaw > +5°  → URPeripheral (10)
     else       → Forehead (3)

2. CGA Lower    pitch < −12°
     yaw < −5°  → LLPeripheral (11)
     yaw > +5°  → LRPeripheral (12)
     else       → Ground (8)

3. Chin         pitch ∈ [−12, −6)
     → Chin (4)

4. Torso        pitch ∈ [−6, −3)  AND  |yaw| < 5°
     → Torso (5)

5. Face         |pitch| ≤ 3°
     yaw < −1°  → LeftEye (0)
     yaw > +1°  → RightEye (1)
     yaw ∈ [−1, +1]  → LeftEye (0)   // default to left eye

6. Mouth        pitch ∈ (−6, 0)
     → Mouth (2)

7. Forehead     pitch ∈ (0, +12]
     → Forehead (3)
```

> [!NOTE]
> Rule 5 defaults ambiguous centre-gaze to LeftEye (region 0). This matches the
> current `ClassifyRegion` behaviour and the biological observation that humans
> default to their interlocutor's left eye (which is on the gazer's right, but
> since left/right is relative to the partner, the classifier maps it to
> `LeftEye`).

### 5.2 Boundary Summary Table

| Region | ID | Yaw Min (°) | Yaw Max (°) | Pitch Min (°) | Pitch Max (°) |
|:-------|:---|:------------|:------------|:--------------|:--------------|
| LeftEye | 0 | −∞ | −1 | −3 | +3 |
| RightEye | 1 | +1 | +∞ | −3 | +3 |
| Mouth | 2 | −5 | +5 | −6 | 0 |
| Forehead | 3 | −5 | +5 | 0 | +12 |
| Chin | 4 | −∞ | +∞ | −12 | −6 |
| Torso | 5 | −5 | +5 | −6 | −3 |
| Ground | 8 | −5 | +5 | −∞ | −12 |
| UL Peripheral | 9 | −∞ | −5 | +12 | +∞ |
| UR Peripheral | 10 | +5 | +∞ | +12 | +∞ |
| LL Peripheral | 11 | −∞ | −5 | −∞ | −12 |
| LR Peripheral | 12 | +5 | +∞ | −∞ | −12 |

### 5.3 Region Representative Points

Each region has a single representative point used for:
- Panel art: the label/icon centre.
- `stgcal sweep`: the calibration target angle.
- Trace log region-centre references.

| Region | ID | Rep. Yaw (°) | Rep. Pitch (°) |
|:-------|:---|:-------------|:----------------|
| LeftEye | 0 | −1.8 | +0.5 |
| RightEye | 1 | +1.8 | +0.5 |
| Mouth | 2 | 0.0 | −4.5 |
| Forehead | 3 | 0.0 | +7.0 |
| Chin | 4 | 0.0 | −9.0 |
| Torso | 5 | 0.0 | −4.5 |
| Ground | 8 | 0.0 | −18.0 |
| UL Peripheral | 9 | −15.0 | +18.0 |
| UR Peripheral | 10 | +15.0 | +18.0 |
| LL Peripheral | 11 | −15.0 | −18.0 |
| LR Peripheral | 12 | +15.0 | −18.0 |

---

## 6. Panel Projection

### 6.1 Formula

Given the panel plane at distance `d` Skyrim units forward of the eye anchor,
an angular point `(yaw°, pitch°)` projects to panel-local coordinates:

```
x_panel = d × tan(yaw × π / 180)
z_panel = d × tan(pitch × π / 180) / cos(yaw × π / 180)
```

### 6.2 Example (Default Configuration)

With `d = 49.0` Skyrim units (`hcepPanelForwardOffsetCm = 70.0`, 70 / 100 × 70):

| Region | Yaw° | Pitch° | Panel X (units) | Panel Z (units) |
|:-------|:-----|:-------|:----------------|:----------------|
| LeftEye | −1.8 | +0.5 | −1.54 | +0.43 |
| RightEye | +1.8 | +0.5 | +1.54 | +0.43 |
| Mouth | 0.0 | −4.5 | 0.00 | −3.86 |
| Forehead | 0.0 | +7.0 | 0.00 | +6.02 |
| UL Peripheral | −15.0 | +18.0 | −13.13 | +16.85 |
| UR Peripheral | +15.0 | +18.0 | +13.13 | +16.85 |
| LL Peripheral | −15.0 | −18.0 | −13.13 | −16.85 |
| LR Peripheral | +15.0 | −18.0 | +13.13 | −16.85 |

### 6.3 Texture Generation

The `GenerateHcepPanelTexture.py` script will be updated to:

1. Read the region table from a shared JSON file (`config/gaze_regions.json`).
2. Project each region's representative point and boundary polygon onto a
   texture canvas using the formula above.
3. Draw labelled circles/shapes at the projected positions with the region's
   assigned colour.
4. Export the chroma-keyed DDS as before.

This ensures the texture is **derived from the spec**, not hand-drawn. Any
boundary change in the JSON propagates to the texture, the classifier, and the
hit detector simultaneously.

---

## 7. Region Colours

Each region has an assigned diagnostic colour used by:
- Beam tinting (arrow colour changes per region).
- Panel region highlight.
- Trace log visualisation.

| Region | ID | Colour | Hex | R | G | B |
|:-------|:---|:-------|:----|:--|:--|:--|
| LeftEye | 0 | Green | `#33FF59` | 0.20 | 1.00 | 0.35 |
| RightEye | 1 | Orange | `#FF9926` | 1.00 | 0.60 | 0.15 |
| Mouth | 2 | Pink | `#FF59BF` | 1.00 | 0.35 | 0.75 |
| Forehead | 3 | Purple | `#D966FF` | 0.85 | 0.40 | 1.00 |
| Chin | 4 | Warm Tan | `#E6BF99` | 0.90 | 0.75 | 0.60 |
| Torso | 5 | Crimson | `#FF2633` | 1.00 | 0.15 | 0.20 |
| Ground | 8 | Blue | `#2659E6` | 0.15 | 0.35 | 0.90 |
| UL Peripheral | 9 | Cyan | `#33CCFF` | 0.20 | 0.80 | 1.00 |
| UR Peripheral | 10 | Bright Cyan | `#66F2FF` | 0.40 | 0.95 | 1.00 |
| LL Peripheral | 11 | Silver | `#A6B3CC` | 0.65 | 0.70 | 0.80 |
| LR Peripheral | 12 | Slate | `#8080A6` | 0.50 | 0.50 | 0.65 |

---

## 8. Data File: `config/gaze_regions.json`

A machine-readable version of this specification, consumed by:
- `GenerateHcepPanelTexture.py` (panel art).
- Unit tests (boundary verification).
- Future tooling (Configurator region editor).

```json
{
  "coordinate_convention": {
    "yaw_positive": "gazer_right",
    "pitch_positive": "up",
    "frame": "body_relative_total_deflection"
  },
  "regions": [
    {
      "id": 0, "name": "LeftEye", "label": "Left Eye",
      "cognitive": "Logic, reason",
      "rep_yaw": -1.8, "rep_pitch": 0.5,
      "colour": [0.20, 1.00, 0.35]
    },
    {
      "id": 1, "name": "RightEye", "label": "Right Eye",
      "cognitive": "Creativity, emotion",
      "rep_yaw": 1.8, "rep_pitch": 0.5,
      "colour": [1.00, 0.60, 0.15]
    }
  ],
  "_comment": "Full region list follows the same structure for all 11 active regions."
}
```

The full JSON will be generated during Phase 4 implementation.

---

## 9. Public API Contract

The `gazeRegion` field in `TrueGazeAPI.h::ActorGazeTelemetry` and the
`OarConditions::QueryGazeRegionPayload::targetRegionId` use the IDs defined in
this specification. The enum values are **ABI-stable** — existing IDs will never
be renumbered.

New regions (6, 7) may be activated in future versions. Consumers should treat
any ID outside 0–12 as "unknown" and any ID of 6 or 7 as "reserved / not yet
active".

---

*Approved by Kirk LaSalle, 2026-10-03.*
