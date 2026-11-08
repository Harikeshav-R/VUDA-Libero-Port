# EXP-009 Controlled Scheduling Experiment Implementation Plan

> **For agentic workers:** Use the repository's executing-plans or subagent-driven-development workflow to implement the tasks below. Checkboxes describe future work, not completed verification.

**Goal:** Produce reproducible, correctness-checked measurements of native EGL/OpenGL and disjoint CUDA interference under matched serial/concurrent and thread/process controls, then state what supported telemetry can and cannot establish about scheduling.

**Architecture:** Extend the existing native probe rather than building a replacement renderer or scheduler. Use one measurement-window contract for in-process and independently executed process workers. A finite orchestration script records repetitions, configuration, raw outputs, failures, and aggregates; profiling is a separate run.

**Tech stack:** Existing C++17/CUDA/EGL/OpenGL probe, Python standard-library orchestration, Slurm, and the site-provided Nsight Systems when profiling.

**Spec:** [Gate 4 capture protocol](GATE4_CAPTURE_PROTOCOL.md), with the concrete EXP-009 requirements below. This file is the authoritative next-experiment contract; other living docs should link here rather than duplicate its detailed matrix.

## Status and evidence boundary

**Completed.** The experiment was implemented, executed on OSC Ascend (job `8401856`), and verified locally. All 60 matrix cells and 12 profiling captures succeeded. See the [EXP-009 public report](../evidence/public/exp009-controlled-scheduling.md) and [machine audit](../evidence/public/exp009-controlled-scheduling-audit.json).
The permanent target is **EPFL**. **OSC Ascend is a temporary preliminary testbed**. Use the Ascend SSH endpoint supplied by the user; keep usernames, charge codes, node names, physical identifiers, and private paths out of public documents.

### What EXP-008 actually established

- GPU allocation succeeded: job `8391277`, one A100-PCIE-40GB, five CPUs, 16 GiB, driver `580.95.05`, toolkit `12.8.1`, `g++ 11.5.0`; exit `0:0` in 42 seconds.
- Original and updated native sources compiled. The updated probe passed eight controlled failure-rejection scenarios and ten normal invocations. Four original serial/concurrent fault injections were not reached because final readbacks were absent; this is not eight observed hardware failures.
- Final checks now reject failed runs; worker initialization failures propagate without the original readiness hang. Concurrent CUDA latency is reported independently of rendering latency.
- The measurements remain fixed-count, same-process, static-output checks. The concurrent workers lack independent warmup. Two GL pixels and seven CUDA indices are not full-output or per-iteration freshness proof.
- A failed earlier batch selected the Slurm spool directory through `dirname "$0"`. An explicitly exported experiment root fixed it. Preserve the failed attempt; do not repeat that path assumption.
- Spatial sharing, RM TSG identity, a VUDA port, CUDA consumption of rendered pixels, and real policy inference/training remain unproved or unimplemented.

Read [EXP-008 findings](../evidence/public/exp008-correctness-latency.md) and its [audit](../evidence/public/exp008-correctness-latency-audit.json) for observations. Preserve those files and historical raw captures rather than updating them with EXP-009 results.

## Global constraints

- Use only an allocated compute-node GPU. Recheck grant, hostname, visibility, actual CPU affinity, software, and CUDA/EGL/NVML physical UUID/PCI correspondence before rendering. An NVML inventory can contain unallocated GPUs.
- Recover the previously user-authorized research charge project from private allocation provenance. Do not infer permission from a course login or pick an arbitrary scheduler association. If the private record is unavailable, resolve that prerequisite before submission.
- Keep native EGL/OpenGL and disjoint CUDA buffers. Keep `glFinish()` and per-stream CUDA completion waits as the completion-limited control. Queue-depth experiments are separate.
- Retain failure rejection, explicit render-worker OpenGL API binding, safe context transfer, checked CUDA statuses, and bounded error propagation from EXP-008.
- Use supported APIs only. Internal allocation observation, channel mutation, shadow channels, undocumented SCG controls, driver/service changes, MPS/MIG changes, and privilege escalation are outside EXP-009.
- Keep timing runs separate from validation runs and profiler runs. Document every instrumentation difference.
- Public data must be sanitized derivatives. Preserve commands, source/fork/commit plus effective file digests, binary digests, configuration, inputs, allocation identity, UTC capture times, failures, and raw outputs privately.
- A successful controlled experiment may still leave Gate 4 inconclusive. Never force a positive spatial-sharing or negative TSG verdict to satisfy completion.

## File and interface map

Read the current implementation before editing; line numbers are orientation, not patch anchors.

| Existing file | Responsibility in this work |
| --- | --- |
| `src/probe/main.cpp:7-57` | Explicit mode/time/validation/report selection and nonzero failure exit |
| `src/probe/benchmark_runner.h:10-63` | Configuration, per-worker result contract, mode entry points |
| `src/probe/benchmark_runner.cpp` | Warmup, common measurement window, serial/concurrent execution, failure coordination, latency accounting |
| `src/probe/egl_renderer.h`, `src/probe/egl_renderer.cpp` | Native rendering and an observable iteration-dependent validation tag without replacing the renderer |
| `src/probe/cuda_compute.h`, `src/probe/cuda_compute.cu` | Disjoint compute, iteration-dependent validation output, checked completion and numerical validation |
| `src/probe/Makefile`, `src/probe/CMakeLists.txt` | Register any actual added sources/tests; keep both build descriptions consistent |

Proposed new files, to create only during implementation:

- `scripts/run-controlled-scheduling.py`: finite standard-library orchestrator, independent process startup, repetition ordering, timeout/cleanup, and aggregation. Factor native measurement helpers only where shared thread/process semantics require them; do not introduce a scheduler abstraction.
- `src/probe/tests/test_measurement_window.cpp`: deterministic boundary/error tests for actual native window-accounting code, without requiring a GPU for clock arithmetic.
- `scripts/tests/test_controlled_scheduling.py`: process failure/timeout and aggregation behavior tests. Test real subprocess behavior, not mocked output echoes.
- `evidence/public/exp009-controlled-scheduling.md` and a JSON audit: create after real results exist; include failures and inconclusive outcomes. They are not present evidence now.

The current binary has no mode or duration flag. A conservative proposed CLI is `--mode solo-render|solo-compute|serial|concurrent`, `--topology threads|processes`, `--duration-seconds`, `--warmup-seconds`, `--validation`, and `--report-json`. Confirm the smallest coherent interface against the source before implementing it. Do not run these options as if they already exist. Preserve the existing fixed-count path only as an explicitly named historical/control path, not a silent fallback for failed timed execution.

## Experimental matrix

Use the same renderer, CUDA arithmetic, compiler settings, input configuration, physical GPU, and CPU grant within a comparison block. Initial controls retain `128×128 RGBA8`, intensity `100`, and CUDA sizes `1024` and `262144`. Those grid sizes are not measured headroom.

| Condition | Workers and ordering | Purpose |
| --- | --- | --- |
| Solo GL | One render worker using its assigned CPU placement | Render baseline |
| Solo CUDA | One compute worker using its assigned CPU placement | Compute baseline |
| Serial, threads | Two same-process workers; explicit render-complete then compute-complete sequencing | Matched thread-topology serial control |
| Concurrent, threads | Same two workers, common start/window, no per-iteration cross-worker barrier | Same-process independent submission |
| Serial, processes | Separate independently initialized render/compute processes; explicit completion sequencing | Matched process-topology serial control |
| Concurrent, processes | Same separate process roles, common start/window, no per-iteration cross-worker barrier | Cross-process independent submission |

The old single-thread serial loop can remain a labeled reference but does not substitute for the matched two-worker serial control. Thread and process serial coordination have different transport costs; record these and do not attribute their difference uniquely to GPU scheduling. Solo CPU placement must match its corresponding co-running worker.

**Proposed finite settings:** each active worker receives at least five seconds of warmup; each measurement window lasts fifteen seconds; five repetitions per condition and size. These are experimental defaults, not measured facts. Rotate condition order deterministically across repetition blocks and record the order/seed. Keep all conditions of a block on the same allocated physical GPU where possible. If allocations differ, stratify results by allocation and identity rather than treating them as interchangeable repetitions.

Six conditions × two sizes × five repetitions means sixty measurement windows. Calculate requested walltime from actual warmup, measurement, build, validation, and drain work with margin. The earlier five-minute smoke allocation is not a suitable total-runtime assumption. Split finite batches only with explicit block/provenance accounting; never silently drop cells to fit a grant.

## Measurement and correctness contracts

### One shared window, explicit boundary accounting

1. Create each worker's own context/stream, perform warmup, and publish readiness. Worker failure must unblock peers and coordinator.
2. Publish one future common start and one deadline using a monotonic clock with the same verified time base across processes. Keep setup, warmup, readiness waiting, and final validation outside the measured interval.
3. A timed operation must start at or after the common start and before the deadline. Retain submit and observed completion times. Count throughput only for operations completed within that common window.
4. Use the half-open window `[start, deadline)` for both submission and completion eligibility. An operation completing exactly at or after the deadline is late/drained, even if submitted earlier. Drain it safely and exclude it from in-window throughput and its latency population. Record late-operation latency separately and label in-window quantiles as boundary-censored; do not present them as an uncensored distribution. Do not extend each worker's denominator independently.
5. Record requested and observed window boundaries, worker first/last progress, completed counts, late/drained counts, and start skew. A worker with zero progress invalidates a co-running comparison; do not hide starvation in an aggregate pair rate.
6. Report per-workload throughput over the common window and host submit-to-completion p50/p95/p99 for that same in-window population. Record sample count and percentile definition. Serial reports additionally expose pair count/latency; unequal partial final pairs must not become completed pairs.
7. Bound sample storage without silently dropping records or allocating unpredictably in the measured loop. Capacity exhaustion or a failed operation rejects the run. Record drain/validation overhead separately.

The process coordinator should spawn/exec workers before they initialize CUDA/EGL. Do not fork an already initialized GPU context. Use bounded readiness/start/error/stop messages, exact worker association, and verified exit status. Serial ordering must wait for actual API completion; concurrent mode must not inherit the serial per-iteration handshake.

### Freshness evidence without hidden serialization

Use separate validation and performance runs with the same configuration and ownership/completion rules:

- **Validation runs:** use a finite deterministic sequence, read and validate every produced iteration's identity and expected pixels/numerical output, and fail on stale, skipped, repeated, or mismatched output. Keep this path out of reported performance. Render and CUDA outputs remain disjoint; this does not require CUDA/GL image handoff.
- **Timed runs:** maintain iteration-dependent tags, completed counts, and final output checks outside the measured window. Report their weaker validation strength explicitly. A final tag alone does not prove every preceding output was fresh.
- Use an encoding that remains exact over the run's full iteration range. Do not silently wrap a color byte or rely on float precision for arbitrarily large integer IDs.
- Include a stale-output case where an earlier iteration is skipped but the last iteration is correct. Per-iteration validation must reject the earlier stale output even though a final-tag-only check could pass.
- Run all EXP-008-style output and worker-failure rejection scenarios after extending the probe. Check numerical non-finite values and invalid buffer/dimension bounds relevant to the new validation path; preserve actual API error context.

## Result record

Use machine-readable output with explicit units and schema version. At minimum retain:

- Condition, topology, repetition/block/order, full workload configuration, source/binary identity, allocation/physical identity, CPU placement, and instrumented/unprofiled status.
- Warmup readiness and shared monotonic start/deadline; actual first/last submit/completion and start skew for each worker.
- Per-worker in-window completed count, late/drained count, exact latency population count, p50/p95/p99 definition and values, common-window throughput, and validation strength/outcome.
- Serial pair count/latency, worker exits/errors, timeout classification, and invalid-run reason where applicable.

Failed runs remain in the evidence register. Aggregation must not turn a failed run into a zero-valued valid sample, drop outliers without explanation, or pool different GPU allocations/instrumentation levels silently. Report all five repetitions plus medians/ranges. Use unprofiled observations for performance conclusions.

## Implementation and execution tasks

### Task 1 — Timed modes and accounting

- [X] Read the existing source/build interfaces and EXP-008 failure paths. Define the concrete native configuration/result types and JSON schema before splitting implementation ownership.
- [X] Write failing behavior tests for the actual accounting helper: with window `[100, 200)`, submit/completion `(150, 190)` counts; `(190, 200)` and `(190, 210)` are late/drained; a submit at `200` is disallowed. Apply the same half-open convention in every topology and in the report.
- [X] Implement explicit modes, monotonic shared-window accounting, independent warmup/readiness, and separate per-worker serial/concurrent latency. Preserve the completion-limited workload.
- [X] Verify the nearest native build and boundary tests; exercise actual timed solo/serial/concurrent modes on an allocated GPU. Ensure a zero-progress peer and invalid duration produce failed results, not a report that looks valid.

### Task 2 — Matched process topology and finite orchestration

- [X] Implement the orchestrator and independently initialized process roles against Task 1's result contract. Assign comparable CPU placement and record actual affinity.
- [X] Add behavior tests using actual small subprocesses: failure before readiness, exit after readiness, timeout while sibling waits, malformed/incomplete result, and an unpaired serial completion. Each must terminate peers, preserve failure details, and reject aggregation.
- [X] Implement matched serial and independent concurrent coordination for both topologies; record IPC/start-skew costs rather than treating them as GPU costs.
- [X] Exercise the real two-process render/CUDA pair on the same allocated physical GPU. Confirm context/device selection independently in each worker and prove child failures cannot leave an unattended workload or allocation request.

### Task 3 — Freshness and rejection proof

- [X] Implement exact iteration identities plus the separate per-iteration validation path, using native GL host readback and CUDA numerical validation without introducing image interop.
- [X] Demonstrate stale/skipped/repeated output rejection with a later-correct final iteration; verify encoding bounds and finite numerical values. Reject worker/API failures and suppress usable timing reports.
- [X] Prove validation/initialization/readback work is excluded from timed populations. State precisely what timed final checks do not prove.
- [X] Re-exercise the original six output-corruption and two worker-initialization failure scenarios through the updated modes. Private EXP-008 test inputs can inform the regressions where available; do not overwrite preserved captures or assert only that a marker was printed.

### Task 4 — Run the complete unprofiled matrix

- [X] Freeze effective source, commands, configuration, binary, and helper digests in a new EXP-009 namespace. Reuse the explicit-root launcher pattern; refuse missing allocation or physical identity mismatch.
- [X] Request a finite allocation through the user-supplied Ascend endpoint using the authorized project recovered privately. Compile and run only on the compute node. Record actual grant/toolchain; do not infer them from job `8391277`.
- [X] Execute all sixty planned windows, or retain a failed/incomplete matrix with exact reasons and continue reachable cells. Mark the experiment incomplete if acceptance cells are missing; never rename a subset as the full experiment.
- [X] Audit per-worker progress, validation, deadline accounting, failures, all raw results, five-repetition aggregates, and identity consistency. Preserve broad timing variation instead of assigning a cause from throughput alone.

### Task 5 — Separate profiles and bounded scheduling interpretation

- [X] After the unprofiled controls are sound, profile finite matched representative runs for both loads and topologies, including solo/serial references. Record selected cases and any reduced capture duration as a separate instrumentation configuration.
- [X] Select the site-provided profiler independently of the application toolkit. The previous working combination was Nsight `2025.6.3.541` with toolkit `12.8.1`; recheck actual versions/help and request `--gres=nsight` for profiling. A100 lacks the documented Blackwell HES backend; do not seek a privileged workaround.
- [X] Capture only supported approved capabilities, preserve diagnostics, export to fresh paths, and correlate child/process/phase identities and CUDA/GL/scheduling records. Check exact-version option/engine/ID semantics before claiming attribution. Do not invent collection flags or convert restore/save brackets to active-warp intervals.
- [X] Record actual resource/headroom observations where supported. Reduced grids and device-aggregate activity alone do not prove graphics headroom or attributable active overlap. If graphics execution attribution remains unavailable, label that specific boundary inconclusive.
- [X] Publish sanitized findings/audit, update status/ledger/protocol/runbook where justified, preserve new raw evidence privately, and validate public plus separate private manifests. Obtain independent evidence/privacy review and run the relevant native/Python tests plus actual target smoke.

## Review focus and completion criteria

| Risk | Required proof |
| --- | --- |
| Deadline/drain inflates throughput | Boundary tests plus real timestamps distinguish in-window completion from post-window drain |
| Worker startup/failure fabricates concurrency | Shared start, both workers' progress, bounded failure exits, and real child-failure smoke |
| Constant or wrapped output passes as fresh | Exact sequence validation rejects an earlier stale iteration even when the final iteration is correct |
| Topology comparison hides CPU/IPC differences | Matched worker placement, thread/process serial controls, and recorded coordination/start-skew costs |
| Trace overlap is promoted to a causal verdict | Source-backed attribution limits and a distinct observed/inference/inconclusive classification |

The controlled scheduling experiment is complete only when the full declared matrix, validation/rejection proof, target execution, separate profile attempts/outcomes, raw preservation, and reviewed report exist. A denied/unavailable profiler capability is a recorded outcome, not permission to escalate. Scheduling may remain inconclusive despite a completed experiment. Author-source access, supported TSG/control-mapping observation, representative MuJoCo rendering, real model work, and EPFL migration remain separate later work.

## Human work that can proceed independently

- Follow up with the VUDA v2 authors for source/commit, reproduction environment, graphics control mappings, channel/subcontext redirection, initialization/teardown, and OpenGL guidance. Without an artifact or complete design, classify later work as independent reverse engineering.
- Obtain scope-specific site answers separately for own-process observation, channel/mapping mutation, and undocumented partition controls. Scheduler access does not settle those permissions. No support email is authorized for automatic sending by this plan.
- Obtain the mentor's runnable native renderer/application stack, actual policy/checkpoint, and inference/training schedule. Representative native MuJoCo rendering follows the synthetic control; policy evaluation follows only when those inputs exist.
- Inventory and review EPFL independently when access arrives. OSC compatibility and authorization do not transfer automatically.
