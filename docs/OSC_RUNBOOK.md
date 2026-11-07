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
On OSC systems, `--gres=nsight` is the documented allocation option to mitigate DCGM service contention with NVIDIA profiling tools. Note that raw inventory on Ascend node `NODE_A` observed `RmProfilingAdminOnly: 0` (indicating non-root profiling counters were not restricted by the driver on that host), but `--gres=nsight` remains OSC's supported mechanism to request profiling-safe execution without DCGM interference. Actual hardware counter access remains untested in this project.

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
  nsys profile \
       --trace=cuda,opengl,osrt,nvtx \
       --opengl-gpu-workload=true \
       --sample=cpu \
       --output="vuda_native_probe_${SLURM_JOB_ID}" \
       ./path/to/executable [arguments...]
  ```
- Resulting `.nsys-rep` files can be downloaded and inspected in the NVIDIA Nsight Systems GUI.
- Use a new output basename for each capture/export. EXP-003 produced a report but its final SQLite export failed because the destination already existed; preserve that error and both original artifacts.
- Hardware metrics and GPU context-switch collection are separate capabilities. Check supported options and site permissions; `--gres=nsight` addresses DCGM contention, not every profiler permission or undocumented driver operation.

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
