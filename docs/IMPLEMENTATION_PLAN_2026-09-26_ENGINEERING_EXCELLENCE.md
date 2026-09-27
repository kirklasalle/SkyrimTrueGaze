# Implementation Plan — Engineering Excellence & World-Class Hardening

**Date:** 2026-09-26
**Owner:** Kirk LaSalle
**Companion to:** [`docs/AUDIT_REPORT_2026-09-26.md`](AUDIT_REPORT_2026-09-26.md)
**Supersedes (for engineering work):** the engineering portions of `IMPLEMENTATION_PLAN_SOTA_RUNTIME_TO_RELEASE.md` (S1-S4 remain the product-acceptance authority; this plan covers the infrastructure that plan deferred).
**Status vocabulary:** same four states as ROADMAP (📐 Designed / 🔨 Implemented / 🧪 Unit-verified / ✅ In-engine verified).

---

## Executive Thesis

The product is ahead of the infrastructure. Every remaining crash class in the project's history has been either fixed (SEH guard, logging ownership) or root-caused (NIF parser, vtable slot). What separates TrueGaze from a world-class codebase now is **proof infrastructure**: concurrency evidence, performance measurement, logic-module tests, CI, formatting/linting, and documentation truth. This plan closes exactly that gap in six phases, ordered by risk-reduction per unit of effort.

**Rule for this plan:** no new gameplay features until E1-E3 are ✅. Gameplay work (R14+) resumes after the foundation is proven.

---

## Phase E1 — Concurrency Proof & Shutdown Safety

**Goal:** close the two residual concurrency defects (audit §3.1 C-1, C-2) and make the bridge lifecycle provably safe.

**Status:** 📐 Designed

### E1.1 — Shutdown-safe worker lifecycle (fixes C-1)

- Replace the 250 ms wait + detach in `NamedPipeServer::Stop()` with an ownership-safe design:
  - Signal `_shutdownEvent`, `CancelIoEx` on the atomic handle (already done).
  - **Never detach.** Instead: if join would deadlock (loader lock during `atexit`), leak the server intentionally — allocate the `NamedPipeServer` with `new` and never delete it (process-lifetime object). Document: a leaked 4 KB object at process exit is strictly safer than a detached thread referencing a destroyed object.
  - Move the `atexit` hook to stop the worker *before* loader lock acquisition, and verify with a repeated start/stop stress test (100 cycles).
- Add `Stop()` idempotency test + shutdown-during-connect test to `HcepBridgeClientMock.cpp`.

**Acceptance:** 100 start/stop cycles, no crash, no detached-thread warning; game-exit path verified in-engine once.

### E1.2 — Triple-buffer memory-model proof + stress test (fixes C-2)

- Write the happens-before argument as a comment block in `NamedPipeServer.hpp`: writer = payload store → epoch fetch_add(release) → readyIndex store(release); reader = readyIndex load(acquire) → payload load. Show the 3-slot invariant (writer's next slot ≠ reader's current slot for a single reader).
- Add a stress test to the bridge mock: writer thread publishes at 10 kHz with mutating sequence numbers; reader thread asserts every read packet is internally consistent (sequence monotonic within tolerance, CRC valid) across ≥10M publications.
- Optional: run the mock under TSan via a clang/WSL preset if available; otherwise document why MSVC-only TSan is unavailable and rely on the stress test.

**Acceptance:** stress test passes; the memory-model comment exists and names the exact orderings.

### E1.3 — Pipe hygiene

- Reuse a member event in `DrainOutboundQueue` instead of create/destroy per call (C-3).
- Rate-limit the security-descriptor fallback warning to once per session (B-10).
- Add a threat-model section to `docs/HCEP_BRIDGE_SPEC.md`: local single-user scope, plaintext payload rationale, CRC-as-integrity-not-auth, `trackedPersonId` minimization decision (zero by default unless HCEP Desktop needs it).

**Acceptance:** spec updated; no per-cycle warning spam in a 10-minute disconnected run.

---

## Phase E2 — Performance Measurement & Hot-Path Optimization

**Goal:** replace the unevidenced 150 µs claim with measured numbers, and remove the O(N²) crowd risk.

**Status:** 📐 Designed

### E2.1 — Make the profiler real (fixes Q-4, P2-1)

- Either **use** `PerformanceProfiler` (instrument `TickActorList` + `EndFrame` with `ScopedTimer` using `fetch_add` accumulation — fixing the known `.store()` bug) or **delete it**. Recommendation: use it.
- Fix the budget check: the `EndFrame` warning threshold (1,500 µs) and the documented budget (150 µs) disagree by 10×. Decide the real budget from measurement, then align the constant, the comment, and the docs.
- Publish into `stgstatus`: last/peak frame µs, actor count, culled count.

**Acceptance:** `stgstatus` shows live frame timings; the budget constant matches the docs.

### E2.2 — Cull before resolve (fixes P1-3, half)

- In `TickActor`, the LOD tier-3 cull currently happens *after* `ComputeDeflection` is queued but the target resolution happens inside `ComputeDeflection`. Move the distance check to the top of `TickActor` (it already computes `distanceMeters` — reorder so culled actors never enter `ComputeDeflection`).
- Verify with the existing `_culledTicks` counter that culled actors no longer produce target resolutions.

**Acceptance:** `stgstatus` shows target resolutions ≈ eligible non-culled ticks.

### E2.3 — Cache head positions in the social scan (fixes P1-3, rest)

- `TargetSelector::ResolveTarget`'s social scan calls `GetActorHeadPosition` (4 `GetObjectByName` string lookups) per candidate per observer per frame. Add a per-frame position cache: a small static map (formId → {worldPos, headPos, frameTag}) built once per frame by the first observer that scans, reused by the rest. Invalidate by frame counter.
- Alternatively (simpler): cache the head bone pointer per actor in `ActorGazeRuntime` and pass it into the scan — the engine already caches bones; the selector should reuse them.

**Acceptance:** at 30 actors in a cell, frame cost measured (E2.1 profiler) drops measurably; no behaviour change (same targets selected — verify via target-trace log diff).

### E2.4 — Benchmark artifacts

- Add a benchmark scenario to `docs/TEST_SCENARIO.md` Stage 4: Whiterun market at midday (dense), 1/10/25/50-actor synthetic counts if feasible.
- Record frame µs percentiles into `docs/evidence/` with the DLL hash.

**Acceptance:** one evidence bundle with measured numbers at ≥3 actor densities.

---

## Phase E3 — Logic-Module Test Coverage

**Goal:** the most logic-dense, bug-dense modules get harnesses.

**Status:** 📐 Designed

### E3.1 — Extract & test TargetSelector's pure logic

- Extract the pure decision core (cone test, salience comparison, hysteresis, bias arithmetic) into a `TargetSelection` free-function layer that takes positions/angles as inputs — no SDK types. `ResolveTarget` becomes a thin adapter that gathers inputs and applies the decision.
- Port the existing behavioural knowledge into tests: point-blank in-cone (cart case), 110° hold cone, player salience bias (1.5 m / 3.0 m dialogue), hysteresis 0.8 m, fixation hold 1.5 s, defer decision matrix (slot semantics).
- Register in CTest.

**Acceptance:** ≥15 assertions covering every priority branch and every bias constant; all pass.

### E3.2 — Test ConfigManager::Sanitise

- Pure function; test every clamp boundary, the yaw-weight renormalisation, the interval/tier ordering fixes, and the engineTarget validation.

**Acceptance:** ≥20 assertions; all pass.

### E3.3 — Test the triple-buffer reader path + ValidateTelemetryPacket fuzz-lite

- Property-style test: 10k random-mutation packets (bit flips, NaN injection, range violations) — validator must reject all; valid packets must pass.
- Reader-consistency test folded into E1.2's stress test.

**Acceptance:** fuzz-lite suite passes; zero false-accepts on the mutation corpus.

### E3.4 — Test infrastructure

- Add `enable_testing()` + `add_test` (CTest) for all three executables.
- Add a Debug-forced build property on the test targets so asserts never compile out.
- Replace raw `assert` with a minimal check macro that counts failures and continues (failure report at exit, non-zero exit code) — keeps the suite framework-free but isolates failures.

**Acceptance:** `ctest` runs all suites from a single command; a deliberately-broken assertion fails only its own test.

---

## Phase E4 — Build, CI & Code-Quality Infrastructure

**Goal:** the build proves itself on every change.

**Status:** 📐 Designed

### E4.1 — Formatting & lint config

- Add `.clang-format` (match the existing style: 4-space indent, the `SKSEPluginInfo` trailing-semicolon rule documented in a comment), `.clang-tidy` (start with: bugprone-*, performance-*, modernize-use-*; exclude extern/), `.editorconfig`.
- Run clang-format over `src/`, `include/`, `tests/` once; commit as an isolated commit (no functional changes).
- Add `/WX` (warnings-as-errors) — the tree is /W4-clean today; fix any stragglers first.

**Acceptance:** format check passes; build clean with /WX.

### E4.2 — CI workflow (works around the Actions limitation)

- Primary: `.github/workflows/build-test.yml` — windows-latest, vcpkg manifest mode, standalone preset, ctest. **Known blocker:** private repo + GitHub Free = zero Actions minutes (issue #9). Two paths:
  - If the repo goes public (license decision pending, E6): enable Actions directly.
  - Otherwise: document + script a **local CI gate** (`scripts/Invoke-CiGate.ps1`: configure standalone → build → ctest → charter verify → doc-consistency check) and wire it into the pre-commit/pre-push hook alongside the charter hook.
- The workflow file ships either way so going public instantly activates it.

**Acceptance:** local CI gate script runs green end-to-end; workflow file committed.

### E4.3 — Sanitizer presets

- Add `CMakePresets.json` entries: `standalone-asan` (MSVC ASan for the test executables), `standalone-ubsan` (MSVC /fsanitize=undefined where supported).
- Run the suites under ASan once; record results.

**Acceptance:** presets exist; one clean ASan run documented.

### E4.4 — Static analysis pass

- Run clang-tidy (or MSVC /analyze) over `src/`; fix or suppress-with-reason every finding. Expect: performance-unnecessary-value-param, modernize passes, bugprone-narrowing.

**Acceptance:** zero unexplained findings.

### E4.5 — CMake hygiene

- `enable_testing()` + CTest registration (E3.4).
- Replace `GLOB_RECURSE` with an explicit source list (or keep GLOB but add a configure-time file-count sanity check).
- Sync `vcpkg.json` version-string to the project version (or derive it).
- Configure-time `VCPKG_ROOT` check with a clear message.

**Acceptance:** `cmake --preset windows-release` fails with an actionable message when prerequisites are missing.

---

## Phase E5 — Documentation Truth Restoration

**Goal:** zero contradictions. The project's charter demands it; the audit found 14 violations.

**Status:** 📐 Designed

### E5.1 — Fix the 14 contradictions (audit §8)

Execute in one documentation pass, in this order:

1. **License decision (D-7)** — Kirk decides: GPLv3 (current LICENSE) or proprietary. Then: README badge, GOVERNANCE.md, ROADMAP R0 item, PRD, packaging docs all state the same thing. This is the only item requiring a human decision; everything else is mechanical.
2. ROADMAP: update R2 status line (D-2), Phase 1 known-gaps block (D-4), Phase 3 (D-3), OAR paragraph (D-5), VR claims → "head-directed pilot; eye-tracking adapter future" (D-6), R6 gate annotation (D-1), R5 checkbox (D-14), R8 header (current-milestone mismatch).
3. STATUS.md: fix header/footer dates, fix tier percentages to sum to 100, update `tg*`→`stg*`, refresh Phase 6/7 checklists.
4. PRD: version → 1.0.5, C++23, VR/OAR claims aligned.
5. TEST_SCENARIO: Stage 2 NPC-to-NPC note, Stage 4 SEH note (now closed by G3).
6. Deduplicate: delete `docs/AUDIT_REPORT_2026-09-15.md` OR `docs/TrueGaze™ — Deep World-Class Technical…Audit.md` (byte-identical); annotate README↔TRUEGAZE_ARCHITECTURE overlap (R0 item).
7. Global `tg`→`stg` console-command sweep in stale docs (careful: `tg` is a vanilla command name — match `tgstatus|tgv|tgvisuals|tgon|tgoff|tgmode|tgradius|tgverbose` only).

**Acceptance:** a re-run of the audit's §8 checklist finds zero contradictions.

### E5.2 — Doc-consistency checker script

- `scripts/Test-DocConsistency.ps1`: greps for the known contradiction patterns (version strings across README/PRD/CMake/vcpkg, C++ standard claims, `tg*` command names in docs, "not yet verified" phrases in files newer than their verification evidence, duplicate file detection by hash).
- Wire into the local CI gate (E4.2).

**Acceptance:** the script catches a deliberately-introduced contradiction; passes on the fixed tree.

### E5.3 — Single source of truth

- STATUS.md remains the implemented-truth document; ROADMAP remains the plan document; README remains the marketing/quickstart document. Add a three-line header to each stating exactly this and cross-linking, so future edits know which document owns which claim.

**Acceptance:** each doc's role statement exists.

---

## Phase E6 — Legal & Distribution Decision

**Goal:** one license identity, propagated everywhere; shipping tree contains only intended artifacts.

**Status:** 📐 Designed (blocked on Kirk's decision for E6.1)

### E6.1 — License decision (Kirk)

Options:

- **A. GPLv3 (current LICENSE):** mod-distribution friendly, requires source availability for the DLL. Compatible with the "public modding SDK" language. README badge → GPL.
- **B. Proprietary + separate mod distribution grant:** matches the original posture; requires a distribution-terms document; "public SDK" language must be removed or scoped.
- **C. Dual:** proprietary engine + open SDK headers/examples.

Deliverable: decision recorded in GOVERNANCE.md + ROADMAP R0 closed + README badge + PRD + packaging manifest all consistent.

### E6.2 — Shipping-tree hygiene

- Remove `skyrim/meshes/TrueGaze/GazeBeam.nif` + `GazeRegionPanel.nif` from the shipping tree (retired crash-causing assets; keep in `scratch/` or a `dev-assets/` folder excluded from packaging). Update `Package-Release.ps1` exclusion list.
- Verify the v1.0.5 archive contents against a manifest; regenerate if the NIFs are present.

**Acceptance:** package audit shows no retired assets; manifest lists every file with SHA-256.

### E6.3 — Release manifest

- `Package-Release.ps1` emits `manifest.json`: version, DLL/PDB SHA-256, supported runtimes, INI key count, known limitations, license identifier.

**Acceptance:** manifest exists in the dist archive.

---

## Phase E7 — Robustness Refinements (post-foundation gameplay-adjacent)

**Goal:** the remaining medium-severity code findings, batched.

**Status:** 📐 Designed

### E7.1 — Per-actor SEH isolation (P2-7)

- Move the `__try/__except` boundary from the whole frame body to inside `TickActorList`'s loop (per actor) plus keep a coarse frame-level guard. A fault in one actor abandons that actor's frame only.

### E7.2 — EyeAimConstraint hardening (P2-5, B-3)

- Replace `g_frameOpen` bool with a frame-generation counter; `BeginFrame` bumps it, `Apply` records the generation, withdraw only touches current-generation entries.
- Add an overflow counter + one-time warn when `FindOrCreate` hits the 512 cap.

### E7.3 — Emitter cap visibility (P2-6)

- Log once per session when `kMaxEmitterActors` is hit, naming the actor count and suggesting the LOD/visuals config.

### E7.4 — Solve-gate alignment (P1-5, B-1)

- Align `WorldTargetToLocalGaze`'s singularity gate with `IsInVisualCone`'s (16 units² horizontal, point-blank always solvable). Add a regression test in the extracted TargetSelection layer (E3.1).

### E7.5 — Consolidate eye-anchor math (P1-4)

- One `EyeAnchorFromHeadBone` in a shared header (`Engine/EyeAnchor.hpp`); one eye-height fallback constant (decide 125 vs 160 — measure against a real rig; the 160 in PlayerGazeResolver predates the 125 tuning). Delete the duplicates.

### E7.6 — Feedback packet semantics (P2-2)

- Populate `relationshipRank` from the observer→target relationship (base-form read, cached per profile refresh), `gameFrameNumber` from the engine frame counter, and either compute the true mutual-gaze angle (NPC gaze ray vs player gaze ray) or rename the field in the protocol docs.

### E7.7 — Public API contract (P2-9)

- Document in `include/TrueGazeAPI.h`: game-thread-only requirement, lifetime rules (pointers valid until next tick), `lodTier` placeholder status. Add a debug thread-id assertion.

### E7.8 — Trivial batch (Q-6, Q-9, B-7, B-8, B-4, B-5, B-6, K-7)

- Remove dead `startYaw`; clamp `fGazeRayThicknessCm`; consider a Center/Face region for `ClassifyRegion`; soften the MicroJitter stall decay; cache `RE::UI::GetSingleton()` once per frame; convert `_frame*` members to a parameter struct; sync vcpkg version.

---

## Delivery Order & Milestones

| Order | Phase | Effort estimate | Unblocks |
| --- | --- | --- | --- |
| 1 | E1 Concurrency & shutdown | 1-2 sessions | Crash-class closure; bridge trust |
| 2 | E2 Performance | 1-2 sessions | Crowd scenes; budget truth |
| 3 | E3 Test coverage | 2-3 sessions | Refactor safety for everything after |
| 4 | E4 Build/CI/quality | 1-2 sessions | Every future change is gated |
| 5 | E5 Documentation truth | 1 session | Charter compliance; onboarding |
| 6 | E6 Legal & packaging | 0.5 session + Kirk decision | Public posture |
| 7 | E7 Robustness batch | 1-2 sessions | P2 register cleared |

**Total: ~8-12 focused sessions.** After E1-E4, resume gameplay work (R14: perceptual tuning, VR pilot S7, OAR pin S6) on a foundation that proves itself.

---

## Definition of Done (this plan)

1. `scripts/Invoke-CiGate.ps1` runs green: standalone build, ctest (all suites), charter verify, doc-consistency.
2. Bridge stress test passes 10M publications; shutdown cycle test passes 100×.
3. `stgstatus` reports measured frame timings; budget constant matches docs.
4. Audit §8 checklist re-run finds zero contradictions.
5. One license identity everywhere; retired NIFs out of the shipping tree.
6. P1 and P2 registers in `AUDIT_REPORT_2026-09-26.md` fully closed or explicitly re-scoped with reasons.

---

## Risks & Mitigations

| Risk | Mitigation |
| --- | --- |
| E3 extraction changes target-selection behaviour | Behaviour-preservation tests first (capture current decisions as golden cases via target-trace logs), then refactor, then diff |
| E4 /WX surfaces a wall of warnings | Fix incrementally; if >50 findings, scope /WX to new code via target properties |
| E5 license decision stalls everything | E5.1 items 2-7 are independent of E6.1 — do them first |
| E2 caching changes selection order subtly | Golden-case diff (same as E3 mitigation) |
| CI remains impossible (private repo) | Local CI gate is the deliverable; workflow file ships dormant |
