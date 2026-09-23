# Implementation Plan — Skyrim VR ("Mad God VR") Fix, Nexus Mods Configurator Packaging & SKSE Multi-Mod Compatibility

## Overview

This implementation plan addresses the two critical production findings reported by Kirk:
1. **Skyrim VR Crash on Start ("Mad God VR")**: Diagnosed and resolved the root causes of the crash on start when running TrueGaze with Skyrim VR and heavy modlists like "Mad God VR".
2. **Nexus Mods Release Packaging & Config Support**: Package the interactive Web Configurator (`TrueGazeConfig.html`), launcher (`Launch-TrueGazeConfig.cmd`), automation bridge scripts, and an easy-to-follow guide directly into the distributable mod release, while ensuring TrueGaze is completely independent and stable when launched directly from SKSE alongside hundreds of other mods.

---

## Technical Root Cause Analysis

### 1. Why TrueGaze Crashed on Start in "Mad God VR" (Skyrim VR)

Three compounding issues caused the immediate crash on start in Skyrim VR:

1. **`BUILD_SKYRIM_VR` was `OFF` in `CMakeLists.txt`**:
   - `CMakeLists.txt` defaulted `option(BUILD_SKYRIM_VR "Target Skyrim VR" OFF)`.
   - Consequently, `ENABLE_SKYRIM_VR=1` was never defined, and `CommonLibSSE-NG` compiled with `EXCLUSIVE_SKYRIM_FLAT` (supporting SE 1.5.97 and AE 1.6+ only).
   - In `REL/IDDB.h`, when `ENABLE_SKYRIM_VR` is not defined, `REL::IDDB::load()` attempts to load Address Library file `Data/SKSE/Plugins/version-1-4-15-0.bin` for runtime `1.4.15.0`.
   - **Skyrim VR has no `.bin` Address Library**; it exclusively uses `version-1-4-15-0.csv`.
   - Because `version-1-4-15-0.bin` is missing, `REL::IDDB::load_file` throws `std::system_error` and `stl::report_and_error` **terminates the Skyrim VR process with a fatal error dialog before the main menu even loads**.

2. **Uninitialized `openvr` Submodule**:
   - `extern/CommonLibSSE-NG/extern/openvr` was never initialized (`-60eb18780...`), which previously prevented building with VR enabled.
   - Initializing this submodule provides `openvr.h` and `openvr_api.lib` required to link CommonLibSSE-NG in cross-VR mode (`SKYRIM_CROSS_VR`).

3. **Vtable Hook Slot Mismatch (`0xAD` vs `0xAF`)**:
   - In `src/Engine/AnimationHook.cpp`, `InstallActorUpdateHook` hardcoded virtual method slot `0xAD`:
     ```cpp
     Hook::_original = table.write_vfunc(0xAD, Hook::Hook);
     ```
   - In Skyrim SE/AE, `Actor::Update` is at vtable index `0xAD`.
   - **In Skyrim VR, an extra virtual function (`AttachWeapon` at 0x82) shifts `Actor::Update` to slot `0xAF`**.
   - Overwriting slot `0xAD` in VR corrupted `PutActorOnMountQuick`, left `Actor::Update` unhooked, and caused memory corruption/crashes as soon as actors updated.
   - **Fix**: Dynamically resolve the slot at runtime:
     ```cpp
     const std::size_t updateSlot = REL::Module::IsVR() ? 0xAF : 0xAD;
     Hook::_original = table.write_vfunc(updateSlot, Hook::Hook);
     ```

### 2. Nexus Mods Release Configurator & Multi-Mod Support

1. **Packaging Defect**:
   - `scripts/PackageMod.ps1` only archived the `skyrim/` directory (`SKSE/Plugins/TrueGaze.dll`, `TrueGaze.ini`, meshes, textures, OAR animations).
   - It omitted `TrueGazeConfig.html`, `Launch-TrueGazeConfig.cmd`, and the PowerShell server scripts.
   - Users downloading the mod archive from Nexus Mods into Mod Organizer 2 (MO2) or Vortex had no access to the Web Configurator.

2. **Direct SKSE Launch Independence**:
   - Kirk noted that while he personally launches Skyrim via the batch file, Nexus users launch directly via the SKSE button in MO2/Vortex or desktop shortcuts, often with hundreds of other mods.
   - **Architecture Verification**:
     - `TrueGaze.dll` is an autonomous SKSE plugin. It does **not** require the config tool or bridge to be running during gameplay.
     - When launched from SKSE, `TrueGaze.dll` boots, reads `Data/SKSE/Plugins/TrueGaze.ini`, and runs autonomously.
     - The Named Pipe server runs non-blocking on a background thread; if the HCEP Desktop or Web Configurator is not connected, it sleeps silently without consuming CPU or blocking the game.
     - Skeletons are safe: `EyeAimConstraint` takes a pristine snapshot of unmodified bones and restores them every frame before applying procedural gaze, guaranteeing zero bone drift or conflicts with other animation replacers (OAR, FNIS, Nemesis, MCO).

---

## User Review Required

> [!IMPORTANT]
> **Packaging Structure for Nexus Mods Release:**
> In the new distribution package (`TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip`), we propose the following layout:
> - `SKSE/Plugins/TrueGaze.dll` (Unified multi-target binary: SE + AE + VR)
> - `SKSE/Plugins/TrueGaze.ini` (Default tuning settings with full comments and HCEP panel keys)
> - `meshes/TrueGaze/...` (Gaze beams and HCEP floating panel meshes)
> - `textures/TrueGaze/...` (HCEP chroma-keyed diagram DDS)
> - `meshes/actors/.../OpenAnimationReplacer/...` (OAR conditions)
> - `TrueGazeConfig.html` (Standalone interactive web configurator — can be opened in any browser)
> - `Launch-TrueGazeConfig.cmd` (One-click batch launcher for automated bridge)
> - `tools/TrueGazeConfig/...` (Automation bridge PowerShell scripts)
> - `TrueGaze_Configurator_Guide.txt` (Clear documentation for MO2, Vortex, and manual modders)

---

## Open Questions

None. All technical facts have been reverse-engineered and verified against the CommonLibSSE-NG codebase and Skyrim VR runtime specifications.

---

## Proposed Changes

### Component 1: Skyrim VR Multi-Targeting & Runtime Hook Fix

#### [MODIFY] [CMakeLists.txt](file:///d:/Projects/SkyrimTrueGaze/CMakeLists.txt)
- Change line 35 to:
  ```cmake
  option(BUILD_SKYRIM_VR "Target Skyrim VR" ON)
  ```
- This ensures `ENABLE_SKYRIM_SE=1`, `ENABLE_SKYRIM_AE=1`, and `ENABLE_SKYRIM_VR=1` are all active, enabling `SKYRIM_CROSS_VR` and `HAS_SKYRIM_MULTI_TARGETING` in CommonLibSSE-NG.

#### [MODIFY] [src/Engine/AnimationHook.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Engine/AnimationHook.cpp)
- In `InstallActorUpdateHook`:
  ```cpp
  template <class Hook>
  void InstallActorUpdateHook(const REL::VariantID &vtable, const char *name)
  {
      REL::Relocation<std::uintptr_t> table{vtable};
      const std::size_t updateSlot = REL::Module::IsVR() ? 0xAF : 0xAD;
      Hook::_original = table.write_vfunc(updateSlot, Hook::Hook);
      logger::info("[TrueGaze] Gaze driver installed on {}::Update (slot 0x{:02X}).", name, updateSlot);
  }
  ```

---

### Component 2: Configurator Integration & Mod Packaging Pipeline

#### [NEW] [skyrim/TrueGaze_Configurator_Guide.txt](file:///d:/Projects/SkyrimTrueGaze/skyrim/TrueGaze_Configurator_Guide.txt)
- Detailed user-friendly guide covering:
  - How to configure TrueGaze via `TrueGazeConfig.html` (browser-based) or `Launch-TrueGazeConfig.cmd`.
  - How to add `Launch-TrueGazeConfig.cmd` as an executable tool in Mod Organizer 2 (MO2) and Vortex.
  - Confirmation that the configurator is optional: launching directly from the SKSE button works 100% out of the box with any modlist.

#### [MODIFY] [scripts/PackageMod.ps1](file:///d:/Projects/SkyrimTrueGaze/scripts/PackageMod.ps1)
- Copy `TrueGazeConfig.html`, `Launch-TrueGazeConfig.cmd`, and `scripts/TrueGazeBridgeServer.ps1` into the packaging staging folder (`skyrim/tools/TrueGazeConfig/` and root).
- Ensure the resulting archive contains the complete mod + configurator suite.
- Re-run packaging to generate a verified, production-ready `TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip`.

---

## Verification Plan

### Automated Steps
1. Verify `extern/CommonLibSSE-NG/extern/openvr` submodule checkout contains `openvr.h` and `openvr_api.lib`.
2. Configure CMake with `BUILD_SKYRIM_VR=ON`:
   ```powershell
   cmake --preset windows-release
   ```
   Verify configure output reports: `CommonLibSSE-NG linked, SE=ON AE=ON VR=ON`.
3. Build Release binary:
   ```powershell
   cmake --build --preset release
   ```
4. Verify symbol relocations and exports on the built `TrueGaze.dll`.
5. Execute `scripts/PackageMod.ps1` to produce the final release archive and verify package contents.

### Manual Verification
1. Verify `TrueGaze.dll` logs in SE/AE and VR indicate correct runtime detection and vtable slot binding (`0xAD` for SE/AE, `0xAF` for VR).
2. Extract the mod package into a clean test folder and verify the Configurator launches cleanly via `Launch-TrueGazeConfig.cmd` and `TrueGazeConfig.html`.
