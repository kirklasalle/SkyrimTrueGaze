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

**Distance behaviour is deliberate:**

| Distance | Tier | What runs |
| :--- | :--- | :--- |
| **< 5 m** | Tier 1 — Dialogue range | Full simulation: saccades, VOR, micro-drift, saccadic blinks |
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
- If the glow sits inside the head, increase `fPupilForwardOffsetCm` (default 7 cm).

> **What this proves:** the visual layer consumes the solver's own state, so a beam that visibly tracks a target is direct evidence that the kinematics are driving bones. It is the cheapest way to confirm Stages 1–2 without squinting at a subtle head turn.

---

## Stage 6 — Broad Multi-Race & Dialogue Field Acceptance (Humanoid & Beast Races)

**Goal:** Verify natural biological eye contact, VOR decoupling, and Social Triangle gaze cycling in third-person camera across diverse humanoid and beast races in peaceful interior/settlement cells.

### Test Cells & Candidate NPCs:

| Race Family | Test Cell | Candidate NPC | Observed Dynamics |
| :--- | :--- | :--- | :--- |
| **Human (Imperial)** | `RiverwoodTrader` | Lucan Valerius | Saccadic fixation, VOR counter-rotation, third-person dialogue camera |
| **Human (Nord)** | `WhiterunBanneredMare` | Hulda / Uthgerd | Argyle & Cook Social Triangle cycling (eye → eye → mouth) |
| **Elf (Bosmer / Dunmer)** | `Riverwood` / `WhiterunDrunkenHuntsman` | Faendal / Jenassa | Elongated craniofacial rig compatibility, socket tracking |
| **Beast (Khajiit)** | Whiterun Exterior Caravan | Kharjo / Ri'saad | Feline craniomandibular rig, wide pupillary axis, neck strain |
| **Beast (Argonian)** | Riften Docks / Market | Madesi / Scouts-Many-Marshes | Extended reptilian snout morphology, horn/ridge stability |

### Procedure:

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

### Pass Criteria:

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
