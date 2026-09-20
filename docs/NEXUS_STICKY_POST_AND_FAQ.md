# TrueGaze™ — Nexus Mods Sticky Post & Community Kit

**Product:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine  
**Author:** Kirk LaSalle  
**Nexus Mods Link:** [https://www.nexusmods.com/skyrimspecialedition/mods/192480](https://www.nexusmods.com/skyrimspecialedition/mods/192480)  
**GitHub Repository:** [https://github.com/kirklasalle/SkyrimTrueGaze](https://github.com/kirklasalle/SkyrimTrueGaze)  

---

## 📌 Ready-to-Paste Nexus Sticky Post (BBCode)

*Copy the entire block below and paste it as a Sticky / Pinned Comment in the Nexus Mods "Posts" tab for Mod #192480:*

```bbcode
[center]
[size=5][b]Welcome to TrueGaze™ v1.0.0 — Biological NPC Gaze & Kinematics Engine[/b][/size]
[i]Author & Architect: Kirk LaSalle | An Official First-Party HCEP Implementation[/i]
[/center]

Thank you for trying TrueGaze™! This sticky post serves as the official Quick Start Guide, FAQ, and Troubleshooting Hub for the mod.

[size=4][b]✨ The TrueGaze Difference[/b][/size]
In vanilla Skyrim, NPC eye contact is stiff, dead-eyed, and robotic. TrueGaze replaces this with a live, continuous biological kinematics runtime engine founded on peer-reviewed biomechanical science:
• [b]Ballistic Saccades:[/b] Peak velocities up to 745°/s modeled by the empirical Main Sequence equation (Bahill, Clark & Stark 1975).
• [b]Vestibulo-Ocular Reflex (VOR):[/b] Continuous counter-rotation compensation (Baloh et al. 1975).
• [b]Micro-Saccadic Brownian Drift:[/b] 1–3 Hz physiological ocular tremor (Ornstein-Uhlenbeck drift) with per-actor unique stochastic seeding.
• [b]Social Triangle Scanning:[/b] Spontaneous foveal shifts between Left Eye, Right Eye, and Mouth during dialogue (Argyle & Cook 1976).
• [b]Zero Script Taint:[/b] NO Papyrus scripts, NO ESP plugins, NO form overrides. 100% safe to install or remove at any point in your playthrough.

---

[size=4][b]📦 Quick Installation (MO2 / Vortex)[/b][/size]
1. [b]Prerequisites:[/b]
   • [url=https://skse.silverlock.org/]SKSE64[/url] (matching your game runtime: 1.5.97, 1.6.640, 1.6.1170, or VR).
   • [url=https://www.nexusmods.com/skyrimspecialedition/mods/32444]Address Library for SKSE Plugins[/url].
2. [b]Install Mod:[/b] Download the Main Archive via your mod manager (Mod Organizer 2 or Vortex) and enable it.
3. [b]Load Order:[/b] Because TrueGaze has no plugin (ESP/ESL), load order does not matter! You can place it anywhere in your mod list.

---

[size=4][b]⚙️ Configuration & Customization[/b][/size]
TrueGaze ships with pristine biological defaults out of the box. Diagnostic laser visuals are [b]OFF by default[/b] so your game remains atmospheric and organic.

To customize your gaze dynamics:
• [b]Interactive Visual Web Configurator:[/b] Open [b]TrueGazeConfig.html[/b] in any browser (or double-click [b]Launch-TrueGazeConfig.cmd[/b]) to adjust saccade velocity, ocular strain sharing, and HCEP profiles with a real-time animated pupil preview!
• [b]Manual INI Tuning:[/b] Edit [b]Data/SKSE/Plugins/TrueGaze.ini[/b] directly.

---

[size=4][b]⌨️ In-Game Console Commands (`~`)[/b][/size]
Open the game console at any time to run native engine commands (instant, zero Papyrus overhead):
• [b]tgstatus[/b] — Display live engine status, active target resolution, tracked actor count, and visual emitters.
• [b]tg[/b] — Toggle the TrueGaze kinematics engine on or off globally.
• [b]tgvisuals[/b] — Toggle developer diagnostic gaze rays (pupil and terminus light emitters).
• [b]tgmode[/b] — Cycle visual render modes (0 = Both, 1 = Light Only, 2 = Geometry Only).
• [b]tgverbose[/b] — Toggle verbose diagnostic logging (Debug vs. Info).

---

[size=4][b]❓ Frequently Asked Questions (FAQ)[/b][/size]

[b]Q: Is it safe to install or uninstall mid-playthrough?[/b]
[b]A: YES. 100% safe.[/b] TrueGaze attaches zero scripts to your save game. Deleting or disabling the mod leaves your save file completely pristine.

[b]Q: Is TrueGaze compatible with High Poly Head, EFM, or custom skin/eye textures?[/b]
[b]A: YES.[/b] TrueGaze dynamically resolves head and ocular sockets using geometric bounds analysis and candidate search. It works seamlessly with High Poly Head, Expressive Facegen Morphs, CotR, and custom skin/eye meshes.

[b]Q: Does TrueGaze work with Open Animation Replacer (OAR)?[/b]
[b]A: YES.[/b] TrueGaze automatically hooks the OAR dynamic messaging interface to publish live HCEP cognitive mode and mutual gaze states without static dependencies.

[b]Q: Does TrueGaze impact my frame rate (FPS)?[/b]
[b]A: NO.[/b] TrueGaze features a 3-tier distance Level-of-Detail (LOD) manager. Full kinematics evaluate only within dialogue range (< 5 m), dropping to lightweight head kinematics at 5–15 m, and completely bypassing actors beyond 15 m. Average frame time is less than 0.05 ms.

[b]Q: What if I encounter an issue or crash?[/b]
[b]A:[/b] TrueGaze logs detailed initialization and runtime diagnostic markers to:
[i]Documents\My Games\Skyrim Special Edition\SKSE\TrueGaze.log[/i]
Please upload or paste your log if reporting any anomaly! We also provide full companion debug symbols ([b]TrueGaze-v1.0.0-Symbols.zip[/b]) on the Files tab for crash loggers (Crash Logger SSE / Trainwreck).

Enjoy living, breathing eye contact in Skyrim!
— Kirk LaSalle
```

---

## 📢 Reddit / Discord Community Announcement Template

*Recommended template for sharing in r/skyrimmods, Skyrim Guild, or community Discords:*

> **Title:** [Release] TrueGaze™ — Living Biological NPC Gaze & Biomechanical Kinematics Engine (SE / AE / VR)
>
> **Links:**  
> Nexus Mods: https://www.nexusmods.com/skyrimspecialedition/mods/192480  
> GitHub (Source Code & SDK): https://github.com/kirklasalle/SkyrimTrueGaze  
>
> Hey everyone!
>
> I'm excited to share the initial 1.0.0 release of **TrueGaze™**, an SKSE plugin designed to bring authentic biological eye movement and gaze kinematics to Skyrim NPCs.
>
> Rather than relying on simple static head-tracking, TrueGaze implements a continuous kinematics engine grounded in peer-reviewed oculomotor research:
> * **Main Sequence Saccades:** Rapid ballistic eye shifts modeled using empirical peak velocity formulas ($V_{peak} = V_{max}(1 - e^{-\theta/c})$), reaching up to 745°/s.
> * **Vestibulo-Ocular Reflex (VOR):** Biological eye-head decoupling where eyes counter-rotate to maintain focus on the player as the NPC's body turns.
> * **Micro-Saccadic Brownian Drift:** Continuous 1–3 Hz physiological ocular tremor (Ornstein-Uhlenbeck process) uniquely seeded per actor so NPCs never look like frozen mannequins.
> * **Argyle & Cook Social Triangle:** Natural scanning patterns (Left Eye → Right Eye → Mouth) during active dialogue.
> * **Zero Script Taint:** No Papyrus scripts, no ESP/ESL plugins. Safe to add or remove anytime.
> * **Web Configurator:** Includes a standalone HTML configurator (`TrueGazeConfig.html`) with an interactive real-time animated pupil preview.
>
> Compatible with Skyrim SE 1.5.97, AE (1.6.640, 1.6.1170), and VR.
>
> Feedback and bug reports are warmly welcomed!
> — Kirk LaSalle
