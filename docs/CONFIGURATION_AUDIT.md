# Configuration and Console Synchronization Audit

## Scope and evidence (November 12, 2026)

Reviewed runtime configuration read/save/sanitize paths, visual tuning, shipped
INI, root/packaged webpage schema, offline fallback, serializer, web/native
presets, command registration/handlers, user/developer guides, receiving design,
status and roadmap. Historical audit/release records remain historical, not
retrospectively rewritten. Skyrim Cinema was not modified.

Kirk's four tavern screenshots show progression: readable separated cards,
named incoming observations, explicit placement overflow and an inspector at
entry 5/16 containing both anatomy and front-panel measurements. They do not prove
correct left/right anatomy, obstacle visibility, all paging paths or frame budgets.
The physical-panel `Unsupported` label means no anatomical region is assigned
to that quad crossing; it is not evidence that the anatomy measurement failed.

The apparently contradictory SUPPRESSED console output is earlier scrollback:
the log records startup auto-status during loading at 16:16:42. The previous
`stgstatus` handler did not print the incoming section. Both status paths now
share the incoming reporter, so a new `stgstatus` provides a fresh snapshot.
Native cleanup logs exist (5,233 us queued / 417 us cleanup, and 32,781 / 139 us).
These demonstrate task execution, not instantaneous hiding or exhaustive
nested-menu acceptance. Screenshot/log engine cost of 655 us with a 100,504 us
session peak exceeds the declared 150 us budget; performance is NOT certified.

## Corrections

- Front B's default clearance changes from 70 to 10 cm ahead of the resolved
  eye midpoint (creatures: cranial anchor). Panel size remains 27.5 world units.
  The existing shared render/intersection transform is retained. Anatomy, solver,
  above-head A and collision-aware text placement are unchanged.
- Added missing arrow-width, bridge retention, trace and calibration controls:
  shipped INI and webpage now cover 109 keys. Compiled fallback numeric/boolean
  defaults match shipped defaults rather than the older no-INI baseline.
  Existing explicit tuning is not migrated to new biological values.
- Offline mode generates its template from the schema instead of an obsolete
  hand-written configuration. Tooltip default values use that schema too.
- Web sliders match runtime clamp bounds; booleans encoded as `1`/`0` now display
  correctly. Corrected the old RGB description: retained shipped 13174890 is
  `#C9086A`, not brand gold `#C9A86A` (13215850). No explicit user color is changed.
- Saving older files now includes newly introduced keys/sections and retains
  comments and unknown keys; repeated saves do not duplicate inserted keys.
- Console presets now match all five existing webpage preset key/value sets.
  This intentionally changes console behavior for previously omitted keys,
  including player-only panels in the developer preset. Presets remain partial
  overlays; they do not reset unlisted settings.
- Preset writes no longer attempt an empty-path write and now report failures
  rather than claiming success; a loaded INI is required.
- Removed obsolete console claims that geometry assets are never installed.
  Corrected help for `stgv` (grouped switches) and `stgtrace` (start/status, not stop).
- Published the full 27-command library in both guides and a printable
  [command card](CONSOLE_COMMANDS_CARD.md). Added executable regression checks to
  local/CI parity gates; Node.js 22+ is required, with no npm packages.
- Replaced the competing single-eye animation loops with one paired-eye renderer.
  Anatomical right/left labels follow portrait convention. Each eye turns toward
  the shared target at a fixed virtual depth, producing inward convergence for
  central targets and less convergence at greater depth. Head/blink state and
  actual tuning keys are shared; this is not live game telemetry or clinically
  calibrated eye tracking. Tests reconstruct both rays to the same 3D target.
- Reproduced held-pointer drift with a failing regression. Fixed incomplete head
  counter-rotation and amplified per-frame jitter: pointer fixation is noise-free,
  head motion is fully compensated, and autonomous jitter is shared and smoothed
  at its configured degree scale. Both eyes retain shared-target convergence.

## Configuration ownership and acceptance

Shipped defaults, compiled missing-key fallbacks and the webpage schema must
agree. A user's live INI can intentionally differ; equality with shipped
defaults is not a deployment objective. Console toggles and web edits both persist
configuration, so avoid simultaneous stale writers.

For the controlled install, preserve live INI bytes except the specifically
requested `fHcepPanelForwardOffsetCm` value. Back up the full old file before
changing this key; verify a one-value-only difference and install only while
Skyrim is closed. No automatic launch.

`stgreload` updates tuning, not every startup-only subsystem: restart for command
registration/bridge changes; start a new trace for filters and limits.
Validation commands: Release plugin build, six-suite standalone CI gate plus
configurator regression, and whitespace check. See [current status](STATUS.md)
for the candidate actually installed and [field protocol](TEST_SCENARIO.md).
These automated gates pass. Browser checks verified 109 controls, persistence
of a new key in an old INI, numeric-boolean display and inward convergence.
A 45-frame held-pointer capture after settling measured zero X/Y motion for
both pupils. The game candidate is backed up and installed without launch;
live/staged preflight each passes 16 checks, zero failures, existing OAR warning.
All assets and the exact one-value-only live INI delta are hash/byte verified.

Next field gates: near-face clipping across poses/helmets/scales; oblique
parallax comparison; exact marker/render agreement; unchanged anatomy results;
crowd and camera transitions; nested-menu OFF/ON preservation; trace review and
measured steady-state CPU costs. Moving a diagram closer does not calibrate its
semantic regions to real facial surfaces, and does not implement occlusion.
