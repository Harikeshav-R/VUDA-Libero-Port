# EXP-002: Standalone Native C++/CUDA/EGL Correctness & Benchmark Probe Results

## 1. Provenance and Scope
- **Experiment ID:** EXP-002
- **Execution Date:** 2026-11-06 (UTC 2026-11-07 02:12)
- **Target Testbed:** Ohio Supercomputer Center (OSC) Ascend Cluster (interim temporary preliminary testbed awaiting host lab EPFL GPU allocation)
- **Execution Context:** Slurm batch job `8374921` on compute node `NODE_A` (A100-PCIE-40GB)
- **Source Code:** `src/probe/` (`egl_renderer.cpp`, `cuda_compute.cu`, `benchmark_runner.cpp`, `main.cpp`)

---

## 2. Hardware and Environment Observations
- **GPU Accelerator:** NVIDIA A100-PCIE-40GB (Compute Capability 8.0, 108 SMs, 40,441 MiB device memory, ECC enabled, MIG disabled)
- **Driver & Firmware:** NVIDIA Driver `580.95.05`, GSP firmware `580.95.05`, Linux kernel `5.14.0-570.154.1.el9_6.x86_64`
- **EGL Version:** 1.5 (EGL Device Extension matched target CUDA device ordinal 0 via `EGL_CUDA_DEVICE_NV`)
- **OpenGL Context:** OpenGL Core Profile 3.3.0 (Vendor: NVIDIA Corporation, Renderer: NVIDIA A100-PCIE-40GB/PCIe/SSE2)
- **FBO Configuration:** 128×128 RGBA8 color renderbuffer with 24-bit depth renderbuffer (matching LIBERO camera resolution)
- **CUDA Configuration:** CUDA 12.8.1 runtime (`nvcc V12.8.93`), compute grid of 262,144 elements with loop intensity 100

---

## 3. Correctness and Interop Registration/Mapping
1. **EGL Offscreen Rendering:**
   - Shaders compiled and linked successfully.
   - Colored geometric scene rendered to FBO.
   - Pixel readback via `glReadPixels` checked background RGB(51, 102, 153) within tolerance 3 and foreground RGB(204, 51, 76) within tolerance 4: **PASSED**.
2. **CUDA Compute Correctness:**
   - Kernel launched on stream and synchronized.
   - Output buffer copied and numerically verified against CPU evaluation across sample indices: **PASSED**.
3. **CUDA-OpenGL Device-Resident Interop (Gate 6 Prerequisite):**
   - `cudaGraphicsGLRegisterImage` on FBO color renderbuffer: **SUCCESS**.
   - `cudaGraphicsMapResources` and `cudaGraphicsSubResourceGetMappedArray` returned a `cudaArray_t`: **SUCCESS**.
   - The log reports successful unmap/unregistration; the probe does not check those calls' return statuses.
   - **Finding:** Registration/mapping capability is demonstrated. The probe does not read mapped pixels in CUDA, verify tensor handoff, or establish a copy-free implementation.

---

## 4. Empirical Benchmark Measurements (500 Iterations + 50 Warmup)

| Mode | Total Time (ms) | Mean Latency (μs) | p50 Latency (μs) | p99 Latency (μs) | Measured Throughput |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Solo EGL Render** | 20.04 ms | 40.0 μs | 26.9 μs | 187.0 μs | 24,950.7 fps |
| **Solo CUDA Compute** | 14.27 ms | 28.5 μs | 28.4 μs | 34.7 μs | 35,028.1 ips |
| **Serial Interleaved (Render → Compute)** | 230.38 ms | 460.7 μs | 489.6 μs | 583.9 μs | 2,170.4 pairs/s |
| **Concurrent Multi-Threaded (Thread 1 Render, Thread 2 Compute)** | 92.27 ms | 163.5 μs | 20.9 μs | 335.2 μs | 5,419.0 pairs/s |

---

## 5. Architectural Analysis & Interpretation

### Concurrency Metrics
- **Theoretical Serial Sum (Solo Render + Solo Compute):** 34.31 ms
- **Observed Serial Interleaved:** 230.38 ms
- **Observed Concurrent Multi-Threaded:** 92.27 ms
- **Speedup vs. Serial Interleaved:** **2.497×**
- **Speedup vs. Theoretical Serial Sum:** **0.372×**

### Interpretation corrected during Gate 5 review
1. **Observed overhead:** Serial alternation takes about 6.7× the independently measured solo sum. Context switching, synchronization, submission overhead, and interference are candidate contributors; this measurement does not isolate their costs.
2. **Observed submission advantage:** Concurrent host threads improve pair throughput over serial alternation in this single unprofiled run. This does not identify whether the gain is host-side, device-side, or both.
3. **Scheduling remains inconclusive:** The 0.372× ratio is solo sum divided by concurrent wall time, not evidence that concurrent time is shorter. Wall times cannot reveal TSG identities or simultaneous active SM execution.
4. **Workload limits:** Rendering clears/draws one triangle and calls `glFinish()` each frame. CUDA launches 1,024 blocks of 256 threads; resource headroom is not measured. Concurrent latency columns describe the rendering thread, while throughput uses whole-run wall time. No five-repetition steady-state study or cross-process baseline was performed.
5. **Gate 5 implication:** These observations justify further investigation, not a claim that VUDA is necessary or compatible. See the [Gate 5 assessment](../../research/GATE5_ASSESSMENT.md) for evidence limits and next prerequisites.

Raw execution output remains unchanged. This public derivative corrects interpretation without claiming a new GPU experiment.
