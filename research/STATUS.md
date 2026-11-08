# Project Status: VUDA-Libero-Port

## Research goal

Determine whether native EGL/OpenGL rendering and CUDA model inference/training can execute simultaneously on one NVIDIA GPU through spatial sharing. Enabling GPU rendering, matching devices, concurrent host submission, and time-sharing do not satisfy the mentor's goal. The deliverable includes the serialization root cause, version-specific VUDA mechanisms/module changes, and an evidence-backed native OpenGL prototype feasibility verdict, including negative results.

**Permanent target: EPFL host lab.** OSC Ascend, Cardinal, and Pitzer are temporary preliminary testbeds while awaiting EPFL access. OSC observations and permissions do not establish EPFL compatibility or authorization.

## Current decision — November 7, 2026

**Gate 5 source/policy assessment is recorded; runtime redirection remains blocked.** Read [Gate 5 assessment](GATE5_ASSESSMENT.md) and [version-specific driver review](GATE5_DRIVER_ASSESSMENT.md). No author artifact, complete GL control-mapping procedure, or site-specific approval for undocumented controls is available. No shim or low-level runtime test was created or run in this assessment.

Existing native probe and profiler artifacts establish useful baseline capability. Earlier summaries overstated their conclusions: elapsed GL batch ranges and wall-time slowdowns do not prove TSG membership or absence of spatial execution. Gate 4 is corrected to **Partial / inconclusive on active co-execution**. Original logs/traces remain unchanged.

**EXP-005 offline follow-up:** [Preserved metadata queries](../evidence/public/exp005-trace-metadata-audit.json) show that EXP-003 set `COLLECT_GPU_CTX_SW_TRACE=false`; missing switch events are not evidence of denied or unsupported collection. Nsight also warned that the installed CUDA driver compatibility version `13.0` was unsupported by that build and that it would use tracing libraries for `12.6`. This is not the loaded toolkit version, nor proof that all captured events are invalid. CPU sampling was throttled four times. Resolve profiler compatibility before relying on a new decisive capture.

The [observer readiness review](GATE5_OBSERVER_REVIEW.md) resolves source-level transfer-envelope routing and allocation/lifecycle/status decoding requirements. It does not supply a working interposer or compiled target ABI proof. EXP-007 now exercises the existing reduced-grid option (`--elements 1024`, four 256-thread blocks); grid size alone proves neither measured headroom nor TSG isolation.

**EXP-006 live follow-up:** [New evidence](../evidence/public/exp006-live-prerequisites.md) locates site-provided Nsight `2025.6.3.541` in `cuda/13.2.1` while retaining toolkit `12.8.1`. Fresh export reproduces all 91 schemas and 90 non-export-metadata table row multisets from the preserved EXP-003 database. User-authorized GPU preflight `8389776` completed successfully on an A100/driver `580.95.05`; metric discovery lists `cuda-visible` and `ga100`. These are exercised prerequisites, not a new trace or spatial-sharing result.

**EXP-007 actual capture:** [The repaired smoke](../evidence/public/exp007-profile-results.md) completed as job `8390387` with exit `0:0`, after an initial NVML-inventory guard refusal in `8389929`. A pre-context CUDA/EGL UUID guard compiled and ran; CUDA, EGL, selected NVML, and profiler physical identities matched. All 21 logged commands succeeded: ten unprofiled runs, four profiles, four fresh exports, and three builds/identity steps. Nsight `2025.6.3.541` collected GPU context-switch and GA100 metric records without the old unsupported-driver warning. CUDA tracing used the documented software fallback; CPU thread scheduling was intentionally disabled. Capture UTC was November 8, corresponding to November 7 locally.

**EXP-007 proof boundary:** instrumented traces show non-overlapping restore/save brackets, not exact active-warp execution or RM TSG membership. Metrics remain device aggregates. EXP-007 itself lacked final serial/concurrent output checks and independent compute-latency reporting; EXP-008 addresses those bounded gaps below. Gate 4 stays partial; neither a positive spatial-sharing result nor a universal negative follows.

**Hardware-backend clarification:** the versioned Nsight `2025.6` guide requires Blackwell-or-later GPUs for HES CUDA hardware tracing. The recorded A100 is Ampere and fails that requirement; root access or a toolkit upgrade cannot add this backend. EXP-007 already used `--trace=cuda`, which automatically selects hardware where supported and can fall back. GPU switch/metric collection remains available separately. See the [source-backed explanation and future-target command](../evidence/public/exp007-profile-results.md#cuda-hardware-tracing-prerequisites-and-20256-cli-behavior). No new GPU allocation or Blackwell run occurred for this clarification.

**EXP-008 correctness/latency prerequisite:** [The native experiment](../evidence/public/exp008-correctness-latency.md) completed as job `8391277`, exit `0:0`, on an A100/driver `580.95.05` with toolkit `12.8.1`. Both source versions compiled; eight repaired rejection scenarios exited cleanly with `1` and no report, versus seven original zero exits and one readiness hang. Four original output injections were not reached because final readbacks were absent. Ten normal runs passed all six final checks and reported independent concurrent EGL/CUDA latency. A first batch failed before compilation because Slurm relocated its entrypoint; the explicit-root repair and failed attempt are preserved separately. These are static sampled/pixel correctness checks, not iteration freshness or active spatial execution. Equal-duration steady-state, independent concurrent warmup, matched process controls, headroom, and scheduling attribution remain open.

**Next technical milestone — EXP-009 (planned only):** complete the [controlled scheduling experiment](EXP009_CONTROLLED_SCHEDULING_PLAN.md): matched serial/concurrent thread/process controls, independent warmup, equal-duration repetitions, explicit deadline/drain accounting, and separate per-iteration validation. The plan distinguishes controlled interference measurements from an attributable active-execution verdict. No EXP-009 code, allocation, runtime result, or profile exists yet. Author access and site permission for later internal work are separate human prerequisites, not a reason to stall standard-API controls.

## Evidence gates

| Gate | Milestone | Status | Evidence and remaining limits |
| --- | --- | --- | --- |
| 1 | Problem formulation and literature | Passed | Mentor requires native OpenGL spatial sharing; historical S1–S23 review and live v2 mechanism review. Author-source request remains pending. |
| 2 | Environment discovery | Passed for recorded OSC allocations | A100-PCIE-40GB, open driver/GSP `580.95.05`, CUDA toolkit `12.8.1`, NVIDIA EGL/OpenGL demonstrated in later probe logs. Inventory is not a current allocation grant. |
| 3 | Independent native EGL/CUDA baselines | Passed, bounded smoke/baseline scope | EXP-002 records pixel and sampled numerical checks plus four synthetic timings. Not repeated steady-state characterization, cross-process coverage, or real-model evaluation. |
| 4 | Standard-API co-execution profiling | Partial; active sharing and TSG root cause inconclusive | EXP-007 captures switch/metric records; EXP-008 adds tested final-output rejection and independent concurrent CUDA latency. Static samples and restore/save brackets do not establish active spatial sharing or RM TSG identity. Freshness, matched steady-state/process controls, headroom, and attribution remain incomplete. |
| 5 | TSG/channel-redirection technical and policy review | Assessment recorded; experimental go-ahead blocked | Version-specific candidate RM paths reviewed. Actual GL allocations/control mappings, artifact or complete independent design, and site permission remain missing. GSP incompatibility is not proved. |
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

## Mechanism and application invariants

- VUDA v1 uses forwarding channels and kernel-module page-table changes. V2 describes user-space allocation interception/object duplication, redirected CUDA channel/subcontext parents, preserved CUDA data VAS, shared graphics control mappings, and shadow-channel SCG controls. V2 reports no driver patch; its internals are not supported public APIs.
- V2 evaluates Vulkan/CUDA, not native OpenGL. Its A800 driver `610.57.04` is not the recorded A100 driver `580.95.05`. Shared architecture does not establish compatibility.
- Do not try parent redirection without the required control mappings. Source definitions alone do not reveal proprietary GL initialization or GSP runtime acceptance.
- Keep scheduling, image handoff, and resource partitioning as independent ablations. Neither CUDA/GL mapping nor compute-only masks establish graphics partitioning.
- Preserve LIBERO's native renderer, observation freshness, camera/layout/orientation, environment/episode associations, rewards, termination, and control rate. Overlap independent groups, not inference with production of its own input.
- Training must preserve policy versions and rollout/update barriers. No checkpoint or algorithm was supplied; synthetic kernels do not resolve that gap.

## Provenance and privacy

The first inventory and toolchain follow-up are user-supplied evidence. Later ledger entries record setup/probe/profile execution; raw logs corroborate outputs, not every historical actor. EXP-004/EXP-005 were offline reviews. EXP-006 connected live for module/scheduler/tool checks, report conversion, and an allocated GPU preflight. EXP-007 compiled and ran the identity guard/native probe and collected new traces. EXP-008 separately compiled original/updated probes and exercised failure rejection plus normal correctness/latency runs on an authorized allocation. Exact commands, UTC times, physical identities, failed attempts, and outcomes are preserved privately. EXP-009 currently records a future plan only.

Raw captures remain under ignored `evidence/osc/`; reviewed derivatives are public. Initial inventory capture time is 2026-11-07 00:41 UTC; EXP-002 and EXP-003 record later UTC times corresponding to November 6 locally. Preserve raw bytes and separate later interpretation.

A course account's scheduler access is not approval to charge this research. Use a PI-authorized project. No written OSC approval for the proposed internal controls is present. The project's restrictions are precautions, not a fabricated OSC blanket ban. `--gres=nsight` handles profiler contention, not driver mutation permission.

## Next actions and their dependencies

1. **Independent Gate 5 permission prerequisite:** obtain scope-specific OSC guidance using the [unsent inquiry](GATE5_ASSESSMENT.md#unsent-support-inquiry). Request separate answers for observation, channel/mapping mutation, and undocumented SCG commands; require an isolated test/recovery arrangement where needed. Do not send automatically. This does not block approved standard-API EXP-009 controls.
2. **Independent mechanism-access prerequisite:** obtain actual VUDA v2 source/commit, reproduction requirements, control mappings, redirection, initialization/teardown, and OpenGL advice. Without an artifact or complete design, classify later work as independent reverse engineering. Do not invent ioctl structures or command words.
3. **Next executable research work:** implement and run [EXP-009](EXP009_CONTROLLED_SCHEDULING_PLAN.md), using the [Gate 4 proof limits](GATE4_CAPTURE_PROTOCOL.md). Complete the full declared matrix, validation/rejection proof, target runs, separate profile attempts/outcomes, and reviewed evidence. Per-iteration validation is separate from lower-overhead timed final-tag checks. Scheduling attribution may remain inconclusive after a complete controlled experiment; do not launch another allocation merely to repeat capability checks.
4. Use the [observer review](GATE5_OBSERVER_REVIEW.md) as a source-derived capture contract, not an implemented tracer. Only after site permission, implementation, and transparency review, capture own-process GL/CUDA lifecycles without altering allocations. Verify forwarding, statuses, initialization, teardown, subcontexts, address ranges, and control mappings before any rewrite.
5. Reproduce the source-backed Vulkan mechanism on an approved isolated target before attempting native GL redirection. Treat fixed SCG partitioning as a separate later gate.
6. Resume LIBERO image consumption and real inference/training only with the actual application/model/schedule. EPFL requires its own inventory and safety review.
