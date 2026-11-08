# OSC Cluster Operations Runbook: Ascend, Cardinal, and Pitzer

This runbook documents operational procedures for conducting VUDA-Libero-Port preliminary experiments on the Ohio Supercomputer Center (OSC) clusters: **Ascend** (primary Ampere A100 testbed), **Cardinal** (Hopper H100 testbed), and **Pitzer** (Volta V100 legacy comparator).

**Host Lab vs. Temporary Cluster Testbed:** The primary target research environment for this project is the host lab at **EPFL**. OSC cluster access (Ascend, Cardinal, Pitzer) serves solely as a **temporary preliminary testbed** while awaiting EPFL GPU provisioning. All cluster environments, toolchain checks, and baseline probes documented herein are preparatory steps to de-risk native OpenGL and CUDA co-execution before porting to the permanent EPFL infrastructure.

---

## 1. Safety, Authorization, and Multi-Tenant Etiquette

### Mandatory Account Placeholder
- **CRITICAL:** Always replace `YOUR_OSC_ACCOUNT` (without angle brackets) with your authorized research project account code.
- **Coursework Allocation Account Caution:** In initial exploratory jobs (e.g., Job `8374257`), an academic course allocation account was observed from the local-only coursework reference batch script at `evidence/reference/module6_assignment_cnn_intro.ipynb`. That identifier corresponds to a local academic course allocation, **not** an authorized general research project grant. Do not assume or reuse course allocations for unauthorized experimentation.
- Ensure all allocation requests are charged to an authorized project account confirmed by your research mentor.

### Project Safety Precautions
OSC clusters are shared multi-tenant resources. The following operational boundaries represent project safety precautions to maintain system stability and respect site resources:
1. **Never Execute on Login Nodes:** Run GPU binaries, intensive compilation, simulation loops, and the collector only in an allocated compute-node shell. Use login nodes for file management, editing, and job submission.
2. **No Privileged Operations:** Do not attempt `sudo`, `su`, package manager installations (`dnf`, `yum`, `apt`), or kernel module loading/unloading (`insmod`, `rmmod`).
3. **No Hardware Resets or Daemons:** Do not execute `nvidia-smi -r` (GPU reset). Do not launch background system daemons (such as `nvidia-cuda-mps-control`) without prior site review and permission.
4. **No Unvalidated Low-Level IOCTLs:** Do not issue raw, unvalidated `ioctl` commands against `/dev/nvidia*` or `/dev/nvidia-uvm` control devices.
5. **Ephemeral Storage Clean-up:** Use `$TMPDIR` on compute nodes for temporary scratch files, and clean up temporary test files upon job completion.

---

## 2. Cluster Hardware & Published Node Families Reference

The table below lists the published hardware families across the three OSC clusters, along with observed node-level attributes:

| Cluster | GPU Accelerator Family | Published Memory Options | Observed Compute Node Details (Job 8374257) |
| :--- | :--- | :--- | :--- |
| **Ascend** | NVIDIA A100-PCIE | 40-GB and 80-GB variants | Compute node `NODE_A`: NVIDIA A100-PCIE-40GB (Compute Capability 8.0, 40,960 MiB memory, ECC on, MIG disabled), AMD EPYC 7H12 64-core host CPU, RHEL 9.6 (Plow) |
| **Cardinal** | NVIDIA H100-PCIE | 94-GB variants | Intel Xeon Platinum 8470 host CPUs (cluster architecture documented in S22; specific node OS/kernel unobserved) |
| **Pitzer** | NVIDIA Tesla V100 | 16-GB and 32-GB variants | Heterogeneous host configurations (documented in S17; specific node OS/kernel unobserved) |

---

## 3. Transferring Files and Collector Scripts

To stage diagnostic scripts or experimental source code onto OSC from your local machine:

```bash
# Set your OSC username and target cluster login host
export OSC_USER="your_username"
export OSC_HOST="ascend.osc.edu" # Options: ascend.osc.edu, cardinal.osc.edu, pitzer.osc.edu

# Create project working directory on OSC NFS home
ssh "${OSC_USER}@${OSC_HOST}" "mkdir -p ~/vuda-workspace/scripts ~/vuda-workspace/evidence"

# Transfer diagnostic script to OSC
scp scripts/pitzer-diagnostics.sh "${OSC_USER}@${OSC_HOST}:~/vuda-workspace/scripts/"

# Ensure executable permissions on remote host
ssh "${OSC_USER}@${OSC_HOST}" "chmod +x ~/vuda-workspace/scripts/pitzer-diagnostics.sh"
```

---

## 4. Requesting Slurm GPU Allocations

### Standard Interactive GPU Allocation
Use `salloc` to request an interactive compute node allocation. The following command matches the baseline parameters used in initial Ascend inventory job 8374257 (1 GPU, 4 CPU cores, 16 GB system RAM, 20 minutes):

```bash
salloc --account=YOUR_OSC_ACCOUNT \
       --nodes=1 \
       --ntasks=1 \
       --cpus-per-task=4 \
       --mem=16G \
       --gpus-per-node=1 \
       --time=00:20:00
```

### Profiler-Enabled GPU Allocation (Nsight DCGM Contention Handling)
On OSC systems, `--gres=nsight` is the documented allocation option to mitigate DCGM service contention with NVIDIA profiling tools. Raw inventory on Ascend node `NODE_A` observed `RmProfilingAdminOnly: 0`, indicating profiling counters were not restricted to root on that host. EXP-007 subsequently collected GPU metric and context-switch records with site-provided Nsight `2025.6.3` in an authorized user job, without privilege escalation or manual service changes. This does not establish every node's access or DCGM state; retain the documented allocation request and actual diagnostics.

For profiling sessions (where additional time may be required, e.g., 30 minutes):

```bash
salloc --account=YOUR_OSC_ACCOUNT \
       --nodes=1 \
       --ntasks=1 \
       --cpus-per-task=4 \
       --mem=16G \
       --gpus-per-node=1 \
       --gres=nsight \
       --time=00:30:00
```

---

## 5. Compute Node Verification & Shell Attachment

### Verifying Hostname vs. Allocation NodeList
When `salloc` succeeds, determine whether you have been placed directly in a compute-node shell or remain on the login node:

```bash
# Check current hostname
hostname -f

# Check assigned Slurm node list
echo "Assigned Node: $SLURM_JOB_NODELIST"
```

### Entering the Compute Node Shell
- If `hostname -f` displays a login node while `$SLURM_JOB_NODELIST` identifies an allocated compute node:
  **DO NOT execute GPU commands in the current shell.**
- Enter the assigned compute node interactively using `srun`:
  ```bash
  srun --overlap --ntasks=1 --pty bash -l
  ```
- Confirm that `hostname -f` now reflects the compute node (e.g., `NODE_A`).

### Matching Physical GPU Identity Before Rendering

NVML inventory row count is not the number of allocated CUDA-visible devices. EXP-007's first attempt refused a two-row NVML inventory despite one CUDA-visible GPU. Do not select the first NVML row, remove allocation guards, or equate Slurm indices, CUDA ordinals, EGL indices, and Linux device minors.

EXP-008 changed correctness checking, error propagation, and latency reporting, but the cross-API UUID guard still remains in a separate experiment control rather than integrated in the public probe. Keep historical EXP-007 results unchanged.

### Batch entrypoint paths and planning notes

When submitting batch jobs via `sbatch`, Slurm may execute a copied script from its internal spool directory where `dirname "$0"` does not point to the actual repository workspace root. EXP-008's first batch attempt failed when writing its execution log there before compilation occurred. Always pass and require an explicit workspace root variable, and preserve failed job attempts separately rather than overwriting them. See [EXP-008 record](../evidence/public/exp008-correctness-latency.md).

For structured multi-condition matrices like [EXP-009](../research/EXP009_CONTROLLED_SCHEDULING_PLAN.md), total walltime must be computed from warmup iterations, measurement windows, repeat counts, validation runs, build time, and queue drain buffers; a five-minute smoke test is insufficient for the full experiment budget. This planning note does not allocate new GPU jobs, introduce new probe command-line flags validated by code, change normal allocation guidance, or modify profiler `--gres=nsight` flags. Private cluster endpoints, account strings, node lists, and hardware UUIDs remain excluded from public documentation.
---

## 6. Running System Diagnostics & Collecting Evidence

The diagnostic collector script (`scripts/pitzer-diagnostics.sh`) outputs all inventory data directly to `stdout`. It does not write to a log file internally.

Execute the script inside the interactive compute-node shell, piping through `tee` to capture output:

```bash
cd ~/vuda-workspace
mkdir -p evidence

# Run diagnostic collector script and capture all output
set -o pipefail
bash scripts/pitzer-diagnostics.sh 2>&1 | tee "evidence/osc-system-info-${SLURM_CLUSTER_NAME:-cluster}-${SLURM_JOB_ID}.txt"
```

### Retrieving Evidence to Local Workspace
From your local development workstation, retrieve the captured log file into the local-only evidence hierarchy:

```bash
export OSC_USER="your_username"
export OSC_HOST="ascend.osc.edu"
export JOB_ID="8374257" # Replace with actual Slurm Job ID

mkdir -p "evidence/osc/ascend/${JOB_ID}"
scp "${OSC_USER}@${OSC_HOST}:~/vuda-workspace/evidence/osc-system-info-*.txt" "evidence/osc/ascend/${JOB_ID}/"
```

*(Note: Raw captures saved to `evidence/osc/` are local-only and excluded from version control. A sanitized summary of the Ascend baseline is documented in [`evidence/public/ascend-inventory.md`](../evidence/public/ascend-inventory.md).)*

---

## 7. Software Modules & Toolchain Setup

System build compilers (`cc`, `gcc`, `g++`, `cmake`) and graphics libraries (`libEGL.so.1`, `libGL.so.1`) are already present in `/usr/bin` system paths on compute nodes.

### Preserving Baseline Environment & Loading CUDA
Do not run `module purge` or reload base cluster modules unnecessarily, as this may alter default system toolchains.

On Ascend (verified on node `NODE_A`):
```bash
# Check CUDA module availability
module spider cuda

# Load the verified CUDA 12.8.1 toolkit module
module load cuda/12.8.1

# Verify nvcc compiler availability
nvcc --version
```

### Profiler Modules (Separate Step)
Loading `nvhpc` is a separate profiling step. Check availability first:
```bash
module spider nvhpc
```
Note: Loading an `nvhpc` module (e.g., `nvhpc/25.1`) may alter compiler paths or runtime libraries. Verify environment settings when switching between development and profiling sessions.

**Cross-Cluster Caution:** Do not generalize Ascend module versions (`cuda/12.8.1`, `nvhpc/25.1`) to Cardinal or Pitzer. Always query `module spider` on the target cluster first.

### Selecting the Newer Bundled Profiler Independently

EXP-006 verified that Ascend's `cuda/13.2.1` bundles Nsight Systems `2025.6.3.541`; `nvhpc/25.1` still bundles `2024.7.1.84`. A missing named `nsys` module does not imply a missing profiler. Discover the binary, then restore the established build toolkit:

```bash
module load cuda/13.2.1
NSYS_BIN="$(command -v nsys)"
module load cuda/12.8.1
nvcc --version
"$NSYS_BIN" --version
```

These commands ran in disposable login and allocated shells; EXP-007 later successfully used that selection for native application traces. Use the explicit `NSYS_BIN`, not whichever older `nsys` remains on `PATH`; do not migrate the application to CUDA 13.2 merely to select the profiler. Recheck versions on every new target. See [discovery](../evidence/public/exp006-live-prerequisites.md) and [actual capture](../evidence/public/exp007-profile-results.md).

### Python & Conda Environment Setup
Ascend provides a preconfigured Miniconda module:
```bash
# Check and load Miniconda module (Conda 24.1.2, Python 3.10.13)
module spider miniconda3
module load miniconda3/24.1.2-py310
```

### Workspace Environment Setup Helper
A helper script is provided at `~/vuda-workspace/scripts/env-setup.sh`:
```bash
# Source the helper script to load tested CUDA and Miniconda modules
source ~/vuda-workspace/scripts/env-setup.sh
```
### Verifying Native Graphics Libraries
Check that system OpenGL and EGL headers and libraries resolve correctly:

```bash
# Verify pkg-config versions for EGL and OpenGL
pkg-config --modversion egl gl

# Verify linker resolution
pkg-config --libs egl gl
# Expected output: -lEGL -lGL
```

---

## 8. Profiling Workflow with Nsight Systems (Supported Template)

When running inside an allocation requested with `--gres=nsight`:

- **Timing is not active execution:** `--opengl-gpu-workload=true` captures GPU-side batched elapsed timing using `glQueryCounter()`, not per-operation active SM occupancy. Overlapping GL/CUDA ranges do not prove spatial sharing or TSG membership. See [Nsight Systems 2024.7 OpenGL Trace](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html#opengl-trace).
- **Supported Command Template:**
  ```bash
  "${NSYS_BIN:?Select and verify the profiler first}" profile \
       --trace=cuda,opengl,osrt,nvtx \
       --opengl-gpu-workload=true \
       --sample=none \
       --output="vuda_native_probe_${SLURM_JOB_ID}" \
       ./path/to/executable [arguments...]
  ```
- Resulting `.nsys-rep` files can be downloaded and inspected in the NVIDIA Nsight Systems GUI.
- Use a new output basename for each capture/export. EXP-003 produced a report but its final SQLite export failed because the destination already existed; preserve that error and both original artifacts.
- EXP-006 subsequently re-exported the preserved report to an unused file. All 91 table schemas and all 90 non-export-metadata row multisets matched the original SQLite export. The historical export error remains preserved; content correspondence is now verified.
- Hardware metrics and GPU context-switch collection are separate capabilities. Check supported options and site permissions; `--gres=nsight` addresses DCGM contention, not every profiler permission or undocumented driver operation.
- **Compatibility gate:** EXP-003 diagnostics warned that Nsight `2024.7.1.84` did not support the installed CUDA-driver compatibility version `13.0` and used `12.6` tracing libraries. Check a site-provided profiler's documented driver support before new decisive collection; do not change the driver or install tools automatically. The recorded toolkit `12.8.1` is a separate version.
- **Collection is opt-in:** EXP-003 metadata records `COLLECT_GPU_CTX_SW_TRACE=false`; absent switch events are not proof of an unsupported device or denied capability. CPU sampling was also throttled four times. Follow the [staged Gate 4 protocol](../research/GATE4_CAPTURE_PROTOCOL.md), retaining diagnostics and all failed capability checks.
- **Scheduling telemetry is version/site-specific:** the [2024.7 guide](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html#gpu-context-switch) states a root-permission/R435.17+ requirement but also describes normal-user records with reduced information for other users. EXP-006's newer CLI help omitted that old root wording; EXP-007 actually collected GPU switches and metrics through the site's existing user-job configuration. Do not generalize old requirements or this success to every tool/node/scope. Ask staff about actual access failures, broader observation, and privacy/isolation; do not use `sudo` or change services/sysctls.
- **Actual newer capture:** EXP-007 used `--cpuctxsw=none` plus `--gpuctxsw=true --gpu-metrics-devices=cuda-visible --gpu-metrics-set=ga100 --gpu-metrics-frequency=1000` for instrumented runs. The CPU scheduling warning limits CPU thread-state reconstruction, not the populated GPU switch tables. CUDA hardware tracing was unavailable and the tool explicitly used software instrumentation; the old unsupported-driver warning was absent. These are actual outcomes, not a portable permission guarantee.
- **Interpretation remains bounded:** restore/save events are not exact active-warp boundaries, scheduling IDs are not automatically RM TSG handles, and metric samples are device aggregates. No save-to-restore gap or throughput threshold proves causal switching cost or spatial sharing. The [EXP-007 report](../evidence/public/exp007-profile-results.md) retains all-mode correctness and matched-control gaps.
- **CUDA hardware backend is architecture-gated:** the [2025.6 guide](https://archive.docs.nvidia.com/nsight-systems/2025.6/UserGuide/index.html#cuda-trace) supports HES tracing on Blackwell-or-later GPUs, not this A100/Ampere target. Existing `--trace=cuda` already requests the default backend; inspect Diagnostics Summary for software fallback. The installed help advertises `cuda`/`cuda-sw`; do not copy `cuda-hw` from a different version. Do not request another A100 allocation or change privileges/toolkits solely to enable unsupported hardware. See the [supported-target template and evidence limit](../evidence/public/exp007-profile-results.md#cuda-hardware-tracing-prerequisites-and-20256-cli-behavior).

---

## 9. Session Termination & Resource Release

Always release compute nodes promptly when experimentation concludes:

```bash
# Exit the srun compute-node subshell
exit

# Exit the salloc allocation shell (terminates the Slurm job)
exit

# If a background job remains running, terminate it explicitly:
export JOB_ID="8374257" # Replace with actual Slurm Job ID
scancel "${JOB_ID}"

# Verify no orphaned allocations remain active
squeue -u $USER
```

---

## 10. Gate 5: Source Review Before Internal Driver Experiments

The [Gate 5 assessment](../research/GATE5_ASSESSMENT.md) and [exact-release driver map](../research/GATE5_DRIVER_ASSESSMENT.md) record the current decision: source assessment complete, runtime channel redirection blocked. Native probe timings do not establish that redirection is necessary.

Official [OSC GPU guidance](https://www.osc.edu/resources/technical_support/supercomputers/gpu_computing) permits multiple GPU tasks in one job and documents `--gres=nsight` for DCGM contention. It does not settle permission for RM object duplication, channel/subcontext reparenting, shared control mappings, or undocumented SCG commands. The [User Code of Ethics](https://www.osc.edu/supercomputing/support/codeofethics) requires a PI-authorized account purpose and respect for system integrity.

The restrictions in §1 are project safety precautions, not a claim that OSC publishes a blanket ban on user-space interception. Request a specific site decision before runtime allocation observation or mutation. The [unsent inquiry](../research/GATE5_ASSESSMENT.md#unsent-support-inquiry) discloses each stage separately; [OSC support](https://www.osc.edu/resources/technical_support) lists `oschelp@osc.edu`. No message has been sent by this assessment.

Before mutation, require the author artifact or a complete reviewed independent design, actual GL object/control-mapping evidence, an approved isolated allocation, and site-defined fault monitoring/recovery. A non-mutating observer still runs on the target; do not label it offline or risk-free. One allocated GPU does not guarantee exclusive use of the host/driver.

Do not derive ioctl layouts, privileges, or GPU command words from paper pseudocode. GSP duplication paths exist in the published `580.95.05` host source, but firmware acceptance and native GL compatibility remain unverified. EPFL requires its own inventory and approval; its permanent-target status does not grant unrestricted access.
