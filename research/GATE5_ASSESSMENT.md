# Gate 5: Native OpenGL/CUDA channel-redirection assessment

## Decision and scope

**Assessment completed; experimental go-ahead blocked.** Native OpenGL adaptation remains plausible, not demonstrated. Do not implement or run a channel-mutating shim on OSC from the available information. This is not a finding that GSP makes VUDA impossible, nor a claim that OSC prohibits all such research.

The research endpoint remains simultaneous active native EGL/OpenGL rendering and CUDA model computation for LIBERO. EPFL is the permanent host lab. OSC Ascend is a temporary preliminary testbed; any eventual OSC permission does not authorize experiments at EPFL.

This review covers VUDA v2, exact NVIDIA open-kernel driver release `580.95.05`, existing local EXP-002/EXP-003 artifacts, and official OSC guidance. It performs offline artifact analysis and source review only. It submits no GPU jobs, sends no support inquiry, and issues no driver controls. Review date: 2026-11-06 local; recorded experiment timestamps use UTC separately.

## 1. What the existing experiments establish

The raw EXP-002 log reports valid NVIDIA A100 OpenGL output, sampled CUDA numerical checks, and successful renderbuffer registration/mapping. Its single-run wall times are 20.04 ms solo rendering, 14.27 ms solo compute, 230.38 ms serial alternation, and 92.27 ms concurrent submission. These are synthetic workloads, not LIBERO, policy inference, or training. See [EXP-002](../evidence/public/exp002-probe-results.md).

EXP-003 contains 341 CUDA kernel records and 666 OpenGL workload records. Offline read-only SQLite inspection verified the four timestamp-containment examples in [EXP-003](../evidence/public/exp003-profile-results.md). They show CUDA intervals inside elapsed GL batch intervals. They do **not** identify TSG membership, prove graphics suspension, or distinguish active spatial overlap from waiting, preemption, contention, and profiling overhead.

NVIDIA documents that OpenGL GPU tracing uses `glQueryCounter()` around batches, rather than recording each operation's active execution. The graphics range label is not necessarily an isolated `glClear` operation. Device-level GPU metrics also lack process/context attribution. Correlate supported scheduling and activity evidence rather than interpreting timestamp containment as direct execution evidence. [S24]

### Offline audit of the preserved SQLite export

Input: local-only `evidence/osc/ascend/8375678/vuda_exp003_profile_8375678.sqlite`, opened with SQLite URI `mode=ro`.

| Check | Observed result |
| --- | --- |
| CUDA kernel rows | 341 |
| OpenGL workload rows | 666 |
| GL workload start labels | 333 `glBindFramebuffer`, 331 `glClear`, 2 `glReadPixels` |
| GL batches labeled `glClear`, wholly inside solo NVTX phase | 99; min 32.512 μs, mean 39.604 μs, max 568.226 μs |
| GL batches labeled `glClear`, wholly inside concurrent NVTX phase | 90; min 471.202 μs, mean 536.500 μs, max 668.739 μs |
| GPU context-switch event table | `GPU_CONTEXT_SWITCH_EVENTS` absent |
| Device metric table | `GPU_METRICS` absent |
| Context-switch enumeration | `ENUM_GPU_CTX_SWITCH` exists; labels are not captured switch events |

The phase-filtered counts need not equal application iteration counts: batching and phase-boundary selection differ. The earlier 18.4× figure compares selected ranges, not all-range phase means. This audit does not recompute an aggregate speedup from those selected ranges.

Reproducible phase query:

```sql
SELECT n.text, COUNT(*) AS batches,
       MIN(w.end-w.start)/1000.0 AS min_us,
       AVG(w.end-w.start)/1000.0 AS mean_us,
       MAX(w.end-w.start)/1000.0 AS max_us
FROM NVTX_EVENTS n
JOIN OPENGL_WORKLOAD w ON w.start >= n.start AND w.end <= n.end
JOIN StringIds s ON s.id = w.nameId
WHERE n.text IN ('Solo_EGL_Render', 'Concurrent_Multi_Threaded')
  AND s.value = 'glClear'
GROUP BY n.text;
```

The EXP-003 log ends with `Exportation error: File already exists` and exit code 1. Capture/report generation succeeded before this export step. The existing SQLite file has matching phase timings and event examples, but the failed export does not establish its byte-identical correspondence to that `.nsys-rep`. Preserve both files; establish that link later by exporting the report to a new filename, not overwriting the original.

### Probe limitations relevant to Gate 5

- `EGLRenderer::renderFrame()` clears, draws a triangle, and calls `glFinish()`. A GL batch labeled `glClear` may include later draw work. This is not a representative MuJoCo scene.
- `CudaCompute::launchCompute()` launches 1,024 blocks of 256 threads for 262,144 elements. This is not a demonstrated low-resource/headroom configuration. Grid size alone does not prove occupancy or saturation.
- `runConcurrentMultiThreaded()` independently submits work but synchronizes every render frame and CUDA iteration. It returns rendering-thread latency statistics and whole-pair wall throughput, not CUDA latency statistics.
- `testOpenGLInterop()` obtains a `cudaArray_t` but does not consume or verify mapped pixels in CUDA. Mapping success is not proof of copy-free implementation, tensor handoff correctness, or overlapping ownership.
- Existing results lack the historical plan's five steady-state repetitions, matched cross-process baseline, and real-model workload. No TSG/channel allocation capture exists.

**Gate 4 is therefore partial:** profiling collection exists, but the claimed root cause and active co-execution verdict remain inconclusive. Gate 3 passes the bounded native smoke/baseline milestone only. Gate 6 remains partial at registration/mapping, before verified image consumption.

### November 7 follow-up: collection settings and compatibility warnings

The [EXP-005 read-only metadata audit](../evidence/public/exp005-trace-metadata-audit.json) found `COLLECT_GPU_CTX_SW_TRACE=false`. Therefore, absent GPU context-switch events do not show a failed or denied capture attempt. GL workload collection was enabled. Nsight `2024.7.1.84` warned that CUDA driver compatibility version `13.0` was unsupported and that it would collect CUDA traces using libraries for `12.6`. This warning is separate from the recorded toolkit `12.8.1`; it neither proves all existing GPU events wrong nor establishes adequate support for a decisive new run. CPU sampling was throttled four times.

Export metadata records the expected basenames, UTC `2026-11-07T02:58:49`, and append/lazy settings. These reinforce consistency but do not authenticate the preserved report/database pairing. No local `nsys` executable was available for fresh export. The [corrected capture protocol](GATE4_CAPTURE_PROTOCOL.md) keeps compatibility, collection capability, and active-execution proof as separate gates. This follow-up did not connect to OSC or run a GPU workload.

### EXP-006: fresh export and live tool discovery

The [live follow-up](../evidence/public/exp006-live-prerequisites.md) exported the hash-matched preserved report to a new destination using the original exporter version. All 91 table schemas and all 90 non-export-metadata row multisets matched the original SQLite. This resolves the earlier content-correspondence uncertainty without overwriting originals or erasing the failed historical command. It adds no activity that was not captured.

Live discovery also found site-provided Nsight `2025.6.3.541` bundled with `cuda/13.2.1`; its absolute executable runs after restoring toolkit `12.8.1`. User-authorized allocation `8389776` completed a finite identity/tool-capability preflight on an A100 with driver `580.95.05`. No internal driver observation/mutation or application GPU workload occurred in that preflight. Help/status success is not proof of supported collection or active spatial execution.

### EXP-007: actual standard-API telemetry

The [repaired native smoke](../evidence/public/exp007-profile-results.md) completed successfully with the newer profiler and unchanged toolkit/probe source. Physical CUDA/EGL/NVML/profiler identities matched; GPU switch and metric records and fresh exports now exist. The initial inventory-cardinality guard failure remains preserved. These later runs are separate from the original offline assessment.

Restore/save brackets are not exact active-warp execution, and scheduling IDs are not mapped RM TSG handles. CPU scheduling was intentionally disabled and CUDA tracing used software instrumentation. Full all-mode correctness, matched steady-state/process controls, attribution, and resource-use checks remain incomplete. Gate 4 stays partial; the separate author-artifact/control-mapping and internal-operation permission prerequisites remain unchanged.

## 2. VUDA mechanism and adaptation boundary

V1 and v2 are different designs. V1 describes forwarding channels and kernel-module page-table work. V2 describes an `LD_PRELOAD` library intercepting allocation `ioctl()` calls, duplicating the existing graphics TSG into CUDA's driver client, and redirecting selected CUDA subcontext/channel parents while preserving CUDA's virtual-address-space handle. [S1, S2]

TSG unification alone is insufficient. V2 §4.2 requires correct translation for shared graphics control buffers through CUDA's address space. Its page-directory grafting discussion is a requirement, not a portable recipe. Section 6's no-driver-patch statement does not supply all mapping, privilege, initialization, teardown, and firmware details needed for another driver/frontend. [S1]

Controlled partitioning is a separate mechanism: a dedicated shadow channel submits undocumented Simultaneous Compute and Graphics (SCG) commands for mode selection and TPC quotas. A compute SM mask does not partition graphics. The paper's evaluated GA100 configuration defaults to inter-SM SCG after redirection; do not assume every GPU defaults to intra-SM sharing. [S1 §§5–7.1]

The paper evaluates Vulkan/CUDA, including A800 with driver `610.57.04`, not OpenGL/A100 with `580.95.05`. Ampere-family similarity is motivation for investigation, not compatibility proof. Keep the native GL TSG's initialized graphics state if a later verified adaptation supports it. Do not replace LIBERO's renderer with Vulkan to claim an OpenGL port. [S1]

The [version-specific driver assessment](GATE5_DRIVER_ASSESSMENT.md) maps candidate RM interfaces and validation boundaries. Open source definitions do not establish the proprietary OpenGL user-mode driver's allocation sequence or GSP firmware behavior at runtime.
### Author communication and technical clarifications (Bin Xu, November 2026)

Correspondence with Bin Xu (first author of VUDA, PhD student at IPADS Lab, Shanghai Jiao Tong University) established key architectural and operational constraints:

1. **Artifact release status:** Source code was withheld during paper double-blind review. The authors are preparing an anonymized public release soon.
2. **OpenGL feasibility:** While the paper evaluated Vulkan only, the author expects OpenGL adaptation to be relatively straightforward because OpenGL and Vulkan share much of the lower-level path in the NVIDIA user-mode driver (UMD). The author is actively testing the OpenGL path to include native OpenGL support in the release.
3. **Privilege requirement (Root/Admin):** The current channel-redirection implementation requires administrator access because it must issue specific `ioctl()` calls to the kernel-mode driver (KMD). Running without root would require modifying the KMD module itself to permit the relevant ioctls from unprivileged users.
4. **Hardware and architecture suitability:**
   - Basic GPU sharing is expected to work across a broad range of NVIDIA architectures.
   - Fine-grained resource partitioning (SCG mode/TPC quotas) may not function on Hopper/Blackwell (H/B-series) GPUs.
   - H100 is explicitly **not recommended** due to reduced graphics rendering hardware compared to client/workstation architectures.
   - Graphics-capable GPUs such as **RTX 4090** or **RTX 6000 Ada** (Ada Lovelace) are the author's recommended platforms for simultaneous rendering and compute workloads.
5. **Implications for OSC vs. EPFL:** Because multi-tenant shared clusters (OSC) prohibit root access and KMD modifications, Gate 5 runtime redirection cannot be deployed unprivileged on OSC. The permanent target lab at **EPFL** (or a dedicated bare-metal workstation with an RTX 4090/6000 Ada and root access) is required for Gate 5.


## 3. Policy: findings versus project precautions

Official OSC GPU guidance documents shared compute mode, multiple CUDA processes, and multiple GPU tasks in one job. It says `--gres=nsight` handles DCGM/profiler contention. It does not grant authorization for TSG object duplication, channel reparenting, shared control mappings, or undocumented SCG commands. [S19]

OSC's User Code of Ethics requires respecting the account's PI-authorized purpose, privacy, and system integrity. Those requirements matter for research allocation authorization and possible shared-node faults. The reviewed pages do not specifically approve or ban this proposed mechanism. The project runbook's ban on unvalidated device controls is a project precaution, not a quotation of OSC policy. [S25]

**Decision:** obtain a written, scope-specific OSC response before low-level runtime experimentation. Non-root execution, a successful allocation, default shared compute mode, and `--gres=nsight` do not settle this question. A full allocated GPU may still share its host and driver with other GPUs/jobs.

### Unsent support inquiry

Verified contact: `oschelp@osc.edu`. [S26] Supply the authorized project account privately, not in public research notes.

**Subject:** Review request: NVIDIA channel-redirection research on Ascend

> Could OSC review whether the following GPU scheduling experiments are permitted, and whether they require a dedicated test node?
>
> Our permanent host lab is EPFL; OSC would be a temporary preliminary testbed under an authorized research project. Prior Ascend evidence identifies an A100 with NVIDIA open driver/GSP `580.95.05`.
>
> We would first observe only our own EGL/OpenGL and CUDA processes' allocation calls with a non-mutating user-space `LD_PRELOAD` interceptor. A separate later experiment would use driver object duplication, redirect selected CUDA channel/subcontext parents into our OpenGL Timeslice Group, preserve CUDA data address spaces, and arrange shared graphics control mappings. Controlled partitioning would additionally require a shadow channel submitting undocumented SCG mode/TPC-quota commands, as described in VUDA v2 §§4–6: https://arxiv.org/html/2605.01352v2.
>
> Please distinguish permission for observation, channel/mapping mutation, and undocumented SCG commands; identify allocation/isolation requirements, supported monitoring, abort conditions, and recovery arrangements. We would not change drivers, firmware, services, MIG, clocks, MPS, or access another job's GPU; no such experiment has been run for this assessment.
>
> Thank you.

## 4. Next executable stages and stop criteria

| Stage | Required evidence before proceeding | Current disposition |
| --- | --- | --- |
| Offline reconstruction | Exact driver definitions, source/artifact provenance, control-mapping and teardown procedure | Source assessment recorded; author artifact/procedure missing |
| Non-mutating own-process allocation observation | Site review, approved project, actual allocation identity, reviewed observer that forwards original calls unchanged | Not implemented or run |
| Corrected standard-API baseline | Verified device identity, bounded low-resource and representative shader work, correctness, repetitions, scheduling/activity evidence where supported | Native smoke exists; decisive Gate 4 evidence incomplete |
| Reproduce author Vulkan/CUDA mechanism | Actual artifact or reviewed independent design, compatible stack, isolated approved host, known recovery path | Blocked |
| Native OpenGL redirection | Observed GL TSG/objects/control addresses; valid subcontexts/VAS; no address collisions; initialization and teardown proof | Blocked |
| Fixed SCG partition | Independently reviewed commands, architecture-specific behavior, preserved graphics capacity per active GPC | Blocked separately from redirection |
| LIBERO/model integration | Verified observation handoff, model/checkpoint, algorithm/policy barriers | Not part of this Gate 5 review |

Before any mutation, require a complete channel/control-mapping design; do not try parent-handle rewrites alone. Stop on an unexplained allocation class, unsupported status, conflicting address mapping, device mismatch, graphics-state loss, Xid/fault, or missing site approval. Site staff must define monitoring/recovery on the chosen testbed; do not improvise GPU resets.

No technical impossibility has been established. Conversely, no working OpenGL shim, compatible control-mapping procedure, or safe OSC mutation path has been established. The smallest credible next runtime contribution is an approved non-mutating allocation trace, alongside the corrected standard-API evidence—not a pretend VUDA port.

## Sources reviewed

- **S1:** [VUDA v2, §§4–6 and §7.1](https://arxiv.org/html/2605.01352v2), September 26, 2026 version; live mechanism/evaluation review. Publication/peer-review status not independently established.
- **S2:** [VUDA v1](https://arxiv.org/html/2605.01352v1); historical version distinction retained from the existing source register, not freshly re-reviewed here.
- **S19:** [OSC GPU Computing](https://www.osc.edu/resources/technical_support/supercomputers/gpu_computing), live review: compute modes, Nsight/DCGM, multiple tasks in one job.
- **S24:** [Nsight Systems 2024.7 User Guide](https://archive.docs.nvidia.com/nsight-systems/2024.7/UserGuide/index.html#opengl-trace), OpenGL Trace, GPU Context Switch, GPU Metrics; live review matched to captured profiler version. The guide describes version/platform permission limits; seek supported site collection rather than privilege escalation.
- **S25:** [OSC User Code of Ethics](https://www.osc.edu/supercomputing/support/codeofethics), Authorized Use items 1, 3, 4 and Appropriate Use; live review.
- **S26:** [OSC Technical Support](https://www.osc.edu/resources/technical_support), live verified contact and support procedure.
- **S27:** NVIDIA open-kernel source `580.95.05`; exact files and symbols in the linked driver assessment. Source review is not runtime validation.
