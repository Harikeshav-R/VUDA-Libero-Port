# EXP-008: Final-output rejection and independent concurrent latency

## Question and scope

Can the existing native probe reject bad final outputs in every execution mode and expose CUDA latency independently of rendering?

OSC Ascend remains a **temporary preliminary testbed**. **EPFL is the permanent host lab**. This experiment addresses a Gate 4 measurement prerequisite, not a VUDA implementation, LIBERO integration, or proof of spatial sharing.

## Method and source changes

The original probe is frozen separately from the candidate. Both use native EGL/OpenGL rendering, disjoint deterministic CUDA data, host pixel readback, `glFinish()`, and per-iteration CUDA completion waits. No scheduler, queue-depth change, image-consumption path, model, or checkpoint is introduced.

The candidate adds:

- Fatal final-output checks for solo rendering, solo compute, and both outputs after serial and concurrent runs. Checks occur outside timing; failed checks prevent the benchmark report.
- Checked CUDA launch/completion and stream/device operations, checked EGL context transfer, and worker failure propagation without readiness deadlock. The render worker explicitly binds `EGL_OPENGL_API` before taking and releasing its context.
- Separate concurrent EGL and CUDA host submit-to-completion latency distributions, including p50/p95/p99. Both throughput rows use the same whole concurrent-phase wall envelope. Serial latency remains a render/compute pair latency.
- An explicit inconclusive scheduling statement instead of throughput-threshold spatial-sharing or TSG labels.

A test-only pass-through library forwards actual CUDA/GL calls and then corrupts one selected host readback, or returns a controlled worker-initialization failure. Six output scenarios cover solo GL/CUDA, serial GL/CUDA, and concurrent GL/CUDA. Two more cover CUDA stream creation and EGL worker context binding. These are controlled application-boundary failures, not observed hardware faults or internal driver allocation interception.

Rejection scenarios use `5` measured iterations, warmup `2`, `1024` elements, intensity `100`, and an eight-second process timeout. The longer configuration below applies to normal runs, not fault scenarios.

The finite batch requests one GPU, four CPUs, 16 GiB, and five minutes under the previously user-confirmed research project. It builds the original and candidate, runs the rejection scenarios, and requests five unprofiled repetitions per CUDA array size (`1024` and `262144` elements). Each repetition uses `500` measured iterations, intensity `100`, and `128×128 RGBA8`. Solo/serial paths use `50` warmup iterations; concurrent workers still lack independent warmup.

The existing pre-context CUDA/EGL UUID guard and NVML UUID/PCI correlation remain mandatory. Source snapshots, digests, build commands, physical identifiers, allocation records, binaries, and raw results remain private under `evidence/osc/ascend/exp008-correctness-latency/`.

## Execution record

The initial fifteen-minute no-shell allocation request `8390936` remained pending with scheduler reason `Priority`. The local SSH command timed out after 300 seconds without an observed grant. A subsequent scheduler query confirmed it remained pending; the session cancelled that exact job before submitting a finite batch. No GPU execution is attributed to that request.

Batch `8391087` received one A100 GPU, five CPUs, and 16 GiB, then failed after four seconds with exit `1:0`. Slurm had copied the entrypoint into its spool directory, so `dirname("$0")` selected that directory rather than the experiment workspace. Opening `baseline-step.log` failed with `Permission denied` before baseline inventory, compilation, or application execution. This was a launcher-path error, not a GPU permission or scheduling result.

The repair passes an explicit experiment root through the batch environment and requires it in the entrypoint. A local copied-script smoke failed before repair (exit `127`, children sought in the wrong directory) and passed afterward (exit `0`, both child logs written to the explicit workspace). This smoke used sentinel child scripts, not GPU workloads. The failed remote namespace remains unchanged; the repair uses a separate namespace and the same native source snapshots.

**Repaired batch `8391277` completed with exit `0:0` in 42 seconds.** Its application log spans UTC November 8, `01:19:15–01:19:53`, corresponding to November 7 locally. Actual grant: one NVIDIA A100-PCIE-40GB, five CPUs, 16 GiB; driver `580.95.05`, toolkit `12.8.1` / `nvcc V12.8.93`, `g++ 11.5.0`. Baseline and candidate CUDA/EGL UUIDs agreed, and the selected NVML UUID/PCI correlation succeeded. No new profiler identity capture is claimed.

Both original and candidate compiled with the existing Makefile (`-O3`, `sm_80`). The pre-existing unused `chosen_index` warning remained in both builds; no warning was suppressed. Source review reported no material findings. All nine candidate source/build snapshots match the public working tree. Remote/retrieved hashes matched all 30 failed-attempt files and 95 repaired-attempt files, including executed binaries.

## Observed rejection and normal-run results

All eight repaired rejection cases exited exactly `1`, without a timeout or benchmark report. An independent log audit checked that earlier mode checks completed and that each failure occurred at its intended phase.

| Cases | Original result | Repaired result |
| --- | --- | --- |
| Solo GL/CUDA output corruption (2) | Printed warnings and returned `0` | Rejected with exit `1` |
| Serial/concurrent GL/CUDA output scenarios (4) | Returned `0`; requested corruptions were never reached because final readbacks were absent | Readbacks reached; corruptions rejected with exit `1` |
| CUDA worker stream-creation failure (1) | Ignored error and returned `0` | Rejected with exit `1` |
| EGL worker context-bind failure (1) | Hung until the test's eight-second timeout | Rejected with exit `1`, without timeout |

Thus the original passed **0/8 rejection assertions**, not eight successfully injected failures. The repaired probe passed **8/8**, with all injections reached. Ten normal invocations then completed: five per array size. Every normal run passed all six final-output checks and emitted both concurrent latency rows with finite ordered percentiles.

### Fixed-count observations

Values below are medians across five repetitions. Ranges include every repetition. Percentile columns are medians of per-run percentiles, not pooled percentiles. Concurrent EGL/CUDA wall intervals are intentionally identical; their individual latency vectors are not interchangeable.

| Elements | Mode | Wall ms: median [range] | p50 μs | p95 μs | p99 μs |
| --- | --- | --- | --- | --- | --- |
| 1024 | Solo EGL | 12.82 [12.76–19.27] | 20.0 | 21.0 | 97.8 |
| 1024 | Solo CUDA | 5.39 [5.38–5.75] | 10.7 | 11.1 | 11.5 |
| 1024 | Serial pair | 169.70 [169.49–183.01] | 333.7 | 335.6 | 342.7 |
| 1024 | Concurrent EGL | 149.46 [65.40–150.30] | 322.6 | 323.6 | 324.3 |
| 1024 | Concurrent CUDA | 149.46 [65.40–150.30] | 322.6 | 323.7 | 324.6 |
| 262144 | Solo EGL | 12.67 [12.62–24.31] | 19.9 | 21.0 | 90.5 |
| 262144 | Solo CUDA | 10.66 [10.64–10.70] | 21.2 | 21.6 | 22.7 |
| 262144 | Serial pair | 175.02 [174.87–177.82] | 344.2 | 346.3 | 352.3 |
| 262144 | Concurrent EGL | 165.76 [165.39–177.70] | 333.2 | 334.1 | 334.7 |
| 262144 | Concurrent CUDA | 165.76 [165.39–177.70] | 333.2 | 334.1 | 336.9 |

The reduced-grid concurrent wall range is broad; no steady-state or causal scheduling explanation follows. The [sanitized machine-readable audit](exp008-correctness-latency-audit.json) includes all control rows, throughput, individual regression outcomes, source/binary digests, and configuration.

## Interpretation limits and next decision

Final validation retains two GL pixels and seven CUDA sample indices, including `1000`; tested element counts must exceed `1000`. It does not verify the full CUDA output buffer or every iteration's freshness. Constant expected outputs may survive skipped work. Checked API failures do not change that boundary.

Host latency includes submission and completion waits, not just active GPU execution. Concurrent throughputs share the joint wall interval; they are not independent worker-active throughputs. Added error checks change host instrumentation, so comparisons with EXP-007 are not isolated scheduling interventions.

Five fixed-count repetitions are not equal-duration steady-state controls. Independent concurrent warmup, matched process-topology controls, representative MuJoCo shader work, resource headroom, and authoritative active-execution attribution remain open. No new Nsight profile, RM TSG mapping, channel redirection, SCG command, privileged operation, service change, or support message is part of EXP-008.

**Gate 4 remains partial/inconclusive on active spatial sharing.** The next measurement step remains matched equal-duration and process controls, not another capability-only profiling smoke. Gate 5's author-artifact/control-mapping and site-approval blockers remain separate.

## Primary sources

- [CUDA Runtime API 12.8.1: Error Handling](https://docs.nvidia.com/cuda/archive/12.8.1/cuda-runtime-api/group__CUDART__ERROR.html): `cudaGetLastError()` returns and resets the host thread's runtime error and can report previous asynchronous errors.
- [Khronos EGL: eglMakeCurrent](https://registry.khronos.org/EGL/sdk/docs/man/html/eglMakeCurrent.xhtml): contexts bind to a calling thread; release with `EGL_NO_CONTEXT` respects its currently bound client API.
- [Gate 4 protocol](../../research/GATE4_CAPTURE_PROTOCOL.md): correctness, matched-control, and attribution requirements remain independent.
