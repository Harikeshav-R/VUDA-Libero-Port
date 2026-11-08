# VUDA–OpenGL/LIBERO Research

Research into simultaneous native EGL/OpenGL rendering and CUDA model computation on one NVIDIA GPU, using VUDA's design as a starting point for LIBERO.

**Target Environment:** The permanent research environment and host lab for this project is **EPFL**. All exploration, allocations, and preliminary setup on Ohio Supercomputer Center (OSC) clusters (Ascend, Cardinal, Pitzer) are strictly **temporary preliminary testbeds** while awaiting EPFL GPU access.

**Current state:** Native EGL/CUDA probes and Nsight traces exist for the OSC A100 temporary testbed. Gates 1–3 pass within their recorded scope. Gate 4 is closed with a conclusive negative result for standard runtime APIs: EXP-009 executed all 60 cells of the controlled scheduling matrix and 12 separate Nsight profiles on an A100 GPU (job `8401856`, elapsed 24:35). Massive GPU context switching (~32k/2s) and ~12x latency inflation (from ~27 μs GL / ~10.6 μs CUDA to ~326–337 μs) prove the GPU hardware engine actively time-slices native OpenGL and CUDA rather than executing them spatially. Gate 5's [technical and policy assessment](research/GATE5_ASSESSMENT.md) is recorded, but runtime channel redirection remains blocked on implementation details and site approval. Gate 6 verifies registration/mapping only, not CUDA-side image consumption or zero-copy implementation.

- [Start here: EXP-009 controlled scheduling implementation and execution plan](research/EXP009_CONTROLLED_SCHEDULING_PLAN.md)
- [EXP-009 controlled scheduling results and telemetry audit](evidence/public/exp009-controlled-scheduling.md)
- [Current status and next steps](research/STATUS.md)
- [Research ledger: sources, findings, decisions, questions, experiments](research/RESEARCH_LOG.md)
- [OSC setup and operating commands](docs/OSC_RUNBOOK.md)
- [Agent context and working instructions](AGENTS.md)
- [Gate 5 decision, trace audit, and unsent site inquiry](research/GATE5_ASSESSMENT.md)
- [Version-specific NVIDIA driver assessment](research/GATE5_DRIVER_ASSESSMENT.md)
- [Gate 5 observer decoding and transparency review](research/GATE5_OBSERVER_REVIEW.md)
- [Gate 4 supported capture protocol and proof limits](research/GATE4_CAPTURE_PROTOCOL.md)
- [Sanitized Ascend inventory and CUDA toolchain evidence](evidence/public/ascend-inventory.md)
- [Live profiler discovery, report re-export, and allocated preflight](evidence/public/exp006-live-prerequisites.md)
- [Completed native profiler smoke, scheduling evidence, and timing limits](evidence/public/exp007-profile-results.md)
- [Final-output rejection and independent CUDA latency](evidence/public/exp008-correctness-latency.md)
- [Evidence provenance and publication boundaries](evidence/PROVENANCE.md)

## Public layout

```text
AGENTS.md
README.md
docs/
  OSC_RUNBOOK.md
evidence/
  PROVENANCE.md
  PUBLIC_SHA256SUMS
  public/
    ascend-inventory.md
    exp002-probe-results.md
    exp003-profile-results.md
    exp005-trace-metadata-audit.json
    exp006-live-prerequisites.md
    exp007-profile-results.md
    exp007-profile-audit.json
    exp008-correctness-latency.md
    exp008-correctness-latency-audit.json
src/
  probe/
    CMakeLists.txt
    Makefile
    main.cpp
    egl_renderer.h
    egl_renderer.cpp
    cuda_compute.h
    cuda_compute.cu
    benchmark_runner.h
    benchmark_runner.cpp
    controlled_runner.h
    controlled_runner.cpp
    measurement_window.h
    worker_wire_protocol.h
    operation_trace.h
    nvtx_wrapper.h
    device_identity_guard.h
    device_identity_guard.cpp
  STATUS.md
  RESEARCH_LOG.md
  EXP009_CONTROLLED_SCHEDULING_PLAN.md
  GATE5_ASSESSMENT.md
  GATE5_DRIVER_ASSESSMENT.md
  GATE5_OBSERVER_REVIEW.md
  GATE4_CAPTURE_PROTOCOL.md
  reference/
    VUDA_OPENGL_LIBERO_PLAN.md
    VUDA_SESSION_NOTES.md
    SAVE_RESEARCH_ARTIFACTS_PLAN.md
scripts/
  pitzer-diagnostics.sh
  run-controlled-scheduling.py
```

Public research references are labeled derivatives of the original research/export records. They retain source findings and citations while omitting private attachments and workstation paths. Current status and the ledger supersede historical session status.

## Latest evidence

Under Slurm job `8374921` on the OSC Ascend temporary testbed (node `NODE_A`, NVIDIA A100-PCIE-40GB, driver `580.95.05`, CUDA `12.8.1`), the standalone native probe (`EXP-002`) established:
- **Solo EGL Render (128×128 RGBA8):** 20.04 ms (40.0 μs mean, 24,950.7 fps). Output verified via `glReadPixels`.
- **Solo CUDA Compute:** 14.27 ms (28.5 μs mean, 35,028.1 ips). Numerical accuracy verified on host CPU.
- **Serial Interleaved (Render → Compute):** 230.38 ms (460.7 μs mean, 2,170.4 pairs/s), about 6.7× the solo wall-time sum.
- **Concurrent Multi-Threaded:** 92.27 ms (163.5 μs rendering-thread mean, 5,419.0 pairs/s), yielding a **2.497× speedup** over serial interleaved execution in this run.
- **Interop capability:** Renderbuffer registration, mapping, and `cudaArray_t` retrieval succeeded. Mapped-pixel consumption and copy-free behavior were not checked.
- **Interpretation:** The solo-sum/concurrent-time ratio is 0.372×. Slowdown and throughput gains do not identify TSG membership or prove hardware time-sharing.

Detailed findings are documented in [the sanitized EXP-002 results](evidence/public/exp002-probe-results.md).

Under Slurm job `8375678` with `--gres=nsight`, profiling (`EXP-003`) captured 341 CUDA kernels and 666 OpenGL workload batches:
- **Elapsed-range inflation:** Selected GL batches labeled `glClear` span roughly 608 μs versus selected solo ranges around 34 μs.
- **Timestamp containment:** CUDA intervals fall within those elapsed GL batch ranges. This does not prove graphics suspension or active spatial overlap.
- **Limits:** No GPU context-switch events or GPU metrics table exists in the preserved SQLite export. The log ends with an existing-file export error. NVIDIA's batch-timing semantics require a narrower conclusion: scheduling remains inconclusive.

Detailed hardware traces and analysis are documented in [the sanitized EXP-003 profile results](evidence/public/exp003-profile-results.md).

The EXP-005 offline metadata audit found that GPU context-switch capture was disabled and that Nsight warned of unsupported CUDA-driver compatibility version `13.0`, falling back to `12.6` tracing libraries. It also recorded CPU sampling throttling. See [sanitized query results](evidence/public/exp005-trace-metadata-audit.json). No new GPU run was performed; profiler compatibility must be resolved before a decisive capture.

EXP-006 connected to OSC, found Nsight `2025.6.3.541` inside `cuda/13.2.1`, and retained toolkit `12.8.1` through explicit profiler-path selection. Fresh export verified all 91 schemas and 90 non-export-metadata table contents against the preserved EXP-003 database. User-authorized GPU preflight `8389776` completed successfully on an A100; help/status discovery is not application-tracing proof. See [live findings](evidence/public/exp006-live-prerequisites.md).

EXP-007 completed the native profiling smoke with the newer profiler while preserving toolkit `12.8.1`. Physical CUDA/EGL/NVML/profiler identities matched; all recorded commands succeeded, and instrumented captures contain GPU switch and metric records. Restore/save brackets do not establish active spatial sharing or RM TSG membership. See [actual results, diagnostics, and fixed-count timing limits](evidence/public/exp007-profile-results.md).

EXP-008 adds fatal final-output checks, worker error propagation, and separate concurrent CUDA latency reporting. Its [experiment record](evidence/public/exp008-correctness-latency.md) distinguishes launcher failures, source review, actual target verification, and remaining Gate 4 controls.

EXP-009 implemented controlled scheduling with required `--mode`, timed serial/concurrent thread/process controls, 5s warmup and 15s measurement windows across a 60-window matrix, strict per-iteration validation, and tagged iterations. Job `8401856` completed on target in 24:35, verifying builds (Make and CMake), 13 fault rejections, 11 CLI parameter rejections, 12 per-iteration validation runs, closed-pipe worker rejection, and a 12-cell timed smoke run. All 60 matrix cells were verified 60/60 via raw binary timestamp replay (`EXP9TS01_LE`). All 12 separate Nsight profiling captures and SQLite exports succeeded, demonstrating ~32k context switches per 2s window. See [sanitized findings](evidence/public/exp009-controlled-scheduling.md) and [machine audit](evidence/public/exp009-controlled-scheduling-audit.json).
## Public preservation checks

Run from the repository root:

```bash
shasum -a 256 -c evidence/PUBLIC_SHA256SUMS
bash -n scripts/pitzer-diagnostics.sh
```

These checks validate published derivatives and shell syntax, not GPU functionality. No public automated application test suite exists. EXP-008 separately compiled original/updated probes on an authorized GPU allocation, exercised eight failure-rejection scenarios, and completed ten normal invocations with final checks and independent concurrent latency. Those bounded checks do not establish per-iteration freshness or complete Gate 4 validation.

## Private originals

The original workspace retains raw OSC captures, mentor screenshots, an export ZIP, and the supplied coursework notebook. They are intentionally excluded from this public repository through `.gitignore`:

- `research/archive/`
- `evidence/osc/`
- `evidence/reference/`
- `evidence/SHA256SUMS`

A fresh clone will not contain those originals. Public findings are available through the sanitized evidence summary and research references. New captures require privacy review before publication; preserve originals privately and publish reviewed derivatives only.
