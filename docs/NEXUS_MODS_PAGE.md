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

## 📦 Installation & Uninstallation

### Mod Organizer 2 / Vortex:
1. Download `TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip`.
2. Install with your mod manager and enable the mod.
3. Launch Skyrim via `skse64_loader.exe`.

### Standalone Configurator:
* Open `TrueGazeConfig.html` in your browser (or run `Launch-TrueGazeConfig.cmd`) to adjust presets, inspect your saves, and customize kinematics.

### Safe Uninstallation:
* TrueGaze attaches no Papyrus scripts and creates no persistent form data. You can disable or remove the mod at any time without corrupting your save files.

---

## ⌨️ Vanilla In-Game Console Commands (`~`)

TrueGaze includes optional runtime console commands (vanilla only, zero Papyrus):
* `tgstatus` — Display full runtime status, tracked actors, and active kinematics state.
* `tg` — Toggle the gaze kinematics engine on/off.
* `tgvisuals` — Toggle developer diagnostic visuals.
* `tgverbose` — Toggle diagnostic log verbosity.

---

[center]
[b]TrueGaze™ is designed and created by Kirk LaSalle.[/b]  
[i]A First-Party Implementation of the Human Communication Eye Protocol (HCEP).[/i]
[/center]
