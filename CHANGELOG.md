# Changelog

All notable changes to the **TrueGaze™** project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

> [!IMPORTANT]
> **Correction notice.** Earlier entries in this changelog described several features as "implemented" that are not functional, because they were written from design intent rather than from the code. Those entries have been annotated below. See [`docs/STATUS.md`](docs/STATUS.md) for the verified capability matrix and [`docs/AUDIT_REMEDIATION_LOG.md`](docs/AUDIT_REMEDIATION_LOG.md) for the latest Principal-Engineer due-diligence audit and remediation log.

## [2.1.0] - 2026-11-23 - WP-11, Principal-Engineer Audit Remediation, VR-First HCEP & R&D Program

### WP-11: Faction Sight, Contextual NPC Voices, Persuasion Visuals & Diegetic Rumors
- **Faction Sight & Social Drawbacks (`FactionSight.hpp`):** Implemented all 7 Skyrim faction profiles (Dark Brotherhood, Thieves Guild, Companions, College of Winterhold, Imperial Legion / Stormcloaks, Bards College, Vampires / Dawnguard-Vigilants) with civilian-attire and demeanor masking counterplay (`bModuleFactionSight`).
- **Contextual NPC Voice Responses (`NpcVoiceResponder.hpp/.cpp`):** Added restraint-governed vanilla `TESTopic` barks via `VirtualMachine::DispatchMethodCall(..., "ObjectReference", "Say", ...)` with 18 s global / 60 s per-NPC cooldowns and hard non-interruption gates (`bEnableNpcVoiceResponses`).
- **Persuasion Reticle & Physical NPC Tells (`GazeReticle.cpp`, `ResponseLadder.cpp`, `GazeEngine.cpp`):** Added Phase 1 vector labels, affinity halos, Guard's Glance directional chevrons, commit blooms (`bEnablePersuasionVisuals`), and physical NPC nod/aversion/guard eye-darts.
- **Diegetic Pilgrimage Discovery (`AwakeningRitual.cpp`, `Build-ExpandedPlayEsl.py`):** Added Sleeping Giant Inn rumor dialogue topics, Elder Scroll / Moth Priest quest hooks (`MQ205`, `DA04`, `DLC1VQ03`), `TGZ_Note_CassianRumor` (`0x01000809`), and `TrueGaze_ExpandedPlay.seq`. Synchronized **131** INI/HTML settings (`22` `[ExpandedPlay]` keys).

### Principal-Engineer Audit Remediation (2026-11-23 — Full Details in `docs/AUDIT_REMEDIATION_LOG.md`)
- **Clean-Room IP & License Compliance (`SEC-01`):** Removed extracted proprietary Bethesda `skyrim/meshes/marker_arrow.nif` (loaded directly from the player's own `Skyrim - Meshes0.bsa`) and replaced proof-of-concept extracted `.fuz` files with programmatically synthesized 44.1 kHz 16-bit mono PCM timing `.wav` files in `scripts/Build-ExpandedPlayEsl.py` matched to exact subtitle reading length (zero Bethesda BSA assets redistributed).
- **Zero-Clobber Console Commands (`CRIT-01`):** Replaced destructive `RE::SCRIPT_FUNCTION` table slot reclamation in `ConsoleCommands.cpp` with a non-destructive `RE::Console::Accept` (`VTABLE_Console[0]` slot `0x01`) Scaleform callback proxy hook. All 30 `stg*` commands work across SE/AE/VR with **zero** vanilla console commands overwritten.
- **Pre-Save `.ess` Stat Integrity (`CRIT-04`):** Installed a SE/AE/VR-identical vfunc hook on `PlayerCharacter::SaveGame` (`TESForm` slot `0x0E`) in `AnimationHook.cpp` that strips procedural bones and `ExpandedPlayModifiers` strictly *before* `.ess` serialization and restores them synchronously right after; switched `ExpandedPlayModifiers::ApplyActorDelta` from `kPermanent` to `kTemporary`.
- **Crash, Deadlock & Concurrency Fixes (`CRIT-02`, `CRIT-03`, `CRIT-05`, `CRIT-07`, `CRIT-08`):**
  - Reordered `PlayerHook::Hook` SEH breaker check *before* `EyeAimConstraint::WithdrawActor` (`CRIT-02`).
  - Added `TraceLogger::Abandon()` + `std::atexit` registration in `Main.cpp`, bounded `_buffer` memory, closed `_fileStream` on cap auto-stop, fixed cap log text, sanitized non-finite floats, and emitted valid JSON `null` (`CRIT-03`).
  - Replaced raw-struct seqlock UB in `NamedPipeServer` with trivially destructible `SRWLOCK`, closed the `Stop(Join)` pipe handle leak, clamped `reconnectIntervalSec`, and added 5-second warning rate-limiting (`CRIT-05`, `SEC-04`).
  - Hardened `AwakeningRitual.cpp` with real `IVirtualMachine*` pointer (never `nullptr`), `address() != 0` / exception guards, and full Skyrim VR `1.4.15` `Quest` VM fallback (`CRIT-07`).
  - Marshaled `AwakeningRitual` event-sink `ConfigManager` mutations onto the game thread via `SKSE::GetTaskInterface()->AddTask`, and moved static `RE::NiPointer` caches in `AwakeningRitual.cpp`, `IncomingGazeManager.cpp`, and `VisualEffectsManager.hpp` to immortal heap holders to prevent `DLL_PROCESS_DETACH` allocator crashes (`CRIT-08`).
- **Engine, VR, OAR & Persuasion Correctness (`CRIT-06`, `CRIT-09`, `CRIT-10`, `CRIT-11`):**
  - Moved `_gameTimeSec` and calibration sweep advancement from per-actor `TickActor` to per-frame `EndFrame`; clamped hitch deltas to prevent alt-tab actor wipeout; replaced `WrapPi` `while` loops with `std::isfinite` + `std::remainder`; fixed player warmth profile dead code, RNG seed order, frame-rate-independent magnetic settle, configured LOD tier publishing (`DOC-05`), and single per-frame player-centered `SkyrimFeedbackPacket` emission.
  - Corrected Riverwood `ActorBase` FormID inversions in `PersuasionTransactions.hpp` & `ExpandedPlayTests.cpp` directly against `Skyrim.esm` (`0x013478` Delphine, `0x013479` Orgnar, `0x01347A` LucanValerius, `0x01347C` Gerdur).
  - Upgraded `VrController.cpp` to read Skyrim VR's live 6DOF `VR_NODE_DATA::HmdNode` with canonical `125.0` eye-height fallback; guarded `OarCustomConditions.cpp` component pointers and added `OarConditions::EvictActor`; fixed `NpcVoiceResponder.cpp` cooldown burn order and `MicroJitter.hpp` `theta > 0` guard.
- **Security, Packaging & Governance (`SEC-02`–`SEC-05`, `ARCH-01`–`ARCH-03`, `DOC-01`–`DOC-06`):**
  - Eliminated DOM XSS in `TrueGazeConfig.html` (`.value`/`.textContent` DOM assignment instead of `innerHTML` interpolation) and synchronized `skyrim/TrueGazeConfig.html`.
  - Hardened `TrueGazeBridgeServer.ps1` (CSP + `nosniff`, `-RequireToken` on `/api/scan` & `/api/shutdown`, 256 KB body cap & atomic write on `/api/ini`, bounded tail ring + clamped `lines` on `/api/log`, full asset deployment on `/api/deploy`) and synchronized `skyrim/tools/TrueGazeConfig/TrueGazeBridgeServer.ps1`.
  - Wired `scripts/verify_charter.py` and bridge copy-sync into `Invoke-CiGate.ps1`; replaced `Invoke-Expression` with splatting in `Install-AllPrerequisites.ps1`; added `SEQ` + clean-room `Sound/Voice` to `Package-Release.ps1` and `LaunchTrueGaze.bat`; removed no-op `Actor`/`Character` vtable hooks (`AnimationHook.cpp`) and hardcoded `G:\`/`D:\vcpkg` paths across tooling scripts.
- **R&D Innovation Program (`docs/RD_INNOVATION_PROGRAM.md`):** Formalized a 5-track R&D program (`NOV-01`–`NOV-05`) anchored on Skyrim VR as the first-party HCEP reference application and an empirical ReSaver `.ess` verification protocol.
- **Live SE/AE `1.7.104.0` Verification, Forensic `.ess` Tooling & Diagnostic Polish:** Added `scripts/inspect_ess_actor_values.py` (pure-Python SSE/AE/VR LZ4 `.ess` ActorValue inspector), updated `GrantInnNoteToPlayerOnly(source)` to log exact `questStartSource`, and verified `v2.1.0` in a 12-minute live gameplay session (`469,440` ticks, `27,783` target resolutions, `0` errors/SEH/budget warnings, live `Console::Accept` `stgtrace`, clean exit with `16,179`-event `bad_json=0` JSONL trace).

## [2.0.0] - 2026-11-22 - Expanded Play, Real-Time Persuasion, Staged Lore Awakening & Combat/Creature Sight

### Expanded Play Gameplay Layer (Phase G0–G3, Opt-In)
- **Zero-Overhead Default-OFF Layer & Config Parity (`[ExpandedPlay]`):** Added 19 `[ExpandedPlay]` settings (`sUnlockMode`, `bEnableStagedAwakening`, plus 17 module/reticle/demeanor keys) synchronized across `ConfigManager.hpp/.cpp`, `TrueGaze.ini`, and `TrueGazeConfig.html` (**128** total INI/web keys) and 3 new console commands (`stgexpanded`, `stgdemeanor`, `stgitems` — **30** total commands). When `bEnabled=false`, Expanded Play exits immediately with zero overhead.
- **SKSE `'TGZE'` Co-Save & Reversible RPG Modifiers:** Added `CoSaveSerializer` (`SIGN`, `LEDG`, `BOOK`, `AWKN` v1 binary records with `ResolveFormID` remap, backward-compatible 8/9-byte `AWKN` decoding, and NaN/truncation safety) and `ExpandedPlayModifiers` (`ModifierLedger`), stripping all *Moth's Stillness*, pre-barter Speechcraft, and Combat/Creature Sight ActorValue deltas automatically on save or disable.
- **3rd-Person Hybrid Demeanor & 1st-Person Gaze Reticle (`TrueGazeReticle.swf`):** Added 6-priority `DemeanorController` (Manual / Auto / 60s Hybrid pin), procedural Scaleform HUD reticle with HCEP-02 region tinting, 4-tick dwell arc, reciprocity pulse, soft magnetism, accessibility shapes, 3 color-blind palettes, and Telltale-style whispers.
- **Perception, Interpretation & Hard Child Safety Gate:** Added `PerceptionGate` (15m presence / 8m region specificity, LOS, hemisphere, lighting/Night-Eye, helmet/hood scaling) and `InterpretationEngine` with an unconditional Line-1 & Exit Child/Consent Safety Gate plus 10 Skyrim cultural multipliers.
- **512-LRU GazeLedger, Progression & Read-Only *Ocular Contemplation*:** Added 512-entry LRU ledger with 14-day exponential decay and 2 Hz spouse / Riverwood Sven-Camilla-Faendal love-triangle witnesses, `PresenceSignature` (`0..100` daily caps + 4-axis titles), and read-only `BookStudyTracker` (`RE::BookMenu` + `BSInputDeviceManager` + safe per-frame study chimes/notices) awarding *Moth's Stillness* scholarship ranks without conflicting with other book mods.
- **4-Rung `ResponseLadder`, 7 OAR V3 Conditions & Tavern Gameplay:** Extended `OarConditions` / `OarCustomConditions` to register 7 total OAR conditions (`TrueGaze_GetInterpretation`, `TrueGaze_GetDemeanor`, `TrueGaze_GetPresenceTier`, `TrueGaze_GetImpression`), added gentle non-dialogue FaceGen expressions, `TrueGaze_OnGazeInterpreted` SKSE ModEvent + `[TrueGaze::AiNpcBridge]` context emission, Eye-Contact Conversation Start, and 3-round Tavern Staring Contests.

### Real-Time Oblivion Persuasion & Multi-Transaction Engine (`WP-8`)
- **Zero-Menu 3D Persuasion Wheel (`PersuasionTransactions.hpp`):** Maps *TES IV: Oblivion*'s 6 persuasion vectors (`Admire`, `Joke`, `Boast`, `Coerce`, `Plea`, `SlyBribe`) and non-linear GMST Bribe curves (`fBribeCostCurve = 1.50`, `fBribeCurve = 0.75`) onto real-time 3D gaze across every Skyrim NPC (bespoke Riverwood cast profiles + deterministic `FNV-1a` + cultural/guard/child rules everywhere else).
- **Phase 1 Micro-Expression Probes (`0.35 s–1.99 s`), `"Knowing Look"` Bribes & `"Guard's Glance"` Witnesses:** Players read `Love`/`Like`/`Dislike`/`Hate` facial tells before committing at `2.0 s`, dip `Belt/Hand -> Eyes` for covert bribery, get warned/blocked when law witnesses spot them at `2 Hz`, earn *Stand-Down Grace* when sheathing steel mid-gaze, and receive reversible pre-barter Speechcraft buffs.

### Staged Ancestor Moth Lore Awakening & Optional ESL (`WP-9`)
- **Four Order-Independent Keys & Five Stages (`Stage 0 → Stage 4`):** Implemented `AwakeningRitual.hpp/.cpp` with *The Word*, *The Vessel*, *Shared Sight* (both follower trading and follower-free relic drop/offer), *The Pilgrimage* across Skyrim's 7 base Sanctuaries, and the Final Sanctuary Ceremony (`Ritual` vs. `Sandbox` / `player.stgexpanded`).
- **Optional Light Plugin `TrueGaze_ExpandedPlay.esp` (`scripts/Build-ExpandedPlayEsl.py`):** ESL-flagged (`0x0200`, `0` load-order slots), `Skyrim.esm` only, zero Papyrus scripts, zero vanilla world-cell edits — adding *The First Canticle of the Eye*, *Amulet of the Ancestor Moth*, *Moth-Silk Browband*, *Ring of Quiet Sight*, *Silver Moth Medallion*, *Moth-Silk Ribbon*, and the **`0`-Magicka Right-Hand Spell *Sight of the First Canticle***.
- **1st & 3rd-Person Open-Book Casting, Parchment Checklist & Zero-Conflict Sanctuary Discovery:** Equipping *Sight of the First Canticle* visibly floats the spine-flipped open Canticle tome above the player's left hand while the right hand glows with Moth light (in-engine verified in both 1st and 3rd person); casting before Stage 4 opens a Skyrim parchment `[X]`/`[ ]` checklist; visiting each of the 7 Sanctuaries cleanly reveals its lore relic (once per save, Player-owned) and automatically teaches *Sight of the First Canticle*.

### Smart Brandishing, Combat Sight & Creature Sight (`WP-10`)
- **Smart Brandishing (`CombatAndCreatureSight.hpp`):** Separates lethal weapons/hostile spells from peaceful utility items (torches, *Healing*, *Candlelight*, open Canticle book) so carrying light never triggers armed coercion.
- **Combat Sight (`Stage 2+`) & Kindred Flank Warnings (`Stage 3+`):** Reversible `3.5 s` tactical bonuses for reading enemy *Weapon Wind-Up* (`+Block`/`+Armor`), *Spell Charge* (`+Magic Resist`), *Shield Rhythm* (`+Offense`), and *Footwork Lunge* (`+Armor`/`+Stamina Regen`), plus combat flank alerts from relic-bearing companions.
- **Creature Sight & Limb Anchors:** Extended `TargetSelector.cpp` with creature jaw/claw/paw/wing/hoof bones and added *Gentle Sight* (Prey, Stage 1+), *Kyne's Stillness* (Predators, Stage 2+), and *Dov's Weight* (Dragons, Stage 2+).

### Lore Chronicle, Illustrations, Tests & Release Packaging
- **New Illustrated Lore Document & Artwork:** Created [`docs/TRUEGAZE_LORE_AND_STORY.md`](docs/TRUEGAZE_LORE_AND_STORY.md) with 5 original lore/action illustrations (`docs/images/lore/`) plus the new `v2.0.0` hero banner (`docs/images/truegaze_hero_v200.png`), refreshed `README.md`, `EXPANDED_PLAY_GUIDE.md`, `USER_GUIDE.md`, `DEVELOPER_GUIDE.md`, `CONSOLE_COMMANDS_CARD.md`, `STATUS.md`, and `ROADMAP.md`.
- **Verification & Packaging:** All 7 CTest suites (Release + Standalone), `ConfiguratorTests.js` (`128` keys, `5` presets, `30` commands), and 19/19 Pre-Flight Health Checks pass; `Package-Release.ps1` and `PackageMod.ps1` now bundle `TrueGaze_ExpandedPlay.esp` and the complete illustrated documentation suite.

## [1.1.0] - 2026-11-17 - Kinematics, Security Hardening & Incoming Gaze Diagnostics

### Due-diligence hardening, kinematics & release-hygiene remediation (2026-11-17)

- **IPC & teardown safety:** Fixed `NamedPipeServer::Stop(Abandon)` latching,
  `atexit` LIFO construction order in `Main.cpp`, `CloseHandle` ordering after
  `_workerThread->join()`, member-owned `OVERLAPPED` + `GetOverlappedResult(..., TRUE)`
  after `CancelIoEx`, event-driven `ReadFile` (removing `PeekNamedPipe` 2 ms polling),
  `ERROR_MORE_DATA` message draining, and `TraceLogger::StopTracing()` join after
  hitting the 100 MB cap. Unified `include/TrueGazeBridgeSchema.hpp` and
  `src/Bridge/TelemetryPacket.h` to one canonical 64 B / 32 B CRC-32 schema.
- **Security & GPL-3.0 packaging:** Removed wildcard CORS on `TrueGazeBridgeServer.ps1`,
  added `Host`/`Origin` + per-session `X-TrueGaze-Token` authentication and strict
  `TrueGaze.ini` path allowlisting, replaced `innerHTML` interpolation in
  `TrueGazeConfig.html` with safe DOM nodes, switched the pipe DACL to fail-closed
  `TokenUser` SID, and bundled `LICENSE`, `licenses/`, and `SOURCE_OFFER.txt` in
  `Package-Release.ps1` and `PackageMod.ps1`.
- **Kinematics & engine fixes:** Wired `state.vor.headTrackingSpeed`/`eyeMaxAngle`
  and 2nd-order critically-damped SmoothDamp in `VorCoordinator`, preserved `[0, 1]`
  `[SkeletalHierarchy]` yaw weights in `ConfigManager::Sanitise()`, moved LOD
  culling/throttling above `_actors` allocation/override consumption, restored
  `_lastFrameUs = 0` when `!_frameOpen`, gated `RegisterReceiver` on actor
  eligibility, wired `_calState` overrides in `ApplyToSkeleton`, fixed `Guard`/`Child`
  category reachability in `CharacterProfile`, made `IsEyeSaturated()` use active
  limits, stabilized `THINK`-mode CGA into timed episodes, and guarded `EyeAimConstraint`,
  `PlayerGazeResolver`, `IncomingGazeManager`, and `VisualEffectsManager` edge cases.
- **CI & doc alignment:** Added `build-plugin` (`windows-release` `TrueGaze.dll`) to
  `.github/workflows/build-test.yml`, unified `IncomingGazeTests`/`TraceTests` output in
  `CMakeLists.txt`, fixed `LaunchTrueGaze.bat` registry/AE `1.6.1170` detection, and
  aligned OAR `config.json` (`1.0.8`, 1-arg `IsMutualGaze`), `PRD.md`, `ROADMAP.md`,
  `GOVERNANCE.md`, `LICENSE_RESOLUTION.md`, `HCEP_BRIDGE_SPEC.md`, and
  `HCEP_META_CONTROLLER_SPEC.md`.

### Near-face panel and configuration/command synchronization

- Reduced front B default eye-anchor clearance from 70 to 10 cm without changing
  panel size, anatomy, gaze solver or collision-aware text cards.
- Synchronized compiled missing-key defaults, all 109 INI/web keys, offline
  template generation and tooltip defaults. Fixed older-INI saves dropping new
  keys/sections. Root and packaged webpage remain identical.
- Matched console preset overlays to the existing five webpage presets (including
  player-only developer panels); preset writes explicitly report failures.
- Published all 27 commands in user/developer guides and a command card; corrected
  grouped `stgv`, trace start/status and geometry help. Live `stgstatus` now
  includes the incoming reporter, not only historical startup auto-status.
- Added Node.js-based schema/default/preset/command and save-roundtrip regression
  checks to existing parity gates. See `docs/CONFIGURATION_AUDIT.md`; screenshots
  confirm presentation progression, not anatomical accuracy or CPU acceptance.
- Configurator preview now shows labeled anatomical right/left eyes converging
  independently on one target at fixed virtual depth, with shared simulated head
  motion/blinking and real tuning keys. Removed the competing legacy animation
  loop. The preview is illustrative, not live game telemetry or clinically
  calibrated tracking; rendering and shared-target/depth geometry have regressions.
- Fixed preview fixation wobble and apparent unilateral drift: exact head
  counter-rotation, stable held-pointer targets and unamplified/smoothed shared
  autonomous noise. Held-fixation regressions cover both eyes and high jitter.

### Incoming-gaze panel ownership correction

- Added transient full-screen menu suppression for all development diagnostics:
  receiving cards/inspection, leaders, markers, rays, panel meshes and lights.
  Nested menus must all close before fresh-frame restoration. Scene cleanup uses
  the existing game-thread path; console toggles, INI settings and the biological
  solver remain unchanged. Console/dialogue stay usable. Native transition
  acceptance remains pending.

- Replaced overlapping free-form captions with grouped, native-font-measured
  actor cards, dark backings, protected-area/collision-aware placement and stable
  ownership leaders. Exact reticle positions and gaze/trace calculations remain
  unchanged. Card overflow is explicit; `stgintext` opens complete live inspection
  with `stginnext`/`stginprev` paging and native scrolling for long entries.
- Added active-frame timing windows (up to 256 samples) with median, p95 and
  window peak, distinct from session-wide peaks. Layout/paging/crowd/identity tests
  cover mathematical behavior; native presentation and anatomical/visibility truth
  remain separate field gates.

- **Whiterun tavern text correction:** dynamic GFx fields now use the installed
  `$EverywhereFont` alias and explicit AS2 text formatting instead of an unmapped
  default font. GFx-measured text bounds keep annotations on-screen; compact,
  opt-in normal counters no longer cover the compass. Incoming trace data and
  gaze behavior are unchanged. Follow-up Whiterun evidence confirms readable
  target labels; positive-hit presentation remains pending.

- Added human-readable actor names and gaze descriptions beside receiving
  panels/HUD. Duplicate names retain distinguishing IDs; detailed telemetry stays
  available through `stgintext`. Human summaries count distinct observers rather
  than anatomy/panel measurements. Zero matches and visibility uncertainty are
  stated explicitly; gaze and incoming trace semantics are unchanged.

- Replaced the owner's self-panel interpretation with a separate observer/target
  receiving pass. Camera and posed-eye sources are explicit; outgoing cognitive
  region IDs and OAR/API semantics are unchanged.
- Added independently controlled above-head/front panels, mapped anatomy and
  physical-quad markers, observer IDs, ownership leaders, projected text, and an
  original native first-person player telemetry movie.
- Added bounded deterministic receiving geometry and versioned incoming trace
  events. Retired self-panel agreement and panel-based beam-terminus relocation;
  zero measured hits no longer report 100% calibration success.
- Added shipped/default/configurator parity and native UI asset deployment and
  packaging. Existing INI tuning remains preserved.
- Added the [receiving contract, audit and acceptance gates](docs/INCOMING_GAZE_PANEL_DESIGN.md).
  **Implementation is not in-game certification:** detailed creature profiles,
  native UI/render behavior, occlusion and runtime budgets remain pending.

### Added

- **Combat Tactical Glances & Multi-Target Dynamic Focus Lock** — Implemented an authentic combat gaze state machine in `TargetSelector.cpp` and `GazeEngine.cpp`:
  - **Predominant Primary Focus Lock**: The eyes maintain a firm, locked gaze on the adversary's face and eyes. Damped micro-jitter and magnetic attractor dynamics keep the gaze steady.
  - **Multi-Target Tactical Threat Scanning**: When engaged with multiple hostile combatants, the observer scans secondary hostiles (for ~0.50s) every 2.5–3.5s to evaluate threats, immediately reacting to any secondary hostile initiating an incoming attack swing.
  - **Close Encounter Strike & Block Attention**: In close quarters ($\le 4.5\text{m}$), an opponent initiating an attack strike (`GetAttackState() != kNone`) or raising a shield block (`actorState2.wantBlocking`) causes brief, alert tactical attention to shift to the hands/weapon/shield for 0.40–0.45s before returning.
  - **Movement & Footwork Saccades**: When the player (or combatant) moves or changes footing, the eyes take a crisp glance down to the opponent's feet/terrain (0.30s) and then saccade right back up to the opponent's eyes, always returning to the magnetic focus lock.
  - **15th Biomechanical Kinematics Test**: Added `TestCombatTacticalGlances` to `tests/KinematicsTests.cpp`, verifying state transitions, timers, glance expirations, and vergence crossing. *(Correction, audit 2026-10-15: this entry previously said "under 100% test coverage". No coverage tooling exists, and until the F-01 fix below the test's asserts were compiled out in the Release configuration CI runs. The test now runs with live asserts and passes.)*
- **Physiological Binocular Vergence & Dynamic Eye Crossing** — Implemented distance detection and independent per-eye vergence ($\theta = \arctan(\frac{\text{halfIPD}}{\max(D, 3.5\text{ units})}) \times \frac{180}{\pi}$). As an actor or the player approaches within close range ($1.0\text{m} \to 1.8^\circ$, $0.5\text{m} \to 3.7^\circ$, $0.2\text{m} \to 9.1^\circ$, point-blank $\to 17^\circ\text{--}30^\circ$), `leftEyeYaw` rotates inward toward the nose (+Yaw) and `rightEyeYaw` rotates inward toward the nose (-Yaw), physiologically crossing the eyes at point-blank range just like real human eyes.
- **Focal Point Hit Detection & Magnetic Eye Contact Lock** — Integrated real-time 3D facial focal point detection and foveal magnetic attractor dynamics. When an observer's line of sight enters the target's eye contact zone ($\le 2.5^\circ$), magnetic attractor dynamics damp micro-jitter by $70\%$ and smoothly lock the ocular lines of sight directly into alignment with the target pupil center like a magnet, eliminating saccadic jitter and drift while holding eye contact.
- **Bi-Directional NPC-to-NPC & Player Mutual Gaze** — Generalised mutual eye contact detection across all actor combinations (Player $\leftrightarrow$ NPC and NPC $\leftrightarrow$ NPC). When both actors target each other and both achieve focal eye contact, mutual gaze duration accumulates, driving OAR animation conditions and HUD feedback.
- **Real-Time 3D Marker Arrow & Ray Focal Convergence** — Updated `VisualEffectsManager::UpdateActor` to pass the true detected target focal distance $D$. In-game 3D marker arrows, gaze rays, and terminus illumination points physically converge and terminate at the exact depth of the target actor's face in world space.
- **Torso Divergence Metric & Seated Vehicle Decoupling** — Re-architected `GetActorReferenceOrientation` in `GazeEngine.cpp` and `GetObserverHeadingRad` in `TargetSelector.cpp`: evaluates the torso divergence metric $\text{diff} = |\text{WrapPi}(\text{spineHeading} - \text{rootYaw})|$. When $\text{diff} > 30^\circ$ ($0.5236\text{ rad}$) — such as the $\sim 90^\circ$ orthogonal seating across the Helgen cart bed (`PrisonerCarriage01` where `actor->GetSitSleepState()` reports `kNormal`) — or when `sitState != kNormal` or `GetOccupiedFurniture()` exists, TrueGaze adopts the spine's physical horizontal forward facing `std::atan2(forward.x, forward.y)` from `cachedSpine` (with clavicles as fallback).
- **Level Horizon Pitch Invariant** — Strictly enforces `ref.pitchRad = 0.0f` across both standing and seated postures, measuring gaze elevation relative to the world horizon and permanently preventing idle posture / spine tilt from injecting upward skyward pitch bias.
- **Anatomical Cervical Limits ($\pm 70^\circ$)** — Enforces $\pm 70^\circ$ yaw limits relative to the torso coordinate frame whenever an actor is seated or their torso diverges from their root capsule, preventing backward neck-snapping.
- **Hermite Cubic Head Engagement Smoothstep** — Implemented C1 Hermite cubic smoothstep ease-in across $[0.75 \times \text{thresh}, 1.0 \times \text{thresh}]$ in `BoneController::CalculateHierarchyStrain`, eliminating the $4.2^\circ$ per-frame jump when head tracking engages.
- **Exponential CGA & Scene Deferral Blending** — Added `cgaAversionBlend` and `headChainYieldAlpha` smooth exponential blending (rate $8.0/\text{s}$, $\sim 125\text{ ms}$) in `GazeEngine::ApplyToSkeleton`, eliminating all per-frame head bone snaps when transitioning between target tracking, THINK mode (CGA), and scene deferral.
- **Conversational Eye Majority** — Isolated peripheral aversion vertices strictly to deliberate CGA / THINK mode episodes in `SocialTriangle.hpp` and boosted foveal eye contact weighting to $4.0\times$ (`LeftEye` = 4.0, `RightEye` = 4.0, `Mouth` = 1.0). *(Correction, audit 2026-10-15: this entry previously claimed an "85%+" eye majority. With neutral profile weights the steady-state share of vertex **selections** landing on an eye is ≈ 67% (≈ 33.5% per eye, ≈ 11% each for Mouth / ThirdEye / Chest). Share of **fixation time** may be higher because of eye dwell, but it has not been measured. `KinematicsTests` now asserts the eyes take > 50% and that each eye out-draws every non-eye feature.)*

### Fixed

- **Dialogue and scripted-scene eye lock** — Script-directed NPC targets now remain active TrueGaze eye targets instead of collapsing to `None` while the vanilla head pose is preserved. A generalized six-second dialogue-partner latch covers player and NPC addressees through engine signal flicker, and dialogue onset explicitly reacquires an eye vertex before normal long eye dwells and brief aversions resume.
- **FaceGen eye anchors instead of guessed offsets** — The previous 7.0 cm forward / 1.5 cm up visual adjustment placed arrows too low in game. Source origins, target eye points, and developer visuals now prefer the loaded eye shader's left/right globe centres, transformed through the head-part skin bind into the posed head frame. Measured from vanilla `eyesmale.nif` / `eyesfemale.nif` and baked FaceGen heads: globe centres at x=±2.4, y=7.57, z=3.75 head-bone units, cornea front at y=9.11. Arrows now start on the visible pupil (globe centre + 1.54 units along the gaze), and NPCs aim at the partner's visible pupil on the observer-correct side. Fallback-only offsets (`fPupil*`, `fEyeAnchor*`, and the social preset) now default to the measured globe centre, 10.8 cm forward / 5.4 cm up with a 2.4-unit half-IPD, and a rig warning is logged when eye geometry is unavailable. Eye social vertices no longer add the erroneous 2.8 cm eyebrow offset.
- **Actual posed-head ocular residual** — Eye angles are solved against the head transform actually applied, including animated tilt/roll and partial head-chain weights, rather than subtracting a simulated head turn that was never fully applied. Marker arrows use the real per-eye focal distance, including actor scale, instead of clipping to the configured default reach. Geometry regressions cover tilted/rolled heads, the former 19.5-degree weighted-head miss, and binocular endpoints.
- **Reciprocal listener attention** — Speaking NPCs with a current addressee can become that listener's dialogue target. Ambient headtrack clearing no longer destroys dialogue/script/procedure targets. Weighted conversational glances are capped at 1.2 seconds and return to an eye; normal configured eye dwell remains longer. Mutual-gaze accumulation runs after the eye solve and excludes ballistic acquisition, ocular saturation, and tracking error above 2.5 degrees.
- **Offset player rays in first person** — `GazeEngine::ApplyToSkeleton` updated player-attached gaze visuals in first person, where the third-person head rig is offset from the camera, so the player's pupil rays and HCEP panel floated detached in view (`ScreenShot328.png`–`ScreenShot330.png`). Player visuals are now removed while the player is in first person and restored in third person, implementing the design guard that the player asset is never visible in first person. Player eye gaze, NPC visuals, and third-person player visuals are unchanged.
- **Validation boundary** — Builds and mathematical regressions do not establish in-game pupil-surface alignment or morph calibration. The `FaceGenEyeCenters` rig probe, model-local centres, and `TrueGaze::Aim` residual/error telemetry supported the cart/tavern verification. **In-game verification (2026-10-16):** the Whiterun tavern NPC-to-NPC exchange (`ScreenShot321.png`–`ScreenShot327.png`), the Helgen opening cart scene (`ScreenShot331.png`–`ScreenShot338.png`), and first/third-person player rays were confirmed in game.
- **HCEP panel calibration** — Increased the panel height by 10% from 25.0 to 27.5 Skyrim units and synchronized the runtime fallback, packaged INI, configurator copies, and user guide.
- **Test asserts were compiled out in CI (audit 2026-10-15 F-01)** — `truegaze_force_asserts()` passed `/DNDEBUG` to `target_compile_definitions`, which takes macro *names*; MSVC rejected the resulting `-D/DNDEBUG` (warning C5102) and NDEBUG stayed defined, so every `assert()` in the Release-configured suites — the configuration CI runs — compiled to nothing. The helper now applies `/UNDEBUG` to non-Debug test builds, `tests/TestSupport.hpp` `#error`s if NDEBUG is ever defined again, and the CI gate fails on C5102.
- **Tests that only passed because asserts were off (F-02)** — With asserts live: the neutral-weight social-triangle test asserted a pre-eye-lock design and now asserts the current contract; `TraceTests` expected a blink event while blink tracing is off by default and now opts in explicitly; `GeometryTests` asserted a non-existent `PanelHitResult::agreement` member and now asserts classifier/panel agreement directly. All 5 suites (15 kinematics suites, 150 validation checks) pass in Release and Debug.
- **Failed asserts hung ctest (F-02)** — A failing assert opened a modal CRT / WER dialog. `tests/TestSupport.hpp` routes CRT reports to stderr and suppresses the dialogs so a failure exits non-zero.
- **`NextWeightedVertex` indexed profile weights by candidate position (F-02)** — Now indexed by `Vertex` value as documented; behaviour is unchanged today because the candidate order matches the enum. Peripheral weights (e.g. a shy NPC's `LowerRight`) remain inert in this path — documented, not changed.
- **Exported C API raced the game thread (F-03)** — `TrueGaze_GetActorGaze` / `TrueGaze_OverrideActorMode` read and wrote `GazeEngine`'s unsynchronised per-actor map from the caller's thread. The game thread now publishes a mutex-guarded snapshot per tick and applies queued mode overrides at the start of the actor's next tick (`src/Engine/ApiTelemetryCache.hpp`). Threading contract documented in `include/TrueGazeAPI.h`.
- **`TrueGaze_GetVersion()` reported 1.0.7 (F-07)** — The version is now generated from `project(VERSION)` (`cmake/TrueGazeVersion.h.in`) and used by `SKSEPluginInfo`, the log lines and the API.
- **NaN INI values bypassed sanitisation (F-04)** — `strtof` accepts `nan`/`inf`, and NaN fails every range comparison. Non-finite INI floats now keep the compiled default with a warning; `Sanitise()` also guards NaN. The parser lives in SDK-free `src/Engine/ConfigParse.hpp` and is covered by 43 new `ValidationTests` checks.
- **Bone restore could write through a rebuilt skeleton (F-06b)** — `EyeAimConstraint` now records the rig root each bone was posed under and only restores if that rig is still the actor's live 3D.
- **Per-actor-per-frame heap allocation in combat (F-10)** — The hostile-combatant scan reuses one buffer.
- **Helgen Opening Cart Scene Head Yaw Defect (+90° Offset)** — Diagnosed root cause from `TrueGaze.log` and `ScreenShot284.png`–`ScreenShot295.png`: `actor->GetSitSleepState()` returns `kNormal` (0) on carriage rigs because prisoners ride via scripted package idle (`CartIdle`). Naive sit checks collapsed back to vehicle road axis ($0^\circ$), adding $+90^\circ$ on top of Ralof's $+90^\circ$ animated chest and rotating his head towards Lokir or the driver. The Torso Divergence Metric decouples the chest heading regardless of engine sit state.
- **Distant Staring & Skyward Ceiling Gaze Defect** — Eliminated $+30^\circ\text{--}+45^\circ$ upward pitch corruption on standing NPCs caused by sampling raw spine bones with negative-Z idle posture curvature.
- **Attention Cone Blindspot & Lateral Side-Step Trigger** — Eliminated $20^\circ\text{--}40^\circ$ lateral heading sway from spine idle animations in `GetObserverHeadingRad`, restoring accurate forward visual cones so NPCs notice and track the player directly in front of them without requiring a 1m lateral side-step.
- **Cervical Snapping & Jumping Across Target / Region / Think Transitions** — Completely eliminated head jumping when entering/exiting THINK mode (CGA aversion), target acquisitions, and scene deferrals.
- **Scene Procedure Dialogue Target Hijacking** — Reordered Section 1c in `TargetSelector.cpp` so live Voice Address Detection (`high->voiceState` & `lastSpokenToArray`) and active player dialogue holds take precedence over secondary scene procedure headtrack slots.

### Security

- **Named pipe now rejects remote clients (F-08)** — `PIPE_REJECT_REMOTE_CLIENTS` added to the HCEP bridge pipe mode, complementing the existing user-only DACL.
- **SEH guard no longer swallows every exception (F-05)** — Only data faults raised by TrueGaze's own frame body (access violation, bounds, integer divide/overflow, escaped C++ exceptions) are handled; stack overflow, heap corruption, breakpoints, etc. go to the game / crash logger. After 3 handled faults a circuit breaker disables TrueGaze for the session with one HUD message and one log line instead of faulting every frame.

### Changed

- **Single-source version + CI parity (F-07)** — `scripts/Get-TrueGazeVersion.ps1` reads `project(VERSION)`; `Package-Release.ps1`, `PackageMod.ps1` (both previously hardcoded 1.0.5) and `Invoke-CiGate.ps1` use it. The CI gate also checks `PRD.md` and `ROADMAP.md`, and `.github/workflows/build-test.yml` now runs `Invoke-CiGate.ps1` itself.
- **Audit record** — `docs/AUDIT_REPORT_2026-10-15.md` and `docs/IMPLEMENTATION_PLAN_2026-10-15_AUDIT_REMEDIATION.md`.

## [1.0.8] - 2026-10-04 - OAR Conditions API V3, Biometric Privacy Hardening & Charter Realignment

### Added

- **OAR Conditions API V3 Integration (GitHub Issue #6)** — Vendored official Open Animation Replacer Conditions API (interface V3, upstream commit `f4e7688`) in `extern/OpenAnimationReplacer-API/`. Subclassed `Conditions::CustomCondition` to register three native condition evaluators (`TrueGaze_IsMode`, `TrueGaze_IsMutualGaze`, `TrueGaze_GetGazeRegion`) during `kPostLoad`.
- **Thread-Safe Actor Gaze Cache & Staleness Guard (GitHub Issue #6)** — Re-architected OAR cache with timestamping and 2.0-second staleness timeout (`STALE_AFTER_SEC = 2.0f`). Stale actor records cleanly evaluate to false/unknown instead of persisting obsolete gaze states.
- **Biometric Named Pipe Client Audit Logging (GitHub Issue #7 / Law 6)** — Named pipe server now queries and logs client process ID, session ID, process image name, pipe security posture, and biometric retention policy upon connection, plus disconnect audit summaries (accepted, integrity-rejected, and semantically rejected frame counts).
- **Biometric Tracked Person ID Privacy Opt-In (GitHub Issue #7 / Law 6)** — Hardened biometric frame handling to unconditionally zero `trackedPersonId` after integrity checks unless explicitly enabled via `[Bridge] bRetainTrackedPersonId=true` in `TrueGaze.ini`.

### Fixed

- **OAR Condition Registration False-Success (GitHub Issue #6 / Law 7)** — Removed the non-functional SKSE messaging "hook" which broadcast dummy message types that OAR never listened for while falsely logging registration success.
- **Core Tenets Charter Divergence (GitHub Issue #8)** — Restored the canonical phrasing of the Four Core Tenets from `Permanent_Active_Directives.txt` in both `AGENTIC_PRIME_DIRECTIVE.md` and `AGENTIC_SACRED_COVENANT.md`. Cleared known divergence overrides in `config/charter_manifest.json`; strict charter verification (`scripts/verify_charter.py --strict`) passes cleanly.
- **OAR Integration Documentation (`docs/OAR_INTEGRATION.md`)** — Updated the guide to reflect the actual OAR Conditions API V3 components, corrected all 13 HCEP gaze regions to match `src/Engine/GazeRegion.hpp`, documented the in-game editor workflow, and clarified that TrueGaze exposes condition hooks but does not bundle `.hkx` animations.

## [1.0.7] - 2026-10-03 - Gaze Arrow & Panel Calibration, Geometry Engine & Universal Coverage

### Added

- **Gaze Geometry & Region Core (`src/Visuals/GazeGeometry.hpp`, `src/Engine/GazeRegion.hpp`)** — Pure SDK-free mathematical foundation for 3D arrow ray scaling, binocular convergence, ray-panel intersection, and HCEP-02 angular region classification. Decoupled from SKSE/CommonLibSSE for standalone testability.
- **Eyeball Cross-Section Calibration (`fArrowCrossSectionMm`)** — Calibrated the widest arrow cross-section (the 160-unit arrowhead) directly to adult human eyeball diameter (default 24.0 mm = 1.68 Skyrim units), configured via INI key `fArrowCrossSectionMm`. Arrowheads now visually read as the eyeball's own line of sight rather than oversized beams.
- **Binocular Gaze Convergence** — Dual gaze rays now dynamically converge at a fixation point along the cyclopean line of sight at reach distance, replacing parallel projection with physiologically authentic convergence.
- **Standalone Geometry Test Suite (`tests/GeometryTests.cpp`)** — 14th test executable verifying column scaling, eyeball dimensions, convergence geometry, ray-plane intersection, angle-to-panel projection, and region classifier parity.
- **Comprehensive Calibration Architectural Suite (`docs/`)**:
  - `docs/Region Map Specification — HCEP-02 Gaze Regions.md` (Approved 2026-10-03)
  - `docs/Technical Design — Gaze Arrow & HCEP Panel Calibration and Hit Detection.md` (Approved 2026-10-03)
  - `docs/Trace Log Schema — TrueGaze Gaze Diagnostics.md` (Approved 2026-10-03)
  - `docs/Verification & Test Plan — Gaze Arrow & HCEP Panel Calibration.md` (Approved 2026-10-03)
- **HCEP Floating Diagram Panel (`GazeRegionPanel.nif`)** — The 3D chroma-keyed HCEP-02 cognitive gaze diagram floats in front of actors with dynamic active-region highlighting (Third-eye, Left/Right eye, Mouth, Chest, Far Upper/Lower regions), verified live in running Skyrim tavern sessions (`ScreenShot164.png`, `ScreenShot177.png`).
- **Universal Actor Coverage** — `bHcepPanelAllActors=true` enabled across compile-time defaults and INI so the floating diagram panel is active across the Player and all NPCs.
- **Adjusted Panel Forward Offset** — Increased `fHcepPanelForwardOffsetCm` from 35.0 cm to 70.0 cm (~49 Skyrim units) to position the panel cleanly in front of NPC faces and prevent clipping into the skull or neck during dialogue, gestures, or leaning.
- **Ray–Panel Physical Hit Detection & Accuracy Tracking (Phase 2 A7)** — Implemented `IntersectGazeWithPanel` in `GazeGeometry.hpp` and integrated real-time ray-panel hit detection into `VisualEffectsManager.cpp`. Measures physical intersection coordinates on the floating HCEP diagram panel, extracts equivalent gaze angles, and tracks three-way agreement between intended, classified, and ray-hit regions. Live accuracy metrics are printed by `stgstatus` and captured in session logs.
- **Panel Visual Feedback & Reticle Hotspot (A7.4)** — Terminus illumination dynamically positions on the floating HCEP diagram panel surface at the exact hit coordinate `(hitX, hitZ)`, and the emissive panel glow reflects real-time three-way agreement (vibrant regional hue) versus boundary divergence (amber warning alert).
- **In-Engine Calibration Command Suite (`stgcal`, `stgcalsweep`, `stgcaloff`, `stgcalaxes`) (§8)** — Added `stgcal` (step through 11 canonical region representative angles on targeted or all actors), `stgcalsweep` (automated 22-second sweep holding each region for 2.0s with live trace telemetry and summary report), `stgcaloff` (restore autonomous gaze), and `stgcalaxes` (verify head bone local X=Right, Y=Forward, Z=Up basis). Enabled via `bEnableCalibrationCommands` in `[Debug]`.
- **Structured JSONL Gaze Trace Logger (Phase 3 A8.1)** — Implemented high-throughput asynchronous trace logger (`src/Engine/TraceLogger.hpp` & `.cpp`) conforming to `docs/Trace Log Schema — TrueGaze Gaze Diagnostics.md`. Emits per-frame `GAZE_TICK`, `SACCADE_ONSET`, `SACCADE_COMPLETE`, `REGION_CHANGE`, `CGA_ENTER`, `CGA_EXIT`, `BLINK`, `HIT_MISMATCH`, and `CALIBRATION` events into `TrueGaze_GazeTrace.jsonl`. Features dedicated background I/O worker thread, 256-event / 1.0s amortized flush cadence, and 100 MB hard cap safety auto-stop.
- **Trace Console Control (`stgtrace`, `stgtraceoff`, `stgtraceflush`) (A8.2)** — Added console commands to start, inspect, stop, and flush live trace logging without restarting Skyrim. Supports targeting specific actors clicked in the console. Added full `[Debug]` INI configuration options for trace filtering and buffer sizing.
- **Automated Trace Log Analysis Tool (`scripts/analyze_trace.py`) (A8.3)** — Built Python validation tool to parse JSONL trace logs, compute dwell distributions and saccade metrics, calculate three-way agreement rates (target $\ge 99.5\%$), and detect biomechanical anomalies with optional `--strict` CI enforcement.
- **Standalone Trace Test Suite (`tests/TraceTests.cpp`)** — 15th test executable verifying trace logger lifecycle, exact schema formatting, saccade and region events, CGA, blinks, hit mismatches, calibration, and actor filtering.
- **Interactive Eye Kinematics Simulator** — Added live canvas-based eye kinematics simulation to `TrueGazeConfig.html` and synced identically to `skyrim/TrueGazeConfig.html`.

### Fixed

- **Matrix Axis Basis Column Scaling (Fix A1)** — Resolved critical `NiMatrix3` rotation bug in `VisualEffectsManager.cpp` where row-scaling was squashing head-space X/Z rather than mesh-local X/Z. Row-scaling was inadvertently shearing the gaze ray toward head +Y and compressing deflections ~11x (causing up to 32.97° directional error). Fixed by scaling basis matrix columns 0 and 2 (`G::ScaleBasisColumns`).
- **Cyclopean Eye Panel Centering (Fix A6)** — The floating HCEP panel is now anchored directly to the cyclopean eye midpoint (`eyeMidLocal`) rather than the head-bone origin (0, forward, 0), guaranteeing angular ray projections land on exact physical diagram coordinates.
- **SKSE Version Multi-Split** — Synchronized `src/Main.cpp` `SKSEPluginInfo` export from stale `1.0.5.0` to `{1, 0, 7, 0}` and updated console banner from `v1.0.0` to `v1.0.7`.
- **CI Workflow & Preset Aliasing** — Updated `.github/workflows/build-test.yml` to use `standalone` preset (configured, built, and tested in seconds with 0 external dependencies) and updated `Invoke-CiGate.ps1` to dynamically resolve version from `vcpkg.json` and cleanly map build/test presets.
- **Truthful OAR Logging (`LAW7-TRUTHFUL-LOG`)** — Clarified `OarConditions.cpp` log output upon dispatching custom condition registration message.

## [Unreleased] - 2026-10-01 - Category Gaze Profiles & Configuration Truth (R15)

> Execution of `docs/IMPLEMENTATION_PLAN_2026-09-27_CONFIGURATOR_CATEGORY_PRESETS.md`
> (roadmap Phase R15), from the configurator deep audit
> (`docs/AUDIT_REPORT_2026-09-27_CONFIGURATOR.md`). All changes verified:
> standalone ctest 3/3 green (13 test suites), full SKSE plugin builds and
> links against CommonLibSSE-NG v7.5.4, configurator parity gate green.
> Default parity preserved: `bEnableCategoryProfiles=false` = exact pre-R15
> behaviour.

### Added

- **Category Gaze Profiles (C1/C2)** — every entity CLASS in Skyrim now gazes
  like itself. New `GazeCategory` enum (18 categories: Player, Guard, Child,
  Vampire, Werewolf, Khajiit, Argonian, Elf, Orc, Creature_Predator/Prey/
  Dragon, Undead, Daedra, Construct, Other) gathered via safe base-form reads
  (race editor-ID match, `TESForm::LookupByEditorID` keyword cache — no
  relocations). `CategoryProfileBundle` multiplier bundles layer ON TOP of the
  temperament axes (Khajiit feline darting, Argonian reptilian stillness,
  vampire unblinking lock, predator stalk, prey skitter, dragon imperious
  stare, draugr hollow stare, Dwarven mechanical fixation...), applied after
  temperament + combat, before clamps.
- **`[Profiles]` INI section (C2.2)** — 31 keys: `bEnableCategoryProfiles`
  master + per-category high-signal multipliers (aversion rate, fixation scale,
  path randomness). Absent keys keep compiled defaults; all clamped [0.1..3]
  in `Sanitise()` with logged warnings. Mirrored in the configurator with full
  rich tooltips.
- **Player play-style bias (C2.5)** — the player's equipped state (dagger/bow
  = stealth-leaning, two-hander = heavy-combat) biases their own gaze profile:
  wary watchfulness vs steady directness. Equipped base-form reads only;
  0 = parity.
- **`stgpreset` / `stgreload` console commands (C4.1)** — `stgpreset` cycles
  the five re-calibrated Quick Presets at runtime (writes INI + refreshes
  tuning in one consistent pass); `stgreload` re-reads `TrueGaze.ini` from
  disk. Eleven `stg*` commands now registered.
- **Configurator parity gate (C4.2)** — `scripts/Test-ConfiguratorParity.ps1`
  (standalone) + stage 6 of `scripts/Invoke-CiGate.ps1`: parses the HTML
  DEFAULTS block and diffs the shipped INI (bool/numeric/pipe-path
  normalisation), and verifies the root/skyrim HTML copies are identical.
  F1-class default drift is now unshippable.
- **13th unit-test suite `TestCategoryProfiles`** — neutral-bundle parity,
  18-category stereotype sanity, bundle application order, predator/prey
  creature split, play-style bias direction, extreme-bundle clamps.

### Changed

- **Relationship axis re-enabled (C1.3, audit F5)** — `GatherTemperament` now
  reads the relationship rank through the R14 E7.6 proven-safe path (full
  null-guard chain + const-correct `BGSRelationship::GetRelationship`).
  Lovers hold eyes longer; enemies lock. History comment preserved.
- **Quick Presets re-calibrated (C3.2, audit F2)** — all five global presets
  re-derived from the v1.0.6 verified baseline. The calm/combat speed model
  means INI speed keys are the COMBAT ceiling (calm = ×0.25 at runtime), so
  the presets no longer double-count a slowdown. 12° head-engage floor in all
  non-developer presets (the verified anti-twitch value). Developer preset =
  the shipped INI exactly (testing fidelity).
- **HTML DEFAULTS synced to the shipped INI (C3.3, audit F1)** —
  `fSaccadeSpeedMult` 1.0→1.5, `fVelocitySaturation` 14→18,
  `fMicroJitterAmp` 0.35→0.28, `fHeadOnsetDelaySec` 0.12→0.13,
  `fHeadEngageThresholdDeg` 8→12, `fPupilGlowIntensity` 0.5→0.35,
  `bGazeRaysTerminus` false→true, `bEnableConsoleCommands` false→true
  (matched to the INI per Kirk's decision).
- **Tooltip truth pass (C3.4, audit F9)** — stale weight recommendations
  corrected to the eye-dominant values (`fHeadYawWeight` rec 0.60-0.70 →
  0.20-0.30, dflt 0.65 → 0.245; spine/neck weights likewise; head-engage
  rec 6-12 → 12).
- **INI comment cleanup (C4.3, audit F7/F10)** — the `[Visuals]` GazeBeam
  comment now states the NIF was RETIRED and removed in v1.0.6; the
  `[Console]` block deduplicated with the full `stg*` command list.

### Fixed (documentation truth, audit F6)

- `docs/STATUS.md` claimed `stgpreset`/`stgreload` console commands that were
  never registered. Both now exist (C4.1) and the claim is true.

## [1.0.6] - 2026-09-27 - Engineering Excellence Implementation

> Execution of `docs/IMPLEMENTATION_PLAN_2026-09-26_ENGINEERING_EXCELLENCE.md`
> (roadmap Phase R14). Builds on the 2026-09-26 audit entry below. All changes
> verified: standalone test suite 3/3 green, full SKSE plugin builds and links
> against CommonLibSSE-NG v7.5.4.

### Added

- **License resolution (E6.1)** — `docs/LICENSE_RESOLUTION.md` analysis; LICENSE
  confirmed GPLv3 (forced by the CommonLibSSE-NG GPLv3 dependency) with a
  project notice: copyright Kirk LaSalle 2026, SCOPE section excluding HCEP
  theory as proprietary trade secret, honest biometric-data notice. README
  badge corrected to GPL-3.0.
- **`tests/ValidationTests.cpp` (E3.3)** — telemetry-validation fuzz-lite:
  10,000-mutation deterministic corpus (byte flips with CRC recompute, NaN, Inf,
  enum range, reserved bytes, magic/version) asserting `ValidateTelemetryPacket`
  rejects malformed input. SDK-free; runs in standalone mode.
- **Test infrastructure (E3.4)** — `enable_testing()` + CTest registration for
  KinematicsTests, ValidationTests, HcepBridgeClientMock in both standalone and
  plugin builds; `truegaze_force_asserts()` keeps asserts live in Release-built
  tests (`/MDd`, `/Od`, `/RTC1`, `_DEBUG`).
- **Quality configs (E4.1)** — `.clang-format` (4-space, Allman, 100-col),
  `.clang-tidy` (bugprone/performance/modernize, conservative), `.editorconfig`.
- **CI (E4.2)** — `.github/workflows/build-test.yml` (windows-latest, vcpkg
  cache, configure→build→ctest) and `scripts/Invoke-CiGate.ps1` local gate
  (configure, build, test, charter-manifest verify, doc-consistency spot check).
- **`src/Engine/GazeAnchors.hpp` (E7.5)** — shared eye-anchor geometry; the
  byte-identical `EyeAnchorFromHeadBone` copies in GazeEngine.cpp and
  TargetSelector.cpp consolidated, and the 125-vs-160 eye-height disagreement
  (PlayerGazeResolver) resolved to the canonical 125.
- **CMake test presets (E4.2)** — `testPresets` for debug/release/standalone so
  `ctest --preset` works for CI and the local gate.

### Changed

- **`NamedPipeServer::Stop()` (E1.1)** — `JoinPolicy` enum {Join, Abandon};
  Join unconditionally joins the worker and closes all handles (fixes the
  use-after-free detach path); Abandon deliberately leaks at process exit.
  `Main.cpp` atexit uses Abandon. Double-Stop is a safe no-op (stress-tested).
- **Triple-buffer memory model (E1.2)** — full happens-before proof documented
  in `NamedPipeServer.hpp`; 30k-publish stress test with per-packet CRC +
  sequence-regression assertions added to the bridge mock.
- **Drain-event churn (E1.3)** — `DrainOutboundQueue` reuses a member
  `_drainEvent` instead of CreateEvent/CloseHandle per call; security warning
  rate-limited. HCEP_BRIDGE_SPEC gained a Security & Threat Model section.
- **`PerformanceProfiler` made real (E2.1)** — atomic fetch_add ScopedTimer,
  CAS peak tracking, `RecordFrame()` wired into `GazeEngine::EndFrame`; the
  over-budget warn threshold now derives from `kFrameBudgetUs` (×10
  conservative) instead of a divergent literal; `stgstatus` prints
  last/peak/budget.
- **Cull before resolve (E2.2)** — `TickActor` LOD cull moved ahead of
  ComputeDeflection so culled actors no longer pay solver cost.
- **Per-frame scan cache (E2.3)** — `TargetSelector` rebuilds its candidate
  position cache once per frame (frame-tagged) and iterates cone+distance only,
  removing the per-observer O(N) position re-resolution from the social scan.
- **Per-actor SEH isolation (E7.1)** — each `TickActor` call wrapped in its own
  `__try` frame; a defective actor skips exactly one actor instead of aborting
  the whole list for the frame.
- **EyeAimConstraint frame generation (E7.2)** — `g_frameOpen` bool replaced by
  a monotonic `g_frameGeneration` counter; touched-bone table capacity (512)
  warns once per saturation episode.
- **Emitter cap logging (E7.3)** — the silent 64-actor visual emitter cap now
  warns once per saturation episode and re-arms when capacity frees.
- **Singularity gate alignment (E7.4)** — `WorldTargetToLocalGaze` point-blank
  gate reduced 25→4 units to match `IsInVisualCone`'s 4-unit guard; NPCs 4-25
  units away are no longer forced to stare dead ahead while still being
  selected as targets.
- **Feedback packet semantics (E7.6)** — `relationshipRank` now reports the
  real `BGSRelationship` level mapped onto the documented -4..+4 axis (was
  hard-coded 0); `gameFrameNumber` reports the engine's monotonic frame counter
  (was 0).
- **TrueGazeAPI lodTier (E7.7)** — telemetry `lodTier` now comes from the real
  `LodManager` classifier (was an eye-saturation placeholder); contract docs
  added to `include/TrueGazeAPI.h`.
- **Trivial batch (E7.8)** — dead `startYaw` removed; `fGazeRayThicknessCm`,
  `fGazeRayLengthMeters`, `fGazeRayOpacity` clamped in `Sanitise`; dialogue-menu
  singleton lookup cached once per frame; frame-scope scratch members grouped
  into `GazeEngine::FrameScope`.
- **CMake hygiene (E4.4)** — explicit plugin source list (replaces
  `file(GLOB_RECURSE)`), existence-checked; VCPKG_ROOT configure check with an
  actionable message; `vcpkg.json` version-string synced 1.0.0 → 1.0.5.

### Fixed

- `/O2` vs `/RTC1` incompatibility (D8016) in Release-built test targets —
  force-asserts now also applies `/Od`.
- `spdlog::debug` missing from the standalone PCH logging fallback.
- Unused `writerFn` lambda and dead `publishes`/`kPublishCount` in the bridge
  mock stress section; the sequence-regression check now actually sets and
  reports the `torn` flag.

### Known open (tracked, not fixed in this entry)

- E3.1 TargetSelector pure-logic extraction (heavy refactor, deferred).
- E3.2 Sanitise unit tests (Sanitise is SDK-bound via logger; needs extraction).
- E2.4 benchmark artifacts to replace the conservative ×10 budget threshold.
- E5 documentation truth pass (PRD 1.0.3/C++20 staleness, STATUS tier sums).
- E6.2 retired `GazeBeam.nif` removal from the shipping tree.

## [Unreleased] - Engineering Excellence Audit - 2026-09-26

> **Full world-class codebase audit** (`docs/AUDIT_REPORT_2026-09-26.md`) plus a
> companion engineering plan (`docs/IMPLEMENTATION_PLAN_2026-09-26_ENGINEERING_EXCELLENCE.md`,
> roadmap Phase R14). Documentation-only release: no code changes in this entry.

### Added

- **`docs/AUDIT_REPORT_2026-09-26.md`** — full codebase audit: ~7,900 lines of
  project-owned C++ inventoried; architecture, concurrency, code-quality (12
  structural + 10 correctness findings with file:line references), test coverage
  gaps, build system, performance (O(N²) crowd risk identified), security,
  tooling, governance, and a prioritized P1/P2/P3 findings register.
- **`docs/IMPLEMENTATION_PLAN_2026-09-26_ENGINEERING_EXCELLENCE.md`** — six-phase
  plan (E1 concurrency proof, E2 performance, E3 test coverage, E4 build/CI,
  E5 documentation truth, E6 legal/packaging, E7 robustness batch).
- **ROADMAP Phase R14** — the engineering-excellence phase, now the current
  milestone.

### Fixed (documentation drift — audit §8)

- ROADMAP Phase 1: stale "known gaps" block (smoothstep trajectory, fixed seed,
  no eye residual) marked as resolved-by-R3 with a historical note; status raised
  to ~95%.
- ROADMAP Phase 3: stale "telemetry is consumed by nobody" and "data race present"
  warnings replaced with the resolved truth (triple buffer + S4 fusion) and a
  historical note; remaining open items (memory-model stress test, shutdown
  lifecycle) tracked in R14 E1.
- ROADMAP Phase 4: OAR status clarified — registration against a pinned API
  version and a proven-firing rule remain open (SOTA S6).
- ROADMAP R2: stale "not yet observed in-game" deliverable replaced with the
  verified truth (field-verified September 25-26); the open try/catch item marked
  superseded by the R13 SEH guard.
- ROADMAP R6 publication gate: contradiction with R7's completed release resolved
  with a historical note; open items carried into R14 E5/E6 and S5.2.
- ROADMAP R8: "Current Milestone" header corrected (R14 is current); VR claims
  scoped with an honesty note — `VrController` is an approximate pose, not an
  OpenVR HMD/eye-tracking feed, until the S7 contract is implemented.

### Known open (tracked, not fixed in this entry)

- LICENSE is GPLv3 but the README badge still reads "Proprietary" — decision
  pending (R14 E6.1).
- `NamedPipeServer::Stop()` detach path use-after-free risk (R14 E1.1).
- `PerformanceProfiler` dead code; 150 µs budget claim unmeasured (R14 E2.1).
- `TargetSelector` has zero tests; O(N²) social scan (R14 E2.3/E3.1).
- Retired crash-causing `GazeBeam.nif` still in the shipping tree (R14 E6.2).
- PRD version (1.0.3) and C++20 claims stale vs v1.0.5/C++23 (R14 E5.1).
- STATUS.md tier percentages sum to 110%; header/footer dates disagree (R14 E5.1).

## [Unreleased] - Gold Standard Scene Integration - 2026-09-26

> **The Gold Standard: flawless integration of TrueGaze during the Helgen opening
> scene — which makes it correct for ANY in-game directed animation, scene, or
> OAR-driven mod.** This release hardens the scene-integration contract, fixes a
> sticky-defer defect that silently disabled TrueGaze after any NPC-to-NPC scene,
> makes the gaze frame SEH-fault-tolerant (a defect in TrueGaze can no longer take
> the game down for ANY failure class), and gives the plugin sole ownership of its
> log so no crash can ever again be invisible.

### Fixed — Sticky scene defer (head chain permanently yielded)

- `sceneDeferActive` was set when an NPC was directed at another NPC and **never
  cleared when the scene ended**. After any NPC-to-NPC staged moment, TrueGaze's
  head chain stayed yielded to vanilla forever — the actor silently stopped
  receiving TrueGaze gaze for the rest of the session.
- The flag is now a **per-frame decision**: cleared at the top of every target
  resolution, re-set only by the head-track classification below. The moment a
  scene releases the actor, TrueGaze re-engages on the very next frame.

### Changed — Scene defer generalised to all direction head-track slots

- The defer check previously used the single `AIProcess::GetHeadtrackTarget()`
  relocation call, which reads only the default slot. It now reads the
  `HighProcessData::headTrackTarget[]` array directly — a pure data-side read,
  no engine dispatch — across all six slots with per-slot semantics:
  - **kDefault** (ambient AI glances) and **kCombat** (combat targeting) are NOT
    direction — TrueGaze continues to own those gazes.
  - **kAction**, **kScript** (quest/mod LookAt — the OAR/scene-mod surface),
    **kDialogue**, and **kProcedure** (package procedures — the Helgen cart's
    mechanism) ARE direction: vanilla is authoritative, TrueGaze yields the head
    chain, eyes never yield.
  - When the directed target is the player, TrueGaze takes hold with eye-anchor
    precision (unchanged).
- This is the generalisation that extends the cart-scene contract to any
  directed animation, scene, or mod.

### Changed — SEH fault tolerance for the gaze frame

- Every hard crash in this project's history has been an SEH access violation —
  a failure class C++ `try/catch` structurally cannot catch (documented as a
  known limitation since the first hook). The TrueGaze-owned frame body
  (`BeginFrame → TickActor(player) → TickAllActors → EndFrame`) is now wrapped
  in `__try/__except`: the first faults log the exception code and faulting
  address (with the PDB, the exact line), the frame is abandoned, and **the
  game continues**. Skyrim's own update call remains outside the frame, as
  always — engine code is never swallowed.

### Changed — Logging ownership (no crash can be invisible again)

- The 2026-09-25 evening session crashed during data load and `TrueGaze.log`
  showed nothing after kPostLoad: `SKSE::Init()`'s default `InitInfo` had
  re-truncated the log file and replaced the default logger, silencing the
  plugin from its own diagnostics.
- The plugin now installs its logger BEFORE `SKSE::Init` and passes an explicit
  `InitInfo{log=false}` so SKSE never opens, truncates, or replaces it. One
  sink, one pattern, one level policy, from load to exit.

### Added — Scene-defer diagnostics

- `stgstatus` now reports `scene defer ACTIVE (head chain yielded)` or
  `inactive` plus a lifetime yield-frame count.
- The periodic target-trace log line appends `scene=DEFERRED` while deferring,
  so acceptance logs for any directed scene are self-describing.

### Added — Player parity & voice-address detection (field-test rounds 3-6)

- **Player parity in directed scenes** (Kirk directive: "The Player should be
  treated the same as the NPC especially when directed to from vanilla
  Skyrim"): the direction-slot scan is two-pass — a slot aimed at the PLAYER
  wins over any slot aimed at another NPC. The player is also exempt from the
  body-yaw cone check in the dialogue branch (seated sideways actors in the
  cart had the player outside their cone): a person addressing you looks at
  you, full stop.
- **Voice-address detection**: the game stages LOOK direction independently of
  SPEECH — during scene-delivered lines the head-track slots pointed at other
  NPCs even while the NPC spoke to the player. TrueGaze now reads the speech
  itself: `HighProcessData.voiceState` (kStart/kContinue = speaking now) +
  `lastSpokenToArray` (who the speech was delivered to). An actor speaking to
  the player looks at the player — pure data-side reads, no relocation calls.
- **Dialogue player hold**: a 3-second latch re-serves the player target
  through the direction signal's mid-line flicker, so an NPC speaking to the
  player keeps looking at the player for the whole line.
- **Defer player glance**: while deferring, the player within 0.4-4 m and a
  150° cone gets an eyes-only glance — a person an arm's length away is never
  ignored.

### Added — Calm/Combat speed model & eye-to-eye dominance (field-test rounds 2-8)

- **Calm/Combat speed model** (Kirk directive: "slowed down by at least 50%…
  faster speeds are for combat scenarios and any high intense action", then
  "move the baseline down at least another 50%"): the tuned INI speeds are the
  COMBAT ceiling. The calm baseline runs at ONE QUARTER of them — pursuit
  glide ×0.25, saccade duration ×4, scanpath cadence ×8 (with the extra dwell
  directive), cervical slew caps ×0.25. Combat restores full speed instantly.
- **Eye-to-eye fixation dominance** (Kirk directive: "looking into character
  eyes is important and must last longer before shifting, and it MUST happen
  when dialogue occurs"): holds on LeftEye/RightEye are multiplied 2.5×
  baseline / 4.0× during dialogue, in every scanpath stepper. Calm dialogue
  eye holds ≈ 6-17 s; other regions stay brief glances between eye holds.
- **Eye-lock bias**: eye-to-eye transition probability 0.45→0.60; eye vertices
  carry a structural 2.0× weight in profile-weighted selection — gaze lands
  on the eyes more often per cycle.
- **Head catch-up**: the head chain runs at 1.54× the calm eye rate (graceful
  S-curve preserved; +10% after round-8 verification).
- **Graceful region transitions**: the catch-up saccade threshold is 45° when
  calm (20° in combat), so all region-to-region motion flows through the
  exponential pursuit glide — no ballistic snapping between regions.
- **Engineering lessons fixed en route** (documented in the session record):
  Vmax is diagnostic-only in a unit-area-normalised velocity profile (duration
  is the real speed lever); `2.0f * speedScale` was a silent no-op at
  speedScale 0.5 (reciprocal `1.0f / speedScale` is the correct inversion); a
  per-frame overwrite of the scanpath's fixation duration defeated the cadence
  lever (subsystems own their internal timing; the engine seeds once).

## [1.0.6] - 2026-09-25

### Added — Character Gaze Profiles: Temperament-Driven Gaze (Phases C1-C5)

> **Every NPC now looks like THEMSELVES.** TrueGaze reads Skyrim's own characterization
> — the Confidence, Aggression, and Assistance actor values, relationship ranks,
> guard/child/race flags, and combat state — and projects it onto the HCEP-02 Enhanced
> Diagram as behavioural multipliers. A cowardly merchant, a foolhardy guard, a lover,
> and a wolf each gaze differently, grounded in published gaze psychology.

#### 🎭 The Spectrum (no NPC is a "type")

Every entity is a point in (Confidence × Aggression × Relationship × StoryState) space:

| Entity | Gaze behaviour |
| --- | --- |
| Cowardly merchant (Confidence 0) | 1.45× aversion rate, brief eye contact, darting glances, Lower-Right (shyness) region bias |
| Foolhardy guard (Confidence 3 + IsGuard) | 0.55× aversion, unflinching lock, steady dwells |
| Lover (relationship +4) | Long mutual-gaze holds, Chest (Heart) region visits, HEART bias |
| Enemy (relationship −4) | Hostile lock — stares, doesn't look away, LOGIC bias |
| Child | Quick curious scanning, wide saccades |
| Creature (wolf) | No social triangle — fixation-dominant animal attention |
| Anyone in combat | Hard lock: minimal aversion, LOGIC, long fixations |

#### 🔬 Implementation

- **`src/Engine/CharacterProfile.hpp`** — pure `Classify(TemperamentInput) → GazeProfile`
  classifier (no SDK in the math; SDK adapter `GatherTemperament` reads AVs,
  `BGSRelationship::GetRelationship`, and archetype flags). All multipliers clamped
  [0.1..3]. **DEFAULT PARITY: disabled = all 1.0 = exact pre-profile behaviour.**
- **`SocialTriangle`** — new `NextWeightedVertex` / `UpdateWeighted`: profile vertex
  weights turn the HCEP-02 diagram's region semantics into actual visit probabilities.
- **`GazeEngine`** — per-actor profile cache (refreshed on combat edges only, never
  per frame); fixation scale, triangle enable, weighted vertices, and CGA aversion-rate
  gating all profile-modulated.
- **Player behavioural profile** — the player's own face-attention (crosshair holds on
  faces) accumulates and warms the player's 3rd-person gaze: longer mutual-gaze holds,
  slower scanning. The character reflects the player's behaviour. (1st person is
  unaffected — eyes-only control, sacred invariant.)
- **Dev panel** — the floating HCEP diagram already lights the active region with the
  diagram's colours (green LeftEye, orange RightEye, pink Mouth, purple ThirdEye,
  crimson Chest, cyan CGA peripherals).
- **Config**: `[CharacterProfile] bEnableCharacterProfiles=1` (INI + configurator
  "Character Gaze Profiles" panel with rich tooltip).

#### 📚 Psychology grounding

- Gaze avoidance ↔ low confidence/shyness (Shackelford et al., PAID 1996)
- Affective eye contact effects (Frontiers in Psychology 2018)
- Personality shapes gaze patterns (SAGE QJEP 2025, 116-participant eye-tracking)
- Gaze aversion = cognitive-load management (PMC8188832)

#### ✅ Verification

- 12th unit-test suite `TestCharacterProfile`: default parity, confidence ordering,
  lover/enemy axes, creature fallback, combat lock, clamps, and a 20,000-sample
  weighted-vertex distribution check. **All 12 suites pass.**

## [1.0.5] - 2026-09-25

### ✅ LANDMARK: Eye-to-Eye Targeting & Eyes-Lead-Head — IN-ENGINE VERIFIED (September 25, 2026)

> **Kirk LaSalle directly observed NPCs looking into the player's eyes (and each other's
> eyes) in live Skyrim gameplay for the first time.** Tavern NPCs tracked the player's
> eyes with visible FaceGen eye-morph movement leading the head. The foundational science
> — eye-anchor targeting, FaceGen Look-morph gain, head-onset delay, and head-engagement
> threshold — is **proven correct in-engine**.
>
> This milestone marks the transition from "eyes move" (verified September 2026) to
> **"eyes look into eyes"** — the core promise of TrueGaze.

#### 👁️ Eye-Anchor Targeting — "The Eyes ARE the Target"

- **Root cause found and fixed**: gaze was aimed at the `NPC Head [Head]` bone origin,
  which sits at the base of the skull — 5-8 cm below and behind the eyeballs. NPCs looked
  at the throat or nose bridge, never the eyes.
- **New `EyeAnchorFromHeadBone()` projection**: lifts the aim point onto the eyeline by
  projecting along the head bone's own world basis vectors (+Y forward, +Z up via
  `NiMatrix3::GetVectorY/Z`). Tracks seated, leaning, and crouched poses (including bound
  prisoners in the Helgen cart) because it follows the bone's actual orientation, not a
  fixed world-space offset.
- **Applied to BOTH sides**: the target actor's eye point AND the observer's ray origin
  now use the same anchor, so the solve is genuinely **eye-to-eye**, not throat-to-forehead.
- **New `[GazeTarget]` INI section** with tunable offsets: `fEyeAnchorForwardCm` (8.0),
  `fEyeAnchorUpCm` (8.5), `fEyeMorphGain` (1.7), `fEyeMorphFullScaleDeg` (20.0).
- **Files**: `TargetSelector.cpp/hpp`, `GazeEngine.cpp`, `GazeTuning.hpp`,
  `ConfigManager.hpp/cpp`, `TrueGaze.ini`, `TrueGazeConfig.html` (both copies).

#### 👁️ FaceGen Eye-Lead — Eyes Clearly Move First on Vanilla NPCs

- **Critical discovery**: vanilla Skyrim humanoid rigs have **NO eye bones** — the
  skeleton probe reports `origin=GeometricHeadSocket`, "eyes are FaceGen morphs". The
  ENTIRE "eyes move" path on stock NPCs runs through `EfmBlinkController::ApplyGazeMorphs`
  → FaceGen `LookLeft/Right/Up/Down` modifier morphs. `EyeAimConstraint` on eye bones is
  a no-op for these rigs.
- **Morph sensitivity boosted**: `fullScaleDeg` reduced from 30→20 and visible gain
  (`eyeMorphGain`) added at 1.7×. A 6° social triangle glance now drives the eyes to ~51%
  morph travel — clearly visible at conversation distance without saturating.
- **Head onset delay tuned**: `fHeadOnsetDelaySec` raised to 0.13s (upper end of the
  biological 80-150ms window). The eyes visibly arrive at the target before the head begins
  to follow.
- **Head engagement threshold tuned**: `fHeadEngageThresholdDeg` raised to 12°. The entire
  close-range social triangle (eye↔eye↔mouth at cart/tavern distance) is carried by the
  **eyes only** — zero head twitch.
- **Files**: `EfmBlinkController.hpp/cpp`, `GazeEngine.cpp`, `GazeTuning.hpp`,
  `ConfigManager.hpp/cpp`, `TrueGaze.ini`, `TrueGazeConfig.html` (both copies).

#### 🪑 Point-Blank Seated Actor Fix (Helgen Cart Enabler)

- **Root cause**: `IsInVisualCone` rejected actors closer than 25 units (~0.35m). In the
  tight Helgen cart, seated NPCs sharing the cart origin were rejected → fell through to a
  vacant ambient forward stare.
- **Fix**: singularity rejection lowered to 4 units (~6 cm); actors within 64 units (~0.9m)
  always treated as in-cone regardless of bearing angle. Seated cart passengers now
  correctly resolve each other.
- **File**: `TargetSelector.cpp`

#### 🎯 Player Targeting in Scripted Scenes (Helgen Cart Fix)

- **Root cause**: the social candidate scan explicitly excluded the player
  (`otherActor != player`), evaluating them separately in a lower-priority block. In the
  cart, other NPCs (Ralof, Lokir, Ulfric) always won the distance comparison because they
  sit closer to each other than to the player. The player was never selected as a target.
- **Fix**: the player now enters the same candidate scan as all other actors, competing
  fairly on distance. The player retains their wider visual cone (110° vs 75° for NPCs)
  as a social-salience advantage and evaluates up to 12m (vs 6m for NPCs). In the cart,
  each NPC now naturally glances between the player and other NPCs based on proximity.
- **File**: `TargetSelector.cpp`

#### 🎛️ New Configurator Panel: "Eye Target & Eye-Lead"

- New `[GazeTarget]` section in `TrueGazeConfig.html` with 4 controls:
  - `fEyeAnchorForwardCm` / `fEyeAnchorUpCm` — eye anchor offsets (tune eye-aim precision)
  - `fEyeMorphGain` — FaceGen eye-lead visible gain (tune eye expressiveness)
  - `fEyeMorphFullScaleDeg` — morph full-scale angle (tune saturation point)
- Rich tooltips with anatomical context, impact descriptions, and recommended ranges.
- Added to the `Social & Dialogue` preset for cart/tavern-optimised defaults.

### Added — Eye-Dominant Gaze, Dialogue-Synced CGA Return & Organic Motion

#### 👁️ Eyes Now Lead, Head Follows (Biologically Correct Gaze Distribution)

- **Head chain weights dramatically reduced**: `fHeadYawWeight` 0.65→0.245, `fNeckYawWeight`
  0.25→0.07, `fHeadPitchWeight` 0.55→0.245 (after two tuning passes: −50% then −30%). Eyes
  now receive the **majority** of the gaze deflection as the residual, producing the natural
  "eyes move first, head follows subtly" effect documented in oculomotor literature.
- **Head onset delay increased**: `fHeadOnsetDelaySec` 0.04→0.10 seconds. The biological
  latency gap between eye saccade onset and head following is now visually perceptible,
  matching the ~80-150ms observed in human subjects (Guitton & Volle, 1987).
- **Head engagement threshold** (`fHeadEngageThresholdDeg`, default 8°): for small gaze
  shifts (e.g. social triangle cycling between eyes/mouth at close range), the head stays
  **perfectly still** and only the eyes move. Eliminates robotic micro-head-turns.

#### 🧠 Eye-Dominant CGA (Cognitive Gaze Aversion) — "Embry Fix"

- **CGA aversion is now eye-only**: `BoneController::CalculateCgaStrain()` routes aversion
  deflections almost entirely to the eye bones with minimal head chain involvement
  (`fCgaHeadInvolvement`, default 0.08 = 8% head, 92% eyes). Eliminates the grotesque
  neck-twist observed on Embry when averting gaze.
- **Per-actor dialogue-sync CGA return** (`bDialogueSyncCgaReturn`): when dialogue begins,
  NPCs in CGA aversion snap their gaze back to the speaker's face — the natural "oh, they
  said something" attention capture. Kirk LaSalle's insight: *timing is the sweet spot for
  CGA; the return-to-face timed with dialogue is what makes bots look alive.*
- **Organic offset variation** (`fCgaDialogueOffsetSec`, default ±2.0s): each NPC gets a
  unique random offset per CGA episode. Some return slightly before dialogue (anticipatory
  — sensed the speaker was about to talk), some after (delayed processing — deep in
  thought). Prevents identical crowd reactions.
- **SocialTriangle::ReturnToFace()**: new method forces CGA scanpath back to the dominant
  eye, consumed by the dialogue-sync system.

#### 🎲 Organic Social Triangle Scanpath (No More Repetitive Orbit)

- **Weighted-random path selection** (`fTrianglePathRandomness`, default 0.6): replaces the
  deterministic LeftEye→RightEye→Mouth orbit with weighted transitions — 45% eye-to-eye
  (the transition humans favour), 35% lateral jumps, 20% same-point re-fixations. At 0 the
  classic orbit is preserved; at 1 full free wandering.
- **Per-visit landing scatter**: every fixation lands on a slightly different angle
  (±max(0.18°, 12% of offset magnitude)), so no orbit can ever repeat — the polygon-tracing
  look is gone. Also de-repetitizes CGA aversion points.
- **Extended diagram de-looped** (SPIRIT/HEART): exits from Third-Eye/Chest land on a
  random triangle vertex; the mouth now jumps to either eye.
- **Fixation rhythm widened**: dwell now varies 200–550ms (was 250–450ms).

#### 🌊 Smooth & Graceful Motion (Snappiness Eliminated)

- **Root cause of the snap found**: the smooth-pursuit path teleported the eyes to each new
  fixation in a single frame (`currentYaw = desiredYaw`), and that hard step propagated
  into the head through the VOR. Replaced with an **exponential glide**
  (`fEyePursuitSpeed`, default 12/s) with exact settle below a 0.02° remainder. Eyes and
  head now move fluidly between fixation points.
- **Head 20% slower**: `fHeadTrackingSpeed` 6.5→5.2; cervical slew caps 130→104 deg/s yaw
  and 90→72 deg/s pitch. All presets cut proportionally.

#### 📋 New INI Keys (TrueGaze.ini)

- `[SkeletalHierarchy] fHeadEngageThresholdDeg` — degrees below which only eyes move
- `[Social] fCgaHeadInvolvement` — head chain fraction during aversion (0=eyes-only)
- `[Social] bDialogueSyncCgaReturn` — CGA ends when dialogue begins
- `[Social] fCgaDialogueOffsetSec` — per-actor ±offset around dialogue onset
- `[Social] fTrianglePathRandomness` — 0 = fixed orbit, 1 = free organic wandering
- `[Kinematics] fEyePursuitSpeed` — smooth ocular pursuit glide rate (1/s)

#### 🔌 Public API (TrueGazeAPI.h)

- `ActorGazeTelemetry` gained `gazeRegion` (0..12, HCEP-02 Enhanced Diagram) + 3 reserved
  padding bytes for ABI stability.
- `TrueGaze_GetVersion()` now reports 0x01000500 (v1.0.5).
- Beam region colours in `VisualEffectsManager` re-matched to `ClassifyRegion()` IDs.

### Changed

- All preset weights updated across TrueGazeConfig.html to reflect eye-dominant tuning,
  slower head speeds, and organic path randomness (vanilla 0.6, subtle 0.5, intense 0.7,
  social 0.7, developer 0.6).
- `BoneController` compiled defaults updated to eye-dominant values.
- Unit tests extended: head engagement threshold, CGA eye-dominant strain, deterministic
  orbit preservation at randomness 0, no fixed loop at randomness 1, landing scatter.
- Codebase reformatted (clang-format) across VorCoordinator, TrueGazeAPI, and headers.

## [1.0.4] - 2026-09-23

### Fixed — NPC Gaze Crash (Multi-Actor Beam Geometry) & NPC Looking-Away

#### 🛑 Crash Fix: Beam Geometry SEH Crash Under Multiple Actors

- **Root Cause**: The Python-generated `GazeBeam.nif` (hand-crafted binary NIF) loaded
  via `BSModelDB::Demand` without error but caused a delayed SEH access violation
  (0xC0000005) inside Skyrim's NIF renderer when 7+ actors simultaneously carried beam
  geometry. C++ `try/catch` cannot intercept SEH — the game crashed silently with no
  log entry. Confirmed via `TrueGaze.log` analysis: log stopped cleanly mid-frame at
  16:19:22 with `priority=5` crosshair focus on the bard (actor `0003550C`), no warning.
- **Fix**: Switched primary beam model from `meshes\TrueGaze\GazeBeam.nif` to
  `meshes\dlc01\effects\fxsoulcairnbeam.nif` (proven vanilla Dawnguard Soul Cairn beam
  strip, present in every AE/SE install via `Meshes01.bsa`). The hand-crafted NIF is
  retired until validated in NifSkope.
- **Default render mode**: `iRayRenderMode` changed from `0` (Both) to `1` (LightOnly).
  NiPointLight emitters are proven crash-free across all actor counts. Mode `0` now
  uses the Dawnguard beam (safe), available via the Developer preset or `stgmode` console
  command. `usingFallbackMesh` detection extended to recognise `fxsoulcairnbeam` for
  correct non-uniform scale compensation.
- **Files**: `src/Visuals/VisualTuning.hpp`, `src/Visuals/VisualEffectsManager.cpp`,
  `skyrim/SKSE/Plugins/TrueGaze.ini`, `TrueGazeConfig.html`, `skyrim/TrueGazeConfig.html`

#### 🛑 Behavioural Fix: NPCs Looking Away From Player

- **Root Cause 1 — Unconditional `ClearHeadtrackTarget`**: Every frame, TrueGaze called
  `ClearHeadtrackTarget` on every NPC before writing its own gaze. When the engine fell
  back to `AmbientInterest` (priority=1, `targetFormId=0`) — a vacant point in the air
  ahead of the NPC — it had already destroyed Skyrim's own tracking. NPCs stared at
  nothing rather than the player. Log confirmed: `priority=1 form=00000000` for several
  minutes of gameplay (Embry, Sven, Delphine all looking away in screenshots).
- **Root Cause 2 — Detection ranges too small**: NPC candidate scan capped at 3.5 m,
  player nearby range 8 m, player cone 75°. Log showed player at 5.85 m — beyond NPC
  scan but within player range, yet outside the 75° cone → ambient fallback.
- **Fix 1**: `ClearHeadtrackTarget` now only called when `state.trackedTargetFormId != 0`
  (a real actor target is locked). When only ambient interest is resolved, Skyrim's
  native headtracking is left in place.
- **Fix 2**: NPC candidate scan widened `3.5 m → 6.0 m`; player nearby range
  `8 m → 12 m`; player detection cone `75° → 110°` (covers natural peripheral social
  attention without allowing backwards neck-snap).
- **Files**: `src/Engine/GazeEngine.cpp`, `src/Engine/TargetSelector.cpp`

#### 🔧 Diagnostic Visuals Off By Default

- `bEnableInGameVisuals`, `bGazeRaysEnabled`, `bDebugGazeRays` all default to `false`.
  Log level defaults to `Info` (2) instead of `Debug` (1).
- All non-developer presets (Vanilla, Subtle, Intense, Social & Dialogue) explicitly set
  these keys to `false` so switching away from the Developer preset cleanly disables
  diagnostic overlays.
- The in-game effect of TrueGaze is the NPC's actual head rotation and FaceGen pupil
  morphs (`LookLeft`/`LookRight`/`LookUp`/`LookDown`) — no laser beams required.
- **Files**: `skyrim/SKSE/Plugins/TrueGaze.ini`, `TrueGazeConfig.html`,
  `skyrim/TrueGazeConfig.html`

### Changed — Version Bump

- CMake project version, `SKSEPluginInfo`, and runtime identity log updated to `1.0.4`.
  On launch the log will show: `TrueGaze v1-0-4-0` and
  `Runtime identity: plugin v1.0.4 build Sep 23 2026`.

---

## [1.0.3] - 2026-09-23

### Fixed — True 3D Head-Height Elevation Targeting & Chest Aiming Defect

- **Dynamic 3D Bone Head-Height Elevation Tracking**:
  - *Root Cause Analysis*: Diagnosed from player logs and screenshots (`ScreenShot62`–`ScreenShot67`) that gaze pitch deflections were calculated as flat horizontal ($+0.4^\circ$, $-0.1^\circ$), causing seated NPCs (Camilla Valerius) to stare horizontally straight ahead into the standing player's chest, and counter-leaning NPCs (Lucan Valerius) to look downward toward the counter. Eye contact only triggered when the player crouched down to the exact horizontal elevation of seated NPCs.
  - *Rigid Elevation Elimination*: Both `TargetSelector.cpp` and `GazeEngine.cpp` previously calculated eye height using actor root translations (feet on the ground) plus a rigid `+160.0f` offset. In interior cells with floor level $z = 0$, this produced $dz = (0 + 160) - (0 + 160) = 0.0$, forcing pitch deflection to zero regardless of whether an actor was seated, leaning, standing, or crouching.
  - *Dynamic Bone Transform Solving*: Implemented `GetActorHeadPosition(RE::Actor* actor)` in `TargetSelector.cpp`, which dynamically resolves the actual `NPC Head [Head]` bone world transform (`head->world.translate`) with fallback candidates (`Head`, `Head1`, `Bip01 Head`). Updated `GazeEngine::WorldTargetToLocalGaze` to compute $dz = \text{targetHead.z} - \text{observerHead.z}$.
  - *Natural Postural Adaptation*: Seated NPCs now realistically tilt their gaze and head upwards toward standing characters, standing characters look down toward seated or counter-leaning characters, and crouching smoothly and continuously adjusts line-of-sight elevation in real time.

### Fixed — 3rd-Person Player Conversational Gaze & Biomechanical Headtracking

- **Player Character 3rd-Person Conversational Engagement**:
  - *Root Cause 1 (Candidate Scanning)*: When `observer == player`, `TargetSelector.cpp` only resolved targets during active dialogue menus, direct crosshair collision targeting, or combat. In 3rd person with a free camera, if the crosshair was not centered directly on an NPC, the player character fell through to an ambient forward idle state with $0.0^\circ$ deflection.
  - *Root Cause 2 (Skeleton Constraint Application)*: In `GazeEngine::ApplyToSkeleton`, an `if (!isPlayer && head)` guard explicitly bypassed head and neck rotations on the player character. Because vanilla humanoid rigs lack eye bones (`eyeL` and `eyeR` are null), the player character never turned their head toward nearby targets.
  - *Conversational Candidate Scanning*: Added a candidate scan in `TargetSelector.cpp` for the player character that locates the nearest conversational partner ($\le 4.5\text{m}$) inside the player's forward visual cone.
  - *3rd-Person Biomechanical Headtracking*: Updated `GazeEngine::ApplyToSkeleton` to allow head and neck tracking on the player character whenever `camera->IsInThirdPerson()` is true, while leaving 1st-person camera and torso spine untouched to prevent camera disturbance and ensure running animations stay aligned.

### Added — Live Console Logger Level Switching & Visual Enhancements

- **Dynamic `stgverbose` Logger Switching**: In `ConsoleCommands.cpp`, `CmdVerbose` now dynamically invokes `spdlog::default_logger()->set_level(...)` to switch the active logging threshold between `debug` and `info` instantly without requiring a game restart.
- **Subtle Laser Beams & Configurator Options**:
  - Calibrated discreet ~2mm hair-thin laser beams (`fGazeRayThicknessCm = 0.20`, `fGazeRayLengthMeters = 2.50`, `fPupilGlowIntensity = 0.35`) originating precisely from anatomical pupil sockets.
  - Exposed `bGazeRaysOnPlayer` toggle in `TrueGazeConfig.html` and `TrueGaze.ini` allowing users to display laser rays on NPCs only or on the player as well.
  - Calibrated the `Developer` preset in `TrueGazeConfig.html` for immediate, high-responsiveness eye movement verification (`fSaccadeSpeedMult = 1.50`, `fHeadTrackingSpeed = 6.50`, `fHeadOnsetDelaySec = 0.04`, `fMaxComfortEyeAngle = 35.0`).

## [1.0.2] - 2026-09-21

### Fixed — Crash to Desktop (CTD) on Save Load & Visual Stability Hardening

- **Crash to Desktop (CTD) on Game Load Resolved**:
  - *Root Cause 1 (FaceGen Modifier KeyFrame Null Pointer)*: In `EfmBlinkController::ApplyGazeMorphs`, `faceGenData->modifierKeyFrame.SetValue()` was called without verifying that `modifierKeyFrame.values != nullptr` or `modifierKeyFrame.count > 11`. During game load and 3rd-person player initialization, the player's FaceGen morph structures are unallocated, producing a null-pointer dereference access violation (0xC0000005). Added robust guard: `if (faceGenData && faceGenData->modifierKeyFrame.values && faceGenData->modifierKeyFrame.count > static_cast<std::uint32_t>(RE::BSFaceGenKeyframeMultiple::Modifier::LookUp))`.
  - *Root Cause 2 (Malformed Handwritten NIF Assets)*: Discovered that procedural scripts had created corrupt binary NIF structures for `GazeBeam.nif` and `GazeRegionPanel.nif`. Calling `RE::BSModelDB::Demand` on these malformed assets caused Skyrim's native resource parser to crash inside `SkyrimSE.exe`. Deleted the corrupt files and hardened `VisualEffectsManager::EnsureBeamGeometry` to default reliably to the verified engine-native `RE::NiPointLight` emitter fallback.
  - *Root Cause 3 (BSModelDB::DBTraits::ArgsType Zero-Initialization)*: Changed `RE::BSModelDB::DBTraits::ArgsType args{};` to `ArgsType args;` to preserve Skyrim's default engine traits (`unk8=true`, `postProcess=true`, `texLoadLevel=3`).
  - *Fault-Tolerant Exception Boundaries*: Wrapped all visual updates in `try / catch` blocks in `src/Engine/GazeEngine.cpp` (`VisualEffectsManager::Get().UpdateActor`) and player ticking in `src/Engine/AnimationHook.cpp`, preventing any potential visual or actor anomaly from disrupting the core kinematics loop or terminating the game.
- **Configurator UI Parity & Preset Normalization**:
  - Exposed `fGazeRayThicknessCm`, `bShowHcepPanel`, `bHcepPanelAllActors`, `fHcepPanelScale`, and `fHcepPanelForwardOffsetCm` in `TrueGazeConfig.html` with rich contextual tooltips.
  - Recalibrated all quick preset yaw strain weights (`vanilla`, `subtle`, `intense`, `social`) so their cervical distribution shares (`fSpine2YawWeight` + `fNeckYawWeight` + `fHeadYawWeight`) sum to exactly 1.00, eliminating engine sanitization warnings.
  - Synchronized `TrueGazeConfig.html` between workspace root and `skyrim/` packaging directory.

## [1.0.1] - 2026-09-21

### Fixed — Skyrim VR ("Mad God VR") Crash on Start & Multi-Targeting Architecture

- **Skyrim VR Startup Crash Resolved**: Diagnosed and resolved fatal game crash on boot when running Skyrim VR (`SkyrimVR.exe` 1.4.15), specifically reported in heavily modded environments such as the "Mad God VR" modlist (500+ mods).
  - *Root Cause 1 (CommonLib Address Library Resolution)*: In previous builds, CMake compiled with `BUILD_SKYRIM_VR=OFF`. Without `SKYRIM_CROSS_VR` and `HAS_SKYRIM_MULTI_TARGETING` defined, CommonLibSSE-NG attempted to load `versionlib-1-4-15-0.bin` via `IDDB::load()`. Because Skyrim VR exclusively utilizes `version-1-4-15-0.csv`, this threw an unhandled `std::system_error` causing immediate termination. Initialized `extern/CommonLibSSE-NG/extern/openvr` submodule and enabled `BUILD_SKYRIM_VR=ON` in `CMakeLists.txt` to compile unified SE/AE/VR multi-targeting.
  - *Root Cause 2 (Vtable Hook Slot Misalignment)*: In Skyrim SE and AE (`SkyrimSE.exe`), `RE::Actor::Update` occupies virtual table index `0xAD`. In Skyrim VR, `AttachWeapon` is inserted at slot `0x82`, shifting `Actor::Update` down by two entries to slot `0xAF`. Hooking slot `0xAD` on VR overwrote `PutActorOnMountQuick`, corrupting virtual dispatch and triggering instant memory access violation crashes upon actor initialization.
  - *Dynamic Hook Dispatch*: Refactored `src/Engine/AnimationHook.cpp` to dynamically evaluate the runtime module using `REL::Module::IsVR() ? 0xAF : 0xAD`.
- **Autonomous Direct SKSE Launch Independence**: Fully verified that TrueGaze runs completely autonomously when launching directly via SKSE (`skse64_loader.exe` or `sksevr_loader.exe`) or mod manager "Run" buttons (Mod Organizer 2 / Vortex). TrueGaze initializes natively on `kDataLoaded`, reads `TrueGaze.ini`, runs bone restorations every frame (ensuring full compatibility with OAR, Nemesis, Pandora, and combat overhauls), and operates without requiring any external batch files, web servers, or active bridge processes during gameplay.
- **Actor Eligibility & Eye Contact Restoration (Zero Eligible Ticks Resolved)**: Fixed defect in `src/Engine/AnimationHook.cpp` and `src/Engine/TargetSelector.cpp` where direct calls to `actor->GetLifeState()` evaluated invalid bitfield offsets on Skyrim AE 1.6.629+ / 1.6.1170 (1.7.104), causing 100% of actors to fail `IsActorEligibleForGaze` (`tick calls: 466, eligible ticks: 0`) and preventing visual emitters (gaze rays and HCEP panels) from attaching. Replaced fragile bitfields with canonical version-independent engine virtual calls (`actor->IsDead()`, index `0x99` SE/AE, `0x9A` VR) and form flags (`IsDisabled()`, `IsDeleted()`). Removed premature worker-thread headtrack clearing in `ActorUpdateHook`, restoring natural headtracking and gaze engagement across all NPCs.
- **Health Verification Update**: Updated `scripts/Test-TrueGazeHealth.ps1` to detect and validate the dynamic slot `0xAF` VR string marker and exception boundary, passing all 15 pre-flight checks with zero warnings or failures.

### Added — TrueGaze Configurator Package Integration & User Guide

- **Web Configurator Suite Bundled in Release**: Included the full standalone visual configurator suite into the production distribution archive (`dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip`):
  - `TrueGazeConfig.html` (Standalone zero-install HTML5 visual configurator with animated pupil kinematics preview, preset cards, and live save inspection).
  - `Launch-TrueGazeConfig.cmd` (Universal batch launcher with automated port scanning, process cleanup, and browser spawning).
  - `tools/TrueGazeConfig/` (Node.js and PowerShell local automation bridge servers for seamless bidirectional file reads and writes).
- **Universal Mod Manager Path Resolution**: Updated `Launch-TrueGazeConfig.cmd` and `TrueGazeBridgeServer.ps1` to auto-detect and resolve relative directories when installed inside Mod Organizer 2 (`<MO2>/mods/TrueGaze/`), Vortex staging folders, or manual `Data/` directories.
- **Comprehensive User Guide**: Authored and packaged `TrueGaze_Configurator_Guide.txt` detailing step-by-step instructions for:
  - Launching directly from SKSE in heavily modded setups.
  - Using Method A (One-Click Automation) and Method B (Direct Browser Configuration).
  - Adding the Configurator as a registered tool in MO2 and Vortex.
  - Mod compatibility, load order notes, and log file locations.

### Added — Option 1 (Laser Eyes) & Option 2 (HCEP Floating Diagram Panel)

- **Option 1: Superman Laser Eyes Refinements**:
  - *Pencil-Thin Ray Thickness*: Scaled geometry down to match the exact diameter of actor pupils (~8mm / 0.008 scale on X/Y axes in `NiMatrix3` rotation bases), replacing large arrows with razor-sharp laser rays.
  - *Dynamic Target Distance Scaling*: Scaled ray length along the Z-axis dynamically based on actual raycast hit distance or `fGazeRayLengthMeters`.
  - *Pupil Origin Socket Anchoring*: Replaced raw head bone centers with anatomical pupil socket derivations (`fPupilForwardOffsetCm`, `fPupilUpOffsetCm`), ensuring beams project outward cleanly from the eyes without intersecting facial geometry.
  - *Ocular Line-of-Sight Tracking*: Oriented laser rays to track the computed saccadic and fixation line-of-sight vectors rather than following static head yaw/pitch.
- **Option 2: HCEP Floating Diagram Panel**:
  - *Floating 3D Display Quad*: Authored `meshes/TrueGaze/GazeRegionPanel.nif`, rendering a 3D planar quad anchored to the actor's head bone and floating stably ~35cm in front of the eyes.
  - *Chroma-Keyed Texture Pipeline*: Processed `hcep-02_enhanced-diagram_keyed-01.jfif` to generate `textures/TrueGaze/GazeRegionPanel.dds` in DXT5 format with an 8-bit alpha channel, removing solid background artifacts for transparent holographic rendering.
  - *Real-Time Gaze Region Emissive Glow*: Dynamic shader highlights on the panel responding in real-time to active HCEP gaze regions (Social Triangle, Mutual Gaze, Intimate, Avoidance, Target/Distraction).
  - *Configurable INI Controls*: Added `[Visuals]` configuration keys: `bShowHcepPanel` (toggle floating display), `bHcepPanelAllActors` (display on all NPCs or player only), `fHcepPanelScale` (overall quad dimensions), and `fHcepPanelForwardOffsetCm` (forward floating distance).

## [1.0.0] - 2026-09-20

### Public Production Release on GitHub ([kirklasalle/SkyrimTrueGaze](https://github.com/kirklasalle/SkyrimTrueGaze))

- **Public Distribution Packaging:** Packaged production archive `dist/TrueGaze-v1.0.0-SkyrimSE-AE-VR.zip` (SHA-256: `41AB39649F3B78F505F6BA4D972460365CC4EFAA0352C61297CB66F60991410F`) and companion debug symbols `dist/TrueGaze-v1.0.0-Symbols.zip` (`TrueGaze.pdb`, SHA-256: `5021AB863EF30539F0A0DE4FC87A9C88C090AEB55B16FF3BC4FF74F28D10EA28`).
- **Diagnostic Visuals Policy:** Shipped `TrueGaze.ini` defaults `bEnableInGameVisuals = false` for pristine, organic eye contact without developer diagnostic laser beams. When toggled (`stgvisuals`), `VisualEffectsManager` probes standalone mesh `meshes\TrueGaze\GazeBeam.nif`, falls back to Dawnguard `fxsoulcairnbeam.nif`, and seamlessly defaults to the verified `NiPointLight` emitter fallback (`TrueGaze_PupilLight`, `TrueGaze_TerminusLight`).
- **Open Animation Replacer (OAR) Dynamic Messaging Hook:** Dynamic SKSE messaging registration (`OarConditions::OnSkseMessage`, `RegisterWithOar`) with zero static compile dependencies. Evaluates `TrueGaze_IsMode`, `TrueGaze_IsMutualGaze`, and `TrueGaze_GetGazeRegion` against live per-actor state cache in real time.
- **Broad Multi-Race & Dialogue Field Acceptance:** Documented Stage 6 test protocol in `docs/TEST_SCENARIO.md` across Humanoid (Nord/Imperial in Whiterun/Riverwood), Elven (Bosmer/Dunmer), and Beast races (Khajiit/Argonian) for third-person dialogue camera, VOR counter-rotation, and Social Triangle cycling.
- **Release Presentation:** Authored release presentation documentation with complete BBCode/Markdown formatting, live configurator screengrabs, hero banner, and release documentation.

### Changed — Skyrim AE Runtime Verification and SOTA Roadmap (2026-09-19)

- Updated the current documentation state after controlled Skyrim AE sessions.
- Confirmed through runtime logs: SKSE load, actor update hooks, eligible ticks, target resolution, skeleton probing, HCEP mode/state consumption, and diagnostic light attachment.
- Documented the remaining evidence boundary: vanilla humanoid eye-node availability varies by rig; perceptual eye/head quality still needs a formal acceptance matrix; visible beam geometry remains unresolved; OAR registration and Skyrim VR require separate validation; packaging and licensing remain release gates.
- Added [`docs/IMPLEMENTATION_PLAN_SOTA_RUNTIME_TO_RELEASE.md`](IMPLEMENTATION_PLAN_SOTA_RUNTIME_TO_RELEASE.md), a staged implementation and publication plan.
- Updated `README.md`, `docs/STATUS.md`, `ROADMAP.md`, `docs/TEST_SCENARIO.md`, and architecture references to distinguish verified runtime behavior from open visual and ecosystem work.
- Added [`docs/TRUEGAZE_SUPPORT_KNOWLEDGE_BASE.md`](TRUEGAZE_SUPPORT_KNOWLEDGE_BASE.md) with web-supported BSA/NifSkope/CommonLib asset-reuse guidance, a `kNotExist` decision tree, legal boundaries, and a repeatable support-report template.

### Fixed — Console Commands "not found": wrong registration mechanism (2026-09-18)

The first in-game run of the console feature produced:

```
Console command table validation FAILED (count is at or beyond the declared
 table length). No commands were registered and no memory was written.
```

**The guard worked; the assumption was wrong.** It refused to write on an unverified
layout instead of corrupting engine memory or failing silently — exactly what it was
built to do. The defect was the mechanism itself.

- **There is no count of console commands.** The previous revision assumed a counter
  sat immediately after the command array and tried to append past it. The SDK's own
  `LocateConsoleCommand` disproves that: it scans the whole array and identifies a real
  command from a **per-entry marker** — an empty `helpString` means the entry is empty,
  and otherwise the help string ends with `1` (live) or `0` (dead).
- **Registration now reclaims, rather than appends.** It fills entries the engine
  already treats as not-a-command, so it needs no count and **cannot displace a working
  vanilla command** (a live entry is never a candidate). Nothing is ever written past
  the end of the array.
- **Both name fields are set to the token you type** (`stgv`, not `TrueGazeRays`). The
  SDK's lookup matches on `functionName`; we cannot be certain which field the engine's
  parser matches, so both hold the short token and the readable text lives in
  `helpString`, carrying the live-command marker.
- **Registration install is now self-diagnosing.** It logs
  `Console table scan: scanned=.. live=.. dead=.. empty=.. reclaimable=..` on every
  launch, so a future failure states which condition broke instead of leaving a
  "not found" to interpret. A partial registration is refused outright — registering
  some commands and not others would be worse than registering none.

### Fixed — Deploy silently wiped the user's configuration (2026-09-18)

**A serious usability bug.** Both `Deploy-TrueGaze.ps1` and `TrueGaze.cmd` copied the
*packaged* `TrueGaze.ini` over the *deployed* one on **every** deploy. The packaged file
is the set of shipped defaults; the deployed file holds the user's live tuning, and
everything the console commands persist. So every build/deploy reset hand-tuned
settings to defaults — which presents as "my settings randomly reverted."

- **The deployed INI is now preserved.** It is copied only when absent (first install).
- Keys present in the newer defaults but absent from the deployed INI are **reported**
  (not silently injected). That is safe — the engine keeps its compiled default for any
  missing key — but you should know a new key exists.
- **`-ForceIni`** added to `Deploy-TrueGaze.ps1` for the case where resetting to
  shipped defaults is genuinely wanted.

### Added — Runtime Console Commands (vanilla `~` console) (2026-09-18)

TrueGaze could only be re-configured by editing `TrueGaze.ini` and restarting. The
console is the right place for **runtime** toggling, because `~` pauses the game and
frees the camera — exactly the moment you want to switch the gaze visuals on and step
back to watch them.

- **`src/Integrations/ConsoleCommands.{hpp,cpp}`** (new) — registers `stg*` commands
  into the engine's own console command table.
- **VANILLA ONLY.** No Papyrus, no ESP/ESL, no MCM, no SkyUI. This is also a hard
  requirement, not just a preference: **Papyrus native functions cannot be called from
  the console**, so the commands must be genuine `SCRIPT_FUNCTION` entries. A pre-flight
  check asserts no Papyrus API usage anywhere in `src/**`.
- **Press `~` and type `stgstatus`** to see the live state, or `stgv` to toggle the gaze
  rays. Commands: `stg` (simulation), `stgvisuals`/`stgv` (visuals / rays), `stgon`/`stgoff`,
  `stgmode` (render mode), `stgradius` (terminus glow), `stgverbose` (logging),
  `stgstatus` (full state).
- Toggles apply **immediately** and are **persisted** to the INI, so a console change
  survives a restart. Each command prints its new state to the console and the file log.
- New `[Console]` INI section + a configurator panel.

> [!IMPORTANT]
> **`bEnableConsoleCommands` is OFF by default, and that is a deliberate engineering
> decision — not an oversight.** It writes into engine memory, so it stays opt-in until
> confirmed inside a running game.
>
> **Registration does not append — it reclaims.** There is **no count** of console
> commands; the SDK's own `LocateConsoleCommand` scans the whole array and identifies a
> live command by a per-entry marker (empty `helpString` = empty entry; otherwise the
> help string ends with `1` for live or `0` for dead). So TrueGaze fills entries the
> engine already treats as not-a-command. That needs no count, and it **cannot displace
> a working vanilla command**, because a live entry is never a candidate.
>
> **An earlier revision of this feature was wrong, and the failure is recorded here.**
> It assumed a count sat after the array, guessed at that offset, and on first launch the
> guard refused to write and logged `Console command table validation FAILED (count is at
> or beyond the declared table length)`. That guard is exactly why nothing was corrupted —
> the plugin did **not** write bad memory and did **not** fail silently; it declined and
> said why. The assumption was the defect; the guard caught it. *Never guess an index;
> never guess an offset.*
>
> Everything still works without this, from `TrueGaze.ini` and `TrueGazeConfig.html`.

### Added — Complete Prerequisite Installer (2026-09-18)

Installing TrueGaze on a fresh machine previously required assembling the
toolchain by hand, and `TrueGaze.cmd prereqs` covered only the two community mod
files — not the build system itself. This adds a single entry point for the
whole machine.

- **`Install-AllPrerequisites.bat`** (new, repository root) — the one-click
  entry point. Batch wrapper only: it finds PowerShell, forwards its arguments,
  and pauses when double-clicked. All real work is delegated to PowerShell,
  because a multi-line `-Command` argument leaks into `cmd.exe` (documented in
  the audit notes).
- **`scripts/Install-AllPrerequisites.ps1`** (new) — installs, in dependency
  order: PowerShell (checked) → **winget** → **Git** → **CMake** → **7-Zip** →
  **Visual Studio Build Tools** with the C++ workload → **VC++ 2015-2022 x64
  Redistributable** → **vcpkg** (bootstrapped, `x64-windows-static-md`) →
  **`CommonLibSSE-NG` submodule** → **SKSE64 + Address Library**.
- **`VCPKG_ROOT` is now persisted.** `CMakePresets.json` resolves its toolchain
  as `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake`, so an unset variable
  is a hard configure failure. This was the single most common blocker on a
  fresh machine and is now set automatically.
- **`-Verify` mode** reports what is present and what is missing without
  changing anything — safe to run at any time.
- **`TrueGaze.cmd prereqs-all`** and a new menu entry `[8]` expose it from the
  project's existing tool.
- Installs needing administrator rights are **reported, not silently skipped**,
  with the exact winget command to run from an elevated terminal.
- Idempotent: re-running installs only what is missing (`winget` "already
  installed" is recognised, not treated as a failure).

> [!IMPORTANT]
> **SKSE64 and the Address Library are now installed** (SKSE `skse64_1_7_104.dll`,
> Address Library `versionlib-1-7-104-0.bin`, both matched to game 1.7.104.0).
> The blocker to in-game testing is therefore **cleared** — nothing now stands
> between the build and a first in-game run. Note the `[Visuals]` verification
> (see below) can now actually be performed.

### Added — In-Game 3D Visual System: "Superman Laser Eyes" (2026-09-18)

TrueGaze computed a full per-actor gaze solution every frame and rendered **nothing**
in-world. The only "debug rays" were text: a throttled `spdlog` line, a console
`Print`, and a HUD message string. `src/Integrations/DebugGazeRenderer.hpp` was an
orphaned header with no `.cpp` and no call site (audit Finding 6). This adds a real,
toggleable in-game 3D visual layer. See
[`docs/Implementation Plan - In-Game 3D Visual System & Gaze Ray Assets.md`](docs/Implementation%20Plan%20-%20In-Game%203D%20Visual%20System%20%26%20Gaze%20Ray%20Assets.md).

- **`src/Visuals/VisualEffectsManager.{hpp,cpp}`** (new) — a **pure consumer** of
  `GazeEngine` state. It never recomputes gaze; it subscribes to the eye residual
  (`eyeYaw`/`eyePitch`), the resolved head/eye bones, and the gaze region. What you
  see is therefore the solver's own answer, not a re-derivation that could drift.
- **`src/Visuals/VisualTuning.hpp`** (new) — immutable per-frame snapshot, mirroring
  `GazeTuning`. A key in `TrueGaze.ini` has exactly one path to the visuals.
- **V1 renders via `NiPointLight`** — a real light at the pupil and, optionally, at
  the gaze terminus. This path needs **no art assets**, so the visual works today.
  Branded beam geometry (NIF) is a later phase; `iRayRenderMode` selects
  `0 Both / 1 Light-only / 2 Geometry-only`.
- **Pupil origin is derived geometrically.** Vanilla humanoid rigs expose **no eye
  bones** (eyes are FaceGen morphs), so the socket is computed from the head bone's
  world basis plus the configurable `fPupilForwardOffsetCm` / `fPupilUpOffsetCm`.
  Custom rigs (XP32/XPMSSE) that do expose `NPC L/R Eye` are used directly.
- **Direction uses the eye residual, not the total deflection.** The head bone's
  world transform already contains the head's share of the turn, so applying the
  total here would double-count it and the beam would overshoot the true gaze.
- **18 keys in a new `[Visuals]` INI section**, all off by default, plus an additively
  added **"In-Game Visuals"** panel in `TrueGazeConfig.html` (14 controls + rich
  tooltips in the page's single tooltip standard). Verified: the INI, the engine and
  the HTML agree on every key.
- **Safety.** Visuals are never serialized; emitters are detached on disable, on
  actor eviction, on `ResetAll`, and on `kSaveGame` alongside `ReleaseBones()`. The
  subsystem cannot affect the simulation, the save game, or actor state.

> [!NOTE]
> The `[Visuals]` subsystem is **developer-facing and off by default**. It is opt-in,
> requires a game restart to take effect (the INI is read at startup), and — like the
> rest of the engine — is **not yet verified in-game** (SKSE64 and the Address Library
> remain uninstalled on the test machine). The `NiPointLight` path is the first thing
> to confirm on the next launch.

### Changed — Vanilla-UI Enforcement: No SkyUI, Papyrus, or MCM (2026-09-18)

TrueGaze is **deliberately vanilla-UI**. The MCM and Papyrus layers were removed by
design on 2026-09-14; this pass makes that stance explicit, enforced, and
self-healing rather than merely documented.

- **`scripts/Deploy-TrueGaze.ps1`** — deploy now **removes stale MCM-era artifacts**
  from the game `Data` folder on every run: `TrueGaze.esp`, `TrueGaze.esl`,
  `TrueGaze_MCM.pex`, `TrueGaze.pex`, `MCM\Config\TrueGaze`, `Interface\MCM\Config\TrueGaze`,
  and `Interface\Translations\TrueGaze_*.txt`. Upgrading over an old install now
  self-heals instead of leaving a stale ESP in the load order or a `.pex` SkyUI
  might still bind.
- **`scripts/Test-TrueGazeHealth.ps1`** — new pre-flight check
  **"No legacy SkyUI/Papyrus/MCM artifacts"**. Reports a `Warn` naming any leftover
  file found, so a confusing in-game state is diagnosed before launch.
- **`LaunchTrueGaze.bat`** — cleanup block expanded to remove the same full set of
  legacy artifacts (previously only the ESP).
- **`TrueGaze.cmd`** — the deploy path now calls a `CLEAN_LEGACY` routine that
  removes the same artifacts, so Kirk's primary self-contained entry point
  self-heals identically. Written with top-level `if exist` tests only (a bare `)`
  inside a parenthesised block would close it early, and these paths contain
  parentheses). Verified by planting fake artifacts and confirming removal.
- **Documentation** — the vanilla-UI stance is now stated explicitly in `README.md`
  (dependency table), `PRD.md` (explicit non-dependencies), `docs/STATUS.md`
  (configuration section), and `ROADMAP.md` (Phase 5). The historical audits
  (`AUDIT_REPORT_2026-09-11.md`, the superseded walkthrough, and the original
  implementation plan) now carry clear banners marking their MCM/Papyrus/ESP
  findings — **C-4**, **C-9**, **C-11**, and the Phase 4 checklist — as **moot**
  rather than merely "resolved by another mechanism".

The only configuration surface is `Data\SKSE\Plugins\TrueGaze.ini`, edited through
the standalone `TrueGazeConfig.html` page.

### Changed — Unified Tooltip Standard in the Configurator (2026-09-18)

`TrueGazeConfig.html` previously used two different tooltip styles: the rich
floating tooltip card on the configuration rows, and plain native browser
`title=""` tooltips on everything above the first section (the action toolbar and
the Quick Presets bar). The rich card is now the **single standard across the
entire page**.

- **New `UI_TOOLTIPS` knowledge base** — each toolbar button and each Quick Preset
  now carries a full entry with a *Function & Mechanism* description, an *Effect*
  summary, *Context*, and a recommendation, matching the depth of the INI
  parameter entries.
- **Toolbar + presets rewired** — every `title=""` attribute was replaced with
  `data-tt-ui="ui:<id>"`, wired through the same `#richTooltip` card by a new
  `wireUiTooltips()` bootstrap pass. No native browser tooltips remain on the page.
- **Bottom action bar** — the *Save Changes to Game* and *Export Copy* buttons in
  the bottom bar now share the toolbar buttons' tooltips.
- **Adaptive card** — interface entries render an `[INTERFACE]` tag (name read from
  the button's own label) and an `Effect:` label, and omit the
  Recommended/Shipped footer, which has no meaning for an action. Configuration
  entries are unchanged: `[SECTION]` tag, `In-Game Visual Impact:`, and the
  Recommended/Shipped footer.
- **Additive only** — page layout, controls, bridge calls, and the simulation canvas
  are untouched. No component was removed or replaced.

### Added — Crosshair-Driven Mutual Gaze (2026-09-14)

- **`src/Engine/PlayerGazeResolver.{hpp,cpp}`** — resolves the player's crosshair into a "who is the player looking at" answer. Reads the game's own `CrosshairPickData` (the same pick the HUD activation prompt uses) and tests whether the crosshair ray falls within the target's **face sweet spot**. The sweet spot is angular, not a fixed radius: the crosshair must be within the head's angular size (`2·atan(0.12 m / d)`) plus a base tolerance, so a close NPC is forgiving and a distant one requires precision — matching natural vision and the crosshair's own on-screen behaviour.
- **`TargetSelector::TargetPriority::CrosshairFocus`** — new highest-priority target. When the crosshair rests on an NPC's face, that NPC looks back at the **player's face** (origin + eye height), producing true eye-to-eye contact instead of proximity-based looking.
- **`mutualGazeHoldSec` is now written.** `GazeEngine::ComputeDeflection` accumulates it while the crosshair holds on the actor's face and resets it the moment it breaks. This is the first production consumer of the field, which previously existed but was never written — the OAR conditions and C API always reported zero.
- **`[Crosshair]` INI section** — `bEnableCrosshairGaze`, `fCrosshairToleranceDeg`, `fCrosshairMaxRangeMeters`, `fCrosshairPointBlankMeters`. Wired through `ConfigManager` (read/sanitise/save), `GazeTuning`, `TargetSelector::s_crosshair`, and `TrueGazeConfig.html`. INI edited in Latin-1 per the byte-preservation rule.

### Added — Governance Enforcement

- **`GOVERNANCE.md`** — the charter enforcement map required by `AGENTIC_SACRED_COVENANT.md` §3.1. States plainly which controls are implemented, which are gaps, and which are not applicable to a Skyrim plugin. Includes the Law 6 biometric gap analysis and the amendment procedure.
- **`scripts/verify_charter.py`** — implements the `LAW10-CHARTER` control. Pins the 10 Laws and 4 Core Tenets by SHA-256 digest and verifies them across every charter document. Detects a Law that has been reworded, dropped, duplicated, renumbered, or left disagreeing between copies.
- **`config/charter_manifest.json`** — canonical digests of the Laws and Core Tenets, plus recorded known divergences.
- **`.github/workflows/charter-integrity.yml`** — CI enforcement of charter integrity, independent of the C++ build.

### Fixed — Charter Integrity

- **Fourth Law capitalisation** — `AGENTIC_PRIME_DIRECTIVE.md` and `AGENTIC_SACRED_COVENANT.md` read "An **I**ntelligence System" where the canonical text reads "An **i**ntelligence System". Corrected.
- **Ninth Law em-dash spacing** — both markdown charters read "verified — recognizing" where the canonical text reads "verified—recognizing". Corrected.

  The 10 Laws are now **byte-identical** across all three charter documents.

### Discovered — Governance Findings

- **🔴 Law 6 (biometric data) — no runtime control.** The HCEP bridge transmits real-world gaze vectors, head pose, blink state, a tracked person identifier, and cognitive/emotional classification over a named pipe created with `nullptr` security attributes (`src/Bridge/NamedPipeServer.cpp:75`). There is no access control, no encryption in transit, no consent capture, and no connection auditing — while `LICENSE` asserts GDPR Article 9, CCPA, and BIPA compliance. Practical risk is low for a single-user local mod, but the governance position is not defensible as written. Remediation plan in `GOVERNANCE.md`.
- **🔴 Law 10 (approval) — no cryptographic control.** `verify_charter.py --update` regenerates the manifest with a plain file write. There is no signature and no separation between recording a change and authorising it. Currently mitigated procedurally (explicit reminder + mandatory diff review), not cryptographically.
- **🟡 Core Tenet divergence.** All four Core Tenets in `AGENTIC_PRIME_DIRECTIVE.md` and `AGENTIC_SACRED_COVENANT.md` are **paraphrases** of the canonical text in `Permanent_Active_Directives.txt`, not reproductions. Recorded in the manifest as disclosed divergences pending Governance Council review. Three options are documented in `GOVERNANCE.md`; the choice is reserved to the Council.
- **🟡 Laws 1–5 and 8 have no runtime control.** Consistent with the covenant's own disclosure. These are governing principles for human conduct on this project, not runtime constraints on the plugin. Recorded so they cannot be mistaken for implemented controls.

### Added — Independent Audit & Honest Status Reporting

- **`docs/AUDIT_REPORT_2026-09-11.md`** — Full independent audit: complete documentation review, complete source review, binary forensics on the shipped `TrueGaze.dll`, distribution-archive inspection, and competitive market research (modding ecosystem landscape, UE5/MetaHuman gaze ecosystem, commercial eye-tracking, academic saccade literature).
- **`docs/STATUS.md`** — Verified capability matrix using a mandatory four-state vocabulary (📐 Designed / 🔨 Implemented / 🧪 Unit-verified / ✅ In-engine verified). Replaces binary "complete/incomplete" reporting, which had overstated progress.
- **Governance policy** — "Reporting and Verification Policy" now prohibits describing a feature as complete before it is ✅ In-engine verified, and prohibits log messages reporting success for operations not performed.

### Changed

- **`README.md`** — Added status banner, badges, and a **Current Project Status** section that disclaims the gap between the designed architecture and the current build. Clarified that the README describes the *target* system.
- **`ROADMAP.md`** — Converted all phase statuses from "Completed (100%)" to a four-state vocabulary with **verified** percentages (Phase 1 ~85%, Phase 2 ~10%, Phase 3 ~70%, Phase 4 ~15%, Phase 5 ~45%, Phase 6 ~40%, Phase 7 ~50%, Phase 8 ~5%). Added the audit-derived remediation roadmap (Phases R0–R5) sequenced by dependency, with milestone projections.

### Fixed — Documentation Accuracy

The following claims in earlier entries were found to be **inaccurate** and have been annotated in place:

- **`EfmBlinkController::ApplyMorphs` — not implemented.** The only substantive logic (the `exprOverrides` writes) is commented out; both code paths are no-ops. The cited `RE::FaceGen::Expression::BlinkLeft` used as an array subscript will not compile once the SDK is present; this code has never been compiled against CommonLibSSE.
- **OAR condition registration — not implemented.** `OarConditions::RegisterWithOar()` contains a `// Future:` comment where registration should be, yet logs a success message and returns `true`. The state cache the evaluators read (`g_actorGazeCache`) is never written to. Net effect: the 7-rule OAR package fires Rule 1 unconditionally and Rules 2–7 never fire.
- **Public modding SDK — export surface only.** All `TrueGazeAPI.cpp` bodies are stubs. `TrueGaze_IsHcepConnected()` returns a hardcoded `false`; `TrueGaze_GetActorGaze()` returns hardcoded zeroes and a hardcoded `0x14` target FormID while returning `true`; `TrueGaze_OverrideActorMode()` is an empty body.

- **Animation hooks — not installed.** `AnimationHook::Install()` constructs no `REL::Relocation`; the hook body that would apply bone rotation is a comment.
- **Configuration — never loaded on the real plugin path.** `ConfigManager::Load()` is called only in the `#else` fallback branch of `Main.cpp`. Every INI setting is inert.

### Discovered — Verified Defects

- **🔴 BLOCKER: the engine is inert.** No bone transform is ever written. `SaccadeGenerator`, `VorCoordinator`, `MicroJitter`, `SocialTriangle`, and `BoneController` have no production call sites — they are referenced only by `tests/KinematicsTests.cpp`.
- **🔴 BLOCKER: the build omits its SDK.** `extern/CommonLibSSE-NG` does not exist. The CMake guards (`if(EXISTS)` / `if(TARGET)`) silently skip both the subdirectory and the link step, so the build succeeds and produces a standalone skeleton. Every `#if __has_include(<RE/Skyrim.h>)` block resolves to a fallback stub.
- **`BoneController::CalculateHierarchyStrain` allocates no residual to the eyes.** Weights `0.10 + 0.25 + 0.65 = 1.00`, so `eyeYaw`/`eyePitch` are declared but never assigned. The eyes should receive `target − head_total`. This is a correctness bug in the project's signature feature.
- **`NamedPipeServer` double buffer is not lock-free.** `_packetBuffers[]` holds plain (non-atomic) 64-byte structs; `_readIndex` orders only the index, not the payload, so the writer can overwrite the slot the reader is mid-`memcpy` on. `_pipeHandle` is additionally written by the worker thread and read by the game thread with no synchronisation. Both are genuine data races.
- **`MicroJitter` is not Brownian.** It resamples an independent uniform target each interval and exponentially approaches it — a mean-reverting process, not a random walk. It is also seeded with a fixed constant (`1337`), so every run produces an identical jitter sequence.
- **`CalculatePeakVelocity` (the Main Sequence equation) is never used.** The saccade trajectory is a generic `smoothstep`. Additionally, `CalculateDuration` sets the correct total duration, but the *velocity profile within it* is a cubic ease rather than a Main Sequence profile — a fidelity gap between the documentation's central scientific claim and the implementation.
- **No biological latency gap is modelled.** Docs specify eyes lead by 20–30 ms and head follows at 120–180 ms; the implementation updates both in the same frame with only a damping constant differentiating them.
- **`PerformanceProfiler::ScopedTimer` uses `.store()` rather than `.fetch_add()`** — only the last measurement survives, which would break the intended per-actor accumulation.
- **`SocialTriangle.hpp` uses `std::max` without including `<algorithm>`** — compiles only by transitive include.
- **`PackageMod.ps1` hardcodes an absolute project path** and runs `cmake --build` with no preceding configure step. `.pdb` files are not shipped.
- **Magic numbers in `TargetSelector.cpp`** — `0.01428f` (Skyrim units-to-metres) and `160.0f` (eye-height offset) appear as bare literals five and six times respectively.
- **`TrueGaze.ini` contains a dead setting** — `sEngineTarget=Auto` is never read by `ConfigManager`. `iLogLevel` is parsed but never applied.
- **`LICENSE` contradicts the "Public Modding SDK" claim** — the license states "No license is granted... copying, distribution... prohibited".
- **`.gitignore` excludes the shipping artifacts** (`*.dll`, `*.zip`, `build/`) that the packaging script depends on.

### Verified Working

Confirmed correct by direct inspection during the audit:

- All 7 DLL exports present: `SKSEPlugin_Load`, `SKSEPlugin_Query`, `SKSEPlugin_Version`, `TrueGaze_GetVersion`, `TrueGaze_IsHcepConnected`, `TrueGaze_GetActorGaze`, `TrueGaze_OverrideActorMode`
- `TelemetryPacket.h` compile-time `static_assert` size guards (64-byte / 32-byte) — exemplary practice
- CRC-32 verify-before-publish in the pipe worker
- Correct DoS guard in the pipe read loop (`PeekNamedPipe` size check before `ReadFile`)
- Asynchronous pipe server with graceful auto-reconnect
- Biomechanical kinematics library — mathematically correct, correctly cited
- 8-suite standalone unit test harness — all passing
- IPC integration test harness
- Correct Skyrim mod package structure (MO2/Vortex layout)
- Well-designed 7-rule OAR condition package

---

## [1.0.0-rc1] - 2026-09-11

### Added

- **Multi-Version SKSE64 Plugin Exports**: Unified DLL exporting `SKSEPlugin_Query` and `SKSEPlugin_Load` (Skyrim SE 1.5.97) and `SKSEPlugin_Version` with Address Library version independence (Skyrim AE 1.6+).
- **NamedPipeServer Implementation & Verification**: Asynchronous Windows Named Pipe worker (`\\.\pipe\TrueGazeBridge`) featuring lock-free double-buffered telemetry exchange (< 10ns read time), CRC-32 validation, and non-blocking `PeekNamedPipe` polling (`src/Bridge/NamedPipeServer.cpp`).
- **End-to-End HCEP Integration Test Harness**: Standalone client test (`tests/HcepBridgeClientMock.cpp`) simulating live HCEP desktop perception streaming and validating 32-byte feedback reception.
- **AnimationHook Implementation**: Post-Havok actor animation evaluation pipeline with eligibility checks (filtering dead, paralyzed, sleeping, or ragdolled actors) and state-based gaze weighting (`src/Engine/AnimationHook.cpp`).
- **TargetSelector Implementation**: Salience prioritization engine resolving dialogue partners, combat adversaries, proximity actors, and ambient focus points (`src/Engine/TargetSelector.cpp`).
- **OarConditions Implementation & Config Rules**: Thread-safe condition cache and sample rules (`skyrim/meshes/actors/character/animations/OpenAnimationReplacer/TrueGaze/config.json`) supporting all 5 HCEP modes (LOGIC, AFFECT, SPIRIT, HEART, THINK).
- **Expressive Facegen Morphs (EFM) Integration**: Implemented `EfmBlinkController::ApplyMorphs` for face morph target weight calculations during saccades.
- **Skyrim VR & Performance Instrumentation**:
  - `VrController.hpp`/`cpp` handling OpenVR HMD 6DOF transforms and foveated gaze projections.
  - `PerformanceProfiler.hpp` microsecond execution timer ensuring frame budget $< 0.15\text{ ms}$.
- **Public Modding SDK**:
  - `include/TrueGazeAPI.h` and `src/Engine/TrueGazeAPI.cpp` exporting public C/C++ API (`TrueGaze_GetActorGaze`, `TrueGaze_GetVersion`, `TrueGaze_IsHcepConnected`, `TrueGaze_OverrideActorMode`).
- **Automated Release Packager**: Created `scripts/PackageMod.ps1` and compiled distribution package `dist/TrueGaze-v1.0.0-rc1-SkyrimSE-AE-VR.zip`.
- **Pure C++20 Kinematics Test Suite**: Standalone automated test harness (`tests/KinematicsTests.cpp`) validating 8 core components.
- **Product Requirements Document (`PRD.md`)**: Comprehensive technical document specifying product vision, personas, functional/non-functional requirements, wire protocols, and acceptance criteria.
- **Detailed Development Roadmap (`ROADMAP.md`)**: Strategic multi-phase roadmap spanning kinematics core through public release and cross-engine expansion.

### Changed

- **Project Directory Refactoring**: Transitioned repository root from `D:\Projects\TrueGaze` to `D:\Projects\SkyrimTrueGaze` across all documentation, architecture guides, headers, build presets, and handover records.
- **Precompiled Header & Compiler Warning Hardening**: Added `[[maybe_unused]]` attributes and MSVC C4189 warning suppression in kinematics coordinators. Wrapped external logging headers with fallback macros for standalone compilation.

---

## [0.2.0] - 2026-09-10

### Added

- Mathematical foundation for biological oculomotor dynamics:
  - `SaccadeGenerator.hpp`: Main Sequence peak velocity ($750^\circ/\text{s}$) and duration calculations.
  - `VorCoordinator.hpp`: Vestibulo-Ocular Reflex eye-head decoupling and counter-rotation.
  - `MicroJitter.hpp`: Brownian random-walk drift eliminating frozen stares.
  - `SocialTriangle.hpp`: Argyle & Cook cyclical scanning between eyes and mouth.
  - `BoneController.hpp`: NetImmerse bone strain distribution (Spine2 10%, Neck 25%, Head 65%).
  - `LodManager.hpp`: 3-tier spatial level-of-detail culling.
  - `EfmBlinkController.hpp`: Micro-blink triggering for saccades $> 20^\circ$.
- 64-byte inbound telemetry wire protocol (`TrueGazeTelemetryPacket`) and 32-byte outbound feedback protocol (`SkyrimFeedbackPacket`).
- Open Animation Replacer (OAR) integration specification and HCEP bridge specification documents.

---

## [0.1.0] - 2026-09-09

### Added

- Initial project architecture and mission blueprint for TrueGaze™ as a first-party HCEP gaming product.
- Modern CMakePresets.json targeting MSVC C++20 for Skyrim SE, AE, and VR.
- Initial CMakeLists.txt and vcpkg.json dependency manifests.
- Master project overview (`README.md`) and technical architecture document (`TRUEGAZE_ARCHITECTURE.md`).
