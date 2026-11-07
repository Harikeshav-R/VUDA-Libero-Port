# Project Status: VUDA-Libero-Port

## Executive Summary

The objective of the VUDA-Libero-Port project is to investigate whether native OpenGL/EGL rendering (the headless graphics pipeline utilized by LIBERO via MuJoCo and robosuite) and CUDA kernels (policy neural network inference and learner training) can execute simultaneously on a single GPU through spatial resource sharing.

The research mentor has clarified that the primary question is not merely enabling GPU-accelerated rendering in LIBERO—which is already supported via EGL—nor is it time-shared GPU co-location where rendering and compute alternate execution via driver context switching. Rather, the goal is to determine the architectural and driver root causes of graphics/compute serialization on NVIDIA datacenter GPUs, reconstruct the spatial sharing mechanism introduced in the VUDA paper, and evaluate the feasibility, adaptation requirements, or fundamental engineering blockers of implementing a lightweight native OpenGL/EGL prototype for LIBERO.

As of November 6, 2026, the project is in the initial research, environment discovery, and experimental planning stage. No code prototype has been created, and no GPU workload execution has been supplied or observed in this research.

---

## Evidence Gates & Verification Milestones

The table below defines the required evidence gates for the research investigation, tracking what has been empirically verified versus what remains pending.

| Gate | Milestone | Status | Key Evidence / Verification Basis |
| :--- | :--- | :--- | :--- |
| **Gate 1** | **Problem Formulation & Literature Foundation** | **Passed** | Comprehensive analysis of VUDA v1 ([arXiv:2605.01352v1](https://arxiv.org/html/2605.01352v1)) and VUDA v2 ([arXiv:2605.01352v2](https://arxiv.org/html/2605.01352v2)); primary sources S1–S23 reviewed historically on 2026-11-06 and preserved in [`research/reference/VUDA_OPENGL_LIBERO_PLAN.md`](reference/VUDA_OPENGL_LIBERO_PLAN.md). The user reported emailing the VUDA authors (recipients unspecified; corresponding author listed as Jinyu Gu in literature) requesting source code access; response pending. |
| **Gate 2** | **Target Cluster Environment & Hardware Discovery** | **Partial** | Initial system and toolchain inventory executed on Ohio Supercomputer Center (OSC) Ascend cluster (node `NODE_A`), Slurm Job 8374257. Hardware confirmed: NVIDIA A100-PCIE-40GB, Ampere CC 8.0, driver 580.95.05 with open kernel module and GSP firmware. Active `/usr/bin/python` (version 3.9.21) lacks package metadata for domain robotics/ML libraries (`mujoco`, `robosuite`, `libero`, `torch`, etc.; does not imply global cluster absence). Direct toolchain discovery and interactive CUDA 12.8.1 load verified via user-transcribed follow-up. See the public sanitized summary in [`evidence/public/ascend-inventory.md`](../evidence/public/ascend-inventory.md) (derived from local-only raw evidence `evidence/osc/ascend/8374257/osc-system-info-ascend-8374257.txt` and `evidence/osc/ascend/8374257/toolchain-followup.txt`). |
| **Gate 3** | **Native EGL & CUDA Independent Baselines** | **Pending** | No GPU workload execution (EGL context creation, rendering, or CUDA kernel launch) has been supplied or observed on the cluster GPU. Baseline solo timings for graphics and compute remain unmeasured. |
| **Gate 4** | **Standard API Co-Execution & Profiling** | **Pending** | Concurrency behavior of unmodified EGL + CUDA under driver 580.95.05 has not been tested. Hardware timeline traces via Nsight Systems (requiring `--gres=nsight` allocation and `--opengl-gpu-workload=true`, with actual counter access untested) have not been collected. |
| **Gate 5** | **Driver TSG & Channel Redirection Assessment** | **Pending** | Technical feasibility and security policy review of user-space `ioctl` interception and Timeslice Group (TSG) parent redirection under OSC multi-tenant policies and the NVIDIA open-kernel/GSP driver architecture remain open. |
| **Gate 6** | **LIBERO Observation Pipeline & Memory Handoff** | **Pending** | Assessment of `cudaGraphicsMapResources` vs CPU `mjr_readPixels` host roundtrip in robosuite/MuJoCo offscreen rendering pipeline has not been implemented or benchmarked. |
| **Gate 7** | **End-to-End Inference & Training Concurrency** | **Pending** | End-to-end policy rollout and learner update co-execution benchmarks in LIBERO remain unaddressed pending resolution of upstream gates. |

---

## Known Facts vs. Unverified Areas

To maintain rigorous scientific standards, all project findings are strictly partitioned into established facts and open uncertainties.

### What is Known (Empirically or Systematically Documented)

1. **VUDA Architecture & Mechanisms:**
   - VUDA v2 operates entirely on Linux with native Vulkan and CUDA. It has not evaluated or demonstrated native OpenGL.
   - VUDA v2 resolves graphics/compute isolation by intercepting driver allocation `ioctl` calls via `LD_PRELOAD`, duplicating the graphics TSG handle into the CUDA client, and parenting selected CUDA channels to that TSG.
   - VUDA v2 utilizes an undocumented Simultaneous Compute and Graphics (SCG) shadow channel to adjust Texture Processing Cluster (TPC) partitioning across active Graphics Processing Clusters (GPCs).
   - In VUDA v1, reported speedups (e.g., 1.85×) measured simulation rollout throughput; the OpenVLA-OFT experiment placed inference on a separate physical GPU rather than co-locating it with rendering.

2. **LIBERO & robosuite Pipeline:**
   - Standard LIBERO relies on `robosuite==1.4.0` and classic MuJoCo (`mujoco>=2.3.0`).
   - Upstream reference pipeline uses two default cameras (`agentview`, `robot0_eye_in_hand`) rendered at provisional 128×128 resolution with a 20 Hz control step.
   - Offscreen rendering in robosuite creates an unshared EGL context (`eglCreateContext(..., EGL_NO_CONTEXT, ...)`).
   - Images are rendered via `mjr_render` and copied back to host CPU memory via `mjr_readPixels` under a Python process-level lock (`_MjSim_render_lock`).

3. **OSC Ascend Hardware Environment (Job 8374257):**
   - Compute node `NODE_A` is equipped with an AMD EPYC 7H12 64-core processor and an NVIDIA A100-PCIE-40GB GPU (PCI identity and UUID redacted, ECC on, MIG disabled).
   - Operating system: Red Hat Enterprise Linux 9.6 (Plow), kernel `5.14.0-570.154.1.el9_6.x86_64`.
   - NVIDIA driver: `580.95.05` (using the open kernel module and GSP firmware `580.95.05`).
   - The CUDA version reported in the `nvidia-smi` header (`13.0`) indicates driver compatibility, not the installed development toolkit.
   - `module spider` reported `cuda/12.8.1` and `nvhpc/25.1` as directly loadable. `cuda/12.8.1` was loaded by the user, exposing `nvcc` release 12.8 (V12.8.93); `nvhpc/25.1` profiler tools remain unverified until loaded and tested.
   - Core system build and graphics tools (`cc`, `gcc`, `g++`, `cmake`, `libEGL.so.1`, `libGL.so.1`, `pkg-config`) are present in system `/usr/bin` paths (specific compiler version strings were not queried in the inventory).
   - Active system Python (`/usr/bin/python`, version 3.9.21) lacks package metadata for domain robotics and machine learning libraries (`torch`, `mujoco`, `robosuite`, `libero`, `rlinf`, `PyOpenGL`, `glfw`), indicating a clean system base rather than global absence across user environments.

### What is Unknown / Unverified

1. **GPU Runtime & Concurrency:**
   - No GPU workload execution (EGL rendering context, draw calls, or CUDA compute kernels) has been supplied or observed on the allocated A100 in this research.
   - It is unknown whether the current NVIDIA driver (580.95.05) on RHEL 9 enforces strict time-slicing between native EGL OpenGL contexts and CUDA contexts, or whether any spontaneous kernel/draw overlap occurs under standard APIs.
   - Hardware profiling using Nsight Systems has not been executed; tracing API intervals alone cannot determine active simultaneous execution without GPU workload metrics, and actual SM utilization and context switch frequencies remain unmeasured.

2. **Driver & Architecture Interception Viability:**
   - It is unknown whether the NVIDIA GSP firmware and open-kernel module on Ampere A100 permit user-space `ioctl` TSG duplication and channel parenting without triggering driver errors or kernel panics.
   - Cluster administrative rules regarding low-level device control manipulation and custom memory mapping have not been vetted with OSC technical support.

3. **Application & Model Workload Details:**
   - The user's actual deployed benchmark suite, policy model architecture, training algorithm, and checkpoint remain unknown; baseline benchmark defaults are known from upstream references (two default cameras at 128×128 resolution, 20 Hz control step).
   - The RL training schedule (on-policy rollout vs. asynchronous off-policy learner updates) is uncharacterized.

4. **Author Source Artifact:**
   - No author-published source repository for VUDA v2 has been discovered in public repositories; response from authors to the user's inquiry email is pending.

---

## Provenance of Cluster Commands & Observations

All cluster data recorded in this workspace was collected under user-executed Slurm job allocations and communicated via command logs. The current workspace is a durable research documentation ledger; it contains no automated build suite, test runner, or prototype implementation. Specifically:

- **Public Redacted Summary:** File [`evidence/public/ascend-inventory.md`](../evidence/public/ascend-inventory.md) provides a sanitized summary of the Ascend hardware, driver, CUDA, and toolchain environment for public research tracking.
- **Raw Inventory:** Log file `evidence/osc/ascend/8374257/osc-system-info-ascend-8374257.txt` (local-only, uncommitted) was produced by running the diagnostic collector script inside interactive Slurm job `8374257` on Ascend compute node `NODE_A`.
- **Toolchain Follow-up:** File `evidence/osc/ascend/8374257/toolchain-followup.txt` (local-only, uncommitted) records commands and outputs pasted by the user into the conversation after the initial inventory (module spider commands, path checks, `pkg-config`, `module load cuda/12.8.1`, and `nvcc --version`). This is labeled as a user transcription without an independent timestamp or renewed Slurm job confirmation; it is associated with job 8374257 by conversation context.
- **Execution Responsibility:** The user submitted the `salloc` request and executed commands on OSC. The assistant did not directly connect to OSC or execute remote shell commands.
- **Account Identification:** The allocation was executed under an academic course allocation account (redacted; placeholder `YOUR_OSC_ACCOUNT`). This account identifier originated from the local-only reference coursework notebook `evidence/reference/module6_assignment_cnn_intro.ipynb`. It is retained solely as supporting evidence for scheduler access patterns and Slurm batch configurations, not as an authorized general research project grant. Future cluster work must use the designated project account placeholder `YOUR_OSC_ACCOUNT` (without angle brackets to prevent shell redirection).

---

## Prioritized Next Steps

The project will advance along the following sequential workstreams:

1. **Develop Standard-API Native C++/CUDA/EGL Probe (EXP-002):**
   - Create a minimal, self-contained C++ test utility that initializes a headless EGL context matching the physical GPU allocated by Slurm (matching GPU UUID and PCI bus ID from `nvidia-smi` / `nvidia-smi -L` and validating with `cudaGLGetDevices`), renders a known geometric target to an offscreen FBO, and executes a concurrent synthetic CUDA compute kernel with known numeric outputs.
   - Implement rigorous correctness checks (CPU pixel verification and compute output verification).
   - Test solo execution, serial execution, and independently submitted concurrent execution (multi-threaded and multi-process). Note that independent submission and wall-clock overlap alone do not prove simultaneous hardware execution.

2. **Conduct Hardware Timeline Profiling under Slurm:**
   - Allocate an interactive GPU session on Ascend with profiler permissions enabled using the supported allocation flag:
     ```bash
     salloc --account=YOUR_OSC_ACCOUNT --nodes=1 --ntasks=1 --cpus-per-task=4 --mem=16G --gpus-per-node=1 --gres=nsight --time=00:30:00
     ```
   - Load `nvhpc/25.1` and run `nsys profile --trace=cuda,opengl,osrt --opengl-gpu-workload=true` on the native probe. Note that `--gres=nsight` is OSC's documented/supported allocation mechanism to request profiling counter permissions, but actual hardware counter access remains untested; further, tracing API intervals alone cannot prove active simultaneous execution without GPU workload metrics.

3. **Inquire with OSC Technical Support:**
   - Submit an inquiry to OSC technical support regarding policy constraints on user-space `ioctl` interception, shadow channel allocation, and Nsight hardware counter profiling on Ascend (A100) and Cardinal (H100) clusters.

4. **Track Author Response & Evaluate Reverse-Engineering Scope:**
   - Await response from authors regarding VUDA v2 source code access. If unavailable, evaluate the scope and requirements of an independent clean-room implementation of the channel parenting shim.
