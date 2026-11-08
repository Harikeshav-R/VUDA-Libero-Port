# EXP-007: Native EGL/CUDA profiler smoke and scheduling evidence

## Result and boundary

**The repaired standard-API smoke completed successfully. GPU context-switch and metric collection now work on this recorded OSC A100 stack. Active spatial sharing and RM TSG membership remain inconclusive.** EPFL is the permanent host lab; OSC is temporary.

The first job, `8389929`, failed safely before compilation or application GPU work. Repaired job `8390387` completed with scheduler exit `0:0` in 51 seconds. Its application log spans UTC **2026-11-08 00:14:39–00:15:27**, corresponding to November 7 locally. All 21 recorded commands returned zero: identity-helper build/run, probe build, ten unprofiled runs, four profiles, and four fresh SQLite exports.

Raw captures remain private under ignored `evidence/osc/ascend/exp007-profiler-smoke/`. This is a reviewed derivative, not a public raw trace or an executed VUDA port. The [sanitized machine-readable audit](exp007-profile-audit.json) records input digests, counts, diagnostics, and query templates.

## 1. Failed attempt and narrow identity repair

The first allocation exposed two NVML A100 inventory rows despite one CUDA-visible device. The launcher incorrectly required exactly one NVML row and refused:

```text
REFUSAL: require one accessible, physically identified A100
```

Only its inventory CSV was produced. This was an experiment-control error, not a graphics, CUDA, profiler-permission, or scheduling limitation. The failed script, source snapshot, inventory, submission records, and output are preserved unchanged.

The repair queries the single CUDA-visible device through supported CUDA APIs, verifies the intended EGL device's physical UUID **before creating a GL display/context**, and selects exactly one NVML row matching CUDA UUID and normalized PCI address. It does not remove allocation guards, select the first inventory row, equate numeric ordinals/minors, or override CUDA visibility. Missing/ambiguous identities, unsupported runtime queries, multiple CUDA-visible devices, and PCI disagreement cause refusal.

Primary source basis:

- [CUDA device visibility](https://docs.nvidia.com/cuda/cuda-programming-guide/05-appendices/environment-variables.html): CUDA ordinals are local to the visible set.
- [EGL_EXT_device_persistent_id](https://registry.khronos.org/EGL/extensions/EXT/EGL_EXT_device_persistent_id.txt): `EGL_DEVICE_UUID_EXT` is a 16-byte cross-API physical-device identifier, queryable without a GL context; multiple handles can identify the same physical device.
- [NVIDIA CUDA graphics interoperability](https://docs.nvidia.com/cuda/cuda-programming-guide/04-special-topics/graphics-interop.html): NVIDIA compares `cudaDeviceProp.uuid` with the Vulkan physical-device UUID.
- [EGL_NV_device_cuda](https://registry.khronos.org/EGL/extensions/NV/EGL_NV_device_cuda.txt): its CUDA-device handle does not specify visibility-mask interaction. Numeric equality alone is not physical identity proof.

A local replay reproduced the original failure on its exact two-row CSV. Seven CPU boundary cases passed: selecting the identified second row, reordering inventory, accepting a singleton, and rejecting unknown UUID, PCI mismatch, multiple CUDA-visible devices, and duplicate UUID matches. Native helper compilation and UUID queries then succeeded in the repaired allocation. CUDA, EGL, selected NVML, and each profile's captured GPU UUID/PCI matched.

**Target coverage limit:** the successful allocation exposed one NVML row. Multi-inventory selection was exercised in CPU cases, not in a successful two-row native allocation. The external guard is private experiment control; the unchanged public probe does not itself implement cross-API UUID proof.

## 2. Executed configuration

The authorized job requested one GPU, four CPUs, 16 GiB, five minutes, and `--gres=nsight`; accounting recorded five CPUs, one A100-PCIE-40GB, and 16 GiB. Requested versus allocated `nsight` bookkeeping does not establish DCGM state. No privilege escalation, service/sysctl/driver change, internal interception, reparenting, MPS daemon, or SCG operation occurred.

- Driver `580.95.05`; toolkit module `cuda/12.8.1`, `nvcc V12.8.93`; `g++ 11.5.0`.
- Nsight `2025.6.3.541` selected by absolute path from `cuda/13.2.1`, independently of the restored build toolkit.
- Nine frozen probe source/build files unchanged; existing Makefile `-O3`, C++17, `sm_80`. Build succeeded with the existing unused `chosen_index` warning.
- CUDA elements `1024` and `262144`, intensity `100`; deterministic synthetic arithmetic, not a model. The grids contain four and 1,024 blocks of 256 threads; grid size is not measured occupancy/headroom.
- Native GL triangle at `128×128 RGBA8`. Each application invocation executes solo GL, solo CUDA, serial interleaving, and independent host-thread submission.
- Five unprofiled repetitions per size: `500` iterations, `50` warmup. Separate profiles: `100` iterations, `20` warmup.
- Common trace flags: `--trace=cuda,opengl,osrt,nvtx --opengl-gpu-workload=true --sample=none --cpuctxsw=none`.
- Instrumented captures additionally use `--gpuctxsw=true --gpu-metrics-devices=cuda-visible --gpu-metrics-set=ga100 --gpu-metrics-frequency=1000`.
- Unique report/export names, matching newer exporter, no overwrite. Four SQLite integrity checks returned `ok`; remote/local hashes matched all 36 result files plus the executed native binary.

All fourteen application invocations logged initial EGL pixel and sampled CUDA checks plus registration/map success. This does **not** verify final serial/concurrent outputs or CUDA-side consumption of mapped pixels. The frozen probe retains its historical EXP-002 banner and unsupported throughput-to-TSG/spatial-sharing heuristic labels; those labels are rejected.

## 3. Actual trace contents

Every export contains 341 CUDA kernel records and six NVTX ranges. API-only captures intentionally disabled GPU switch/metric collection.

| CUDA elements | Collection | GL batches | GPU switch rows | Metric rows | Distinct metric timestamps |
| --- | --- | --- | --- | --- | --- |
| 1,024 | API only | 654 | 0 | 0 | 0 |
| 1,024 | Scheduling + metrics | 666 | 862 | 15,730 | 715 |
| 262,144 | API only | 654 | 0 | 0 | 0 |
| 262,144 | Scheduling + metrics | 662 | 874 | 17,138 | 779 |

Both instrumented exports define 22 GA100 metrics and record `COLLECT_GPU_CTX_SW_TRACE=true`. Their concurrent parent ranges contain two scheduling IDs. Independently paired `RESTORE_START`/`SAVE_END` brackets do not overlap in the recorded streams. This supports a serialized restore/save pattern, **not** exact active-warp intervals, proven GL/CUDA TSG isolation, or a universal scheduling restriction. CUPTI/GL API context IDs and scheduling IDs are different identifier domains; no RM TSG handle mapping is available.

Do not attribute gaps between save and restore events solely to hardware switching overhead. Queue/host idle time and instrumentation can contribute. Worker NVTX ranges include setup/readiness waits; overlapping worker ranges do not prove continuous GPU-ready work. Aggregate metrics also lack per-context active-warp attribution. Numeric counter scaling was not decoded, so no utilization or headroom percentage is claimed.

### Diagnostics that remain relevant

All four captures warn:

```text
Scheduling information is absent. The thread activity is deduced based on OS runtime libraries traces. This is inaccurate and does not take into account asynchronous interrupts and exception faults.
```

This limits CPU thread-state interpretation under `--cpuctxsw=none`; it does not mean GPU switch collection failed.

All four also state:

```text
CUDA hardware tracing is not supported on this system. A legacy (software instrumented) trace was collected instead.
```

The actual CUDA backend was software-instrumented. Do not invent a permission cause or claim hardware warp tracing. The earlier unsupported CUDA-driver-version warning is absent from all four diagnostic tables. Successful collection is bounded to this recorded stack; it does not certify every driver/tool/GPU combination or remove instrumentation effects.

#### CUDA hardware tracing prerequisites and 2025.6 CLI behavior

The [versioned Nsight Systems 2025.6 User Guide](https://archive.docs.nvidia.com/nsight-systems/2025.6/UserGuide/index.html#cli-profile-command-switch-options) states: “The `cuda` option uses the Hardware Event System which is available for GPUs beginning with Blackwell.” Its [CUDA Trace section](https://archive.docs.nvidia.com/nsight-systems/2025.6/UserGuide/index.html#cuda-trace) also excludes MPS workloads, MIG partitions, and vGPU environments from this backend.

**Why this allocation fell back:** A100 is Ampere, not Blackwell. The documented architecture requirement is already unmet. No command-line flag, root privilege, or CUDA toolkit upgrade can add that backend to this A100. This finding does not imply that every other prerequisite would pass on another GPU.

**How to try on a supported target:** use the verified Nsight executable on an authorized Blackwell-or-later allocation with a compatible driver and supported environment. The following is a command template, not an executed Blackwell run:

```bash
"${NSYS_BIN:?Select and verify Nsight Systems 2025.6 first}" profile \
  --trace=cuda,nvtx --sample=none --cpuctxsw=none \
  --force-overwrite=false --output=hw_trace_trial \
  ./your_cuda_application
```

Use a fresh output name. `--trace=cuda` selects the hardware backend where supported, with possible software fallback; inspect the report's **Diagnostics Summary** to verify the backend actually collected. EXP-007 already used that automatic selector and recorded fallback. The captured installed `2025.6.3.541` help advertises `cuda` and `cuda-sw`, not `cuda-hw`; do not transplant an older release's flag. `--trace=cuda-sw` explicitly selects legacy software tracing.

HES changes CUDA timestamp collection overhead, not whether CUDA executes on the GPU. Software-instrumented tracing still records GPU workload activity. HES is distinct from GPU switch tracing, GPU metric sampling, and per-warp analysis; it does not alone prove simultaneous native GL/CUDA spatial execution. No new allocation or driver/service change was needed for this source-based explanation.

## 4. Fixed-count unprofiled timings

Each entry aggregates all five repetitions, without discarding slower first runs. Values come from rounded application wall-time output, not active GPU execution time.

| CUDA elements | Mode | Median wall time (ms) | Observed range (ms) |
| --- | --- | --- | --- |
| 1,024 | Solo GL | 12.88 | 12.85–19.85 |
| 1,024 | Solo CUDA | 5.47 | 5.46–7.85 |
| 1,024 | Serial | 172.03 | 171.81–241.66 |
| 1,024 | Independent threads | 152.37 | 150.72–153.28 |
| 262,144 | Solo GL | 12.97 | 12.83–19.76 |
| 262,144 | Solo CUDA | 10.79 | 10.77–17.47 |
| 262,144 | Serial | 177.16 | 177.12–217.32 |
| 262,144 | Independent threads | 168.67 | 168.01–169.45 |

Independent submission improves wall time relative to serial interleaving in these runs, but remains much slower than the solo-time sum. Neither observation identifies the unique cause or establishes spatial execution. These are fixed-count smoke repetitions, not equal-duration steady-state characterization. CUDA latency is not independently reported in the concurrent result.

## 5. Decision and remaining work

The profiler compatibility/capability smoke is now exercised, including physical GPU correlation, GPU switch events, metric records, and successful fresh exports. **Gate 4 remains partial:** complete all-mode output checks, independent compute latency, equal-duration steady-state and matched cross-process controls, measured resource availability, and authoritative scheduling-ID/engine attribution before stronger causal claims. Representative MuJoCo rendering and real model work remain absent.

No VUDA shim, native GL redirection, resource partitioning, LIBERO pipeline, or inference/training benchmark was implemented. The separate author-artifact/control-mapping and site-permission gates for internal driver work remain blocked. See [current status](../../research/STATUS.md), [capture protocol](../../research/GATE4_CAPTURE_PROTOCOL.md), and [Gate 5 assessment](../../research/GATE5_ASSESSMENT.md).
