# TrueGaze™ — Nexus Mods Page Presentation Guide

> **Nexus Mods Formatting Standard:**  
> Nexus Mods uses **BBCode** for mod descriptions and forum posts. Standard HTML tags (`<div>`, `<h1>`, `<b>`, `<p>`) and standard Markdown headers/tables (`#`, `|---|`) are **not parsed** or are stripped for security.  
> 
> **How to Publish to Nexus Mods:**  
> 1. Open your mod edit page on Nexus Mods.  
> 2. Click the **`[BBCode]`** button on the description toolbar to switch to raw BBCode source mode.  
> 3. Copy the entire contents of [`docs/NEXUS_MODS_PAGE_BBCODE.txt`](./NEXUS_MODS_PAGE_BBCODE.txt) (or the BBCode block below) and paste it into the editor.  
> 4. Click the **`[BBCode]`** button again to preview your rendered layout before saving.

---

## 📋 Ready-to-Paste Nexus Mods Description (BBCode)

```bbcode
[center]
[img]https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/truegaze_hero_banner.jpg[/img]

[size=6][b]TRUE GAZE™[/b][/size]
[size=4][i]Biological NPC Gaze & Biomechanical Kinematics Engine[/i][/size]
[size=3][b]The End of Dead-Eye Zombie Syndrome in Skyrim[/b][/size]

[b]Author & Product Owner:[/b] Kirk LaSalle
[b]Compatibility:[/b] Skyrim Special Edition (1.5.97), Anniversary Edition (1.6.640+, 1.6.1170, 1.7.104.0+), Skyrim VR
[b]Requirements:[/b] [url=https://skse.silverlock.org/]SKSE64[/url] • [url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url] • MSVC 2015–2022 x64
[b]Zero Script Taint:[/b] 100% Native C++ SKSE Plugin • No Papyrus • No ESP/ESL • Completely Safe to Install/Remove Mid-Playthrough
[/center]

[line]

[size=5][b]👁️ What is TrueGaze?[/b][/size]

In vanilla Skyrim and traditional head-tracking mods, characters rotate their necks like mechanical mannequins. When an NPC looks at you, their eyes remain static, frozen, and dead—a phenomenon known as the [b]"dead-eye zombie syndrome."[/b]

[b]TrueGaze™ changes everything.[/b] Built upon 50 years of psycholinguistic and neurobiological research (Argyle & Cook, Kendon, Glenberg, Bahill & Stark), TrueGaze replaces Skyrim's crude headtracking with a [b]fully realized, real-time biological oculomotor kinematics engine[/b]:

[list]
[*] [b]Eyes Lead, Head Follows (VOR Decoupling):[/b] Living eyes initiate movement within 20–30ms; the heavy cervical spine follows 100ms later. While the head turns, the eyes counter-rotate to maintain absolute foveal fixation via the Vestibulo-Ocular Reflex.
[*] [b]Ballistic Saccades (The Main Sequence):[/b] Human eyes do not move with linear damping. TrueGaze computes true physiological saccades via empirical non-linear dynamics:
[center][font=Courier New][b]V_peak = V_max · (1 - e^(-θ / C))[/b][/font][/center]
reaching peak angular speeds of 700°–900°/second.
[*] [b]Micro-Saccadic Brownian Drift:[/b] To prevent subconscious uncanny-valley freezing, TrueGaze injects 1.5–3.0 Hz micro-drift across the ocular plane.
[*] [b]Argyle & Cook Social Triangle Cycling:[/b] During dialogue, NPCs naturally shift their gaze across your facial landmarks (Left Eye → Right Eye → Lips) rather than staring unblinkingly at your chin.
[*] [b]Cognitive Gaze Aversion:[/b] NPCs glance away momentarily when processing complex thoughts or answering questions, reflecting natural human cognitive load.
[*] [b]Scripted Scene Meta-Controller (Helgen Safe):[/b] TrueGaze operates as an intelligent meta-controller during scripted sequences like the Helgen cart ride ([font=Courier New]MQ101[/font]). It modulates micro-kinematic gaze vectors without ever modifying root translations, preserving Havok physics stability.
[/list]

[line]

[size=5][b]📸 In-Game & Configurator Showcase[/b][/size]

[center]
[size=4][b]Authentic Mutual Gaze in Skyrim Taverns[/b][/size]
[img]https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/mutual_gaze_tavern.jpg[/img]
[i]Experience genuine social resonance and eye contact across Skyrim's taverns and cities.[/i]

[size=4][b]Biomechanical Diagnostic & Social Triangle Scanning[/b][/size]
[img]https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/kinematics_social_triangle.jpg[/img]
[i]Diagnostic overlay illustrating facial Social Triangle fixation cycles and VOR counter-rotation.[/i]

[size=4][b]Anatomical Cervical-Cranial Strain Distribution[/b][/size]
[img]https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/skeletal_kinematic_hierarchy.jpg[/img]
[i]Strain decomposition across cervical vertebrae: Spine2 (10%), Neck (25%), Head (65%), Ocular (100%).[/i]

[size=4][b]Standalone TrueGaze Configurator & Launcher (Live Execution Screenshot)[/b][/size]
[img]https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/screengrabs/truegaze_config_03.png[/img]
[i]The included standalone HTML configurator—detects your Skyrim install, inspects hero saves, applies silver presets, and tunes parameters with zero in-game menus.[/i]
[/center]

[line]

[size=5][b]⚡ Performance & LOD[/b][/size]

TrueGaze is written in high-performance native C++23 with zero garbage collection and zero script lag:
[list]
[*] [b]Tier 1 (< 5m / Dialogue Range):[/b] Full biological kinematics (Saccades, VOR, Social Triangle, Micro-drift).
[*] [b]Tier 2 (5m – 15m / Proximity Range):[/b] Head & Neck kinematics active; Eye nodes use simplified tracking.
[*] [b]Tier 3 (> 15m):[/b] Standard game engine LOD; processing completely bypassed for maximum FPS.
[*] [b]Benchmark Impact:[/b] Less than 0.15 ms processing time per frame. Zero measurable frame rate impact even in heavy combat.
[/list]

[line]

[size=5][b]📦 Requirements & Compatibility[/b][/size]

[size=4][b]Requirements:[/b][/size]
[list=1]
[*] [b]The Elder Scrolls V: Skyrim Special Edition / Anniversary Edition / VR[/b]
[*] [url=https://skse.silverlock.org/][b]SKSE64[/b][/url] (Matching your game version: 1.5.97, 1.6.640+, 1.6.1170, 1.7.104.0+, or VR)
[*] [url=https://www.nexusmods.com/skyrimspecialedition/mods/32444][b]Address Library for SKSE Plugins[/b][/url] (or VR Address Library)
[*] [url=https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist][b]Microsoft Visual C++ 2015–2022 x64 Redistributable[/b][/url]
[/list]

[size=4][b]Compatibility:[/b][/size]
[list]
[*] [b]Open Animation Replacer (OAR):[/b] Fully compatible. Includes pre-configured rule package.
[*] [b]Nemesis / Pandora / FNIS:[/b] 100% compatible. TrueGaze operates as an additive runtime bone layer; no animation generation runs needed.
[*] [b]Custom Body & Face Mods (CBBE, 3BA, HIMBO, High Poly Head):[/b] 100% compatible.
[*] [b]Dialogue Mods & Custom Followers:[/b] 100% compatible.
[*] [b]VR Support:[/b] Built-in HMD pose detection for Skyrim VR.
[/list]

[line]

[size=5][b]🎯 Diagnostic Visuals Policy (Organic by Default)[/b][/size]

[quote]
[b]Pure Biological Realism by Default:[/b]  
Shipped [font=Courier New]TrueGaze.ini[/font] is set to [b][font=Courier New]bEnableInGameVisuals = false[/font][/b]. You will experience pure, organic, natural eye contact without developer diagnostic laser beams or immersion-breaking ray overlays.
[/quote]

[list]
[*] [b]For Modders & Screenshot Creators:[/b] Toggle in-game diagnostic visuals at any time via the vanilla console using [font=Courier New]tgvisuals[/font] (or enable [font=Courier New]bEnableInGameVisuals = true[/font] in [font=Courier New]TrueGaze.ini[/font]).
[*] [b]Verified Light Emitter Fallback:[/b] When toggled on, TrueGaze checks for standalone mesh [font=Courier New]Data/meshes/TrueGaze/GazeBeam.nif[/font] first, followed by the Dawnguard fallback. If no mesh geometry is present, the engine automatically defaults to the verified [b][font=Courier New]NiPointLight[/font] emitter fallback[/b]—attaching subtle gold point lights to the pupils and gaze terminus without requiring any art assets.
[/list]

[line]

[size=5][b]🔄 Open Animation Replacer (OAR) Dynamic Condition Hook[/b][/size]

TrueGaze integrates directly with Open Animation Replacer (OAR) via native [b]dynamic SKSE messaging[/b] without any static compile dependencies:
[list]
[*] [b]Zero Missing-DLL Crashes:[/b] If OAR is not installed, TrueGaze detects this cleanly at runtime with zero impact on performance or stability.
[*] [b]Live Dynamic Conditions:[/b] When OAR is active, animators and modders can query live TrueGaze conditions directly:
[list]
[*] [font=Courier New]TrueGaze_IsMode [0..4][/font] — Fires animations based on HCEP cognitive state (LOGIC, AFFECT, SPIRIT, HEART, THINK).
[*] [font=Courier New]TrueGaze_IsMutualGaze [seconds][/font] — Triggers intimate, bashful, or attentive body reactions after sustained eye contact (e.g. >= 2.0s).
[*] [font=Courier New]TrueGaze_GetGazeRegion [0..12][/font] — Triggers defensive guard gestures or weapon glances when gaze fixates on drawn weapons or specific landmarks.
[/list]
[*] [b]Pre-Configured Rule Package:[/b] Ships with 7 ready-to-use OAR rule sets under [font=Courier New]meshes/actors/character/animations/OpenAnimationReplacer/TrueGaze/config.json[/font].
[/list]

[line]

[size=5][b]🐾 Multi-Race & Beast Race Compatibility[/b][/size]

TrueGaze dynamically evaluates bone hierarchies and derives anatomical eye sockets geometrically:
[list]
[*] [b]Humanoid Races (Nord, Imperial, Breton, Redguard):[/b] Full biological saccadic velocity curve, VOR head-lag, and Argyle & Cook social triangle cycling.
[*] [b]Elven Races (Altmer, Bosmer, Dunmer):[/b] Gracefully adapts to elongated craniomandibular rigs and high cheekbone geometries with zero FaceGen clipping.
[*] [b]Beast Races (Khajiit & Argonian):[/b] Fully compatible with feline and reptilian skull shapes, respect anatomical snout morphology, and enforce comfortable cervical limits (yaw <= 45°, pitch <= 35°).
[*] [b]High Poly Head & Custom Skeletons (XP32 / XPMSSE):[/b] 100% plug-and-play compatibility out of the box.
[/list]

[line]

[size=5][b]💾 Installation & Uninstallation[/b][/size]

[size=4][b]Mod Organizer 2 / Vortex:[/b][/size]
[list=1]
[*] Download [font=Courier New]TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip[/font].
[*] Install with your mod manager and enable the mod.
[*] Launch Skyrim via [font=Courier New]skse64_loader.exe[/font].
[/list]

[size=4][b]Standalone Configurator & Save Inspector:[/b][/size]
[list]
[*] Open [font=Courier New]TrueGazeConfig.html[/font] in any web browser (or run [font=Courier New]Launch-TrueGazeConfig.cmd[/font]) to inspect your hero saves, apply curated presets (Pure Oculomotor, Cinema Dynamic, True Intimacy), and tune parameters with zero game restarts.
[/list]

[size=4][b]Safe Mid-Playthrough Uninstallation:[/b][/size]
[list]
[*] TrueGaze attaches [b]no Papyrus scripts[/b] and creates [b]no persistent form data[/b] in your save. You can install, disable, or remove TrueGaze at any point in a playthrough with 100% save-file safety.
[/list]

[line]

[size=5][b]⌨️ Vanilla In-Game Console Commands (`~`)[/b][/size]

TrueGaze provides native engine console commands (vanilla only, zero Papyrus):
[list]
[*] [font=Courier New][b]tgstatus[/b][/font] — Display live runtime status, tracked actor count, and active kinematics state.
[*] [font=Courier New][b]tg[/b][/font] — Toggle the gaze engine on/off globally.
[*] [font=Courier New][b]tgvisuals[/b][/font] — Toggle developer diagnostic visuals (pupil and terminus light emitters).
[*] [font=Courier New][b]tgverbose[/b][/font] — Toggle diagnostic log verbosity (Info vs. Debug/Trace).
[*] [font=Courier New][b]tgon[/b][/font] / [font=Courier New][b]tgoff[/b][/font] — Explicitly enable or disable the gaze driver.
[/list]

[line]

[size=5][b]🔒 Release Verification & SHA-256 Checksums[/b][/size]

For security and integrity verification:

[code]
Package:       TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip
Size:          305.4 KB
SHA-256:       98D9654F807D1530B6619321C6FE9D97E0618AF2E6CD58DA20DE4AF3A19065DF

Package:       TrueGaze-v1.0.0-Symbols.zip
Size:          5.37 MB
SHA-256:       FFCC6680C5EE03F1E47A94098412582BBC7C21A592D2E17C79F2F60D3426BA94
[/code]

[line]

[center]
[size=4][b]TrueGaze™ is authored and engineered by Kirk LaSalle.[/b][/size]  
[size=3][i]A First-Party Implementation of the Human Communication Eye Protocol (HCEP).[/i][/size]
[/center]

```

---

## 📖 GitHub Markdown Preview

*(The rendered preview below is provided for reading convenience within GitHub and local Markdown viewers.)*

---

# TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine
### A First-Party Gaming Product of Kirk LaSalle's Human Communication Eye Protocol (HCEP)

---

![TrueGaze Banner](https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/truegaze_hero_banner.jpg)

# TRUE GAZE™
### *Biological NPC Gaze & Biomechanical Kinematics Engine*
**The End of Dead-Eye Zombie Syndrome in Skyrim**

- **Author & Product Owner:** Kirk LaSalle
- **Compatibility:** Skyrim Special Edition (1.5.97), Anniversary Edition (1.6.640+, 1.6.1170, 1.7.104.0+), Skyrim VR
- **Requirements:** [SKSE64](https://skse.silverlock.org/) • [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) • MSVC 2015–2022 x64
- **Zero Script Taint:** 100% Native C++ SKSE Plugin • No Papyrus • No ESP/ESL • Completely Safe to Install/Remove Mid-Playthrough

---

## 👁️ What is TrueGaze?

In vanilla Skyrim and traditional head-tracking mods, characters rotate their necks like mechanical mannequins. When an NPC looks at you, their eyes remain static, frozen, and dead—a phenomenon known as the **"dead-eye zombie syndrome."**

**TrueGaze™ changes everything.** Built upon 50 years of psycholinguistic and neurobiological research (Argyle & Cook, Kendon, Glenberg, Bahill & Stark), TrueGaze replaces Skyrim's crude headtracking with a **fully realized, real-time biological oculomotor kinematics engine**:

* **Eyes Lead, Head Follows (VOR Decoupling):** Living eyes initiate movement within 20–30ms; the heavy cervical spine follows 100ms later. While the head turns, the eyes counter-rotate to maintain absolute foveal fixation via the Vestibulo-Ocular Reflex.
* **Ballistic Saccades (The Main Sequence):** Human eyes do not move with linear damping. TrueGaze computes true physiological saccades via empirical non-linear dynamics:
  $$V_{	ext{peak}} = V_{	ext{max}} \cdot \left(1 - e^{-	heta/C}ight)$$
  reaching peak angular speeds of 700°–900°/second.
* **Micro-Saccadic Brownian Drift:** To prevent subconscious uncanny-valley freezing, TrueGaze injects 1.5–3.0 Hz micro-drift across the ocular plane.
* **Argyle & Cook Social Triangle Cycling:** During dialogue, NPCs naturally shift their gaze across your facial landmarks (Left Eye → Right Eye → Lips) rather than staring unblinkingly at your chin.
* **Cognitive Gaze Aversion:** NPCs glance away momentarily when processing complex thoughts or answering questions, reflecting natural human cognitive load.
* **Scripted Scene Meta-Controller (Helgen Safe):** TrueGaze operates as an intelligent meta-controller during scripted sequences like the Helgen cart ride (`MQ101`). It modulates micro-kinematic gaze vectors without ever modifying root translations, preserving Havok physics stability.

---

## 📸 In-Game & Configurator Showcase

**Authentic Mutual Gaze in Skyrim Taverns**  
![Mutual Gaze Tavern](https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/mutual_gaze_tavern.jpg)  
*Experience genuine social resonance and eye contact across Skyrim's taverns and cities.*

**Biomechanical Diagnostic & Social Triangle Scanning**  
![Social Triangle](https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/kinematics_social_triangle.jpg)  
*Diagnostic overlay illustrating facial Social Triangle fixation cycles and VOR counter-rotation.*

**Anatomical Cervical-Cranial Strain Distribution**  
![Kinematic Hierarchy](https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/images/skeletal_kinematic_hierarchy.jpg)  
*Strain decomposition across cervical vertebrae: Spine2 (10%), Neck (25%), Head (65%), Ocular (100%).*

**Standalone TrueGaze Configurator & Launcher (Live Execution Screenshot)**  
![Configurator Screenshot](https://raw.githubusercontent.com/kirklasalle/SkyrimTrueGaze/main/docs/screengrabs/truegaze_config_03.png)  
*The included standalone HTML configurator—detects your Skyrim install, inspects hero saves, applies silver presets, and tunes parameters with zero in-game menus.*

---

## ⚡ Performance & LOD

TrueGaze is written in high-performance native C++23 with zero garbage collection and zero script lag:
* **Tier 1 (< 5m / Dialogue Range):** Full biological kinematics (Saccades, VOR, Social Triangle, Micro-drift).
* **Tier 2 (5m – 15m / Proximity Range):** Head & Neck kinematics active; Eye nodes use simplified tracking.
* **Tier 3 (> 15m):** Standard game engine LOD; processing completely bypassed for maximum FPS.
* **Benchmark Impact:** Less than 0.15 ms processing time per frame. Zero measurable frame rate impact even in heavy combat.

---

## 📦 Requirements & Compatibility

### Requirements:
1. **The Elder Scrolls V: Skyrim Special Edition / Anniversary Edition / VR**
2. **[SKSE64](https://skse.silverlock.org/)** (Matching your game version: 1.5.97, 1.6.640+, 1.6.1170, 1.7.104.0+, or VR)
3. **[Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444)** (or VR Address Library)
4. **[Microsoft Visual C++ 2015–2022 x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)**

### Compatibility:
* **Open Animation Replacer (OAR):** Fully compatible. Includes pre-configured rule package.
* **Nemesis / Pandora / FNIS:** 100% compatible. TrueGaze operates as an additive runtime bone layer; no animation generation runs needed.
* **Custom Body & Face Mods (CBBE, 3BA, HIMBO, High Poly Head):** 100% compatible.
* **Dialogue Mods & Custom Followers:** 100% compatible.
* **VR Support:** Built-in HMD pose detection for Skyrim VR.

---

## 🎯 Diagnostic Visuals Policy (Organic by Default)

> **Pure Biological Realism by Default:**  
> Shipped `TrueGaze.ini` is set to `bEnableInGameVisuals = false`. You will experience pure, organic, natural eye contact without developer diagnostic laser beams or immersion-breaking ray overlays.

* **For Modders & Screenshot Creators:** Toggle in-game diagnostic visuals at any time via the vanilla console using `tgvisuals` (or enable `bEnableInGameVisuals = true` in `TrueGaze.ini`).
* **Verified Light Emitter Fallback:** When toggled on, TrueGaze checks for standalone mesh `Data/meshes/TrueGaze/GazeBeam.nif` first, followed by the Dawnguard fallback. If no mesh geometry is present, the engine automatically defaults to the verified **`NiPointLight` emitter fallback**—attaching subtle gold point lights to the pupils and gaze terminus without requiring any art assets.

---

## 🔄 Open Animation Replacer (OAR) Dynamic Condition Hook

TrueGaze integrates directly with Open Animation Replacer (OAR) via native **dynamic SKSE messaging** without any static compile dependencies:
* **Zero Missing-DLL Crashes:** If OAR is not installed, TrueGaze detects this cleanly at runtime with zero impact on performance or stability.
* **Live Dynamic Conditions:** When OAR is active, animators and modders can query live TrueGaze conditions directly:
  * `TrueGaze_IsMode [0..4]` — Fires animations based on HCEP cognitive state (LOGIC, AFFECT, SPIRIT, HEART, THINK).
  * `TrueGaze_IsMutualGaze [seconds]` — Triggers intimate, bashful, or attentive body reactions after sustained eye contact (e.g. >= 2.0s).
  * `TrueGaze_GetGazeRegion [0..12]` — Triggers defensive guard gestures or weapon glances when gaze fixates on drawn weapons or specific landmarks.
* **Pre-Configured Rule Package:** Ships with 7 ready-to-use OAR rule sets under `meshes/actors/character/animations/OpenAnimationReplacer/TrueGaze/config.json`.

---

## 🐾 Multi-Race & Beast Race Compatibility

TrueGaze dynamically evaluates bone hierarchies and derives anatomical eye sockets geometrically:
* **Humanoid Races (Nord, Imperial, Breton, Redguard):** Full biological saccadic velocity curve, VOR head-lag, and Argyle & Cook social triangle cycling.
* **Elven Races (Altmer, Bosmer, Dunmer):** Gracefully adapts to elongated craniomandibular rigs and high cheekbone geometries with zero FaceGen clipping.
* **Beast Races (Khajiit & Argonian):** Fully compatible with feline and reptilian skull shapes, respect anatomical snout morphology, and enforce comfortable cervical limits (yaw <= 45°, pitch <= 35°).
* **High Poly Head & Custom Skeletons (XP32 / XPMSSE):** 100% plug-and-play compatibility out of the box.

---

## 💾 Installation & Uninstallation

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

## 🔒 Release Verification & SHA-256 Checksums

For security and integrity verification:

| Package | File Name | Size | SHA-256 Checksum |
| :--- | :--- | :--- | :--- |
| **Main Mod Archive** | `TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip` | 305.4 KB | `98D9654F807D1530B6619321C6FE9D97E0618AF2E6CD58DA20DE4AF3A19065DF` |
| **Companion Symbols** | `TrueGaze-v1.0.0-Symbols.zip` | 5.37 MB | `FFCC6680C5EE03F1E47A94098412582BBC7C21A592D2E17C79F2F60D3426BA94` |

---

**TrueGaze™ is authored and engineered by Kirk LaSalle.**  
*A First-Party Implementation of the Human Communication Eye Protocol (HCEP).*
