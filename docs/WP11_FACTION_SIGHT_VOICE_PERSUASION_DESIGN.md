# TrueGaze™ v2.1.0 Design & Implementation Plan — WP-11: Faction Sight, Contextual NPC Voices, Persuasion Visuals & Diegetic Rumor Discovery

**Product:** TrueGaze™ — Biological Gaze, Real-Time Persuasion & Expanded Play  
**Target Milestone:** `v2.1.0` (`WP-11A`–`WP-11D`)  
**Architect & Product Owner:** Kirk LaSalle  
**Design Invariants:** Zero Papyrus `.psc`/`.pex` script files (`VMAD`-free; native C++ calls Skyrim's built-in `ObjectReference.Say` and VR `Quest.*` bindings via `VirtualMachine::DispatchMethodCall` where needed) · Zero mandatory ESPs · Optional ESL `TrueGaze_ExpandedPlay.esp` remains `Skyrim.esm`-only (`0` load-order slots, zero vanilla `CELL` edits, clean-room programmatic timing `.wav`s with zero Bethesda BSA redistribution) · 100% reversible `kTemporary` ActorValue modifiers (`ModifierLedger`, stripped strictly *before* `.ess` write in `PlayerCharacter::SaveGame` vfunc hook `slot 0x0E` and on disable) · Unconditional Child Safety Gate.

---

## 1. Strategic Overview

Following the v2.0.0 milestone, **WP-11** closes four high-impact gameplay and immersion opportunities:

1. **`WP-11A` — Enhanced Real-Time Persuasion Visual Feedback & Physical NPC Tells:** Makes the Oblivion-inspired real-time persuasion loop (`0.35 s–1.99 s` Phase 1 Probe → `>= 2.0 s` Commit) unmistakably readable both *on the HUD reticle* and *physically on the NPC's face, head, and eyes* (including physical eye-darts toward watching guards during `"Guard's Glance"`).
2. **`WP-11B` — Restraint-Driven Contextual NPC Voice Responses:** Invokes Skyrim's authentic vanilla voiced `TESTopic` barks via native `VirtualMachine::DispatchMethodCall(..., "ObjectReference", "Say", ...)` across all voice types with full native lip-sync, strictly governed by per-NPC cooldowns, global room cooldowns, probability gates, and non-interruption guards (cooldown budget consumed only after `topic`/`vm`/`handle` validation succeeds).
3. **`WP-11C` — Faction Sight & Authentic Social Drawbacks (All 7 Major Faction Groups):** Gives Dark Brotherhood, Thieves Guild, Companions, College of Winterhold, Imperial Legion / Stormcloaks, Bards College, and Vampires / Dawnguard-Vigilants distinct gaze crafts **and** real social friction—with civilian attire + demeanor masking as an active player counterplay loop.
4. **`WP-11D` — Diegetic In-World Discovery (Rumors & Elder Scroll / Moth Quest Hooks):** Introduces TrueGaze naturally through innkeeper rumors (Sleeping Giant Inn & Sanctuary towns), canon Elder Scroll / Moth Priest quest hooks (`MQ205`, `DA04`, optional `DLC1VQ03`), and an optional sealed **Travel Note of Brother Cassian** (`0x01000809`) in `TrueGaze_ExpandedPlay.esp`.

---

## 2. Work Package Specifications

### WP-11A: Persuasion Visuals & Physical NPC Tells
- **Reticle HUD Extensions (`GazeReticle.hpp/.cpp` & `ReticleHudSnapshot`):**
  - **Active Vector Micro-Label** rendered cleanly below the 4-tick dwell ring during persuasion (`Admire`, `Joke`, `Boast`, `Coerce`, `Plea`, `Sly Bribe`), with a subtle gold shimmer when the `Belt/Hand → Eyes` `"Knowing Look"` cadence is armed.
  - **Phase 1 Resonance Halo (`0.35 s–1.99 s`):** Outer ring accent tint & pulse driven by `ProbeTellResult`:
    - `Love`: warm gold-emerald pulse (`0x5CE68A` / `0xFFD966`) + soft rising cue
    - `Like`: soft warm teal/amber glow (`0x7FD6C2`)
    - `Dislike`: muted violet-amber flicker (`0xC996E6`) — clear cue to pivot demeanor
    - `Hate`: sharp crimson outer tick pulse (`0xFF4D4D`) — clear cue to look away before `2.0 s`
    - `"Guard's Glance"`: steel-red directional warning chevron (`<` or `>`) on the edge of the reticle pointing toward the watching guard's lateral screen side!
  - **Commit Radial Bloom (`TriggerPersuasionBloom`):** `350 ms` expanding radial confirmation halo + brief floating pill (`+Speech · Barter Ready`, `Stand-Down Grace`, `Sly Bribe Primed`, or `Guard Spotted`) paired with subtle vanilla UI sounds (`UISkillsGlowSD`, `ITMGoldUpSD`, `UIMenuOKSD`, `UIMenuCancelSD`).
- **Physical NPC Tells (Works Even With `bImmersiveHideReticle=true`):**
  - Raise FaceGen Phase 1 expression ceiling smoothly from `0.45` to `0.62` (clamp `0.65`) in `ResponseLadder.cpp` while keeping smooth ramp-in/ramp-out and zero dialogue interference.
  - Add lightweight transient **Micro-Gesture & Eye-Dart Override** fields on `ActorGazeRuntime` (consumed cleanly in `GazeEngine.cpp`):
    - `Love`/`Like` probe → subtle natural **micro-nod** (`+5°` damped pitch pulse over `0.55 s`).
    - `Dislike`/`Hate` probe → subtle **aversion yaw turn** (`±10°` over `0.70 s`).
    - `"Guard's Glance"` → **physical eye-dart toward the watching guard's 3D world position** for `0.65 s` before snapping back to the player!

### WP-11B: Restraint-Driven Contextual NPC Voice Responses (`NpcVoiceResponder.hpp/.cpp`)
- **Pure SDK-Free Restraint Governor (`VoiceResponseBudget`):**
  - Per-NPC cooldown (`60.0 s`), global room cooldown (`18.0 s`), deterministic hash/jitter probability gate (`30%` everyday rapport/unease, `70%` dramatic events like `"Guard's Glance"`, `Stand-Down Grace`, Staring Contest win/loss, and `FactionScrutiny`).
  - Hard suppression gates: `isSpeaking`, `inDialogueMenu`, `inScene`, `inCombat`, `isDead`, `isChild`, `playerSneaking` (unless guard/witness warning), or `[ExpandedPlay] bEnableNpcVoiceResponses=false`.
- **Verified Vanilla `Skyrim.esm` `TESTopic` Dispatch (`VirtualMachine::DispatchMethodCall` → `ObjectReference.Say`):**
  - All `TESTopic` FormIDs verified directly from `Skyrim.esm` before compilation (e.g., `Hello`, `Trespass`, `FireWeapon`, `Taunt`, `AcceptYield`) so every vanilla voice type speaks with full native lip-sync and zero redistributed Bethesda audio files.
  - Custom Moth Priest rumor topics in `TrueGaze_ExpandedPlay.esp` use clean-room programmatically generated 44.1 kHz 16-bit mono PCM timing `.wav` files matched to subtitle reading length (`SEC-01`).
  - `Guard's Glance` dispatches on the **watching guard** (or the nervous target NPC) when sightlines cross!

### WP-11C: Faction Sight & Authentic Social Drawbacks (`FactionSight.hpp`)
- **Pure SDK-Free Matrix (`EvaluateFactionSight`):**
  - Inspects player faction bitmask (`FactionFlags`), worn faction armor/masking state (`isWearingFactionGear`, `isMaskedCivilian`), observer faction/role (`isGuard`, `isVigilantOrPriest`, `isFactionPeer`, `isOpposingCivilWar`, `isNordTraditionalist`), time-of-day (`isDaylight`), active demeanor, and gaze region.
  - **All 7 Major Faction Groups:**
    1. **Dark Brotherhood** (`0x0001BDB3`) — Stealth Mark Study (`+10` Sneak / `+8` One-Handed & Marksman transient `4 s` poise), Brotherhood peer respect; unmasked Shrouded Armor / high `Predatory` Signature spikes citizen `unease`/`fear` & guard `suspicion`.
    2. **Thieves Guild** (`0x00029DA9`) — Belt/Hand Sizing-Up Poise (`+12` Pickpocket transient `4 s`), `18 m` `"Guard's Glance"` early warning, `0.80x` bribe gold curve; Guild leathers in Jarl courts/shops raise merchant/guard scrutiny.
    3. **The Companions** (`0x00048362`, Circle `0x00048363`) — `+25%` melee *Weapon Wind-Up* & *Shield Rhythm* Combat Sight potency, `1.25x` `Boast`/Contest respect, Circle *Beast Blood* predator bonus; Companions penalize `Evasive`/`SlyBribe`, prolonged Circle stares raise mild citizen unease.
    4. **College of Winterhold** (`0x0001F259`) — `1.20x` book study credit multiplier, enhanced *Spell Charge* ward (`+18` Magic Resist + `+15%` Magicka Regen); traditional Nord townsfolk show mild guardedness toward staring robed mages.
    5. **Imperial Legion (`0x0002BF9A`) & Stormcloaks (`0x0002BF9B`)** — Allied soldiers greet with `Deference`/`Respect`, stronger `Stern` `Boast`/`Coerce`/`StandDownGrace`; opposing hold guards/citizens add baseline `Suspicion` & resist `Admire`/`Joke` when in uniform.
    6. **Bards College** (`0x00041E83`) — Enhanced Phase 1 micro-expression readability + `+25%` `Admire`/`Joke` pre-barter Speechcraft bonus; `-25%` `Coerce` authority.
    7. **Vampires vs. Dawnguard / Vigilants of Stendarr** — Vampires gain nocturnal `Attraction`/`Predatory` boost + slower blink cadence, **but** Vigilants/Priests/Dawnguard spike `Suspicion` on daylight eye contact unless masked; Dawnguard/Vigilants gain low-light clarity & enhanced anti-undead/mage Combat Sight.
- **Reversible Modifiers (`ModifierLedger`):** Adds `kPickpocketModifier` (`103`) alongside existing transient ActorValues—stripped on expiry, `kSaveGame`, and disable.

### WP-11D: Diegetic In-World Discovery (Rumors & Quest Hooks)
- **Native C++ Discovery Watcher (`DiscoveryHints` in `AwakeningRitual.hpp` + `ExpandedPlayManager.cpp`):**
  - Tracks a 1-byte `discoveryHintsMask` inside `AwakeningState` (backward-compatible 10-byte `AWKN v1` co-save record; seamlessly reads 8-byte, 9-byte, and 10-byte records!).
  - Watches:
    1. **Sleeping Giant Inn / Tavern Rumor Visit:** After conversing with an innkeeper or lingering at the Sleeping Giant Inn hearth, surfaces a warm lore whisper hinting at *Brother Cassian's* primer (*"The innkeeper's talk turns to a quiet Cyrodiilic Moth Priest who stayed by this hearth..."*).
    2. **Canon Elder Scroll / Moth Quests:** Detects `MQ205` (*Elder Knowledge*, `0x0002D516`), `DA04` (*Discerning the Transmundane*, `0x0002D517`), and runtime-optional Dawnguard *Prophet* (`DLC1VQ03*`) — surfacing an atmospheric First Canticle clue tying the Scrolls directly to TrueGaze's stillness.
    3. **Optional ESL Addition (`TrueGaze_ExpandedPlay.esp`):** Adds **`TGZ_Note_CassianRumor` (`0x01000809`, *Innkeeper's Note: The Moth Priest's Passage*)** — a short readable parchment note placed cleanly alongside *The First Canticle of the Eye* on first discovery at the Sleeping Giant Inn (still `Skyrim.esm` only, zero Papyrus, zero `CELL` edits).

---

## 3. Configuration Parity (3 New `[ExpandedPlay]` Keys → 131 Total)

| Key | Type | Default | Purpose |
| :--- | :--- | :---: | :--- |
| `bModuleFactionSight` | `bool` | `true` | Enables all 7 Faction Sight strengths, drawbacks, and civilian/demeanor masking (`WP-11C`). |
| `bEnableNpcVoiceResponses` | `bool` | `true` | Enables restraint-driven vanilla voiced NPC barks via `RE::Actor::Say` (`WP-11B`). |
| `bEnablePersuasionVisuals` | `bool` | `true` | Enables Phase 1 vector/affinity ring halo, Guard's Glance directional chevron, and commit blooms on `TrueGazeReticle.swf` (`WP-11A`). |

Synchronized across `ExpandedPlayConfig.hpp`, `ConfigManager.hpp/.cpp`, `skyrim/SKSE/Plugins/TrueGaze.ini`, `TrueGazeConfig.html`, `skyrim/TrueGazeConfig.html`, and `tests/ConfiguratorTests.js`.

---

## 4. Verification Plan
1. Verify all `Skyrim.esm` `TESFaction`, `TESQuest`, and `TESTopic` FormIDs directly from the live `Skyrim.esm` binary before compiling.
2. Add comprehensive SDK-free unit tests in `tests/ExpandedPlayTests.cpp` covering:
   - `WP-11A` Persuasion visual ring states, directional chevron calculation, and micro-gesture bounds;
   - `WP-11B` `VoiceResponseBudget` cooldowns, suppression gates, and probability restraint;
   - `WP-11C` All 7 Faction Sight strengths, drawbacks, and civilian/demeanor masking;
   - `WP-11D` Discovery hint bitmask & backward-compatible 8/9/10-byte `AWKN v1` co-save round-trips.
3. Build Release x64 + Standalone with `0` errors; run `7/7` Release CTest, `7/7` Standalone CTest, `ConfiguratorTests.js` (`131` keys), ESL verifier, live deployment, and `19/19` Pre-Flight Health Checks.
