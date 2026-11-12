# TrueGaze™ — In-Game Test Scenario

**Product:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine
**Owner:** Kirk LaSalle
**Document status:** Test protocol and regression matrix; runtime verification established September 19, 2026
**Applies to:** Skyrim Special Edition / Anniversary Edition, SKSE64

---

## Purpose

This is a **verification protocol**, not a demo script. It exists to answer one question honestly:

> **Does the gaze engine execute correctly and produce perceptible actor attention in a running game?**
>
> The September 19 Skyrim AE run answered the runtime portion: the plugin loaded, actor hooks invoked, eligible actors ticked, targets resolved, skeletons were probed, HCEP state was consumed, and diagnostic emitters attached. This document now governs the next evidence layer: perceptual bone movement, rig coverage, visual illustration, VR, and long-session acceptance.

The protocol is ordered so that each stage isolates one failure mode. **Stop at the first stage that fails and report it.** Do not skip ahead; a later stage cannot be interpreted if an earlier one is broken.

---

## Before you start

### Unreleased incoming-panel candidate: controlled field gate

**Font field result:** the first Whiterun tavern screenshot failed readability;
the next screenshot shows readable target IDs and zero-measurement labels.
This confirms those glyphs, not a positive incoming hit or full UI acceptance.

**Human-description field result:** the latest tavern screenshot displays readable
names and a reported eye-area match from one observer, but long captions overlap.
That is a failed layout gate, not independent proof of anatomical accuracy.

**Grouped-card retest:** close Skyrim before replacing the DLL. Panel labels
must use the observed actor's display name and "No incoming gaze matches detected"
when empty. Obtain a camera-to-NPC match and an NPC-to-player match: descriptions
must identify "Your view" versus the named NPC's gaze, the receiving actor, and
the estimated body/eye/head area or physical front/above-head panel crossing.
The first-person HUD identifies its owner as "You"; it never implies visibility
or attention is proven. "Estimated; visibility not checked" must remain visible.

**Grouped-card field result:** Kirk reports "it worked". The new tavern screenshot
shows four readable, separated, dark-backed cards, two with reported incoming
eye-area estimates, and explicit overflow: 4/5 cards placed, one needs inspection.
Native card output passes this limited scene check. Inspection paging, actual
anatomical/visibility accuracy and timing budgets are not proven by this image.
The overlays remain visible over the radial Skills/Magic/Items/Map menu; test
menu-aware visibility separately before passing full gameplay presentation.

One observer intersecting anatomy and a panel must count as **one observer** but
multiple measurements. Two observers must count as two, and two identically named
actors must have distinguishing IDs. Move away and verify matches/names clear or
update rather than remaining stale. Each receiver has one dark-backed card even
with both A/B panels enabled. Names head the card; compact colored rows group an
observer's anatomical estimate and panel crossings without counting twice.
`o`/`+` remain at the original projected/mapped positions, not on a displaced card.
Leaders must connect cards to the receiving actor; they are not gaze rays.

Toggle `stgintext` to open the complete non-modal live inspection panel. Technical
IDs, full names/sentences, coordinates, measurement types and quality remain
available there. `stginnext`/`stginprev` page through wrapped text, then observations,
including offscreen and unplaceable cards. Verify wraparound in both directions,
selected-identity stability and text-page access for long entries. Console paging
must work after closing the console without intercepting gameplay input.

### Card layout and truth acceptance matrix

**Near-face B retest (November 12):** use `stgoff`, `stgpanel`, `stgintext` and
`stgstatus` to confirm 10 cm forward clearance. The size stays 27.5 world units.
Compare against a backed-up 70 cm configuration at the same camera/actor pose,
then restore 10 cm and `stgreload`. Test frontal/oblique viewing, head yaw/pitch,
seated/standing actors, helmets, child/adult scales and representative creatures.
Verify quad markers lie on the visible panel and do not clip faces/helmets;
changing clearance must not change anatomical results for identical source/pose.
Keep text-card collision protection unchanged. Panel-region aiming is a diagram
test, not a calibrated facial-surface test. Physical hits have no anatomical
region assignment. Record false hits/misses and clipping rather than declaring
improved accuracy from proximity alone.

Run a fresh `stgstatus` after each transition; startup console scrollback can
legitimately report SUPPRESSED during loading. The November 12 images show a
populated inspector, but do not certify every entry/page or menu restoration.
Capture a full steady-state timing window; the recorded 655 us engine sample
and 100,504 us session peak are over the declared 150 us budget, not acceptance.
See [command card](CONSOLE_COMMANDS_CARD.md) and
[configuration audit](CONFIGURATION_AUDIT.md).

**Development-only menu retest:** enable the desired diagnostics from the console.
Open the radial menu, then inventory/magic/skills/map and return through nested
menus: no cards, inspection, leaders, markers, rays, panel meshes or diagnostic
lights should remain visible. Test journal, container, barter, crafting and book
menus, and loading/main-menu transitions separately. Check the log for menu
suppression registration or cleanup errors.

Return to gameplay: only previously enabled channels should reappear on a fresh
frame, without stale observations or stranded emitters. Toggle diagnostics OFF
from the console while menus are involved; they must stay OFF afterward. With
diagnostics already OFF, opening/closing a menu must never turn them on.
`stgstatus` reports SUPPRESSED/GAMEPLAY separately from saved toggle values.
Console inspection navigation and ordinary dialogue must remain usable.
Compare the live INI before/after menu transitions to confirm no implicit writes.
Record transition/cold timing separately from steady-state UI cost. Mathematical
menu-generation tests do not prove native task/scene cleanup latency. The log
records queue delay and cleanup CPU time when 3D diagnostics are removed; this
work is outside the active-frame finalization/UI timing windows.

| Case | Required evidence / pass condition |
| --- | --- |
| Small scene, both A/B enabled | One card per receiving actor; distinct observers match fixtures; separate physical/anatomical measurements retained |
| Crowded scene (up to 64 receivers) | Zero card-to-card overlaps and zero overlaps with protected template areas; every unplaceable card explicitly counted and accessible in inspection |
| Long and duplicate display names | Native measured wrapping, distinguishing IDs, no lost name/observer/measurement text; oversized entries accessible by inspection text pages |
| Bright/dark interiors and supported resolutions | Readable heading/body/footer and colored rows on backings; inspect near all viewport edges |
| Camera motion / first-third-person transitions | Stable placement where space permits; reflow remains bounded and non-overlapping; correct "Your view"/"You" ownership |
| Crosshair, dialogue/interaction and lower HUD | Protected template areas remain clear; test actual native HUD and any installed UI mods separately |
| Unload, save/load, empty/disabled/faulted frames | No stale cards, names, leaders or markers; missing inspection selection updates safely |
| Known observer/receiver geometry | Compare incoming IDs/coordinates to independently established rig/eye landmarks and ray directions; a rendered `o` is only a diagram mapping, not ground truth |
| Obstacle / out-of-range / boundary cases | Record false-positive/negative evidence separately; unchecked visibility must remain stated, never reported as confirmed clear |
| CPU/resource budget | Capture active sample count, median/p95/window peak, session-wide peaks, placed/overflow cards and visible fields; no performance certification from a screenshot or unit fixture |

For each stable crowd/camera/inspection case, collect a full new window of 256
active-frame samples with `stgstatus`. Hidden/paused UI snapshots and repeated
frame tokens do not populate the UI window. Capture cold-start/session peaks
separately from steady-state window peaks; record resolution, actor/ray counts,
enabled channels and log evidence. Finalization excludes earlier source collection;
UI timing excludes GPU time. Record total game-frame performance separately.
Numerical CPU budgets must be agreed from baseline evidence before a budget pass
is claimed. Mathematical fixture timings are not a substitute for native GFx.

Head boxes and protected UI regions are conservative templates, not proven
measurements of every facial silhouette or third-party HUD. Native font wrapping,
inspection scrolling, visual contrast and all actual field layouts remain gates.
No obstacle raycast or mesh-surface certification was added by this card change.

Historical self-panel calibration is superseded by the
[receiving contract](INCOMING_GAZE_PANEL_DESIGN.md). The current candidate has
passed automated/staged checks and was deployed with Kirk's explicit approval,
without launching Skyrim. Original DLL/INI/assets have a verified rollback backup.
The existing user INI is preserved except the explicitly requested front-panel
clearance during the close-panel install. Missing new keys use compiled defaults.

Use a disposable/previously backed-up save and one or two humanoid NPCs first.

1. Launch through `LaunchTrueGaze.bat` or SKSE. Run `stgstatus` and confirm the
   incoming switches/counters appear. Normal movement, crosshair and console
   input must remain usable. Stop and inspect the log if the UI does not load.
2. Use `stgoff` to establish a neutral baseline, then `stgpanel` for front panel
   B only. The label must identify the **observed** actor, never report its owner's
   outgoing cognitive region as an incoming result.
3. Look at NPC B using the camera in third person: the camera observation belongs
   on **B's** panel, never the player's panel. `o` is mapped approximate anatomy;
   `+` is a separate actual quad intersection. A physical panel miss is allowed.
4. Have an NPC face/look at the player. The incoming observation belongs to the
   **player**. Enable `stgpanela`, `stgintext` and `stghud` independently; switch
   into first person to inspect the player HUD. HUD must not appear solely
   because free camera is active.
5. Toggle `stginanatomy` and `stginpanels` separately, checking states with
   `stgstatus`. Moving/changing a panel must not alter anatomical intersections
   or the biological gaze. `stgv` retains the existing combined ray/front view
   toggle; `stgon` enables every channel for a small-scene diagnostic.
6. Exercise reciprocal NPC-to-NPC gaze, two coincident observers, aversion into
   empty space, camera-mode transitions, unloading/reloading a target, and
   save/load. Stale markers must disappear; identifiers must remain distinguishable.
7. Capture a screenshot, `TrueGaze.log`, optional `stgtrace` output, and the
   incoming finalization/UI cost and panel/label counts from `stgstatus`.

Do not treat approximate volumes as visible mesh hits: **occlusion is unverified**.
Detailed creature eye profiles, obstacles, crowd median/p95/peak and SE/AE/VR
presentation are subsequent separate gates, not passes from this humanoid smoke
test. Stop on the first failure and do not overwrite the only good save.

**Back up a save.** The plugin calls `ReleaseBones()` on save specifically so a procedural rotation is never baked into a save file, but that path has never been exercised against a real save. Do not test on your only save.

**Use a clean, low-risk save.** Ideally a new game or a save inside a small interior with one or two NPCs (e.g. Breezehome, or `coc Riverwood` from the main menu). Crowded cities make it harder to attribute an effect to a specific actor.

**Run the automated pre-flight first.**

```powershell
.\scripts\Test-TrueGazeHealth.ps1
```

It checks the game version, SKSE build match, Address Library version, the DLL's SKSE loader contract, the deployed binary hash and the log path — all in about a second. If it reports failures, **fix them before launching.** It will not tell you the gaze works; it tells you whether the gaze *can* work.

---

## Stage 0 — Prove it loads *(runtime dormant)*

**Goal:** Confirm the plugin loads and installs its hook, with zero risk of uncalibrated runtime behavior.

**Setup:**

```powershell
.\scripts\Deploy-TrueGaze.ps1 -LoadOnly
```

This builds, deploys, and sets `bEnableTrueGaze=false` in the *deployed* copy of `TrueGaze.ini`.

**Procedure:**

1. Launch via `Deploy-TrueGaze.ps1` (which starts `skse64_loader.exe`), or launch `skse64_loader.exe` yourself.
2. Load your save. Stand still for ~10 seconds.
3. Quit the game normally.

**Pass criteria** — `TrueGaze.log` contains:

| Line | Meaning |
| :--- | :--- |
| `Loading True Gaze v1.0.0` | SKSE loaded the plugin |
| `SKSE plugin loaded successfully.` | Messaging listener bound |
| `Game data loaded. Initialising gaze engine.` | Reached `kDataLoaded` |
| **`Gaze driver installed.`** | **The hook is live** |

And the game **did not crash**.

**If this fails,** the problem is loading — not the gaze. Look at `skse64.log` alongside `TrueGaze.log`. A missing log file means SKSE never loaded the plugin at all (check the pre-flight result).

> **Why this stage matters:** `Gaze driver installed.` is the single most valuable line in the log. It proves the vtable hook on `Actor::Update` (slot `0xAD`) was written successfully. An earlier revision of this code hooked a **nonexistent** slot and would have corrupted unrelated vtable entries — this stage is what catches that class of defect before it reaches a save.

---

## Stage 1 — Prove the bones resolve *(the critical unknown)*

**Goal:** Confirm the engine finds the skeleton nodes it needs.

This is **the most likely point of silent failure in the entire engine.** Bone names are matched as strings against the live skeleton:

```cpp
"NPC Spine2 [Spine2]"     "NPC Neck [Neck]"     "NPC Head [Head]"
"NPC L Eye [LEye]"        "NPC R Eye [REye]"
```

The candidate lists are now confirmed on the tested player rig for spine, neck, and head. Vanilla humanoid eye nodes were absent, which is an expected rig limitation because many eyes are FaceGen-driven. If the head candidates miss on another rig, the engine can still run while rotating nothing; therefore a rig matrix remains required.

**Setup:**

```powershell
.\scripts\Deploy-TrueGaze.ps1
```

This sets `bEnableTrueGaze=true`.

**Procedure:**

1. Launch the game and load the save.
2. Stand **within 5 m** of a living humanoid NPC. Face them.
3. Wait ~5 seconds (the probe logs once per actor on first tick).
4. Quit.

**Pass criteria** — the log contains a probe line:

```
[TrueGaze] Skeleton probe for 0001A2B3: spine=yes neck=yes head=yes eyeL=yes eyeR=yes (5 of 5 resolved)
```

| Result | Meaning | Action |
| :--- | :--- | :--- |
| `5 of 5 resolved` | Ideal on a rig with explicit eye nodes. Proceed to Stage 2. | — |
| `head=yes`, eyes `NO` | Expected on many vanilla humanoids; head and geometric pupil-origin paths remain available. | Record the rig and validate perceptual quality. |
| **`head=NO`** | **Blocking.** Gaze cannot be visible. | Stop. Report the actor's race and any skeleton mods; the candidate list needs extending. |
| No probe line at all | No eligible actor was ticked. | Check the actor was living, non-ragdolled, within 15 m, and had `Get3D()` loaded. See "Troubleshooting". |

> **Note:** a `WARN` line reading `No head bone found for <id>` means the probe ran and the head bone missed. This is a *fail*, not a caution — treat it as blocking.

---

## Stage 2 — Prove the gaze is visible *(the headline test)*

**Goal:** Watch an NPC look at you.

**Setup:** as Stage 1 — `bEnableTrueGaze=true`, plugin deployed.

**Procedure:**

1. Load the save. Stand **within 3–5 m** of a humanoid NPC, in their forward field of view.
2. Move **side to side** slowly, staying in front of them.
3. Observe the NPC's **head** and **eyes**.
4. Then walk **past** them and behind; observe whether they continue to track.
5. Step back beyond ~16 m and confirm the head returns to its normal animation.

**Pass criteria:**

| Observation | Expected |
| :--- | :--- |
| Head rotates to keep you in view | ✅ **Runtime acceptance target.** |
| Eyes lead the head on a *large* change of position | Acceptance target where the rig exposes usable eye nodes; otherwise validate the documented geometric fallback. |
| Sub-degree eye flicker while fixing on you | ✅ Micro-saccadic drift (Tier 1 only, under 5 m). |
| Head stops tracking beyond ~16 m | ✅ LOD culling. This is correct, not a bug. |
| **Nothing moves at all** | ❌ See "Troubleshooting". |

**Important — the target is *you*, not arbitrary NPCs.** `TargetSelector` resolves, in priority order: your active dialogue partner → a combat target → **the player within 8 m**. NPCs do not currently gaze at each other. If you are looking for two NPCs to make eye contact, that is not implemented — expect NPCs to look at *you*.

> ✅ **Superseded (September 25, 2026 — Phase R11):** NPC-to-NPC gaze IS now
> implemented and in-engine verified. NPCs look directly into each other's eyes
> (social candidate scan includes all nearby actors; the player competes fairly
> with a salience bias). The paragraph above described the pre-R11 behaviour and
> is retained as historical context for older builds.

**Distance behaviour is deliberate:**

| Distance | Tier | What runs |
| :--- | :--- | :--- |
| **< 5 m** | Tier 1 — Dialogue range | Full biological kinematics: saccades, VOR, micro-drift, saccadic blinks |
| **5–15 m** | Tier 2 — Proximity | Head & neck kinematics; micro-drift disabled |
| **> 15 m** | Tier 3 — Culled | Nothing. Game's own LOD takes over. |

So **stand close.** At 8 m the head still turns but the fine eye movement is intentionally switched off.

---

## Stage 3 — Social triangle *(dialogue)*

**Goal:** Verify eye-scan behaviour during conversation.

**Procedure:**

1. Initiate dialogue with any NPC (within ~2 m).
2. Watch the NPC's **eyes** during the conversation.

**Pass criteria:** the gaze cycles between eye → eye → mouth rather than locking rigidly on one point. This is the Argyle & Cook social-triangle pattern.

**If it does not cycle:** confirm `bEnableSocialTriangle=true` in the deployed INI. This path only activates while the dialogue menu is open.

---

## Stage 4 — Performance and stability

**Goal:** Confirm the engine does not cost frames or crash under load.

**Procedure:**

1. Load into a crowded area — Whiterun market or the Bannered Mare — ideally at night with many NPCs.
2. Play normally for **5–10 minutes**, including at least one combat and one dialogue.
3. Quit.

**Pass criteria:**

| Check | Expected |
| :--- | :--- |
| No crash to desktop | Required |
| No `Frame budget exceeded` warnings, or very few | See below |
| No `Gaze tick threw` lines | Required — any occurrence is a defect |

**On frame budget warnings:** the engine logs when a frame's total gaze cost exceeds its budget. Occasional warnings in a dense scene are tolerable. Sustained warnings mean the per-actor cost is too high and the LOD thresholds need tightening.

**On `Gaze tick threw`:** the tick is wrapped in `try/catch`, so the guard caught a C++ exception and skipped that frame. The game survived, but something is genuinely wrong. Report the message text.

> ⚠️ **Honest limitation:** the guard catches C++ exceptions only, **not access violations (SEH)**. A bad bone pointer or null dereference will still terminate the process. If the game hard-crashes with no log entry, that is the likely cause — check the Windows Event Log for the faulting module.

---

## Stage 5 — In-game visual layer *(optional, developer diagnostic)*

**Goal:** See the solved gaze directly, and confirm the visual matches what the kinematics computed.

This stage is **independent of Stages 1–4** and can be run first or last. It is off by default and requires editing the INI, so skip it for a normal play-test.

**Setup** — set these in `Data\SKSE\Plugins\TrueGaze.ini` (or via `TrueGazeConfig.html` → *In-Game Visuals*):

```ini
[Visuals]
bEnableInGameVisuals=true
bGazeRaysEnabled=true
iRayRenderMode=1          ; Light only - needs no art assets
bGazeRaysTerminus=true    ; show where the gaze lands
fGazeRayLengthMeters=10.000000
```

The INI is read at startup, so **restart the game** after changing it.

**Procedure:**

1. Launch and load into an interior with a few NPCs. Stand 3–5 m away from one.
2. Watch the NPC's head and eyes. In third person, watch your own character too.
3. Walk around the NPC and let them turn to follow you.
4. If you use a custom skeleton (XP32/XPMSSE), note whether the glow sits in the eye socket without adjustment.

**Pass criteria:**

| Check | Expected |
| :--- | :--- |
| A gold glow appears at the eyes of nearby NPCs | Required (colour is `iGazeRayColour`, default TrueGaze gold) |
| The glow tracks the head when the NPC turns | Required |
| With `bGazeRaysTerminus=true`, a second dimmer glow marks the gaze target | Required |
| The glow sits **in the socket**, not in front of or behind the face | Tune `fPupilForwardOffsetCm` / `fPupilUpOffsetCm` |
| Disabling `bGazeRaysEnabled` removes the glow within a frame or two | Required |
| No crash, no new `Gaze tick threw` lines | Required |
| `iRayRenderMode=2` logs a warning and draws nothing *(expected until V2)* | Expected |

**If the beams do not appear:**

- Check the log for the `Visual tuning:` line. It prints every visual setting, so a typo shows up immediately.
- `mode=2` draws nothing at all until the branded assets exist — use mode `1`.
- If the glow is invisible in daylight but obvious indoors, it is a point light and competes with daylight. That is expected; lower or raise `fGazeRayOpacity`.
- If the fallback glow sits inside the head, check `fPupilForwardOffsetCm` (default 10.8 cm); FaceGen eye centers take precedence.

> **What this proves:** the visual layer consumes the solver's own state, so a beam that visibly tracks a target is direct evidence that the kinematics are driving bones. It is the cheapest way to confirm Stages 1–2 without squinting at a subtle head turn.

---

## Stage 6 — Broad Multi-Race & Dialogue Field Acceptance (Humanoid & Beast Races)

**Goal:** Verify natural biological eye contact, VOR decoupling, and Social Triangle gaze cycling in third-person camera across diverse humanoid and beast races in peaceful interior/settlement cells.

### Test Cells & Candidate NPCs

| Race Family | Test Cell | Candidate NPC | Observed Dynamics |
| :--- | :--- | :--- | :--- |
| **Human (Imperial)** | `RiverwoodTrader` | Lucan Valerius | Saccadic fixation, VOR counter-rotation, third-person dialogue camera |
| **Human (Nord)** | `WhiterunBanneredMare` | Hulda / Uthgerd | Argyle & Cook Social Triangle cycling (eye → eye → mouth) |
| **Elf (Bosmer / Dunmer)** | `Riverwood` / `WhiterunDrunkenHuntsman` | Faendal / Jenassa | Elongated craniofacial rig compatibility, socket tracking |
| **Beast (Khajiit)** | Whiterun Exterior Caravan | Kharjo / Ri'saad | Feline craniomandibular rig, wide pupillary axis, neck strain |
| **Beast (Argonian)** | Riften Docks / Market | Madesi / Scouts-Many-Marshes | Extended reptilian snout morphology, horn/ridge stability |

### Procedure

1. **Third-Person Dialogue Check:**
   - Stand in front of Lucan Valerius or Hulda in third-person camera (`F` or mouse wheel zoom).
   - Initiate conversation.
   - Observe third-person mutual gaze: NPC fixes gaze upon player's facial landmarks rather than staring rigidly forward or snapping neck unnaturally.
2. **Social Triangle Verification:**
   - Keep dialogue menu open for 10–15 seconds.
   - Confirm NPC gaze cycles periodically (Left Eye → Right Eye → Mouth) at ~0.35s intervals without head jitter.
3. **Crosshair Sweet-Spot Test:**
   - Exit dialogue. Stand 2–4 meters away.
   - Sweep crosshair onto NPC's head: NPC glances up to establish mutual eye contact within angular tolerance.
   - Sweep crosshair away: NPC returns to ambient attention.
4. **Beast Race Craniomandibular Check (Khajiit / Argonian):**
   - Approach a Khajiit or Argonian NPC.
   - Verify head/neck rotation respects biological angular limits (yaw ≤ 45°, pitch ≤ 35°).
   - Confirm no skeletal distortion, FaceGen mesh tearing, or neck twist artifacts.

### Pass Criteria

| Check | Humanoid (Nord/Elf) | Beast (Khajiit/Argonian) |
| :--- | :--- | :--- |
| Third-person mutual gaze | Natural, organic | Natural, organic |
| Social Triangle cycling | Active in dialogue | Active in dialogue |
| Bone strain distribution | Spine2 10%, Neck 25%, Head 65% | Proportional cervical damping |
| Havok / Rig stability | Zero physics glitches | Zero snout/jaw distortion |

---

## Final analysis

```powershell
.\scripts\Deploy-TrueGaze.ps1 -PostRun
```

This parses the log and reports which markers were reached, the skeleton probe result, and any faults. It is the fastest way to produce a report.

**What to send if it does not work:**

1. Output of `.\scripts\Deploy-TrueGaze.ps1 -PostRun`
2. `TrueGaze.log` (whole file)
3. `skse64.log` from the same folder
4. A one-line description of exactly what you did and what you saw

---

## Troubleshooting

| Symptom | Likely cause | Check |
| :--- | :--- | :--- |
| No `TrueGaze.log` at all | SKSE never loaded the plugin | Launched via `SkyrimSE.exe` or Steam instead of `skse64_loader.exe`? Run the pre-flight. |
| Log stops after `Loading True Gaze` | Crash during `SKSE::Init` | Version mismatch between SKSE and the game. Pre-flight checks this. |
| Log has no `Gaze driver installed.` | Hook install failed | `Actor::Update` slot may differ for this game build. Report the game version. |
| `Gaze driver installed.` but nothing moves | **Bone names did not resolve** | Look for the `Skeleton probe` line. This is the most likely cause. |
| Probe shows `head=NO` | Bone-name mismatch on this rig | Report the actor's race and any skeleton/body mods in use. |
| No `Skeleton probe` line | No eligible actor was ticked | Actor must be living, conscious, non-ragdolled, with 3D loaded, within 15 m. |
| Head moves but eyes look dead | Eye bones unresolved, or beyond 5 m | Stand closer than 5 m; check the probe line's `eyeL`/`eyeR`. |
| Head tracking is subtle | Working as designed | Total head deflection is capped (`HEAD_YAW_LIMIT = 45°`) and the chain is damped. It is not a snap-look. |

---

## What this protocol does *not* cover

Stated plainly so a passing run is not mistaken for a finished product:

| Not tested here | Why |
| :--- | :--- |
| **OAR conditions** | Dynamic SKSE messaging hook is live; verified when OAR is active in session. |
| **Eyelid morphs (EFM)** | Implemented via `SetExpressionOverride` (2026-09-14) but not yet observed in-engine. |
| **HCEP desktop bridge** | Requires the desktop suite running and listening on the named pipe. Out of scope for a gaze test. |
| **Eye-lead *latency*** | The eyes lead in *magnitude* but not yet in *time*. The 20–30 ms biological latency gap is not modelled. |
| **Creature/gaze aversion modes** | Mode 4 (THINK) only arrives from the HCEP bridge; without it, aversion never triggers. |

A passing run promotes the **gaze** rows of `STATUS.md` to ✅ In-engine verified. It does not promote any of the above.

---

## Recording the result

Whatever happens, record it. A failure documented precisely is worth more than a vague success.

| Field | Value |
| :--- | :--- |
| Date | |
| Game version | |
| SKSE version | |
| Stage reached | 0 / 1 / 2 / 3 / 4 / 5 / 6 |
| Skeleton probe result | |
| Verdict | |
| Notes | |

---

*Part of TrueGaze™, an HCEP product by Kirk LaSalle.*
