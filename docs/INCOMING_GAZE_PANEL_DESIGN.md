# Target-Owned Incoming Gaze Diagnostics

## Contract

The observer generates the ray; the observed actor owns the incoming result.
An actor's panel never highlights that actor's own outgoing gaze.

Two measurements are deliberately distinct:

- **Anatomy:** a ray intersects bounded, posed anatomical approximation volumes.
  The display maps the receiving region onto its diagram. Moving a display does
  not change the anatomical result.
- **Panel A / Panel B:** a ray physically intersects the rendered quad above the
  target's head / in front of its face. A display miss is not a gaze error.

These are diagnostics, not measurements of human eye tracking or emotional
state. Human input is the camera ray in both camera modes. NPC/creature input is
the posed eye ray used by the beam renderer. Outgoing cognitive region IDs,
OAR conditions and the public C API retain their existing meanings.

## Source audit

The previous implementation tested an actor's outgoing ray against its OWN
panel (`VisualEffectsManager::UpdateActor`), tinted the whole panel using that
actor's cognitive region, and moved the ray terminus onto that panel.
`GetActorHitInfo` fed this self-panel result to calibration and trace consumers.
Those tests certified self-projection math, not who was looking at whom.

First-person player ticking was skipped; display eligibility was coupled to
ray eligibility. The renderer used an actor-update counter as a frame counter.
Creature anchors and eye-height fallback were predominantly humanoid.
Existing panels are safe reusable quad assets, but the repository had no native
persistent HUD widget. These are directly coupled findings, not a new claim of
runtime safety certification.

## Frames, identity and quality

Gather sources and receivers on the game thread; resolve after actors are posed.
Use the engine frame ID, not actor iteration order. Clear observations each
frame. UI and trace consumers receive value snapshots, never live actor pointers.
Receiver eligibility is independent of whether that actor emits a ray.

An anatomical region is TARGET-relative. Left/right means the target's own
anatomical side, established by its anchors, not the viewer's screen side.
Unknown anatomy is coarse head/body or unsupported, never a guessed human mouth.
Approximation quality and visibility evidence accompany every result.

A collision/visibility approximation is not exact occlusion proof. In particular,
actor-to-actor LOS does not certify the eye-to-surface ray. Unverified visibility
must be labelled as such. Camera picks must correspond to the camera source:
VR controller-first crosshair targets are not silently substituted.

## Displays and controls

Diagnostics remain opt-in through the existing visual master.

- A: camera-facing above-head panel with ownership leader.
- B: actor-relative front panel; retains the existing panel visibility setting.
- Anatomical and physical reticles are distinguishable and independently enabled.
- Text reports observer/receiver IDs, source, measurement, coordinates and quality.
- Optional non-modal first-person HUD lists incoming observations on the player.
- Console and configurator switches must preserve existing INI tuning.

Observers retain stable identifiers even when markers overlap; labels/list rows
remain countable. Capacity is 64 diagnostic actors plus one camera observation.
Any omission is counted explicitly. Panel placement and HUD visibility never
steer gaze or alter mutual-gaze/social behavior.

Front B now defaults to 10 cm ahead of the resolved eye midpoint (creatures:
cranial anchor), rather than 70 cm. Size remains 27.5 world units; render and
intersection use the same head-relative quad. This reduces physical-panel
parallax, not uncertainty in anatomical estimates. Text cards keep their
collision-aware placement. Check clipping for helmets, head rotation and
nonstandard scales before accepting this clearance. The diagram is not a
calibrated facial surface; physical hits have no anatomical region assignment.
See [configuration audit](CONFIGURATION_AUDIT.md) and
[console command card](CONSOLE_COMMANDS_CARD.md).

### Configuration and console

All switches are in `[Visuals]`; `bEnableInGameVisuals` gates diagnostics.
Existing INI files are preserved during deployment; missing new keys use defaults.

| INI key | Default | Console toggle | Meaning |
| --- | --- | --- | --- |
| `bShowHcepPanel` | true (existing shipped default) | `stgpanel` | B: front receiving panel |
| `bIncomingPanelAbove` | false | `stgpanela` | A: above-head receiving panel |
| `bIncomingPlayerHud` | false | `stghud` | First-person incoming player HUD |
| `bIncomingText` | false | `stgintext` | Paged inspection and detailed telemetry |
| `bIncomingAnatomy` | true | `stginanatomy` | Approximate anatomical measurements |
| `bIncomingPanelHits` | true | `stginpanels` | Physical enabled-quad measurements |

`bHcepPanelAllActors=false` limits world panels to the player; it does not
exclude NPCs/creatures from receiving anatomy. `stgon` enables all diagnostic
channels and measurements; `stgoff` hides all channels. `stgstatus` reports them.
`stgv` retains its existing combined ray/front-panel behavior.

The native movie is authored by `scripts/Build-IncomingUi.ps1`, built by CMake
and deployed at `Interface/TrueGazeIncoming.swf`. There is no SkyUI, Papyrus,
ESP or input-capture dependency. The first Whiterun tavern run showed missing-glyph
boxes; the follow-up screenshot confirms readable target labels after explicit
font selection. The subsequent human-label screenshot shows a reported incoming
eye-area match, but overlapping sentences fail the layout gate. Anatomical
accuracy and unobstructed visibility are not established by that screenshot.

Dynamic text now explicitly selects the installed game's `$EverywhereFont`
through an AS2 `TextFormat` and `setNewTextFormat`, with `embedFonts=true`.
The original empty movie contains no font glyphs and must not rely on an
unmapped default font. Game font libraries remain external; no fonts are copied
into the mod. The 1280x720 stage uses 20-pixel headings with the game's
`$EverywhereMediumFont`, 17-pixel body text and 14-pixel footnotes.

GFx `textWidth`/`textHeight` measure wrapped rows at their actual field width.
Each receiver has one 320-pixel-wide card, even with both world panels enabled:
name, distinct-observer summary, compact colored observer/region rows and one
uncertainty footnote. Whole cards are laid out inside a 12-pixel horizontal inset,
between y=156 and y=608. They avoid one another, projected approximate head boxes,
the compass/top-status band, central crosshair/interaction area and lower HUD.
Previous placements follow their anchors while valid; deterministic active-first
placement searches alternatives when a card grows or encounters an obstacle.
These are protected templates, not runtime measurements of every other mod's HUD.

Subtle dark backings support contrast. Leader lines connect displaced cards to
their receiving actor, not an observer. Point markers remain centered at the
projected hit/mapped point and never move with a card. Oversized/unplaceable cards
are explicitly counted rather than clipped or partially populated.
Detailed IDs, coordinates and raw measurement types remain behind
`stgintext`/`bIncomingText`, in the non-modal inspection panel.
Display names are copied on the game thread into immutable UI snapshots; the UI
callback never resolves actors. Unnamed/unavailable actors have explicit ID
labels, and duplicate display names are disambiguated with FormIDs.
Inspection shows one target/observer entry at a time (and zero-match targets),
including every resolved measurement for that pair. `stginnext`/`stginprev`
page through native wrapped text first, then advance/wrap the entry list. Long
names and long entries remain accessible through native text scrolling.
The panel is live and displays its frame token; identity stays selected while
that pair remains present. It also includes offscreen receivers. No gameplay
input is captured. Console navigation waits for the next valid UI snapshot.
Overflow instructions and diagnostic/error counters occupy the reserved top band,
below the compass, rather than competing with the lower HUD.
The full incoming trace schema and measured gaze are unchanged.

### Development-only visibility and menus

Cards are development diagnostics, not an ordinary gameplay HUD. Their existing
console channels (`stgpanel`, `stgpanela`, `stghud`, `stgintext`, `stgon`/`stgoff`)
remain the enable/disable controls. Menu suppression never changes these flags,
the user's INI or the biological gaze solver.

Native menu events suppress all diagnostic cards, inspection, leaders, reticles,
rays, receiving panels and lights for the radial/tween, inventory, magic, map,
skills, journal, container, barter, crafting, book, loading and main menus.
Nested menu state is tracked atomically with a generation. Closing one nested
menu does not restore visuals while another is open; closing the last menu still
requires a snapshot captured by a fresh gameplay frame.

UI suppression touches only copied display data. Existing 3D emitters/panels are
removed through the normal cleanup path dispatched onto the SKSE game-thread task
queue; frame updates cannot recreate them while a blocking menu remains open.
Menu callbacks never dereference scene nodes. Diagnostic collection is suspended
while these menus are open, so hidden physical panels cannot report measured hits.
Console and dialogue are deliberately not blocking menus. Console inspection
commands remain usable; returning from a menu restores only channels still
enabled by the user. Native cleanup/transition timing remains a field gate.

### Diagram mapping and identity

`o` denotes **mapped anatomy**, not a physical panel collision; `+` denotes a
physical quad intersection. Stable colors and readable names identify observers;
duplicate names and detailed telemetry include FormIDs. Coincident reticles retain
separate observer rows in the receiving card/inspection list; markers stay at
their measured/mapped points. Row height comes from native font metrics plus
spacing, not a fixed single-line assumption. The human summary counts **unique observers per target**.
Detailed telemetry separately counts **measurements**: one observer can produce
anatomy plus both panel measurements without becoming three observers.

Zero matches read "No incoming matches detected" (expanded in inspection), not "Nobody is looking".
Camera observations say "Your view"; NPC/creature observations name their gaze.
Anatomical descriptions say "head area", "left eye area", etc. and are explicitly
estimated. Physical intersections describe the front or above-head panel, not
body contact. "Estimated; visibility not checked" remains visible beside the
summary. None of these descriptions establishes attention, intent or emotion.

Normalized display offsets `(u,v)` from panel center are: head `(0,.25)`,
body `(0,-.30)`, target-left/right eye `(+/-.15,.28)`, mouth `(0,.05)`,
forehead `(0,.40)`, chin `(0,-.10)`, target-left/right hand `(+/-.40,-.32)`.
These are an explicitly separate, provisional display map, not the outgoing
angular classifier. Texture/handedness alignment requires in-game inspection.

Receiving limits retain the lowest eligible FormIDs, independently of traversal
order; they are deterministic, **not** a claim of nearest-actor prioritization.
The camera slot is reserved separately from 64 posed-eye slots. Overflow counters
report omitted source/receiver registrations. Screen projection may hide offscreen
markers; card overflow is separate from collection overflow. Inspection and traces
retain all resolved observations, including those whose cards cannot fit.

### Timing evidence

`stgstatus` reports placed/overflow card counts, visible field count and timing
windows of up to 256 active frames: sample count, upper median, nearest-rank p95
and window peak. UI samples use distinct valid incoming frame tokens, excluding
hidden/paused snapshots. Existing session-wide peaks remain separate. No samples
is reported explicitly, not certified as zero cost. Cached text/metrics and
unchanged display properties avoid redundant native calls.

Finalization includes receiver placement, resolution, snapshot/trace work; it
does not include the earlier eye-ray collection/pose pass. Native UI timing includes
movie advance, card layout and GFx updates, not GPU cost or total game frame time.
These measurements enable field profiling; no median/p95 budget is certified yet.

## Creature acceptance matrix

Detailed profiles require anchor and handedness evidence from actual rigs.
Priority families: dog/wolf, horse, dragon, chicken, fox, rabbit, spider, deer,
bear and troll. Until individually verified, these are **pending**, not certified
by a race name or generic head lookup. Coarse posed head/body approximations
remain available where anchors exist; otherwise report unsupported anatomy.

Read-only metadata inspection of the installed `Skyrim - Meshes0.bsa` found:

| Family / archive directory under `meshes/actors` | Cranial anchor | Body anchor | Detail evidence |
| --- | --- | --- | --- |
| Dog / `canine/character assets dog` | `Canine_Head` | `Canine_Spine2` | `Canine_LEye/REye` names exist; radii/axes pending |
| Wolf / `canine/character assets wolf` | `Canine_Head` | `Canine_Spine2` | Same named eye anchors; posed geometry pending |
| Wolf alternate / `wolf/character assets` | `Bip01 Head` | `Bip01 Spine2` | Distinct rig; eye geometry pending |
| Horse / `horse/character assets` | `HorseScull` | `HorseSpine3` | Eyelids are not certified globe centres |
| Dragon / `dragon/character assets` | `NPC Head` | `NPC Spine2` | Jaw/hand names exist; detailed geometry pending |
| Chicken / `ambient/chicken/character assets` | `HEAD` | `Spine` | Coarse only |
| Rabbit / `ambient/hare/character assets` | `RabbitHead` | `RabbitSpine2` | Coarse only |
| Spider / `frostbitespider/character assets` | `NPC Head [Head]` | None verified | Jaw names do not establish eyes/body |
| Deer / `deer/character assets` | `ElkScull` | `ElkSpine3` | Male supplementary file needs live resolved-rig inspection |
| Bear / `bear/character assets` | `NPC Head` | `NPC Spine2` | Eyebrows are not eye-centre evidence |
| Troll / `troll/character assets` | `NPC Head [Head]` | `NPC Spine2 [Spn2]` | Three eye-node names exist; detailed geometry pending |
| Fox | No independently identified skeleton in inspected archives | Pending | Do not certify by presumed canine reuse |

Only coarse head/body receiving volumes use these inspected names. Their
dimensions remain generic approximations. Adding these receiver anchors does
**not** certify the biological solver's creature basis or eye-ray support.
Humanoid offsets are not applied to non-humanoid receiver cranial anchors.
Private asset probes are not distributed in the mod.

## Verification

Paired A-to-B and reciprocal fixtures, overlapping observers, reversed update
order, scaled/rotated targets, panel-transform parity, self-hit rejection, stale
cleanup, missing anatomy, invalid vectors, occlusion quality, capacity/overflow,
camera without a player solve, and versioned trace fields are required.

Preserve legacy self-projection tests as outgoing-math tests only. Do not compare
incoming anatomical region IDs to outgoing cognitive IDs as an accuracy score.

In-game acceptance requires independent A/B/HUD/text toggles, first/third/free
camera, crowd scenes, seated dialogue, combat, obstacles, save/load and shutdown.
SE/AE/VR and each creature family are separate gates. Record screenshots, traces,
CPU median/p95/peak and resource counts; successful compilation is not field
acceptance. Preserve the existing 150 us engine budget target and report optional
visual/UI cost separately.

`stgstatus` exposes incoming finalization and native UI update last/peak
microseconds, plus panel/label counts. This instrumentation does not establish
median/p95 compliance and does not isolate source collection from the existing
gaze solver. Capture an actual crowd benchmark before claiming budget compliance.

## Status

Design approved; receiving implementation compiles and standalone checks pass.
Native UI/input/projection proof and detailed rig/platform acceptance remain open.
Implementation and runtime acceptance are tracked in
[ROADMAP.md](../ROADMAP.md) and [STATUS.md](STATUS.md). Historical outgoing-panel
verification does not certify this incoming-gaze contract.
