# TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine
### A First-Party Gaming Product of Kirk LaSalle's Human Communication Eye Protocol (HCEP)

---

[center]
[img]https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/truegaze_hero_banner.jpg[/img]

[size=6][b]TRUE GAZE™[/b][/size]
[size=4][i]The End of Dead-Eye Zombie Syndrome in Skyrim[/i][/size]

[b]Author & Product Owner:[/b] Kirk LaSalle  
[b]Compatibility:[/b] Skyrim Special Edition (1.5.97), Anniversary Edition (1.6.640+, 1.6.1170, 1.7.104.0+), Skyrim VR  
[b]Requirements:[/b] SKSE64 · Address Library for SKSE Plugins · MSVC 2015-2022 x64  
[b]Zero Script Taint:[/b] 100% C++ SKSE Plugin · No Papyrus · No ESP/ESL · Completely Safe to Install/Remove Mid-Playthrough  
[/center]

---

## 👁️ What is TrueGaze?

In vanilla Skyrim and traditional head-tracking mods, characters rotate their necks like mechanical mannequins. When an NPC looks at you, their eyes remain static, frozen, and dead—a phenomenon known as the **"dead-eye zombie syndrome."**

**TrueGaze™ changes everything.** Built upon 50 years of psycholinguistic and neurobiological research (Argyle & Cook, Kendon, Glenberg, Bahill & Stark), TrueGaze replaces Skyrim's crude headtracking with a **fully realized, real-time biological oculomotor kinematics engine**:

* **Eyes Lead, Head Follows (VOR Decoupling):** Living eyes initiate movement within 20–30ms; the heavy cervical spine follows 100ms later. While the head turns, the eyes counter-rotate to maintain absolute foveal fixation via the Vestibulo-Ocular Reflex.
* **Ballistic Saccades (The Main Sequence):** Human eyes do not move with linear damping. TrueGaze computes true physiological saccades via empirical non-linear dynamics:
  $$V_{\text{peak}} = V_{\text{max}} \cdot \left(1 - e^{-\theta/C}\right)$$
  reaching peak angular speeds of 700°–900°/second.
* **Micro-Saccadic Brownian Drift:** To prevent subconscious uncanny-valley freezing, TrueGaze injects 1.5–3.0 Hz micro-drift across the ocular plane.
* **Argyle & Cook Social Triangle Cycling:** During dialogue, NPCs naturally shift their gaze across your facial landmarks (Left Eye → Right Eye → Lips) rather than staring unblinkingly at your chin.
* **Cognitive Gaze Aversion:** NPCs glance away momentarily when processing complex thoughts or answering questions, reflecting natural human cognitive load.
* **Scripted Scene Meta-Controller (Helgen Safe):** TrueGaze operates as an intelligent meta-controller during scripted sequences like the Helgen cart ride (`MQ101`). It modulates micro-kinematic gaze vectors without ever modifying root translations, preserving Havok physics stability.

---

## 📸 In-Game & Configurator Showcase

[center]
[b]Authentic Mutual Gaze in Skyrim Taverns[/b]
[img]https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/mutual_gaze_tavern.jpg[/img]
[i]Experience genuine social resonance and eye contact across Skyrim's taverns and cities.[/i]

[b]Biomechanical Diagnostic & Social Triangle Scanning[/b]
[img]https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/kinematics_social_triangle.jpg[/img]
[i]Diagnostic overlay illustrating facial Social Triangle fixation cycles and VOR counter-rotation.[/i]

[b]Anatomical Cervical-Cranial Strain Distribution[/b]
[img]https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/skeletal_kinematic_hierarchy.jpg[/img]
[i]Strain decomposition across cervical vertebrae: Spine2 (10%), Neck (25%), Head (65%), Ocular (100%).[/i]

[b]Standalone TrueGaze Configurator & Launcher (Live Execution Screenshot)[/b]
[img]https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/screengrabs/truegaze_config_03.png[/img]
[i]The included standalone HTML configurator—detects your Skyrim install, inspects hero saves, applies silver presets, and tunes parameters with zero in-game menus.[/i]
[/center]

---

## ⚡ Performance & LOD

TrueGaze is written in high-performance native C++23 with zero garbage collection and zero script lag:
* **Tier 1 (< 5m / Dialogue Range):** Full biological kinematics (Saccades, VOR, Social Triangle, Micro-drift).
* **Tier 2 (5m – 15m / Proximity Range):** Head & Neck kinematics active; Eye nodes use simplified tracking.
* **Tier 3 (> 15m):** Standard game engine LOD; processing completely bypassed for maximum FPS.
* **Benchmark Impact:** $< 0.15\text{ ms}$ processing time per frame. Zero measurable frame rate impact even in heavy combat.

---

## 🛠️ Requirements & Compatibility

### Requirements:
1. **The Elder Scrolls V: Skyrim Special Edition / Anniversary Edition / VR**
2. **SKSE64** (Matching your game version)
3. **Address Library for SKSE Plugins**
4. **Microsoft Visual C++ 2015–2022 x64 Redistributable**

### Compatibility:
* **Open Animation Replacer (OAR):** Fully compatible. Includes pre-configured rule package.
* **Nemesis / Pandora / FNIS:** 100% compatible. TrueGaze operates as an additive runtime bone layer; no animation generation runs needed.
* **Custom Body & Face Mods (CBBE, 3BA, HIMBO, High Poly Head):** 100% compatible.
* **Dialogue Mods & Custom Followers:** 100% compatible.
* **VR Support:** Built-in HMD pose detection for Skyrim VR.

---

## 🛡️ Diagnostic Visuals Policy (Organic by Default)

[quote]
[b]Pure Biological Realism by Default:[/b]  
Shipped `TrueGaze.ini` is set to [b]`bEnableInGameVisuals = false`[/b]. You will experience pure, organic, natural eye contact without developer diagnostic laser beams or immersion-breaking ray overlays.
[/quote]

* **For Modders & Screenshot Creators:** Toggle in-game diagnostic visuals at any time via the vanilla console using `tgvisuals` (or enable `bEnableInGameVisuals = true` in `TrueGaze.ini`).
* **Verified Light Emitter Fallback:** When toggled on, TrueGaze checks for standalone mesh `Data/meshes/TrueGaze/GazeBeam.nif` first, followed by the Dawnguard fallback. If no mesh geometry is present, the engine automatically defaults to the verified **`NiPointLight` emitter fallback**—attaching subtle gold point lights to the pupils and gaze terminus without requiring any art assets.

---

## 🎭 Open Animation Replacer (OAR) Dynamic Condition Hook

TrueGaze integrates directly with Open Animation Replacer (OAR) via native **dynamic SKSE messaging** without any static compile dependencies:
* **Zero Missing-DLL Crashes:** If OAR is not installed, TrueGaze detects this cleanly at runtime with zero impact on performance or stability.
* **Live Dynamic Conditions:** When OAR is active, animators and modders can query live TrueGaze conditions directly:
  * `TrueGaze_IsMode [0..4]` — Fires animations based on HCEP cognitive state (LOGIC, AFFECT, SPIRIT, HEART, THINK).
  * `TrueGaze_IsMutualGaze [seconds]` — Triggers intimate, bashful, or attentive body reactions after sustained eye contact (e.g. >= 2.0s).
  * `TrueGaze_GetGazeRegion [0..12]` — Triggers defensive guard gestures or weapon glances when gaze fixates on drawn weapons or specific landmarks.
* **Pre-Configured Rule Package:** Ships with 7 ready-to-use OAR rule sets under `meshes/actors/character/animations/OpenAnimationReplacer/TrueGaze/config.json`.

---

## 🧝 Multi-Race & Beast Race Compatibility

TrueGaze dynamically evaluates bone hierarchies and derives anatomical eye sockets geometrically:
* **Humanoid Races (Nord, Imperial, Breton, Redguard):** Full biological saccadic velocity curve, VOR head-lag, and Argyle & Cook social triangle cycling.
* **Elven Races (Altmer, Bosmer, Dunmer):** Gracefully adapts to elongated craniomandibular rigs and high cheekbone geometries with zero FaceGen clipping.
* **Beast Races (Khajiit & Argonian):** Fully compatible with feline and reptilian skull shapes, respect anatomical snout morphology, and enforce comfortable cervical limits (yaw <= 45°, pitch <= 35°).
* **High Poly Head & Custom Skeletons (XP32 / XPMSSE):** 100% plug-and-play compatibility out of the box.

---

## 📦 Installation & Uninstallation

### Mod Organizer 2 / Vortex:
1. Download `TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip`.
2. Install with your mod manager and enable the mod.
3. Launch Skyrim via `skse64_loader.exe`.

### Standalone Configurator & Save Inspector:
* Open `TrueGazeConfig.html` in any web browser (or run `Launch-TrueGazeConfig.cmd`) to inspect your hero saves, apply curated presets (Pure Oculomotor, Cinema Dynamic, True Intimacy), and tune parameters with zero game restarts.

### Safe Mid-Playthrough Uninstallation:
* TrueGaze attaches **no Papyrus scripts** and creates **no persistent form data** in your save. You can install, disable, or remove TrueGaze at any point in a playthrough with 100% save-file safety.

---

## ⌨️ Vanilla In-Game Console Commands (`~`)

TrueGaze provides native engine console commands (vanilla only, zero Papyrus):
* `tgstatus` — Display live runtime status, tracked actor count, and active kinematics state.
* `tg` — Toggle the gaze engine on/off globally.
* `tgvisuals` — Toggle developer diagnostic visuals (pupil and terminus light emitters).
* `tgverbose` — Toggle diagnostic log verbosity (Info vs. Debug/Trace).
* `tgon` / `tgoff` — Explicitly enable or disable the gaze driver.

---

## 🔐 Release Verification & SHA-256 Checksums

For security and integrity verification:

| Package | File Name | Size | SHA-256 Checksum |
| :--- | :--- | :--- | :--- |
| **Main Mod Archive** | `TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip` | ~304 KB | `41AB39649F3B78F505F6BA4D972460365CC4EFAA0352C61297CB66F60991410F` |
| **Companion Symbols** | `TrueGaze-v1.0.0-Symbols.zip` | ~4.87 MB | `5021AB863EF30539F0A0DE4FC87A9C88C090AEB55B16FF3BC4FF74F28D10EA28` |

---

[center]
[b]TrueGaze™ is authored and engineered by Kirk LaSalle.[/b]  
[i]A First-Party Implementation of the Human Communication Eye Protocol (HCEP).[/i]
[/center]
