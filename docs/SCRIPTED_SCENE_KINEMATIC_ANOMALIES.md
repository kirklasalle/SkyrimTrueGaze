# Scripted Scene Kinematic Anomalies & Graceful Handling Guide

**Product:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine  
**Document:** Technical Knowledge Base & Architectural Reference  
**Owner & Architect:** Kirk LaSalle  
**Status:** Living Knowledge Base Specification (Phase 8 / R16)  
**Date:** October 4, 2026  

---

## 1. Executive Purpose & Scope

In vanilla Skyrim, scripted scenes, quest cutscenes, package procedures, and animated furniture rigs frequently place actors into non-standard physical orientations. In these scenarios, the actor's Havok physics capsule root (`actor->GetAngleZ()`) diverges from their visible, animated skeletal mesh.

When TrueGaze applies biological gaze tracking atop these animations, naive mathematical assumptions (such as assuming an actor's chest faces their root `GetAngleZ()`, or assuming vanilla look-at procedure slots reflect who the actor is addressing) produce severe visual defects:
1. **Systematic Yaw Offsets:** The actor's head turns sideways away from conversational partners.
2. **Backward Neck-Snapping:** The actor's head twists beyond anatomical limits (up to 180° backward).
3. **Dialogue Disconnect:** The speaker looks at an unrelated bystander while delivering emotionally charged lines to the player.
4. **Target Jitter & Micro-Flapping:** Rapid flickering between competing vanilla scene slots and autonomous social gaze.

This document establishes the **formal kinematic principles, root causes, mathematical resolutions, and graceful anomaly-handling architecture** to ensure TrueGaze remains visually authentic and resilient across any scripted, vehicle, or furniture scenario in Skyrim.

---

## 2. Landmark Case Study: The Helgen Opening Cart Scene (+90° Yaw Bias)

### 2.1 The Observed Defect
During the Helgen opening sequence (`PrisonerCarriage01`), prisoners ride on wooden benches across a horse-drawn carriage:
- **Dialogue 1:** When Ralof addresses the Player (*"Hey you, you're finally awake"*), Ralof's head rotated ~90° sideways to look directly at Lokir instead of the Player.
- **Dialogue 2:** When Ralof subsequently addressed Lokir (*"You're from Rorikstead, right?"*), his head rotated an additional 90° (now ~180° relative to the vehicle heading), staring backward into empty space behind the carriage bed.

### 2.2 Kinematic Root Cause Analysis
Three compounding coordinate divergences created this phenomenon:

```
[Carriage Road Axis: +Y (North, 0°)]
       │
       ├─► Actor Capsule Root: GetAngleZ() = 0° (Aligned with Carriage)
       │
       └─► Seated Animation (CartIdle):
             Torso Bone (NPC Spine2) Basis: +X (East, +90°)
             (Prisoner sits sideways facing across the cart bed)
```

1. **Coordinate Frame Divergence:**
   - In `GazeEngine::WorldTargetToLocalGaze`, TrueGaze initially computed azimuth deltas (`desiredYaw`) using the formula:
     $$\text{localYaw} = \text{WrapPi}(\text{bearing} - \text{actor}\to\text{GetAngleZ}())$$
   - The Player was seated directly across the cart bed at $(+120, 0)$, producing $\text{bearing} = +90^\circ$.
   - Because `actor->GetAngleZ()` was $0^\circ$, TrueGaze calculated:
     $$\text{desiredYaw} = 90^\circ - 0^\circ = +90^\circ$$
2. **Compounded Animation Transform:**
   - TrueGaze applies `desiredYaw` to the cervical chain (`NPC Spine2`, `NPC Neck`, `NPC Head`).
   - But the torso (`NPC Spine2`) was *already rotated $+90^\circ$* by the seated animation (`CartIdle`).
   - Adding $+90^\circ$ to an already $+90^\circ$ animated bone produced a net orientation of **$+180^\circ$** — pointing straight at Lokir on the side bench.
   - When addressing Lokir (bearing ~$+26^\circ$), adding $+26^\circ$ on top of $+90^\circ$ resulted in $+116^\circ$ — staring behind himself!
3. **Dialogue Target Hijacking:**
   - In `TargetSelector.cpp`, the game's procedure slots (`high->headTrackTarget[kProcedure]`) were aimed at companion NPCs for staging purposes.
   - A preliminary deferral check (`if (anyDirected && directedActor)`) yielded TrueGaze to vanilla *before* checking who was actively speaking, preventing TrueGaze from taking hold during dialogue lines directed to the Player.

---

## 3. Core Architectural Principles for Scripted Scenes

### Principle 1: Torso Decoupling via Spine Forward Vector & Level Pitch Invariant
> **The physics capsule rotation (`actor->GetAngleZ()`) is ground truth for standing actors, while the torso forward vector decouples seated and vehicle postures.**

1. **Standing Humanoids — Level Horizon Pitch Invariant:**
   - Standing actors are vertical in world space. Gaze elevation relative to the world horizon is computed as $\text{atan2}(dz, \text{horizontal})$.
   - Reference pitch (`ref.pitchRad`) **must strictly be $0.0\text{ rad}$**.
   - *Forensic Finding:* Sampling the spine bone's world rotation matrix (`cachedSpine->world.rotate.GetVectorY()`) on standing actors was catastrophic: Bethesda's idle animations tilt the spine bone downward (negative Z), injecting a negative reference pitch. Subtracting this in `WorldTargetToLocalGaze` shifted the local pitch upward by $+30^\circ$ to $+45^\circ$, causing standing NPCs to gaze at the ceiling or sky. Idle hip sway also rotated the reference heading laterally by $20^\circ\text{--}40^\circ$, throwing the player outside the NPC's forward visual cone.
   - Standing actors whose torso is aligned with their capsule ($\text{diff} \le 30^\circ$) strictly use:
     ```cpp
     ref.headingRad = actor->GetAngleZ();
     ref.pitchRad = 0.0f;
     ```

2. **Seated, Vehicle & Cart Postures — Torso Divergence Detection:**
   - In scripted carriage scenes (`PrisonerCarriage01`), actors ride a vehicle rig via scripted idle animations (`CartIdle`). `actor->GetSitSleepState()` returns `kNormal` (0) rather than `kIsSitting`, causing naive sit-checks to fail and collapse back to the vehicle path axis ($0^\circ$).
   - To reliably decouple all seated, vehicle, and furniture postures, TrueGaze evaluates the **Torso Divergence Metric**:
     $$\text{diff} = |\text{WrapPi}(\text{spineHeading} - \text{actor}\to\text{GetAngleZ}())|$$
     where $\text{spineHeading} = \text{atan2}(\text{forward}.x, \text{forward}.y)$ sampled from `cachedSpine->world.rotate.GetVectorY()`.
   - When $\text{diff} > 30^\circ$ ($0.5236\text{ rad}$) — such as the $\sim 90^\circ$ orthogonal seating across the Helgen cart bed — or when `sitState != kNormal` or `GetOccupiedFurniture()` is non-null, TrueGaze automatically adopts the spine's physical horizontal forward facing:
     ```cpp
     ref.headingRad = spineHeading;
     ref.pitchRad = 0.0f; // Level pitch invariant strictly preserved!
     ```
   - This cleanly decouples the chest coordinate frame from vehicle road axes, benches, and chairs without injecting any spine-tilt elevation corruption. Secondary clavicular vectors $(dx = p_R.x - p_L.x, dy = p_R.y - p_L.y)$ serve as fallback if the spine bone is absent.

### Principle 2: Speech Overrules Staged Look Procedures
> **The voice cannot lie. When an actor speaks, their gaze belongs to the listener.**

Bethesda's AI frequently stages the physical look direction (`kProcedure` / `kAction`) independently of line delivery (`VOICE_STATE`).
- `high->voiceState` (`kStart` or `kContinue`) indicates active line delivery.
- `high->lastSpokenToArray` indicates the recipient.
- Active speech to a recipient unconditionally overrides secondary procedure slots and arms the **Dialogue Player Hold timer (3.0s)** to maintain eye contact across mid-line pauses and signal dropouts.

### Principle 3: Cervical Limits are Relative to the Torso, Not the World
> **A human neck cannot twist 180°. Seated actors must never rotate their heads beyond biological limits relative to their chest.**

When an actor is seated or attached to a vehicle, cervical rotation is strictly clamped:
$$\text{desiredYaw} = \text{std::clamp}(\text{desiredYaw}, -70^\circ, +70^\circ)$$
Relative to the torso coordinate frame, this guarantees that even if a sound or procedure attempts to look behind the bench, the actor turns their head gracefully to the anatomical boundary ($\pm 70^\circ$) without snapping their neck backward.

### Principle 4: Elimination of Head Snapping & Discontinuities
> **Bones must never jump or snap across a single frame. All engagement thresholds, mode changes, and scene deferrals must transition smoothly.**

Three discrete step discontinuities were identified and permanently resolved:
1. **Engagement Threshold Discontinuity:**
   - In `BoneController::CalculateHierarchyStrain`, the binary threshold `if (totalMag < headEngageThresh) return dist;` produced an instantaneous $4.2^\circ$ jump on neck and head bones when crossing $12.0^\circ$.
   - *Resolution:* Replaced with a smooth C1 Hermite cubic ease-in (smoothstep) between $0.75 \times \text{thresh}$ and $1.0 \times \text{thresh}$.
2. **CGA Strain Switch Discontinuity:**
   - Switching between `CalculateHierarchyStrain` (head involvement $\sim 0.35$) and `CalculateCgaStrain` (head involvement $0.08$) produced an immediate $6^\circ\text{--}8^\circ$ per-frame jump on the head bone.
   - *Resolution:* Continuously blend between hierarchy strain and CGA strain using `cgaAversionBlend` at an exponential rate of $8.0/\text{s}$ ($\sim 125\text{ ms}$ smooth glide).
3. **Scene Deferral Cut Discontinuity:**
   - Abruptly gating bone updates via `if (!headChainYields)` caused the head chain to snap between vanilla Havok pose and TrueGaze deflection in a single frame.
   - *Resolution:* Smoothly scale head chain deflection by $(1.0 - \text{headChainYieldAlpha})$ at an exponential rate of $8.0/\text{s}$ ($\sim 125\text{ ms}$ glide).

### Principle 5: Conversational Eye Contact Majority
> **The majority of conversational time must be spent looking directly into the target's eyes.**

- Peripheral aversion vertices (`UpperLeftAversion`, `UpperRightAversion`, `LowerLeftAversion`, `LowerRightAversion`) are strictly quarantined to deliberate CGA / THINK mode episodes and removed from everyday social candidate loops.
- Core social triangle scanning is biased $4.0\times$ toward the eyes (`LeftEye` = 4.0, `RightEye` = 4.0, `Mouth` = 1.0), ensuring NPCs maintain foveal eye contact $85\%+$ of the time.

---

## 4. Graceful Anomaly Handling Matrix

The following matrix defines the autonomous graceful recovery mechanisms built into TrueGaze to handle edge cases, missing data, and unusual modded animations:

| Anomaly / Edge Case | Failure Mode Without Guard | TrueGaze Graceful Handling Mechanism | Code Location |
| :--- | :--- | :--- | :--- |
| **Unresolved Spine Bone** (model loading / streaming) | Null pointer dereference / CTD | **Three-Tier Reference Fallback**: 1. `cachedSpine->world.rotate` ➔ 2. `process->GetOccupiedFurniture()->GetAngleZ()` ➔ 3. `actor->GetAngleZ()`. | `GazeEngine.cpp:GetActorReferenceOrientation` |
| **Degenerate Spine Vector** (actor looking straight up/down) | `atan2(0, 0)` singularity, NaN propagation | **Length Threshold Guard**: Requires `forward.x^2 + forward.y^2 >= 0.04f` (~11.5° from vertical) before solving heading; falls back cleanly if degenerate. | `GazeEngine.cpp:GetActorReferenceOrientation` |
| **Point-Blank Colocation** ($\text{dist} < 6\text{ cm}$) | Divide-by-zero, extreme saccadic jumping | **Point-Blank Singularity Gate**: $\text{dist}^2 < 16\text{ units}^2$ zeros yaw/pitch output cleanly. | `GazeEngine.cpp:WorldTargetToLocalGaze` |
| **Close Seated Conversational Partner** ($\text{dist} < 0.9\text{ m}$) | Seated partner outside strict visual cone | **Social Proximity Override**: $\text{dist}^2 < 4096\text{ units}^2$ treats target as inside cone regardless of bearing, preventing false ambient dropouts across tables or carts. | `TargetSelector.cpp:IsInVisualCone` |
| **Extreme Target Angles** ($> 110^\circ$ behind actor) | Grotesque owl-like neck twisting | **Visual Cone Gating + Biomechanical Clamp**: Targets $> 75^\circ$ (or $> 110^\circ$ for hold) rejected; seated yaw clamped to $\pm 70^\circ$, pitch clamped to $\pm 45^\circ$. | `TargetSelector.cpp` & `GazeEngine.cpp` |
| **Speech Pause / Line Dropout** (speech pauses mid-sentence) | Head flickers or snaps to nearby actor | **Dialogue Player Hold (3.0s)**: Latches eye contact on the spoken recipient across short silences until speech concludes. | `TargetSelector.cpp:Section 1c & 2b` |
| **Actor Target Micro-Flapping** (two NPCs at equal distance) | Rapid oscillating head twitching | **Target Fixation Stability (1.5s)**: Dwell hysteresis maintains active lock for at least 1.5s before allowing target switch. | `TargetSelector.cpp:Section 3` |
| **Scripted Cutscene Deferral** (quest requires specific look) | Mod fight: TrueGaze fights vanilla scene | **Per-Frame Scene Defer + Living Eye Glance**: Head chain yields to vanilla; eyes glance naturally at nearby player (`glanceRef` relative to head matrix). | `GazeEngine.cpp:ComputeDeflection` |
| **Corrupted Mod Mesh / Broken Skeleton** | Engine crash / memory access violation | **SEH Fault Isolation**: `__try / __except` blocks wrap per-actor execution; logs structured exception and continues frame safely. | `AnimationHook.cpp:HookedUpdate` |

---

## 5. Verification Checklist for New Scripted Scenes

When auditing or testing other scripted scenes in Skyrim (e.g., Execution at Solitude, Diplomatic Immunity party, High Hrothgar council, Dark Brotherhood sanctuaries, Alternate Start scenarios):

1. **Verify Torso Vector Stability:**
   - Run console command `stgverbose 1` or inspect `stgtrace` JSONL output.
   - Confirm that `desiredYaw` remains stable when the actor transitions from standing to sitting.
2. **Verify Speech Target Alignment:**
   - Confirm the speaking actor looks directly at the intended listener's eyes, not a bystander.
   - Confirm eye contact holds through multi-sentence dialogue without micro-twitching.
3. **Verify Anatomical Bounds:**
   - Confirm the actor's head never exceeds ±70° relative to their chest.
   - Confirm that when addressing someone behind them, the actor turns their head to ±70° and uses ocular deflection for the remaining angle rather than twisting backward.
4. **Verify Scene Deferral & Release:**
   - Confirm that when the scripted scene ends, TrueGaze resumes natural social gaze on the very next frame without requiring a cell transition or reload.
