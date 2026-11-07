# Agent guide: VUDA / native OpenGL / LIBERO

## Mission and current boundary

Investigate whether native EGL/OpenGL rendering and CUDA model computation can execute simultaneously on one NVIDIA GPU, and whether VUDA's mechanism can enable or control that execution for LIBERO. The mentor explicitly requires **spatial sharing**, not merely GPU rendering, same-device placement, or time-sharing. A well-supported negative result is valid.

This repository is a research workspace, not a working VUDA implementation. At this handoff, it contains research, raw machine inventory, user-reported toolchain output, and a read-only inventory collector. No native rendering/CUDA probe, simulator integration, scheduler shim, model benchmark, or GPU concurrency result has been produced. The latest request authorized saving and organizing research and writing this guide; it did not authorize remote jobs or implement the previously proposed probe.

## Start here

1. Read [research/STATUS.md](research/STATUS.md) for current facts, gates, and the next actionable work. Use current user instructions to resolve scope.
2. Read [research/RESEARCH_LOG.md](research/RESEARCH_LOG.md) before new research or experiments. It tracks reviewed sources, decisions, unresolved questions, and experiment status. Extend an existing entry instead of repeating a completed search without a reason.
3. For technical claims about VUDA, LIBERO, scheduling, image transfer, or hardware portability, read the relevant section of [the historical technical plan](research/reference/VUDA_OPENGL_LIBERO_PLAN.md). Its S1–S23 citations are the existing source register. [Historical session notes](research/reference/VUDA_SESSION_NOTES.md) preserve mentor wording, interpretation corrections, and earlier decisions.
4. For OSC operations, read [docs/OSC_RUNBOOK.md](docs/OSC_RUNBOOK.md) and the evidence for the actual allocation. Do not infer the current allocation from a past node name.
5. Before editing code in a future implementation, inspect its actual build configuration and interfaces. There is currently no application build or test suite to reuse.

Historical plans contain restrictions such as “do not implement in this planning session.” Those describe the earlier request, not a permanent ban on an explicitly requested future implementation. Preserve the historical documents; record changed decisions in the current ledger.

## Workspace ownership and provenance

| Location | Purpose and update policy |
| --- | --- |
| `README.md` | Human entry point, navigation, and layout. |
| `research/STATUS.md` | Authoritative current state and next gates; update after new findings. |
| `research/RESEARCH_LOG.md` | Durable source-review, decision, question, and experiment tracking. |
| `research/archive/2026-11-06/` | Immutable original research/export bundle, screenshots, checksum manifest, and original ZIP. |
| `evidence/osc/ascend/8374257/` | Original remote inventory and separately labeled conversation transcription of later commands. |
| `evidence/PROVENANCE.md` | Evidence origins, privacy, missing inputs, and preservation policy. |
| `evidence/PUBLIC_SHA256SUMS` | Checksums of public research derivatives, sanitized inventory summary, and active collector. |
| `evidence/SHA256SUMS` | Local-only checksums of preserved private originals; excluded from Git. |
| `research/reference/` | Public derivatives of historical research, labeled separately from private originals. |
| `evidence/public/` | Sanitized user-evidence summary; not a raw capture. |
| `scripts/pitzer-diagnostics.sh` | Active collector. Its historical name does not restrict it to Pitzer. |
| `docs/OSC_RUNBOOK.md` | Supported operational workflow; keep commands aligned with actual site setup. |

The archived collector is an intentional snapshot; the active collector may evolve. Do not synchronize edits into the archive. The old `RESEARCH_ARTIFACTS.sha256` resolves relative to its archive directory. Old session-local URLs and absolute workstation paths inside historical prose are provenance, not current filesystem instructions.

## Evidence discipline

- Classify claims as observed raw output, user report, cited source finding, inference, or untested hypothesis. A command recommendation is not an executed command.
- Attribute OSC results to the user. The assistant has not connected to OSC or run its GPU workloads.
- Record capture time separately from conversation date. Job 8374257's inventory says 2026-11-07 00:41 UTC, corresponding to November 6 local time. The later toolchain transcript has no independent timestamp.
- Preserve raw files byte-for-byte. Put annotations and interpretation in sidecars or the ledger. Correct mistaken conclusions without rewriting evidence.
- Capture repository/fork/commit, launch command, configuration, software versions, GPU UUID/PCI identity, allocation grants, seeds, inputs, and output paths for every future experiment.
- Record failed experiments and exact errors. A missing tool, denied profiler capability, failed graphics context, and a scheduling barrier are different outcomes.
- Missing distribution metadata in system Python does not prove that no other environment or source checkout exists.
- Use primary sources. Record the paper version, section, review status, and limitations. A historical source review is not a fresh live verification.
- Keep raw reports private by default: usernames, project codes, hostnames, GPU UUIDs/serials, and process information require review before public publication.

## Technical invariants and known traps

### Scheduling and VUDA

TSG means Timeslice Group. VUDA's examined Vulkan/CUDA configurations use different TSGs that time-share the graphics/compute engine. Do not generalize this observation to every OpenGL driver and GPU.

V1 and v2 are different designs. V1 describes forwarding channels and kernel-module page-table changes. V2 describes a user-space allocation-interception shim, graphics TSG object duplication, CUDA channel/subcontext parent redirection, preserved CUDA data address spaces, shared graphics control mappings, and a shadow channel/daemon for partition controls. V2 says no driver patch is required; that does not make its internal operations supported public APIs.

V2 evaluates Vulkan/CUDA, not native OpenGL. The authors' code is not present. The user reports emailing the authors; no reply or source has been supplied. Do not label an unrelated repository named VUDA as the artifact. In particular, `jgbit/vuda` is a different project, and `timfox/idTech3` is not a faithful implementation of the paper mechanism.

The actual OpenGL allocation path, object bindings, control mappings, initialization/teardown, GPU architecture, firmware, and driver must be investigated before low-level adaptation. Never invent ioctl structures, GPU command words, or working page-table grafting. Without the artifact, this becomes independent reverse engineering rather than a routine API port.

### Workload and correctness

Keep LIBERO's native OpenGL renderer. A Vulkan renderer replacement is a separate experiment, not proof of this port. Classic MuJoCo physics runs on CPU in the inspected reference; the CUDA work in this proposal is model computation, not assumed PhysX physics.

Preserve observation/action ordering, camera identity, image orientation/layout/freshness, episode and environment associations, rewards, termination, and control frequency. A trajectory's inference cannot overlap production of the observation that inference consumes. Use independent environment groups. Preserve the learning algorithm's policy versions and rollout/update barriers; never silently introduce stale-policy training.

Keep three independent workstreams: scheduling, GPU-resident image handoff, and resource partitioning. CUDA/OpenGL registration or double buffering does not prove concurrency. Respect mapped-resource ownership and map/unmap ordering. Keep the initial host readback path when isolating scheduling effects.

Synthetic CUDA work is a scheduling probe, not real inference or training. Real policy/learner evaluation still needs a model, checkpoint, launch configuration, and algorithm schedule.

### Measurement

A successful EGL context, shared GPU identity, overlapping host calls, overlapping timestamp ranges, or improved throughput does not prove simultaneous active GPU execution. Correlate GL/CUDA activity, supported scheduling/context-switch evidence, and unprofiled results. Label ambiguous co-execution evidence inconclusive.

Use solo renderer, solo compute, explicitly serial, and independent-submission baselines. Start with bounded low-resource kernels so saturation does not hide scheduling opportunities. Check actual output before timing. Separate CPU physics, scene preparation, submission, synchronization, readback, preprocessing, and host-to-device copies.

For performance, follow the historical plan's warmup and repeated steady-state measurements. Report tails and interference alongside throughput. Faster environment transitions do not establish faster learning. Do not promise paper speedups or full GPU utilization.

## Current target and operational safety

Observed target: Ascend job 8374257, A100-PCIE-40GB, compute capability 8.0, MIG disabled, NVIDIA open driver/GSP 580.95.05. The user later loaded `cuda/12.8.1` and reported nvcc `V12.8.93`. NVIDIA EGL/OpenGL libraries and system C/C++/CMake paths exist, but no compilation or graphics execution has been demonstrated. Read evidence rather than assuming later allocations match.

Use only the allocated GPU and respect Slurm bindings. CUDA ordinal zero, `SLURM_JOB_GPUS`, EGL enumeration indices, and Linux NVIDIA device minors are not interchangeable. Match UUID or PCI identity; this allocation already shows differing ordinal/minor identifiers.

Run GPU workloads only in an allocated compute-node shell. Verify hostname and allocation identity. A CPU-only OnDemand/Code Server job is not a GPU allocation. Inspect versioned modules before loading; preserve the initial inventory environment. `nvidia-smi`'s CUDA header is driver compatibility information, not the chosen toolkit/runtime.

The account used in the notebook is a course account. Scheduler access does not prove authorization for this research; use an authorized project. The recorded username identifies the remote shell account, not necessarily the person operating the workstation.

Standard-API rendering/CUDA probes are the first step. Internal channel redirection or undocumented scheduling/partition commands need a separate technical review and a specific site-permission inquiry. No verified OSC blanket prohibition was established; do not invent one. An allocation alone does not settle site policy on those operations. Do not install drivers, reset GPUs, change clocks/MIG, stop DCGM, start/alter MPS daemons, patch kernel modules, or change services as part of diagnostics. Use OSC's documented profiler allocation procedure rather than disabling services.

## Handoff and update protocol

After any substantive research or run:

1. Save raw evidence with a stable identifier under `evidence/`; include provenance and privacy status.
2. Add or update a research-log entry with question, method, exact inputs, result, interpretation limits, and next decision. Mark planned/running/observed/inconclusive/blocked explicitly.
3. Update current status, including newly closed gates and remaining prerequisites. Keep the source register aligned with newly reviewed sources.
4. Add changed setup commands to the runbook only after checking them against evidence or authoritative documentation. Preserve known-working command output separately.
5. Verify changed scripts/code through relevant local checks and an actual target smoke run where available. State which target-dependent checks remain unexecuted.
6. Update preservation checksums for new or intentionally changed tracked evidence; retain historical manifests. End with exercised checks and concrete blockers, not claims of an unrun port.

For this documentation-only workspace, validate public checksums, active collector shell syntax, and public Markdown links. Where private originals are present, validate their separate archive/evidence checksums as well. Future native code must gain explicit build/run instructions and correctness checks before being called usable. Do not create placeholder implementations or fake scheduler fallbacks to fill missing research.

## Public/private publication boundary

The user approved sanitized public publication. Keep `research/archive/`, `evidence/osc/`, `evidence/reference/`, and `evidence/SHA256SUMS` local and Git-ignored. Do not force-add them. These paths hold original logs, screenshots, the ZIP, and coursework; they are intentionally absent from a fresh clone. The public entry point for machine evidence is [the sanitized Ascend summary](evidence/public/ascend-inventory.md). Public historical references are derivatives, not byte-identical originals.

Preserve private originals locally; use the public manifest for clone verification. Future evidence must undergo review before publication. Remove usernames, account codes, node names, device identifiers, private paths, and unrelated data from public derivatives. Public checksums validate derivative integrity, not independent execution or provenance authenticity.
