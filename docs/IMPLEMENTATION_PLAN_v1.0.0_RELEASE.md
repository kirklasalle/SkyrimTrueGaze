# Implementation Plan: Final Critical Path to Public Release (v1.0.0)

This plan executes the remaining critical path items to achieve the official Public Release of TrueGaze™ v1.0.0 for Skyrim SE/AE/VR.

## User Review Required

> [!IMPORTANT]
> - **Diagnostic Visuals Policy**: `skyrim/SKSE/Plugins/TrueGaze.ini` defaults `bEnableInGameVisuals = false` and `bGazeRaysEnabled = false` so regular players experience pure, organic eye contact without developer diagnostic laser beams. For modders toggling `tgvisuals`, `VisualEffectsManager` will probe `meshes\TrueGaze\GazeBeam.nif` first, fallback to `meshes\dlc01\effects\fxsoulcairnbeam.nif`, and seamlessly default to the verified `NiPointLight` emitter fallback (`TrueGaze_PupilLight` and `TrueGaze_TerminusLight`) when geometry is absent.
> - **Open Animation Replacer (OAR) Dynamic Messaging Hook**: SKSE messaging registration is finalized without static compile dependencies. TrueGaze dynamically detects `OpenAnimationReplacer.dll` at runtime and registers condition query hooks via SKSE dynamic messaging so modded body animations can respond to `TrueGaze_IsMode`, `TrueGaze_IsMutualGaze`, and `TrueGaze_GetGazeRegion`.
> - **Multi-Race & Dialogue Field Acceptance**: Documented verification scenario in `docs/TEST_SCENARIO.md` for peaceful town cells (Riverwood Trader, Bannered Mare) covering humanoid (Nord, Elf) and beast (Khajiit, Argonian) races.
> - **Release Packaging**: `scripts/PackageMod.ps1` updated for v1.0.0 final release, producing `dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip` and companion symbols `dist/TrueGaze-v1.0.0-Symbols.zip` (`TrueGaze.pdb`), with SHA-256 hash calculation.

---

## Proposed Changes

### 1. Diagnostic Visuals Policy & Light Emitter Fallback

#### [MODIFY] [VisualTuning.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualTuning.hpp)
- Replace any remaining "simulation" terminology with "runtime engine".
- Update beam model path configuration to support primary mod asset (`meshes\TrueGaze\GazeBeam.nif`) and secondary fallback (`meshes\dlc01\effects\fxsoulcairnbeam.nif`).

#### [MODIFY] [VisualEffectsManager.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Visuals/VisualEffectsManager.cpp)
- Enhance `EnsureBeamGeometry`: if primary beam mesh `meshes\TrueGaze\GazeBeam.nif` does not exist, probe `meshes\dlc01\effects\fxsoulcairnbeam.nif`.
- If neither mesh is available, gracefully utilize the verified `NiPointLight` emitter fallback (`EnsureLight`), logging informative diagnostics without errors.

#### [MODIFY] [TrueGaze.ini](file:///d:/Projects/SkyrimTrueGaze/skyrim/SKSE/Plugins/TrueGaze.ini)
- Reaffirm `bEnableInGameVisuals = false` and `bGazeRaysEnabled = false` by default.
- Clarify diagnostic visuals policy and light emitter fallback in file commentary.

---

### 2. Open Animation Replacer (OAR) Dynamic Messaging Hook

#### [MODIFY] [OarConditions.hpp](file:///d:/Projects/SkyrimTrueGaze/src/Integrations/OarConditions.hpp)
- Define custom message types for SKSE dynamic messaging (`kMessage_RegisterConditions`, `kMessage_QueryIsMode`, `kMessage_QueryIsMutualGaze`, `kMessage_QueryGazeRegion`).
- Declare dynamic messaging receiver `OnSkseMessage(SKSE::MessagingInterface::Message* a_msg)`.
- Update `RegisterWithOar()` signature and documentation to reflect dynamic SKSE messaging interface.

#### [MODIFY] [OarConditions.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Integrations/OarConditions.cpp)
- Implement dynamic detection of `OpenAnimationReplacer.dll` via `GetModuleHandleA` and `GetProcAddress("RequestPluginAPI_Conditions")`.
- Implement `OnSkseMessage` to handle incoming condition query messages from other SKSE plugins/evaluators dynamically without static compile dependencies.
- Update `RegisterWithOar()` to report true when dynamic registration or messaging interface is active, and update atomic status flag `g_registeredWithOar`.

#### [MODIFY] [Main.cpp](file:///d:/Projects/SkyrimTrueGaze/src/Main.cpp)
- In `SKSEPluginLoad`, register `MessageHandler` for all senders (`messaging->RegisterListener(nullptr, MessageHandler)`).
- In `MessageHandler`, dispatch messages to `OarConditions::OnSkseMessage(a_msg)` and trigger OAR dynamic registration on `kPostLoad` and `kDataLoaded`.

---

### 3. Broad Multi-Race & Dialogue Field Acceptance

#### [MODIFY] [TEST_SCENARIO.md](file:///d:/Projects/SkyrimTrueGaze/docs/TEST_SCENARIO.md)
- Cleanse all remaining "simulation" occurrences.
- Add **Stage 6: Multi-Race & Dialogue Field Acceptance (Humanoid & Beast Races)**:
  - Locations: Riverwood Trader (Lucan Valerius - Imperial/Human), Bannered Mare (Hulda - Nord), Riverwood (Faendal - Bosmer/Elf), Khajiit Caravans (Kharjo/Ri'saad), Riften/Windhelm (Argonians).
  - Verification checklist: Third-person dialogue camera, VOR counter-rotation, Social Triangle cycling, beast craniomandibular morphology compatibility, zero FaceGen morph tearing or Havok desync.

#### [MODIFY] [STATUS.md](file:///d:/Projects/SkyrimTrueGaze/docs/STATUS.md) & [ROADMAP.md](file:///d:/Projects/SkyrimTrueGaze/ROADMAP.md)
- Document the multi-race field acceptance protocol and update OAR dynamic messaging status.

---

### 4. Public Distribution Packaging & Nexus Artifacts

#### [MODIFY] [PackageMod.ps1](file:///d:/Projects/SkyrimTrueGaze/scripts/PackageMod.ps1)
- Update release version to `1.0.0` (final release).
- Package `dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip`.
- Package `dist/TrueGaze-v1.0.0-Symbols.zip` containing `TrueGaze.pdb`.
- Calculate SHA-256 hashes for both files and output to console.

#### [MODIFY] [NEXUS_MODS_PAGE.md](file:///d:/Projects/SkyrimTrueGaze/docs/NEXUS_MODS_PAGE.md)
- Enrich presentation page with complete feature breakdown, diagnostic visuals policy documentation, OAR dynamic condition guide, multi-race field acceptance notes, and archive SHA-256 hashes.

---

## Verification Plan

### Automated Tests
- Build MSVC Release binary:
  ```powershell
  cmake --preset windows-release
  cmake --build --preset release
  ```
- Run unit test suite:
  ```powershell
  .\bin\Debug\KinematicsTests.exe
  ```
- Run charter verification:
  ```powershell
  python scripts/verify_charter.py
  ```
- Run mod packaging pipeline:
  ```powershell
  powershell -ExecutionPolicy Bypass -File scripts/PackageMod.ps1
  ```

### Manual Verification
- Verify archive contents of `dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip` and `dist/TrueGaze-v1.0.0-Symbols.zip`.
- Verify SHA-256 checksums.
