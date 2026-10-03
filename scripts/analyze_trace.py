#!/usr/bin/env python3
"""
TrueGaze Trace Log Analysis Tool (A8.3)
Parses TrueGaze_GazeTrace.jsonl telemetry files conforming to
'docs/Trace Log Schema — TrueGaze Gaze Diagnostics.md'.

Computes:
  - Event breakdown and timeline duration
  - Three-way agreement rate (intended vs classified vs hit-detected)
  - Region dwell-time distributions
  - Saccade kinematic distributions (amplitude, duration, velocity)
  - Cognitive Gaze Aversion (CGA) episode statistics
  - Eyelid blink intervals and suppression metrics
  - Biomechanical anomaly detection
"""

import argparse
import json
import math
import os
import sys
from collections import Counter, defaultdict
from typing import Any, Dict, List, Optional


REGION_NAMES = {
    0: "LeftEye",
    1: "RightEye",
    2: "Mouth",
    3: "Forehead",
    4: "Chin",
    5: "Torso",
    6: "RightHand",
    7: "LeftHand",
    8: "Ground",
    9: "ULPeripheral",
    10: "URPeripheral",
    11: "LLPeripheral",
    12: "LRPeripheral",
}


def parse_args():
    parser = argparse.ArgumentParser(
        description="Analyze TrueGaze structured JSONL trace telemetry logs."
    )
    parser.add_argument(
        "-f", "--file",
        default="TrueGaze_GazeTrace.jsonl",
        help="Path to TrueGaze_GazeTrace.jsonl (default: TrueGaze_GazeTrace.jsonl)"
    )
    parser.add_argument(
        "--actor",
        help="Filter analysis to a specific actor FormID (e.g. 0x000A2C94)"
    )
    parser.add_argument(
        "--strict",
        action="store_true",
        help="Enforce strict assertions (e.g. >= 99.5%% region agreement) with non-zero exit code"
    )
    parser.add_argument(
        "--json-out",
        help="Write summary statistics to a JSON output file"
    )
    return parser.parse_args()


def load_events(file_path: str, actor_filter: Optional[str] = None) -> List[Dict[str, Any]]:
    if not os.path.exists(file_path):
        candidates = [
            file_path,
            os.path.join("Data", "SKSE", "Plugins", file_path),
            os.path.join(os.path.expanduser("~"), "Documents", "My Games", "Skyrim Special Edition", "SKSE", file_path),
        ]
        found = False
        for c in candidates:
            if os.path.exists(c):
                file_path = c
                found = True
                break
        if not found:
            print(f"Error: Trace log file not found at '{file_path}'.", file=sys.stderr)
            sys.exit(2)

    events = []
    line_no = 0
    with open(file_path, "r", encoding="utf-8") as f:
        for line in f:
            line_no += 1
            line = line.strip()
            if not line:
                continue
            try:
                ev = json.loads(line)
            except json.JSONDecodeError as err:
                print(f"Warning: Line {line_no} failed JSON decode: {err}", file=sys.stderr)
                continue

            if actor_filter:
                ev_actor = ev.get("actor", "").lower()
                target_actor = actor_filter.lower()
                if not ev_actor.endswith(target_actor.lstrip("0x")):
                    continue

            events.append(ev)
    return events


def analyze(events: List[Dict[str, Any]]) -> Dict[str, Any]:
    if not events:
        return {"error": "No events found"}

    event_counts = Counter()
    actors = set()
    start_ts = float("inf")
    end_ts = float("-inf")

    # Metrics
    total_gaze_ticks = 0
    total_hits = 0
    total_agreements = 0
    total_mismatches = 0
    total_panel_misses = 0

    region_dwell_time = defaultdict(float)
    mode_counts = Counter()

    saccade_amplitudes = []
    saccade_durations = []

    cga_episodes = 0
    cga_durations = []
    cga_quadrants = Counter()

    blink_durations = []
    blink_intervals = []

    anomalies = []

    for ev in events:
        t = ev.get("t", "UNKNOWN")
        event_counts[t] += 1
        actor = ev.get("actor", "UNKNOWN")
        if actor != "UNKNOWN":
            actors.add(actor)

        ts = ev.get("ts", 0.0)
        if ts < start_ts:
            start_ts = ts
        if ts > end_ts:
            end_ts = ts

        if t == "GAZE_TICK":
            total_gaze_ticks += 1
            region = ev.get("region", 255)
            hit_region = ev.get("hitRegion", 255)
            dt = ev.get("dt", 0.016)
            mode = ev.get("mode", "UNKNOWN")
            yaw = ev.get("yaw", 0.0)
            pitch = ev.get("pitch", 0.0)

            mode_counts[mode] += 1
            region_dwell_time[region] += dt

            # Biomechanical bounds check
            if abs(yaw) > 85.0 or abs(pitch) > 85.0 or math.isnan(yaw) or math.isnan(pitch):
                anomalies.append(f"Unphysiological deflection at ts={ts:.3f}: yaw={yaw}, pitch={pitch}")

            if hit_region != 255:
                total_hits += 1
                if hit_region == region:
                    total_agreements += 1
                else:
                    total_mismatches += 1
            else:
                total_panel_misses += 1

        elif t == "SACCADE_ONSET":
            amp = ev.get("angularDistance", 0.0)
            exp_dur = ev.get("expectedDuration", 0.0)
            saccade_amplitudes.append(amp)
            if exp_dur > 0.150:
                anomalies.append(f"Excessive predicted saccade duration at ts={ts:.3f}: {exp_dur:.3f}s for amp={amp:.1f}deg")

        elif t == "SACCADE_COMPLETE":
            act_dur = ev.get("actualDuration", 0.0)
            saccade_durations.append(act_dur)

        elif t == "REGION_CHANGE":
            from_reg = ev.get("from", 255)
            to_reg = ev.get("to", 255)
            dwell = ev.get("dwellSec", 0.0)
            # Dwell recorded from explicit region change
            if dwell > 0:
                region_dwell_time[from_reg] += dwell

        elif t == "CGA_ENTER":
            cga_episodes += 1
            quad = ev.get("quadrant", 0)
            cga_quadrants[quad] += 1

        elif t == "CGA_EXIT":
            dur = ev.get("aversionDurationSec", 0.0)
            if dur > 0:
                cga_durations.append(dur)

        elif t == "BLINK":
            b_dur = ev.get("durationMs", 0.0)
            b_int = ev.get("intervalSec", 0.0)
            blink_durations.append(b_dur)
            blink_intervals.append(b_int)

        elif t == "HIT_MISMATCH":
            reason = ev.get("reason", "BOUNDARY")
            classified = ev.get("classified", 255)
            hit = ev.get("hit", 255)
            if reason != "BOUNDARY" and reason != "TRANSIT":
                anomalies.append(f"Hit mismatch [{reason}] at ts={ts:.3f}: classified={classified} != hit={hit}")

    duration_sec = max(0.0, end_ts - start_ts) if start_ts != float("inf") else 0.0
    agreement_rate_pct = (total_agreements / total_hits * 100.0) if total_hits > 0 else 100.0
    panel_hit_pct = (total_hits / total_gaze_ticks * 100.0) if total_gaze_ticks > 0 else 0.0

    return {
        "event_counts": dict(event_counts),
        "total_events": len(events),
        "actors": list(actors),
        "duration_sec": duration_sec,
        "total_gaze_ticks": total_gaze_ticks,
        "total_hits": total_hits,
        "total_agreements": total_agreements,
        "total_mismatches": total_mismatches,
        "total_panel_misses": total_panel_misses,
        "agreement_rate_pct": agreement_rate_pct,
        "panel_hit_pct": panel_hit_pct,
        "region_dwell_sec": {REGION_NAMES.get(k, f"Region_{k}"): v for k, v in region_dwell_time.items()},
        "mode_counts": dict(mode_counts),
        "saccade_count": len(saccade_amplitudes),
        "saccade_mean_amp": sum(saccade_amplitudes) / len(saccade_amplitudes) if saccade_amplitudes else 0.0,
        "saccade_max_amp": max(saccade_amplitudes) if saccade_amplitudes else 0.0,
        "saccade_mean_dur_ms": (sum(saccade_durations) / len(saccade_durations) * 1000.0) if saccade_durations else 0.0,
        "saccade_max_dur_ms": (max(saccade_durations) * 1000.0) if saccade_durations else 0.0,
        "cga_episodes": cga_episodes,
        "cga_mean_dur_sec": sum(cga_durations) / len(cga_durations) if cga_durations else 0.0,
        "cga_quadrants": {REGION_NAMES.get(k, f"Quad_{k}"): v for k, v in cga_quadrants.items()},
        "blink_count": len(blink_durations),
        "blink_mean_interval_sec": sum(blink_intervals) / len(blink_intervals) if blink_intervals else 0.0,
        "anomalies": anomalies,
    }


def print_report(results: Dict[str, Any]):
    print("=" * 70)
    print(" TrueGaze™ — Biological Gaze Trace Telemetry Report (A8.3)")
    print("=" * 70)

    if "error" in results:
        print(f"Error: {results['error']}")
        return

    print(f"Total Events:         {results['total_events']:,}")
    print(f"Session Duration:     {results['duration_sec']:.2f} seconds")
    print(f"Active Actors ({len(results['actors'])}):    {', '.join(results['actors']) if results['actors'] else 'None'}")
    print()

    print("--- Event Breakdown ---")
    for ev_type, count in sorted(results["event_counts"].items()):
        print(f"  {ev_type:<20} {count:>8,}")
    print()

    print("--- Three-Way Agreement & Physical Hit Detection (A7/A8) ---")
    print(f"  Gaze Tick Evaluations:  {results['total_gaze_ticks']:,}")
    print(f"  Panel Quad Hits:        {results['total_hits']:,} ({results['panel_hit_pct']:.1f}%)")
    print(f"  Agreements (Class==Hit):{results['total_agreements']:,}")
    print(f"  Hit Mismatches:         {results['total_mismatches']:,}")
    print(f"  Panel Misses (off-quad):{results['total_panel_misses']:,}")
    agr = results["agreement_rate_pct"]
    pass_str = "[PASS] >= 99.5%" if agr >= 99.5 else "[FAIL] < 99.5%"
    print(f"  Agreement Rate:         {agr:.2f}%  {pass_str}")
    print()

    print("--- Region Dwell-Time Distribution ---")
    total_dwell = sum(results["region_dwell_sec"].values())
    for r_name, dur in sorted(results["region_dwell_sec"].items(), key=lambda x: x[1], reverse=True):
        pct = (dur / total_dwell * 100.0) if total_dwell > 0 else 0.0
        bar = "#" * int(pct / 2.5)
        print(f"  {r_name:<16} {dur:>6.2f}s ({pct:>5.1f}%)  {bar}")
    print()

    if results["saccade_count"] > 0:
        print("--- Saccade Kinematics (Main Sequence) ---")
        print(f"  Total Saccades:       {results['saccade_count']:,}")
        print(f"  Mean Amplitude:       {results['saccade_mean_amp']:.1f}° (Max: {results['saccade_max_amp']:.1f}°)")
        print(f"  Mean Duration:        {results['saccade_mean_dur_ms']:.1f} ms (Max: {results['saccade_max_dur_ms']:.1f} ms)")
        print()

    if results["cga_episodes"] > 0:
        print("--- Cognitive Gaze Aversion (CGA) ---")
        print(f"  Aversion Episodes:    {results['cga_episodes']:,}")
        print(f"  Mean Duration:        {results['cga_mean_dur_sec']:.2f} s")
        quad_str = ", ".join(f"{k}: {v}" for k, v in results["cga_quadrants"].items())
        print(f"  Quadrants Visited:    {quad_str}")
        print()

    if results["blink_count"] > 0:
        print("--- Eyelid Blink Suppression ---")
        print(f"  Total Blinks:         {results['blink_count']:,}")
        print(f"  Mean Interval:        {results['blink_mean_interval_sec']:.2f} s")
        print()

    print("--- Biomechanical Anomaly Checks ---")
    if results["anomalies"]:
        print(f"  [WARN] {len(results['anomalies'])} anomalies flagged:")
        for anom in results["anomalies"][:10]:
            print(f"    - {anom}")
        if len(results["anomalies"]) > 10:
            print(f"    ... and {len(results['anomalies']) - 10} more.")
    else:
        print("  [OK] Zero biomechanical anomalies or unphysiological deflections detected.")
    print("=" * 70)


def main():
    args = parse_args()
    events = load_events(args.file, args.actor)
    results = analyze(events)
    print_report(results)

    if args.json_out:
        with open(args.json_out, "w", encoding="utf-8") as f:
            json.dump(results, f, indent=2)
        print(f"Wrote summary JSON to '{args.json_out}'.")

    if args.strict:
        if results.get("agreement_rate_pct", 0.0) < 99.5:
            print("Strict check failed: Agreement rate below 99.5%", file=sys.stderr)
            sys.exit(1)
        if len(results.get("anomalies", [])) > 0:
            print(f"Strict check failed: {len(results['anomalies'])} anomalies found", file=sys.stderr)
            sys.exit(1)

    sys.exit(0)


if __name__ == "__main__":
    main()
