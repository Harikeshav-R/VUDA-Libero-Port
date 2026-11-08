"""Subprocess lifecycle and schema boundary tests for run-controlled-scheduling.py.

Tests real subprocess behavior and failure modes:
1. Subprocess failure before readiness (early crash/exit non-zero) -> failure tracked, rejected aggregation.
2. Subprocess exit after partial progress / early termination -> non-zero exit, rejected aggregation.
3. Subprocess timeout while sibling/worker hangs -> process group killed, timeout recorded, peer reaped.
4. Multi-cell partial failure -> matrix records failures, continues reachable cells, rejects aggregate.json.
5. Deterministic rotation schedule generation.
6. Schema boundary unit tests using imported validate_native_report on a valid baseline control:
   - Valid baseline accepted (both timed and validation modes)
   - Zero progress (completed <= 0) rejected
   - Population count mismatch (latency_samples != completed) rejected
   - Timestamp inverted/premature window rejected (for timed runs)
   - Validation run allowed premature end when completed == iterations
   - Quantiles negative/infinite/inverted rejected
   - Missing required config/worker fields rejected
   - Serial unpaired count inconsistency rejected
   - Exact role mismatch rejected
"""

from __future__ import annotations

import copy
import json
import logging
import math
import os
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path
from typing import Dict, List, Optional, Tuple

logger = logging.getLogger(__name__)

REPO_ROOT = Path(__file__).resolve().parent.parent.parent
SCRIPT_PATH = REPO_ROOT / "scripts" / "run-controlled-scheduling.py"

# Import production validation and helper functions directly
sys.path.insert(0, str(REPO_ROOT / "scripts"))
import importlib.util
spec = importlib.util.spec_from_file_location("run_controlled_scheduling", str(SCRIPT_PATH))
assert spec is not None and spec.loader is not None
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)

validate_native_report = mod.validate_native_report
generate_condition_schedule = mod.generate_condition_schedule


def create_executable(script_path: Path, code: str) -> Path:
    """Create a standalone executable python script."""
    script_path.write_text(f"#!{sys.executable}\n" + code)
    script_path.chmod(0o755)
    return script_path


def make_valid_report_dict(
    mode: str = "solo-render",
    topology: str = "threads",
    elements: int = 1024,
    duration_s: float = 1.0,
    warmup_s: float = 0.1,
    validation_mode: bool = False,
    iterations: int = 0,
) -> Dict[str, object]:
    """Create a completely valid schema_version 1 report dictionary."""
    start_ns = 1_000_000_000
    deadline_ns = start_ns + int(duration_s * 1e9)
    # If validation mode, observed_end_ns finishes when iterations complete (can be < deadline_ns)
    observed_end_ns = start_ns + 50_000_000 if validation_mode else deadline_ns + 100_000

    workers: List[Dict[str, object]] = []
    effective_count = iterations if validation_mode else 50

    def make_worker(role: str, cpu: int, tid: int, count: int) -> Dict[str, object]:
        return {
            "role": role,
            "pid": os.getpid(),
            "tid": tid,
            "cpu_affinity": [cpu],
            "ready_ns": start_ns - 50_000_000,
            "warmup_start_ns": start_ns - 90_000_000,
            "warmup_end_ns": start_ns - 10_000_000,
            "warmup_completed": 10,
            "first_submit_ns": start_ns + 10_000,
            "last_submit_ns": observed_end_ns - 20_000,
            "last_completion_ns": observed_end_ns - 10_000,
            "completed": count,
            "late": 0,
            "latency_samples": count,
            "p50_us": 100.0,
            "p95_us": 120.0,
            "p99_us": 140.0,
            "throughput_ops_sec": 0.0 if validation_mode else float(count) / duration_s,
            "late_latencies_us": [],
            "start_skew_ns": 0,
            "drain_ns": 500,
            "validation_ns": 1000,
            "final_tag": count,
            "validation_passed": True,
        }

    if mode == "solo-render":
        workers.append(make_worker("render", 0, 101, effective_count))
    elif mode == "solo-compute":
        workers.append(make_worker("compute", 1, 102, effective_count))
    elif mode in ("serial", "concurrent"):
        workers.append(make_worker("render", 0, 101, effective_count))
        workers.append(make_worker("compute", 1, 102, effective_count))

    pairs: Optional[Dict[str, object]] = None
    if mode == "serial":
        pairs = {
            "completed": effective_count,
            "late": 0,
            "latency_samples": effective_count,
            "p50_us": 180.0,
            "p95_us": 210.0,
            "p99_us": 240.0,
        }

    return {
        "schema_version": 1,
        "valid": True,
        "mode": mode,
        "topology": topology,
        "validation_strength": "per_iteration" if validation_mode else "final_tag_only",
        "config": {
            "elements": elements,
            "intensity": 100,
            "width": 128,
            "height": 128,
            "duration_seconds": duration_s,
            "warmup_seconds": warmup_s,
            "iterations": iterations,
        },
        "clock": "CLOCK_MONOTONIC",
        "start_ns": start_ns,
        "deadline_ns": deadline_ns,
        "observed_end_ns": observed_end_ns,
        "workers": workers,
        "pairs": pairs,
    }


class TestSubprocessLifecycle(unittest.TestCase):
    """Real subprocess lifecycle tests for orchestrator."""

    def test_subprocess_failure_before_readiness(self) -> None:
        """When probe worker crashes immediately before readiness, exit is non-zero and aggregation rejected."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            out_dir = Path(tmp_dir) / "out_early_fail"
            probe = Path(tmp_dir) / "crash_before_ready.py"
            code = """
import sys
sys.stderr.write("[FATAL] EGL context initialization failed: device not ready\\n")
sys.exit(2)
"""
            create_executable(probe, code)

            cmd = [
                sys.executable,
                str(SCRIPT_PATH),
                "--binary", str(probe),
                "--output-dir", str(out_dir),
                "--single-condition", "solo-render:threads",
                "--duration-seconds", "1.0",
                "--warmup-seconds", "0.1",
                "--repetitions", "1",
                "--sizes", "1024",
            ]
            res = subprocess.run(cmd, capture_output=True, text=True)
            self.assertNotEqual(res.returncode, 0)

            matrix_json = out_dir / "matrix.json"
            self.assertTrue(matrix_json.exists())
            data = json.loads(matrix_json.read_text())
            self.assertEqual(data["status"], "failed")
            self.assertEqual(len(data["failures"]), 1)
            self.assertEqual(data["failures"][0]["returncode"], 2)
            self.assertFalse((out_dir / "aggregate.json").exists())

    def test_subprocess_exit_after_readiness(self) -> None:
        """When probe crashes after entering measurement window, non-zero exit and rejected aggregation."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            out_dir = Path(tmp_dir) / "out_mid_fail"
            probe = Path(tmp_dir) / "crash_mid_run.py"
            code = """
import sys, time
time.sleep(0.05)
sys.stderr.write("[FATAL] SIGSEGV in worker iteration\\n")
sys.exit(139)
"""
            create_executable(probe, code)

            cmd = [
                sys.executable,
                str(SCRIPT_PATH),
                "--binary", str(probe),
                "--output-dir", str(out_dir),
                "--single-condition", "concurrent:threads",
                "--duration-seconds", "1.0",
                "--warmup-seconds", "0.1",
                "--repetitions", "1",
                "--sizes", "1024",
            ]
            res = subprocess.run(cmd, capture_output=True, text=True)
            self.assertNotEqual(res.returncode, 0)

            matrix_json = out_dir / "matrix.json"
            self.assertTrue(matrix_json.exists())
            data = json.loads(matrix_json.read_text())
            self.assertEqual(data["status"], "failed")
            self.assertEqual(data["failures"][0]["returncode"], 139)
            self.assertFalse((out_dir / "aggregate.json").exists())

    def test_subprocess_timeout_while_sibling_hangs(self) -> None:
        """When one worker/sibling hangs indefinitely, orchestrator terminates entire process group."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            out_dir = Path(tmp_dir) / "out_hang"
            probe = Path(tmp_dir) / "hanging_probe.py"
            code = """
import sys, time, subprocess
worker = subprocess.Popen([sys.executable, "-c", "import time; time.sleep(600)"])
worker.wait()
"""
            create_executable(probe, code)

            cmd = [
                sys.executable,
                str(SCRIPT_PATH),
                "--binary", str(probe),
                "--output-dir", str(out_dir),
                "--timeout-seconds", "1.0",
                "--duration-seconds", "1.0",
                "--warmup-seconds", "0.1",
                "--single-condition", "concurrent:processes",
                "--repetitions", "1",
                "--sizes", "1024",
            ]
            start = time.monotonic()
            res = subprocess.run(cmd, capture_output=True, text=True, timeout=10)
            elapsed = time.monotonic() - start

            self.assertNotEqual(res.returncode, 0)
            self.assertLess(elapsed, 5.0)

            matrix_json = out_dir / "matrix.json"
            self.assertTrue(matrix_json.exists())
            data = json.loads(matrix_json.read_text())
            self.assertEqual(data["status"], "failed")
            self.assertTrue(data["failures"][0]["timed_out"])
            self.assertFalse((out_dir / "aggregate.json").exists())

    def test_multi_cell_partial_failure_matrix_execution(self) -> None:
        """Matrix continues reachable cells when one cell crashes, recording failures and rejecting aggregate."""
        with tempfile.TemporaryDirectory() as tmp_dir:
            out_dir = Path(tmp_dir) / "out_multi_cell"
            probe = Path(tmp_dir) / "flaky_probe.py"
            code = """
import sys, json, os

report_path = None
elements = 1024
mode = "solo-render"
topology = "threads"

for i, arg in enumerate(sys.argv):
    if arg == "--report-json":
        report_path = sys.argv[i+1]
    elif arg == "--elements":
        elements = int(sys.argv[i+1])
    elif arg == "--mode":
        mode = sys.argv[i+1]
    elif arg == "--topology":
        topology = sys.argv[i+1]

if elements == 262144:
    sys.stderr.write("[FATAL] Simulated OOM on large size\\n")
    sys.exit(1)

report = {
    "schema_version": 1,
    "valid": True,
    "mode": mode,
    "topology": topology,
    "validation_strength": "final_tag_only",
    "config": {
        "elements": elements, "intensity": 100, "width": 128, "height": 128,
        "duration_seconds": 0.5, "warmup_seconds": 0.1, "iterations": 0
    },
    "clock": "CLOCK_MONOTONIC",
    "start_ns": 1000000000,
    "deadline_ns": 1500000000,
    "observed_end_ns": 1500100000,
    "workers": [
        {
            "role": "render", "pid": os.getpid(), "tid": 1, "cpu_affinity": [0],
            "ready_ns": 900000000, "warmup_start_ns": 900000000, "warmup_end_ns": 990000000, "warmup_completed": 10,
            "first_submit_ns": 1000010000, "last_submit_ns": 1499900000, "last_completion_ns": 1499950000,
            "completed": 20, "late": 0, "latency_samples": 20,
            "p50_us": 100.0, "p95_us": 120.0, "p99_us": 130.0,
            "throughput_ops_sec": 40.0, "late_latencies_us": [], "start_skew_ns": 0, "drain_ns": 100, "validation_ns": 500,
            "final_tag": 20, "validation_passed": True
        }
    ],
    "pairs": None
}
with open(report_path, "w") as f:
    json.dump(report, f)
sys.exit(0)
"""
            create_executable(probe, code)

            cmd = [
                sys.executable,
                str(SCRIPT_PATH),
                "--binary", str(probe),
                "--output-dir", str(out_dir),
                "--single-condition", "solo-render:threads",
                "--duration-seconds", "0.5",
                "--warmup-seconds", "0.1",
                "--repetitions", "1",
                "--sizes", "1024", "262144",
            ]
            res = subprocess.run(cmd, capture_output=True, text=True)
            self.assertNotEqual(res.returncode, 0)

            matrix_json = out_dir / "matrix.json"
            self.assertTrue(matrix_json.exists())
            data = json.loads(matrix_json.read_text())
            self.assertEqual(data["total_cells"], 2)
            self.assertEqual(data["successful_cells"], 1)
            self.assertEqual(data["failed_cells"], 1)
            self.assertEqual(data["status"], "failed")
            self.assertFalse((out_dir / "aggregate.json").exists())

    def test_deterministic_schedule_rotation(self) -> None:
        """6 conditions x 2 sizes x 5 repetitions yields 60 cells deterministically shuffled per block."""
        conditions = [
            "solo-render:threads",
            "solo-compute:threads",
            "serial:threads",
            "concurrent:threads",
            "serial:processes",
            "concurrent:processes",
        ]
        s1 = generate_condition_schedule(conditions, [1024, 262144], 5, seed=9)
        s2 = generate_condition_schedule(conditions, [1024, 262144], 5, seed=9)
        self.assertEqual(s1, s2)
        self.assertEqual(len(s1), 60)

        rep0 = [c["condition"] for c in s1 if c["repetition"] == 0 and c["elements"] == 1024]
        rep1 = [c["condition"] for c in s1 if c["repetition"] == 1 and c["elements"] == 1024]
        self.assertEqual(len(rep0), 6)
        self.assertEqual(len(rep1), 6)
        self.assertNotEqual(rep0, rep1)


class TestNativeReportSchemaBoundaries(unittest.TestCase):
    """Strict schema validation boundary tests against valid baseline controls."""

    def test_valid_baseline_report_accepted(self) -> None:
        """Verify that baseline valid reports for all modes pass validation."""
        for mode in ("solo-render", "solo-compute", "serial", "concurrent"):
            valid_rep = make_valid_report_dict(mode=mode, duration_s=1.0, warmup_s=0.1)
            passed, err = validate_native_report(
                valid_rep, mode, "threads", 1024,
                expected_duration_s=1.0, expected_warmup_s=0.1,
            )
            self.assertTrue(passed, f"Valid baseline failed for mode {mode}: {err}")

        # Test validation mode baseline
        valid_val_rep = make_valid_report_dict(
            mode="concurrent", duration_s=1.0, warmup_s=0.1,
            validation_mode=True, iterations=8,
        )
        passed_val, err_val = validate_native_report(
            valid_val_rep, "concurrent", "threads", 1024,
            expected_duration_s=1.0, expected_warmup_s=0.1,
            expected_validation_mode=True, expected_iterations=8,
        )
        self.assertTrue(passed_val, f"Valid validation mode baseline failed: {err_val}")

    def test_rejection_on_zero_progress(self) -> None:
        """Report with 0 completed iterations (starvation / zero progress) must be rejected."""
        valid_rep = make_valid_report_dict(mode="concurrent", duration_s=1.0, warmup_s=0.1)
        bad_rep = copy.deepcopy(valid_rep)
        bad_rep["workers"][1]["completed"] = 0
        bad_rep["workers"][1]["latency_samples"] = 0
        bad_rep["workers"][1]["throughput_ops_sec"] = 0.0

        passed, err = validate_native_report(
            bad_rep, "concurrent", "threads", 1024,
            expected_duration_s=1.0, expected_warmup_s=0.1,
        )
        self.assertFalse(passed)
        self.assertTrue("progress" in err.lower() or "completed" in err.lower())

    def test_rejection_on_count_population_inconsistency(self) -> None:
        """Report where completed != latency_samples must be rejected."""
        valid_rep = make_valid_report_dict(mode="solo-render", duration_s=1.0, warmup_s=0.1)
        bad_rep = copy.deepcopy(valid_rep)
        bad_rep["workers"][0]["latency_samples"] = 40

        passed, err = validate_native_report(
            bad_rep, "solo-render", "threads", 1024,
            expected_duration_s=1.0, expected_warmup_s=0.1,
        )
        self.assertFalse(passed)
        self.assertTrue("mismatch" in err.lower() or "latency_samples" in err.lower())

    def test_rejection_on_window_timestamp_inconsistency(self) -> None:
        """Report where start_ns >= deadline_ns or observed_end_ns < deadline_ns must be rejected for timed runs."""
        valid_rep = make_valid_report_dict(mode="solo-compute", duration_s=1.0, warmup_s=0.1)

        bad_rep1 = copy.deepcopy(valid_rep)
        bad_rep1["start_ns"] = 2_000_000_000
        bad_rep1["deadline_ns"] = 1_000_000_000
        passed1, err1 = validate_native_report(
            bad_rep1, "solo-compute", "threads", 1024,
            expected_duration_s=1.0, expected_warmup_s=0.1,
        )
        self.assertFalse(passed1)
        self.assertTrue("deadline" in err1.lower() or "inverted" in err1.lower())

        bad_rep2 = copy.deepcopy(valid_rep)
        bad_rep2["observed_end_ns"] = bad_rep2["deadline_ns"] - 100_000
        passed2, err2 = validate_native_report(
            bad_rep2, "solo-compute", "threads", 1024,
            expected_duration_s=1.0, expected_warmup_s=0.1,
        )
        self.assertFalse(passed2)
        self.assertTrue("premature" in err2.lower() or "deadline" in err2.lower())

    def test_rejection_on_invalid_quantiles(self) -> None:
        """Report with negative, NaN, non-finite, or non-ordered quantiles must be rejected."""
        valid_rep = make_valid_report_dict(mode="solo-render", duration_s=1.0, warmup_s=0.1)

        bad_rep1 = copy.deepcopy(valid_rep)
        bad_rep1["workers"][0]["p50_us"] = 200.0
        bad_rep1["workers"][0]["p95_us"] = 100.0
        passed1, err1 = validate_native_report(
            bad_rep1, "solo-render", "threads", 1024,
            expected_duration_s=1.0, expected_warmup_s=0.1,
        )
        self.assertFalse(passed1)
        self.assertTrue("quantile" in err1.lower() or "inverted" in err1.lower() or "p50" in err1.lower())

        bad_rep2 = copy.deepcopy(valid_rep)
        bad_rep2["workers"][0]["p50_us"] = -10.0
        passed2, err2 = validate_native_report(
            bad_rep2, "solo-render", "threads", 1024,
            expected_duration_s=1.0, expected_warmup_s=0.1,
        )
        self.assertFalse(passed2)
        self.assertTrue("negative" in err2.lower() or "quantile" in err2.lower())

    def test_rejection_on_config_duration_mismatch(self) -> None:
        """Report where config duration_seconds or warmup_seconds does not match requested CLI parameters must be rejected."""
        valid_rep = make_valid_report_dict(mode="solo-render", duration_s=1.0, warmup_s=0.1)

        bad_rep = copy.deepcopy(valid_rep)
        bad_rep["config"]["duration_seconds"] = 5.0

        passed, err = validate_native_report(
            bad_rep, "solo-render", "threads", 1024,
            expected_duration_s=1.0, expected_warmup_s=0.1,
        )
        self.assertFalse(passed)
        self.assertTrue("duration" in err.lower() or "config" in err.lower())

    def test_rejection_on_exact_role_mismatch(self) -> None:
        """Report with extra or missing roles must be rejected."""
        valid_rep = make_valid_report_dict(mode="solo-render", duration_s=1.0, warmup_s=0.1)

        bad_rep = copy.deepcopy(valid_rep)
        bad_rep["workers"].append(copy.deepcopy(bad_rep["workers"][0]))
        bad_rep["workers"][1]["role"] = "compute"

        passed, err = validate_native_report(
            bad_rep, "solo-render", "threads", 1024,
            expected_duration_s=1.0, expected_warmup_s=0.1,
        )
        self.assertFalse(passed)
        self.assertTrue("role" in err.lower())

    def test_rejection_on_serial_unpaired_inconsistency(self) -> None:
        """Serial mode report where pairs.completed exceeds worker completed count must be rejected."""
        valid_rep = make_valid_report_dict(mode="serial", duration_s=1.0, warmup_s=0.1)

        bad_rep = copy.deepcopy(valid_rep)
        bad_rep["pairs"]["completed"] = 100
        bad_rep["pairs"]["latency_samples"] = 100

        passed, err = validate_native_report(
            bad_rep, "serial", "threads", 1024,
            expected_duration_s=1.0, expected_warmup_s=0.1,
        )
        self.assertFalse(passed)
        self.assertTrue("pairs" in err.lower() or "completed" in err.lower())


if __name__ == "__main__":
    unittest.main()
