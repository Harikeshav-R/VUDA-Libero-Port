# EXP-003: Nsight Systems Timeline Capture and Scheduling Evidence Limits

## 1. Provenance and Scope
- **Experiment ID:** EXP-003
- **Execution Date:** 2026-11-06 (UTC 2026-11-07 02:58)
- **Target Testbed:** Ohio Supercomputer Center (OSC) Ascend Cluster (interim temporary preliminary testbed awaiting host lab EPFL GPU allocation)
- **Execution Context:** Slurm batch job `8375678` on compute node `NODE_A` (NVIDIA A100-PCIE-40GB, driver `580.95.05`, CUDA `12.8.1`)
- **Profiling Tool:** NVIDIA Nsight Systems `2024.7.1.84` (`nvhpc/25.1`), invoked with `--gres=nsight`
- **Application Binary:** `src/probe/vuda_native_probe` (built with NVTX timeline annotations)

---

## 2. Profiling Methodology & Trace Configuration
The probe executable was profiled under Nsight Systems using the supported multi-API tracing command template:
```bash
nsys profile \
    --trace=cuda,opengl,osrt,nvtx \
    --opengl-gpu-workload=true \
    --sample=cpu \
    --output=vuda_exp003_profile_8375678 \
    ./vuda_native_probe --iterations 100 --warmup 20 --intensity 100 --elements 262144
```

The preserved SQLite export (`vuda_exp003_profile_8375678.sqlite`) contains:
- `CUPTI_ACTIVITY_KIND_KERNEL`: 341 kernel execution events with microsecond start and end timestamps.
- `OPENGL_WORKLOAD`: 666 GPU-side graphics workload events (framebuffer bindings, clears, and readbacks).
- `NVTX_EVENTS`: Annotated execution phases (`Solo_EGL_Render`, `Solo_CUDA_Compute`, `Serial_Interleaved`, `Concurrent_Multi_Threaded`).

The execution log ends with `Exportation error: File already exists` and exit code 1 after report generation. The database exists and its phase timings match the report's logged run, but fresh export correspondence has not been established. Do not describe the entire profiling/export job as successful.

---

## 3. Quantitative Profiling Measurements

### A. CUDA GPU Kernel Execution Summary
- **Target Kernel:** `compute_workload_kernel(float*, const float*, unsigned long, int)`
- **Total Executions:** 341 instances
- **Total GPU Time:** 8.704 ms (97.7% of all CUDA GPU activity)
- **Mean Kernel Duration:** 25.53 μs (std dev: 2.22 μs)
- **Median Kernel Duration:** 26.50 μs (min: 20.54 μs, max: 27.20 μs)
- **Memory Operations:** Two D2H transfers (67.1 μs each) and one H2D transfer (71.6 μs)

### B. OpenGL Workload Execution Summary
- **Total OpenGL Workloads:** 666 events
- **Selected solo GL batch ranges labeled `glClear`:** 33.1 μs – 35.5 μs.
- **Selected concurrent GL batch ranges labeled `glClear`:** 607.6 μs – 668.7 μs.
- **Selected ranges labeled `glBindFramebuffer`:** 1.98 μs – 3.97 μs.

These selected examples are not all-event minima/maxima or isolated operation durations. The [Gate 5 offline audit](../../research/GATE5_ASSESSMENT.md#1-what-the-existing-experiments-establish) reports phase-filtered batch statistics.

---

## 4. Timestamp Containment, Not Isolated Preemption

The `Concurrent_Multi_Threaded` NVTX window spans approximately 67.93 ms. Offline read-only queries verified these elapsed GL batch ranges containing CUDA kernel intervals:

```text
Timestamp Window (ns)           Duration   Engine  Operation
-----------------------------------------------------------------------------------------------------------------
[1170185281 -> 1170854020]      668.7 μs   GL      glClear
  [1170550306 -> 1170576994]     26.7 μs   CUDA    compute_workload_kernel(float*, const float*, unsigned long, int)
[1170856036 -> 1170858020]        2.0 μs   GL      glBindFramebuffer
[1170860004 -> 1171468454]      608.5 μs   GL      glClear
  [1171165029 -> 1171191621]     26.6 μs   CUDA    compute_workload_kernel(float*, const float*, unsigned long, int)
[1171470438 -> 1171472422]        2.0 μs   GL      glBindFramebuffer
[1171474406 -> 1172082665]      608.3 μs   GL      glClear
  [1171779336 -> 1171805928]     26.6 μs   CUDA    compute_workload_kernel(float*, const float*, unsigned long, int)
[1172084745 -> 1172086761]        2.0 μs   GL      glBindFramebuffer
[1172088745 -> 1172697068]      608.3 μs   GL      glClear
  [1172393674 -> 1172420266]     26.6 μs   CUDA    compute_workload_kernel(float*, const float*, unsigned long, int)
```

### Observations and limits
1. **Selected elapsed ranges inflate:** A roughly 608 μs GL batch versus a selected 33 μs solo batch yields about 18.4×. This is not a phase-wide mean ratio.
2. **Containment is real; suspension is unproved:** CUDA kernel intervals occur inside the elapsed GL ranges. Those timestamps do not reveal whether graphics actively progressed during the contained interval.
3. **Profiler semantics matter:** NVIDIA's [Nsight Systems 2024.7 OpenGL Trace documentation](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html#opengl-trace) says GPU ranges time batches using `glQueryCounter()`. A range named `glClear` need not isolate a clear from subsequent triangle rendering or waiting.

---

## 5. Corrected Gate 4 Verdict

**Partial: GPU/API timeline capture established; active co-execution and TSG root cause inconclusive.** This supersedes earlier claims that these ranges prove TSG isolation, graphics suspension, or the necessity of VUDA.

The SQLite export has neither `GPU_CONTEXT_SWITCH_EVENTS` nor `GPU_METRICS`. `ENUM_GPU_CTX_SWITCH` is an enumeration, not recorded context-switch evidence; `SCHED_EVENTS` concerns CPU scheduling. No GL/CUDA TSG allocation mapping was captured.

The profiled run records 64.45 ms serial and 67.92 ms concurrent wall time, unlike EXP-002's unprofiled concurrent advantage. Keep profiler overhead and configuration differences separate from performance conclusions.

Further work needs supported scheduling/activity correlation, actual allocation identities, verified outputs, bounded low-resource kernels, and repeated unprofiled baselines. No privileged profiling workaround is authorized. Raw logs and traces are unchanged; this correction is interpretation, not a new GPU run.
