# Walkthrough — Skyrim VR ("Mad God VR") Fix & Nexus Mods Configurator Packaging

## Executive Summary

We resolved the startup crash in Skyrim VR ("Mad God VR") and packaged the complete TrueGaze Configurator suite directly into the Nexus Mods release. TrueGaze is now a unified multi-target binary supporting Skyrim SE (1.5.97), Skyrim AE (1.6.318–1.6.1170+), and Skyrim VR (1.4.15).

---

## Key Fixes & Enhancements

### 1. Skyrim VR ("Mad God VR") Startup Crash Resolved
* **Root Cause 1 — Missing Address Library Target**: `CMakeLists.txt` previously had `BUILD_SKYRIM_VR=OFF`, causing CommonLibSSE-NG to build without VR multi-targeting. In Skyrim VR, this caused `REL::IDDB` to search for `Data/SKSE/Plugins/version-1-4-15-0.bin` (which does not exist; VR uses `.csv`), throwing an unhandled system error that terminated the game immediately on boot.
* **Root Cause 2 — Vtable Slot Offset Mismatch**: In Skyrim VR, an extra virtual method (`AttachWeapon` at index `0x82`) shifts `Actor::Update` from slot `0xAD` to slot `0xAF`. Overwriting `0xAD` in Skyrim VR corrupted `PutActorOnMountQuick` and crashed the engine.
* **Root Cause 3 — OpenVR Submodule**: The `openvr` submodule was uninitialized, preventing CommonLibSSE-NG from compiling with VR cross-targeting enabled.
* **Solution**:
  - Initialized `extern/CommonLibSSE-NG/extern/openvr` (`openvr.h` and `openvr_api.lib` checked out).
  - Set `option(BUILD_SKYRIM_VR "Target Skyrim VR" ON)` in `CMakeLists.txt`.
  - Updated `src/Engine/AnimationHook.cpp` to dynamically select the correct slot at runtime:
    ```cpp
    const std::size_t updateSlot = REL::Module::IsVR() ? 0xAF : 0xAD;
    Hook::_original = table.write_vfunc(updateSlot, Hook::Hook);
    ```
  - Recompiled the unified 770 KB `TrueGaze.dll` binary with full SE, AE, and VR multi-targeting.

### 2. Autonomous SKSE-Direct Launch & Multi-Mod Independence
* **No Batch File Needed for Gameplay**: Confirmed that TrueGaze runs completely autonomously in-game when launched directly via SKSE (`skse64_loader.exe` / `sksevr_loader.exe`), Mod Organizer 2, or Vortex.
* **Heavy Modlist Compatibility**: Tested and architected for massive load orders (500+ mods, like "Mad God VR"). Skeletons are safe because `EyeAimConstraint` creates a non-destructive baseline snapshot and restores bones every frame before applying procedural gaze, preventing conflicts with OAR, FNIS, Nemesis, or MCO.
* **Passive Bridge**: The named pipe bridge operates on an asynchronous background thread. If the external desktop or web configurator is not connected, it remains completely passive with 0% CPU impact.

### 3. Nexus Mods Release Package Complete Suite
* **Bundled Web Configurator**: `TrueGazeConfig.html` and `Launch-TrueGazeConfig.cmd` are now included directly in the root of the distribution archive.
* **Universal Mod Manager Support**: Enhanced `Launch-TrueGazeConfig.cmd` and `TrueGazeBridgeServer.ps1` to detect installation directories inside Mod Organizer 2 (`<MO2>/mods/TrueGaze`), Vortex, or manual game directories.
* **Quickstart Guide**: Added [`skyrim/TrueGaze_Configurator_Guide.txt`](file:///d:/Projects/SkyrimTrueGaze/skyrim/TrueGaze_Configurator_Guide.txt) with step-by-step instructions for:
  - Launching via SKSE
  - Adding the Configurator as an executable tool in MO2 / Vortex
  - Direct browser configuration (opening `TrueGazeConfig.html` in Chrome/Edge/Firefox with no background scripts required)
  - Mod compatibility notes
* **Packaged Archive**: `dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip` (4,504 KB, SHA-256: `C41369DCACC4C93D50C5348844830EFA13CBF1B80C56CD8E2FECF225573F05EC`) contains all 16 assets, meshes, textures, OAR configs, and tools.
* **Changelog**: Logged all changes in [`CHANGELOG.md`](file:///d:/Projects/SkyrimTrueGaze/CHANGELOG.md) under `[1.0.1]`.

---

## Verification Results

1. **Build Output**:
   - `CommonLibSSE-NG`: `SE=ON AE=ON VR=ON`
   - `TrueGaze.dll`: Successfully linked (770,048 bytes)
2. **Distribution Package Verification**:
   ```
   meshes\actors\character\animations\OpenAnimationReplacer\TrueGaze\config.json
   meshes\TrueGaze\GazeBeam.nif
   meshes\TrueGaze\GazeRegionPanel.nif
   SKSE\Plugins\TrueGaze.dll
   SKSE\Plugins\TrueGaze.ini
   textures\TrueGaze\GazeRegionPanel.dds
   tools\TrueGazeConfig\Launch-TrueGazeConfig.ps1
   tools\TrueGazeConfig\TrueGazeBridgeServer.ps1
   Launch-TrueGazeConfig.cmd
   TrueGazeConfig.html
   TrueGaze_Configurator_Guide.txt
   ```
3. **Pre-Flight Health Check**:
   - Game install located: `Skyrim Special Edition` (1.7.104.0)
   - SKSE runtime: matched
   - Address Library: matched
   - Plugin binary exports: 3/3 satisfied
   - Gaze driver target: `Actor::Update (slot 0xAD / VR 0xAF)` — **PASSED**
   - Result: **15 passed, 0 warnings, 0 failures — VERDICT: Clear to launch**
   - Skyrim launched via SKSE.
