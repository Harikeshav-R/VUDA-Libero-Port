# Research Log & Decisions Register: VUDA-Libero-Port

## 1. Project Decisions Register

This register documents key architectural, methodological, and operational decisions governing the project.

| ID | Date | Decision Summary | Rationale & Impact | Status |
| :--- | :--- | :--- | :--- | :--- |
| **DEC-001** | 2026-11-06 | **Preserve Native OpenGL/EGL as Target Backend** | The mentor brief specifically requires investigating native OpenGL/EGL for LIBERO (via MuJoCo/robosuite). While replacing MuJoCo's renderer with Filament/Vulkan would keep physics on MuJoCo, it introduces visual appearance divergence and changes the research question into renderer migration rather than native OpenGL portability. (In contrast, ManiSkill changes physics and rendering together by using PhysX with Vulkan.) | **Accepted** |
| **DEC-002** | 2026-11-06 | **Distinguish Baseline EGL from Spatial Sharing** | Enabling standard EGL offscreen rendering on the GPU is already supported by LIBERO and serves as the performance baseline. The research objective is spatial hardware sharing (simultaneous SM/TPC utilization), not time-sliced co-location. | **Accepted** |
| **DEC-003** | 2026-11-06 | **Decouple Scheduling, Transfer, and Storage** | The investigation separates: (1) *Scheduling* (TSG grouping and hardware overlap), (2) *Transfer* (`cudaGraphicsMapResources` device-resident handoff vs. CPU readback), and (3) *Storage* (VRAM allocation footprint). Avoiding a CPU host roundtrip does not guarantee zero-copy transfer or concurrent execution, and vice versa. | **Accepted** |
| **DEC-004** | 2026-11-06 | **Standard API Probe Precedes Low-Level Shims** | Before attempting undocumented `ioctl` interception, shadow channels, or TSG parenting, we must measure and profile unmodified OpenGL and CUDA co-execution under standard Linux drivers to establish empirical ground truth. | **Accepted** |
| **DEC-005** | 2026-11-06 | **Strict Cluster Safety & Policy Compliance** | All cluster operations must strictly adhere to OSC multi-tenant policies. No kernel module modifications, no driver reloading, no MPS daemons without prior permission, and all Slurm allocations must use placeholder account `YOUR_OSC_ACCOUNT` (without angle brackets) rather than unapproved course accounts. | **Accepted** |
| **DEC-006** | 2026-11-06 | **Sanitized Public Research Workspace & Derivative Distribution** | The user explicitly approved publishing the research workspace to a public GitHub repository under a sanitized layout. Raw cluster captures, local screenshots, reference archives, and course materials remain local-only (excluded by `.gitignore` in `research/archive/`, `evidence/osc/`, `evidence/reference/`, and `evidence/SHA256SUMS`). Public tracking relies on reviewed sanitized derivatives in `research/reference/` and `evidence/public/`, with redaction of private account codes, hostnames, usernames, and GPU UUIDs. Authorization permits normal git commits and push (no force push). | **Accepted** |
| **DEC-007** | 2026-11-06 | **OSC Ascend as Temporary Preliminary Testbed Pending EPFL Host Lab GPU Access** | The permanent target research environment and host lab for this project is EPFL. Exploration, allocations, and preliminary environment setup on OSC clusters (Ascend A100, Cardinal H100, Pitzer V100) serve strictly as temporary preliminary testbeds to de-risk baseline execution, toolchains, and initial probes while awaiting EPFL GPU provisioning. All OSC artifacts and setups are treated as disposable preliminary scaffolding. | **Accepted** |
| **DEC-008** | 2026-11-06 | **Gate 5 assessment does not authorize mutation** | Published `580.95.05` source has TSG/context-share duplication and GSP RPC paths, but actual GL allocations, shared control mappings, complete artifact/design, and site approval remain missing. Proceed with offline research; runtime observation and mutation require separate reviewed scopes. No OSC blanket ban is inferred. | **Accepted** |
| **DEC-009** | 2026-11-06 | **Correct Gate 4 evidence standard** | GL batch elapsed times and wall-time slowdown do not prove TSG membership, preemption, or absence of active co-execution. Preserve raw captures and correct public interpretations. Gate 4 is partial/inconclusive; registration/mapping alone is not zero-copy image consumption. | **Accepted** |

---

## 2. Primary Sources Register (S1–S27)

The historical S1–S23 register remains anchored in the [technical plan](reference/VUDA_OPENGL_LIBERO_PLAN.md). Gate 5 freshly reviewed S1 §§4–7.1, S19, and added S24–S27 below. Historical reviews are not represented as fresh verification.

### Group A: The VUDA Papers
- **S1 — VUDA v2 (Controlled Spatial Sharing):** [arXiv:2605.01352v2](https://arxiv.org/html/2605.01352v2) (September 26, 2026).
  *Key Findings:* Operates on Linux with native Vulkan and CUDA. Identifies separate Timeslice Groups (TSGs) as the root cause of engine time-slicing. Implements user-space `ioctl` interception via `LD_PRELOAD`, duplicates graphics TSG handles into CUDA client, parents CUDA channels to the graphics TSG, and uses an undocumented shadow channel for dynamic SCG (Simultaneous Compute and Graphics) TPC quota control. Evaluated on RTX 4090, A800-SXM4, and Jetson AGX Orin.
- **S2 — VUDA v1 (Breaking CUDA-Vulkan Isolation):** [arXiv:2605.01352v1](https://arxiv.org/html/2605.01352v1) (May 2, 2026).
  *Key Findings:* Earlier forwarding-channel and kernel-module design. Demonstrates simulation rollout throughput gains; however, §6.4.2 (OpenVLA-OFT) placed VLA inference on a separate RTX 6000 Pro GPU, overlapping only simulation physics and rendering on the primary GPU.

### Group B: LIBERO, robosuite, and MuJoCo Observation Pipeline
- **S3 — LIBERO Upstream Dependencies:** [LIBERO requirements.txt](https://raw.githubusercontent.com/Lifelong-Robot-Learning/LIBERO/master/requirements.txt). Pins `robosuite==1.4.0`.
- **S4 — robosuite Upstream Dependencies:** [robosuite setup.py](https://raw.githubusercontent.com/ARISE-Initiative/robosuite/v1.4.0/setup.py). Pins `mujoco>=2.3.0`.
- **S5 — LIBERO Environment Wrapper:** [env_wrapper.py](https://raw.githubusercontent.com/Lifelong-Robot-Learning/LIBERO/master/libero/libero/envs/env_wrapper.py). Defines `ControlEnv` and `OffScreenRenderEnv`, setting default camera views (`agentview`, `robot0_eye_in_hand`) at provisional 128×128 resolution with a 20 Hz control step.
- **S6 — robosuite Environment Stepping:** [base.py](https://raw.githubusercontent.com/ARISE-Initiative/robosuite/v1.4.0/robosuite/environments/base.py). Implements `MujocoEnv.step` physics forward steps and observable updates.
- **S7 — Camera Sensor Setup:** [robot_env.py](https://raw.githubusercontent.com/ARISE-Initiative/robosuite/v1.4.0/robosuite/environments/robot_env.py). Samples cameras via `sim.render(...)`.
- **S8 — MuJoCo Render and Physics Bindings:** [binding_utils.py](https://raw.githubusercontent.com/ARISE-Initiative/robosuite/v1.4.0/robosuite/utils/binding_utils.py) (Commit `fbee5844ff5632f5b5698e204ec5357ca50be0df`). Manages `MjRenderContext` and invokes `_MjSim_render_lock`, enforcing process-level host serialization across render and pixel readback calls.
- **S10 — robosuite EGL Context Initialization:** [egl_context.py](https://raw.githubusercontent.com/ARISE-Initiative/robosuite/v1.4.0/robosuite/renderers/context/egl_context.py). Calls `eglCreateContext(..., EGL_NO_CONTEXT, ...)`. Creates an unshared EGL context on a cached display.
- **S13 — MuJoCo Asset & Material Semantics:** [MuJoCo XML Reference](https://mujoco.readthedocs.io/en/stable/XMLreference.html#asset-texture). Confirms identical texture assets store a single copy in `mjModel`, disproving assumptions of redundant host texture duplicates.

### Group C: Core Graphics & Interoperability APIs
- **S9 — MuJoCo Headless Rendering Documentation:** [MuJoCo Rendering](https://mujoco.readthedocs.io/en/stable/programming/rendering.html). Documents OSMesa (CPU) vs. EGL (hardware acceleration). Warns that experimental Filament/Vulkan renderer diverges in appearance from classic `mjr`.
- **S11 — NVIDIA CUDA Runtime OpenGL Interop:** [CUDA OpenGL Interop](https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__OPENGL.html). Documents `cudaGraphicsMapResources` / `cudaGraphicsUnmapResources` synchronization contracts, mapped pointer lifecycles, and restrictions on multisampled renderbuffers.

### Group D: RL Scheduling & Distributed Frameworks
- **S14 — RLinf Deployment & Device Locks:** [RLinf Documentation](https://rlinf.readthedocs.io/en/latest/rst_source/examples/embodied/libero.html) and [OSDI '26 Paper](https://www.usenix.org/system/files/osdi26-yu-chao.pdf). Figure 7 and §3.3 detail spatial GPU placement versus temporal scheduling with explicit `device_lock` mechanisms.

### Group E: Profilers & Vendor Architectural Capabilities
- **S15 — NVIDIA Nsight Systems Documentation:** [Nsight Systems User Guide](https://docs.nvidia.com/nsight-systems/UserGuide/index.html). Specifies CLI switches for tracing CUDA, OpenGL, OS runtime events, and GPU context switches; notes that `--opengl-gpu-workload=true` is required for GPU-side graphics metrics.
- **S16 — Supported Vendor Partitioning & Graphics Extensions:** [NVIGI SDK 1.5.0](https://docs.nvidia.com/nvigi-sdk/1.5.0/docs/nvigi_core/docs/GpuSchedulingForAI.html) and [TensorRT-RTX CiG](https://docs.nvidia.com/deeplearning/tensorrt-rtx/latest/inference-library/compute-graphics.html). Documents Compute-in-Graphics (CiG) capabilities, noting official support is limited to DirectX 12 and Vulkan on specific modern architectures.

### Group F: High-Performance Computing Cluster Hardware & Policies
- **S17 — OSC Pitzer Cluster Specifications:** [OSC Pitzer System](https://www.osc.edu/resources/technical_support/supercomputers/pitzer). Heterogeneous nodes featuring NVIDIA Tesla V100 GPUs (16-GB and 32-GB).
- **S18 — OSC Slurm Batch & Interactive Execution:** [OSC Job Submission](https://www.osc.edu/supercomputing/batch-processing-at-osc/job-submission). Documents `salloc` compute-node shell behavior and interactive session handling.
- **S19 — OSC GPU Policy & Profiler Access:** [OSC GPU Computing](https://www.osc.edu/resources/technical_support/supercomputers/gpu_computing) and [NVHPC Module](https://www.osc.edu/resources/available_software/software_list/nvhpc). Documents shared default compute mode and the `--gres=nsight` allocation flag, which is OSC's documented mechanism to mitigate DCGM contention with the profiler (actual counter access remains untested on this node, where `RmProfilingAdminOnly: 0` was observed).
- **S20 — NVIDIA Volta Architecture Specifications:** [CUDA Legacy GPUs](https://developer.nvidia.com/cuda-legacy-gpus). Tesla V100 is compute capability 7.0.
- **S22 — OSC Ascend & Cardinal Cluster Specifications:** [OSC Ascend System](https://www.osc.edu/resources/technical_support/supercomputers/ascend) (NVIDIA A100-PCIE-40GB/80GB, Ampere CC 8.0) and [OSC Cardinal System](https://www.osc.edu/resources/technical_support/supercomputers/cardinal) (NVIDIA H100-PCIE-94GB, Hopper CC 9.0).

### Group G: Artifact Searches & Hardware Graphics Suitability
- **S12 — Third-Party Mock Reference Disclaimer:** [timfox/idTech3 VUDA.md](https://raw.githubusercontent.com/timfox/idTech3/main/docs/VUDA.md). Non-author mock repository modeling TSG redirection; explicitly noted as a non-author implementation.
- **S21 — Bounded Public Artifact Search:** Systematic search of author profiles ([Bin Xu / Xubbbb](https://github.com/Xubbbb), [Jinyu Gu / IPADS](https://ipads.sjtu.edu.cn/pub/members/jinyu_gu)) and GitHub API queries for `"2605.01352"` and `VUDA spatial`. No verified author artifact was located in this bounded search.
- **S23 — Enterprise GPU Graphics Support Evidence:** [NVIDIA Developer Forum: A100 OpenGL Drawing](https://forums.developer.nvidia.com/t/nvidia-a100-opengl-drawing-issue/196672) and [H100 PCIe Graphics Support](https://forums.developer.nvidia.com/t/h100-pcie-doesnt-have-graphic-support/246884). User reports highlight graphics driver nuances and lack of display heads on datacenter accelerators.

### Gate 5 live source review
- **S24 — Nsight Systems 2024.7 trace semantics:** [User Guide](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html), OpenGL Trace, GPU Context Switch, GPU Metrics. OpenGL uses batched `glQueryCounter()` completion timing; device metrics do not identify process/context. This supersedes claims that elapsed-range containment proves preemption.
- **S25 — OSC User Code of Ethics:** [Authorized Use and Appropriate Use](https://www.osc.edu/supercomputing/support/codeofethics). Requires PI-authorized account purpose and system integrity; does not specifically decide TSG/shadow-channel permission.
- **S26 — OSC support:** [Technical Support](https://www.osc.edu/resources/technical_support). Verified `oschelp@osc.edu`; inquiry drafted but not sent.
- **S27 — NVIDIA exact driver source:** [`580.95.05` tree](https://github.com/NVIDIA/open-gpu-kernel-modules/tree/580.95.05), resolved SHA `2b436058a616676ec888ef3814d1db6b2220f2eb`. Verified structures, class headers, constructors, duplication validation and GSP RPC branches are cited in [the driver assessment](GATE5_DRIVER_ASSESSMENT.md). Host source does not establish firmware acceptance or proprietary GL allocation sequence.

---

## 3. Hypotheses & Open Technical Questions

### Active Hypotheses

- **H-01 (OpenGL Timeslice Group Isolation):**
  *Hypothesis:* The proprietary NVIDIA Linux driver assigns headless EGL OpenGL contexts and CUDA contexts to separate Timeslice Groups (TSGs), causing the graphics/compute engine to time-slice between them at coarse granularity rather than executing them simultaneously on distinct SMs.
  *Validation Plan:* Measure execution timelines using Nsight Systems (`nsys profile --trace=cuda,opengl,osrt --opengl-gpu-workload=true`) on a concurrent EGL/CUDA workload. Note that a lack of observed timeline overlap alone does not prove TSG isolation without confirming channel allocation, resource availability, and the absence of application-level serialization barriers.
  *Gate 5 update:* Remains unconfirmed for the recorded GL stack. Existing SQLite has no GPU context-switch event or GPU metrics table; GL batch timestamp containment is inconclusive.

- **H-02 (Open-Kernel Module & GSP Compatibility):**
  *Hypothesis:* GSP/driver validation may constrain the required grouping and control mappings on `580.95.05`; neither compatibility nor impossibility is established.
  *Gate 5 result:* Reviewed core RM/FIFO source, not just DRM/UVM. `kchangrpapiCopyConstruct_IMPL()` and nonlegacy context-share copying explicitly issue GSP duplication RPCs. Actual GL bindings and control-mapping acceptance remain untested. Future runtime tests require site review and a complete design.

- **H-03 (Interoperability Synchronization Serialization):**
  *Hypothesis:* While CUDA-OpenGL interop (`cudaGraphicsMapResources`) avoids CPU host-memory roundtrips for camera images by retaining buffers in device memory, the associated driver mapping locks and stream synchronization barriers may serialize graphics and compute passes just as strictly as CPU readbacks.
  *Validation Plan:* Benchmark double-buffered OpenGL rendering with alternating CUDA mapped resource processing; inspect stream timestamps for implicit serialization barriers.

- **H-04 (LIBERO Graphics Resource Headroom):**
  *Hypothesis:* MuJoCo's classic rendering pipeline at 128×128 resolution consumes negligible SM compute but relies on fixed-function rasterization and ROP units, leaving substantial compute SM capacity available for policy inference if driver scheduling permits concurrent execution.
  *Validation Plan:* Measure standalone resource use with supported hardware metrics and representative MuJoCo shader work. `--opengl-gpu-workload=true` captures batched timing, not SM occupancy by itself.

### Open Technical Questions

1. *What specific policy model and reinforcement learning training framework (e.g., RLinf, custom PPO, SAC) will be used for the final LIBERO benchmarks?*
2. *Will the authors of VUDA respond with source code access, or must the project proceed as an independent clean-room reverse-engineering effort?*
3. *What are the specific cluster policy boundaries at OSC regarding user-space `ioctl` interception and custom channel allocation?*

---

## 4. Experiment & Evidence Register

### EXP-001: OSC Ascend Environment & Toolchain Inventory
- **Date:** 2026-11-06 (UTC 2026-11-07 00:41)
- **Status:** Completed
- **Execution Context:** Interactive Slurm allocation on OSC Ascend cluster (temporary preliminary testbed awaiting EPFL GPU access), compute node `NODE_A`, Job ID `8374257`. Executed under placeholder account `YOUR_OSC_ACCOUNT` (coursework allocation redacted).
- **Artifacts:**
  - Public sanitized inventory: [`evidence/public/ascend-inventory.md`](../evidence/public/ascend-inventory.md)
  - Raw system inventory: `evidence/osc/ascend/8374257/osc-system-info-ascend-8374257.txt` (local-only uncommitted capture)
  - Toolchain follow-up transcription: `evidence/osc/ascend/8374257/toolchain-followup.txt` (local-only uncommitted transcription)
  - Setup reference notebook: `evidence/reference/module6_assignment_cnn_intro.ipynb` (local-only uncommitted coursework reference; source of temporary course batch configuration; preserved as supporting setup evidence only)
- **Summary of Findings:**
  - Allocated GPU: NVIDIA A100-PCIE-40GB (Compute Capability 8.0, 40,960 MiB memory, ECC on, MIG disabled, UUID and PCI identity redacted).
  - CPU & Memory: AMD EPYC 7H12 64-Core Processor; 5 CPUs allocated to job with affinity mask `112-116`; 16 GB memory allocated.
  - Driver & OS: Driver version `580.95.05` with open kernel module and GSP firmware; OS is Red Hat Enterprise Linux 9.6 (Plow), kernel `5.14.0-570.154.1.el9_6.x86_64`.
  - Toolchain Availability: Native compilers (`cc`, `gcc`, `g++`, `cmake`) and pkg-config (`egl 1.5`, `gl 1.2`) are present in system `/usr/bin` paths (compiler version strings were not queried in the inventory; GCC version in inventory reflects driver build toolchain).
  - Module Verification: System modules `cuda/12.8.1` and `nvhpc/25.1` are directly loadable. The user interactively loaded `cuda/12.8.1` and verified `nvcc` release 12.8 V12.8.93 via follow-up transcription.
  - Python Environment: Active system Python (`/usr/bin/python`, version 3.9.21) lacks package metadata for domain robotics/ML packages (`torch`, `mujoco`, `robosuite`, `libero`, `rlinf`, `PyOpenGL`, `glfw`).
- **Limitations:** EXP-001 was inventory/transcription only, without GPU workloads. Later EXP-002/EXP-003 entries record native execution. Native build files now exist; no automated test suite exists.

---

### EXP-001b: Live Remote Environment & Toolchain Verification on Ascend Login Node
- **Date:** 2026-11-06 (UTC 2026-11-07 02:00)
- **Status:** Completed
- **Execution Context:** Historical ledger records a remote Ascend login-node session (temporary preliminary testbed while awaiting EPFL GPU provisioning). Private login/account/home identifiers are withheld; this review did not reconnect.
- **Verified Findings:**
  - Storage & Quota: Historical ledger reports 500 GB quota with 21 GB used (479 GB free), 1,000,000 inode quota with 258,000 used, and accessible scratch/project storage. Private paths withheld.
  - Native Compilers: `gcc (GCC) 11.5.0`, `g++ (GCC) 11.5.0`, `cmake 3.26.5`, `git 2.47.3`.
  - Native Graphics Headers: `/usr/include/EGL/egl.h`, `/usr/include/GL/gl.h`, `/usr/include/KHR/khrplatform.h` verified. `g++ -lEGL -lGL` successfully compiles and links EGL context calls.
  - CUDA Toolchain: `module load cuda/12.8.1` exposes `nvcc release 12.8, V12.8.93` and includes `$CUDA_HOME/include/cudaGL.h`, `cuda_gl_interop.h`, `cuda.h`, `cuda_runtime.h`. Tested compilation of hybrid CUDA + EGL code via `nvcc -x cu -lEGL -lGL -lcudart` succeeded (exit code 0).
  - Profiler Toolchain: `module load nvhpc/25.1` directly loadable, provides `nsys 2024.7.1.84`.
  - Python / Conda: `module load miniconda3/24.1.2-py310` provides Conda 24.1.2 and Python 3.10.13.
  - Workspace Setup: Staged directory layout at `~/vuda-workspace/` (`scripts/`, `src/`, `build/`, `logs/`, `evidence/`), copied `scripts/pitzer-diagnostics.sh`, created `README.md` (highlighting temporary status pending EPFL access) and `scripts/env-setup.sh`.
---

### EXP-002: Standalone Native C++/CUDA/EGL Correctness & Timing Probe
- **Date:** 2026-11-06 (UTC 2026-11-07 02:12)
- **Status:** Completed
- **Execution Context:** Slurm batch job `8374921` on OSC Ascend (temporary testbed awaiting EPFL), node identifier withheld, A100-PCIE-40GB, driver `580.95.05`, CUDA `12.8.1`.
- **Artifacts:**
  - Public sanitized results: [`evidence/public/exp002-probe-results.md`](../evidence/public/exp002-probe-results.md)
  - Raw execution log: `evidence/osc/ascend/8374921/vuda_exp002_8374921.log` (local-only uncommitted capture)
  - Probe source code: `src/probe/` (`egl_renderer.cpp`, `cuda_compute.cu`, `benchmark_runner.cpp`, `main.cpp`, `Makefile`, `CMakeLists.txt`)
- **Summary of Findings:**
  - *Correctness:* Verified offscreen EGL FBO rendering (128×128 RGBA8) with programmable shaders and pixel assertion against expected RGB values. Verified CUDA arithmetic kernel numerical precision against host CPU.
  - *Interop capability (Gate 6 prerequisite):* Renderbuffer registration, mapping, and `cudaArray_t` retrieval succeeded. CUDA-side pixel consumption, copy-free behavior, and image correctness after handoff were not checked.
  - *Solo Baselines (Gate 3):* Solo EGL Render = 20.04 ms (40.0 μs mean, 24,950 fps); Solo CUDA Compute = 14.27 ms (28.5 μs mean, 35,028 ips). Theoretical zero-overhead serial sum = 34.31 ms.
  - *Serial overhead:* Serial alternation took 230.38 ms, about 6.7× the solo sum. These timings do not uniquely attribute the overhead to context switching or pipeline flushing.
  - *Concurrent Multi-Threaded:* Multi-threaded concurrent submission from separate host threads took 92.27 ms (5,419 pairs/s), delivering a **2.497× throughput speedup** over serial interleaved execution.
  - *Scheduling interpretation corrected:* `34.31 / 92.27 = 0.372×` is the solo-sum/concurrent-time ratio. Wall times do not prove TSG identities or time-sharing.
- **Limitations & Next Steps:** Single-run synthetic timing; concurrent latency columns reflect the render thread. No low-resource headroom, cross-process baseline, repeated steady-state measurements, or active spatial execution established. EXP-003 collected timelines but did not resolve those limits.
---

### EXP-003: Nsight Systems Timeline Capture
- **Date:** 2026-11-06 (UTC 2026-11-07 02:58)
- **Status:** Capture/report produced; final export failed; scheduling verdict inconclusive
- **Execution Context:** Slurm job `8375678` with `--gres=nsight`, OSC Ascend temporary testbed, node identifier withheld, A100-PCIE-40GB, Nsight Systems `2024.7.1.84`.
- **Artifacts:**
  - Public sanitized results: [`evidence/public/exp003-profile-results.md`](../evidence/public/exp003-profile-results.md)
  - Raw trace and reports: `evidence/osc/ascend/8375678/` (`vuda_exp003_profile_8375678.nsys-rep`, `vuda_exp003_profile_8375678.sqlite`, text summaries, and execution log; local-only uncommitted capture)
  - Batch script: `~/vuda-workspace/scripts/run_profile_exp003.sh`
- **Summary of Findings:**
  - *Timeline data:* Preserved export contains 341 CUDA kernels and 666 GL workload batches with NVTX annotations.
  - *Selected elapsed ranges:* GL batches labeled `glClear` span roughly 608 μs versus selected solo ranges around 34 μs. NVIDIA documents that such labels time batches, not isolated operations.
  - *Containment verified offline:* Four published CUDA-within-GL interval examples match the SQLite data. Neither containment nor inflation proves graphics suspension, TSG identity, or active spatial overlap.
  - *Export failure:* Log ends with `Exportation error: File already exists` and exit code 1. Existing database/report pairing requires a future export to a new path.
  - *Corrected verdict:* Gate 4 partial. No GPU context-switch event or GPU metrics table exists in this export. Profiled wall times (64.45 ms serial, 67.92 ms concurrent) differ from EXP-002; do not transfer its unprofiled speedup.

### EXP-004: Gate 5 Offline Driver, Policy, and Trace Assessment
- **Date:** 2026-11-06 local
- **Status:** Assessment recorded; runtime redirection blocked
- **Question:** Can VUDA v2's internal mechanism plausibly support native OpenGL on the recorded A100/open-driver/GSP stack, and what may be tested safely?
- **Method and exact inputs:** Read current/historical project documents, actual probe code, local raw EXP-002/EXP-003 outputs and read-only SQLite; reviewed VUDA v2 §§4–7.1, NVIDIA `580.95.05` source SHA `2b436058a616676ec888ef3814d1db6b2220f2eb`, Nsight 2024.7 semantics, OSC GPU guide/ethics/support. No remote login, GPU job, driver-control operation, or support message.
- **Artifacts:** [Gate 5 assessment and SQL audit](GATE5_ASSESSMENT.md); [exact-release driver map](GATE5_DRIVER_ASSESSMENT.md). Existing raw inputs remain private and unchanged; their review hashes are recorded separately under `evidence/osc/ascend/gate5-inputs.sha256`.
- **Source findings:** RM duplication and allocation wrappers, channel-group/context-share classes, explicit GSP duplication RPC paths, VAS relationships, and validation/lifetime branches exist. Actual GL objects and control mappings are not observed. No portable grafting/SCG command recipe was established.
- **Offline observations:** 341 kernels/666 GL batches; phase-filtered `glClear`-labeled batches: solo 99 (mean 39.604 μs), concurrent 90 (mean 536.500 μs). No `GPU_CONTEXT_SWITCH_EVENTS` or `GPU_METRICS`; enumeration labels are not events. Four interval examples verified.
- **Ruling:** Proceed to Gate 5 research despite incomplete Gate 4 proof because static assessment does not mutate the device. Keep Gate 4 partial rather than assuming need for a shim; cost if wrong is additional baseline work, not unsupported device changes.
- **Ruling:** Treat GSP as a compatibility boundary, not an impossibility proof. Require site-specific approval, artifact or complete reviewed design, and GL/control-mapping evidence before mutation; cost is delaying a potentially viable experiment.
- **Policy result:** Reviewed OSC guidance neither explicitly approves nor bans the proposed internals. PI-authorized account and system integrity rules apply. Project precautions are not OSC quotations. Specific inquiry drafted, not sent; EPFL requires its own review.
- **Next decision:** Obtain site response and author implementation details, strengthen Gate 4, then consider an approved non-mutating own-process observer before any Vulkan reproduction/OpenGL mutation.
- **Exercised verification:** Read-only SQLite queries verified row counts, phase aggregates, missing event/metric tables and four interval examples. Public manifest: 9/9; original private manifest: 13/13; historical archive manifest: 7/7; separate Gate 5 input manifest: 9/9. Collector `bash -n` passed, embedded Python parsed, and missing-allocation smoke exited 2 with the expected refusal. All 59 local links/anchors across 14 public Markdown files resolved. No GPU build/run or shim smoke was performed; no native source changed.
- **Final review:** Independent read-only reviewer reported no material findings after checking the exact-release ABI/class fields, GSP duplication bounds, OSC policy wording, interop limits, and SQLite aggregates. Review covers the assessment, not unrun runtime compatibility.

### Future Experiment Entry Template

```markdown
### EXP-XXX: [Descriptive Experiment Title]
- **Date:** YYYY-MM-DD
- **Status:** [Proposed / In Progress / Completed / Failed]
- **Execution Context:** [Cluster, Node, Job ID, Slurm Account, GPU Model]
- **Artifact:** [Path to raw log, trace file, or source code]
- **Objective:** [Clear, single-sentence hypothesis or goal]
- **Implementation & Method:** [Steps executed, parameters, configurations]
- **Observed Results:** [Quantitative data, timings, profile findings]
- **Limitations & Threats to Validity:** [Caveats, unmeasured variables, hardware-specific factors]
- **Next Steps:** [Follow-on actions prompted by results]
```
