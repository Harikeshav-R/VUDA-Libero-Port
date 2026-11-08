# Gate 4: Corrected capture protocol and proof limits

## Decision and scope

**EXP-007 exercised the newer profiler's standard collection path; EXP-008 exercised bounded output rejection and independent concurrent latency.** Full Gate 4 validation remains incomplete. This protocol originated as an unexecuted EXP-005 plan; EXP-006/EXP-007 separately record live discovery, corrected identity checks, and finite native profiling. EXP-008 then changed the probe and ran its rejection/correctness controls. The next controlled experiment is specified in the [EXP-009 implementation and execution plan](EXP009_CONTROLLED_SCHEDULING_PLAN.md), currently planned rather than implemented or run.

EPFL is the permanent host lab. OSC Ascend is a temporary testbed. Historical EXP-003 used A100-PCIE-40GB, driver/GSP `580.95.05`, toolkit `12.8.1`, Nsight `2024.7.1.84`; EXP-007 successfully captured the same recorded GPU/driver family with independently selected Nsight `2025.6.3.541`. A future allocation must be inventoried again; these versions are not a required deployment configuration.

### Next experiment and independent prerequisites

Use the EXP-009 plan as the single detailed contract for the complete thread/process matrix, warmup/windows, deadline/drain accounting, validation strength, process failure handling, and target acceptance. Its proposed options and durations are not current executable interfaces or measured facts. Completing that experiment is distinct from proving active spatial sharing.

Author source/reproduction access, scope-specific site permission for internal observation/mutation, the mentor's actual model/checkpoint/schedule, and EPFL inventory can proceed as separate workstreams. Standard-API controls do not require waiting for a VUDA artifact, but they also do not authorize internal driver work. Representative native MuJoCo rendering and real model evaluation follow the controlled synthetic baseline rather than replacing its missing controls.

## 1. New observations from the preserved capture

The [sanitized EXP-005 audit](../evidence/public/exp005-trace-metadata-audit.json) contains exact SQL, results, input SHA256, and observation time. The original EXP-003 database was opened read-only.

- `COLLECT_GPU_CTX_SW_TRACE=false`: collection was disabled. Missing switch events are not evidence of unsupported hardware, denied permissions, or an attempted collection failure.
- `GRAPHICS_TRACE_OPTIONS:COLLECT_OPENGL_WORKLOAD=true`: GL workload collection was enabled.
- Nsight warned: `Installed CUDA driver version (13.0) is not supported by this build of Nsight Systems. CUDA trace will be collected using libraries for driver version 12.6`.
- A second warning states: `The operating system throttled the collection of sampling data 4 times.`
- Export metadata names the expected input/output files, records UTC `2026-11-07T02:58:49`, and sets append/lazy options. These support consistency, not authenticated report/database pairing.

The warning's driver compatibility version is not the selected toolkit version. Existing events are not automatically invalid, but their presence alone does not resolve profiler support. CPU-sampling throttling does not establish dropped GPU events or prove that profiling left workload behavior unaffected.

## 2. What the archived profiler guide establishes

Use the [versioned 2024.7 User Guide](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html), not current documentation as an unstated substitute.

| Measurement | Source finding | Evidence limit |
| --- | --- | --- |
| [OpenGL workload trace](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html#opengl-trace) | `glQueryCounter()` measures completion time for batches containing many GL operations. KHR_debug ranges cannot fully account for parallel draws. | Elapsed batch ranges do not identify active graphics SM execution. A label need not isolate one GL operation. |
| [GPU context switches](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html#gpu-context-switch) | The guide states driver R435.17+ and root permission, then also describes normal-user records with valid own-user IDs and reduced information for other users. | This documentation tension needs site clarification. Recognized flags do not establish available collection. Records do not identify RM TSG allocation handles. |
| [GPU metrics](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html#gpu-metrics) | Linux x86-64 is supported; Turing or newer is required; GA100 minimum driver is R450. Elevated counter access is required. Metrics do not know which process or context is involved. | Architecture minima do not settle the observed newer-driver warning. Aggregate activity cannot attribute simultaneous GL/CUDA execution. |
| [Metric selection](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html#cli-profile-command-switch-options) | Device IDs and metric-set aliases are queried through `--gpu-metrics-devices=help` and `--gpu-metrics-set=help`. | Never assume CUDA ordinal zero is the profiler device ID, or hardcode an unqueried metric-set alias. |
| [Export](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html#cli-export-command-switch-options) | `nsys export` processes an existing report; output and overwrite controls are separate. | A fresh export cannot add events that were never captured. Counts alone do not establish full event-content equality. |

[OSC GPU guidance](https://www.osc.edu/resources/technical_support/supercomputers/gpu_computing) documents `--gres=nsight` for its DCGM/profiler conflict. This does not grant root, authorize internal driver operations, or establish actual counter access. Do not stop DCGM, alter services/sysctls, or use privilege escalation. System-wide records also require privacy review before publication.

## 3. Runtime prerequisites and stages

1. **Authorization and identity:** Confirm PI-authorized project purpose, actual allocation/node identity, CPU grants, and the allocated GPU's UUID/PCI identity. Match EGL, CUDA, and profiler device selection physically. Do not infer an allocation from a historical job.
2. **Tool support:** Record actual driver, toolkit, profiler, graphics libraries, launch command, configuration, source revision/hash, and output paths. Review site-provided profiler support for the installed driver. Retain the version-warning evidence; do not install another tool automatically.
3. **Supported collection path:** Record exact-version help/status and actual capture diagnostics. EXP-007 demonstrated standard GPU switch/metric collection through the site's existing authorized user-job configuration without privilege escalation or manual service changes; this resolves basic availability for that capture, not every node or observation scope. Ask staff about actual permission failures, broader/system-wide observation, or isolation/privacy uncertainty. Older documentation alone does not justify escalation, and successful standard tracing does not authorize allocation interception or mutation.
4. **Correctness and ready work:** verify GL pixels and CUDA numerical output under each configuration, including concurrent runs. EXP-009 separates per-iteration identity/output validation from timed runs with lower-overhead final tags; a correct final tag alone cannot establish every earlier output's freshness. Record validation strength and instrumentation differences. Keep disjoint inputs and host readback while isolating scheduling. Failed checks invalidate timing conclusions.
5. **Unprofiled controls:** Run matched solo GL, solo CUDA, explicit serial, and independent submissions, with both reduced and original compute loads. Compare in-process threads and matched cross-process runs when implemented. Warm up each configuration; collect five equal-duration steady-state repetitions. Record medians/ranges, throughput for both workloads, and p50/p95/p99 latency for each.
6. **Separate profiled controls:** Profile matched finite runs to unused output names. Collect only capabilities actually available and approved. Correlate phases, CUDA activity, GL batches, scheduling evidence, and metrics without converting elapsed-range overlap into an active-execution claim.

Use finite representative shader work as well as the triangle control before generalizing to LIBERO. Synthetic arithmetic is not policy inference or training. An unavailable telemetry path leaves Gate 4 inconclusive; it does not license a low-level scheduling shim.

## 4. Probe capabilities and remaining controls

Source interfaces: [`main.cpp`](../src/probe/main.cpp), [`benchmark_runner.cpp`](../src/probe/benchmark_runner.cpp), and [`cuda_compute.cu`](../src/probe/cuda_compute.cu). EXP-008 updates the probe; EXP-005/EXP-007 descriptions refer to their preserved earlier source.

- Existing CLI flags include `--iterations`, `--warmup`, `--intensity`, and `--elements`. No `--mode` or equal-duration CLI exists.
- CUDA launches use 256 threads/block and a ceiling-divided element count. `--elements 1024` produces four blocks, versus 1,024 blocks for 262,144 elements. Neither grid size proves saturation, actual occupancy, or free graphics capacity.
- `verifyOutput()` retains seven sampled indices, including `1000`; element counts must exceed `1000`. This is not full-buffer verification.
- Workers remain independent after one start flag. Rendering uses `glFinish()` and CUDA uses per-stream completion waits. These waits limit queue depth, not thread independence. EXP-008 adds checked statuses, abort/error propagation, and explicit OpenGL client-API binding in the render worker; it does not remove completion waits.
- EXP-008 adds final checks for both outputs in serial/concurrent modes and makes failed checks reject the run rather than print a usable report. Checks happen outside measured intervals, after successful main-thread EGL rebind for concurrent readback.
- The report exposes independent concurrent EGL and CUDA host submit-to-completion latency distributions, including p50/p95/p99. Both concurrent throughput rows use the same full phase wall envelope. Serial latency remains a render/compute pair latency, not separate per-workload serial timing.
- Solo/serial paths retain their configured warmup; the concurrent path still lacks independent worker warmup. Five fixed-count repetitions are not five equal-duration steady-state repetitions.
- Constant final pixels and CUDA samples do not prove every iteration's freshness. Runtime failure checks add host instrumentation overhead, so changes from EXP-007 are not an isolated scheduling intervention.
- Equal-duration orchestration, matched cross-process comparison, measured headroom, representative shader work, and authoritative scheduling attribution remain incomplete. CUDA consumption of mapped GL pixels remains a separate Gate 6 task.

The application no longer converts a throughput ratio into a spatial-sharing or TSG verdict. See the [EXP-008 evidence](../evidence/public/exp008-correctness-latency.md) for the executed checks and remaining limits; none of these source changes closes Gate 4 by itself.

## 5. Failure classification

| Observed outcome | Permitted conclusion and next action |
| --- | --- |
| Flag unknown or missing executable | Tool/version prerequisite missing; not GPU scheduling failure. |
| Explicit permission rejection | Capability unavailable under that collection path. Ask staff; do not escalate. |
| Requested collection produces no events | Retain requested flags, metadata, diagnostics, and collection window. Missing tables alone do not identify permission, unsupported hardware, or lack of activity. |
| Counter resource unavailable | Possible competing collector/resource conflict. Use OSC's documented allocation procedure and site diagnosis; do not assume DCGM is the only cause or alter services. |
| Unsupported-driver fallback warning | Compatibility unresolved; do not rely on that run as decisive scheduling proof. |
| EGL initialization or output failure | Separate graphics/identity/injection/correctness investigation. A context error is not a TSG finding. |
| Existing export destination | Preserve both original artifacts. Export to a new path, never overwrite historical files. |

## 6. Decision matrix

| Evidence | Verdict |
| --- | --- |
| Overlapping host API calls or improved throughput | Submission/performance observation only; active spatial sharing unproved. |
| CUDA activity inside elapsed GL batch ranges, even with aggregate SM activity | Inconclusive on active simultaneous GL/CUDA execution. |
| Overlapping scheduling/residency records with improved throughput | Still inconclusive without attributable active graphics execution; validate event semantics before interpreting residency. |
| Complete, correctly attributed same-engine scheduling records show mutually exclusive residency while both workloads have ready work | Supports time-sharing in that captured configuration. Does not identify RM TSG handles, prove a universal scheduling restriction, or uniquely exclude application/resource causes. |
| Supported, correctly attributed graphics and CUDA active-execution evidence establishes simultaneous progress on disjoint execution resources | Positive only within the captured workload/device/driver scope, corroborated by correctness and unprofiled controls. No such evidence exists yet. |
| Capabilities, compatibility, attribution, or resource availability remain unresolved | Gate 4 stays partial/inconclusive. Do not infer that redirection is necessary. |

No fixed throughput ratio, slowdown threshold, or reduced grid proves spatial sharing or TSG isolation. Serial alternation wall time is not necessarily the sum of solo times, as EXP-002 already illustrates.

## 7. Fresh export without overwriting originals

Local `nsys` was unavailable during EXP-005. EXP-006 subsequently performed a fresh remote export with the historical exporter and verified all 91 schemas and 90 non-export-metadata row multisets against the preserved SQLite. See [executed evidence](../evidence/public/exp006-live-prerequisites.md#2-fresh-export-and-full-event-content-comparison). The following remains a generic syntax example, not the exact executed path/filename:

```bash
nsys export --type=sqlite \
  --output=evidence/osc/ascend/8375678/vuda_exp003_profile_8375678_reexport.sqlite \
  evidence/osc/ascend/8375678/vuda_exp003_profile_8375678.nsys-rep
```

Preserve its command, tool version, diagnostics, input digest, and output digest in a new experiment manifest. Do not append to historical manifests. Compare schema, `CUPTI_ACTIVITY_KIND_KERNEL`, `OPENGL_WORKLOAD`, `NVTX_EVENTS`, and relevant event content/phase times with read-only queries. Counts and metadata alone are insufficient. SQLite byte equality is not required because export metadata and database layout can differ. A fresh export establishes a newly observed derivation, not historical provenance authenticity.

## 8. EXP-007 follow-up: collected telemetry is not an active-execution verdict

[Actual capture results](../evidence/public/exp007-profile-results.md) and [sanitized audit](../evidence/public/exp007-profile-audit.json) now establish native compilation/execution, physical CUDA/EGL/NVML/profiler correlation, successful new exports, and populated GPU switch/metric records with Nsight `2025.6.3.541`. The initial wider-inventory guard failure remains preserved. Successful native execution used a single-row NVML inventory; seven CPU cases cover the wider-inventory selector boundary.

The export names switch tag `8` as `RESTORE_START` and `7` as `SAVE_END`. Independently paired brackets are disjoint in the recorded streams, but those endpoints include restore/save and do not mark exact active graphics/CUDA warp boundaries. Scheduling IDs lack direct RM TSG mapping. Do not convert inter-bracket gaps into causal hardware-switch overhead, or infer absence of all foreign activity from own-process recorded rows. Worker NVTX ranges also include setup/readiness waits.

The old unsupported-driver warning is absent. The explicit CUDA software-tracing fallback remains; intentional `--cpuctxsw=none` means CPU thread-state reconstruction is inaccurate. Device metric rows do not provide per-context simultaneous active-warp attribution, and their numeric scaling was not decoded. Gate 4 remains partial pending the correctness, matched-control, attribution, and resource-availability requirements above.

EXP-008 subsequently exercised final checks and failure rejection in all modes and exposed separate concurrent CUDA latency, with ten normal runs and eight clean failure exits. See its [runtime evidence](../evidence/public/exp008-correctness-latency.md). Static output samples do not close freshness, matched-control, attribution, or resource-availability requirements.

## Evidence preservation

Local-only raw HTTP source captures and URL/SHA256 provenance are retained under `evidence/osc/ascend/exp005-offline/profiler/`. They include the archived User Guide and separately labeled current Analysis Guide. Authored `notes_*.txt` and the rejected first protocol draft are analytical artifacts, not raw retrieved source. The public metadata audit is a sanitized derivative of the unchanged private SQLite input.
