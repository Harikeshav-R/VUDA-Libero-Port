#!/usr/bin/env python3
"""EXP-009 controlled scheduling matrix orchestrator.

Executes deterministic rotation of 6 conditions across multiple buffer sizes
and repetitions, monitors timeouts with process-group cleanup, validates native
schema version 1 JSON reports, logs command/stdout/stderr receipts, and produces
matrix.json and aggregate.json.
"""

from __future__ import annotations

import argparse
import datetime
import json
import logging
import math
import os
import random
import signal
import statistics
import subprocess
import sys
import time
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple, Union

logger = logging.getLogger("run_controlled_scheduling")

ALL_CONDITIONS: List[str] = [
    "solo-render:threads",
    "solo-compute:threads",
    "serial:threads",
    "concurrent:threads",
    "serial:processes",
    "concurrent:processes",
]


def get_allowed_cpus() -> List[int]:
    """Determine allowed CPU cores for the current process/allocation."""
    if hasattr(os, "sched_getaffinity"):
        try:
            affinity = sorted(list(os.sched_getaffinity(0)))  # type: ignore[attr-defined]
            if affinity:
                return affinity
        except OSError as e:
            logger.debug("Failed sched_getaffinity: %s", e)
    cpu_count = os.cpu_count() or 1
    return list(range(cpu_count))


def select_cpu_placement(
    explicit_render_cpu: Optional[int],
    explicit_compute_cpu: Optional[int],
) -> Tuple[int, int]:
    """Select render and compute CPUs.

    Orchestrator assigns first two allowed CPUs to render and compute roles,
    leaving orchestrator unpinned on the allocated cpuset.
    """
    allowed = get_allowed_cpus()
    r_cpu = explicit_render_cpu
    c_cpu = explicit_compute_cpu

    if r_cpu is None:
        r_cpu = allowed[0] if len(allowed) > 0 else 0
    if c_cpu is None:
        c_cpu = allowed[1] if len(allowed) > 1 else allowed[0]

    return r_cpu, c_cpu


def generate_condition_schedule(
    conditions: Sequence[str],
    sizes: Sequence[int],
    repetitions: int,
    seed: int,
) -> List[Dict[str, Union[str, int]]]:
    """Generate deterministic rotation schedule across repetitions and sizes.

    For each repetition block and size, permutes conditions deterministically
    using a seeded random generator so that every condition runs exactly once
    per (size, repetition) block in a varied order across blocks.
    """
    schedule: List[Dict[str, Union[str, int]]] = []
    rng = random.Random(seed)

    for rep in range(repetitions):
        for size in sizes:
            block_conditions = list(conditions)
            rng.shuffle(block_conditions)
            for idx, cond in enumerate(block_conditions):
                schedule.append({
                    "condition": cond,
                    "elements": size,
                    "repetition": rep,
                    "block_index": idx,
                })
    return schedule


def compute_quantiles(sorted_samples: Sequence[float]) -> Dict[str, float]:
    """Calculate nearest-rank ceil(p * N) - 1 quantiles for p50, p95, p99."""
    n = len(sorted_samples)
    if n == 0:
        return {"p50": 0.0, "p95": 0.0, "p99": 0.0}

    def rank_val(p: float) -> float:
        rank = max(0, min(n - 1, math.ceil(p * n) - 1))
        return sorted_samples[rank]

    return {
        "p50": rank_val(0.50),
        "p95": rank_val(0.95),
        "p99": rank_val(0.99),
    }


def validate_quantiles(obj: Dict[str, object], context_name: str) -> Tuple[bool, str]:
    """Validate presence, finiteness, non-negativity, and monotonic ordering of p50, p95, p99."""
    for q_key in ("p50_us", "p95_us", "p99_us"):
        if q_key not in obj:
            return False, f"{context_name} missing mandatory quantile key '{q_key}'"
        val = obj.get(q_key)
        try:
            f_val = float(val)  # type: ignore[arg-type]
        except (ValueError, TypeError):
            return False, f"{context_name} quantile '{q_key}' is not a valid float: {val}"
        if math.isnan(f_val) or math.isinf(f_val):
            return False, f"{context_name} quantile '{q_key}' is non-finite: {f_val}"
        if f_val < 0.0:
            return False, f"{context_name} quantile '{q_key}' is negative: {f_val}"

    p50 = float(obj["p50_us"])  # type: ignore[arg-type]
    p95 = float(obj["p95_us"])  # type: ignore[arg-type]
    p99 = float(obj["p99_us"])  # type: ignore[arg-type]

    if p50 > p95:
        return False, f"{context_name} quantiles inverted: p50_us ({p50}) > p95_us ({p95})"
    if p95 > p99:
        return False, f"{context_name} quantiles inverted: p95_us ({p95}) > p99_us ({p99})"

    return True, ""


def validate_native_report(
    report_dict: Dict[str, object],
    expected_mode: str,
    expected_topology: str,
    expected_elements: int,
    expected_duration_s: float,
    expected_warmup_s: float,
    expected_validation_mode: bool = False,
    expected_iterations: int = 0,
) -> Tuple[bool, str]:
    """Validate report strictly against EXP-009 schema_version:1 contract.

    Rejects malformed JSON, invalid schema versions, missing fields, zero progress,
    count vs latency population mismatches, invalid or inverted window timestamps,
    non-finite/inverted quantiles, role mismatches, and serial pair inconsistencies.
    Handles 'per_iteration' finite validation runs separately from 'final_tag_only' timed runs.
    """
    if not isinstance(report_dict, dict):
        return False, "Report is not a JSON object"

    if report_dict.get("schema_version") != 1:
        return False, f"Expected schema_version 1, got {report_dict.get('schema_version')}"

    if report_dict.get("valid") is not True:
        return False, f"Report indicates valid=False: {report_dict.get('error', 'unknown error')}"

    mode = report_dict.get("mode")
    topology = report_dict.get("topology")
    if mode != expected_mode:
        return False, f"Report mode '{mode}' does not match expected '{expected_mode}'"
    if topology != expected_topology:
        return False, f"Report topology '{topology}' does not match expected '{expected_topology}'"

    validation_strength = report_dict.get("validation_strength")
    if validation_strength not in ("per_iteration", "final_tag_only"):
        return False, f"Invalid validation_strength: {validation_strength}"

    is_per_iteration = (validation_strength == "per_iteration")
    if expected_validation_mode and not is_per_iteration:
        return False, f"Expected validation_strength 'per_iteration' for validation run, got {validation_strength}"
    if not expected_validation_mode and is_per_iteration:
        return False, f"Timed run cannot have validation_strength 'per_iteration'"

    config = report_dict.get("config")
    if not isinstance(config, dict):
        return False, "Report missing 'config' dictionary"

    required_config_keys = [
        "elements", "intensity", "width", "height",
        "duration_seconds", "warmup_seconds", "iterations",
    ]
    for c_key in required_config_keys:
        if c_key not in config:
            return False, f"Config missing required key '{c_key}'"

    if config.get("elements") != expected_elements:
        return False, f"Config elements {config.get('elements')} does not match expected {expected_elements}"

    # Verify duration_seconds and warmup_seconds in config match requested CLI parameters
    try:
        cfg_dur = float(config["duration_seconds"])  # type: ignore[arg-type]
        if abs(cfg_dur - expected_duration_s) > 1e-3:
            return False, f"Config duration_seconds ({cfg_dur}) does not match requested ({expected_duration_s})"
    except (ValueError, TypeError) as e:
        return False, f"Config duration_seconds invalid: {e}"

    try:
        cfg_warmup = float(config["warmup_seconds"])  # type: ignore[arg-type]
        if abs(cfg_warmup - expected_warmup_s) > 1e-3:
            return False, f"Config warmup_seconds ({cfg_warmup}) does not match requested ({expected_warmup_s})"
    except (ValueError, TypeError) as e:
        return False, f"Config warmup_seconds invalid: {e}"

    if is_per_iteration and expected_iterations > 0:
        cfg_iter = int(config.get("iterations", 0))
        if cfg_iter != expected_iterations:
            return False, f"Config iterations ({cfg_iter}) does not match expected ({expected_iterations})"

    for key in ("clock", "start_ns", "deadline_ns", "observed_end_ns", "workers"):
        if key not in report_dict:
            return False, f"Report missing mandatory key '{key}'"

    if report_dict.get("clock") != "CLOCK_MONOTONIC":
        return False, f"Expected clock CLOCK_MONOTONIC, got {report_dict.get('clock')}"

    # Window timestamp consistency checks
    try:
        start_ns = int(report_dict["start_ns"])  # type: ignore[arg-type]
        deadline_ns = int(report_dict["deadline_ns"])  # type: ignore[arg-type]
        observed_end_ns = int(report_dict["observed_end_ns"])  # type: ignore[arg-type]
    except (ValueError, TypeError) as e:
        return False, f"Invalid timestamp type in report: {e}"

    if start_ns >= deadline_ns:
        return False, f"Inverted window: start_ns ({start_ns}) >= deadline_ns ({deadline_ns})"

    # In timed runs, observed_end_ns must not end before deadline_ns.
    # In per_iteration validation runs, execution finishes as soon as N iterations validate,
    # so observed_end_ns can finish before deadline_ns.
    if not is_per_iteration and observed_end_ns < deadline_ns:
        return False, f"Window premature end: observed_end_ns ({observed_end_ns}) < deadline_ns ({deadline_ns})"

    workers = report_dict.get("workers")
    if not isinstance(workers, list) or len(workers) == 0:
        return False, "Report 'workers' must be a non-empty list"

    expected_roles: List[str] = []
    if expected_mode == "solo-render":
        expected_roles = ["render"]
    elif expected_mode == "solo-compute":
        expected_roles = ["compute"]
    elif expected_mode in ("serial", "concurrent"):
        expected_roles = ["render", "compute"]

    observed_roles = [w.get("role") for w in workers if isinstance(w, dict)]
    if sorted(observed_roles) != sorted(expected_roles):
        return False, f"Observed worker roles {observed_roles} do not match exact expected roles {expected_roles}"

    worker_completed_map: Dict[str, int] = {}
    required_worker_keys = [
        "role", "pid", "tid", "cpu_affinity", "ready_ns",
        "warmup_start_ns", "warmup_end_ns", "warmup_completed",
        "first_submit_ns", "last_submit_ns", "last_completion_ns",
        "completed", "late", "latency_samples", "p50_us", "p95_us", "p99_us",
        "throughput_ops_sec", "late_latencies_us", "start_skew_ns",
        "drain_ns", "validation_ns", "final_tag", "validation_passed",
    ]

    for w in workers:
        if not isinstance(w, dict):
            return False, "Worker entry is not an object"

        role = str(w.get("role", "unknown"))
        for w_key in required_worker_keys:
            if w_key not in w:
                return False, f"Worker '{role}' missing required key '{w_key}'"

        if w.get("validation_passed") is not True:
            return False, f"Worker '{role}' validation_passed is not True"

        try:
            completed = int(w["completed"])  # type: ignore[arg-type]
            latency_samples = int(w["latency_samples"])  # type: ignore[arg-type]
            tp = float(w["throughput_ops_sec"])  # type: ignore[arg-type]
        except (ValueError, TypeError) as e:
            return False, f"Worker '{role}' invalid metric types: {e}"

        if math.isnan(tp) or math.isinf(tp) or tp < 0.0:
            return False, f"Worker '{role}' invalid throughput: {tp}"

        # Zero-progress / starvation rejection
        if completed <= 0:
            return False, f"Worker '{role}' completed {completed} operations: zero progress / starvation rejected"

        if is_per_iteration and expected_iterations > 0 and completed != expected_iterations:
            return False, f"Worker '{role}' completed ({completed}) != expected iterations ({expected_iterations})"

        # Count population consistency: completed in-window ops must match latency_samples
        if latency_samples != completed:
            return False, f"Worker '{role}' count mismatch: completed ({completed}) != latency_samples ({latency_samples})"

        # Quantile validation: finite, non-negative, ordered
        q_valid, q_err = validate_quantiles(w, f"Worker '{role}'")
        if not q_valid:
            return False, q_err

        worker_completed_map[role] = completed

    if expected_mode == "serial":
        pairs = report_dict.get("pairs")
        if not isinstance(pairs, dict):
            return False, "Serial mode requires non-null 'pairs' dictionary"
        for p_key in ("completed", "late", "latency_samples", "p50_us", "p95_us", "p99_us"):
            if p_key not in pairs:
                return False, f"Serial 'pairs' missing key '{p_key}'"

        try:
            pairs_completed = int(pairs["completed"])
            pairs_latency_samples = int(pairs["latency_samples"])
        except (ValueError, TypeError) as e:
            return False, f"Serial 'pairs' invalid count types: {e}"

        if pairs_completed <= 0:
            return False, f"Serial pairs completed {pairs_completed}: zero progress rejected"

        if is_per_iteration and expected_iterations > 0 and pairs_completed != expected_iterations:
            return False, f"Serial pairs completed ({pairs_completed}) != expected iterations ({expected_iterations})"

        if pairs_latency_samples != pairs_completed:
            return False, f"Serial pairs count mismatch: completed ({pairs_completed}) != latency_samples ({pairs_latency_samples})"

        min_worker_completed = min(worker_completed_map.values()) if worker_completed_map else 0
        if pairs_completed > min_worker_completed:
            return False, f"Serial pairs completed ({pairs_completed}) exceeds worker completed ({min_worker_completed})"

        pq_valid, pq_err = validate_quantiles(pairs, "Serial pairs")
        if not pq_valid:
            return False, pq_err
    else:
        if report_dict.get("pairs") is not None:
            return False, f"Non-serial mode '{expected_mode}' must have null pairs"

    return True, ""


def kill_process_group(pgid: int) -> None:
    """Send SIGTERM then SIGKILL to entire process group."""
    try:
        os.killpg(pgid, signal.SIGTERM)
    except (ProcessLookupError, PermissionError, OSError) as e:
        logger.debug("killpg SIGTERM (%d) error: %s", pgid, e)

    time.sleep(0.05)

    try:
        os.killpg(pgid, signal.SIGKILL)
    except (ProcessLookupError, PermissionError, OSError) as e:
        logger.debug("killpg SIGKILL (%d) error: %s", pgid, e)


def execute_probe_cell(
    binary_path: Path,
    output_dir: Path,
    condition: str,
    elements: int,
    repetition: int,
    block_index: int,
    duration_s: float,
    warmup_s: float,
    timeout_s: float,
    validation_mode: bool,
    iterations: int,
    render_cpu: int,
    compute_cpu: int,
) -> Dict[str, object]:
    """Execute single native probe cell with process group isolation, timeout, and guaranteed cleanup."""
    mode, topology = condition.split(":")
    cell_prefix = f"cell_{condition.replace(':', '_')}_elem{elements}_rep{repetition}_idx{block_index}"

    report_path = output_dir / f"{cell_prefix}_report.json"
    stdout_path = output_dir / f"{cell_prefix}_stdout.log"
    stderr_path = output_dir / f"{cell_prefix}_stderr.log"

    cmd: List[str] = [
        str(binary_path),
        "--mode", mode,
        "--topology", topology,
        "--elements", str(elements),
        "--duration-seconds", str(duration_s),
        "--warmup-seconds", str(warmup_s),
        "--report-json", str(report_path),
        "--render-cpu", str(render_cpu),
        "--compute-cpu", str(compute_cpu),
    ]

    if validation_mode:
        cmd.extend(["--validation", "--iterations", str(iterations if iterations > 0 else 8)])
    elif iterations > 0:
        cmd.extend(["--iterations", str(iterations)])

    start_time_utc = datetime.datetime.now(datetime.timezone.utc).isoformat()
    start_mono = time.monotonic()
    timed_out = False
    returncode: Optional[int] = None
    proc_pgid: Optional[int] = None

    with open(stdout_path, "w") as out_f, open(stderr_path, "w") as err_f:
        proc: Optional[subprocess.Popen[str]] = None
        try:
            proc = subprocess.Popen(
                cmd,
                stdout=out_f,
                stderr=err_f,
                start_new_session=True,
                text=True,
            )
            try:
                proc_pgid = os.getpgid(proc.pid)
            except (ProcessLookupError, PermissionError, OSError):
                proc_pgid = proc.pid

            returncode = proc.wait(timeout=timeout_s)
        except subprocess.TimeoutExpired:
            timed_out = True
            returncode = -1
        except OSError as e:
            logger.error("Subprocess launch error: %s", e)
            returncode = -1
        finally:
            if proc_pgid is not None:
                kill_process_group(proc_pgid)
            if proc is not None:
                try:
                    proc.kill()
                    proc.wait()
                except (ProcessLookupError, PermissionError, OSError) as e:
                    logger.debug("Failed cleaning proc %s: %s", proc.pid, e)

    elapsed_s = time.monotonic() - start_mono
    end_time_utc = datetime.datetime.now(datetime.timezone.utc).isoformat()

    receipt: Dict[str, object] = {
        "condition": condition,
        "mode": mode,
        "topology": topology,
        "elements": elements,
        "repetition": repetition,
        "block_index": block_index,
        "render_cpu": render_cpu,
        "compute_cpu": compute_cpu,
        "command": cmd,
        "start_time_utc": start_time_utc,
        "end_time_utc": end_time_utc,
        "elapsed_seconds": elapsed_s,
        "returncode": returncode,
        "timed_out": timed_out,
        "report_file": str(report_path.name),
        "stdout_file": str(stdout_path.name),
        "stderr_file": str(stderr_path.name),
    }

    if timed_out:
        receipt["status"] = "failed"
        receipt["error"] = f"Execution timed out after {timeout_s}s"
        return receipt

    if returncode != 0:
        receipt["status"] = "failed"
        receipt["error"] = f"Native binary exited with returncode {returncode}"
        return receipt

    if not report_path.exists():
        receipt["status"] = "failed"
        receipt["error"] = f"Report file {report_path.name} was not created"
        return receipt

    try:
        report_data = json.loads(report_path.read_text())
    except (json.JSONDecodeError, OSError) as e:
        receipt["status"] = "failed"
        receipt["error"] = f"Failed to parse report JSON: {e}"
        return receipt

    valid, err_msg = validate_native_report(
        report_data,
        mode,
        topology,
        elements,
        expected_duration_s=duration_s,
        expected_warmup_s=warmup_s,
        expected_validation_mode=validation_mode,
        expected_iterations=(iterations if iterations > 0 else 8) if validation_mode else iterations,
    )
    if not valid:
        receipt["status"] = "failed"
        receipt["error"] = f"Schema validation error: {err_msg}"
        return receipt

    receipt["status"] = "success"
    receipt["report"] = report_data
    return receipt


def compute_matrix_aggregates(results: List[Dict[str, object]], is_validation_run: bool = False) -> Dict[str, object]:
    """Aggregate results across repetitions.

    For timed runs (is_validation_run=False), aggregates per-worker throughput and quantiles.
    For validation runs (is_validation_run=True), summarizes per-iteration correctness checks
    without pooling into performance throughput.
    """
    grouped: Dict[str, List[Dict[str, object]]] = {}
    for res in results:
        cond_key = f"{res['condition']}:{res['elements']}"
        grouped.setdefault(cond_key, []).append(res)

    conditions_summary: Dict[str, object] = {}

    for cond_key, cell_list in grouped.items():
        rep_count = len(cell_list)

        if is_validation_run:
            all_passed = True
            iterations_verified: List[int] = []
            for cell in cell_list:
                rep_dict = cell.get("report")
                if not isinstance(rep_dict, dict):
                    all_passed = False
                    continue
                workers = rep_dict.get("workers", [])
                if isinstance(workers, list):
                    for w in workers:
                        if isinstance(w, dict):
                            if w.get("validation_passed") is not True:
                                all_passed = False
                            iterations_verified.append(int(w.get("completed", 0)))

            conditions_summary[cond_key] = {
                "repetitions": rep_count,
                "validation_mode": True,
                "validation_passed": all_passed,
                "iterations_verified": min(iterations_verified) if iterations_verified else 0,
            }
            continue

        render_throughputs: List[float] = []
        compute_throughputs: List[float] = []
        render_p50: List[float] = []
        render_p95: List[float] = []
        render_p99: List[float] = []
        compute_p50: List[float] = []
        compute_p95: List[float] = []
        compute_p99: List[float] = []
        pair_counts: List[float] = []

        for cell in cell_list:
            rep_dict = cell.get("report")
            if not isinstance(rep_dict, dict):
                continue
            workers = rep_dict.get("workers", [])
            if isinstance(workers, list):
                for w in workers:
                    if not isinstance(w, dict):
                        continue
                    role = w.get("role")
                    tp = float(w.get("throughput_ops_sec", 0.0))
                    p50 = float(w.get("p50_us", 0.0))
                    p95 = float(w.get("p95_us", 0.0))
                    p99 = float(w.get("p99_us", 0.0))
                    if role == "render":
                        render_throughputs.append(tp)
                        render_p50.append(p50)
                        render_p95.append(p95)
                        render_p99.append(p99)
                    elif role == "compute":
                        compute_throughputs.append(tp)
                        compute_p50.append(p50)
                        compute_p95.append(p95)
                        compute_p99.append(p99)

            pairs = rep_dict.get("pairs")
            if isinstance(pairs, dict):
                pair_counts.append(float(pairs.get("completed", 0)))

        def stat_pack(vals: List[float]) -> Dict[str, float]:
            if not vals:
                return {"median": 0.0, "min": 0.0, "max": 0.0}
            return {
                "median": float(statistics.median(vals)),
                "min": float(min(vals)),
                "max": float(max(vals)),
            }

        workers_summary: Dict[str, object] = {}
        if render_throughputs:
            workers_summary["render"] = {
                "throughput_ops_sec": stat_pack(render_throughputs),
                "p50_us": stat_pack(render_p50),
                "p95_us": stat_pack(render_p95),
                "p99_us": stat_pack(render_p99),
            }
        if compute_throughputs:
            workers_summary["compute"] = {
                "throughput_ops_sec": stat_pack(compute_throughputs),
                "p50_us": stat_pack(compute_p50),
                "p95_us": stat_pack(compute_p95),
                "p99_us": stat_pack(compute_p99),
            }

        conditions_summary[cond_key] = {
            "repetitions": rep_count,
            "workers": workers_summary,
            "pairs_completed": stat_pack(pair_counts) if pair_counts else None,
        }

    return {"conditions": conditions_summary}


def load_identity_manifest(path_str: Optional[str]) -> Optional[Dict[str, object]]:
    """Read explicit physical identity manifest from EXP009_IDENTITY_FILE if available."""
    if not path_str:
        return None
    p = Path(path_str)
    if not p.exists():
        logger.warning("EXP009_IDENTITY_FILE specified but does not exist: %s", path_str)
        return None
    try:
        data = json.loads(p.read_text())
        if isinstance(data, dict):
            return data
    except (json.JSONDecodeError, OSError) as e:
        logger.warning("Failed reading identity manifest %s: %s", path_str, e)
    return None


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="EXP-009 controlled scheduling matrix orchestrator")
    parser.add_argument("--binary", required=True, type=Path, help="Path to native probe binary")
    parser.add_argument("--output-dir", required=True, type=Path, help="Output directory for reports and logs")
    parser.add_argument("--duration-seconds", type=float, default=15.0, help="Common measurement window duration")
    parser.add_argument("--warmup-seconds", type=float, default=5.0, help="Per-worker warmup duration")
    parser.add_argument("--repetitions", type=int, default=5, help="Number of repetitions per condition")
    parser.add_argument("--sizes", type=int, nargs="+", default=[1024, 262144], help="CUDA array element sizes")
    parser.add_argument("--seed", type=int, default=9, help="RNG seed for deterministic rotation schedule")
    parser.add_argument("--timeout-seconds", type=float, default=90.0, help="Per-cell execution timeout in seconds")
    parser.add_argument("--validation", action="store_true", help="Enable strict per-iteration validation")
    parser.add_argument("--iterations", type=int, default=0, help="Fixed iterations for validation or historical mode")
    parser.add_argument("--single-condition", type=str, default=None, help="Run single condition only, e.g. solo-render:threads")
    parser.add_argument("--render-cpu", type=int, default=None, help="Explicit CPU core affinity for render worker")
    parser.add_argument("--compute-cpu", type=int, default=None, help="Explicit CPU core affinity for compute worker")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    binary_path = args.binary.resolve()
    if not binary_path.exists():
        logger.error("Binary not found: %s", binary_path)
        return 1

    output_dir = args.output_dir.resolve()
    output_dir.mkdir(parents=True, exist_ok=True)

    render_cpu, compute_cpu = select_cpu_placement(args.render_cpu, args.compute_cpu)
    logger.info("Assigned CPU placement: render-cpu=%d, compute-cpu=%d", render_cpu, compute_cpu)

    conditions = [args.single_condition] if args.single_condition else ALL_CONDITIONS
    schedule = generate_condition_schedule(
        conditions=conditions,
        sizes=args.sizes,
        repetitions=args.repetitions,
        seed=args.seed,
    )

    identity_file = os.environ.get("EXP009_IDENTITY_FILE")
    identity_manifest = load_identity_manifest(identity_file)

    env_metadata: Dict[str, object] = {
        "slurm_job_id": os.environ.get("SLURM_JOB_ID"),
        "hostname": os.environ.get("HOSTNAME"),
        "exp009_identity_file": identity_file,
        "identity_manifest": identity_manifest,
        "allowed_cpus": get_allowed_cpus(),
        "assigned_render_cpu": render_cpu,
        "assigned_compute_cpu": compute_cpu,
    }

    results: List[Dict[str, object]] = []
    failures: List[Dict[str, object]] = []

    for item in schedule:
        cond = str(item["condition"])
        elems = int(item["elements"])
        rep = int(item["repetition"])
        b_idx = int(item["block_index"])

        cell_res = execute_probe_cell(
            binary_path=binary_path,
            output_dir=output_dir,
            condition=cond,
            elements=elems,
            repetition=rep,
            block_index=b_idx,
            duration_s=args.duration_seconds,
            warmup_s=args.warmup_seconds,
            timeout_s=args.timeout_seconds,
            validation_mode=args.validation,
            iterations=args.iterations,
            render_cpu=render_cpu,
            compute_cpu=compute_cpu,
        )

        if cell_res["status"] == "success":
            results.append(cell_res)
        else:
            failures.append({
                "condition": cond,
                "elements": elems,
                "repetition": rep,
                "block_index": b_idx,
                "reason": str(cell_res.get("error", "Unknown error")),
                "timed_out": bool(cell_res.get("timed_out", False)),
                "returncode": cell_res.get("returncode"),
                "receipt": cell_res,
            })

    matrix_status = "complete" if len(failures) == 0 else "failed"
    matrix_output: Dict[str, object] = {
        "status": matrix_status,
        "schema_version": 1,
        "environment": env_metadata,
        "config": {
            "conditions": conditions,
            "sizes": args.sizes,
            "repetitions": args.repetitions,
            "seed": args.seed,
            "duration_seconds": args.duration_seconds,
            "warmup_seconds": args.warmup_seconds,
            "timeout_seconds": args.timeout_seconds,
            "validation": args.validation,
            "iterations": args.iterations,
            "render_cpu": render_cpu,
            "compute_cpu": compute_cpu,
        },
        "total_cells": len(schedule),
        "successful_cells": len(results),
        "failed_cells": len(failures),
        "results": results,
        "failures": failures,
    }

    matrix_json_path = output_dir / "matrix.json"
    matrix_json_path.write_text(json.dumps(matrix_output, indent=2))

    if matrix_status == "complete":
        aggregate_data = compute_matrix_aggregates(results, is_validation_run=args.validation)
        aggregate_data["matrix_status"] = "complete"
        aggregate_data["schema_version"] = 1
        aggregate_data["environment"] = env_metadata
        agg_json_path = output_dir / "aggregate.json"
        agg_json_path.write_text(json.dumps(aggregate_data, indent=2))
        return 0
    else:
        logger.error("Matrix execution encountered %d failed cells.", len(failures))
        return 1


if __name__ == "__main__":
    sys.exit(main())
