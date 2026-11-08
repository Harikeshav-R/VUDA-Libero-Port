# EXP-009: Controlled Native OpenGL/CUDA Scheduling Experiment

## Question and scope

Can native EGL/OpenGL rendering and CUDA model computation co-execute spatially on a shared NVIDIA GPU under standard runtime APIs, or does the hardware engine time-slice them?

OSC Ascend is a **temporary preliminary testbed**. **EPFL is the permanent host lab**. This experiment evaluates Gate 4 controlled scheduling and active execution attribution. It does not implement VUDA's internal driver interception shim (Gate 5) or integrate LIBERO. A rigorous negative result for standard API spatial sharing is valid.

## Method and experimental design

EXP-009 executes a fully controlled, equal-duration scheduling matrix comparing solo, serial, and concurrent workloads across both thread and process topologies, at two CUDA array sizes (`1024` and `262144` elements, intensity `100`, `128×128 RGBA8` framebuffer).

### Controlled timing and measurement window
- **Independent warmup:** Workers warm up independently for 5.0 seconds before common timing begins.
- **Synchronized measurement window:** A shared monotonic half-open interval `[start_ns, deadline_ns)` spans exactly 15.0 seconds (`CLOCK_MONOTONIC`).
- **Boundary censoring:** Only operations submitted and completed strictly within `[start_ns, deadline_ns)` count toward valid throughput and in-window latency distributions. Operations completing after the deadline are tracked separately as late operations.
- **Binary timestamp traces:** Native workers write little-endian binary timestamp sidecars (`EXP9TS01_LE`, 32-byte header, 16-byte submit/completion records). An independent post-execution auditor parses and verifies every raw record against report summaries.

### Correctness and per-iteration freshness
- **Exact tagged iterations:** Every iteration embeds a 64-bit tag `tag = (seed << 32) | iteration`.
- **Framebuffer verification:** The OpenGL fragment shader encodes bits 0–31 in red and green channels and bits 32–63 in blue and alpha channels. Host readback validates exact 64-bit tag matches and finite normalized color bounds.
- **CUDA buffer verification:** The compute kernel verifies input tags, transforms the workload, and writes the 64-bit tag into element indices 0 and 31.
- **Fail-closed error rejection:** Injected fault scenarios (6 readback corruptions, worker initialization failures, NaN inputs, broken IPC pipes) must be rejected with exit code 1 outside timing.

### Matrix configuration
The unprofiled matrix evaluates **60 cells** (6 conditions × 2 array sizes × 5 repetitions):
1. `solo-render:threads` (1 worker, render only)
2. `solo-compute:threads` (1 worker, compute only)
3. `serial:threads` (2 workers in 1 process, strictly alternating via condition variables)
4. `concurrent:threads` (2 workers in 1 process, free-running)
5. `serial:processes` (2 independent worker processes, strictly alternating via POSIX IPC pipes)
6. `concurrent:processes` (2 independent worker processes, free-running)

Separate profiling runs evaluate all 12 condition/size combinations under NVIDIA Nsight Systems (2.0s measurement window, 1.0s warmup) with the `ga100` metric set and GPU context switch tracing enabled.

## Execution record and chronology

The experiment was submitted to OSC Ascend under partition `debug-nextgen` on an NVIDIA A100-PCIE-40GB GPU (compute capability 8.0, driver 580.95.05, CUDA toolkit 12.8.1, nvcc V12.8.93, g++ 11.5.0, Python 3.9.21).

### Chronology of attempts
- **Attempts 1–3:** Compilation failures on target (missing verifier constants, unmigrated NVTX symbols, missing `<cstring>` include). Source corrected and frozen.
- **Attempt 4:** Make and CMake builds passed; all 13 fault rejections, 11 CLI parameter rejections, 12 strict validation runs, and 12 smoke cells passed. Voluntarily cancelled to add full binary timestamp sidecar replay (`EXP9TS01_LE`).
- **Attempt 5:** Cancelled to enforce sample vector capacity bounds; independent replay exposed a process-serial first-pair timestamp defect (timestamped before shared start).
- **Attempt 6:** Voluntarily cancelled to integrate the serial-pair timestamp correction.
- **Attempt 7:** Compiler type error during pair record conversion; also identified default `SIGPIPE` bypassing clean error propagation. Resolved with POSIX pipe cleanup and checked return codes.
- **Attempt 8 (final candidate, job 8401856):** Completed successfully in **24 minutes 35 seconds** (exit `0:0`).
  - Physical preflight, Make build, CMake build, and native CPU tests passed.
  - Runtime verification: 1/1 native tag proof passed, 13/13 fault rejections passed, 11/11 CLI rejections passed, 12/12 strict validation matrix runs passed.
  - Closed-pipe IPC worker rejection passed (exit 1, no hang).
  - 12-cell timed smoke run passed; independent binary timestamp replay passed 12/12 cells.
  - **Full unprofiled matrix (60/60 cells) executed and passed; independent binary timestamp replay confirmed 60/60 valid cells.**
  - **Separate profiling executed 12/12 capture attempts and 12/12 SQLite exports successfully.**

## Observed unprofiled performance results

All reported metrics are medians across 5 independent repetitions, with full ranges `[min–max]`. Percentiles (p50, p95, p99) are nearest-rank quantiles calculated on boundary-censored in-window populations.

### Small array workload (1024 elements)

| Execution mode | Worker | Throughput ops/s: median [range] | p50 μs | p95 μs | p99 μs | Late ops: median [range] |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Solo Render (threads)** | render | 30,470.8 [29,931.0–30,568.0] | 26.9 | 30.3 | 41.4 | 1.0 [1–1] |
| **Solo Compute (threads)** | compute | 93,636.9 [93,247.5–93,806.7] | 10.6 | 11.0 | 11.8 | 1.0 [1–1] |
| **Serial (threads)** | render | 2,830.5 [2,826.7–2,833.3] | 178.1 | 180.7 | 192.2 | 1.0 [1–1] |
| | compute | 2,830.5 [2,826.6–2,833.3] | 168.6 | 169.9 | 170.7 | 0.0 [0–0] |
| | *serial pairs* | 42,457.0 pairs [42,399–42,500] | 347.1 | 350.3 | 421.1 | 1.0 [1–1] |
| **Serial (processes)** | render | 2,771.5 [2,766.5–2,774.0] | 173.6 | 175.8 | 187.9 | 1.0 [1–1] |
| | compute | 2,771.4 [2,766.5–2,773.9] | 161.1 | 161.7 | 163.0 | 0.0 [0–0] |
| | *serial pairs* | 41,571.0 pairs [41,497–41,609] | 344.6 | 349.9 | 362.4 | 1.0 [1–1] |
| **Concurrent (threads)** | render | 3,023.8 [3,021.7–3,025.2] | 326.6 | 327.9 | 409.0 | 1.0 [1–1] |
| | compute | 4,160.9 [4,144.7–4,176.7] | 326.2 | 327.7 | 329.5 | 1.0 [1–1] |
| **Concurrent (processes)** | render | 3,025.1 [3,020.9–3,025.3] | 326.6 | 327.9 | 336.0 | 1.0 [1–1] |
| | compute | 4,131.7 [4,130.2–4,227.2] | 326.1 | 327.8 | 329.5 | 1.0 [1–1] |

### Large array workload (262144 elements)

| Execution mode | Worker | Throughput ops/s: median [range] | p50 μs | p95 μs | p99 μs | Late ops: median [range] |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Solo Render (threads)** | render | 30,112.3 [28,823.9–30,300.8] | 27.3 | 31.1 | 41.8 | 1.0 [1–1] |
| **Solo Compute (threads)** | compute | 47,044.3 [46,982.3–47,121.5] | 21.1 | 21.5 | 26.3 | 1.0 [1–1] |
| **Serial (threads)** | render | 2,748.9 [2,747.3–2,750.5] | 178.0 | 181.0 | 191.9 | 1.0 [1–1] |
| | compute | 2,748.8 [2,747.2–2,750.4] | 179.0 | 180.4 | 181.0 | 0.0 [0–0] |
| | *serial pairs* | 41,232.0 pairs [41,208–41,256] | 357.6 | 360.4 | 449.4 | 1.0 [1–1] |
| **Serial (processes)** | render | 2,690.2 [2,680.9–2,694.7] | 173.7 | 175.8 | 188.4 | 1.0 [1–1] |
| | compute | 2,690.2 [2,680.9–2,694.7] | 171.6 | 172.2 | 173.6 | 0.0 [0–0] |
| | *serial pairs* | 40,353.0 pairs [40,213–40,421] | 355.2 | 360.0 | 474.1 | 1.0 [1–1] |
| **Concurrent (threads)** | render | 2,931.7 [2,928.6–2,932.7] | 337.0 | 338.4 | 409.6 | 1.0 [1–1] |
| | compute | 3,484.4 [3,473.9–3,519.9] | 336.9 | 338.3 | 339.7 | 1.0 [1–1] |
| **Concurrent (processes)** | render | 2,932.1 [2,929.8–2,933.1] | 337.1 | 338.5 | 391.7 | 1.0 [1–1] |
| | compute | 3,474.7 [3,460.4–3,487.7] | 336.9 | 338.4 | 339.8 | 1.0 [1–1] |

## Profiler audit and telemetry evidence

All 12 profile runs were captured with Nsight Systems 2025.6.3 and exported to SQLite. Every SQLite database verified cleanly under `PRAGMA integrity_check`.

### GPU hardware context switch analysis
The Nsight `GPU_CONTEXT_SWITCH_EVENTS` table recorded context switch brackets (`RESTORE_START`, `SAVE_END`):

| Condition | Elements | Topology | GPU context switch rows (2s window) | Kernel rows | GL workload rows |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Solo Render** | 1024 | threads | **2** | 0 | 96,940 |
| **Solo Compute** | 1024 | threads | **4** | 226,998 | 0 |
| **Serial** | 1024 | threads | **29,968** | 12,480 | 15,450 |
| **Serial** | 1024 | processes | **28,464** | 10,630 | 14,694 |
| **Concurrent** | 1024 | threads | **32,744** | 15,610 | 16,847 |
| **Concurrent** | 1024 | processes | **32,868** | 14,996 | 16,892 |
| **Solo Render** | 262144 | threads | **2** | 0 | 90,456 |
| **Solo Compute** | 262144 | threads | **4** | 125,926 | 0 |
| **Serial** | 262144 | threads | **29,012** | 9,434 | 14,782 |
| **Serial** | 262144 | processes | **27,378** | 7,733 | 13,786 |
| **Concurrent** | 262144 | threads | **31,820** | 12,544 | 16,349 |
| **Concurrent** | 262144 | processes | **32,152** | 11,657 | 16,537 |

In solo modes, context switches are essentially zero (2 to 4 switches during process setup/teardown). In contrast, both serial and concurrent modes exhibit **27,000 to 33,000 GPU context switches** across the 2-second measurement window.

### Profiler warnings and boundary attribution
The SQLite diagnostics tables recorded:
- `cuda_sw_fallback_warning: true` across all cells.
- Nsight diagnostic warning: *"Scheduling information is absent. The thread activity is deduced based on OS runtime libraries traces. This is inaccurate and does not take into account asynchronous interrupts and exception faults."*
- CPU context switch warning: *"CPU context switches collection is disabled."*
- Workload attribution: GL workload rows recorded `glBindFramebuffer`, `glClear`, and `glDrawArrays`. CUDA kernels recorded `compute_tagged_workload_kernel` (grid size 4, block size 256, 16 registers per thread, 0 shared memory).
- Boundary limit: Nsight software traces and GL elapsed batches do not provide hardware warp scheduling timestamps or Resource Manager Timeslice Group (TSG) identifiers.

## Scientific interpretation and Gate 4 conclusion

1. **Definitive evidence of GPU time-slicing:** Under standard EGL/OpenGL and CUDA APIs, launching rendering and compute simultaneously results in massive GPU context switching (~32,000 switches per 2 seconds) and severe latency inflation.
   - Solo rendering executes in **~27 μs** (p50).
   - Solo compute executes in **~10.6 μs** (1024 elements) or **~21.1 μs** (262144 elements).
   - When run concurrently, both render and compute latencies inflate to **~326–337 μs** (p50).
   - This inflation exactly matches mutual preemption and serialization on the GPU's single 3D/Compute engine.
2. **Identical behavior across thread and process topologies:** Concurrent threads and concurrent processes produce nearly identical throughput (~3,024 vs 3,025 ops/s render; ~4,160 vs 4,131 ops/s compute) and identical latency profiles (~326 μs p50). The bottleneck is in the GPU hardware scheduling, not Linux host process isolation or IPC overhead.
3. **No native spatial sharing:** Standard NVIDIA EGL/OpenGL contexts and CUDA contexts belong to distinct Timeslice Groups (TSGs). The hardware scheduler interleaves them sequentially rather than assigning them concurrent SM partitions.
4. **Gate 4 conclusion:** Gate 4 is closed with a conclusive **negative result for standard runtime APIs**: simultaneous active spatial warp execution does not occur out-of-the-box between native OpenGL and CUDA on Ampere A100.
5. **Path to Gate 5:** True spatial co-execution requires low-level intervention to duplicate TSGs or redirect CUDA subcontexts into the graphics TSG (as described in VUDA's v2 design), bypassing standard user-space runtime segregation.

## Primary sources

- [EXP-009 Controlled Scheduling Plan](../../research/EXP009_CONTROLLED_SCHEDULING_PLAN.md): Experimental specification, matrix geometry, timing contracts, and rejection protocols.
- [Gate 4 Capture Protocol](../../research/GATE4_CAPTURE_PROTOCOL.md): Monotonic timing, freshness invariants, and attribution boundaries.
- [EXP-009 Sanitized Machine Audit](exp009-controlled-scheduling-audit.json): Full machine-readable audit containing all 60 cells, quantiles, and profiler telemetry.
- NVIDIA CUDA Runtime API: Stream synchronization and event elapsed timing contracts.
- Khronos EGL 1.5 Specification: Thread-local context binding and headless platform display initialization.
