# Trace Log Schema — TrueGaze Gaze Diagnostics

**Project:** TrueGaze™ — Biological NPC Gaze & Biomechanical Kinematics Engine  
**Author:** Antigravity (Engineering), Kirk LaSalle (Director & Approver)  
**Date:** 2026-10-03  
**Status:** ✅ Approved — Ready for Development  
**Companion:** [Technical Design](Technical%20Design%20%E2%80%94%20Gaze%20Arrow%20&%20HCEP%20Panel%20Calibration%20and%20Hit%20Detection.md) · [Region Map Specification](Region%20Map%20Specification%20%E2%80%94%20HCEP-02%20Gaze%20Regions.md)

---

## 1. Purpose

The trace log records every gaze event at per-frame granularity in a machine-
readable JSONL format. It serves three audiences:

1. **Developer (Kirk):** real-time debugging of gaze behaviour — why did the NPC
   look at the wrong spot? When did a saccade fire? Was CGA invoked?
2. **Automated tests:** post-hoc analysis of calibration sweeps, dwell-time
   distributions, and agreement rates.
3. **TrueGaze data pipeline:** future ingestion into the HCEP Desktop platform
   for aggregate gaze analytics across sessions.

---

## 2. File Location & Lifecycle

| Property | Value |
|:---------|:------|
| Filename | `TrueGaze_GazeTrace.jsonl` |
| Directory | `Data/SKSE/Plugins/` (same as `TrueGaze.log`) |
| Format | JSON Lines (one JSON object per line, `\n` terminated) |
| Encoding | UTF-8, no BOM |
| Rotation | New file per game session (overwrite on `stgtrace on`) |
| Max size | **100 MB** hard cap — tracing auto-stops and logs a warning |

---

## 3. Event Types

### 3.1 `GAZE_TICK`

Emitted **once per actor per engine tick** while tracing is active. This is the
high-frequency event — most analysis starts here.

```json
{
  "t": "GAZE_TICK",
  "ts": 1234567.890,
  "actor": "0x000A2C94",
  "target": "0x00000014",
  "yaw": 3.42,
  "pitch": -1.07,
  "eyeYaw": 1.22,
  "eyePitch": -0.53,
  "region": 1,
  "hitRegion": 1,
  "mode": "LOGIC",
  "lod": 0,
  "mutual": true,
  "mutualSec": 1.43,
  "headPct": 0.62,
  "dt": 0.0167
}
```

| Field | Type | Description |
|:------|:-----|:------------|
| `t` | string | Event type |
| `ts` | float | Game time in seconds (from `RE::BSTimer::delta` accumulation) |
| `actor` | string | FormID of the gazing actor (hex) |
| `target` | string | FormID of the gaze target (hex, `"null"` if ambient) |
| `yaw` | float | Total body-relative yaw deflection (degrees, + = gazer's right) |
| `pitch` | float | Total body-relative pitch deflection (degrees, + = up) |
| `eyeYaw` | float | Eye residual yaw (head-local, degrees) |
| `eyePitch` | float | Eye residual pitch (head-local, degrees) |
| `region` | int | Classified gaze region (0–12, from `ClassifyRegion`) |
| `hitRegion` | int | Hit-detected region (0–12, or 255 for no hit) |
| `mode` | string | Active HCEP cognitive mode (`LOGIC`, `AFFECT`, `SPIRIT`, `HEART`, `THINK`) |
| `lod` | int | LOD tier (0 = Tier1, 1 = Tier2) |
| `mutual` | bool | Is mutual gaze active? |
| `mutualSec` | float | Duration of current mutual gaze (seconds) |
| `headPct` | float | Head contribution to total deflection (0–1) |
| `dt` | float | Frame delta time (seconds) |

### 3.2 `SACCADE_ONSET`

Emitted when a new saccade begins (velocity exceeds the saccade threshold).

```json
{
  "t": "SACCADE_ONSET",
  "ts": 1234568.100,
  "actor": "0x000A2C94",
  "fromRegion": 0,
  "toVertex": 2,
  "angularDistance": 6.12,
  "expectedDuration": 0.032
}
```

| Field | Type | Description |
|:------|:-----|:------------|
| `fromRegion` | int | Region ID at saccade start |
| `toVertex` | int | Target `SocialTriangle::Vertex` (mapped to `GazeRegion`) |
| `angularDistance` | float | Total angle traversed (degrees) |
| `expectedDuration` | float | Predicted saccade duration (seconds, from main sequence) |

### 3.3 `SACCADE_COMPLETE`

Emitted when the saccade settles (velocity drops below threshold).

```json
{
  "t": "SACCADE_COMPLETE",
  "ts": 1234568.135,
  "actor": "0x000A2C94",
  "landedRegion": 2,
  "actualDuration": 0.035,
  "overshootDeg": 0.3
}
```

### 3.4 `REGION_CHANGE`

Emitted when the classified region changes (debounced by the settling window).

```json
{
  "t": "REGION_CHANGE",
  "ts": 1234568.140,
  "actor": "0x000A2C94",
  "from": 0,
  "to": 2,
  "dwellSec": 0.87,
  "fromLabel": "LeftEye",
  "toLabel": "Mouth"
}
```

| Field | Type | Description |
|:------|:-----|:------------|
| `from` | int | Previous region ID |
| `to` | int | New region ID |
| `dwellSec` | float | Time spent in the previous region (seconds) |
| `fromLabel` | string | Human-readable label of previous region |
| `toLabel` | string | Human-readable label of new region |

### 3.5 `CGA_ENTER` / `CGA_EXIT`

Emitted when Controlled Gaze Aversion begins or ends.

```json
{
  "t": "CGA_ENTER",
  "ts": 1234570.200,
  "actor": "0x000A2C94",
  "trigger": "MUTUAL_THRESHOLD",
  "quadrant": 9,
  "quadrantLabel": "ULPeripheral"
}
```

```json
{
  "t": "CGA_EXIT",
  "ts": 1234572.800,
  "actor": "0x000A2C94",
  "returnRegion": 0,
  "aversionDurationSec": 2.60,
  "trigger": "DIALOGUE_SYNC"
}
```

### 3.6 `BLINK`

Emitted at each blink onset.

```json
{
  "t": "BLINK",
  "ts": 1234569.500,
  "actor": "0x000A2C94",
  "durationMs": 180,
  "intervalSec": 3.2
}
```

### 3.7 `HIT_MISMATCH`

Emitted when `hitRegion != classifiedRegion` outside the settling window.
This is a diagnostic event — it should be rare once calibration is correct.

```json
{
  "t": "HIT_MISMATCH",
  "ts": 1234571.000,
  "actor": "0x000A2C94",
  "classified": 3,
  "hit": 0,
  "yaw": 0.5,
  "pitch": 4.2,
  "hitX": 0.43,
  "hitZ": 3.60,
  "reason": "BOUNDARY"
}
```

| Field | Type | Description |
|:------|:-----|:------------|
| `reason` | string | `BOUNDARY` (near a region edge), `DEFERRED` (vanilla anim), `TRANSIT` (saccade in progress) |

### 3.8 `CALIBRATION`

Emitted by `stgcal` commands.

```json
{
  "t": "CALIBRATION",
  "ts": 1234580.000,
  "command": "sweep",
  "region": 9,
  "inputYaw": -15.0,
  "inputPitch": 18.0,
  "classified": 9,
  "hit": 9,
  "pass": true
}
```

---

## 4. Console Commands

### 4.1 `stgtrace on [actors]`

Starts tracing. Optional `actors` argument filters by FormID (comma-separated).
Default: trace all actors with visuals enabled.

```
stgtrace on                    → trace all visual actors
stgtrace on 0x000A2C94         → trace only Orgnar
stgtrace on 0x00000014,0x000A2C94 → trace player and Orgnar
```

### 4.2 `stgtrace off`

Stops tracing, flushes the buffer, and reports the file size and event count.

```
> stgtrace off
TrueGaze: Trace stopped. 14,328 events written (2.1 MB) to
  Data/SKSE/Plugins/TrueGaze_GazeTrace.jsonl
```

### 4.3 `stgtrace flush`

Forces an immediate buffer flush without stopping the trace. Useful for
inspecting the file mid-session.

### 4.4 `stgtrace status`

Reports current tracing state:

```
> stgtrace status
TrueGaze Trace: ACTIVE
  Duration:    45.3 sec
  Events:      3,412
  File size:   512 KB / 100 MB limit
  Actors:      ALL (3 active)
  Buffer:      87 / 256 events pending flush
```

---

## 5. Performance Budget

| Metric | Budget | Rationale |
|:-------|:-------|:----------|
| Per-tick overhead (tracing on) | **≤ 50 μs** per actor | Serialize to pre-allocated buffer, not disk |
| Buffer flush frequency | Every **256 events** or **1 second**, whichever comes first | Amortise I/O |
| I/O thread | Dedicated flush thread | No main-thread file I/O |
| Memory budget | **4 MB** ring buffer | 256 events × ~2 KB × 8 actor slots |
| Event size | **0.5–2 KB** per line | Compact JSON, no pretty-printing |

### 5.1 Event Rate Estimate

- 1 actor at 60 FPS: ~60 `GAZE_TICK` events/second = ~60 KB/s.
- 5 actors at 60 FPS: ~300 events/second = ~300 KB/s.
- At 300 KB/s, the 100 MB cap is reached in ~5.5 minutes.

For longer sessions, the user should filter to fewer actors (`stgtrace on <id>`)
or accept that the trace will auto-stop.

---

## 6. INI Keys

| Key | Section | Type | Default | Description |
|:----|:--------|:-----|:--------|:------------|
| `bEnableTraceLogging` | `[Debug]` | bool | `false` | Master switch (still requires `stgtrace on`) |
| `iTraceBufferSize` | `[Debug]` | int | `256` | Events per flush batch |
| `iTraceMaxFileSizeMB` | `[Debug]` | int | `100` | Hard cap before auto-stop |
| `bTraceGazeTick` | `[Debug]` | bool | `true` | Include high-frequency GAZE_TICK events |
| `bTraceSaccades` | `[Debug]` | bool | `true` | Include SACCADE_ONSET / SACCADE_COMPLETE |
| `bTraceRegionChanges` | `[Debug]` | bool | `true` | Include REGION_CHANGE events |
| `bTraceCGA` | `[Debug]` | bool | `true` | Include CGA_ENTER / CGA_EXIT |
| `bTraceBlinks` | `[Debug]` | bool | `false` | Include BLINK events (high volume) |
| `bTraceHitMismatches` | `[Debug]` | bool | `true` | Include HIT_MISMATCH events |

> [!TIP]
> For most calibration work, disable `bTraceGazeTick` and `bTraceBlinks` to
> keep the file small. The saccade, region-change, and mismatch events are
> usually sufficient.

---

## 7. Worked Example — Conversation with CGA

The following is a simulated trace extract showing a player talking to an NPC.

```jsonl
{"t":"GAZE_TICK","ts":100.000,"actor":"0x000A2C94","target":"0x00000014","yaw":-1.6,"pitch":0.4,"eyeYaw":-0.3,"eyePitch":0.1,"region":0,"hitRegion":0,"mode":"LOGIC","lod":0,"mutual":true,"mutualSec":0.85,"headPct":0.81,"dt":0.017}
{"t":"REGION_CHANGE","ts":100.870,"actor":"0x000A2C94","from":0,"to":2,"dwellSec":0.87,"fromLabel":"LeftEye","toLabel":"Mouth"}
{"t":"SACCADE_ONSET","ts":100.870,"actor":"0x000A2C94","fromRegion":0,"toVertex":2,"angularDistance":5.1,"expectedDuration":0.028}
{"t":"SACCADE_COMPLETE","ts":100.900,"actor":"0x000A2C94","landedRegion":2,"actualDuration":0.030,"overshootDeg":0.2}
{"t":"GAZE_TICK","ts":100.917,"actor":"0x000A2C94","target":"0x00000014","yaw":0.1,"pitch":-4.3,"eyeYaw":0.05,"eyePitch":-0.9,"region":2,"hitRegion":2,"mode":"AFFECT","lod":0,"mutual":true,"mutualSec":1.77,"headPct":0.75,"dt":0.017}
{"t":"BLINK","ts":101.200,"actor":"0x000A2C94","durationMs":160,"intervalSec":2.8}
{"t":"CGA_ENTER","ts":102.650,"actor":"0x000A2C94","trigger":"MUTUAL_THRESHOLD","quadrant":9,"quadrantLabel":"ULPeripheral"}
{"t":"SACCADE_ONSET","ts":102.650,"actor":"0x000A2C94","fromRegion":0,"toVertex":5,"angularDistance":22.4,"expectedDuration":0.045}
{"t":"SACCADE_COMPLETE","ts":102.698,"actor":"0x000A2C94","landedRegion":9,"actualDuration":0.048,"overshootDeg":0.8}
{"t":"REGION_CHANGE","ts":102.700,"actor":"0x000A2C94","from":0,"to":9,"dwellSec":1.78,"fromLabel":"LeftEye","toLabel":"ULPeripheral"}
{"t":"GAZE_TICK","ts":102.717,"actor":"0x000A2C94","target":"0x00000014","yaw":-14.2,"pitch":17.1,"eyeYaw":-6.1,"eyePitch":5.3,"region":9,"hitRegion":9,"mode":"THINK","lod":0,"mutual":false,"mutualSec":0.0,"headPct":0.57,"dt":0.017}
{"t":"CGA_EXIT","ts":105.300,"actor":"0x000A2C94","returnRegion":0,"aversionDurationSec":2.65,"trigger":"DIALOGUE_SYNC"}
{"t":"REGION_CHANGE","ts":105.300,"actor":"0x000A2C94","from":9,"to":0,"dwellSec":2.60,"fromLabel":"ULPeripheral","toLabel":"LeftEye"}
```

**Reading this trace:**
1. NPC Orgnar dwells on the player's left eye for 0.87 s (LOGIC mode).
2. Saccade to the mouth — shifts to AFFECT mode.
3. Blink at 101.2 s.
4. Mutual gaze threshold hit at 102.65 s → CGA fires, eyes dart to UL
   Peripheral (region 9, THINK mode).
5. CGA holds for 2.65 s, then returns to left eye on dialogue sync.
6. Throughout, `region == hitRegion` (no mismatches).

---

## 8. Post-Processing Tools (Future)

Once the trace format is stable, the following tools can be built:

| Tool | Purpose |
|:-----|:--------|
| `scripts/analyze_trace.py` | Parse JSONL, compute dwell distributions, agreement rates, saccade statistics |
| `scripts/trace_to_csv.py` | Convert to CSV for spreadsheet analysis |
| HCEP Desktop ingestion | Pipe trace data to the Bridge for live visualisation |
| Region heat map | Aggregate dwell time per region into a panel-shaped heat map image |

---

*Approved by Kirk LaSalle, 2026-10-03.*
