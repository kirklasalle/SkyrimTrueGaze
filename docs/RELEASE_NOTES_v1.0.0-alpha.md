# TRUE GAZE™ v1.0.0-alpha — Technical Preview Release Notes
### Biological NPC Gaze & Biomechanical Kinematics Engine for Skyrim
**Author & Product Owner:** Kirk LaSalle  
**Release Date:** September 19, 2026  
**Build Target:** Skyrim SE 1.5.97 · AE 1.6.640+ · AE 1.6.1170 · AE 1.7.104.0+ · Skyrim VR  
**SDK Architecture:** CommonLibSSE-NG · C++23 · MSVC 19.44+  
**Package:** `dist/TrueGaze-v1.0.0-rc1-SkyrimSE-AE-VR.zip`  

---

## 1. Executive Overview

TrueGaze™ is an advanced biological gaze perception and oculomotor kinematics engine for *The Elder Scrolls V: Skyrim*. Built as a gaming execution layer of Kirk LaSalle's **Human Communication Eye Protocol (HCEP)**, TrueGaze replaces the rigid, mannequin-like head-tracking of legacy game engines with scientifically grounded human oculomotor dynamics.

This **v1.0.0-alpha Technical Preview** provides the foundational autonomous in-engine kinematics, crosshair-directed mutual gaze, third-person player gaze mirroring, and live runtime diagnostics. It is verified crash-free and Havok physics-safe across modern Skyrim AE runtimes, including the notoriously fragile Helgen opening cart ride (`MQ101`).

---

## 2. Capability & Verification Status Matrix

Following project governance standards, all features are reported strictly according to their verified state:

| Feature Area | Status | Verified Capabilities |
| :--- | :---: | :--- |
| **Main Sequence Saccades** | ✅ In-Engine Verified | Non-linear peak velocity curve ($V_{\text{peak}} \propto \text{Amplitude}$), ballistic eye acquisition. |
| **Vestibulo-Ocular Reflex (VOR)** | ✅ In-Engine Verified | Decoupled eye-head counter-rotation during actor movement. |
| **Micro-Jitter Drift** | ✅ In-Engine Verified | Sub-degree Brownian drift ($0.15^\circ - 0.35^\circ$) eliminating frozen eye fixations. |
| **Social Triangle Scanning** | ✅ In-Engine Verified | Dynamic cyclical scanning between left eye, right eye, and mouth during interaction. |
| **Conversational Gaze Aversion** | ✅ In-Engine Verified | Periodic natural cognitive look-away breaks ($0.35 - 0.7\text{ s}$) to prevent unnatural staring. |
| **Crosshair Sweet Spot** | ✅ In-Engine Verified | Mutual gaze detection when the player centers their view on an NPC's face. |
| **3rd-Person Player Tracking** | ✅ In-Engine Verified | Live cervical and ocular gaze updates on the player character in third person. |
| **Runtime Console Commands** | ✅ In-Engine Verified | 9 live console commands (`tg`, `tgstatus`, `tgvisuals`, `tgv`, `tgon`, `tgoff`, `tgmode`, etc.) via `~`. |
| **In-Game Light Emitters** | ✅ In-Engine Verified | Dual `NiPointLight` emitters (pupil origin and target terminus) for visual debugging. |
| **HCEP Desktop Telemetry Bridge** | 🔨 Implemented | 64-byte IPC streaming over `\\.\pipe\TrueGazeBridge` connected with published HCEP Desktop. |
| **Visible Beam Geometry** | 🔨 In Calibration | NIF geometry attached via `BSModelDB::Demand`; scale and material tuning in progress. |
| **Open Animation Replacer (OAR)** | 📐 Designed | Condition state evaluators published; native OAR API binding pending Phase S6. |
| **Skyrim VR HMD Eye-Tracking** | 📐 Designed | Head-directed VR functional; specialized hardware eye-tracker adapter pending Phase S7. |

---

## 3. Installation & Setup

### Requirements:
1. **Skyrim Special Edition / Anniversary Edition** (1.5.97, 1.6.640+, 1.6.1170, or 1.7.104.0+).
2. **SKSE64** matching your Skyrim executable version.
3. **Address Library for SKSE Plugins** (matching your runtime version).
4. **Microsoft Visual C++ 2015–2022 x64 Redistributable**.

### Mod Manager Installation (Recommended):
1. Download `TrueGaze-v1.0.0-rc1-SkyrimSE-AE-VR.zip`.
2. Install via **Mod Organizer 2 (MO2)** or **Vortex**.
3. Enable the mod. Ensure Skyrim is launched via `skse64_loader.exe`.

### Manual Installation:
Extract the archive and place the contents into your Skyrim `Data/` directory:
```text
Data/
└── SKSE/
    └── Plugins/
        ├── TrueGaze.dll
        └── TrueGaze.ini
```

---

## 4. In-Game Runtime Controls

Open the Skyrim console (`~`) at any time during gameplay to execute:

* `tgstatus` — Output full real-time engine diagnostics (tracked actors, tick counts, active targets, visual emitters, and bridge state).
* `tg` — Toggle TrueGaze kinematics engine on or off.
* `tgvisuals` / `tgv` — Toggle in-game gaze visual emitters and debug rays.
* `tgmode` — Cycle visual render modes (`0` = Lights + Geometry, `1` = Lights only, `2` = Geometry only).
* `tgon` / `tgoff` — Turn all visual diagnostics on or off.
* `tgverbose` — Toggle between Info and Debug logging levels in `TrueGaze.log`.

Configuration changes made through console commands persist immediately to `TrueGaze.ini`.

---

## 5. Artifact Hashes & Provenance

* **Archive:** `TrueGaze-v1.0.0-rc1-SkyrimSE-AE-VR.zip`  
  **SHA-256:** `052E031F58A34D6C6E7E279582B925BDA95E217674398E18057DF421EF290130`
* **Binary:** `TrueGaze.dll` (Release x64, 724,480 bytes)  
  **SHA-256:** `C31AD8757A127DF25FB1CC28867D12029F6454E40BD96A440D5A3449E2D2ED02`
* **Log Location:** `%USERPROFILE%\Documents\My Games\Skyrim Special Edition\SKSE\TrueGaze.log`

---

## 6. Feedback & Issue Reporting

Please report issues, log files, or kinematics feedback to Kirk LaSalle via the GitHub repository:  
`https://github.com/kirklasalle/SkyrimTrueGaze/issues`
