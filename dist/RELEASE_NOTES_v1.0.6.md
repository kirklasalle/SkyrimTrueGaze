# TrueGaze™ v1.0.6 — Engineering Excellence (R14)

**Release date:** September 27, 2026
**Commit:** `eebdd52` · **Tag:** `v1.0.6`
**License:** GPL-3.0 (see note below)

---

## 📦 Downloads

| Asset | SHA-256 |
| :--- | :--- |
| `TrueGaze-v1.0.6-SkyrimSE-AE-VR.zip` | `520853E9096355F9E243CA16035F163B0B270A53CEC63A97B1830DE0ECCD4871` |
| `TrueGaze-v1.0.6-Symbols.zip` | `6214449F8A8412A76CDAF41CA1479B3524BDFE4E81D458300DED036689BA9BA8` |

`TrueGaze.dll` (791,040 bytes) SHA-256:
`67A599745C70057E77A12B77EB3CD8F4F418FD5D61DD8FA6A07746E616022D91`

---

## 🎯 About this release

v1.0.6 executes the **Engineering Excellence plan** (roadmap Phase R14) — the
direct implementation of the findings of the full independent codebase audit
dated 2026-09-26. Every change in this release was verified before shipping:
standalone test suite **3/3 green** (KinematicsTests, ValidationTests,
HcepBridgeClientMock), and the full SKSE plugin builds and links against
CommonLibSSE-NG v7.5.4.

This release also **removes the retired crash-causing `GazeBeam.nif`** from the
distribution tree (audit finding E6.2). The beam path now uses only the proven
vanilla Dawnguard `fxsoulcairnbeam.nif` and the crash-free `NiPointLight`
emitter fallback.

---

## ✨ Added

- **License resolution (E6.1)** — LICENSE confirmed **GPLv3** (forced by the
  CommonLibSSE-NG GPLv3 dependency), with a project notice: copyright Kirk
  LaSalle 2026, a SCOPE section excluding HCEP theory as a proprietary trade
  secret, and an honest biometric-data notice. README badge corrected to
  GPL-3.0. Full analysis in `docs/LICENSE_RESOLUTION.md`.
- **`tests/ValidationTests.cpp` (E3.3)** — telemetry-validation fuzz-lite:
  a 10,000-mutation deterministic corpus (byte flips with CRC recompute, NaN,
  Inf, enum range, reserved bytes, magic/version) asserting
  `ValidateTelemetryPacket` rejects malformed input. SDK-free; runs in
  standalone mode.
- **Test infrastructure (E3.4)** — `enable_testing()` + CTest registration for
  KinematicsTests, ValidationTests, and HcepBridgeClientMock in both standalone
  and plugin builds; `truegaze_force_asserts()` keeps asserts live in
  Release-built tests (`/MDd`, `/Od`, `/RTC1`, `_DEBUG`).
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

## 🔧 Changed

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
- **Per-actor SEH isolation (E7.1)** — each `TickActor` call wrapped in its
  own `__try` frame; a defective actor skips exactly one actor instead of
  aborting the whole list for the frame.
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
  actionable message; `vcpkg.json` version-string synced.

## 🐛 Fixed

- `/O2` vs `/RTC1` incompatibility (D8016) in Release-built test targets —
  force-asserts now also applies `/Od`.
- `spdlog::debug` missing from the standalone PCH logging fallback.
- Unused `writerFn` lambda and dead `publishes`/`kPublishCount` in the bridge
  mock stress section; the sequence-regression check now actually sets and
  reports the `torn` flag.
- **Removed retired crash-causing `GazeBeam.nif` from the shipping tree (E6.2).**

## 📋 Known open (tracked, not fixed in this release)

- E3.1 TargetSelector pure-logic extraction (heavy refactor, deferred).
- E3.2 Sanitise unit tests (Sanitise is SDK-bound via logger; needs extraction).
- E2.4 benchmark artifacts to replace the conservative ×10 budget threshold.
- E5 documentation truth pass (PRD 1.0.3/C++20 staleness, STATUS tier sums).

---

## 📥 Installation

1. **Requirements:** Skyrim SE (1.5.97), AE (1.6.640+ / 1.6.1170+), or Skyrim VR
   (1.4.15) · **SKSE64** · **Address Library** (both matched to your game
   version — Nexus-only downloads).
2. Extract the zip into your `Data/` folder (or install via MO2/Vortex — the
   layout is mod-manager ready).
3. Launch via `skse64_loader.exe` (or your mod manager's SKSE launch).

The bundled **TrueGaze Web Configurator** (`TrueGazeConfig.html` +
`Launch-TrueGazeConfig.cmd`) provides a zero-install visual configuration UI.
See `TrueGaze_Configurator_Guide.txt` for the full walkthrough.

**Diagnostic visuals are off by default** — the in-game effect of TrueGaze is
the NPCs' actual head rotation and FaceGen pupil morphs. Enable the optional
gaze rays / HCEP panel via the configurator or the `stg*` console commands.

## ⚖️ License

TrueGaze is licensed under the **GNU GPL v3** — forced by its CommonLibSSE-NG
dependency. The LICENSE file carries a SCOPE notice: the HCEP theory itself is
excluded as a proprietary trade secret of Kirk LaSalle. See
`docs/LICENSE_RESOLUTION.md` for the full analysis.

---

**Full changelog:** [`CHANGELOG.md`](https://github.com/kirklasalle/SkyrimTrueGaze/blob/main/CHANGELOG.md)
**Verified capability matrix:** [`docs/STATUS.md`](https://github.com/kirklasalle/SkyrimTrueGaze/blob/main/docs/STATUS.md)
