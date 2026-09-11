# TRUE GAZE™ — Open Animation Replacer (OAR) Integration Guide
### Author: Kirk LaSalle
### Platform: The Elder Scrolls V: Skyrim (SE/AE/VR)
### Target Binary: `TrueGaze.dll` (SKSE64)

---

## 1. Overview & Purpose

**Open Animation Replacer (OAR)**, created by Ershin, is the open-source industry standard for dynamic animation replacement in Skyrim. OAR allows modders to replace animations conditionally based on actor states (equipment, weather, factions, location, keywords) without modifying `.hkx` behavior files or rerunning patchers.

**True Gaze™** bridges Kirk LaSalle's **Human Communication Eye Protocol (HCEP)** directly into OAR by exposing **native C++ conditions**. This enables animators and modders to trigger bespoke bodily gestures, postures, and idles that correspond directly to an NPC's cognitive, emotional, and gaze states.

---

## 2. Exposed TrueGaze OAR Custom Conditions

`TrueGaze.dll` registers the following custom condition functions via OAR's native C++ Plugin API (`OAR_API`):

### 2.1. `TrueGaze_IsMode`
Evaluates whether the actor's current HCEP cognitive-emotional mode matches the specified mode ID.

* **Condition Name:** `TrueGaze_IsMode`
* **Parameters:**
  * `mode` (Integer, 0–4):
    * `0` = **LOGIC** (Analytical, structured, steady on-face fixation)
    * `1` = **AFFECT** (Empathetic engagement, Social Triangle eye-mouth cycling)
    * `2` = **SPIRIT** (Deep mutual gaze, high rapport, unbroken connection)
    * `3` = **HEART** (Lower-face empathic resonance, warm validation)
    * `4` = **THINK** (Cognitive gaze aversion, defocused, processing)

### 2.2. `TrueGaze_IsMutualGaze`
Evaluates whether the actor is engaged in direct mutual eye contact with the player.

* **Condition Name:** `TrueGaze_IsMutualGaze`
* **Parameters:**
  * `durationThreshold` (Float, seconds): Minimum duration of unbroken mutual gaze required to evaluate to `true` (e.g., `1.5` seconds).
  * `comparisonOperator` (Standard OAR comparison: `==`, `>=`, `<=`).

### 2.3. `TrueGaze_GetGazeRegion`
Evaluates the 3D focal region where the actor is looking.

* **Condition Name:** `TrueGaze_GetGazeRegion`
* **Parameters:**
  * `regionId` (Integer, 0–12):
    * `0` = Left Eye
    * `1` = Right Eye
    * `2` = Mouth / Lips (Social Triangle vertex)
    * `3` = Forehead / Upper Face
    * `4` = Chin / Lower Face
    * `5` = Torso / Chest
    * `6` = Right Hand / Drawn Weapon
    * `7` = Left Hand / Shield / Spell
    * `8` = Floor / Ground (Gaze Aversion - Submissive/Shame)
    * `9` = Upper Left Peripheral (Gaze Aversion - Internal Cognition/THINK)
    * `10` = Upper Right Peripheral (Gaze Aversion - Visual Recall/THINK)
    * `11` = Horizon / Environment (Panoramic Scanning)
    * `12` = Defocused / Space (Daydreaming)

### 2.4. `TrueGaze_IsSaccadeActive`
Returns `true` during the ballistic phase of an eye jump (duration: 20ms–50ms). Useful for micro-expression triggers.

---

## 3. Sample OAR `config.json` Implementations

Below are concrete configuration snippets demonstrating how mod authors can wire up custom animations.

### Example A: Triggering a "Thoughtful Chin Scratch" in THINK Mode
When an NPC enters **THINK Mode** (averting gaze to process information or answer a complex question), play a thoughtful listening animation:

```json
{
  "name": "TrueGaze - Cognitive Thinking Pose",
  "priority": 1500,
  "conditions": [
    {
      "condition": "TrueGaze_IsMode",
      "requiredVersion": "1.0.0",
      "arguments": [
        {
          "type": "Numeric",
          "value": 4
        }
      ]
    },
    {
      "condition": "IsTalking",
      "negated": false
    }
  ]
}
```

### Example B: Triggering a "Bashful Smile / Glance Away" on Sustained Mutual Gaze
When the player and a follower sustain mutual eye contact for more than 2.5 seconds in **SPIRIT Mode**:

```json
{
  "name": "TrueGaze - Intimate Mutual Gaze Reaction",
  "priority": 2000,
  "conditions": [
    {
      "condition": "TrueGaze_IsMutualGaze",
      "requiredVersion": "1.0.0",
      "arguments": [
        {
          "type": "Numeric",
          "value": 2.5
        },
        {
          "type": "Comparison",
          "value": ">="
        }
      ]
    },
    {
      "condition": "GetRelationshipRank",
      "arguments": [
        {
          "type": "Numeric",
          "value": 3
        },
        {
          "type": "Comparison",
          "value": ">="
        }
      ]
    }
  ]
}
```

### Example C: Triggering "Threat Awareness" when Looking at Drawn Weapons
When an NPC’s gaze saccades down to the player’s drawn sword (`regionId: 6`), transition into a defensive combat ready stance:

```json
{
  "name": "TrueGaze - Weapon Glance Combat Guard",
  "priority": 1800,
  "conditions": [
    {
      "condition": "TrueGaze_GetGazeRegion",
      "requiredVersion": "1.0.0",
      "arguments": [
        {
          "type": "Numeric",
          "value": 6
        }
      ]
    },
    {
      "condition": "IsWeaponDrawn",
      "negated": false
    }
  ]
}
```

---

## 4. Technical Architecture: C++ Registration Lifecycle

```
[Skyrim Engine Init]
        │
        ▼
[SKSEPlugin_Load]
        │
        ▼
[Request OAR Interface via SKSE Messaging]
        │
        ▼
[OAR_API::RegisterCustomCondition("TrueGaze_IsMode", &EvaluateModeCondition)]
[OAR_API::RegisterCustomCondition("TrueGaze_IsMutualGaze", &EvaluateMutualGazeCondition)]
[OAR_API::RegisterCustomCondition("TrueGaze_GetGazeRegion", &EvaluateRegionCondition)]
        │
        ▼
[Active In Gameplay: OAR queries TrueGaze condition cache at 0.001ms overhead]
```

1. **Zero Garbage Collection / Pure Native**: Condition evaluation checks an in-memory bitmask maintained by `TrueGaze::ActorGazeStateCache`.
2. **Instant Response**: OAR queries the cache during its normal evaluation tick without incurring additional scene-graph traversals.
3. **Graceful Fallback**: If OAR is not installed, `TrueGaze` logs an informative notice and continues executing all procedural bone kinematics without interruption.
