# Project Status: VUDA-Libero-Port

## Research goal

Determine whether native EGL/OpenGL rendering and CUDA model inference/training can execute simultaneously on one NVIDIA GPU through spatial sharing. Enabling GPU rendering, matching devices, concurrent host submission, and time-sharing do not satisfy the mentor's goal. The deliverable includes the serialization root cause, version-specific VUDA mechanisms/module changes, and an evidence-backed native OpenGL prototype feasibility verdict, including negative results.

**Permanent target: EPFL host lab.** OSC Ascend, Cardinal, and Pitzer are temporary preliminary testbeds while awaiting EPFL access. OSC observations and permissions do not establish EPFL compatibility or authorization.

## Current decision — November 6, 2026

**Gate 5 source/policy assessment is recorded; runtime redirection remains blocked.** Read [Gate 5 assessment](GATE5_ASSESSMENT.md) and [version-specific driver review](GATE5_DRIVER_ASSESSMENT.md). No author artifact, complete GL control-mapping procedure, or site-specific approval for undocumented controls is available. No shim or low-level runtime test was created or run in this assessment.

Existing native probe and profiler artifacts establish useful baseline capability. Earlier summaries overstated their conclusions: elapsed GL batch ranges and wall-time slowdowns do not prove TSG membership or absence of spatial execution. Gate 4 is corrected to **Partial / inconclusive on active co-execution**. Original logs/traces remain unchanged.

## Evidence gates

| Gate | Milestone | Status | Evidence and remaining limits |
| --- | --- | --- | --- |
| 1 | Problem formulation and literature | Passed | Mentor requires native OpenGL spatial sharing; historical S1–S23 review and live v2 mechanism review. Author-source request remains pending. |
| 2 | Environment discovery | Passed for recorded OSC allocations | A100-PCIE-40GB, open driver/GSP `580.95.05`, CUDA toolkit `12.8.1`, NVIDIA EGL/OpenGL demonstrated in later probe logs. Inventory is not a current allocation grant. |
| 3 | Independent native EGL/CUDA baselines | Passed, bounded smoke/baseline scope | EXP-002 records pixel and sampled numerical checks plus four synthetic timings. Not repeated steady-state characterization, cross-process coverage, or real-model evaluation. |
| 4 | Standard-API co-execution profiling | Partial; active sharing and TSG root cause inconclusive | EXP-003 has 341 kernels and 666 GL workload batches. No GPU context-switch events or GPU metrics table in the preserved SQLite export. Batched timestamp ranges do not identify active SM execution. Export step ended with an existing-file error. |
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

The profile run reports 64.45 ms serial and 67.92 ms concurrent wall time; do not substitute EXP-002's unprofiled speedup. The final export command failed because the SQLite destination already existed. Matching ranges support consistency but do not authenticate the export/report pairing. A future re-export must use a new output path.

## Mechanism and application invariants

- VUDA v1 uses forwarding channels and kernel-module page-table changes. V2 describes user-space allocation interception/object duplication, redirected CUDA channel/subcontext parents, preserved CUDA data VAS, shared graphics control mappings, and shadow-channel SCG controls. V2 reports no driver patch; its internals are not supported public APIs.
- V2 evaluates Vulkan/CUDA, not native OpenGL. Its A800 driver `610.57.04` is not the recorded A100 driver `580.95.05`. Shared architecture does not establish compatibility.
- Do not try parent redirection without the required control mappings. Source definitions alone do not reveal proprietary GL initialization or GSP runtime acceptance.
- Keep scheduling, image handoff, and resource partitioning as independent ablations. Neither CUDA/GL mapping nor compute-only masks establish graphics partitioning.
- Preserve LIBERO's native renderer, observation freshness, camera/layout/orientation, environment/episode associations, rewards, termination, and control rate. Overlap independent groups, not inference with production of its own input.
- Training must preserve policy versions and rollout/update barriers. No checkpoint or algorithm was supplied; synthetic kernels do not resolve that gap.

## Provenance and privacy

The first inventory and toolchain follow-up are user-supplied evidence. Later ledger entries record remote setup/probe/profile execution; local raw logs corroborate their outputs. This assessment reanalyzed those existing artifacts offline and did not connect to OSC or rerun experiments. Do not infer who issued every historical command from output alone.

Raw captures remain under ignored `evidence/osc/`; reviewed derivatives are public. Initial inventory capture time is 2026-11-07 00:41 UTC; EXP-002 and EXP-003 record later UTC times corresponding to November 6 locally. Preserve raw bytes and separate later interpretation.

A course account's scheduler access is not approval to charge this research. Use a PI-authorized project. No written OSC approval for the proposed internal controls is present. The project's restrictions are precautions, not a fabricated OSC blanket ban. `--gres=nsight` handles profiler contention, not driver mutation permission.

## Next actions, in dependency order

1. Obtain scope-specific OSC guidance using the [unsent inquiry](GATE5_ASSESSMENT.md#unsent-support-inquiry). Request separate answers for observation, channel/mapping mutation, and undocumented SCG commands; require an isolated test/recovery arrangement where needed. Do not send automatically.
2. Obtain the actual VUDA v2 source/commit and reproduction/control-mapping/teardown details. Without it, explicitly scope independent reverse engineering; do not invent ioctl structures or command words.
3. Strengthen Gate 4 on an authorized allocation: bounded low-resource work, verified physical identity and outputs, repeated unprofiled baselines, and supported scheduling/activity correlation. Preserve failed profiling steps. No privileged workaround or service change.
4. Only after permission and a reviewed observer, capture own-process GL/CUDA object lifecycles without altering allocations. Verify forwarding, status propagation, initialization, teardown, subcontexts, address ranges, and control mappings before any rewrite.
5. Reproduce the source-backed Vulkan mechanism on an approved isolated target before attempting native GL redirection. Treat fixed SCG partitioning as a separate later gate.
6. Resume LIBERO image consumption and real inference/training only with the actual application/model/schedule. EPFL requires its own inventory and safety review.
