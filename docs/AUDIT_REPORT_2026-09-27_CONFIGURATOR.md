# TrueGaze Configurator Deep Audit — 2026-09-27

**Scope:** The complete configuration surface of TrueGaze v1.0.6 — `TrueGaze.ini`, `ConfigManager` (engine), `TrueGazeConfig.html` (configurator), the Quick Presets system, and the Character Gaze Profiles — audited for currency, parity, and design direction.
**Auditor:** GitHub Copilot (Z.ai GLM 5.3), at the direction of Kirk LaSalle.
**Method:** Direct source inspection of `skyrim/SKSE/Plugins/TrueGaze.ini`, `src/Engine/ConfigManager.{hpp,cpp}`, `src/Engine/CharacterProfile.hpp`, `src/Engine/GazeEngine.cpp`, `src/Engine/TargetSelector.cpp`, `src/Engine/AnimationHook.cpp`, `src/Integrations/ConsoleCommands.cpp`, and `TrueGazeConfig.html` (all four copies).
**Companion plan:** [`IMPLEMENTATION_PLAN_2026-09-27_CONFIGURATOR_CATEGORY_PRESETS.md`](IMPLEMENTATION_PLAN_2026-09-27_CONFIGURATOR_CATEGORY_PRESETS.md)

---

## 1. Executive Summary

The configuration surface is **structurally healthy but drifting**. The engine's INI schema (`ConfigManager`) is the source of truth and is complete and well-sanitised. The configurator HTML tracks it almost perfectly — with a handful of default-value drifts and one stale comment. The Quick Presets are the weakest layer: they encode **pre-Gold-Standard tuning** (pre-calm/combat speed model, pre-eye-dominance weights) and no longer represent the shipped baseline. The Character Gaze Profiles system is architecturally ideal for the requested category presets (Player/NPC/race/creature), but currently reads only a subset of the available characterization signals and has **no configurator exposure at all** beyond a single master toggle.

**Headline findings:**

| # | Finding | Severity |
| :--- | :--- | :--- |
| F1 | HTML default drift: 6 keys where the configurator's DEFAULTS disagree with the shipped INI/compiled defaults | 🟡 P2 |
| F2 | Quick Presets encode stale pre-1.0.6 tuning (notably skeletal weights and head-engage threshold) | 🟡 P2 |
| F3 | Character Gaze Profiles has zero per-category configurability — one master toggle only | 🔴 P1 (the gap this audit was commissioned to close) |
| F4 | Race is read by NO module — the profile system has no race axis despite the design claim | 🔴 P1 |
| F5 | Relationship rank is hard-disabled in `GatherTemperament` (relocation crash risk) while `RelationshipRankForActor` uses the same call safely elsewhere — an inconsistency to resolve | 🟡 P2 |
| F6 | STATUS.md claims `stgpreset`/`stgreload` console commands exist; they are not registered | 🟡 P2 (doc truth) |
| F7 | INI `[Visuals]` comment still references the retired `GazeBeam.nif` as "DISABLED" — now removed from the tree entirely | 🟢 P3 |
| F8 | Four copies of `TrueGazeConfig.html` must be kept in sync manually (root, `skyrim/`, two `dist/` trees) | 🟢 P3 (process) |

---

## 2. Inventory: The Three Configuration Surfaces

### 2.1 Engine INI schema (`ConfigManager`) — the source of truth

`ConfigManager::ApplyIni` reads **57 keys across 12 sections**; `Save` writes all 57; `Sanitise` clamps 40+ of them with logged warnings. Verified complete and consistent:

| Section | Keys | Notes |
| :--- | :--- | :--- |
| `[General]` | 3 | `bEnableTrueGaze`, `bEnableCreatures`, `sEngineTarget` |
| `[Kinematics]` | 9 | incl. `fEyePursuitSpeed`, `fHeadOnsetDelaySec` |
| `[SkeletalHierarchy]` | 6 | incl. `fHeadEngageThresholdDeg`; yaw weights renormalised to 1.0 |
| `[GazeTarget]` | 4 | eye anchor + morph gain |
| `[CharacterProfile]` | 1 | `bEnableCharacterProfiles` — **the entire category system's only control** |
| `[Social]` | 8 | triangle, CGA, dialogue-sync |
| `[Crosshair]` | 4 | player gaze sweet spot |
| `[Bridge]` | 3 | HCEP pipe |
| `[LOD]` | 2 | tier distances |
| `[Debug]` | 2 | rays, log level |
| `[Visuals]` | 18 | rays, panel, emitters |
| `[Console]` | 1 | `bEnableConsoleCommands` |

**Verdict: current.** Every key the engine reads is present in the shipped INI, and vice versa. The R14 E7.8 clamps (`fGazeRayThicknessCm`, `fGazeRayLengthMeters`, `fGazeRayOpacity`) are present in `Sanitise`. The dead `bLockFreeTelemetry` key is correctly not read and documented as such.

### 2.2 Shipped INI (`skyrim/SKSE/Plugins/TrueGaze.ini`) — ✅ current

Matches the engine schema key-for-key. Comments are rich and accurate, with two exceptions (F7, and the `[Console]` comment block which still carries a duplicated legacy explanation). Defaults match compiled defaults.

### 2.3 Configurator HTML (`TrueGazeConfig.html`) — 🟡 drifting

The HTML `DEFAULTS` schema covers **all 12 sections and all 57 keys** — full structural parity. The drift is in *values* and *stale content*:

**F1 — Default-value drift (HTML DEFAULTS vs shipped INI/compiled defaults):**

| Key | HTML default | INI/compiled default | Assessment |
| :--- | :--- | :--- | :--- |
| `fSaccadeSpeedMult` | 1.0 | **1.5** | INI is the tuned combat ceiling; HTML shows the old baseline |
| `fVelocitySaturation` | 14.0 | **18.0** | same class of drift |
| `fMicroJitterAmp` | 0.35 | **0.28** | |
| `fHeadOnsetDelaySec` | 0.12 | **0.13** | trivial, but parity is parity |
| `fHeadEngageThresholdDeg` | 8.0 | **12.0** | 12° is the verified Gold Standard value |
| `fPupilGlowIntensity` | 0.5 | **0.35** | |
| `bGazeRaysTerminus` | false | **true** | |
| `bEnableConsoleCommands` | false | **true** | HTML is more conservative; acceptable but should be a *documented* divergence |
| `fHcepPanelForwardOffsetCm` | 10–100 range | Sanitise clamps 5–200 | HTML slider range is narrower than the engine's clamp |

None of these are crashes — the configurator writes what the user sees, and `Sanitise` catches nonsense — but the configurator's "default" reset behaviour silently produces a *different configuration* than the shipped INI. That is a truth violation by this project's own standards.

**Other HTML findings:**

- The HTML tooltip knowledge base (`TOOLTIPS`) recommends **old weight values** (`fHeadYawWeight` rec: "0.60-0.70", dflt "0.65") that contradict the shipped eye-dominant weights (0.245). Same for `fSpine2YawWeight`/`fNeckYawWeight` recommendations.
- The HTML "export template" (the INI it writes on save) is generated from live state, so it is correct; only the DEFAULTS reset path is stale.

---

## 3. Preset System Audit

### 3.1 Current state

Five Quick Presets exist in the HTML: **Vanilla Balanced, Subtle & Natural, Intense, Social & Dialogue, Developer**. Each is a partial-key overlay (only the keys it sets are changed; the rest keep their current value).

**F2 — Stale preset tuning.** The presets were calibrated against the **pre-Gold-Standard** engine:

| Preset value | Shipped v1.0.6 baseline | Problem |
| :--- | :--- | :--- |
| `fHeadYawWeight` 0.245–0.315 (presets) | 0.245 (INI) | ✅ consistent |
| `fHeadEngageThresholdDeg` 5–12 (presets) | 12 (INI) | "Intense" sets 5° — reintroduces the head-twitch the 12° value was tuned to eliminate |
| `fSaccadeSpeedMult` 0.7–1.15 (presets) | 1.5 (INI) | All non-developer presets now run *slower than the shipped combat ceiling* — but the calm/combat speed model (×0.25 calm) already scales these down at runtime, so a preset-set 0.7 runs at an effective 0.175 calm. The presets predate the speed model and double-count the slowdown. |
| `fHeadOnsetDelaySec` 0.06–0.12 | 0.13 | minor |
| Developer preset `fHeadOnsetDelaySec` 0.06 | 0.13 | the developer preset should match the verified baseline for testing fidelity |

**Assessment:** the presets are not *wrong* — they are *calibrated to an engine that no longer exists*. The calm/combat speed model (September 26) changed the meaning of every speed key: the INI value is now the **combat ceiling**, and calm behaviour is derived. The presets were never re-derived against this semantics change.

### 3.2 Preset role going forward (Kirk's directive)

> "I do want to keep the presets. The presets are for baseline configurations and development."

This audit endorses a **two-layer preset architecture**:

1. **Baseline presets (the existing five, re-calibrated)** — global engine baselines. Their job: give a user a known-good starting point for the whole engine. They should be re-derived from the v1.0.6 verified baseline (the shipped INI values), with each preset's *intent* (subtle/intense/social) expressed as a **delta from the verified baseline**, not as frozen pre-1.0.6 numbers.
2. **Category presets (new, per Kirk's directive)** — *not global*. They belong to the **Character Gaze Profiles** system (see §4), because a "cowardly NPC" or "predator animal" preset is a statement about *who is looking*, not about the engine's global speeds. A category preset must be a **profile override/multiplier layer**, not a second global INI preset bank.

This resolves the design tension cleanly: global presets tune the *engine*; category presets tune the *population*.

---

## 4. Character Gaze Profiles — The Category Preset Foundation

### 4.1 What exists (verified in source)

`src/Engine/CharacterProfile.hpp` implements a pure `Classify(TemperamentInput) → GazeProfile` function with a **multiplier contract** (every output is a multiplier on the engine's base parameters; 1.0 = unchanged; default parity guaranteed). Inputs currently gathered by the SDK adapter:

- ✅ Confidence, Aggression, Assistance (from the **base form** — the safe read path, per the 2026-09-25 crash lesson)
- ❌ **Relationship rank: hard-disabled** (`in.relationshipRank = 0.0f` with a comment explaining the `BGSRelationship::GetRelationship` relocation crash). **However**, `GazeEngine::RelationshipRankForActor()` (added in R14 E7.6) now calls the *same* `RE::BGSRelationship::GetRelationship` safely for the feedback packet. **F5: the profile system should adopt the proven-safe read path and re-enable the relationship axis.**
- ✅ `isGuard`, `isChild`, `isHumanoid`, `inCombat` (Actor virtuals — proven safe)
- ❌ **Race: not read anywhere.** F4. The header's own doc comment claims "race" is part of the characterization, but no code reads `GetRace()`. Khajiit/Argonian/Elf/Orc gaze differentiation is designed but absent.

Profile outputs (all multipliers, clamped): `aversionRateMult`, `aversionDwellMult`, `mutualGazeThresholdMult`, `fixationScaleMult`, `pathRandomnessMult`, `modeBiasAffect`, 9 `vertexWeights`, `triangleEnabled`.

Consumption (verified in `GazeEngine.cpp`): the profile is cached per actor (`ActorGazeRuntime::profile`), refreshed only on combat edges, and applied to `_frame.fixationScale`, `_frame.triangleEnabled`, `_frame.vertexWeights`, and the CGA engagement roll. The player gets a unique **behavioural warmth** blend from their own face-attention accumulator.

### 4.2 Why this is the right home for category presets

The category request maps *exactly* onto the profile system's architecture:

| Kirk's category | Existing profile mechanism | Gap |
| :--- | :--- | :--- |
| **Player** | Player behavioural warmth blend (attention accumulator) | No configurator control; warmth is implicit |
| **Player type** (stealth/combat/magic play styles) | — | New: play-style-driven profile bias (needs design) |
| **NPCs (generic)** | Confidence/Aggression axes | ✅ exists |
| **NPC type** (guard, child, merchant, bard…) | `isGuard`, `isChild` archetypes | Only 2 archetypes; no extensibility, no user control |
| **Race** (Khajiit, Argonian, Elf, Orc…) | — | **Not read at all** (F4) |
| **Creatures** | `!isHumanoid` simplified profile | One-size-fits-all; no predator/prey/dragon split |
| **Animals** | same creature branch | Same |
| **Other** (undead, Daedra, constructs) | — | New: needs a classification axis |

The multiplier contract is the key enabler: a category preset can be expressed as a **named multiplier bundle** layered on top of the temperament-derived profile, with 1.0 everywhere = exact current behaviour. This preserves the project's additive-only and default-parity guarantees.

### 4.3 Recommended category model (design, not code)

Extend `TemperamentInput` with a **category enum** derived at gather time:

```
Category ∈ { Player, HumanoidNPC, Guard, Child, Vampire, Werewolf,
             Khajiit, Argonian, Elf, Orc, BeastRace, Creature_Predator,
             Creature_Prey, Creature_Dragon, Undead, Daedra, Construct, Other }
```

- Derived from: `IsPlayerRef()`, `IsGuard()`, `IsChild()`, `GetRace()` (with keyword fallbacks for modded races), `IsHumanoid()`, and actor-base keywords (`ActorTypeUndead`, `ActorTypeDaedra`, `ActorTypeAnimal`…).
- `Classify` gains an optional **category override layer**: a table of per-category multiplier bundles (INI-configurable in a new `[Profiles]` section), applied *after* the temperament axes, before clamping.
- The configurator gains a **"Gaze Profiles" category panel**: per-category cards (Player, NPC Types, Races, Creatures, Other) each showing the category's multiplier sliders and a reset-to-default. This is where Kirk's "control presets based on categories" lives.

---

## 5. Console Command Truth (F6)

`ConsoleCommands.cpp` registers exactly nine `stg*` commands: `stg`, `stgvisuals`, `stgv`, `stgon`, `stgoff`, `stgmode`, `stgradius`, `stgverbose`, `stgstatus`. **`stgpreset` and `stgreload` do not exist**, yet STATUS.md line 129 claims they do. Either implement them (a `stgpreset <name>` command would also serve the preset re-calibration work) or correct STATUS.md. The INI's `[Console]` comment also lists the old `tg*` names in one place and `stg*` in another — internal comment drift.

---

## 6. Findings Register (prioritised)

| ID | Severity | Finding | Fix |
| :--- | :--- | :--- | :--- |
| F3 | 🔴 P1 | Character Gaze Profiles: no category axis, no per-category configurability, no configurator panel | Implement category layer (plan §C1–C4) |
| F4 | 🔴 P1 | Race never read; race-based gaze differentiation designed but absent | Add race gathering + race category bundles |
| F1 | 🟡 P2 | HTML DEFAULTS drift from shipped INI on 6–9 keys | Sync DEFAULTS to v1.0.6 INI; add a parity check to the CI gate |
| F2 | 🟡 P2 | Quick Presets calibrated to pre-speed-model engine; double-count the calm slowdown | Re-derive all five presets from the v1.0.6 baseline |
| F5 | 🟡 P2 | Relationship axis disabled in profiles while the same SDK call is proven safe in `RelationshipRankForActor` | Adopt the proven path; re-enable the relationship axis with the crash log referenced |
| F6 | 🟡 P2 | STATUS.md claims `stgpreset`/`stgreload`; not registered | Implement `stgpreset` (serves presets) + fix STATUS |
| F7 | 🟢 P3 | INI `[Visuals]` comment references removed `GazeBeam.nif` | Comment update |
| F8 | 🟢 P3 | Four HTML copies synced manually | Add copy-sync to `Invoke-CiGate.ps1` / packaging script |
| F9 | 🟢 P3 | HTML tooltip recommendations contradict shipped weights | Rewrite affected tooltip rec/dflt rows |
| F10 | 🟢 P3 | `[Console]` INI comment block duplicated/stale (`tg*` vs `stg*`) | Comment cleanup |

---

## 7. Verified Good (do not regress)

- `ConfigManager` single-layer load with candidate paths, layering `ApplyIni`, and honest missing-INI reporting — exemplary.
- `Sanitise()` clamp-and-report discipline — every clamp logs; yaw-weight renormalisation; tier ordering; engine-target validation.
- The profile system's **multiplier contract + default parity** — the exact right architecture for category presets; preserve it.
- Base-form AV reads (the 2026-09-25 crash lesson) — any new gather (race, keywords) must follow the same base-form-first pattern.
- The player behavioural warmth blend — a genuinely novel feature; keep and expose it.
- HTML structural parity: all 57 keys, all 12 sections present in the configurator schema.

---

## 8. Conclusion

The engine's configuration spine is world-class. The drift is concentrated where configuration meets *product decisions*: the presets (stale calibration), the configurator defaults (stale values), and the profile system (designed for categories but not yet delivering them). The companion implementation plan sequences the category preset work in four stages — race/category gathering, the profile override layer, INI + configurator exposure, and preset re-calibration — each preserving default parity and the additive-only contract.

**Next:** [`IMPLEMENTATION_PLAN_2026-09-27_CONFIGURATOR_CATEGORY_PRESETS.md`](IMPLEMENTATION_PLAN_2026-09-27_CONFIGURATOR_CATEGORY_PRESETS.md)
