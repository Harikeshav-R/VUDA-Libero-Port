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

---

## 2. Primary Sources Register (S1–S23)

*Note: The primary source literature review was conducted historically on 2026-11-06 and anchored in the detailed feasibility plan at [`research/reference/VUDA_OPENGL_LIBERO_PLAN.md`](reference/VUDA_OPENGL_LIBERO_PLAN.md). These sources are not re-fetched dynamically but serve as the immutable conceptual foundation of the workspace.*

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

---

## 3. Hypotheses & Open Technical Questions

### Active Hypotheses

- **H-01 (OpenGL Timeslice Group Isolation):**
  *Hypothesis:* The proprietary NVIDIA Linux driver assigns headless EGL OpenGL contexts and CUDA contexts to separate Timeslice Groups (TSGs), causing the graphics/compute engine to time-slice between them at coarse granularity rather than executing them simultaneously on distinct SMs.
  *Validation Plan:* Measure execution timelines using Nsight Systems (`nsys profile --trace=cuda,opengl,osrt --opengl-gpu-workload=true`) on a concurrent EGL/CUDA workload. Note that a lack of observed timeline overlap alone does not prove TSG isolation without confirming channel allocation, resource availability, and the absence of application-level serialization barriers.

- **H-02 (Open-Kernel Module & GSP Compatibility):**
  *Hypothesis:* On modern NVIDIA datacenter drivers (such as 580.95.05 on RHEL 9), the open-source kernel module delegates hardware channel allocation and scheduling control to the on-chip GPU System Processor (GSP) firmware, potentially preventing user-space `ioctl` channel-parenting manipulation without firmware support.
  *Validation Plan:* Review driver `nvidia-drm` / `nvidia-uvm` open kernel module source for TSG allocation ioctls; test low-level allocation calls in an isolated environment.

- **H-03 (Interoperability Synchronization Serialization):**
  *Hypothesis:* While CUDA-OpenGL interop (`cudaGraphicsMapResources`) avoids CPU host-memory roundtrips for camera images by retaining buffers in device memory, the associated driver mapping locks and stream synchronization barriers may serialize graphics and compute passes just as strictly as CPU readbacks.
  *Validation Plan:* Benchmark double-buffered OpenGL rendering with alternating CUDA mapped resource processing; inspect stream timestamps for implicit serialization barriers.

- **H-04 (LIBERO Graphics Resource Headroom):**
  *Hypothesis:* MuJoCo's classic rendering pipeline at 128×128 resolution consumes negligible SM compute but relies on fixed-function rasterization and ROP units, leaving substantial compute SM capacity available for policy inference if driver scheduling permits concurrent execution.
  *Validation Plan:* Measure standalone GPU SM occupancy and throughput during intensive MuJoCo offscreen rendering loops using supported Nsight Systems GPU workload metrics (`--opengl-gpu-workload=true`) or a dedicated graphics profiler, subject to cluster toolchain support.

### Open Technical Questions

1. *What specific policy model and reinforcement learning training framework (e.g., RLinf, custom PPO, SAC) will be used for the final LIBERO benchmarks?*
2. *Will the authors of VUDA respond with source code access, or must the project proceed as an independent clean-room reverse-engineering effort?*
3. *What are the specific cluster policy boundaries at OSC regarding user-space `ioctl` interception and custom channel allocation?*

---

## 4. Experiment & Evidence Register

### EXP-001: OSC Ascend Environment & Toolchain Inventory
- **Date:** 2026-11-06 (UTC 2026-11-07 00:41)
- **Status:** Completed
- **Execution Context:** Interactive Slurm allocation on OSC Ascend cluster, compute node `NODE_A`, Job ID `8374257`. Executed under placeholder account `YOUR_OSC_ACCOUNT` (coursework allocation redacted).
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
- **Limitations:** This was a static environment inventory and user transcription. No GPU compute kernels or graphics contexts were initialized or benchmarked. The current repository contains no automated build or test suite; EXP-002 and EXP-003 represent proposed experimental designs for future implementation.

---

### EXP-002: Standalone Native C++/CUDA/EGL Correctness & Timing Probe (Proposed)
- **Status:** Proposed / Specification Stage
- **Objective:** Establish the first empirical baseline of headless EGL OpenGL rendering and CUDA compute execution on the allocated A100 GPU under standard APIs.
- **Experimental Design:**
  1. *EGL Device Binding:* Initialize headless EGL binding explicitly to the physical GPU allocated by Slurm (matching GPU UUID and PCI bus ID from `nvidia-smi` / `nvidia-smi -L` and validating with `cudaGLGetDevices`). Create an offscreen Framebuffer Object (FBO) with standard RGBA8 color attachment at 128×128 (matching LIBERO resolution).
  2. *Rendering Workload:* Render a known shaded geometric scene with programmable shaders. Read back pixel buffer to CPU and assert output against known pixel values.
  3. *CUDA Workload:* Launch a configurable, low-resource synthetic compute kernel (avoiding saturated, arbitrary long-duration kernels). Verify numerical correctness.
  4. *Execution Modes:*
     - Solo EGL rendering.
     - Solo CUDA compute.
     - Serial interleaved execution (Render, then Compute sequentially).
     - Multi-threaded independent execution (separate CPU threads issuing Render and Compute calls concurrently to the same GPU).
     - Multi-process independent execution (separate processes issuing Render and Compute calls).
  5. *Measurements:* Wall-clock total duration, per-iteration latency distribution, and CPU/GPU memory footprint.
- **Limitations:** Independent workload submission and wall-clock overlap alone do not prove simultaneous hardware execution due to CPU submission latencies and coarse timing granularity. Hardware-level verification requires profiler timeline analysis under EXP-003.

---

### EXP-003: Nsight Systems Concurrency & Context-Switch Profiling (Proposed)
- **Status:** Proposed / Gated on EXP-002 Implementation and Profiler Support Review
- **Objective:** Collect microsecond-accurate hardware timeline traces of concurrent EGL rendering and CUDA kernel execution to determine whether driver time-slicing or spatial execution occurs.
- **Requirements:** Interactive Slurm allocation on Ascend with `--gres=nsight` enabled and `nvhpc/25.1` loaded. Profiling procedure will follow the supported command template documented in the OSC Runbook (`docs/OSC_RUNBOOK.md`).
- **Evaluation Criteria:**
  - Do OpenGL draw calls and CUDA kernels show overlapping execution on the GPU hardware timeline? (Note: API interval overlap on the host timeline cannot prove active spatial execution on the GPU; GPU workload metrics and context switch events must be evaluated together.)
  - Does the GPU Context Switch trace reveal preemption events between graphics and compute contexts?

---

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
