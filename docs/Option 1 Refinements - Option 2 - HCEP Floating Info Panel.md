# Option 1 Refinements + Option 2: HCEP Floating Info Panel

## Option 1: Remaining Beam Issues

### Problem Analysis from Screenshots 14–17

The screenshots confirm the beams ARE rendering and oriented correctly (BeamRotation fix is working), but three issues remain:

| Issue | Visible Symptom | Root Cause |
|-------|----------------|------------|
| **Size** | Beams are massive pink rectangles, not thin laser lines | The custom GazeBeam.nif isn't loading (likely NIF format issue); the fallback `marker_arrow.nif` (~240 units long, ~30 units wide) is rendered at `scale = 1.0` |
| **Origin** | Beams originate from inside the head / behind the face | `pupilForwardOffsetCm = 7.0` and `pupilUpOffsetCm = 1.5` are too small for Skyrim's head bone (which sits at the base of the skull/upper neck) |
| **Movement** | Beams appear to pivot from the head center, not the eyes | Consequence of wrong origin — when origin is deep in the head, the beam appears head-anchored even though the direction is eye-driven |

### Proposed Fixes

#### Fix 1: Non-uniform beam scaling via rotation matrix

Skyrim's `NiTransform` only supports **uniform** `local.scale`. To get a **thin + long** beam from any mesh shape, we encode non-uniform scale directly into the `NiMatrix3` rotation columns:

```
X column = right * beamRadius    → controls beam width (thin)
Y column = forward * beamLength  → controls beam reach (long)  
Z column = up * beamRadius       → controls beam height (thin)
```

This works regardless of which mesh loaded (custom GazeBeam.nif or fallback marker_arrow.nif).

**[MODIFY] [VisualEffectsManager.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualEffectsManager.cpp)**

Rewrite `BeamRotation()` → `BeamTransform()` to accept cross-section radius and length:

```cpp
RE::NiMatrix3 BeamTransform(const RE::NiPoint3& forward,
                            float crossSectionRadius,
                            float beamLength) noexcept
{
    RE::NiPoint3 fwd = forward;
    (void)fwd.Unitize();
    RE::NiPoint3 refUp{0,0,1};
    if (std::abs(fwd.z) > 0.98f) refUp = {0,1,0};
    RE::NiPoint3 right = Cross(fwd, refUp);
    (void)right.Unitize();
    RE::NiPoint3 up = Cross(right, fwd);
    (void)up.Unitize();
    // Non-uniform scale baked into the basis columns:
    // - X,Z scaled to crossSectionRadius for pencil-thin width
    // - Y scaled to beamLength for the configured reach
    return RE::NiMatrix3(right * crossSectionRadius,
                         fwd * beamLength,
                         up * crossSectionRadius);
}
```

Update the geometry block to use `BeamTransform()`:

```cpp
emitters.geometry->local.rotate = BeamTransform(
    localDirection,
    _tuning.ThicknessUnits(),   // ~0.56 Skyrim units for 0.8cm
    _tuning.LengthUnits()       // 700 units for 10m
);
emitters.geometry->local.scale = 1.0f;
```

The mesh (whether marker_arrow or GazeBeam) is treated as a **unit-dimension template** — the rotation matrix stretches it to the desired proportions.

---

#### Fix 2: Better pupil origin offsets

Skyrim's `NPC Head [Head]` bone is at the base of the skull / upper neck. The eye sockets are approximately:
- **12cm forward** from the head bone (not 7cm)
- **6cm up** (not 1.5cm)

**[MODIFY] [VisualTuning.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualTuning.hpp)**

Update defaults:
```diff
-        float pupilForwardOffsetCm{7.0f};
-        float pupilUpOffsetCm{1.5f};
+        float pupilForwardOffsetCm{12.0f};
+        float pupilUpOffsetCm{6.0f};
```

**[MODIFY] [ConfigManager.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ConfigManager.hpp)**

Same default updates in the config.

---

#### Fix 3: Add `ThicknessUnits()` helper

**[MODIFY] [VisualTuning.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualTuning.hpp)**

Add a conversion helper so the beam width works in Skyrim units:

```cpp
/// Beam cross-section radius in Skyrim units.
[[nodiscard]] constexpr float ThicknessUnits() const noexcept
{
    return gazeRayThicknessCm / kCmPerMeter * kUnitsPerMeter * 0.5f; // radius, not diameter
}
```

---

#### Fix 4: Regenerate `GazeBeam.nif` as unit-dimension mesh

**[MODIFY] [GenerateGazeBeamNif.py](file:///d:/Projects/SkyrimTrueGaze/scripts/GenerateGazeBeamNif.py)**

Change to unit dimensions: `radius=1.0, length=1.0`. The runtime non-uniform scaling via `BeamTransform()` handles all sizing. This also removes the dependency on having the correct length baked into the NIF.

---

### Expected Result After Option 1 Fixes

- **Thin pencil beams** (~8mm cross-section) projecting 10m from each NPC's eye area
- **Beams start at the eye sockets**, not from inside the head
- **No change needed** if custom NIF fails — the fallback `marker_arrow.nif` will also render as a thin beam thanks to the non-uniform scaling in the rotation matrix

---

## Option 2: HCEP Floating Info Panel

### Architecture

A head-anchored textured quad floating ~40cm in front of each NPC's face, showing the HCEP gaze-region diagram with the currently-active region highlighted.

### Answers to Open Questions (from Kirk)

| Question | Answer |
|----------|--------|
| Billboard vs head-anchored? | **Head-anchored** — tracks head orientation, eyes move freely |
| Active region highlighting? | **Yes** — glow the active gaze region |
| Panel size? | **50cm × 30cm** (~35×21 Skyrim units), can scale up |
| Which actors? | **Player-only enable**, visible to all |

### Proposed Changes

#### [NEW] `GazeRegionPanel.nif` — Panel quad mesh

A flat quad (2 triangles) authored in the XZ plane, unit dimensions (1×1). Runtime scaling via `local.scale` handles the actual size. Uses `BSEffectShaderProperty` with a texture slot for the HCEP diagram.

**Generated by:** [NEW] [GenerateGazeRegionPanelNif.py](file:///d:/Projects/SkyrimTrueGaze/scripts/GenerateGazeRegionPanelNif.py)

---

#### [NEW] `GazeRegionPanel.dds` — HCEP diagram texture

The HCEP-02 diagram (without the person — just labels, circles, and arrows on a transparent/semi-transparent background) converted to DDS format for in-game rendering.

> [!IMPORTANT]
> DDS texture creation requires converting the HCEP diagram PNG to DDS (DXT5 with alpha). I'll create a script that converts the diagram to a clean overlay version and exports it as DDS using Python's Pillow + a DDS encoder.

---

#### [MODIFY] [VisualEffectsManager.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualEffectsManager.hpp)

Add panel fields to `ActorEmitters`:

```cpp
RE::NiPointer<RE::NiAVObject> hcepPanel{};  // The HCEP region quad
uint8_t lastGazeRegion{0xFF};               // Last highlighted region
```

Add panel lifecycle methods:

```cpp
void EnsureHcepPanel(ActorEmitters& a_emitters, RE::NiAVObject* a_anchor) noexcept;
void UpdateHcepPanel(ActorEmitters& a_emitters, RE::NiAVObject* a_headBone,
                     uint8_t a_gazeRegion) noexcept;
```

---

#### [MODIFY] [VisualEffectsManager.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualEffectsManager.cpp)

In `UpdateActor()`:
1. Call `EnsureHcepPanel()` when `_tuning.showHcepPanel` is enabled and the actor is the player
2. Call `UpdateHcepPanel()` to position the panel in front of the head and highlight the current gaze region

Panel positioning:
```cpp
// Panel is head-anchored, offset forward by ~40cm
const float panelOffset = 28.0f; // ~40cm in Skyrim units
panel->local.translate = localHead + headForward * panelOffset;
panel->local.rotate = headBone->world.rotate; // anchored to head orientation
panel->local.scale = _tuning.hcepPanelScale;  // ~35 units for 50cm width
```

Region highlighting: Change the panel's `BSEffectShaderProperty` emissive colour based on `a_gazeRegion`:

| Region | Colour |
|--------|--------|
| Third-eye | Purple (spiritual) |
| Upper region | Blue (positivity) |
| Right eye | Orange (creativity) |
| Left eye | Green (logic) |
| Mouth | Pink (connection) |
| Chest/heart | Red (love) |
| Far Upper | Cyan (memories) |
| Far Lower | Grey (shyness) |
| Lower region | Dark blue (tiredness) |

---

#### [MODIFY] [VisualTuning.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualTuning.hpp)

Add panel config:

```cpp
bool showHcepPanel{false};            // master switch for the HCEP diagram panel
float hcepPanelScale{0.5f};           // panel uniform scale (50cm width at 0.5)
float hcepPanelForwardOffsetCm{40.0f}; // how far in front of the head
const char* hcepPanelModelPath{"meshes\\TrueGaze\\GazeRegionPanel.nif"};
```

---

#### [MODIFY] [ConfigManager.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ConfigManager.hpp) + [ConfigManager.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ConfigManager.cpp)

Add INI keys in `[Visuals]`:

```ini
bShowHcepPanel=0
fHcepPanelScale=0.5
fHcepPanelForwardOffsetCm=40.0
```

---

#### [MODIFY] [GazeEngine.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp)

Wire panel config through `RefreshTuning`.

---

### Summary of All Changes

| File | Option | Change |
|------|--------|--------|
| `VisualEffectsManager.cpp` | 1 | `BeamRotation` → `BeamTransform` with non-uniform scale |
| `VisualEffectsManager.cpp` | 2 | `EnsureHcepPanel` + `UpdateHcepPanel` methods |
| `VisualEffectsManager.hpp` | 2 | Panel fields in `ActorEmitters`, new methods |
| `VisualTuning.hpp` | 1+2 | Fix pupil offsets, add `ThicknessUnits()`, panel config |
| `ConfigManager.hpp/.cpp` | 1+2 | Fix offset defaults, add panel INI keys |
| `GazeEngine.cpp` | 2 | Wire panel tuning |
| `GenerateGazeBeamNif.py` | 1 | Unit-dimension mesh |
| `GenerateGazeRegionPanelNif.py` [NEW] | 2 | Panel quad NIF generator |
| `GazeRegionPanel.nif` [NEW] | 2 | Generated panel mesh |

---

## Verification Plan

### Build
- `cmake --build build/windows-release --config Release`

### Manual Verification
1. Deploy via `scripts/Deploy-TrueGaze.ps1`
2. Enable visuals + HCEP panel in TrueGaze.ini
3. Verify in tavern scene:
   - Beams are thin laser lines from NPC eye sockets
   - Beams track eye direction, not just head direction
   - HCEP panel floats in front of the player's face
   - Active region highlights when gaze direction changes
   - Panel tracks head orientation, beams move independently
