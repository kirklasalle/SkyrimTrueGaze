# HCEP META-CONTROLLER ARCHITECTURE
## Narrative-Aware Gaze Modulation & Scripted Scene Integration

**Author:** Kirk LaSalle  
**Co-Author:** Antigravity AI (Pair Programmer)  
**Date:** September 19, 2026  
**Document Version:** 1.0.0-PROD  
**Classification:** Proprietary & Core Architectural Specification  
**Governing Authority:** Permanent Active Directives (PAD), Agentic Prime Directive, Sacred Covenant  
**Target Runtimes:** Skyrim Special Edition / Anniversary Edition (1.7.104.0+), Skyrim VR, HCEP Desktop Suite (.NET 9)  

---

## 1. Executive Summary & Vision

### 1.1 The Core Philosophy: Meta-Controller vs. Brute Override
In conventional game animation and AI systems, dynamic gaze plugins and scripted narrative sequences are in perpetual conflict:
- **The Scripted Problem:** A bespoke narrative scene (such as Skyrim’s iconic opening cart ride `MQ101`, execution at Helgen, or council meetings) establishes strict actor attention vectors through quest scenes (`BGSScene`), dialogue packages, and look-at targets.
- **The Naive Mod Failure:** A standard autonomous gaze system treats all actors uniformly, forcibly overriding bone rotations to look at the player or arbitrary world points. This breaks emotional continuity, causes severe immersion rupture (e.g., an actor delivering a solemn line to an ally while uncontrollably staring at the player's chest), and in physics-constrained scenes like the Helgen cart, risks catastrophic Havok physics instability.

**The Solution — The HCEP Meta-Controller:**
The Human Communication Eye Protocol (HCEP) does not usurp the narrative director; **it serves as a Meta-Controller that modulates how the narrative target is perceived and engaged.** 
* The **Skyrim Engine / Script** defines the **Macro Target** (who or what the actor is engaging with to advance the story).
* The **HCEP Meta-Controller** defines the **Micro-Kinematics & Biological Dynamics** (dwell durations, social-triangle saccades, stress-induced micro-jitter, conversational gaze aversion, pupil convergence, and blink synchronization).

```
┌────────────────────────────────────────────────────────────────────────┐
│                   SKYRIM NARRATIVE & SCRIPT ENGINE                     │
│    (Quest Scenes, Dialogue Packages, Actor Look-At, Furniture Binds)   │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Macro Narrative Intent:
                                    │ "Actor A engages Actor B"
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                      HCEP META-CONTROLLER PLANE                        │
│   ├── Scene Sentiment & Actor Emotion Parsing (Anger, Fear, Neutral)   │
│   ├── Cognitive Mode Modulation (Logic, Affect, Spirit, Heart, Think)   │
│   ├── Real-World Player Telemetry (Live Kinect Gaze, Face Pose, Blink)  │
│   └── Biomechanical Expression Formulation                             │
└───────────────────────────────────┬────────────────────────────────────┘
                                    │ Modulated Biological Dynamics:
                                    │ (Social Triangle, Micro-Saccades,
                                    │  Dwell Distributions, Aversion)
                                    ▼
┌────────────────────────────────────────────────────────────────────────┐
│                    TRUEGAZE™ KINEMATICS RUNTIME                        │
│   ├── Main Sequence Velocity Profile (Vmax ∝ Amplitude)                │
│   ├── Vestibulo-Ocular Reflex (VOR) Decoupling                         │
│   ├── Cervical-Cranial Strain Distribution (Spine -> Neck -> Head)     │
│   └── Additive Transform Injection & Zero-Taint Bone Withdrawal        │
└────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Opening Scene Case Study: The Helgen Cart Ride (`MQ101`)

### 2.1 The Environmental & Mechanical Constraints
Skyrim’s intro sequence (`MQ101`) is historically the most fragile script in modded gaming:
1. **Furniture/Cart Binding:** Actors (Player, Ralof, Ulfric Stormcloak, Lokir of Rorikstead) are attached to moving cart furniture references via hardpoint links.
2. **Physics Solvers:** The carts are multi-jointed Havok rigid bodies drawn along spline paths by horses with complex collision volumes.
3. **Rigid Package Locks:** All four characters operate under locked AI packages with player control disabled (`DisablePlayerControls(abMovement=true, abFighting=true, abCamSwitch=true)`).

### 2.2 TrueGaze Engine Guarantees During `MQ101`
To operate safely in this environment, TrueGaze enforces the following mathematical and architectural constraints:
* **Zero Translation Touch Policy:** TrueGaze modifies *only* rotational quaternions/matrices for cervical and ocular nodes (`NPC Head [Head]`, `NPC Neck [Neck]`, and geometric eye origins). It **never** modifies the root bone (`NPC Root [Root]`), pelvis, or position vector ($T_{x,y,z}$). The actor’s spatial attachment to the cart furniture remains mathematically invariant.
* **Additive Delta Layering:**
  $$\mathbf{R}_{\text{final}} = \mathbf{R}_{\text{anim}} \times \Delta\mathbf{R}_{\text{HCEP}}$$
  The rotation is computed as a localized deflection relative to the current keyframed animation. Before the next animation evaluation tick, the deflection is completely withdrawn, preventing any cumulative rotational drift.
* **Cervical Strain Clamping:**
  While seated on the cart bench, an actor cannot realistically rotate their neck 180 degrees. TrueGaze applies a non-linear strain clamp:
  $$\theta_{\text{neck, effective}} = \theta_{\text{max}} \cdot \tanh\left(\frac{\theta_{\text{target}}}{\theta_{\text{max}}}\right)$$
  ensuring no actor exhibits unnatural or bone-snapping head orientations.

### 2.3 Player Gaze Immersion While Physically Bound
While the player character’s hands and body are bound to the cart bench, **the player’s real-world consciousness in front of the Kinect is unrestrained**.
* As Kirk looks at Ralof, Ralof enters the player's real-time gaze cone.
* TrueGaze's `PlayerGazeResolver` translates Kirk's real-world pitch/yaw into the game world, allowing the 3rd-person player character’s head and eyes to mirror Kirk’s natural exploration of the cart, the mountains, and fellow prisoners.
* If Kirk holds eye contact with Ralof, the mutual gaze angle drops below the threshold ($\le 7.5^\circ$), triggering reciprocal awareness in Ralof.

---

## 3. The Multi-Tiered Control Architecture

### Tier 0: Narrative Authority (Skyrim Engine)
The native game engine retains authority over:
* Quest scene staging (`BGSScene`).
* Dialogue execution and phoneme lip-syncing.
* Target assignment for scripted events (e.g., Lokir looking at the Imperial Captain when she speaks).

### Tier 1: HCEP Cognitive & Atmospheric Meta-Controller
The Meta-Controller evaluates the narrative context and assigns appropriate cognitive/emotional modulation:
* **Scene Sentiment Extraction:** Ingests actor emotion states from the engine:
  ```cpp
  RE::Actor::ExpressionType expr = actor->GetExpression();
  // Neutral (0), Anger (1), Fear (2), Happy (3), Sad (4), Surprise (5), Puzzled (6), Disgusted (7)
  ```
* **Cognitive Mode Formulation:** Translates scene sentiment into the 5 core HCEP modes:
  1. **LOGIC (Mode 0):** Objective, cold, analytical assessment. Characterized by stable eye vectors, minimal emotional jitter, and fixed focus.
  2. **AFFECT (Mode 1):** Emotionally driven, expressive engagement. Rapid micro-saccades, reactive social-triangle scanning.
  3. **SPIRIT (Mode 2):** Transcendent, solemn, or defiant focus. Sustained ocular lock, low blink frequency, dignified posture.
  4. **HEART (Mode 3):** Empathetic, warm, comradely connection. Gentle saccadic velocity, soft social-triangle transitions, synchronized blinking.
  5. **THINK (Mode 4):** Internal cognitive processing. Frequent micro-aversions (gaze drifts up-and-away or down-and-left during cognitive recall), delayed focal snap.

### Tier 2: Biomechanical Kinematics Engine (TrueGaze C++)
Transforms the HCEP cognitive mode into physical skeletal deflections:
* **Main Sequence Saccades:** Saccadic duration and peak angular velocity obey physiological scaling:
  $$V_{\text{max}} = \frac{V_0 \cdot A}{A_0 + A}$$
  where $A$ is the saccadic amplitude in degrees, $V_0 \approx 500^\circ/\text{s}$, and $A_0 \approx 10^\circ$.
* **Social Triangle Generator:** During dialogue, the gaze point does not stare blindly at a single coordinate. It traverses an equilateral triangle bounding the interlocutor’s face:
  - Left Eye vertex ($V_1$) $\longleftrightarrow$ Right Eye vertex ($V_2$) $\longleftrightarrow$ Mouth/Phoneme vertex ($V_3$).
  - Dwell times at each vertex are stochastically sampled based on HCEP Mode (e.g., `Heart` mode spends 70% on eyes; `Logic` mode spends more time assessing mouth movement and facial asymmetry).
* **Conversational Gaze Aversion:** Humans do not maintain 100% continuous eye contact; doing so is perceived as predatory or psychotic. HCEP injects natural cognitive aversions:
  - Holding eye contact for $1.8 - 3.2\text{ s}$.
  - Breaking eye contact for $0.35 - 0.7\text{ s}$ along a diagonal quadrant.
  - Returning smoothly with an anticipatory eye lead followed by head alignment.

### Tier 3: Visual & Audio Synchronization
* **Blink Coordination:** Eye blinks are triggered dynamically during the peak velocity phase of large saccades ($> 15^\circ$), hiding saccadic blur just as the human brain suppresses visual processing during rapid eye motion (saccadic masking).
* **Micro-Jitter & Physiological Drift:** Sub-degree Brownian drift ($0.15^\circ - 0.35^\circ$) is continuously injected to simulate physiological tremor, preventing actors from looking like rigid mannequins even when holding a fixed gaze.

---

## 4. Narrative Archetype Profiles (Helgen Sequence Demonstration)

| Character | Dominant Emotion | HCEP Mode | Mean Dwell | Jitter Freq | Saccade Style | Behavioral Description |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Lokir of Rorikstead** | `Fear` (Intense) | `Affect` (Panicked) | $150 - 250\text{ ms}$ | High ($12\text{ Hz}$) | Hyper-saccadic | Darting eyes, rapid nervous scanning of guards, unable to sustain eye contact for $>0.5\text{ s}$, frequent stress-blinking. |
| **Ulfric Stormcloak** | `Anger` / Gagged | `Spirit` / `Logic` | $900 - 1400\text{ ms}$ | Very Low ($2\text{ Hz}$) | Heavy, deliberate | Unwavering, stoic stare. Slow head turns, intense sustained ocular fixation on Tullius and the player. Minimal aversions. |
| **Ralof of Riverwood** | `Neutral` / Comradely | `Heart` / `Affect` | $500 - 750\text{ ms}$ | Balanced ($4\text{ Hz}$) | Conversational | Active social-triangle scanning between player's eyes and mouth. Natural cognitive aversions when reminiscing about Sovngarde. |
| **Imperial Captain** | `Anger` / Disdain | `Logic` (Authoritative) | $700 - 1100\text{ ms}$ | Low ($3\text{ Hz}$) | Rigid, sharp | Downward-angled gaze, assessing prisoners with detachment. Sharp, abrupt saccadic target acquisition. |

---

## 5. Telemetry & IPC Expansion Specification

To allow the HCEP Desktop Suite to intelligently modulate narrative scenes, the bi-directional Named Pipe protocol (`\\.\pipe\TrueGazeBridge`) is expanded with rich contextual feedback.

### 5.1 Extended Skyrim Feedback Packet (`SkyrimFeedbackPacket_v110`)
Total size: **32 bytes** (retaining strict 8-byte alignment).

```cpp
#pragma pack(push, 1)
struct SkyrimFeedbackPacket_v110
{
    // --- Header (4 bytes) ---
    uint32_t magic;               // 0x534B5952 ("SKYR")
    
    // --- Protocol & Status (4 bytes) ---
    uint16_t version;             // 0x0110 (v1.1.0)
    uint8_t  actorEmotion;        // 0=Neutral, 1=Anger, 2=Fear, 3=Happy, 4=Sad, 5=Surprise, 6=Puzzled, 7=Disgusted
    uint8_t  sceneFlags;          // Bit 0: InDialogue, Bit 1: InCombat, Bit 2: InScriptedScene, Bit 3: InFurniture
    
    // --- Actor & Target Context (8 bytes) ---
    uint32_t activeTargetFormId;  // FormID of the actor currently receiving attention
    int16_t  relationshipRank;    // Skyrim relationship level (-4 to +4)
    uint16_t sceneStageId;        // Active quest scene phase/stage
    
    // --- Spatial & Kinematic Telemetry (12 bytes) ---
    float    distanceToTarget;    // Distance in meters
    float    mutualGazeAngleDeg;  // Angle between Player Gaze and NPC Gaze (degrees)
    uint32_t gameFrameNumber;     // Monotonic Skyrim render frame counter
    
    // --- Integrity Checksum (4 bytes) ---
    uint32_t crc32;               // CRC32 of bytes 0..27
};
#pragma pack(pop)
static_assert(sizeof(SkyrimFeedbackPacket_v110) == 32, "Feedback packet must remain exactly 32 bytes");
```

### 5.2 Scene State Extraction Logic (`GazeEngine.cpp`)
```cpp
void GazeEngine::HarvestSceneContext(RE::Actor* actor, Bridge::SkyrimFeedbackPacket_v110& feedback) noexcept
{
    if (!actor) return;

    // 1. Emotion Harvesting
    feedback.actorEmotion = static_cast<uint8_t>(actor->GetExpression());

    // 2. Scene Flags
    uint8_t flags = 0;
    auto* ui = RE::UI::GetSingleton();
    if (ui && ui->IsMenuOpen(RE::DialogueMenu::MENU_NAME)) flags |= 0x01;
    if (actor->IsInCombat()) flags |= 0x02;
    if (actor->GetCurrentPackage() && actor->GetCurrentPackage()->packData.type == RE::PackageTypes::kScene) flags |= 0x04;
    if (actor->GetOccupiedFurniture()) flags |= 0x08;
    feedback.sceneFlags = flags;

    // 3. Narrative Target Resolution
    if (auto* dialogueTarget = actor->GetDialogueTarget()) {
        feedback.activeTargetFormId = dialogueTarget->GetFormID();
    } else {
        feedback.activeTargetFormId = state.trackedTargetFormId;
    }

    // 4. Relationship Rank
    if (const auto* player = RE::PlayerCharacter::GetSingleton()) {
        feedback.relationshipRank = static_cast<int16_t>(actor->GetRelationshipRank(const_cast<RE::PlayerCharacter*>(player)));
    }
}
```

---

## 6. Implementation Roadmap & Verification Gates

### Phase M1: Context Harvesting in Engine (In-Engine Data Pipeline)
* Query `Actor::GetExpression()`, `PackageType`, and dialogue target during the eligible actor tick.
* Package data into the updated feedback wire struct.
* Unit-verify through `tests/KinematicsTests.cpp`.

### Phase M2: HCEP Desktop Cognitive Modulation Loop
* In `TrueGazeBridgeClient.cs`, ingest the extended `SkyrimFeedbackPacket`.
* Feed `actorEmotion`, `sceneFlags`, and `relationshipRank` into `HCEPPipelineOrchestrator`.
* Adapt outbound HCEP Mode and Action Unit thresholds dynamically in response to game events.

### Phase M3: Helgen Opening Sequence Acceptance Gate
* Launch fresh game starting from `MQ101`.
* Verify:
  1. Zero cart flipping, physics glitches, or actor displacement.
  2. Player character mirrors real-world Kinect head tracking while bound on the cart.
  3. NPCs maintain scripted narrative focus while exhibiting lifelike biological micro-saccades and emotion-matched dwell times.
  4. Console `tgstatus` confirms stable tick rate and zero frame overruns.

---

## 7. Governance & Architectural Covenant

This specification is authored under the supreme authority of **Kirk LaSalle's Permanent Active Directives (10 Laws)**.
* **Law 1 & 6 (Safety & Privacy):** Local named-pipe IPC only. Telemetry remains ephemeral on-device memory and is never logged to external disks or transmitted off-system.
* **Law 9 (Transparency & Auditable State):** All meta-controller states are fully queryable via in-game console commands (`tgstatus`, `tgverbose`) with human-readable logging.
* **Law 10 (Operational Boundaries):** The Meta-Controller operates strictly within designated bone rotation bounds, guaranteeing complete stability and respect for Skyrim's core engine logic.
