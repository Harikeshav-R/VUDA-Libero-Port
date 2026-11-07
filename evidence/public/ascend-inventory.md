# Ascend inventory and toolchain: sanitized evidence summary

## Provenance and redaction

This is a derived summary of a user-supplied OSC Ascend inventory for job `8374257` and later user-pasted module/toolchain output. It is not the raw capture and was not produced by assistant access to OSC. The original files remain local under `evidence/osc/ascend/8374257/`, excluded from Git.

The raw inventory reports capture time 2026-11-07 00:41 UTC (November 6 local time). The later toolchain commands have no independent timestamp or repeated job identity; their association with this allocation comes from the conversation.

Remote usernames, numeric user/group IDs, project codes, node/login hostnames, home and working paths, GPU UUID/serial/PDI, PCI bus address, network endpoints, process IDs, and unrelated system details are withheld. The course notebook, mentor screenshots, original ZIP, and raw evidence are not public artifacts. This summary retains only facts needed to choose and reproduce the next research step.

## Allocation and system observations

| Item | User-supplied evidence |
| --- | --- |
| Cluster | OSC Ascend |
| Job | 8374257; interactive, running at capture |
| Request | One node, one task, four CPU cores, 16 GB host memory, one GPU, 20 minutes |
| Actual allocation | Five CPUs, 16 GB host memory, one A100 GPU |
| CPU affinity | Five CPUs within one NUMA node; CPU indices withheld |
| Host processor | AMD EPYC 7H12; two sockets, 64 cores per socket in host inventory, not job allocation |
| Operating system | RHEL 9.6 (Plow), x86_64 |
| Kernel | 5.14.0-570.154.1.el9_6.x86_64 |
| GPU | NVIDIA A100-PCIE-40GB, Ampere |
| GPU memory | 40,960 MiB nominal device memory; 0 MiB used in the captured listing |
| Compute capability | 8.0 |
| MIG | Disabled |
| Compute mode | Default |
| Driver | 580.95.05; NVIDIA open kernel module |
| GSP firmware | 580.95.05 |
| Driver CUDA header | 13.0; compatibility indicator, not selected toolkit/runtime |
| Profiling module parameter | RmProfilingAdminOnly: 0; actual profiler/counter access untested |
| Device numbering | CUDA-visible ordinal 0 and NVIDIA device minor differed; do not treat ordinal, minor, and EGL index as equivalent |

The captured GPU listing reported no running processes. This is a snapshot, not proof of whole-node exclusivity. Respect allocation device bindings on every run.

The allocation used a course account. Permission to charge unrelated research to that account has not been established. Use an approved project account.

## Software observations

- NVIDIA `libEGL_nvidia`, `libnvidia-eglcore`, `libGLX_nvidia`, and `libnvidia-glcore` appeared in the linker cache. Generic EGL/OpenGL dispatch libraries and Mesa libraries were also present. Presence does not prove successful NVIDIA context creation or correct rendering.
- `pkg-config --modversion egl gl` returned `1.5` and `1.2`; these are package metadata, not a queried current-context OpenGL version.
- `pkg-config --cflags --libs egl gl` later returned `-lEGL -lGL`.
- `cc`, `gcc`, `g++`, and `cmake` resolved under `/usr/bin`. Their executable versions were not collected.
- Initially, `nvcc`, `nsys`, `ncu`, `eglinfo`, and `vulkaninfo` were absent from the active PATH. `glxinfo` was present; it was not run as a headless EGL test.
- Apptainer reported version `1.5.4`.
- Active Python was `/usr/bin/python`, version `3.9.21`, with NumPy `1.26.4`.
- That Python environment reported no installed distribution metadata for MuJoCo, robosuite, LIBERO, RLinf, PyTorch, PyOpenGL, GLFW, Transformers, and the queried NVIDIA CUDA runtime distributions. This does not establish absence from other environments or source checkouts.
- `MUJOCO_GL`, `PYOPENGL_PLATFORM`, `MUJOCO_EGL_DEVICE_ID`, `DISPLAY`, and virtual-environment indicators were unset at inventory capture.

## Follow-up module and compiler evidence

The user reported that `module spider cuda/12.8.1` and `module spider nvhpc/25.1` each said the module could be loaded directly. NVHPC was not reported loaded, and profiler executable availability remains untested.

The user then reported:

```text
$ module load cuda/12.8.1
$ nvcc --version
nvcc: NVIDIA (R) Cuda compiler driver
Copyright (c) 2005-2025 NVIDIA Corporation
Built on Fri_Feb_21_20:23:50_PST_2025
Cuda compilation tools, release 12.8, V12.8.93
Build cuda_12.8.r12.8/compiler.35583870_0
```

No driver installation was needed or performed. The module's generic installation warning was not evidence of a missing driver.

## Conclusions and limits

The inventory and follow-up establish a concrete A100/driver target, installed graphics libraries, system build tools, and a working nvcc version query after module loading. They do not demonstrate source compilation, EGL context creation, rendered pixel correctness, CUDA kernel execution, same-physical-device graphics placement, profiler capture, or simultaneous active graphics/CUDA execution.

The next proposed step is a bounded standard-API native EGL/OpenGL + CUDA correctness and scheduling probe. Match physical devices using locally captured identity; do not infer EGL placement from a CUDA visibility mask. No undocumented scheduling controls are included in that proposed first step.
