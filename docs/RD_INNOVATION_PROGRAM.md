# TrueGaze™ Research & Development Innovation Program (R&D Charter)

**Program Owner:** Kirk LaSalle (Architect & Product Owner, HCEP Ecosystem)  
**Flagship Platform:** SkyrimTrueGaze (`v2.1.0+`) — **First-Party Gaming Application for the Human Communication Eye Protocol (HCEP)**  
**Strategic North Star:** **Skyrim VR (`1.4.15`) + Real-Time Biometric HCEP Bridge** as the reference immersive standard, with 100% parity on Skyrim SE (`1.5.97`) and AE (`1.6+`).  
**Established:** November 23, 2026 (derived from Phase 3 Novelty & Specialized Elements Register)

---

## 1. Purpose & Strategic Value

During the November 23, 2026 Principal-Engineer due-diligence audit, five technical pillars of `SkyrimTrueGaze` were identified as **genuine, non-mainstream engineering innovations** with high intrinsic intellectual property and product-differentiation value across the broader HCEP ecosystem.

Rather than leaving these mechanisms as ad-hoc mod code, this document establishes a **disciplined, evidence-gated Research & Development (R&D) lifecycle** so each innovation can be:
1. Formally specified with falsifiable mathematical and runtime invariants;
2. Validated empirically on **Skyrim VR (6DOF HMD + Eye Tracking)** and flat SE/AE;
3. Hardened into clean-room, reusable C++23 / Python modules transferable to HCEP Desktop and future Creation Engine / Unreal / Unity first-party applications.

---

## 2. R&D Stage-Gate Governance Model

Every R&D track progresses through five strict, evidence-backed gates. No track advances without artifacts checked into the repository:

| Gate | Name | Required Exit Artifact |
| :--- | :--- | :--- |
| **`R0`** | **Hypothesis & Prior-Art Brief** | Problem statement, neuroscience / engine primary-source citations, success metrics, and clean-room IP boundary check. |
| **`R1`** | **Formal Spec & Invariants** | Exact mathematical equations, state-machine transitions, coordinate frames, failure-mode bounds (NaN/Inf/hitch/VR). |
| **`R2`** | **Reference Prototype & Automated Harness** | SDK-free C++23 or Python reference implementation + deterministic unit/fuzz/benchmark tests in `tests/`. |
| **`R3`** | **Empirical Field Certification (SE/AE + VR)** | Live Skyrim SE/AE and Skyrim VR (`1.4.15`) telemetry traces (`TrueGaze_GazeTrace.jsonl`), frame-time percentiles (p50/p95/p99), and save-file integrity proofs (ReSaver). |
| **`R4`** | **HCEP First-Party Extraction** | Versioned ABI/spec (`include/TrueGazeAPI.h`, `HCEP_BRIDGE_SPEC.md`) ready for reuse across HCEP Desktop & partner titles. |

---

## 3. Active R&D Tracks (`NOV-01` – `NOV-05`)

### Track 1 (`NOV-01`): Biomechanical Oculomotor Kinematics & VR-Native HCEP Cognitive Engine
- **Current Gate:** `R2` (Unit-verified 16/16 kinematics tests; `R3` VR field certification open)
- **Core Assets:** `src/Kinematics/SaccadeGenerator.hpp`, `VorCoordinator.hpp`, `MicroJitter.hpp`, `SocialTriangle.hpp`, `src/Engine/CharacterProfile.hpp`, `src/Engine/VrController.cpp`, `src/Bridge/NamedPipeServer.cpp`
- **Intrinsic Innovation:**
  - Compile-time 65-entry skewed-Gaussian Main Sequence CDF (`constexpr` zero-allocation ballistic trajectory LUT).
  - Exact 2nd-order critically damped VOR spring with biological head-onset latency gap (`130 ms` eye-lead decoupling).
  - Fixed-step 120 Hz exact Ornstein-Uhlenbeck (OU) micro-saccadic drift ($\mu + (x-\mu)e^{-\theta\Delta t} + \sigma\sqrt{(1-e^{-2\theta\Delta t})/(2\theta)}\,\mathcal{N}(0,1)$) with `theta > 0` numerical guards.
  - 5-mode HCEP cognitive state machine (`LOGIC`, `AFFECT`, `SPIRIT`, `HEART`, `THINK`) coupled to a 13-region gaze classifier.
- **VR-First HCEP Research Objectives (Next Milestones):**
  1. **True 6DOF HMD + Eye-Tracker Fusion (`R3`):** Validate `VR_NODE_DATA::HmdNode` world-space coordinate alignment (`VrController.cpp`) against OpenVR eye-tracking headsets (Quest Pro, Varjo, Vive Pro Eye, Bigscreen Beyond + EyeTrackVR) streaming over `\\.\pipe\TrueGazeBridge`.
  2. **Binocular Vergence & Depth Parallax in VR:** Extend player-to-NPC incoming gaze resolution to use interpupillary distance (IPD) and binocular convergence distance (`0.3 m – 6.0 m`) for true stereoscopic eye-contact detection.
  3. **Sub-150 µs Crowd Scaling Benchmark:** Capture 25-NPC tavern traces at 90 Hz / 120 Hz VR refresh rates to certify frame-budget compliance (`p99 <= 150 µs`).

---

### Track 2 (`NOV-02`): Zero-Flash Procedural Scaleform GFx Architecture (Flat & VR World-Space UI)
- **Current Gate:** `R2` (Deterministic 25-byte SWF8 builder & Scaleform overlays live; VR world-space projection in `R1`)
- **Core Assets:** `scripts/Build-IncomingUi.ps1`, `src/ExpandedPlay/GazeReticle.cpp`, `src/Visuals/IncomingGazeManager.cpp`
- **Intrinsic Innovation:** Hand-assembled 25-byte SWF8 stubs (`TrueGazeIncoming.swf`, `TrueGazeReticle.swf`) requiring zero Adobe Flash / Animate proprietary toolchains, populated entirely via native Scaleform GFx ActionScript 2 `Invoke` calls (`createEmptyMovieClip`, `TextFormat`, `$EverywhereFont`).
- **R&D Objectives:**
  1. **Automated SWF8 Binary Validator (`R2`):** Add a Python/PowerShell structural verifier asserting SWF8 signature, frame rect, `SetBackgroundColor`/`ShowFrame`/`End` tag boundaries, and reproducible SHA-256 hash in CI.
  2. **Ultrawide, 4K & HUD-Mod Compatibility Matrix (`R3`):** Formalize normalized viewport coordinates across 16:9, 21:9, 32:9, SkyUI/Nordic UI/TrueHUD stacks.
  3. **VR World-Space Diegetic Reticle (`R1 → R3`):** In Skyrim VR, flat 2D screen-space SWF reticles cause binocular rivalry/eye strain. Research attaching diegetic gaze cues directly to `VR_NODE_DATA::CrosshairParent` / target-anchored 3D billboards (`NiBillboardNode`) at true vergence depth.

---

### Track 3 (`NOV-03`): Clean-Room Programmatic TES5 Binary Asset Synthesis
- **Current Gate:** `R2` (Pure-Python ESL/SEQ + clean-room programmatic timing WAV pipeline operational & GPL-3.0 clean)
- **Core Assets:** `scripts/Build-ExpandedPlayEsl.py`, `scripts/Build-GazeBeamNif.py` (clean-room `GazeBeam.nif` / `GazeRegionPanel.nif`)
- **Intrinsic Innovation:** 100% deterministic, Creation-Kit-free, pure-Python compilation of TES5 Form-Version-44 ESL-flagged plugins (`TES4`/`MGEF`/`SPEL`/`ARMO`/`BOOK`/`MISC`/`QUST`/`DLBR`/`DIAL`/`INFO`), binary `.seq` quest tables, and clean-room audio assets—with built-in binary round-trip structural verification (`verify_plugin`).
- **R&D Objectives:**
  1. **Typed CTDA Condition Compiler (`R2`):** Replace raw hex-encoded `CTDA` blobs in `Build-ExpandedPlayEsl.py` (`L412-L417`) with a typed Python AST builder (`ctda(op, value, func_id, param1, ...)`) validated against `Skyrim.esm` FormIDs automatically.
  2. **Clean-Room Programmatic `.lip` / Neutral Ambient Breath & Voice Synthesis (`R1 → R2`):** Investigate clean-room procedural phoneme/viseme `.lip` generation (or open-licensed original voice recordings encoded to `.xwm`/`.fuz` via open tools) so custom Moth Priest dialogue achieves full expressive lip-sync without touching a single byte of Bethesda BSA assets.
  3. **Automated IP-Cleanliness CI Gate (`R2`):** Add a CI script verifying zero extracted Bethesda BSA hashes or unauthorized `.fuz`/vanilla `.nif` blobs exist anywhere in `skyrim/` or `dist/`.

---

### Track 4 (`NOV-04`): Zero-Menu 3D Spatial Persuasion & Living Diegetic Interfaces
- **Current Gate:** `R2` (Unit-verified across 24-permutation FNV-1a algebra & Oblivion GMST curves; `R3` live tuning & VR embodiment open)
- **Core Assets:** `src/ExpandedPlay/PersuasionTransactions.hpp`, `FactionSight.hpp`, `BookStudyTracker.cpp`, `ResponseLadder.cpp`, `AwakeningRitual.cpp`
- **Intrinsic Innovation:**
  - Translates *TES IV: Oblivion*'s 4-quadrant Persuasion Wheel and non-linear Bribe GMST equations into zero-menu, real-time 3D gaze interactions (`0.35 s–1.99 s` micro-expression probes → `>= 2.0 s` commits, `Belt/Hand → Eyes` covert bribes, `2 Hz` witness sightline checks).
  - O(1)-memory deterministic 4! (`24`) FNV-1a affinity algebra scaling to every modded NPC without co-save bloat.
  - Read-only `BookMenu` live text injection (`Ocular Contemplation`) and physical open-book spell casting.
- **R&D Objectives:**
  1. **Skyrim.esm Ground-Truth FormID Verification Harness (`R2`):** Automate extraction/assertion of bespoke NPC, Faction, Quest, and Topic FormIDs against a local `Skyrim.esm` fixture (preventing regressions like `CRIT-09`).
  2. **VR Hand-Gesture + Gaze Multimodal Persuasion (`R1 → R3`):** Combine `VR_NODE_DATA::LeftWandNode`/`RightWandNode` physical gestures (weapon lowering, open palm, belt-pouch reach) with HMD/eye gaze for true embodied VR persuasion—the signature showcase of HCEP in VR.
  3. **Empirical Fun & Restraint Telemetry Study (`R3`):** Collect anonymized session traces measuring Phase 1 probe readability, voice bark frequency, and false-positive guard triggers across Riverwood, Whiterun, and Riften.

---

### Track 5 (`NOV-05`): Cryptographic Governance & High-Assurance SKSE Systems Engineering
- **Current Gate:** `R2` (Local `verify_charter.py` integrated into `Invoke-CiGate.ps1`; pre-save vfunc hook & zero-clobber console proxy implemented)
- **Core Assets:** `scripts/verify_charter.py`, `config/charter_manifest.json`, `scripts/Invoke-CiGate.ps1`, `src/Engine/AnimationHook.cpp` (`PlayerSaveGameHook`), `src/Integrations/ConsoleCommands.cpp` (`ConsoleAcceptHook`)
- **Intrinsic Innovation:**
  - Cryptographic SHA-256 charter pinning for safety/privacy invariants (e.g., Law 6 Biometric Minimization, Unconditional Child Safety Gate).
  - Non-destructive `Console::Accept` Scaleform command interception (0 vanilla `SCRIPT_FUNCTION` slots clobbered).
  - Strictly pre-save `PlayerCharacter::SaveGame` (`slot 0x0E`) modifier stripping across SE, AE, and VR.
- **R&D Objectives:**
  1. **ReSaver / FallrimTools `.ess` Forensic Proof (`R3` — Immediate Priority for `CRIT-04`):** Execute the formal `.ess` clean-save verification protocol in `docs/AUDIT_REMEDIATION_LOG.md §4` to produce signed, reproducible evidence that TrueGaze leaves zero baked ActorValue deltas on uninstall.
  2. **Automated CI Restoration (`R2`):** Restore GitHub Actions billing or attach a self-hosted Windows runner so `build-test.yml` and `charter-integrity.yml` gate every commit automatically.
  3. **In-Process Fuzz & Soak Harness (`R2 → R3`):** Extend `HcepBridgeClientMock` and `ValidationTests` with 1-hour continuous soak tests (including simulated `Stop(Join)`/`Start()` churn, `NaN`/`Inf` injection, and alt-tab 60-second hitch injection).

---

## 4. R&D Cadence & Operating Protocol

1. **Bi-Weekly R&D Experiment Log:** Record each `R2`/`R3` experiment in `docs/RD_EXPERIMENT_LOG.md` (to be created on first VR field run) with hardware spec, Skyrim build (`SE 1.5.97` / `AE 1.6.1170` / `AE 1.7.104` / `VR 1.4.15`), DLL SHA-256, trace file metrics, and pass/fail against the gate rubric.
2. **Clean-Room IP Rule (Non-Negotiable):** No Bethesda `.nif`, `.dds`, `.fuz`, `.xwm`, `.lip`, or `.pex` bytes may ever be committed to Git or staged into release archives. Reference vanilla assets strictly by virtual BSA path (`meshes\...`, vanilla `TESTopic` FormIDs) or synthesize clean-room originals programmatically.
3. **VR-First Regression Rule:** Any new engine hook, UI element, or address lookup MUST specify its Skyrim VR (`1.4.15`) path at design time (`R1`) before code review; raw SE/AE `RELOCATION_ID`s without VR validation or VM fallback are prohibited.
