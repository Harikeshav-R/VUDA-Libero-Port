> Public derivative of the original research/export record. Private screenshots, raw logs, coursework, ZIP, and local paths are omitted or redacted. Original files remain local in `research/archive/2026-11-06/`. Historical completion claims and checksums describe that original bundle, not this derivative. Current status: [research/STATUS.md](../STATUS.md).

# VUDA, OpenGL, and LIBERO: Session Record

## Status and saved artifacts

This records the substantive questions, requirements, decisions, corrections, commands, source-search results, and verification from the research session. It is a research handoff, not a claim that a native OpenGL VUDA port exists.

- [Detailed technical findings, source citations, and validation plan](VUDA_OPENGL_LIBERO_PLAN.md).
- [OSC diagnostic collector](../../scripts/pitzer-diagnostics.sh), usable on Ascend, Cardinal, and Pitzer despite the original filename.
- Resource-storage reference image (private image omitted from public repository).
- Mentor research brief (private image omitted from public repository).
- Screenshot of the mentor clarification questions (private image omitted from public repository).

The detailed plan was already present in the repository when the save request arrived. Its SHA-256 matches the session-local canonical plan exactly. Preserve that existing file. Historical `local://vuda-opengl-feasibility-plan.md` links refer to `VUDA_OPENGL_LIBERO_PLAN.md` in this export; historical `local://pitzer-diagnostics.sh` links refer to the adjacent shell script. Other `local://pasted-image-...` links map to the adjacent PNGs with the same names.

No VUDA implementation, CUDA/OpenGL scheduling prototype, diagnostic output from OSC, GPU trace, inference/training benchmark, or demonstrated LIBERO speedup was produced. No packages or drivers were installed. No cluster job was submitted by the assistant.

## Original assignment

The user asked whether functionality from https://arxiv.org/pdf/2605.01352 can be adapted to OpenGL for LIBERO, for both rendering plus training and rendering plus inference on the same GPU.

The mentor's original reading questions were:

1. What are the paper's most important assumptions?
2. What motivates it?
3. What principles underlie its solution?
4. What is the solution overview?
5. What are its limitations?

Supporting references supplied by the user:

- https://arxiv.org/pdf/2504.15930
- https://arxiv.org/pdf/2512.22560
- https://www.usenix.org/system/files/osdi26-yu-chao.pdf — particularly Figure 7.
- https://libero-project.github.io/main.html
- https://rlinf.readthedocs.io/en/latest/rst_source/examples/simulators_index.html

The mentor said to focus on VUDA rather than spend substantial time on the supporting RL papers. The first two supporting arXiv references above were not independently investigated in depth during this session. RLinf's Figure 7 and its simulator/LIBERO documentation were read.

## Scope clarification and mentor confirmation

The user initially reported that the current setup renders on CPU while inference/training runs on GPU. That is a reported deployment fact; the exact backend and measured breakdown were not provided.

The user then asked the mentor:

> If I understand correctly, you said that the current setup renders on CPU while training/inference runs on GPU. It looks like LIBERO supports GPU rendering through OpenGL/EGL, so will it work if we first enable that on the same GPU using regular driver scheduling?
>
> So are we trying to investigate whether rendering and CUDA kernels execute simultaneously, rather than time-sharing the GPU?
>
> And if the VUDA paper findings improve throughput or control contention for our OpenGL use case?

The mentor replied:

> 1. Libero supports offloading rendering operations to the GPU.
> 2. Yes, our goal is to investigate whether rendering and CUDA kernels can execute on the same GPU simultaneously (spatial sharing, not time-sharing).
> 3. We hope to gain some insights from this VUDA paper: (1) Why can't the current rendering and CUDA kernel run on the same GPU simultaneously? What is the root cause? (2) What is VUDA's solution to this problem? How is this solution implemented? Which modules were modified? (3) Can we use the same approach to implement a simple prototype system in the Libero scenario (using EGL/OpenGL as the rendering backend)? What adaptations do we need to make based on the VUDA design concept? For example, switching the rendering backend from Vulkan to OpenGL? Or switching to a specific GPU model? If transplantation requires significant engineering costs or is not feasible, what are the reasons?

The mentor brief additionally names graphics-backend effects, SM contention, HBM/device-memory contention, L2 cache, LIBERO and ManiSkill, and both rendering/inference and rendering/training.

Final interpretation: enabling EGL is only a baseline. The research deliverable is a root-cause account, a concrete version-specific VUDA implementation/component analysis, and an evidence-backed feasibility assessment for a small native OpenGL/EGL prototype. A documented negative feasibility result is valid. Replacing LIBERO rendering with Vulkan would be a separate intervention, not proof of a native OpenGL port.

## Findings and corrections that must survive handoff

### Paper versions and mechanisms

- V1: https://arxiv.org/html/2605.01352v1 , dated May 2, 2026, titled *VUDA: Breaking CUDA-Vulkan Isolation for Spatial Sharing of Compute and Graphics on the Same GPU*.
- V2: https://arxiv.org/html/2605.01352v2 , dated September 26, 2026, titled *VUDA: Enabling Controlled Spatial Sharing of Graphics and Compute on NVIDIA GPUs*.
- TSG means **Timeslice Group**, not Thread Safety Group, Task Scheduling Group, or Target Scheduling Group.
- In the examined CUDA/Vulkan configurations, different TSGs time-share the graphics/compute engine. VUDA redirects selected CUDA channels into the existing graphics TSG.
- V1 uses forwarding channels and kernel-module page-table changes. V2 instead describes user-space `LD_PRELOAD`/`ioctl()` interception, object duplication and allocation-parent redirection, separate data address spaces selected through subcontexts, shared control-buffer mappings, a shadow channel for partition controls, and a daemon.
- V2 explicitly says it does not require a driver patch. It still depends on internal driver operations and undocumented commands; it is not a supported public-API-only solution.
- The ordinary OpenGL API does not become incapable of such a port merely because its frontend is implicit. Conversely, shared NVIDIA hardware does not establish that Vulkan's exact mechanism works with OpenGL. The GL allocation path, control mappings, driver state, and actual hardware must be investigated.
- V2 experiments cover native Vulkan/CUDA under Linux, on RTX 4090, A800-SXM4, and Jetson AGX Orin. Native OpenGL support is not demonstrated.

### Evaluation boundaries

- V1 reports up to 1.80× data-generation throughput, 1.85× MLP RL **rollout** throughput, and 1.23× VLA rollout throughput.
- The v1 OpenVLA-OFT experiment uses a separate inference GPU. Its result is not evidence of co-located VLA inference and rendering, and rollout throughput is not measured end-to-end learner acceleration.
- V2 reports 1.40–1.62× ManiSkill simulation throughput against the same asynchronous pipeline. Images lag simulation by one control step; the experiment does not establish unchanged learning quality.
- V2's driving-perception example reduces measured misses from 80.1% to zero among 1,861 measured inferences while sustaining 60 FPS. This is empirical attainment, not a formal real-time guarantee.
- Inter-SM partitioning leaves L2 and device-memory bandwidth shared. GPC locality, contention, and partition changes remain limitations; a reconfiguration experiment reaches a 232-ms frame.

### LIBERO, transfer, and storage

- The inspected upstream LIBERO pins `robosuite==1.4.0`; that robosuite release requires `mujoco>=2.3.0`, not an exact pinned MuJoCo version. Installed forks may differ.
- Classic `MjSim.step` calls CPU `mujoco.mj_step`. It is not the paper's CUDA PhysX physics path. The relevant CUDA computation here is model inference/training unless the deployed physics backend changes.
- Camera observations reach `MjRenderContext.render`/`mjr_render`, then `read_pixels`/`mjr_readPixels` into host NumPy arrays. A process-local global render lock spans rendering and readback in the inspected binding.
- Hardware rendering can coexist with CPU physics, CPU scene preparation, command submission, and host image readback. High CPU utilization or slow `render()` timing does not isolate rasterization cost.
- EGL context creation, GPU resource sharing, cross-process transport, and TSG scheduling are separate mechanisms.
- CUDA/OpenGL map/unmap provides ownership and ordering guarantees. Graphics must not access a CUDA-mapped registered resource. Registration and double buffering do not by themselves prove spatial concurrency.
- Multisampled renderbuffers cannot simply be registered as arbitrary CUDA images. Tensor preprocessing/layout and resource ownership still matter.
- The six-identical-face storage claim in the reference image is not generally correct for MuJoCo host storage. Both reviewed current and 2.3.0 texture documentation state that identical cube-face images need only one copy in `mjModel`.
- CPU model storage, GPU cubemap storage, resources duplicated across contexts, per-camera render targets, readback buffers, and CUDA tensors must be counted separately. Do not assume three software layers imply three full data copies.

### Inference/training correctness

A single trajectory's inference cannot consume an observation before its rendering completes. Use independent trajectories/groups to expose overlap without stale images. A learner update may not be ready during rollout, and on-policy barriers/policy versions must not be changed silently. Offline training on stored demonstrations has no live rendering to overlap unless a separate workload is co-located.

For independent phases with unchanged standalone times, the ideal pairwise bound is `(T_render + T_compute) / max(T_render, T_compute)`. This is not a speedup prediction. End-to-end results must include host/transfer work, batching tradeoffs, contention, memory capacity, and algorithm dependencies.

## Available OSC hardware

The user has access to all three clusters while awaiting EPFL GPU access. Published counts/specifications below are not observations of an allocated node.

### Pitzer

- 224 standard nodes: 40 cores, 192 GB host memory.
- 340 standard nodes: 48 cores, 192 GB host memory.
- 32 dual-V100 nodes: 40 cores, 384 GB host memory; OSC identifies the GPUs as 16-GB V100s.
- 42 dual-V100 nodes: 48 cores, 384 GB host memory; OSC identifies the GPUs as 32-GB V100s.
- 4 quad-V100 nodes: 48 cores, 768 GB host memory, four 32-GB V100s and NVLink.
- The user also listed four 80-core, 3-TB SMP nodes. OSC's current full listing additionally distinguishes other large-memory nodes; inventory the actual allocation rather than assuming the abbreviated user list is exhaustive.
- Reported theoretical system peak: 3.9 PFLOPS.

### Ascend

- 24 nodes: 88 usable cores, 921 GB usable host memory, four 80-GB A100s.
- 214 nodes: 120 usable cores, 472 GB usable host memory, two 40-GB A100s.
- 84 nodes: 120 usable cores, 472 GB usable host memory, up to three 40-GB A100s; OSC notes that some resources are reserved or still being tested.
- Reported theoretical system peak: 16.0 PFLOPS.

### Cardinal

- 326 dense nodes: 96 usable CPU cores, 128 GB CPU-attached HBM, 512 GB DDR5.
- 32 GPU nodes: 1 TB host DDR5, four H100s with 94 GB device memory each and NVLink. OSC lists Xeon Platinum 8470 CPUs for this node class, not the CPU Max 9470 used elsewhere.
- 16 large-memory nodes: CPU-attached HBM and 2 TB DDR5.
- Four login nodes are not GPU experiment nodes.
- Reported theoretical system peak: 10.5 PFLOPS.

### Hardware selection decision

Start with Ascend A100 inventory because its Ampere family is closer to the paper's evaluated A800. This is an engineering priority, not a compatibility or speed guarantee. Collect Cardinal H100 next as a newer-architecture comparison, and retain Pitzer V100 as an older comparator. V100 compute capability is 7.0; the paper does not validate Volta. NVIDIA TensorRT-RTX SCG documentation's exclusion of compute capability <=7.5 applies to that product path, not a proof about every undocumented sharing mechanism.

On every target, verify actual native graphics support, correctness, GPU identity, driver/library configuration, allocation isolation, and CUDA/GL coexistence. A100 forum reports and H100 rendering discussions are warnings, not an OSC support matrix. H100 vGPU documentation must not be generalized to bare-metal EGL. No display connector, a successful CUDA operation, large VRAM, or CPU HBM alone decides graphics compatibility.

## Agreed provisional defaults

The user said to use defaults until a mentor-provided workload arrives:

- One allocated GPU; one task; four CPU cores; 16 GB host memory; 20 minutes for inventory.
- Native EGL/OpenGL; `agentview` and `robot0_eye_in_hand`; 128×128 RGB; no depth or segmentation; 20-Hz control.
- `libero_spatial`, task index 0, one environment for correctness, then two independent environments for overlap diagnosis. Confirm the installed task registry/assets first.
- Bounded FP32 CUDA work on disjoint data for the initial scheduling probe, explicitly labeled synthetic. No automatic large VLA download and no claim of real inference/training from this probe.
- Preserve the site driver, current module baseline, benchmark semantics, observation freshness, and training-policy barriers.
- Primary provisional metric: completed environment transitions/s, with renderer/model solo and co-running throughput and latency percentiles reported separately.

## OSC commands and safety

The collector is adjacent to this document. It only inventories state; missing tools are logged. Use an allocated GPU compute-node shell, never a login node for workloads.

From a local machine, after the files are saved here:

```bash
scp pitzer-diagnostics.sh YOUR_OSC_USERNAME@ascend.osc.edu:~/
ssh YOUR_OSC_USERNAME@ascend.osc.edu
```

Allocate on the selected cluster, replacing the account placeholder:

```bash
salloc --account=YOUR_OSC_PROJECT --nodes=1 --ntasks=1 --cpus-per-task=4 --mem=16G --gpus-per-node=1 --time=00:20:00
```

If `salloc` leaves a login-node shell, enter the allocation:

```bash
srun --overlap --ntasks=1 --pty bash -l
```

Collect without changing modules for the first capture:

```bash
set -o pipefail
bash "$HOME/pitzer-diagnostics.sh" 2>&1 | tee "osc-system-info-${SLURM_CLUSTER_NAME:-cluster}-${SLURM_JOB_ID}.txt"
```

Run from a Cardinal login shell for the second architecture; OSC documents shared home storage. Exit the allocation when finished. For a later authorized profiling allocation, append `--gres=nsight` to the allocation request. OSC uses this scheduler option to address DCGM counter contention. Do not stop services manually. Ascend requires versioned module loads; inspect `module spider nvhpc` before choosing a version. The collector does not load/install modules.

The collector reports OS/kernel, Slurm grants, CPU/affinity/RAM/storage, selected environment settings, full GPU and topology queries, driver parameters, module availability, tool/profiler paths and versions, graphics-library discovery, and Python package metadata. It does not establish actual EGL rendering or GPU co-residency. `nvidia-smi` may expose unallocated physical GPUs, and its CUDA header is a driver compatibility indicator rather than a toolkit/framework runtime version. Review private paths, project IDs, hostnames, device identifiers, and process information before publishing output.

No `sudo`, driver replacement, GPU reset, clock changes, MPS daemon changes, kernel patches, or undocumented device-control commands are authorized by these diagnostic instructions.

## Public implementation search

No verified public author implementation was located. The search included paper pages, candidate author public GitHub repositories, the corresponding author's institutional page, exact-ID/title web queries, and repository-metadata searches for the paper ID, `VUDA spatial`, and `org:SJTU-IPADS VUDA`. Zero metadata-query matches do not establish absence from all source code. A broader CUDA/Vulkan isolation query returned an unrelated lyrics project. Relevant-looking public LibreCuda/inference forks were also checked at README/branch level without identifying a VUDA artifact.

False positives:

- https://github.com/jgbit/vuda — unrelated header-only CUDA-like runtime over Vulkan.
- https://github.com/timfox/idTech3/blob/main/docs/VUDA.md — explicitly describes approximate channel redirection, modeled grafting, and placeholder jobs rather than the paper mechanism.

Potential low-level background, not an identified VUDA implementation:

- https://github.com/Xubbbb/LibreCuda — README describes a rudimentary independent CUDA driver API implementation using ioctls/QMDs; inspected public branches are `main` and `turing`.
- https://api.github.com/repos/Xubbbb/llama.cpp/branches?per_page=100 — inspected response lists `master`; no VUDA branch was identified there.

Next step: contact corresponding author Jinyu Gu at `gujinyu@sjtu.edu.cn`, verified at https://ipads.sjtu.edu.cn/pub/members/jinyu_gu . Request v2 source/commit, reproduction instructions, permissions, supported drivers/GPUs, and any OpenGL/A100/H100 experience. No message was sent by the assistant.

## Validation and remaining prerequisites

Executed locally:

- Checked the diagnostic collector using `bash -n`.
- Parsed its embedded Python successfully.
- Ran its missing-allocation guard: exit code 2 with an explicit refusal before diagnostics.
- Verified that existing `VUDA_OPENGL_LIBERO_PLAN.md` and the session-local canonical plan have identical SHA-256 values.

Still required:

- Remote inventory reports from actual OSC GPU allocations.
- A working hardware EGL render on the allocated device with verified placement and correct output.
- Representative CUDA work with observable independent ready work.
- GPU-side scheduling/co-residency evidence, not merely overlapping host submissions or elapsed timestamp intervals.
- Real inference/training workload, model/checkpoint, framework versions, policy-update schedule, and learning-quality baseline.
- Actual VUDA implementation or enough driver-specific information for an explicitly scoped independent reverse-engineering project.

The detailed source list and the full follow-on validation matrix are in [VUDA_OPENGL_LIBERO_PLAN.md](VUDA_OPENGL_LIBERO_PLAN.md). This session did not produce the three standalone research files some initial subagents claimed (`vuda-paper-findings.md`, `libero-render-findings.md`, `opengl-cuda-findings.md`); they were absent from the session-local artifact inventory. Their unsupported draft statements were not accepted as authoritative evidence. Preserve the consolidated, corrected findings instead of fabricating those missing files.
