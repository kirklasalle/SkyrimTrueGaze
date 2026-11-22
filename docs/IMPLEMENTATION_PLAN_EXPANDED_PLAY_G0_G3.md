# Implementation Plan — TrueGaze Expanded Play (Phases G0–G3: Riverwood & Sleeping Giant Inn Demo)

**Project:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine  
**Author:** Antigravity (Architecture & Engineering), Kirk LaSalle (Director & Approver)  
**Date:** 2026-11-18  
**Status:** 📐 Ready for WP-1 Implementation  
**Companion Design:** [GAMEPLAY_DESIGN_DOCUMENT.md](GAMEPLAY_DESIGN_DOCUMENT.md) (v0.3, Director Decisions D1–D12)  
**Builds on:** [Region Map Specification — HCEP-02](Region%20Map%20Specification%20%E2%80%94%20HCEP-02%20Gaze%20Regions.md), [OAR_INTEGRATION.md](OAR_INTEGRATION.md)

---

## 1. Scope & Non-Negotiable Engineering Invariants

This plan translates **Phases G0–G3** (the approved **Riverwood & Sleeping Giant Inn Demo**) from [GAMEPLAY_DESIGN_DOCUMENT.md](GAMEPLAY_DESIGN_DOCUMENT.md) into concrete C++23 modules, binary serialization contracts, integration hooks, and unit/playtest verification gates.

### 1.1 Deliverables in scope (G0–G3)

- **G0 Foundations:** `[ExpandedPlay]` INI config (`bEnabled=false` by default), `stgexpanded` console commands, SKSE co-save persistence (`'TGZE'`), and reversible tagged ActorValue/disposition modifiers.
- **G1 Player Input:** First-person **Gaze Reticle** (HCEP-02 color glow, dwell progress ring, reciprocity pulse, soft social-triangle magnetism, Demeanor/whisper HUD) and third-person **Hybrid Demeanor** (`Manual` radial/hotkey + `Auto` context selector).
- **G2 Perception & Interpretation:** `PerceptionGate` (LOS, distance, hemisphere, lighting, closed-helmet occlusion, awareness), dwell band classification, deterministic `InterpretationEngine` with hard child/consent safety invariant, and **Rungs 1–4** response dispatch (kinematics, FaceGen expressions, OAR conditions, vanilla/whisper barks, SKSE ModEvent + HCEP bridge output).
- **G3 Relationships, Book Study & Riverwood Slice:** 512-entry LRU **Gaze Ledger** with game-day decay and triangle observer hooks (spouse + Sven/Camilla/Faendal), passive **Presence** & silent **Signature**, **Ocular Contemplation** read-only `RE::BookMenu` study tracker (*Moth's Stillness* RPG stats), **Eye-Contact Conversation Start**, **Sleeping Giant Inn Staring Contest**, and the **First Canticle** DLL-only awakening (with optional ESL quest hooks).

### 1.2 Non-negotiable engineering invariants

1. **Zero-Cost Default Off:** `[ExpandedPlay] bEnabled = false` by default. When disabled, `ExpandedPlayManager` executes an immediate early return on tick/menu hooks, makes zero co-save mutations beyond preserving frozen state, and strips any active tagged modifiers cleanly.
2. **SDK-Free Deterministic Core (`TrueGazeKinematics` / Standalone Testable):** All pure math, dwell classifiers, interpretation matrices, demeanor resolvers, ledger LRU/decay logic, Presence/Signature math, co-save buffer codecs, and book word-count dwell calculators compile **without** `SKSE/` or `RE/` headers and are verified by a new CTest suite (`ExpandedPlayTests`) under both `TRUEGAZE_STANDALONE=ON` and full SKSE builds.
3. **Hard Child & Consent Safety Gate (§11.3):** Enforced inside `InterpretationEngine::EvaluateLook` before any region/dwell branch runs—if `ctx.isChild` is true, `Attraction` and `IntrusionLeer` are **structurally unreachable** regardless of flags, dwell, target region, or INI values. Body regions (`Torso`, `Chin`, `Mouth`) never trigger intrusive/mature interpretations on children, and all rules remain gender-neutral.
4. **No Papyrus Native Bindings in Core DLL:** Respecting the invariant recorded in [ConsoleCommands.hpp](../src/Integrations/ConsoleCommands.hpp), `TrueGaze.dll` will **not** reintroduce a Papyrus native function table (`RegisterFuncs`). External consumers (including Mantella/CHIM and optional ESL quest scripts) receive events via C++ `SKSE::GetModCallbackEventSource()` (`ModCallbackEvent`), the HCEP named pipe (`NamedPipeServer`), and OAR custom conditions (`OarConditions`).
5. **Strictly Read-Only Book Observer (§13.2.1):** `BookStudyTracker` never modifies `Interface/book.swf`, never edits `TESObjectBOOK` forms, and never intercepts `TESObjectBOOK::Read` or Papyrus `OnRead`.

---

## 2. Module Layout (`src/ExpandedPlay/`)

```text
src/ExpandedPlay/
  ExpandedPlayTypes.hpp        // SDK-free enums & POD contracts (Demeanor, DwellBand, Interpretation, Ledger, BookStudy)
  ExpandedPlayConfig.hpp       // SDK-free [ExpandedPlay] snapshot & sanitisation bounds
  DemeanorController.hpp       // SDK-free 3rd-person Manual/Auto/Hybrid state machine + 60s pin + 0.80x efficacy
  PerceptionGate.hpp           // Pure perception scoring (distance, cone, lighting, helmet occlusion, awareness)
  InterpretationEngine.hpp     // SDK-free dwell classifier + region x dwell x relationship x context evaluator
  GazeLedger.hpp               // SDK-free 512-entry LRU memory table, decay, 8-event ring buffer, triangle hooks
  PresenceSignature.hpp        // SDK-free Presence (0..100), Signature 4-axis scales, titles, Moth's Stillness ranks
  BookStudyTracker.hpp         // SDK-free page-dwell math & word-count splitter + optional RE::BookMenu observer API
  BookStudyTracker.cpp         // Game-side RE::MenuOpenCloseEvent + GFxMovieView / BSAnimationGraphEvent observer
  ExpandedPlayModifiers.hpp    // Reversible, tagged ActorValue & disposition modifier ledger (header/SDK-guarded)
  ExpandedPlayModifiers.cpp    // Game-side ActorValueModifier & faction/relationship application + safe teardown
  GazeReticle.hpp              // Game-side 1st-person reticle, dwell ring, pulse, Demeanor icon & whisper HUD
  GazeReticle.cpp              // Zero-SWF-authoring Scaleform RE::IMenu (mirrors IncomingMenu host-SWF pattern)
  ResponseLadder.hpp           // Rungs 1-4 dispatch + Eye-Contact Conversation Start + Staring Contest controller
  ResponseLadder.cpp           // Kinematics feedback, BSFaceGenAnimationData, OAR, ModCallbackEvent, pipe JSON, AI package/bark
  CoSaveSerializer.hpp         // Binary buffer codec (SDK-free) + SKSE SerializationInterface bridge
  CoSaveSerializer.cpp         // 'TGZE' chunk Save/Load/Revert + ResolveFormID handling
  ExpandedPlayManager.hpp      // Top-level orchestrator interface ticked by GazeEngine
  ExpandedPlayManager.cpp      // 10 Hz focused target + 2 Hz up-to-4 aware observers (< 0.1 ms budget)
```

> [!NOTE]
> `CMakeLists.txt` uses an explicit `TRUEGAZE_SOURCES` list (audit finding R14 E4.4). Every new `.cpp` under `src/ExpandedPlay/` must be added to `TRUEGAZE_SOURCES` in [CMakeLists.txt](../CMakeLists.txt), while the SDK-free `.hpp` headers are shared automatically through the `TrueGazeKinematics` `INTERFACE` target.

---

## 3. Core C++23 Data Contracts (`ExpandedPlayTypes.hpp` & Binary Co-Save Schema)

### 3.1 Enums and value structs (`src/ExpandedPlay/ExpandedPlayTypes.hpp`)

```cpp
#pragma once
#include <array>
#include <cstdint>

namespace TrueGaze::ExpandedPlay
{
    enum class Demeanor : uint8_t
    {
        Courteous = 0,
        Warm      = 1,
        Stern     = 2,
        Evasive   = 3,
        Deferential = 4,
        Predatory = 5,
        Count     = 6
    };

    enum class DemeanorMode : uint8_t
    {
        Manual = 0,
        Auto   = 1,
        Hybrid = 2
    };

    enum class DwellBand : uint8_t
    {
        None     = 0,
        Glance   = 1, // < 0.6 s
        Look     = 2, // 0.6 - 2.0 s
        Gaze     = 3, // 2.0 - 4.5 s
        Stare    = 4, // 4.5 - 9.0 s
        Fixation = 5  // > 9.0 s
    };

    enum class Interpretation : uint8_t
    {
        None            = 0,
        Respect         = 1,
        Warmth          = 2,
        Attraction      = 3,
        Challenge       = 4,
        IntrusionLeer   = 5,
        Suspicion       = 6,
        Deference       = 7,
        Contempt        = 8,
        Compassion      = 9,
        Deceit          = 10,
        Recognition     = 11
    };

    enum class RelationshipTier : uint8_t
    {
        Stranger     = 0,
        Acquaintance = 1,
        Friend       = 2, // Skyrim rank +1..+2
        Confidant    = 3, // Skyrim rank +3
        Partner      = 4, // Amulet of Mara / courting or rank +4 non-spouse
        Spouse       = 5,
        Follower     = 6,
        Rival        = 7, // Skyrim rank -1..-2
        Enemy        = 8, // Skyrim rank -3..-4
        Superior     = 9, // Jarl, Steward, Guildmaster
        Subordinate  = 10
    };

    enum class PresenceTier : uint8_t
    {
        Unnoticed  = 0, // 0..19
        Noticed    = 1, // 20..39
        Steady     = 2, // 40..59
        Commanding = 3, // 60..79
        Legendary  = 4  // 80..100
    };

    enum class ScholarshipRank : uint8_t
    {
        Novice     = 0, // < 5 books
        Reader     = 1, // >= 5 books
        Acolyte    = 2, // >= 15 books
        Scholar    = 3, // >= 35 books
        Prelate    = 4, // >= 75 books
        Canticle   = 5  // >= 150 books
    };

    enum class NotableEventType : uint8_t
    {
        None               = 0,
        FirstMutualGaze    = 1,
        WarmSharedGlance   = 2,
        StareDownWon       = 3,
        StareDownLost      = 4,
        LeerPublic         = 5,
        SpouseJealousy     = 6,
        TriangleWitnessed  = 7, // Riverwood Sven / Camilla / Faendal hook
        ComfortedGrief     = 8,
        CanticleAwakening  = 9
    };

    struct NotableEvent
    {
        NotableEventType type{NotableEventType::None};
        float gameDaysTimestamp{0.0f};
        uint32_t locationFormId{0};
        uint32_t thirdPartyFormId{0}; // witness, spouse, or love-triangle rival
    };

    struct Impressions
    {
        float respect{0.0f};    // [-100, +100]
        float warmth{0.0f};     // [-100, +100]
        float unease{0.0f};     // [-100, +100]
        float suspicion{0.0f};  // [-100, +100]
        float attraction{0.0f}; // [-100, +100]
        float fear{0.0f};       // [-100, +100]
    };

    struct GazeLedgerEntry
    {
        uint32_t npcFormId{0};
        float lastSeenGameDays{0.0f};
        uint64_t lruSequence{0};
        Impressions impressions{};
        Impressions permanentFloor{}; // preserved floor from notable events
        std::array<NotableEvent, 8> notableEvents{};
        uint8_t eventHead{0};
        uint8_t eventCount{0};
        bool recognized{false};
    };

    struct SignatureAxes
    {
        float warmth{0.0f};    // -100 (Cold) .. +100 (Warm)
        float dominance{0.0f}; // -100 (Deferential) .. +100 (Commanding)
        float candour{0.0f};   // -100 (Guileful) .. +100 (Candid)
        float restraint{0.0f}; // -100 (Indulgent) .. +100 (Disciplined)
    };

    struct PresenceState
    {
        float current{20.0f};
        float maximum{20.0f};
        float shakenUntilGameDays{0.0f};
        bool timidDebuff{false};
        float dailyGainAccumulated{0.0f};
        float lastGainGameDay{0.0f};
        bool canticleAwakened{false};
    };

    struct BookStudyEntry
    {
        uint32_t bookFormId{0};
        uint64_t pagesReadMask{0}; // 1 bit per spread up to 64 spreads
        uint8_t totalSpreads{0};
        bool fullStudyAwarded{false};
        float lastStudiedGameDays{0.0f};
    };
}
```

### 3.2 Binary SKSE Co-Save Schema (`UniqueID = 'TGZE'`, version `1`)

All Expanded Play state persists in the SKSE co-save (`SKSE::GetSerializationInterface()`), leaving zero edits in `.ess` vanilla records when disabled or removed.

| Record Tag | Version | Payload Layout (little-endian) | Notes |
|:--|:--|:--|:--|
| `'SIGN'` | `1` | `uint8_t awakened`, `uint8_t demeanor`, `uint8_t demeanorMode`, `uint8_t reserved`, `PresenceState` (`current`, `maximum`, `shakenUntilGameDays`, `uint8_t timid`, `dailyGainAccumulated`, `lastGainGameDay`), `SignatureAxes` (4 × `float`), `uint32_t booksFullyStudiedCount`, `uint32_t skillBoostFormId`, `float skillBoostExpiresGameDays` | ~68 bytes fixed |
| `'LEDG'` | `1` | `uint32_t entryCount` (`<= 512`), followed by `entryCount` records of `{ uint32_t npcFormId (ResolveFormID), float lastSeenGameDays, uint64_t lruSequence, Impressions, Impressions permanentFloor, uint8_t eventCount, uint8_t recognized, NotableEvent[eventCount] (each resolves locationFormId & thirdPartyFormId) }` | Unresolved/deleted mod FormIDs are skipped cleanly on load. Max ~48 KB at 512 cap |
| `'BOOK'` | `1` | `uint32_t bookCount` (`<= 1024` LRU), followed by `bookCount` records of `{ uint32_t bookFormId (ResolveFormID), uint64_t pagesReadMask, uint8_t totalSpreads, uint8_t fullStudyAwarded, float lastStudiedGameDays }` | ~22 KB worst-case |

---

## 4. Subsystem Engineering Specifications

### 4.1 `DemeanorController` (G1 — Third-Person Hybrid Control)

- **State & Modes:** Operates on `DemeanorMode` (`Manual`, `Auto`, `Hybrid` default).
- **Auto Evaluation Priority (first match wins):**
  1. Location has Jarl court/temple keyword OR target is `RelationshipTier::Superior` and player is not Thane → `Demeanor::Deferential`.
  2. Player weapon drawn OR non-ally hostile within `15.0 m` → `Demeanor::Stern`.
  3. Player sneaking OR carrying stolen items within `8.0 m` of owner/guard → `Demeanor::Evasive`.
  4. Focused target is `Spouse`, `Partner`, or `Friend+` → `Demeanor::Warm`.
  5. Wearing Dark Brotherhood armor AND `predatoryUnlocked` AND target is adult AND outside peaceful town unless explicitly enabled → `Demeanor::Predatory`.
  6. Fallback → `Demeanor::Courteous`.
- **Hybrid Pinning:** Choosing a demeanor manually via hotkey/wheel sets `pinnedOverride = true`, `pinExpirySec = 60.0f`, and captures `contextDigest` (current cell/location FormID, combat bit, dialogue target FormID). Pin clears early if the context digest changes (conversation ends, combat starts/ends, cell changes) or timer expires.
- **Output Parameters:** Translates active `Demeanor` into concrete parameters consumed by `InterpretationEngine` and `ActorGazeRuntime` (target dwell duration, aversion tendency, region weights) and scales outcome intensity by `fThirdPersonEfficacy` (`0.80f` default) compared to manual 1st-person targeting (`1.00f`).

### 4.2 `PerceptionGate` (G2 — Visibility & Occlusion)

Evaluates whether `observer` can legitimately perceive the player's look before any interpretation triggers:

1. **Awareness gate:** Rejects if observer is dead, unconscious, ragdolled, sleeping (`AIProcess` sleep state), or actively engaged in combat. Scripted scene targets (`IsInScene()`) receive a `0.50x` intensity multiplier rather than full social interruption.
2. **Distance & LOS gate:** Rejects if distance `> 15.0 m` (`> 8.0 m` blocks eye/mouth/torso region specificity, capping to `Glance` presence only) or if raycast LOS from observer eyes to player eyes fails (`NiPoint3` check via Skyrim collision/LOS test).
3. **Hemisphere check:** `dot(observerEyeForward, dirToPlayer) > -0.15f` (~197° horizontal awareness arc; prevents NPCs behind you from reacting to your reticle unless they turn).
4. **Lighting & Night-Eye:** Scales perception factor `[0.25, 1.0]` by ambient cell/player light level (`GetLightLevel`), with `0.85` floor for Khajiit/Vampire/Dark Elf observers in dim rooms.
5. **Helmet / Mask Occlusion:** Checks player `BGSBipedObjectForm::BipedObjectSlot::kHead` / `kHair` / `kCirclet` slots and closed-face helmet keywords (`ArmorHelmet` + non-open-face): full-face helmets scale facial `Warmth`/`Attraction`/`Recognition` bond accumulation by `0.35x` while preserving `Challenge`/`Suspicion`/`Deference`.

### 4.3 `InterpretationEngine` (G2 — Safety Invariant + Dwell Matrix)

- **Dwell Bands:**
  - `Glance`: `[0.08 s, 0.60 s)`
  - `Look`: `[0.60 s, 2.00 s)`
  - `Gaze`: `[2.00 s, 4.50 s)`
  - `Stare`: `[4.50 s, 9.00 s)`
  - `Fixation`: `>= 9.00 s`
  - *Torso/Hands exception:* Torso/Lower regions escalate one band faster (`Look` starts at `0.45 s`, `Stare` starts at `3.00 s`) only when `matureReactionsEnabled` is true and no compassion/emblem override applies.
- **Safety Gate (executed at line 1 of `EvaluateLook`):**
  ```cpp
  if (ctx.isChild) {
      // Children can NEVER trigger Attraction or IntrusionLeer, and body regions
      // map to neutral curiosity/shyness only.
      if (region == GazeRegion::Torso || region == GazeRegion::Mouth || region == GazeRegion::Chin) {
          return { Interpretation::None, 0.0f, /*allowSocialBump=*/false };
      }
  }
  ```
- **Context Overrides for `Torso` (`GazeRegion::Torso`, HEART mode):**
  - If observer is wounded (`HP < 0.75`), grieving, a beggar, or wearing a known guild cuirass while player shares faction → resolves to `Compassion` or `Recognition`, **never** `IntrusionLeer`.
  - `IntrusionLeer` requires **all** of: `cfg.enableMatureReactions == true`, `!ctx.isChild`, `tier` is not `Spouse`/`Partner`, `!ctx.hasCompassionOrEmblemContext`, and dwell `>= 3.0 s` (`Stare`+).
- **Cultural Modifiers (Starter Set for Riverwood & Skyrim):** Applies multiplicative modifiers by observer race (Nord candour/strength bonus on eye contact, Altmer impertinence penalty if not Deferential/Thane, Orc stare-down challenge bias, Khajiit warmth tolerance, Argonian `0.6x` eye sensitivity).

### 4.4 `GazeReticle` (G1 — First-Person HUD Overlay)

Follows the exact zero-external-SWF pattern established by `IncomingMenu` in [IncomingGazeManager.cpp](../src/Visuals/IncomingGazeManager.cpp):
- Generates a lightweight host SWF via [Build-IncomingUi.ps1](../scripts/Build-IncomingUi.ps1) (or extends the script to also emit `skyrim/Interface/TrueGazeReticle.swf`), loaded via `RE::BSScaleformManager::GetSingleton()->LoadMovie(this, uiMovie, "TrueGazeReticle")`.
- Draws procedurally from C++ using Scaleform `createEmptyMovieClip` (`lineStyle`, `moveTo`, `lineTo` arc segments for the dwell ring) and `TextFormat` (`$EverywhereFont` for Demeanor badge and Telltale-style whispers, e.g., *"Delphine noted your steady eyes."*).
- **HCEP-02 Tint Table (canonical hex from Region Map Spec):**
  - `LeftEye`: `#33FF59` · `RightEye`: `#FF9926` · `Mouth`: `#FF59BF` · `Forehead`: `#D966FF`
  - `Chin`: `#E6BF99` · `Torso`: `#FF2633` · `Ground`: `#2659E6`
  - `ULPeripheral`: `#33CCFF` · `URPeripheral`: `#66F2FF` · `LLPeripheral`: `#A6B3CC` · `LRPeripheral`: `#8080A6`
- **Soft Magnetism:** Optional `<= 2.0 deg` reticle angular bias toward `LeftEye`/`RightEye`/`Mouth` anchors (configurable `fReticleMagnetismStrength`, `0.0` to `1.0`), computed on camera look input or subtly snapping the Gaze Reticle highlight without hijacking mouse combat aim (disabled automatically whenever weapon is drawn or in combat).
- **Accessibility:** Color-blind palettes (`Normal`, `Deuteranopia`, `Protanopia`, `Tritanopia`), distinct vector shape markers (`◆` eyes, `●` mouth, `▲` forehead, `■` torso), and `bImmersiveHideReticle` mode.
- **Menu Safety:** Reuses `DiagnosticMenuPolicy` so the reticle hides cleanly inside `InventoryMenu`, `BarterMenu`, `Book Menu`, `MapMenu`, and `Main Menu`.

### 4.5 `GazeLedger` & Riverwood Relationship Hooks (G3)

- **LRU Capacity:** Fixed 512 active `GazeLedgerEntry` slots. Neutral strangers (`max(|impressions|) < 5.0` and `eventCount == 0`) are **not** inserted into persistent storage—only tracked in a transient 16-slot session cache—preventing town passers-by from evicting meaningful NPCs.
- **Exponential Game-Day Decay:** On interaction or daily sweep, each impression $v$ decays toward `permanentFloor` with a half-life of `14.0` in-game days (`v = floor + (v - floor) * exp2(-deltaDays / 14.0f)`).
- **Spouse & Love-Triangle Witness Hook:**
  - When player triggers `Warmth`, `Attraction`, or `IntrusionLeer` on target $T$, `GazeLedger` queries up to 4 nearby aware observers (`< 12.0 m` with LOS to both player and $T$).
  - If an observer is the player's **Spouse** (`PlayerMarriedFaction`), logs `NotableEventType::SpouseJealousy` on the spouse and adds `unease +15, warmth -10`.
  - **Riverwood Test Hook (Sven / Camilla / Faendal):** If Camilla is the target and Sven or Faendal observes `Attraction`/`Warmth`, logs `NotableEventType::TriangleWitnessed` and bumps `unease`/`suspicion` on the rival, giving an immediate visible test case in Riverwood.

### 4.6 `BookStudyTracker` — Quantified Reading & *Moth's Stillness* (G3, D12)

- **Lifecycle:**
  1. Subscribes to `RE::MenuOpenCloseEvent` for `"Book Menu"` (`RE::BookMenu::MENU_NAME`).
  2. On open: queries `RE::BookMenu::GetTargetForm()` (`RE::TESObjectBOOK*`), `RE::BookMenu::GetDescription()`, and `RE::BookMenu::GetTargetReference()`. Splits text by `[pagebreak]` tags (and falls back to ~220 words per dual-page spread if no explicit breaks exist) into `std::vector<SpreadMetrics>` containing word counts $W_p$.
  3. On page turn: observes `BSAnimationGraphEvent` (`TurnPageLeft` / `TurnPageRight`) and polls `BookMenu::GetRuntimeData().book` (`GFxMovieView::GetVariable` pagination frame/page index) on a low-frequency tick while `"Book Menu"` is open, recording real elapsed `std::chrono::steady_clock` seconds per spread index $p$.
  4. Spread credit rule:
     $$\mathrm{MinDwell}(W_p) = \mathrm{clamp}\!\left(\frac{W_p}{6.5},\; 2.5\text{ s},\; 12.0\text{ s}\right)$$
     - Spread elapsed `< 1.5 s` → ignored (speed-flip guard).
     - Spread elapsed capped at `45.0 s` without page-turn or mouse/VR gaze motion (AFK guard).
     - If elapsed $\ge \mathrm{MinDwell}(W_p)$ and bit `p` is unset in `BookStudyEntry::pagesReadMask` (or `>= 7.0` game days since last full study for 25% refresh), marks spread `p` credited.
  5. On close: computes batch awards once and routes them through `PresenceSignature` and `ExpandedPlayModifiers`:
     - `+0.05` Max Presence per newly credited spread (`+0.15` for Moth Canticles / rare lore volumes; `1.15x` if 1st-person/VR eye-scan swept left→right spread).
     - `+0.25` Full-Study Bonus when `popcount(pagesReadMask) / totalSpreads >= 0.80f`.
     - If `book->TeachesSkill()` and `>= 80%` studied: awards `+0.5` Max Presence + `2.0` game-hour *Deep Craft* (`+10%` skill XP multiplier applied safely via tagged skill-use/rate modifier without touching vanilla's +1 point on open).
     - Increments `booksFullyStudiedCount` toward **Ocular Scholarship (*Moth's Stillness*)** thresholds (`5 / 15 / 35 / 75 / 150`), applying reversible ActorValue bonuses via `ExpandedPlayModifiers`.

### 4.7 `ResponseLadder` & Riverwood First-Slice Features (G2–G3)

| Feature | Implementation Mechanism |
|:--|:--|
| **Rung 1 (Eyes)** | Feeds `Interpretation` & strength into `ActorGazeRuntime` override (`modeOverrideTimerSec`, `mutualGazeHoldSec` reciprocity lock, aversion quadrant bias toward `LRPeripheral` on unease or `ULPeripheral` on warmth). |
| **Rung 2 (Face)** | Queries `actor->GetFaceGenAnimationData()` on the game thread to apply gentle, temporary expression weights (smile/mood happy on `Warmth`, furrowed brow/anger on `Challenge`/`IntrusionLeer`, neutral reset on exit). |
| **Rung 3 (OAR)** | Publishes `interpretation`, `impressionStrength`, `demeanor`, and `presenceTier` into `OarConditions` cache alongside existing `hcepMode`, `gazeRegion`, and `mutualGazeHoldSec`. |
| **Rung 4 (Words, Whisper & Bridge)** | Rate-limited per-NPC cooldown (`>= 20 s` per NPC). Triggers Telltale whispers on `GazeReticle`, triggers vanilla voice topic cues / subtitle barks (using vanilla dialogue topic/voice hooks where ESL is absent, plus full dialogue topics when ESL is loaded), fires `SKSE::ModCallbackEvent("TrueGaze_OnGazeInterpreted", ...)` with human-readable string arg, and emits JSON event packets over `GazeEngine::Pipe()`. |
| **Eye-Contact Conversation Start** | When `PresenceTier >= Noticed` (`>= 20.0`) OR in Riverwood tutorial context, holding mutual `Gaze` (`>= 2.0 s`) on a non-hostile adult NPC within `4.5 m` (not in combat/scene/dialogue) activates `actor->ActivateRef(player, ...)` cleanly or triggers greeting approach. Toggleable via `bEnableEyeContactInitiate`. |
| **Sleeping Giant Inn Staring Contest** | Lightweight C++ state machine started via `stgexpanded contest` (or tavern challenge dialogue in the optional ESL). Locks opposing NPC (`OarConditions` + `ActorGazeRuntime` unblinking lock), tracks `EfmBlinkController` blink pressure + frost vignette overlay on `GazeReticle`, requires player to hold eye fixation without looking away or blinking past limit for escalating rounds (5 s → 10 s → 15 s). Awards `+1.0` Max Presence, Warmth/Respect ledger boost, and `StareDownWon` event. |
| **First Canticle Awakening (DLL-only fallback)** | When `canticleAwakened == false` and the player completes their first full book study (`>= 80%`) or sustains a `4.5 s` mutual gaze inside the Sleeping Giant Inn (`RiverwoodSleepingGiantInn`), triggers the diegetic whisper *"Your gaze stills—the First Canticle awakens."* and grants `+5.0` Max Presence. |

---

## 5. Integration Points with Existing Codebase

### 5.1 Configuration (`src/Engine/ConfigManager.hpp/.cpp` & `skyrim/SKSE/Plugins/TrueGaze.ini`)

Add `[ExpandedPlay]` section to `ConfigManager`:

| INI Key | Type | Default | Sanitised Bounds |
|:--|:--|:--|:--|
| `[ExpandedPlay] bEnabled` | `bool` | `false` | `false` / `true` |
| `sDemeanorMode` | `std::string` | `"Hybrid"` | `"Manual"` \| `"Auto"` \| `"Hybrid"` |
| `bModuleSocialReactions` | `bool` | `true` | — |
| `bModuleRelationshipMemory` | `bool` | `true` | — |
| `bModuleBookStudy` | `bool` | `true` | — |
| `bModuleMatureReactions` | `bool` | `false` | Opt-in, default `false` |
| `bModulePresenceProgression` | `bool` | `true` | — |
| `bEnableReticle` | `bool` | `true` | — |
| `fReticleOpacity` | `float` | `0.22f` | `[0.0f, 1.0f]` |
| `fReticleMagnetismStrength` | `float` | `0.35f` | `[0.0f, 1.0f]` |
| `iColorBlindMode` | `int` | `0` | `[0, 3]` (`0`=Normal, `1`=Deut, `2`=Prot, `3`=Trit) |
| `bImmersiveHideReticle` | `bool` | `false` | — |
| `bEnableEyeContactInitiate` | `bool` | `true` | — |
| `bEnableImpressionWhispers` | `bool` | `true` | — |
| `fThirdPersonEfficacy` | `float` | `0.80f` | `[0.50f, 1.00f]` |
| `bEmitAiNpcBridgeEvents` | `bool` | `true` | — |

### 5.2 Console Commands (`src/Integrations/ConsoleCommands.cpp`)

Add `stgexpanded` (reclaiming an unused dead/empty command slot using the existing `LocateConsoleCommand` pattern):
- `stgexpanded on` / `stgexpanded off` — toggles `[ExpandedPlay] bEnabled` (defers modifier teardown/activation safely if menus are open, saves INI, refreshes engine).
- `stgexpanded status` — prints master state, current `Demeanor` & mode, `Presence` (`current/max`, tier), `Signature` 4-axis summary, `ScholarshipRank`, and tracked `GazeLedger` count.
- `stgexpanded demeanor <courteous|warm|stern|evasive|deferential|predatory|auto|manual|hybrid>` — sets demeanor pin or mode.
- `stgexpanded book` — dumps current/last `BookStudyTracker` session telemetry (last book FormID, spreads read, *Moth's Stillness* rank).
- `stgexpanded contest` — starts/aborts a Staring Contest against the actor currently under the crosshair (for rapid Sleeping Giant Inn testing before ESL dialogue exists).
- Extend existing `stgstatus` log/console summary with a single `ExpandedPlay: enabled/disabled` line.

### 5.3 OAR Custom Conditions (`src/Integrations/OarConditions.hpp` & `OarCustomConditions.cpp`)

Extend `OarConditions::ActorState` and register 4 new read-only OAR conditions during `kPostLoad`:
- `TrueGaze_GetInterpretation` (numeric enum matching `Interpretation`)
- `TrueGaze_GetDemeanor` (numeric enum matching `Demeanor`)
- `TrueGaze_GetPresenceTier` (numeric enum matching `PresenceTier`)
- `TrueGaze_GetImpression` (component selectors: dimension + threshold comparison on `GazeLedger` values)

### 5.4 Main Plugin Lifecycle (`src/Main.cpp`)

- In `SKSEPluginLoad`: register `SKSE::GetSerializationInterface()` with `SetUniqueID('TGZE')`, `SetSaveCallback`, `SetLoadCallback`, and `SetRevertCallback`.
- In `MessageHandler`:
  - `kDataLoaded`: install `BookStudyTracker` menu sink and `GazeReticle` menu registration; sync `ExpandedPlayConfig`.
  - `kPreLoadGame` / `kNewGame`: call `ExpandedPlayManager::Get().ResetSession()` (clears transient runtime state prior to `CoSaveSerializer` loading `'TGZE'`).
  - `kPostLoadGame`: call `ExpandedPlayManager::Get().ApplyLoadedModifiers()` after co-save load completes.
  - `kSaveGame`: strip transient temporary modifiers if disabled, keeping only reversible records.

---

## 6. Ordered Work Packages (WP-1 through WP-7)

| WP | Phase | Target Files | Definition of Done |
|:--|:--|:--|:--|
| **WP-1** | **G0** | `ExpandedPlayTypes.hpp`, `ExpandedPlayConfig.hpp`, `ConfigManager.hpp/.cpp`, `TrueGaze.ini`, `ConsoleCommands.cpp` | `[ExpandedPlay]` keys parse, clamp, and persist; `stgexpanded on/off/status` works; `bEnabled=false` adds zero runtime overhead. |
| **WP-2** | **G0** | `CoSaveSerializer.hpp/.cpp`, `ExpandedPlayModifiers.hpp/.cpp`, `Main.cpp` | `'TGZE'` co-save round-trip (`SIGN`/`LEDG`/`BOOK`) passes binary codec unit tests; ActorValue modifiers apply/revert cleanly on disable/load/save. |
| **WP-3** | **G1** | `DemeanorController.hpp`, `GazeReticle.hpp/.cpp`, `scripts/Build-IncomingUi.ps1`, `CMakeLists.txt` | Hybrid Demeanor transitions & 60 s pin timer pass unit tests; `GazeReticle` renders HCEP-02 region tint, dwell ring, reciprocity pulse, and whispers in 1st person. |
| **WP-4** | **G2** | `PerceptionGate.hpp`, `InterpretationEngine.hpp` | Child safety invariant verified (`isChild == true` never returns `Attraction`/`IntrusionLeer`); dwell bands, Torso HEART-vs-Leer disambiguation, and race weights pass tests. |
| **WP-5** | **G3** | `GazeLedger.hpp`, `PresenceSignature.hpp`, `BookStudyTracker.hpp/.cpp` | 512-LRU eviction, game-day decay, spouse/triangle hooks, Presence daily diminishing returns, and `BookStudyTracker` read-only `RE::BookMenu` pagination + *Moth's Stillness* stat progression verified. |
| **WP-6** | **G3** | `ResponseLadder.hpp/.cpp`, `ExpandedPlayManager.hpp/.cpp`, `GazeEngine.hpp/.cpp`, `OarConditions.hpp/.cpp`, `OarCustomConditions.cpp` | Ticked at 10 Hz focus / 2 Hz aware (`< 0.1 ms` budget); FaceGen expressions, OAR conditions, `TrueGaze_OnGazeInterpreted` ModEvent, HCEP pipe JSON, Eye-Contact Conversation Start, Staring Contest, and First Canticle awakening functional. |
| **WP-7** | **Verify** | `tests/ExpandedPlayTests.cpp`, `CMakeLists.txt` | Standalone + SKSE CI builds green, `ctest --output-on-failure` passes 100% including `ExpandedPlayTests`, and Riverwood playtest matrix signed off. |
| **WP-8** | **G3+** | `PersuasionTransactions.hpp`, `ExpandedPlayManager.cpp` | Real-time Oblivion Persuasion Wheel probes (`0.35 s–1.99 s`), 6-vector deterministic NPC affinity matrix, Guard's Glance witness block, Stand-Down Grace, and reversible Speech/Barter transactions. |
| **WP-9** | **Lore / ESL** | `AwakeningRitual.hpp/.cpp`, `CoSaveSerializer.cpp`, `scripts/Build-ExpandedPlayEsl.py`, `skyrim/TrueGaze_ExpandedPlay.esp` | Hybrid 4-tumbler Staged Awakening (`Stage 0 → 4`), optional `Skyrim.esm`-only zero-Papyrus ESL plugin, right-hand glowing Moth spell + 1st/3rd-person left-hand open Canticle book visual, parchment MessageBox ritual checklist, live book-study HUD progress, follower-free Shared Sight offering, and `stgitems` dev drop. |
| **WP-10** | **Combat & Creature** | `CombatAndCreatureSight.hpp`, `TargetSelector.cpp`, `ExpandedPlayModifiers.hpp/.cpp`, `ExpandedPlayManager.hpp/.cpp`, `tests/ExpandedPlayTests.cpp` | Smart Brandishing (distinguishes lethal steel/hostile spells from torches, Canticle spell & peaceful utility), Stage 2+ Combat Sight (weapon wind-up, spell charge, shield rhythm, footwork lunge reversible modifiers + Kindred Sight combat flank warnings), and Creature Sight (extended jaw/claw/paw/wing/hoof bone lookup + Prey *Gentle Sight*, Predator *Kyne's Stillness*, Dragon *Dov's Weight*). |

---

## 7. Verification & Riverwood Playtest Matrix

### 7.1 Automated CTest verification (`ExpandedPlayTests`)

Add `tests/ExpandedPlayTests.cpp` linked against `TrueGazeKinematics` with `truegaze_force_asserts(ExpandedPlayTests)` in **both** the `TRUEGAZE_STANDALONE` branch and full build branch of [CMakeLists.txt](../CMakeLists.txt):

- **Dwell & Interpretation tests:** Verify all 5 dwell bands (`Glance`..`Fixation`), `Torso` Compassion/Emblem override vs `IntrusionLeer`, and hard child invariant (`Attraction`/`IntrusionLeer` impossible when `isChild=true`).
- **Hybrid Demeanor tests:** Verify `Auto` priority ordering, `Hybrid` 60-second manual pin + early clear on context change, and `0.80x` third-person scaling.
- **Ledger & Decay tests:** Verify 512-entry LRU eviction preserves high-impression/recent entries, neutral strangers stay out of persistent storage, exponential decay respects `permanentFloor`, and spouse/triangle witness events fire cleanly.
- **Book Study tests:** Verify `[pagebreak]` parser, $\mathrm{MinDwell}(W_p)$ clamping (`2.5 s`..`12.0 s`), `< 1.5 s` speed-flip rejection, `45 s` AFK cap, `uint64_t pagesReadMask` dedup + 7-day refresh, and `ScholarshipRank` transitions (`5/15/35/75/150`).
- **Co-Save Binary Codec tests:** Verify `'TGZE'` serialize → deserialize byte-parity, truncated/corrupt chunk safety, and zero-FormID filtering.

Run commands after each work package:
```powershell
cmake --preset windows-standalone
cmake --build build/standalone --config Release
ctest --test-dir build/standalone -C Release --output-on-failure
```
*(Plus full CommonLibSSE-NG build verification before merging WP-2, WP-3, WP-5, WP-6, and WP-7.)*

### 7.2 Riverwood & Sleeping Giant Inn In-Game Acceptance Matrix

| Scenario | Test Steps in Riverwood | Expected Observable Result |
|:--|:--|:--|
| **Default Off Parity** | Load save with `bEnabled=false` | Zero reticle, zero whispers, zero stat modifiers, identical `TrueGaze.log` performance |
| **Toggle & Gaze Reticle** | `stgexpanded on` in Riverwood | Subtle HCEP-02 colored glow matches targeted face/torso region; dwell arc fills; pulse fires on mutual gaze |
| **3rd-Person Hybrid Demeanor** | Switch to 3rd person; sneak near guard / talk to Delphine / pin `Warm` | Auto shifts to `Evasive`/`Courteous`; manual pin holds for 60 s or until cell/conversation ends |
| **Riverwood Triangle** | Look warmly at **Camilla Valerius** (`> 2.5 s`) while **Sven** or **Faendal** has LOS | Witness event logged on Sven/Faendal; rival `unease` rises; whisper displays if enabled |
| **Merchant Suspicion & Rapport** | Inside Riverwood Trader, watch **Lucan's** hands/counter vs steady polite eye contact | Watching counter raises `suspicion`; polite gaze builds `respect`/`warmth` in `GazeLedger` |
| **Eye-Contact Conversation** | Hold `> 2.0 s` mutual gaze on **Orgnar** at the Sleeping Giant Inn bar (`Presence >= 20`) | Orgnar initiates interaction without pressing `E` |
| **Tavern Staring Contest** | Target patron in Sleeping Giant Inn → `stgexpanded contest` | Unblinking lock, blink pressure vignette, 3 escalating rounds; win awards `+1.0` Max Presence |
| **Ocular Contemplation** | Read a 3+ page book slowly (`>= MinDwell`/page) vs flipping quickly (`< 1.5 s`/page) | Slow study awards Presence, Disciplined/Candid Signature shift, and updates `stgexpanded book`; fast flip awards `0` |
| **Child Safety & Save Safety** | Gaze at **Dorthe** or **Frodnar** (`Torso`/`Mouth`, `5 s`), then `stgexpanded off` and save/reload | Zero mature/attraction triggers; disabling strips all modifiers cleanly and freezes state |
