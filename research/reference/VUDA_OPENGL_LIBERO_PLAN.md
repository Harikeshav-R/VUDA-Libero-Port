> Public derivative of the original research/export record. Private screenshots, raw logs, coursework, ZIP, and local paths are omitted or redacted. Original files remain local in `research/archive/2026-11-06/`. Historical completion claims and checksums describe that original bundle, not this derivative. Current status: [research/STATUS.md](../STATUS.md).

# VUDA–OpenGL/LIBERO Feasibility and Validation Plan

## Context

The mentor confirms that LIBERO already supports GPU rendering and that the research objective is simultaneous rendering and CUDA execution on one GPU: spatial sharing, not merely placement or time-sharing. Investigate the serialization root cause, reconstruct VUDA's implementation and modified modules, and assess a simple native EGL/OpenGL prototype for LIBERO, including necessary GPU/driver adaptations and engineering barriers. The user has OSC Pitzer/V100, Ascend/A100, and Cardinal/H100 access while awaiting EPFL access, authorizes temporary workload defaults, requests system-inventory commands, and asks for an online search for the real VUDA implementation. Do not change cluster state or implement the port in this planning session.

## Feasibility verdict and evidence

**Recommendation: investigate native OpenGL/CUDA spatial sharing, using ordinary GPU-backed LIBERO rendering as the baseline.** Establish the actual serialization mechanism rather than assuming Vulkan's observed TSG behavior applies unchanged to OpenGL. Reconstruct VUDA's implementation, then determine whether its channel/subcontext/control-mapping mechanisms can support a bounded LIBERO prototype. Ordinary EGL co-location alone does not satisfy the mentor's research objective.

| Requested outcome | Assessment | Decisive condition |
| --- | --- | --- |
| Move the reported CPU rendering workload onto the model's GPU | Supported by the standard MuJoCo OpenGL/EGL stack on suitable Linux/NVIDIA deployments | Correct renderer/backend selection, NVIDIA graphics libraries/device access, and physical GPU placement |
| Render concurrently with CUDA inference across independent LIBERO environments | Plausible research target | Native GL/CUDA scheduling permits or can be made to permit co-execution; inference and rendering have resource headroom |
| Render concurrently with CUDA learner training | Plausible, with additional algorithm and memory constraints | Independent ready work exists, all resident state fits, and the training algorithm permits the proposed rollout/update schedule |
| Accelerate a single environment's observation-dependent inference by overlapping that inference with its own input rendering | Not possible for the dependent operations | The model must wait for the observation it consumes |
| Guarantee full GPU utilization or reproduce the paper's speedups in LIBERO | Not supported by current evidence | Requires target-specific profiling and end-to-end measurements |

### Clarified deployment premise and mentor brief

Treat CPU rendering in the user's deployment as a reported fact. The unobserved cause may be a software backend, software OpenGL implementation, or a deployment-specific rendering path; do not claim which cause applies. Separately measure CPU physics, scene preparation, driver submission, and host image readback because these remain CPU-side even with GPU rasterization.

MuJoCo's documented Linux distinction is OSMesa for headless software rendering versus EGL for hardware-accelerated headless rendering. The reviewed robosuite source supports these alternatives. EGL itself is a context-management interface; its use or an environment-variable setting alone does not prove NVIDIA hardware executes the rendering. [S8–S10]

Distinguish three acceptance levels: GPU-backed rendering, graphics and CUDA residing on the same physical GPU, and simultaneous active graphics/CUDA execution. The first two do not require proving or changing TSG membership. VUDA is relevant to the third, not a prerequisite for the first.

The mentor image names LIBERO, ManiSkill, Vulkan, graphics-backend effects, SM/device-memory/L2 contention, and both inference and training. Treat ManiSkill as a comparison workload with matched GPU, observation sizes, and model work where possible, not as an identical software stack. The paper's ManiSkill uses PhysX/SAPIEN/Vulkan, whereas the verified LIBERO reference uses MuJoCo/robosuite/OpenGL. An unmodified comparison changes physics and rendering together and cannot isolate graphics API causality. Use a separate controlled native GL/Vulkan probe for backend-specific claims. For LIBERO, “CUDA kernel” means model compute in this investigation unless a different physics backend is explicitly deployed.

### What VUDA establishes

- V1, May 2, 2026, and v2, September 26, 2026, are materially different papers. Use explicit versioned citations. V2's Section 7.1 says all experiments use native Vulkan graphics plus CUDA under Linux. OpenGL is mentioned in the motivation, not demonstrated in the evaluation. [S1, S2]
- Motivation: independent graphics and compute work can leave complementary GPU capacity unused, yet the examined native runtimes put their channels in different Timeslice Groups (TSGs). The graphics/compute engine time-slices these groups. This is a scheduling restriction in the examined configurations, not a universal impossibility for all CUDA/graphics APIs, drivers, engines, and architectures. [S1 §§1–2]
- Core assumptions: independent ready work; compatible NVIDIA channel/subcontext scheduling machinery; correct address translation after sharing; non-conflicting space for necessary control mappings; sufficient complementary resources for useful overlap. V2's controlled-sharing extension also assumes usable SCG controls and TPC allocations respecting graphics' GPC locality. [S1 §§4–5]
- V2's shim intercepts `ioctl()` allocation calls, duplicates the existing graphics TSG object into the CUDA driver client, and changes selected CUDA subcontext/channel parent bindings. It keeps CUDA's data virtual-address-space handle. Subcontext identifiers select the appropriate page tables. Shared graphics control buffers still need page-table grafting. [S1 §§4, 6]
- V2 uses a dedicated shadow channel to submit undocumented simultaneous-compute-and-graphics (SCG) control commands. Inter-SM mode partitions TPCs, not arbitrary individual SMs. Balanced quota changes preserve graphics capacity across active GPCs. [S1 §§5–6]
- V2 requires no driver patch according to Section 6. That does not mean it uses only supported public APIs. The implementation depends on internal driver operations and undocumented commands. The reviewed text does not provide enough detail to settle all privilege, firmware, teardown, or driver-compatibility requirements for a new port. [S1]
- V2 evaluates RTX 4090 / driver 610.43.02, A800-SXM4 / 610.57.04, and Jetson AGX Orin / 540.4.0. These are evaluated configurations, not minimum supported versions or a compatibility guarantee. [S1 §7.1]
- V2's ManiSkill 3.0.1/PhysX 5.10/SAPIEN 3 experiment reports 1.40–1.62× throughput over the same asynchronous pipeline. Images lag simulation by one control step. It does not measure the effect on learning. [S1 §7.3]
- Important correction to the user's interpretation: v1's 1.85× is **rollout throughput**, not measured end-to-end learner acceleration. Its OpenVLA-OFT experiment places inference on a separate RTX 6000 Pro; VUDA overlaps physics and rendering on the simulation GPU. The 1.23× result is not evidence of same-GPU VLA inference plus rendering. [S2 §6.4]
- Limitations include shared L2/memory bandwidth, GPC-local graphics imbalance, workload-dependent resource tradeoffs, and reconfiguration transients. V2 reports a 232-ms frame during a quota-change experiment. Zero misses among 1,861 measured perception inferences is an empirical result, not a formal hard-real-time guarantee. [S1 §§7.2, 7.4, 8]
- A renewed public artifact search checked the paper metadata/HTML, Bin Xu's `Xubbbb` GitHub profile and public repository names/descriptions, Jinyu Gu's institutional publication page, exact paper-ID/title web searches, and GitHub repository-metadata queries for `"2605.01352"` and `VUDA spatial`. The two repository queries returned zero results, but they search metadata, not all source code. No verified author implementation was located in this bounded search; do not claim that no implementation exists or that all author accounts were exhaustively searched. `jgbit/vuda` is a different header-only CUDA-like Vulkan library. `timfox/idTech3` explicitly says its channel redirection is approximated, page-table grafting is modeled, and jobs are placeholders. Neither is the paper's implementation. The corresponding author's institutional page confirms `gujinyu@sjtu.edu.cn`; contacting the authors for v2 source, permissions, command definitions, and hardware support is the next artifact-acquisition step. [S1, S12, S21]

### What changes for LIBERO

The reviewed upstream LIBERO `requirements.txt` pins `robosuite==1.4.0`. That robosuite release requires `mujoco>=2.3.0`, not a pinned exact MuJoCo version. RLinf deployments may use forks or different versions; do not present this upstream reference as an inventory of the user's installation. [S3, S4]

Verified reference observation path:

1. LIBERO `ControlEnv.step(action)` delegates to `self.env.step(action)`. Default cameras are `agentview` and `robot0_eye_in_hand`, at 128×128 in this wrapper. `OffScreenRenderEnv` forces offscreen rendering. [S5]
2. robosuite `MujocoEnv.step` invokes `self.sim.forward()`, controller work, `self.sim.step()`, and observable updates across physics substeps. Camera observables sample through `RobotEnv._create_camera_sensors` and `self.sim.render(...)`. [S6, S7]
3. `MjSim.step` calls classic `mujoco.mj_step`, not MJX or MuJoCo Warp. This is the CPU physics path; the relevant CUDA work for this proposal is policy inference or learner computation, rather than the paper's GPU PhysX physics. [S8]
4. `MjRenderContext.render` calls `mjv_updateScene` and `mjr_render`. `read_pixels` allocates a NumPy RGB array and passes it to `mjr_readPixels`. MuJoCo documents this as GPU framebuffer-to-CPU transfer. This establishes host readback, not its measured performance cost. [S8, S9]
5. `MjSim.render` holds module-level `_MjSim_render_lock` across rendering and pixel readback. This is an additional host-side serialization point for renderer calls within a process. It does not itself prove CUDA/GL device scheduling isolation. Do not remove the lock without a separate context/thread-safety design. [S8]
6. robosuite's `EGLGLContext` calls `eglCreateContext(..., EGL_NO_CONTEXT, ...)`: these contexts do not request an EGL share group. It caches an EGL display within the process. Context object sharing, cross-process buffer transport, and shared hardware TSG scheduling are separate problems. [S10]
7. A CUDA policy consuming these host images needs a host-to-device path unless the deployment replaces it. Exact tensor conversion, preprocessing, IPC, and transfer costs remain deployment-specific and unmeasured. [INFERENCE from S8 and CUDA model placement]

Do not confuse classic MuJoCo with its GPU physics alternatives. Replacing physics or the renderer would change the experiment rather than validate a native OpenGL scheduling port.

### Scheduling, transfer, and storage are three separate workstreams

- **Scheduling:** Does independent GL and CUDA work execute concurrently on the graphics/compute engine? A shared EGL object group, more Python processes, CUDA streams, or asynchronous CPU submission does not establish that.
- **Transfer:** CUDA/OpenGL interoperability supports GL buffer/image registration. `cudaGraphicsMapResources` orders prior graphics before subsequent CUDA work in its stream. `cudaGraphicsUnmapResources` orders prior stream work before subsequent graphics. Accessing a registered resource from graphics while CUDA owns its mapping is undefined. Device-resident handoff avoids a host roundtrip but is not automatically copy-free or free of synchronization. [S11]
- Map/unmap ordering can be broader than a single buffer's producer dependency. Double buffering alone is therefore not proof of useful GL/CUDA overlap. Submission ordering and stream/context placement need profiling independently of TSG redirection. [S11]
- Texture registration has format/type restrictions; multisampled renderbuffers are disallowed. MuJoCo framebuffer resolution/MSAA handling and model tensor layout/preprocessing must be preserved in any later interop design. Do not promise a direct PyTorch tensor alias of an arbitrary GL texture. [S11]
- **Storage:** The supplied diagram is useful for ownership layers, but it does not establish three full copies. OpenGL driver/library resource storage is the GPU resource's implementation, not automatically an extra complete duplicate.
- The diagram's six-identical-face claim is not generally true for MuJoCo host storage. Its texture documentation explicitly says: “if they are the same, only one copy is stored in mjModel.” Materials and textures are referenced assets. GPU cubemap face storage and repeated allocations across separate rendering contexts must be measured separately. Do not infer six render passes from six textured faces on a board. [S13]
- Static meshes/textures are uploaded during `mjr_makeContext` and retained; modified assets can be explicitly re-uploaded. Render targets, shadow buffers, model textures, and CUDA tensors are different allocations. Use “device memory/VRAM,” not “HBM” generically: the memory technology depends on the GPU. [S9]

### Inference and training require different success criteria

**Inference:** Preserve each environment's state → observation → action dependency. Overlap rendering for environment group A with inference on already-ready observations for group B. Keep environment, episode, step, camera, and action associations intact. Multiple independent trajectories provide opportunity; one trajectory's dependent render/infer pair does not. Splitting inference batches may lose batching efficiency, so compare full rollout throughput and action-latency tails, not isolated kernel occupancy.

**Training:** Distinguish three cases. Offline training on stored demonstrations has no live simulator rendering to overlap unless another job is deliberately co-located. A phase-separated on-policy workflow can overlap independent trajectories during rollout without overlapping the learner update. True learner/rollout overlap requires an algorithm-approved policy-version schedule; do not silently introduce stale observations, stale-policy rollouts, or simultaneous mutation of inference weights. Model parameters, activations, gradients, optimizer state, renderer resources, and any rollout model must fit concurrently. Multi-GPU collectives and synchronization remain additional constraints.

RLinf Figure 7 distinguishes placement across GPUs from time-sharing via context switching/offload. Its device locks intentionally provide exclusive device ownership in temporal scheduling. A low-level VUDA port cannot create simultaneous submissions if the application schedule prohibits them. Preserve that workflow unless a later, separate design proves an equivalent schedule. [S14]

For a dependency-free pair with unchanged standalone costs, the ideal bound is `(T_render + T_compute) / max(T_render, T_compute)`. It is at most 2× for that pair. For a full iteration with only fraction `f` removable, the ideal bound is `1 / (1 - f)`. For example, hiding a 10% rendering contribution caps that improvement near 1.11×. These are analytical bounds under stated assumptions, not predictions for LIBERO.

## Mentor baseline intake

The mentor has resolved the intended endpoint: diagnose why graphics and CUDA do not execute simultaneously in the target configuration, understand VUDA's concrete implementation, and assess a simple OpenGL/EGL prototype with explicit adaptation costs or reasons against feasibility. Do not ask again whether GPU rendering alone is sufficient; it is only the baseline. Native OpenGL is the target for the portability investigation; preserve LIBERO observation semantics.

The mentor's runnable baseline is not yet available. The user authorizes temporary defaults rather than blocking on that baseline; use the choices in the OSC section below. Record the eventual repository/fork/commit, launch command, configuration, container/environment, assets, and checkpoint when supplied. Do not invent a mentor-provided model or treat synthetic CUDA work as trained policy inference.

Clarify what the reported CPU-rendering measurement includes: software rasterization alone or the broader environment/render call including CPU physics, scene preparation, synchronization, and readback. Request existing logs or traces if available; do not ask the mentor to rerun measurements to establish the reported slowdown.

Prioritize rendering plus independent CUDA work, then representative policy inference, then learner updates. Use one physical GPU and unchanged observation semantics. Defaults for task/cameras/environment counts are fixed below; the real model/checkpoint and algorithm remain unknown. The primary provisional metric is completed environment transitions/s, with p50/p95/p99 action/frame latency and solo/co-running model throughput reported separately; do not introduce an arbitrary acceptable-regression threshold.

The mentor has no VUDA implementation; the public search result is recorded above. Cluster permissions, installed graphics stack, profiler access, and the exact allocated V100 variant remain unknown until inventory output arrives. Use only sanctioned user-space diagnostics on Pitzer; no driver changes, GPU reset, clock changes, MPS daemon changes, kernel patches, or undocumented control commands without a separate approved experiment and site permission.

## OSC clusters: inventory, temporary defaults, and compatibility gates

### Known hardware and limits

The user has access to three OSC sites. Pitzer has Volta V100 nodes with 16-GB or 32-GB devices. Ascend has Ampere A100 nodes with 40-GB or 80-GB devices. Cardinal has Hopper H100 GPU nodes with 94-GB devices. These are OSC's documented device configurations, not observations of a particular allocation. Collect CPU affinity, Slurm CPU/memory grants, actual GPU variant/UUID, driver, and MIG state. Cluster-wide CPU totals and CPU-attached HBM are not the allocated CUDA GPU's execution resources or VRAM. OSC's Cardinal GPU nodes use Xeon Platinum 8470 CPUs; CPU Max 9470/HBM specifications describe other node classes. [S17, S22]

Start inventory on Ascend, then collect Cardinal as the second architecture and Pitzer as the older comparator when practical. This priority is an engineering choice: A100 is Ampere, like VUDA v2's tested A800, but is not itself a tested configuration in the paper. H100 introduces Hopper adaptation uncertainty; V100 is Volta compute capability 7.0 and outside the paper's Ampere/Ada evaluation. NVIDIA's TensorRT-RTX SCG/CiG documentation excludes compute capability 7.5 or below for that product path, not necessarily every undocumented mechanism. [S1, S16, S20, S22]

Do not select a rendering target solely from CUDA speed, memory size, architecture family, lack of display connectors, or an NVML GPU listing. Native hardware OpenGL/EGL context support, rendering correctness, driver/library availability, allocation isolation, and same-device CUDA interoperability need validation on each actual node. NVIDIA forum reports show OpenGL use on A100 with older driver-specific corruption, while an H100 response warns normal rendering may require custom work; these are cautionary reports, not a support matrix for OSC's bare-metal nodes. The reviewed H100 vGPU page concerns virtualization and cannot establish bare-metal EGL support. If one site fails, preserve the specific failure and test the next site; if all fail due to a required unsupported capability, use Pitzer/Ascend/Cardinal for compute/control baselines and request a suitable graphics-capable EPFL target rather than claiming all NVIDIA/OpenGL sharing is impossible. [S23]

### Temporary workload defaults

- Allocate one GPU, one task, four CPU cores, 16 GB host memory, and 20 minutes for inventory. Do not request an entire GPU node just for diagnostics. Record rather than force the GPU memory variant initially; prefer a full allocated GPU over a MIG slice for the later native-graphics study, without changing MIG state yourself.
- Select native EGL/OpenGL, RGB-only output from `agentview` and `robot0_eye_in_hand`, 128×128 images, no depth/segmentation, 20-Hz control, and unchanged benchmark observations. These match the inspected LIBERO wrapper reference where specified.
- Select `libero_spatial`, task index 0, one environment for correctness; the later independent-trajectory overlap check uses two environments, one per group. This is a proposed benchmark selection, not a claim that its local API/assets are installed; confirm the installed benchmark registry before execution.
- Start scheduling diagnosis with bounded FP32 CUDA compute on disjoint data, explicitly labeled synthetic. Do not download a large VLA checkpoint by default or claim synthetic work measures inference/training. Add the actual model when supplied.
- Preserve the site driver and existing compatible modules. Select CUDA/PyTorch support for the actual device after inventory (`sm_70` matters on V100). Neither the `nvidia-smi` header nor installed `nvcc` alone identifies the CUDA runtime used by a model framework. Ascend requires explicit versions when loading modules; use `module spider` and do not blindly load the latest version or alter the initial inventory's environment.

### Exact inventory procedure

Connect to the chosen cluster login host (`ascend.osc.edu` first, `cardinal.osc.edu` second, or `pitzer.osc.edu` for the comparator), then request an allocation using the user's actual OSC project code:

```bash
salloc --account=YOUR_OSC_PROJECT --nodes=1 --ntasks=1 --cpus-per-task=4 --mem=16G --gpus-per-node=1 --time=00:20:00
```

Skip this if already inside a suitable GPU allocation. OSC documents that `salloc` starts an interactive compute-node shell; verify the hostname against the job's NodeList. If it leaves a login-node shell, enter the allocation with `srun --overlap --ntasks=1 --pty bash -l` before collecting node diagnostics. Do not run GPU workloads on a login node. [S18]

The session-local deliverable `local://pitzer-diagnostics.sh` is a read-only collector that works on all three OSC clusters despite its original filename. Download it as `pitzer-diagnostics.sh`, transfer it to the OSC home directory if needed, retain the current module environment for the first capture, and run:

```bash
set -o pipefail
bash ./pitzer-diagnostics.sh 2>&1 | tee "osc-system-info-${SLURM_CLUSTER_NAME:-cluster}-${SLURM_JOB_ID}.txt"
```

It gathers allocation/OS/kernel, CPU/affinity/RAM/storage, selected rendering/CUDA/container environment values, full GPU identity/driver/memory/compute mode/topology and query output, NVIDIA module parameters, loaded and available CUDA/NVHPC modules, tool/profiler availability, graphics-library discovery, and Python package metadata. It logs absent tools and nonzero exits instead of hiding them. It refuses execution without `SLURM_JOB_ID`, does not create an EGL context or CUDA workload, does not install/load software, and does not dump all environment variables. The log is the only output file created by the invocation. Review usernames, project IDs, hostnames, paths, device UUIDs, serial/process details before sharing publicly.

The diagnostic commands behind the collector include `scontrol show job "$SLURM_JOB_ID"`, `lscpu`, `nproc`, `free -h`, `taskset -pc`, `nvidia-smi -L`, GPU field queries, `nvidia-smi topo -m`, `nvidia-smi -q`, `/proc/driver/nvidia/version`, `/proc/driver/nvidia/params`, `module list`, `module spider cuda`, `module spider nvhpc`, `nvcc --version`, `nsys --version`, `nsys status --environment`, `ncu --version`, `pkg-config --modversion egl gl`, `ldconfig -p`, and Python `importlib.metadata`. Missing distribution metadata does not exclude a source checkout. Actual GL vendor/device matching requires a later controlled probe; CUDA visibility masks must not be assumed to constrain EGL enumeration.

OSC provides Nsight through NVHPC and documents DCGM counter contention. For a later authorized profiling allocation, append `--gres=nsight` to the allocation request; this uses OSC's supported mechanism instead of manually stopping DCGM. Use `module spider nvhpc` and the site's compatible module setup before loading a profiler; do not change the first inventory's module baseline. OSC's default GPU compute mode is shared, allowing multiple CUDA processes; that is not a guarantee of graphics/CUDA spatial execution or permission to use another job's GPU. [S19]

### Verification already performed for the diagnostic deliverable

After generalizing the collector to all OSC sites, `bash -n` passes, its embedded Python parses, and a guard smoke run without `SLURM_JOB_ID` exits 2 with the expected refusal message. No OSC allocation, NVIDIA inventory, EGL rendering, or CUDA execution has been run by this assistant; remote output is still required.

## Approach

This plan specifies a corrected feasibility assessment and ordered validation gates. Do not implement a VUDA shim, replace the renderer, or migrate physics under this request.

### 1. Reconstruct VUDA and establish the native baseline

Start the assessment with the mentor's three questions: root cause, concrete VUDA implementation, and OpenGL prototype feasibility. Produce a version-specific component map: v1 forwarding channels and kernel-module page-table changes versus v2 user-space shim, allocation interception/object duplication, preserved data subcontexts, shared control mappings, shadow-channel partitioner, and daemon. Distinguish modified application/runtime interfaces from modified driver binaries; do not claim v2 patches the kernel driver. If author source is available, identify actual files/functions and changed modules from it; otherwise report paper-level components without inventing source paths.

Use MuJoCo/robosuite's native hardware OpenGL path, normally EGL for headless Linux, as the experimental baseline. `MUJOCO_GL=egl` and `PYOPENGL_PLATFORM=egl` must be selected before relevant imports in the worker process. Use the existing job launcher rather than inventing an application entrypoint. Enabling this baseline is not itself evidence of spatial sharing.

The deployed fork's effective backend selection must be read before changing its configuration. The reference source is `robosuite/utils/binding_utils.py` and `robosuite/renderers/context/egl_context.py` at v1.4.0. If the fork uses another selection mechanism, use that mechanism instead of assuming the environment variables override it. Never diagnose software rendering from high CPU utilization or a slow `render()` call alone.

Preserve the user's reported CPU-rendering-plus-model deployment as baseline A. Define baseline B as GPU-backed rendering with model work disabled, baseline C as ordinary same-GPU rendering plus unchanged model work, and baseline D as the model alone using fixed recorded inputs. Compare A against C for the offload effect; use B and D as solo references for co-location interference. Establish these baselines before any channel-redirection experiment.

### 2. Validate GPU placement and split remaining host costs

Record GPU UUID/model, driver, CUDA, OS, graphics libraries/container access, exact simulator/framework versions, model/checkpoint, camera settings, process topology, and peak device memory. Read graphics vendor/renderer/version from the actual current LIBERO rendering context, not a separate desktop context. Match the selected EGL GPU to the CUDA device physically; visibility-mask ordinals alone are insufficient.

Measure CPU physics, `mjv_updateScene`, rendering submission/completion, `mjr_readPixels`, preprocessing, and host-to-device image transfer separately. Keep task, initial states/actions, camera count/resolution, visual effects, and control frequency unchanged between baselines. Accept normal cross-backend image rounding differences only after checking geometry, orientation, camera identity, image format, reward, and termination; do not require byte-identical software/hardware rasterization.

If hardware rendering works, proceed to scheduling diagnosis; do not stop at GPU offload as though the research objective were met. If it fails, record the concrete backend/library/device-access failure and complete all reachable paper/source feasibility analysis; do not silently reinstall drivers or claim a scheduling barrier.

### 3. Measure co-location with inference and training separately

Use independent ready work. For inference, render environment group A while computing actions for already-rendered group B; preserve per-environment observation/action freshness. For training, retain the existing algorithm's update barriers and policy versions. If the workflow exposes no independent learner work, report that constraint instead of changing its learning semantics.

Compare A, B, and C above using end-to-end transitions/s, frame/action-latency tails, model throughput, CPU load, and device-memory peak. Investigate SM execution pressure, device-memory capacity and bandwidth, and L2 interference independently. Use hardware counters where supported; a throughput slowdown alone does not identify L2 contention. Compare solo renderer/model measurements to co-running measurements rather than equating high utilization with useful work.

If ordinary co-location already demonstrates spatial execution, document the conditions and explain why redirection is unnecessary there. Throughput improvement alone does not establish this result. If host costs or application barriers dominate, distinguish them from driver scheduling in the root-cause report. In either case, complete the VUDA mechanism and OpenGL adaptation analysis; the research deliverable is not just a faster EGL configuration.

### 4. Test whether the graphics backend changes the scheduling barrier

Use a minimal native NVIDIA EGL/OpenGL plus CUDA probe with disjoint buffers before changing LIBERO. Compare render-only, CUDA-only, explicitly serial, and independently submitted execution in-process and across processes. Include both short low-resource CUDA work and representative model kernels. Separate lack of ready work, explicit synchronization, and resource saturation from TSG isolation.

Use a corresponding Vulkan/CUDA probe on the same GPU with matched scene/output and compute work for backend-specific claims. Use ManiSkill's existing Vulkan stack as an application-level reference, with CPU/GPU physics differences explicitly reported; do not call a LIBERO/ManiSkill comparison an API-only experiment.

Correlate GPU workload tracing, context switches, hardware activity, correctness, and unprofiled throughput. Overlapping CPU calls or timestamp intervals alone do not establish simultaneous active execution. If the current stack already supports useful co-execution, do not add TSG redirection.

### 5. Gate a native OpenGL VUDA port on demonstrated need

Identify the authors' actual v2 artifact early, independently of hardware profiling, because implementation/module analysis is a required deliverable. When available, inspect it without changing the deployed driver. If scheduling isolation remains on the target, reproduce the Vulkan/CUDA mechanism before attempting redirection on GL. The native GL question is whether its TSG can host selected CUDA channels while preserving graphics initialization, CUDA data subcontexts, and shared control-state mappings.

Inspect target driver allocations rather than inventing ioctl layouts, GPU commands, control mappings, privilege requirements, or address-space assumptions. If the actual artifact or required procedure is unavailable, report independent reverse engineering as the prerequisite; do not substitute external-memory interop, analytical “compute windows,” or another project called VUDA.

Keep scheduler changes, GPU-resident image handoff, and resource partitioning as separate ablations. Start any later resource-sharing evaluation with a fixed partition; dynamic quotas require transient-latency validation. Complete the report with separate verdicts for GPU offload, ordinary co-location, simultaneous execution, and learning benefit.

The final feasibility deliverable must answer each mentor question explicitly: (1) observed and inferred causes of serialization in the target GL/CUDA stack; (2) VUDA's mechanism and version-specific implementation/module changes; (3) the smallest credible OpenGL/EGL prototype boundary, required driver/GPU features, reusable versus unverified components, and concrete reasons any adaptation is costly or infeasible. Keep the LIBERO renderer OpenGL for this assessment: adapt the VUDA graphics-side integration rather than silently replacing LIBERO's renderer with Vulkan. A GPU-model change is justified only by an observed missing capability or a documented artifact requirement, not presumed necessary for ordinary GPU rendering.

## Verification

### Checks actually performed

- Read local root and README.md: the repository is an exploration shell, not an existing VUDA implementation.
- Read both supplied images, including the clarified mentor brief. Rechecked MuJoCo's documented hardware EGL versus software OSMesa paths and robosuite v1.4.0's backend-selection source. This establishes supported capability, not the cause of CPU rendering in the user's deployment.
- Read v1 and v2 mechanism/evaluation sections, official CUDA/OpenGL registration and map/unmap contracts, MuJoCo rendering/resource docs, upstream LIBERO and pinned robosuite source, RLinf simulator documentation, and RLinf Figure 7's explanation.
- Ran `uname -s -m && command -v nvidia-smi`. Output was `Darwin arm64`; no `nvidia-smi` path was returned and the command exited 1. No connected Linux/NVIDIA target was supplied. This is a limitation of local verification, not evidence for or against the port.
- No GPU concurrency experiment, throughput benchmark, training run, inference run, or port smoke test has executed. No performance claim in this assessment is a measured LIBERO result.

### Concrete follow-on commands and expected evidence

Run on the intended Linux/NVIDIA machine, not on the current macOS workstation:

```bash
nvidia-smi -L
nvidia-smi --query-gpu=uuid,name,driver_version,memory.total,memory.used,compute_mode --format=csv
nsys --version
nsys status --environment
python -c 'import importlib.metadata as m; print({p: m.version(p) for p in ("mujoco", "robosuite", "torch")})'
```

Expected: an NVIDIA device, recorded exact versions, profiler capability information, and installed simulator/model runtime versions. A missing package or executable is a missing prerequisite, not an invitation to silently install or change drivers.

For the supported headless hardware-rendering baseline, set the following in the existing worker launch environment before importing MuJoCo, robosuite, or PyOpenGL:

```bash
export MUJOCO_GL=egl
export PYOPENGL_PLATFORM=egl
```

Use the deployed fork's existing `render_gpu_device_id` / `MUJOCO_EGL_DEVICE_ID` mechanism after checking its visibility-mask mapping; do not choose an arbitrary ordinal. Record the actual worker context's graphics vendor, renderer, and version, and confirm NVIDIA hardware placement on the intended CUDA GPU. An environment variable, successful context creation, or a CUDA-visible process alone does not establish the rendered workload's placement.

New-behavior check: run the same fixed task, initial state, action sequence, cameras, and image dimensions under the reported CPU baseline and hardware EGL baseline. Expected: NVIDIA GPU rendering produces the intended camera observations and unchanged physics/reward/termination behavior. Report measured frame and transition rates; do not prescribe a speedup threshold before observing baseline costs. Keep image rounding differences separate from wrong cameras, orientation, formats, or stale frames.

An already-installed RLinf checkout with its downloaded OpenVLA-OFT checkpoint and both actor/rollout `model_path` entries configured can run the documented reference recipe from the RLinf checkout root:

```bash
bash examples/embodiment/run_embodiment.sh libero_10_grpo_openvlaoft
```

This launches real training, not an inference-only microbenchmark. Do not run it on a shared device or with an unconfigured checkpoint just to obtain a trace. Record the recipe's component placement first; a disaggregated default is not a same-GPU test.

For a configured, finite run of that recipe, the documented Nsight options support:

```bash
nsys profile --trace=cuda,opengl,nvtx,osrt --opengl-gpu-workload=true --sample=none --output=libero-baseline bash examples/embodiment/run_embodiment.sh libero_10_grpo_openvlaoft
```

Use an unused output name rather than overwriting a report. Add `--gpuctxsw=true` only where the target supports context-switch capture. Add GPU metrics only after `nsys profile --gpu-metrics-devices=help` reports supported devices and required access. Profiler-permission failures do not establish a scheduling limitation.

The OpenGL tracer batches draws and uses timestamp queries. Device metrics do not identify processes/contexts by themselves. Correlate CUDA activity, GL workload ranges, context switches, device hardware metrics, and unprofiled throughput. If that evidence cannot distinguish preempted intervals from simultaneous active execution, label co-residency inconclusive rather than claiming concurrency. [S15]

For a later native-GL scheduling probe, use independent render and CUDA inputs: expected output matches solo execution using the same backend, with both workloads making progress and verified scheduling evidence. For later LIBERO inference, use fixed initial states and action replay first: image geometry, camera identity, orientation, format, freshness, and state/reward/termination sequences must match the baseline semantics before running a model. Do not require byte-identical pixels when comparing software and hardware rendering. Replay is a correctness test, not evidence of closed-loop policy speedup.

For each later performance configuration, discard initialization/warmup, run five equal-duration steady-state repetitions, and report medians plus ranges. Measure completed environment transitions/s, camera images/s, learner updates/s, p50/p95/p99 action and frame latency, CPU physics/observation time, readback and host-to-device transfer time, device memory peak, and GPU compute/graphics/memory activity. Keep model, task, seeds, camera settings, CPU allocation, and algorithm schedule fixed. Report unprofiled performance separately from profiling runs.

For training, compare task success/return against environment transitions and wall time across identical seeds. Faster transitions alone do not establish faster learning.

## Assumptions and contingencies

- Native OpenGL/EGL and LIBERO benchmark semantics remain the target. New MuJoCo documentation also describes Filament/Vulkan and an `mjr` compatibility option, but it warns that appearance differs from the classic renderer. Renderer migration is a separate project, not proof of the requested native OpenGL port. [S9]
- The mentor has confirmed spatial sharing as the research objective; ordinary GPU rendering and time-shared co-location are baselines only. The user now requests OSC diagnostic commands and a public artifact search, not port execution. Until remote output is supplied, complete source-based mechanism/module analysis and distinguish empirical hardware/driver hypotheses from observations.
- OSC Pitzer/V100, Ascend/A100, and Cardinal/H100 access is known; actual allocated variants, site drivers, native graphics capability, installed software, profiler permissions, RL schedule, and model/checkpoint remain unknown. Use the explicit temporary defaults above. Do not assume architecture-family matching establishes VUDA compatibility, CPU HBM belongs to the CUDA GPU, or a checkpoint is available.
- The paper's actual v2 implementation is the preferred starting point for any later port. If it cannot be obtained, scope the next project explicitly as independent reverse engineering, not a routine API adaptation.
- No native OpenGL route is documented in the reviewed NVIGI/CiG support tables. Their documented D3D12/Vulkan paths are not evidence that GL support is impossible. MPS/green contexts do not by themselves establish graphics partitioning; selected newer graphics-capable MIG products must not be generalized to all MIG GPUs. These are hardware-specific alternatives, not the default native port plan. [S1 §2.3, S16]

## Primary sources and exact anchors

- **S1 — VUDA v2:** https://arxiv.org/html/2605.01352v2 ; §§1–2 scheduling premise, §§3–6 API/mechanisms, §7.1 platforms, §7.2 partition transients, §7.3 simulation semantics, §7.4 empirical inference SLO, §8 limitations.
- **S2 — VUDA v1:** https://arxiv.org/html/2605.01352v1 ; §6.4 rollout evaluation, especially §6.4.2 separate VLA inference GPU.
- **S3 — LIBERO reference dependencies:** https://raw.githubusercontent.com/Lifelong-Robot-Learning/LIBERO/master/requirements.txt ; `robosuite==1.4.0`.
- **S4 — robosuite v1.4.0 dependencies:** https://raw.githubusercontent.com/ARISE-Initiative/robosuite/v1.4.0/setup.py ; `mujoco>=2.3.0`.
- **S5 — LIBERO wrapper:** https://raw.githubusercontent.com/Lifelong-Robot-Learning/LIBERO/master/libero/libero/envs/env_wrapper.py ; `ControlEnv.__init__`, `ControlEnv.step`, `OffScreenRenderEnv`.
- **S6 — robosuite stepping:** https://raw.githubusercontent.com/ARISE-Initiative/robosuite/v1.4.0/robosuite/environments/base.py ; `MujocoEnv.step`, around lines 355–405.
- **S7 — camera observable:** https://raw.githubusercontent.com/ARISE-Initiative/robosuite/v1.4.0/robosuite/environments/robot_env.py ; `_setup_observables`, `_create_camera_sensors`, around lines 320–430.
- **S8 — render/physics binding:** https://raw.githubusercontent.com/ARISE-Initiative/robosuite/v1.4.0/robosuite/utils/binding_utils.py ; `MjRenderContext` around lines 60–210, `MjSim.step`/`render` around lines 1080–1140. v1.4.0 tree SHA observed: `fbee5844ff5632f5b5698e204ec5357ca50be0df`.
- **S9 — MuJoCo rendering:** https://mujoco.readthedocs.io/en/stable/programming/rendering.html ; classic renderer, Context and GPU resources, Buffers for rendering; Filament appearance caveat.
- **S10 — EGL setup:** https://raw.githubusercontent.com/ARISE-Initiative/robosuite/v1.4.0/robosuite/renderers/context/egl_context.py ; `EGLGLContext.__init__`, global `EGL_DISPLAY`, `eglCreateContext`.
- **S11 — CUDA/OpenGL contracts:** https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__OPENGL.html and https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__INTEROP.html ; GL registration restrictions, `cudaGLGetDevices`, `cudaGraphicsMapResources`, `cudaGraphicsUnmapResources`, mapped-pointer lifetime.
- **S12 — non-author search result disclaimer:** https://raw.githubusercontent.com/timfox/idTech3/main/docs/VUDA.md ; Research vs engine, Limitations. Use only to reject it as the paper artifact.
- **S13 — texture/material ownership:** https://mujoco.readthedocs.io/en/stable/XMLreference.html#asset-texture ; `type=cube`, same-face one-copy statement; `body/geom material` shared-reference semantics.
- **S14 — RLinf deployment and scheduling:** https://rlinf.readthedocs.io/en/latest/rst_source/examples/embodied/libero.html and https://www.usenix.org/system/files/osdi26-yu-chao.pdf ; §3.3 and Figure 7, exclusive `device_lock`, spatial placement, temporal offload.
- **S15 — profiler semantics:** https://docs.nvidia.com/nsight-systems/UserGuide/index.html ; CLI Profile Command Switch Options, OpenGL Trace, GPU Context Switch, GPU Metrics.
- **S16 — supported vendor alternatives:** https://docs.nvidia.com/nvigi-sdk/1.5.0/docs/nvigi_core/docs/GpuSchedulingForAI.html ; Supported graphics APIs and Limitations. https://docs.nvidia.com/deeplearning/tensorrt-rtx/latest/inference-library/compute-graphics.html ; CiG availability and release-specific D3D-only TensorRT-RTX limitation.
- **S17 — Pitzer hardware:** https://www.osc.edu/resources/technical_support/supercomputers/pitzer and https://www.osc.edu/resources/technical_support/supercomputers/pitzer/guidance_on_requesting_resources_on_pitzer ; node heterogeneity and V100 memory variants.
- **S18 — OSC interactive allocation:** https://www.osc.edu/supercomputing/batch-processing-at-osc/job-submission ; `salloc`/`srun` and compute-node shell behavior. https://slurm.schedmd.com/faq.html ; interactive-step versus ordinary `srun --pty` resource consumption.
- **S19 — OSC GPU/profiling policy:** https://www.osc.edu/resources/technical_support/supercomputers/gpu_computing and https://www.osc.edu/resources/available_software/software_list/nvhpc ; `--gpus-per-node=1`, shared compute mode, NVHPC Nsight, and `--gres=nsight`.
- **S20 — V100 capability:** https://developer.nvidia.com/cuda-legacy-gpus ; V100 compute capability 7.0. TensorRT-RTX SCG scope is S16, not a universal statement about all Volta hardware mechanisms.
- **S21 — bounded artifact search:** https://github.com/Xubbbb ; https://api.github.com/users/Xubbbb/repos?per_page=100&type=owner ; https://api.github.com/search/repositories?q=%222605.01352%22&per_page=10 ; https://api.github.com/search/repositories?q=VUDA+spatial&per_page=10 ; https://ipads.sjtu.edu.cn/pub/members/jinyu_gu ; https://github.com/jgbit/vuda . The `bin_xu` institutional URL tried during discovery returned an unrelated SDC paper and is not artifact evidence.
- **S22 — additional OSC hardware:** https://www.osc.edu/resources/technical_support/supercomputers/ascend ; A100 40/80-GB nodes and versioned module requirement. https://www.osc.edu/resources/technical_support/supercomputers/cardinal ; H100 94-GB GPU nodes, their CPU configuration, and cluster host. https://www.nvidia.com/en-us/data-center/a100/ ; Ampere architecture.
- **S23 — bounded graphics-support evidence:** https://forums.developer.nvidia.com/t/nvidia-a100-opengl-drawing-issue/196672 ; user-reported OpenGL rendering and driver-specific corruption on A100, not OSC certification. https://forums.developer.nvidia.com/t/h100-pcie-doesnt-have-graphic-support/246884 ; response warns ordinary rendering is not the default product use. https://docs.nvidia.com/ai-enterprise/release-8/latest/infra-software/vgpu/reference/hopper-h100.html ; virtualization-specific scope, not a bare-metal graphics capability test.
