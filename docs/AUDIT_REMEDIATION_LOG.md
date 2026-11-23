# TrueGaze™ Principal-Engineer Audit Remediation Log (2026-11-23)

**Audit Scope:** Full codebase, build system, companion tooling, packaging scripts, binary asset builders, tests, and documentation at `v2.1.0` (HEAD `3ad9c51` + WP-11 working tree).  
**Status:** All **Phase 1 (Immediate Release Blockers)**, **Phase 2 (Security Hardening)**, and **Phase 3 (Architectural & Doc Reconciliation)** items remediated in source, scripts, and docs.

---

## 1. Master Traceability Matrix

| ID | Severity | Status | Root Cause Summary | Remediated Files & Concrete Fix |
| :--- | :--- | :--- | :--- | :--- |
| **`SEC-01`** | **Critical** | **Remediated** | Proprietary Bethesda `marker_arrow.nif` committed & packaged; 4 extracted `.fuz` files in working tree violated EULA & GPL-3.0. | Removed `skyrim/meshes/marker_arrow.nif` (Skyrim loads `meshes\marker_arrow.nif` natively from user's own `Skyrim - Meshes0.bsa`); stripped BSA audio extractor from `scripts/Build-ExpandedPlayEsl.py` and generated clean-room 44.1 kHz 16-bit mono PCM timing `.wav` files matched to subtitle reading length; cleaned `scripts/Package-Release.ps1`, `clean_install_truegaze.ps1`, `deploy_truegaze.ps1`, `LaunchTrueGaze.bat`. |
| **`SEC-02`** | **Critical** | **Remediated** | DOM XSS via unescaped `${val}` / `${displayVal}` into `row.innerHTML` in `TrueGazeConfig.html` (chainable with bridge token to `/api/launch`). | `TrueGazeConfig.html:2210-2268` (+ synced `skyrim/TrueGazeConfig.html`): assigned text input `.value` and slider `.textContent` via safe DOM properties instead of `innerHTML`; added strict CSP + `nosniff` headers in `scripts/TrueGazeBridgeServer.ps1`. |
| **`SEC-03`** | **Medium** | **Remediated** | `/api/scan` & `/api/shutdown` lacked `-RequireToken`; `POST /api/ini` unbounded `ReadToEnd()` OOM & non-atomic write; `GET /api/log` full-file RAM read + HTTP 500 on `?lines=-1`. | `scripts/TrueGazeBridgeServer.ps1` (+ synced `skyrim/tools/TrueGazeConfig/TrueGazeBridgeServer.ps1`): enforced `-RequireToken` on `/api/scan` & `/api/shutdown`, capped `POST /api/ini` at 256 KB with atomic temp-file replace, clamped `lines` to `[1, 2000]` with bounded 512 KB EOF seek + ring queue. |
| **`SEC-04`** | **Medium** | **Remediated (Local)** | `Invoke-CiGate.ps1` Stage 4 only checked JSON syntax instead of `verify_charter.py`; `NamedPipeServer.cpp:507` logged every invalid packet un-rate-limited. *(Note: GitHub Actions billing hold remains an account-level action for the repo owner.)* | `scripts/Invoke-CiGate.ps1`: executes `python scripts/verify_charter.py --verbose`; `src/Bridge/NamedPipeServer.cpp`: 5-second rate limiter with suppressed count on semantic rejection warnings. |
| **`SEC-05`** | **Low** | **Remediated** | `Invoke-Expression $cmd` under admin elevation in `scripts/Install-AllPrerequisites.ps1:126`. | Replaced with direct call-operator argument array splatting (`& winget @wingetArgs`). |
| **`CRIT-01`** | **Critical** | **Remediated & Live-Verified (SE/AE)** | `ConsoleCommands.cpp` overwrote 30 live `RE::SCRIPT_FUNCTION` slots using a false `helpString` empty/trailing-`'0'` heuristic without checking `executeFunction`. | `src/Integrations/ConsoleCommands.cpp`: replaced slot clobbering with a non-destructive `RE::Console::Accept` (`VTABLE_Console[0]` slot `0x01`, SE/AE/VR identical) Scaleform `CallbackProcessor` proxy intercepting only the 30 `stg*` commands (0 vanilla commands touched). Live-verified `stgtrace` dispatch in SE/AE `1.7.104.0`. |
| **`CRIT-02`** | **High** | **Remediated** | `PlayerHook::Hook` ran `EyeAimConstraint::WithdrawActor` *before* checking `s_sehBreakerTripped` and *outside* SEH, crashing on Frame N+1 after a breaker trip. | `src/Engine/AnimationHook.cpp:332-347`: checks `s_sehBreakerTripped` and calls `HandleSehBreakerTripped()` *before* `WithdrawActor`. |
| **`CRIT-03`** | **High** | **Remediated & Live-Verified (SE/AE)** | `TraceLogger::~TraceLogger()` joined `_workerThread` during `DLL_PROCESS_DETACH` under Windows Loader Lock; unbounded `_buffer`; open stream on cap; hardcoded `"100 MB"` msg; invalid JSON `nan`/`"null"`. | `src/Engine/TraceLogger.hpp/.cpp` + `src/Main.cpp`: added `TraceLogger::Abandon()` + `std::atexit` hook, bounded `_buffer`, closed `_fileStream` on cap auto-stop, dynamic MB cap message, `SafeFloat()`, and RFC-8259 JSON `null`. Live-verified clean exit while tracing active (`16,179` events, `bad_json=0`). |
| **`CRIT-04`** | **High** | **Remediated (Code + Baseline `.ess` Verified) / Active-Buff Gate Open** | `ExpandedPlayModifiers` used `kPermanent` AVs and stripped them in SKSE `kSaveGame`, which fires *after* `.ess` write. | `src/Engine/AnimationHook.cpp` (`PlayerSaveGameHook` on `VTABLE_PlayerCharacter[0]` slot `0x0E` = `TESForm::SaveGame`, SE/AE/VR) strips strictly *before* `_original()` and restores synchronously right after; `src/ExpandedPlay/ExpandedPlayModifiers.cpp` switched to `kTemporary`. Hook fired across 5 live saves and `scripts/inspect_ess_actor_values.py` verified 0 leaked AV blocks across 4 baseline saves (`modifiers=0`); `modifiers > 0` Stage-4 stress test remains. See §2 for ReSaver `.ess` verification steps. |
| **`CRIT-05`** | **High** | **Remediated** | `NamedPipeServer` seqlock lacked acquire fence & had formal C++ data race; `Stop(Join)` vs `WorkerLoop` startup leaked `hPipe` blocking restarts; unclamped `reconnectIntervalSec`. | `src/Bridge/NamedPipeServer.hpp/.cpp`: trivially destructible Win32 `SRWLOCK` around 3-slot publish/read, fixed `WAIT_OBJECT_0` exit to always run `DisconnectNamedPipe`/`CloseHandle`, clamped `reconnectIntervalSec` to `[0.05, 60.0]`. |
| **`CRIT-06`** | **High** | **Remediated & Live-Verified (SE/AE)** | `_gameTimeSec` & calibration sweep advanced per actor; `WrapPi` looped infinitely on `Inf`/huge floats; `>=0.5s` hitch evicted all actors; player warmth profile dead-coded; `cgaRoll` drawn before RNG seed; FPS-dependent magnetic settle. | `src/Engine/GazeEngine.hpp/.cpp`, `src/Engine/TargetSelector.cpp`, `src/Engine/AnimationHook.cpp`: moved time/calibration ticks to `EndFrame`, clamped hitch delta, O(1) `std::remainder` `WrapPi`, per-frame warmth blend copy, RNG seeded prior to roll, exponential frame-rate-independent magnetic lock. Live-verified per-frame real-time `ts` progression (`133.148s`–`405.121s` over ~6.2 min unpaused play). |
| **`CRIT-07`** | **High** | **Remediated (Code) / VR Field Gate Open** | `AwakeningRitual.cpp` passed `a_vm = nullptr` to Papyrus native objective bindings and called raw `RELOCATION_ID`s without checking `address() != 0` or VR compatibility. | `src/ExpandedPlay/AwakeningRitual.cpp`: exception-guarded address resolution (`address() != 0`), passes real `VirtualMachine::GetSingleton()`, and implements full Skyrim VR `1.4.15` fallback via `DispatchMethodCall(questHandle, "Quest", ...)`. |
| **`CRIT-08`** | **Medium** | **Remediated** | Event sinks mutated non-atomic `ConfigManager::expandedPlayEnabled` off the main thread; static `NiPointer` caches risked `DecRefCount` after Skyrim heap teardown. | `src/ExpandedPlay/AwakeningRitual.cpp` (marshaled via `SKSE::GetTaskInterface()->AddTask` + immortal heap `g_openBookTemplate`), `src/Visuals/IncomingGazeManager.cpp` (immortal `g_receivers`), `src/Visuals/VisualEffectsManager.hpp` (immortal singleton). |
| **`CRIT-09`** | **Medium** | **Remediated** | `PersuasionTransactions.hpp` swapped Riverwood NPC `ActorBase` FormIDs and used Delphine's RefID (`0x013485`). | Verified directly against `Skyrim.esm` `NPC_` records and fixed `src/ExpandedPlay/PersuasionTransactions.hpp` + `tests/ExpandedPlayTests.cpp` (`0x013478` Delphine, `0x013479` Orgnar, `0x01347A` LucanValerius, `0x01347C` Gerdur). |
| **`CRIT-10`** | **Medium** | **Remediated** | Unchecked `AddBaseComponent()` pointers in `OarCustomConditions.cpp`; `OarConditions` cache never evicted stale actors. | Null-checked all 7 custom conditions in `src/Integrations/OarCustomConditions.cpp`; added `OarConditions::EvictActor()` called from `GazeEngine::EndFrame()`. |
| **`CRIT-11`** | **Medium** | **Remediated** | Per-actor feedback packet ring flood; premature voice cooldown burn; VR `VrController` ignored live HMD and used stale `+160.0` height; `MicroJitter` lacked `theta > 0` guard. | Single per-frame player-centered feedback in `GazeEngine.cpp`; reordered checks before `TryApprove` in `NpcVoiceResponder.cpp`; live 6DOF `VR_NODE_DATA::HmdNode` + `125.0` fallback in `VrController.cpp`; `theta >= 1e-4f` guard in `MicroJitter.hpp`. |
| **`ARCH-01`** | **High** | **Remediated** | `Package-Release.ps1` omitted `.seq` & `Sound/Voice/`; `TrueGazeBridgeServer.ps1` `/api/deploy` only copied DLL+INI. | Added `SEQ` + clean-room `Sound/Voice` to `Package-Release.ps1`, `LaunchTrueGaze.bat`, and full payload deploy to `TrueGazeBridgeServer.ps1` `/api/deploy`. |
| **`ARCH-02`** | **Medium** | **Remediated** | No-op `Actor`/`Character` vtable hooks ran on Havok worker threads every frame; dead global `g_pipeServer` in `Main.cpp`. | Removed no-op `ActorHook`/`CharacterHook` installs in `src/Engine/AnimationHook.cpp`; removed dead `g_pipeServer` in `src/Main.cpp`. |
| **`ARCH-03`** | **Medium** | **Remediated** | Hardcoded personal `G:\` / `D:\vcpkg` paths across scripts. | Replaced with Steam registry / dynamic drive / `VCPKG_ROOT` discovery across `Build-ExpandedPlayEsl.py`, `Deploy-TrueGaze.ps1`, `TrueGazeBridgeServer.ps1`, `clean_install_truegaze.ps1`, `deploy_truegaze.ps1`, `verify_truegaze_install.ps1`, `LaunchTrueGaze.bat`, `TrueGaze.cmd`. |
| **`DOC-01`–`06`** | **Medium** | **Remediated** | Version drift (`1.1.0` / `2.0.0` / `2.1.0`), key count drift (`109`/`128` vs `131`), `HCEP_BRIDGE_SPEC` / `WP11` inaccuracies, `PublishState` hardcoded LOD thresholds, `TrueGazeBridgeServer.ps1` copy encoding drift. | Reconciled `vcpkg.json`, `README.md`, `CHANGELOG.md`, `PRD.md`, `ROADMAP.md`, `docs/STATUS.md`, `docs/HCEP_BRIDGE_SPEC.md`, `docs/WP11_FACTION_SIGHT_VOICE_PERSUASION_DESIGN.md`; wired configured LOD tiers in `GazeEngine.cpp:PublishState`; synced & gated `TrueGazeBridgeServer.ps1` copies in `Invoke-CiGate.ps1`. |

---

## 2. ReSaver / FallrimTools `.ess` Clean-Save Verification Protocol (`CRIT-04`)

To empirically certify the new `PlayerCharacter::SaveGame` (`slot 0x0E`) pre-save strip hook and `kTemporary` modifier design before tagging a public release:

1. **Baseline Save (`Save_A_Baseline.ess`):**
   - Load a clean character with `TrueGaze.dll` active and `[ExpandedPlay] bEnabled=false`.
   - Record player's exact `Speechcraft`, `Illusion`, `MagickaRateMult`, `DamageResist`, and `ResistMagic` via console (`player.getavinfo speechcraft` etc.) and save as `Save_A_Baseline`.
2. **Active-Buff Save (`Save_B_WithBuffs.ess`):**
   - Enable Expanded Play (`stgexpanded` or `player.stgexpanded`), read a book / gain *Moth's Stillness* rank and trigger a transient Persuasion Speechcraft bonus (`+5` to `+15`) or Combat Sight bonus.
   - Verify via `player.getavinfo speechcraft` that the temporary modifier is visibly non-zero while playing.
   - Immediately trigger a manual save (`Save_B_WithBuffs`) *while the buffs are actively applied*.
   - Confirm via `TrueGaze.log` that `PlayerCharacter::SaveGame` pre-save hook fired cleanly and that buffs remain active immediately after saving (no flicker).
3. **Forensic Inspection (Automated CLI Script or FallrimTools ReSaver):**
   - **Automated CLI Inspector (zero extra downloads):**
     `python scripts/inspect_ess_actor_values.py "<path>\Save_A_Baseline.ess" "<path>\Save_B_WithBuffs.ess"`
     Verifies `PlayerCharacter` `ACHR` (`0x000014`) `changeFlags` (specifically `CHANGE_ACTOR_PERM_ACTOR_VALUE_MODIFIERS`), byte size, SHA-256, and diffs all `ExpandedPlayModifiers` ActorValue modifier blocks (`Speechcraft`, `Illusion`, `MagickaRateMult`, `StaminaRateMult`, `DamageResist`, `ResistMagic`, combat/stealth modifiers).
   - **Optional GUI Verification (FallrimTools ReSaver):**
     Open `Save_B_WithBuffs.ess` → `ChangeForms → ACHR → 00000014 (Player)` and inspect `ActorValue` modifiers (`kPermanent` and `kTemporary`).
   - **Pass Criterion:** Zero residual delta from `ExpandedPlayModifiers` compared to `Save_A_Baseline.ess`.
4. **Abrupt-Uninstall Round-Trip Test:**
   - Exit Skyrim, temporarily move `TrueGaze.dll` and `TrueGaze_ExpandedPlay.esp` out of `Data\`, launch Skyrim, and load `Save_B_WithBuffs.ess`.
   - Run `player.getavinfo speechcraft`, `illusion`, `magickaratemult`, `resistmagic`, `damageresist`.
   - **Pass Criterion:** Every stat matches `Save_A_Baseline.ess` to `0.00` delta with zero lingering buffs or warnings.

---

## 3. Skyrim VR (`1.4.15`) First-Party HCEP Certification Checklist (`CRIT-07`, `CRIT-11`)

1. Launch Skyrim VR `1.4.15` with `SKSEVR` + `VR Address Library` + `TrueGaze.dll` (`v2.1.0`) + `TrueGaze_ExpandedPlay.esp`.
2. Confirm in `TrueGaze.log`:
   - `Pre-save safety hook installed on PlayerCharacter::SaveGame (slot 0x0E).`
   - `Registered 30 console command(s) via non-destructive Console::Accept Scaleform hook (0 vanilla commands modified).`
   - Zero unresolved `IDDB` / `Relocation` errors during `kDataLoaded` or Pilgrimage quest advancement.
3. Walk physically (room-scale + head pitch/roll/yaw) in front of an NPC and verify via `stgv` / `TrueGaze_GazeTrace.jsonl` that `VrController::GetHmdPose()` tracks true 6DOF HMD eye origin and forward vector.
4. Trigger Pilgrimage Stage 10 at the Sleeping Giant Inn and confirm `AwakeningRitual.cpp`'s VR `DispatchMethodCall(..., "Quest", ...)` path adds the quest & objectives cleanly into the VR journal.

---

## 4. Live Field Verification Log (2026-11-23 — Skyrim SE/AE 1.7.104.0)

- **12-Minute Live Session (`18:04:58`–`18:16:47`):** Executed cleanly across Whiterun (Bannered Mare), Riverwood, Sleeping Giant Inn, and Tamriel exterior with `469,440` ticks, `27,783` target resolutions (`none 0`), `0` errors, `0` SEH faults, `0` frame-budget warnings, and `0` anchor failures.
- **Post-Run Verification (`Deploy-TrueGaze.ps1 -PostRun`):** `12 Passed`, `2 Warnings` (expected `GeometricHeadSocket` fallback on vanilla NPCs/creatures lacking dedicated eye bones), `0 Failed`.
- **Console Dispatch, Exit Safety & JSONL Integrity (`CRIT-01`, `CRIT-03`, `CRIT-06`):** `Console::Accept` registered 30 commands (`0 vanilla commands modified`) and dispatched `stgtrace` live mid-session; Skyrim exited cleanly while tracing was still `ACTIVE` (`16,179` events, `4,427.0 KB`, `bad_json=0`, `ts=[133.148s, 405.121s]`).
- **Forensic `.ess` Baseline (`CRIT-04`):** `PlayerCharacter::SaveGame` (`slot 0x0E`) fired across 5 live saves; `scripts/inspect_ess_actor_values.py` verified identical `Player ACHR changeFlags=0xB0000C22` and `0` leaked ActorValue modifier blocks across `Save10`, `Quicksave0`, `Autosave3`, and `Save11` (`modifiers=0` baseline).
- **Diagnostic Polish:** Updated `AwakeningRitual::GrantInnNoteToPlayerOnly(const char* source)` (`AwakeningRitual.hpp/.cpp` + `ExpandedPlayManager.cpp`) to log exact `questStartSource` (`OrgnarDialogue`, `SleepingGiantBar`, `SkyrimCourier`, `SanctuaryDiscovery`, `ElderScrollQuest`) rather than hardcoding Orgnar's 3rd rumor.
