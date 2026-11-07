# VUDA–OpenGL/LIBERO Research

Research into simultaneous native EGL/OpenGL rendering and CUDA model computation on one NVIDIA GPU, using VUDA's design as a starting point for LIBERO.

**Target Environment:** The permanent research environment and host lab for this project is **EPFL**. All exploration, allocations, and preliminary setup on Ohio Supercomputer Center (OSC) clusters (Ascend, Cardinal, Pitzer) are strictly **temporary preliminary testbeds** while awaiting EPFL GPU access.

**Current state:** Native EGL/CUDA probes and Nsight traces exist for the OSC A100 temporary testbed. Gates 1–3 pass within their recorded scope; Gate 4 is partial because timing ranges do not establish TSG membership or active spatial execution. Gate 5's [technical and policy assessment](research/GATE5_ASSESSMENT.md) is recorded, but runtime channel redirection remains blocked on implementation details and site approval. Gate 6 verifies registration/mapping only, not CUDA-side image consumption or zero-copy implementation.

## Start here

- [Current status and next steps](research/STATUS.md)
- [Research ledger: sources, findings, decisions, questions, experiments](research/RESEARCH_LOG.md)
- [OSC setup and operating commands](docs/OSC_RUNBOOK.md)
- [Agent context and working instructions](AGENTS.md)
- [Gate 5 decision, trace audit, and unsent site inquiry](research/GATE5_ASSESSMENT.md)
- [Version-specific NVIDIA driver assessment](research/GATE5_DRIVER_ASSESSMENT.md)
- [Sanitized Ascend inventory and CUDA toolchain evidence](evidence/public/ascend-inventory.md)
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
research/
  STATUS.md
  RESEARCH_LOG.md
  GATE5_ASSESSMENT.md
  GATE5_DRIVER_ASSESSMENT.md
  reference/
    VUDA_OPENGL_LIBERO_PLAN.md
    VUDA_SESSION_NOTES.md
    SAVE_RESEARCH_ARTIFACTS_PLAN.md
scripts/
  pitzer-diagnostics.sh
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

## Public preservation checks

Run from the repository root:

```bash
shasum -a 256 -c evidence/PUBLIC_SHA256SUMS
bash -n scripts/pitzer-diagnostics.sh
```

These checks validate published derivatives and shell syntax, not GPU functionality. Native probe build files exist in `src/probe/`; no automated application test suite exists. This Gate 5 review changes research records only and does not rerun the GPU probe.

## Private originals

The original workspace retains raw OSC captures, mentor screenshots, an export ZIP, and the supplied coursework notebook. They are intentionally excluded from this public repository through `.gitignore`:

- `research/archive/`
- `evidence/osc/`
- `evidence/reference/`
- `evidence/SHA256SUMS`

A fresh clone will not contain those originals. Public findings are available through the sanitized evidence summary and research references. New captures require privacy review before publication; preserve originals privately and publish reviewed derivatives only.
