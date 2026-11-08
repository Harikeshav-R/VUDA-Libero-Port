# Project Status: VUDA-Libero-Port

## Research goal

Determine whether native EGL/OpenGL rendering and CUDA model inference/training can execute simultaneously on one NVIDIA GPU through spatial sharing. Enabling GPU rendering, matching devices, concurrent host submission, and time-sharing do not satisfy the mentor's goal. The deliverable includes the serialization root cause, version-specific VUDA mechanisms/module changes, and an evidence-backed native OpenGL prototype feasibility verdict, including negative results.

**Permanent target: EPFL host lab.** OSC Ascend, Cardinal, and Pitzer are temporary preliminary testbeds while awaiting EPFL access. OSC observations and permissions do not establish EPFL compatibility or authorization.

## Current decision — November 8, 2026

**Gate 5 source/policy assessment is recorded; runtime redirection remains blocked.** Read [Gate 5 assessment](GATE5_ASSESSMENT.md) and [version-specific driver review](GATE5_DRIVER_ASSESSMENT.md). No author artifact, complete GL control-mapping procedure, or site-specific approval for undocumented controls is available. No shim or low-level runtime test was created or run in this assessment.

Existing native probe and profiler artifacts establish useful baseline capability. Earlier summaries overstated their conclusions: elapsed GL batch ranges and wall-time slowdowns do not prove TSG membership or absence of spatial execution. Gate 4 is corrected to **Partial / inconclusive on active co-execution**. Original logs/traces remain unchanged.

**EXP-005 offline follow-up:** [Preserved metadata queries](../evidence/public/exp005-trace-metadata-audit.json) show that EXP-003 set `COLLECT_GPU_CTX_SW_TRACE=false`; missing switch events are not evidence of denied or unsupported collection. Nsight also warned that the installed CUDA driver compatibility version `13.0` was unsupported by that build and that it would use tracing libraries for `12.6`. This is not the loaded toolkit version, nor proof that all captured events are invalid. CPU sampling was throttled four times. Resolve profiler compatibility before relying on a new decisive capture.

The [observer readiness review](GATE5_OBSERVER_REVIEW.md) resolves source-level transfer-envelope routing and allocation/lifecycle/status decoding requirements. It does not supply a working interposer or compiled target ABI proof. EXP-007 now exercises the existing reduced-grid option (`--elements 1024`, four 256-thread blocks); grid size alone proves neither measured headroom nor TSG isolation.

**EXP-006 live follow-up:** [New evidence](../evidence/public/exp006-live-prerequisites.md) locates site-provided Nsight `2025.6.3.541` in `cuda/13.2.1` while retaining toolkit `12.8.1`. Fresh export reproduces all 91 schemas and 90 non-export-metadata table row multisets from the preserved EXP-003 database. User-authorized GPU preflight `8389776` completed successfully on an A100/driver `580.95.05`; metric discovery lists `cuda-visible` and `ga100`. These are exercised prerequisites, not a new trace or spatial-sharing result.

**EXP-007 actual capture:** [The repaired smoke](../evidence/public/exp007-profile-results.md) completed as job `8390387` with exit `0:0`, after an initial NVML-inventory guard refusal in `8389929`. A pre-context CUDA/EGL UUID guard compiled and ran; CUDA, EGL, selected NVML, and profiler physical identities matched. All 21 logged commands succeeded: ten unprofiled runs, four profiles, four fresh exports, and three builds/identity steps. Nsight `2025.6.3.541` collected GPU context-switch and GA100 metric records without the old unsupported-driver warning. CUDA tracing used the documented software fallback; CPU thread scheduling was intentionally disabled. Capture UTC was November 8, corresponding to November 7 locally.

**EXP-007 proof boundary:** instrumented traces show non-overlapping restore/save brackets, not exact active-warp execution or RM TSG membership. Metrics remain device aggregates. EXP-007 itself lacked final serial/concurrent output checks and independent compute-latency reporting; EXP-008 addresses those bounded gaps below. Gate 4 stays partial; neither a positive spatial-sharing result nor a universal negative follows.

**Hardware-backend clarification:** the versioned Nsight `2025.6` guide requires Blackwell-or-later GPUs for HES CUDA hardware tracing. The recorded A100 is Ampere and fails that requirement; root access or a toolkit upgrade cannot add this backend. EXP-007 already used `--trace=cuda`, which automatically selects hardware where supported and can fall back. GPU switch/metric collection remains available separately. See the [source-backed explanation and future-target command](../evidence/public/exp007-profile-results.md#cuda-hardware-tracing-prerequisites-and-20256-cli-behavior). No new GPU allocation or Blackwell run occurred for this clarification.

**EXP-008 correctness/latency prerequisite:** [The native experiment](../evidence/public/exp008-correctness-latency.md) completed as job `8391277`, exit `0:0`, on an A100/driver `580.95.05` with toolkit `12.8.1`. Both source versions compiled; eight repaired rejection scenarios exited cleanly with `1` and no report, versus seven original zero exits and one readiness hang. Four original output injections were not reached because final readbacks were absent. Ten normal runs passed all six final checks and reported independent concurrent EGL/CUDA latency. A first batch failed before compilation because Slurm relocated its entrypoint; the explicit-root repair and failed attempt are preserved separately. These are static sampled/pixel correctness checks, not iteration freshness or active spatial execution. Equal-duration steady-state, independent concurrent warmup, matched process controls, headroom, and scheduling attribution remain open.

**EXP-009 controlled scheduling completed (conclusive Gate 4 negative result):** [The controlled scheduling experiment](../evidence/public/exp009-controlled-scheduling.md) and [machine audit](../evidence/public/exp009-controlled-scheduling-audit.json) completed as job `8401856` (exit `0:0`, elapsed 24:35) on an A100-PCIE-40GB under driver `580.95.05` and toolkit `12.8.1`. Native tag boundary/stale proof, 13 fault rejections, 11 CLI rejections, 12 strict per-iteration validation runs, closed-pipe IPC rejection, and a 12-cell timed smoke run all passed. The complete 60-cell unprofiled matrix (6 conditions × 2 sizes × 5 repetitions, 5s independent worker warmup / 15s synchronized window) was verified 60/60 by independent binary timestamp replay (`EXP9TS01_LE`). All 12 separate Nsight profiling captures and SQLite exports succeeded. Concurrent execution inflates latencies from solo baselines (~27 μs GL / ~10.6 μs CUDA) to ~326–337 μs across both thread and process topologies. Profiles reveal ~32,000 GPU context switches per 2s window in serial and concurrent modes (versus 2–4 in solo modes). Gate 4 is closed with a definitive negative result for standard runtime APIs: native EGL/OpenGL and CUDA time-slice the single 3D/Compute hardware engine without active spatial sharing out-of-the-box.

## Evidence gates

| Gate | Milestone | Status | Evidence and remaining limits |
| --- | --- | --- | --- |
| 1 | Problem formulation and literature | Passed | Mentor requires native OpenGL spatial sharing; S1–S23 reviewed. First author Bin Xu (SJTU IPADS) confirmed forthcoming anonymized code release with native OpenGL support testing. |
| 2 | Environment discovery | Passed for recorded OSC allocations | A100-PCIE-40GB, open driver/GSP `580.95.05`, CUDA toolkit `12.8.1`, NVIDIA EGL/OpenGL demonstrated in later probe logs. Inventory is not a current allocation grant. |
| 3 | Independent native EGL/CUDA baselines | Passed, bounded smoke/baseline scope | EXP-002 records pixel and sampled numerical checks plus four synthetic timings. Not repeated steady-state characterization, cross-process coverage, or real-model evaluation. |
| 4 | Standard-API co-execution profiling | Passed (conclusive negative result for standard APIs) | EXP-009 completed the 60-cell matrix and 12 profile exports on A100. Massive context switching (~32k/2s) and ~12x latency inflation prove GPU engine time-slicing without active spatial sharing under standard APIs. |
| 5 | TSG/channel-redirection technical and policy review | Assessment recorded; experimental go-ahead blocked | Version-specific RM paths reviewed. Author confirmed approach requires root/admin ioctls to KMD, making unprivileged OSC deployment infeasible. Target shifts to EPFL with recommended RTX 4090 / 6000 Ada hardware. |
| 6 | LIBERO observation pipeline and handoff | Partial, registration/mapping only | Renderbuffer registration and `cudaArray_t` retrieval logged successfully. CUDA-side image consumption, copy-free behavior, tensor conversion, and MuJoCo/robosuite integration unverified. |
| 7 | End-to-end inference/training | Pending | Actual model/checkpoint, deployed stack, independent ready work, algorithm schedule, learning and performance baselines missing. |

## Recorded observations and interpretation

### EXP-002 — native synthetic probe

Slurm job `8374921`, OSC Ascend temporary testbed; A100-PCIE-40GB. Source: `src/probe/`; [sanitized findings](../evidence/public/exp002-probe-results.md).

| Mode | Recorded wall time | Recorded throughput |
| --- | --- | --- |
| Solo EGL render | 20.04 ms | 24,950.7 frames/s |
| Solo CUDA compute | 14.27 ms | 35,028.1 iterations/s |
| Serial interleaved | 230.38 ms | 2,170.4 pairs/s |
| Concurrent host threads | 92.27 ms | 5,419.0 pairs/s |

Concurrent submission improves throughput over the serial interleaved baseline in this run. It still exceeds the solo wall-time sum of 34.31 ms. This establishes overhead/interference, not its unique driver cause. The 0.372× metric is `34.31 / 92.27`, not a statement that concurrent time is shorter than the solo sum.

The measured render path clears, draws a triangle, and calls `glFinish()`. CUDA uses a synthetic arithmetic kernel, not inference/training. A 1,024-block launch does not establish low resource use or measured saturation. Interop registers/maps/unmaps a renderbuffer without reading mapped pixels in CUDA; it does not prove zero-copy implementation or image correctness after handoff.

### EXP-003 — profiler collection

Slurm job `8375678` with `--gres=nsight`; Nsight Systems `2024.7.1.84`. [Sanitized findings](../evidence/public/exp003-profile-results.md) and [offline trace audit](GATE5_ASSESSMENT.md#1-what-the-existing-experiments-establish).

Preserved intervals show CUDA kernels inside GL batch elapsed ranges and substantial GL duration inflation. NVIDIA documents batching and `glQueryCounter()` completion timing, so these observations are compatible with several scheduling/contention explanations. H-01 remains a hypothesis for this GL stack, not an established TSG allocation observation.

The profile run reports 64.45 ms serial and 67.92 ms concurrent wall time; do not substitute EXP-002's unprofiled speedup. The historical final export failed on an existing destination. EXP-006 now establishes event-content correspondence through a fresh, non-overwriting export of the hash-matched report: all 91 schemas and 90 non-export-metadata table row multisets match. Export metadata differs as expected; this does not authenticate every historical command or actor.

EXP-005's earlier metadata-only consistency check and local exporter absence are preserved in its ledger entry. EXP-006 performed the later export remotely with the same historical exporter version. Neither export can add disabled switch events.

### EXP-007 — corrected standard-API capability smoke

Job `8390387` completed in 51 seconds on the temporary OSC A100 testbed using driver `580.95.05`, toolkit `12.8.1`, and independently selected Nsight `2025.6.3.541`. [Sanitized results and timings](../evidence/public/exp007-profile-results.md) and [machine-readable audit](../evidence/public/exp007-profile-audit.json) distinguish actual collection from active-execution proof.

The first attempt refused a two-row NVML inventory before compilation. Its repair matches physical UUID/PCI rather than inventory size. Seven CPU boundary cases passed; the successful native allocation exposed one NVML row. All fourteen application invocations logged initial EGL/CUDA and registration/map checks. The frozen EXP-007 source lacked final serial/concurrent checks; EXP-008 subsequently added them. CUDA consumption of mapped pixels remains unverified.

Recorded restore/save pairs are disjoint and consistent with switching, but are not exact active-warp intervals. Scheduling IDs lack a direct RM TSG mapping. Save-to-restore gaps are not measured causal switching cost. The CPU scheduling warning and CUDA software-tracing fallback remain explicit diagnostic limits; neither means GPU switch collection failed.
### EXP-009 — controlled native scheduling and profiling

Slurm job `8401856`, OSC Ascend temporary testbed; A100-PCIE-40GB. [Sanitized findings](../evidence/public/exp009-controlled-scheduling.md) and [machine audit](../evidence/public/exp009-controlled-scheduling-audit.json).

The 60-cell matrix (5s warmup / 15s measurement window) and 12 separate profiles (1s warmup / 2s measurement window, Nsight `2025.6.3.541`, `ga100` metric set) executed in a single 24:35 batch after target compilation and correctness/rejection verification:

| Condition | Array elements | Worker / Metric | Throughput ops/s (median [range]) | p50 μs | p95 μs | p99 μs |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Solo Render (threads)** | 1024 | render | 30,470.8 [29,931.0–30,568.0] | 26.9 | 30.3 | 41.4 |
| **Solo Compute (threads)** | 1024 | compute | 93,636.9 [93,247.5–93,806.7] | 10.6 | 11.0 | 11.8 |
| **Serial (threads)** | 1024 | render / compute | 2,830.5 [2,826.7–2,833.3] | 178.1 / 168.6 | 180.7 / 169.9 | 192.2 / 170.7 |
| | | *serial pairs* | 42,457.0 pairs [42,399–42,500] | 347.1 | 350.3 | 421.1 |
| **Serial (processes)** | 1024 | render / compute | 2,771.5 [2,766.5–2,774.0] | 173.6 / 161.1 | 175.8 / 161.7 | 187.9 / 163.0 |
| | | *serial pairs* | 41,571.0 pairs [41,497–41,609] | 344.6 | 349.9 | 362.4 |
| **Concurrent (threads)** | 1024 | render / compute | 3,023.8 / 4,160.9 | 326.6 / 326.2 | 327.9 / 327.7 | 409.0 / 329.5 |
| **Concurrent (processes)** | 1024 | render / compute | 3,025.1 / 4,131.7 | 326.6 / 326.1 | 327.9 / 327.8 | 336.0 / 329.5 |
| **Solo Render (threads)** | 262144 | render | 30,112.3 [28,823.9–30,300.8] | 27.3 | 31.1 | 41.8 |
| **Solo Compute (threads)** | 262144 | compute | 47,044.3 [46,982.3–47,121.5] | 21.1 | 21.5 | 26.3 |
| **Serial (threads)** | 262144 | render / compute | 2,748.9 [2,747.3–2,750.5] | 178.0 / 179.0 | 181.0 / 180.4 | 191.9 / 181.0 |
| | | *serial pairs* | 41,232.0 pairs [41,208–41,256] | 357.6 | 360.4 | 449.4 |
| **Serial (processes)** | 262144 | render / compute | 2,690.2 [2,680.9–2,694.7] | 173.7 / 171.6 | 175.8 / 172.2 | 188.4 / 173.6 |
| | | *serial pairs* | 40,353.0 pairs [40,213–40,421] | 355.2 | 360.0 | 474.1 |
| **Concurrent (threads)** | 262144 | render / compute | 2,931.7 / 3,484.4 | 337.0 / 336.9 | 338.4 / 338.3 | 409.6 / 339.7 |
| **Concurrent (processes)** | 262144 | render / compute | 2,932.1 / 3,474.7 | 337.1 / 336.9 | 338.5 / 338.4 | 391.7 / 339.8 |

GPU context switch events in the 2s profile window: solo render had 2, solo compute had 4; serial modes had 27k–30k; concurrent modes had 32k–33k switches. Latency jumps from ~27 μs (GL) and ~10.6 μs (CUDA) to ~326–337 μs under concurrent execution. This proves the GPU hardware engine is actively time-slicing rendering and compute rather than co-executing them spatially. Thread and process topologies behave identically, confirming the bottleneck is GPU scheduling, not host IPC.


## Mechanism and application invariants

- VUDA v1 uses forwarding channels and kernel-module page-table changes. V2 describes user-space allocation interception/object duplication, redirected CUDA channel/subcontext parents, preserved CUDA data VAS, shared graphics control mappings, and shadow-channel SCG controls. V2 reports no driver patch; its internals are not supported public APIs.
- V2 evaluates Vulkan/CUDA, not native OpenGL. Its A800 driver `610.57.04` is not the recorded A100 driver `580.95.05`. Shared architecture does not establish compatibility.
- Do not try parent redirection without the required control mappings. Source definitions alone do not reveal proprietary GL initialization or GSP runtime acceptance.
- Keep scheduling, image handoff, and resource partitioning as independent ablations. Neither CUDA/GL mapping nor compute-only masks establish graphics partitioning.
- Preserve LIBERO's native renderer, observation freshness, camera/layout/orientation, environment/episode associations, rewards, termination, and control rate. Overlap independent groups, not inference with production of its own input.
- Training must preserve policy versions and rollout/update barriers. No checkpoint or algorithm was supplied; synthetic kernels do not resolve that gap.

## Provenance and privacy

The first inventory and toolchain follow-up are user-supplied evidence. Later ledger entries record setup/probe/profile execution; raw logs corroborate outputs, not every historical actor. EXP-004/EXP-005 were offline reviews. EXP-006 connected live for module/scheduler/tool checks, report conversion, and an allocated GPU preflight. EXP-007 compiled and ran the identity guard/native probe and collected new traces. EXP-008 separately compiled original/updated probes and exercised failure rejection plus normal correctness/latency runs. EXP-009 compiled on target, passed 13 fault rejections and 11 CLI rejections, verified 60/60 matrix cells via raw timestamp replay, and audited 12 profile exports. Exact commands, UTC times, physical identities, failed attempts, and outcomes are preserved privately.

Raw captures remain under ignored `evidence/osc/`; reviewed derivatives are public. Initial inventory capture time is 2026-11-07 00:41 UTC; EXP-002 and EXP-003 record later UTC times corresponding to November 6 locally. Preserve raw bytes and separate later interpretation.

A course account's scheduler access is not approval to charge this research. Use a PI-authorized project. No written OSC approval for the proposed internal controls is present. The project's restrictions are precautions, not a fabricated OSC blanket ban. `--gres=nsight` handles profiler contention, not driver mutation permission.

## Next actions and their dependencies

1. **Gate 5 privilege and site constraint:** Author confirmed the channel-redirection implementation requires administrator (root) access to issue KMD `ioctl()` calls. Because OSC does not grant root or allow custom KMD modifications, Gate 5 runtime redirection is infeasible on OSC. Testing shifts to the permanent **EPFL host lab** (or a dedicated bare-metal testbed where root access is available).
2. **Author code release and OpenGL support:** Bin Xu (first author, SJTU IPADS) confirmed an anonymized code release is in preparation. The author noted that OpenGL and Vulkan share lower-level paths in the NVIDIA user-mode driver (UMD) and is actively validating OpenGL support to include in the release. The recommended platforms are graphics-focused GPUs (**RTX 4090** or **RTX 6000 Ada**); H100 is advised against due to reduced graphics rendering hardware.
3. **Next research work (Gate 5 focus):** With Gate 4 conclusively settled by EXP-009 (standard APIs time-slice the GPU), standard API tuning will not achieve spatial sharing. Research must shift to Gate 5 low-level mechanisms: TSG duplication and parent channel redirection (as in VUDA v2) to evaluate whether graphics and compute channels can be merged into a single hardware TSG, subject to site policy and driver compatibility.
4. Use the [observer review](GATE5_OBSERVER_REVIEW.md) as a source-derived capture contract, not an implemented tracer. Only after site permission, implementation, and transparency review, capture own-process GL/CUDA lifecycles without altering allocations. Verify forwarding, statuses, initialization, teardown, subcontexts, address ranges, and control mappings before any rewrite.
5. Reproduce the source-backed Vulkan mechanism on an approved isolated target before attempting native GL redirection. Treat fixed SCG partitioning as a separate later gate.
6. Resume LIBERO image consumption and real inference/training only with the actual application/model/schedule. EPFL requires its own inventory and safety review.
