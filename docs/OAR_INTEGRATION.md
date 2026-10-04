# TRUE GAZE™ — Open Animation Replacer (OAR) Integration Guide
### Author: Kirk LaSalle
### Platform: The Elder Scrolls V: Skyrim (SE/AE/VR)
### Target Binary: `TrueGaze.dll` (SKSE64)

---

## 1. Overview & Purpose

**Open Animation Replacer (OAR)**, created by Ershin, is the modern standard for dynamic animation replacement in Skyrim. OAR allows modders to replace animations conditionally based on actor states (equipment, weather, factions, location, keywords) without modifying `.hkx` behavior graphs or rerunning external patchers.

**True Gaze™** bridges Kirk LaSalle's **Human Communication Eye Protocol (HCEP)** directly into OAR by registering **native C++ custom conditions** via OAR's Conditions API (Interface Version V3). This enables animators and mod authors to trigger bespoke bodily gestures, postures, and idles corresponding directly to an NPC's cognitive, emotional, and gaze states in real time.

> [!NOTE]
> **Status (October 2026, Issue #6):**
> Custom condition registration is 🔨 implemented via the official OAR Conditions API V3 (vendored from upstream `f4e7688` in `extern/OpenAnimationReplacer-API`).
> TrueGaze exposes the runtime condition evaluators, but does **not** ship proprietary `.hkx` animation files. Animation modders can build custom OAR submods using these conditions.

---

## 2. Exposed TrueGaze OAR Custom Conditions

`TrueGaze.dll` registers three custom conditions with OAR at `kPostLoad` when `OpenAnimationReplacer.dll` is present:

### 2.1. `TrueGaze_IsMode`
Evaluates whether the actor's current HCEP cognitive-emotional mode matches the specified mode index.

* **Condition Name:** `TrueGaze_IsMode`
* **Required Version:** `1.0.8`
* **Components:**
  * `Mode` (`Numeric`, range 0–4):
    * `0` = **LOGIC** (Analytical, structured, steady on-face fixation)
    * `1` = **AFFECT** (Empathetic engagement, Social Triangle eye-mouth cycling)
    * `2` = **SPIRIT** (Deep mutual gaze, high rapport, unbroken connection)
    * `3` = **HEART** (Lower-face empathic resonance, warm validation)
    * `4` = **THINK** (Cognitive gaze aversion, defocused, processing)

### 2.2. `TrueGaze_IsMutualGaze`
Evaluates whether the actor has held direct mutual eye contact with the player for at least the specified duration.

* **Condition Name:** `TrueGaze_IsMutualGaze`
* **Required Version:** `1.0.8`
* **Components:**
  * `Minimum seconds` (`Numeric`, float seconds >= threshold): Minimum duration of unbroken mutual gaze required to evaluate to `true` (e.g. `2.0` seconds).
* **Negation:** To trigger animations when mutual gaze is broken or less than the threshold, check the OAR **Negated** toggle.

### 2.3. `TrueGaze_GetGazeRegion`
Evaluates whether the actor's current classified 3D focal gaze target matches the specified region index (0–12, defined in `src/Engine/GazeRegion.hpp`).

* **Condition Name:** `TrueGaze_GetGazeRegion`
* **Required Version:** `1.0.8`
* **Components:**
  * `Region` (`Numeric`, range 0–12):
    * `0` = **Left Eye** (Social Triangle vertex)
    * `1` = **Right Eye** (Social Triangle vertex)
    * `2` = **Mouth / Lips** (Social Triangle vertex)
    * `3` = **Forehead / Third-Eye** (Upper face)
    * `4` = **Chin** (Lower face)
    * `5` = **Torso / Chest** (Empathic resonance zone)
    * `6` = **Right Hand** (Reserved for target classifier expansion)
    * `7` = **Left Hand** (Reserved for target classifier expansion)
    * `8` = **Ground / Floor** (Gaze Aversion: shame / submission)
    * `9` = **Upper Left Peripheral** (CGA: positivity / constructive hope)
    * `10` = **Upper Right Peripheral** (CGA: memory retrieval / analytical thought)
    * `11` = **Lower Left Peripheral** (CGA: fatigue / negativity / sadness)
    * `12` = **Lower Right Peripheral** (CGA: shyness / hesitation / fear)

---

## 3. Creating Animations with TrueGaze Conditions

### 3.1. Recommended Workflow: OAR In-Game Editor
The easiest and most reliable way to create OAR animation replacements is through OAR's built-in ImGui editor:

1. Launch Skyrim with `TrueGaze.dll` and `OpenAnimationReplacer.dll` installed.
2. In game, press the OAR menu hotkey (default: **Shift + O**).
3. Navigate to or create your mod / submod.
4. Click **Add Condition** and locate the TrueGaze conditions:
   * `TrueGaze_IsMode`
   * `TrueGaze_IsMutualGaze`
   * `TrueGaze_GetGazeRegion`
5. Configure the numeric component (`Mode`, `Minimum seconds`, or `Region`) directly using OAR's GUI. The editor displays live previews of the actor's current value and whether the condition passes.
6. Save directly from the UI. OAR will generate correctly formatted JSON automatically.

### 3.2. Manual OAR Submod Structure
In OAR, animation replacers are organized in subdirectories under `meshes/actors/character/animations/OpenAnimationReplacer/<YourModName>/<SubModName>/`:

```
meshes/actors/character/animations/OpenAnimationReplacer/
  └── MyGazeReactions/
      ├── config.json                     # Main mod config (name, description)
      ├── 1000_ThinkingChinScratch/
      │   ├── config.json                 # Submod config with conditions
      │   └── idle_thinking.hkx           # Replacement animation
      └── 2000_MutualGazeBashful/
          ├── config.json                 # Submod config with conditions
          └── idle_bashful.hkx            # Replacement animation
```

#### Example Submod `config.json` (THINK Mode)
```json
{
  "name": "TrueGaze - Cognitive Thinking Pose",
  "priority": 1500,
  "conditions": [
    {
      "condition": "TrueGaze_IsMode",
      "requiredPlugin": "TrueGaze.dll",
      "requiredVersion": "1.0.8",
      "Mode": {
        "value": 4.0
      }
    },
    {
      "condition": "IsTalking",
      "negated": false
    }
  ]
}
```

#### Example Submod `config.json` (Sustained Mutual Gaze >= 2.5s)
```json
{
  "name": "TrueGaze - Intimate Mutual Gaze Reaction",
  "priority": 2000,
  "conditions": [
    {
      "condition": "TrueGaze_IsMutualGaze",
      "requiredPlugin": "TrueGaze.dll",
      "requiredVersion": "1.0.8",
      "Minimum seconds": {
        "value": 2.5
      }
    }
  ]
}
```

---

## 4. Technical Architecture & Lifecycle

```
[SKSE Init]
    │
    ▼
[SKSEPlugin_Load (kPostLoad)]
    │
    ├─► Probe GetModuleHandleA("OpenAnimationReplacer.dll")
    │     └─► Request OAR Conditions API (Interface V3)
    │           ├─► OAR_API::Conditions::AddCustomCondition<IsModeCondition>()
    │           ├─► OAR_API::Conditions::AddCustomCondition<IsMutualGazeCondition>()
    │           └─► OAR_API::Conditions::AddCustomCondition<GetGazeRegionCondition>()
    │                 └─► Log: "[TrueGaze] OAR integration active: 3/3 conditions registered."
    │
[Active Gameplay Tick]
    │
    ├─► GazeEngine::Update() solves kinematics and publishes actor state
    │     └─► OarConditions::PublishActorState(actorId, state) (shared_mutex protected)
    │
    └─► OAR evaluates animation submod conditions (any thread)
          └─► CustomCondition::EvaluateImpl reads OarConditions::TryGetActorState()
                └─► If state is older than 2.0s, returns false (fails safe)
```

1. **Native C++ API V3**: TrueGaze subclasses OAR's `Conditions::CustomCondition`, delegating UI, serialisation, and negation to OAR's condition system.
2. **Thread Safety**: Evaluators access the live state cache guarded by `std::shared_mutex`. OAR worker threads never perform game-state traversals or mutate TrueGaze memory.
3. **Staleness Guard**: Actor cache records expire after 2.0 seconds (`STALE_AFTER_SEC = 2.0f`). If an actor ceases ticking, conditions cleanly evaluate to `false` rather than retaining stale states.
4. **Graceful Fallback**: If OAR is not installed, TrueGaze logs an informational message and operates normally without error.

