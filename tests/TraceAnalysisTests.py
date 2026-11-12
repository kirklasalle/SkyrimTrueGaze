import json
import os
import pathlib
import runpy
import subprocess
import sys
import tempfile
import unittest


ANALYZER = pathlib.Path(__file__).resolve().parents[1] / "scripts" / "analyze_trace.py"
_module = runpy.run_path(str(ANALYZER))
analyze = _module["analyze"]
load_events = _module["load_events"]


class IncomingTraceAnalysisTests(unittest.TestCase):
    def test_unavailable_evidence_is_not_accuracy_or_a_miss(self):
        events = [{"t": "GAZE_TICK", "schema": 2, "region": 1,
                   "hitRegion": 255, "hitSemantics": "retiredSelfPanel"}]
        result = analyze(events)
        self.assertIsNone(result["agreement_rate_pct"])
        self.assertEqual(result["unavailable_hits"], 1)
        self.assertEqual(result["total_panel_misses"], 0)
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "trace.jsonl"
            path.write_text(json.dumps(events[0]) + "\n", encoding="utf-8")
            process = subprocess.run(
                [sys.executable, str(ANALYZER), "--file", str(path), "--strict"],
                capture_output=True, text=True, encoding="utf-8",
                env={**os.environ, "PYTHONIOENCODING": "utf-8"},
            )
            self.assertEqual(process.returncode, 1)
            self.assertIn("Legacy agreement unavailable", process.stderr)

    def test_incoming_ownership_and_legacy_are_separate(self):
        incoming = {"t": "INCOMING_HIT", "schema": 2,
                    "observer": "0x00000001", "target": "0x00000002",
                    "measurement": "anatomy", "visibilityVerified": False}
        legacy = {"t": "GAZE_TICK", "region": 1, "hitRegion": 1}
        result = analyze([incoming, legacy])
        self.assertEqual(result["agreement_rate_pct"], 100)
        self.assertEqual(result["incoming_measurements"], {"anatomy": 1})
        self.assertEqual(result["incoming_unverified"], 1)
        with tempfile.TemporaryDirectory() as directory:
            path = pathlib.Path(directory) / "trace.jsonl"
            path.write_text(json.dumps(incoming) + "\n", encoding="utf-8")
            self.assertEqual(load_events(str(path), "0x00000002"), [incoming])
            self.assertEqual(load_events(str(path), "0x00000003"), [])


if __name__ == "__main__":
    unittest.main()
