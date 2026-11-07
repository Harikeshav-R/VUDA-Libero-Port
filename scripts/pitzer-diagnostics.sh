#!/usr/bin/env bash
# Run inside an allocated OSC GPU compute-node shell (Ascend, Cardinal, or Pitzer).
# This collector reads system state. It does not install software, launch
# GPU workloads, change device settings, or dump the complete environment.
# Redirect or pipe its output to a file when invoking it.

if [[ -z "${SLURM_JOB_ID:-}" ]]; then
    printf 'No SLURM_JOB_ID is set. Obtain a GPU allocation before running this collector.\n' >&2
    exit 2
fi

run() {
    printf '\n$'
    printf ' %q' "$@"
    printf '\n'
    if ! command -v "$1" >/dev/null 2>&1; then
        printf '[unavailable] %s\n' "$1"
        return 0
    fi
    "$@"
    local status=$?
    if [[ "$status" -ne 0 ]]; then
        printf '[exit status %s; collecting remaining diagnostics]\n' "$status"
    fi
    return 0
}

printf '=== Allocation and operating system ===\n'
run date -u
run hostname -f
run uname -a
run cat /etc/os-release
run scontrol show job "$SLURM_JOB_ID"
run lscpu
run nproc
run free -h
run taskset -pc "$$"
run df -hT . /tmp

printf '\n=== Selected environment settings ===\n'
for key in SLURM_CLUSTER_NAME SLURM_JOB_ID SLURM_JOB_NODELIST SLURM_JOB_GPUS SLURM_STEP_GPUS \
    SLURM_CPUS_PER_TASK SLURM_MEM_PER_NODE SLURM_MEM_PER_CPU \
    CUDA_VISIBLE_DEVICES NVIDIA_VISIBLE_DEVICES NVIDIA_DRIVER_CAPABILITIES \
    MUJOCO_GL PYOPENGL_PLATFORM MUJOCO_EGL_DEVICE_ID \
    CUDA_HOME CUDA_PATH CUDA_MPS_PIPE_DIRECTORY CUDA_MPS_ACTIVE_THREAD_PERCENTAGE \
    DISPLAY WAYLAND_DISPLAY VIRTUAL_ENV CONDA_DEFAULT_ENV \
    LD_LIBRARY_PATH LD_PRELOAD APPTAINER_NAME SINGULARITY_NAME; do
    printf '%s=%s\n' "$key" "${!key-<unset>}"
done

printf '\n=== GPU hardware and driver ===\n'
run nvidia-smi
run nvidia-smi -L
run nvidia-smi --query-gpu=index,uuid,name,pci.bus_id,driver_version,memory.total,memory.used,compute_mode --format=csv
run nvidia-smi --query-gpu=index,compute_cap --format=csv
run nvidia-smi topo -m
run nvidia-smi -q
run cat /proc/driver/nvidia/version
run cat /proc/driver/nvidia/params

printf '\n=== Loaded and available software modules ===\n'
run module -t list
run module spider cuda
run module spider nvhpc

printf '\n=== Toolchain, profiler, and graphics-library discovery ===\n'
for executable in python python3 nvcc nsys ncu apptainer singularity eglinfo glxinfo vulkaninfo; do
    printf '%s: ' "$executable"
    if ! command -v "$executable"; then
        printf '[unavailable]\n'
    fi
done
run nvcc --version
run nsys --version
run nsys status --environment
run ncu --version
run apptainer --version
run singularity --version
run pkg-config --modversion egl gl
run ldconfig -p

printf '\n=== Active Python and installed package metadata ===\n'
python_executable=''
if command -v python >/dev/null 2>&1; then
    python_executable=python
elif command -v python3 >/dev/null 2>&1; then
    python_executable=python3
else
    printf '[unavailable] python/python3\n'
fi
if [[ -n "$python_executable" ]]; then
    run "$python_executable" - <<'PY'
import platform
import sys
from importlib import metadata

print("executable:", sys.executable)
print("python:", sys.version)
print("platform:", platform.platform())
for name in (
    "mujoco", "robosuite", "libero", "rlinf", "torch", "numpy",
    "PyOpenGL", "glfw", "transformers", "nvidia-cuda-runtime-cu11",
    "nvidia-cuda-runtime-cu12", "nvidia-cuda-runtime-cu13",
):
    try:
        version = metadata.version(name)
    except metadata.PackageNotFoundError:
        print(f"{name}: no installed distribution metadata")
    else:
        print(f"{name}: {version}")
PY
fi

printf '\n=== Interpretation limits ===\n'
printf '%s\n' \
    'This is an inventory, not a rendering, inference, or concurrency test.' \
    'nvidia-smi can expose physical GPUs beyond the allocation; honor Slurm device bindings.' \
    'The CUDA version in the nvidia-smi header is a driver compatibility level, not the installed toolkit version.' \
    'Missing distribution metadata does not rule out a source checkout.' \
    'No EGL context was created. Actual rendering vendor and allocated-device matching still require a controlled probe.' \
    'Review hostnames, project identifiers, paths, GPU UUIDs, and process details before sharing publicly.'
