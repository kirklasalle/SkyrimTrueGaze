# TrueGaze Console Commands Card

Current source: `src/Integrations/ConsoleCommands.cpp`, `kCommands` (30 commands).
Open Skyrim's vanilla console with `~`. Commands take **no arguments** (`stgexpanded` also checks whether an actor reference is selected in the console).
Set `[Console] bEnableConsoleCommands=true` before startup; changing registration
requires a restart. No Papyrus, ESP, SkyUI or MCM is required.

## Command library

| Command | Action |
| --- | --- |
| `stghelp` | List every registered command and its description. |
| `stgstatus` | Print current switches, menu gate, incoming counts/timings and engine/resource counters. |
| `stg` | Toggle biological gaze kinematics, not just diagnostics. |
| `stgvisuals` | Toggle visual master; enabling also arms rays and front panel B. Other channel flags are retained. |
| `stgv` | Toggle visual master + rays + B together: all ON becomes OFF; any OFF becomes all ON. Not rays-only. |
| `stgpanel` | Toggle front receiving panel B; enabling also enables visual master. |
| `stgpanela` | Toggle above-head receiving panel A; enabling also enables visual master. |
| `stghud` | Toggle first-person player incoming HUD; enabling also enables visual master. |
| `stgintext` | Toggle complete non-modal incoming inspector; enabling also enables visual master. |
| `stginnext` | Next text page, then next observation; wraps. Requires inspector ON. |
| `stginprev` | Previous text page, then previous observation; wraps. Requires inspector ON. |
| `stginanatomy` | Toggle approximate anatomical measurements; enabling also enables visual master. |
| `stginpanels` | Toggle physical quad measurements; enabling also enables visual master. |
| `stgon` | Enable master, rays, A/B, HUD, inspector and both measurement channels. Does not change actor filters or turn kinematics ON. |
| `stgoff` | Disable visual master, rays, A/B, HUD and inspector; preserve measurement preferences and kinematics. |
| `stgmode` | Cycle 0 Both -> 1 Light only -> 2 Geometry only -> 0. |
| `stgradius` | Toggle terminus glow, not its radius. |
| `stgverbose` | Switch Info/Debug logging immediately. |
| `stgpreset` | Cycle vanilla -> subtle -> intense -> social -> developer; first call is vanilla each session. Writes INI and reloads. No name argument. |
| `stgreload` | Reload the effective INI, sanitize and refresh tuning. Does not register commands. |
| `stgtrace` | Start JSONL trace if OFF; otherwise report status and flush. Does NOT stop tracing. |
| `stgtraceoff` | Stop trace and flush pending events. |
| `stgtraceflush` | Flush an active trace without stopping it. |
| `stgcal` | Step 11 representative angular deflections; selected actor or all tracked actors. |
| `stgcalsweep` | Sweep 11 angular deflections, 2 seconds each; selected actor or all tracked actors. |
| `stgcaloff` | Stop calibration and restore autonomous gaze. |
| `stgcalaxes` | Print head-axis convention and selected actor's 3D availability; not geometric accuracy certification. |
| `stgexpanded` | Toggle Expanded Play (`[ExpandedPlay] bEnabled`) on/off and print status, unlock Stage 4 (`player.stgexpanded`), OR start/abort a Tavern Staring Contest when an NPC ref is selected while Expanded Play is ON. |
| `stgdemeanor` | Cycle 3rd-person Expanded Play Demeanor (`Courteous`..`Predatory`) and pin for 60s in Hybrid mode. |
| `stgitems` | Drop all TrueGaze Expanded Play relics & book in front of the player (Player-owned, never marked Steal) and teach the `Sight of the First Canticle` right-hand spell (`0` Magicka) when the optional ESL is active. |

## Safe test sequence

1. `stgstatus` - check kinematics, Expanded Play flags, visual master and engine counters.
2. `stgoff` then `stgpanel` - isolate front panel B and associated receiving cards.
3. `stgintext` - inspect named observations; `stginnext` / `stginprev` page.
4. `stgtrace` - optionally select an actor first to filter the new trace.
5. `stgtraceoff`, `stgcaloff`, `stgoff` - stop test instruments for ordinary play.

## Expanded Play test sequence

1. Run **stgexpanded** (no selection) to turn `[ExpandedPlay] bEnabled` ON and print Presence, Signature Title, Book Scholarship, and GazeLedger counts.
2. In 3rd person, run **stgdemeanor** to pin a posture (auto-clears after 60s or on cell/combat/dialogue change in `Hybrid` mode).
3. In console with Expanded Play ON, click an adult NPC (e.g., Orgnar in Riverwood) and run **stgexpanded** to start/abort the 3-round Staring Contest.
4. Run **stgexpanded** (no selection) again at any time to disable Expanded Play cleanly and revert all temporary *Moth's Stillness* stat modifiers.

See [Expanded Play Guide](EXPANDED_PLAY_GUIDE.md) for INI module toggles and mechanics details.

**Persistence:** settings toggles attempt to save the effective INI and refresh
live tuning. Presets intentionally replace their listed keys, not the entire
configuration; unchanged keys retain prior values. The developer preset enables
front panels for the player only; use `bHcepPanelAllActors=true` for a crowd test.
Trace start/stop, paging and calibration are session actions. `stgcal` and
`stgcalsweep` require `[Debug] bEnableCalibrationCommands=true`.

**Receiving truth:** the observer generates gaze; the observed actor owns the
panel. Front B defaults to 10 cm ahead of the resolved eye midpoint (creatures:
cranial anchor). Collision-aware text cards are separate. `o` maps estimated
anatomy onto a diagram; `+` marks a physical quad intersection, not a semantic
anatomical region. Visibility/occlusion remains unverified. Angular calibration
does not validate receiving anatomy or obstacle clearance.

**Menus:** full-screen blocking menus temporarily suppress diagnostics without
changing switches. Nested menus must all close before fresh observations return.
Console and dialogue are excluded. Status lines are snapshots, not live console
widgets; run `stgstatus` again rather than relying on earlier scrollback.

**Configuration edits:** save with the webpage, then use `stgreload` for tuning.
Command registration and bridge/startup settings require restart; trace filters
and limits are captured when a new trace starts. Avoid simultaneous console and
web saves: either can overwrite the other's stale copy.

See [User Guide](USER_GUIDE.md), [Developer Guide](DEVELOPER_GUIDE.md) and
[field acceptance](TEST_SCENARIO.md).
