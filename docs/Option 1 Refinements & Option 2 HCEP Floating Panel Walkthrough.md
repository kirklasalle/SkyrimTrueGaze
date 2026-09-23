# Option 1 Refinements & Option 2 HCEP Floating Panel Walkthrough

## Summary of Accomplishments

We have successfully implemented and verified both visual systems for TrueGaze:
1. **Option 1: Beam Refinements ("Superman Laser Eyes")**
   - **Pencil-Thin Laser Rays**: Implemented `BeamTransform(forward, radius, length)` encoding non-uniform scale directly into the rotation matrix basis columns. Any mesh (custom `GazeBeam.nif` or fallback `marker_arrow.nif`) is scaled down to a thin ~8mm beam rather than a giant slab.
   - **Precise Pupil Origin**: Updated pupil offsets from `7.0cm / 1.5cm` to `12.0cm forward` and `6.0cm up` from the `NPC Head [Head]` bone, seating the beam origin in the eye sockets.
   - **Eye-Gaze Tracking**: Beams track the eye gaze residual direction relative to the head.

2. **Option 2: HCEP Floating Diagram Panel**
   - **Chroma-Keyed Alpha Transparency**: Converted `docs/images/hcep-02_enhanced-diagram_keyed-01.jfif` via a new texture pipeline (`GenerateHcepPanelTexture.py`), cleanly keying out the white background into transparent alpha. Output to `textures/TrueGaze/GazeRegionPanel.dds` (uncompressed 32-bit RGBA8 DDS) and `docs/images/hcep-02_enhanced-diagram_keyed-01.png`.
   - **3D Floating Quad Mesh**: Authored `GazeRegionPanel.nif` via `GenerateGazeRegionPanelNif.py` in the XZ plane with `BSEffectShaderProperty` self-illumination and `NiAlphaProperty` smooth blending (`0x10ED`).
   - **All Actors Supported**: Enabled for Player, NPCs, and Creatures alike.
   - **Head-Anchored**: Floats ~35cm in front of the head bone and rotates with the head so the eye beams project through the floating regions.
   - **Dynamic Region Highlighting**: When the actor glances into different gaze regions (Third-eye, Upper region, Right eye, Left eye, Mouth, Chest, Far Upper, Far Lower, Lower region), the panel's shader base color dynamically shifts to highlight the active region.

3. **Deployment Pipeline Enhancement**
   - Updated `scripts/Deploy-TrueGaze.ps1` to automatically deploy `skyrim/meshes/TrueGaze/*` and `skyrim/textures/TrueGaze/*` directly into Skyrim's `Data/` folder on every run.

---

## Detailed Changes

### C++ Code
| File | Changes |
| --- | --- |
| [`VisualTuning.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualTuning.hpp) | Added `ThicknessUnits()`, updated `pupilForwardOffsetCm` (12.0f) and `pupilUpOffsetCm` (6.0f), added HCEP panel tuning (`showHcepPanel`, `hcepPanelAllActors`, `hcepPanelScale`, `hcepPanelForwardOffsetCm`). |
| [`ConfigManager.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ConfigManager.hpp) | Declared HCEP panel config fields and updated default pupil offsets. |
| [`ConfigManager.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/ConfigManager.cpp) | Handled INI reading, sanitization clamping, and saving for all new visual keys under `[Visuals]`. |
| [`GazeEngine.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Engine/GazeEngine.cpp) | Wired config values into `VisualEffectsManager::Get().SetTuning(vis)`. |
| [`VisualEffectsManager.hpp`](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualEffectsManager.hpp) | Added `hcepPanel`, `lastGazeRegion` to `ActorEmitters`, declared `EnsureHcepPanel()`, `UpdateHcepPanel()`, `DetachPanel()`. |
| [`VisualEffectsManager.cpp`](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualEffectsManager.cpp) | Replaced `BeamRotation` with `BeamTransform(forward, radius, length)` for non-uniform scaling; implemented HCEP panel placement, head-anchoring, and dynamic shader region tinting. |

### Asset & Script Generators
| File | Purpose |
| --- | --- |
| [`GenerateHcepPanelTexture.py`](file:///d:/Projects/SkyrimTrueGaze/scripts/GenerateHcepPanelTexture.py) | Chroma-keys white background from `hcep-02_enhanced-diagram_keyed-01.jfif` and outputs PNG + 32-bit RGBA8 DDS (`GazeRegionPanel.dds`). |
| [`GenerateGazeRegionPanelNif.py`](file:///d:/Projects/SkyrimTrueGaze/scripts/GenerateGazeRegionPanelNif.py) | Generates `GazeRegionPanel.nif` quad mesh with `BSEffectShaderProperty` and `NiAlphaProperty`. |
| [`GenerateGazeBeamNif.py`](file:///d:/Projects/SkyrimTrueGaze/scripts/GenerateGazeBeamNif.py) | Generates unit-dimension beam cylinder `GazeBeam.nif`. |
| [`Deploy-TrueGaze.ps1`](file:///d:/Projects/SkyrimTrueGaze/scripts/Deploy-TrueGaze.ps1) | Deploys DLL, INI, custom meshes, and textures to game `Data/`. |

---

## Verification & Validation

### 1. Texture Generation
- Input: `docs/images/hcep-02_enhanced-diagram_keyed-01.jfif` (2760x1504)
- Ran `python scripts/GenerateHcepPanelTexture.py`
- Result: Cleanly generated `docs/images/hcep-02_enhanced-diagram_keyed-01.png` and `skyrim/textures/TrueGaze/GazeRegionPanel.dds` with pure transparent background and zero white border fringe.

### 2. Mesh Generation
- Ran `python scripts/GenerateGazeRegionPanelNif.py` and `python scripts/GenerateGazeBeamNif.py`
- Result: Valid Skyrim SE 20.2.0.7 NIF files generated in `skyrim/meshes/TrueGaze/` and `build/TrueGaze/meshes/TrueGaze/`.

### 3. Solution Compilation
- Ran: `cmake --build build/windows-release --config Release`
- Output: `TrueGaze.vcxproj -> TrueGaze.dll` built successfully with 0 errors.

### 4. Unit Test Suite
- Ran: `bin/Release/KinematicsTests.exe`
- Result: **ALL 11 BIOMECHANICAL KINEMATICS TESTS PASSED!**

### 5. Deployment Verification
- Ran: `powershell -ExecutionPolicy Bypass -File scripts/Deploy-TrueGaze.ps1 -NoBuild -NoLaunch`
- Output:
  - `TrueGaze.dll` deployed
  - `TrueGaze meshes -> Data\meshes\TrueGaze (3 files)` deployed
  - `TrueGaze textures -> Data\textures\TrueGaze (1 files)` deployed
  - 15 health checks passed, 0 warnings, 0 failures. Clear to launch!

---

## How to Test In-Game

1. In your `Data/SKSE/Plugins/TrueGaze.ini` (or using the in-game console commands), set:
   ```ini
   [Visuals]
   bEnableInGameVisuals=true
   bGazeRaysEnabled=true
   bShowHcepPanel=true
   bHcepPanelAllActors=true
   ```
2. Launch Skyrim using `skse64_loader.exe`.
3. In third-person or around any NPC / creature:
   - Thin laser beams will project from their pupils tracking their gaze.
   - The HCEP diagram panel will float comfortably in front of their head with a completely transparent background.
   - The panel tracks their head orientation while eye beams move dynamically across the panel regions.
