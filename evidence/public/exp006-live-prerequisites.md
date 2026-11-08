# EXP-006: Live OSC prerequisites and verified report re-export

## Scope and provenance

**Observed on November 7, 2026, UTC:** the assistant queried OSC Ascend login-shell scheduler/module metadata and re-exported a 539,434-byte historical report. After the user confirmed a PI-authorized project, a GPU allocation ran the finite preflight below. The export was lightweight CPU-only conversion; the allocated preflight queried identity and tool capabilities, not an application workload or new trace. EPFL remains the permanent host lab; OSC is a temporary testbed.

Private captures retain exact SSH/SCP commands, UTC start/end times, stdout/stderr, exit statuses, module paths, scheduler associations, allocation grants, and digests under `evidence/osc/ascend/exp006-live-prerequisites/`. Public identifiers and private paths are omitted here. No driver mutation, service change, tool installation, interception, or support email occurred.

## 1. Live prerequisite findings

At the initial scheduler query time, no jobs were returned. Scheduler associations alone did not establish research authorization. The user subsequently selected a PI-authorized project in this conversation; its code remains private.

| Site module inspected | Observed bundled Nsight Systems version |
| --- | --- |
| `nvhpc/24.11` | `2024.6.1.90-246134905481v0` |
| `nvhpc/25.1` | `2024.7.1.84-247135125610v0` |
| `cuda/13.2.1` | `2025.6.3.541-256337736014v0` |

Named module searches for `nsight`, `nsys`, and `nsight-systems` failed. That did **not** establish absence of a newer profiler: inspecting the advertised CUDA bundle located one. The captured module catalog is scoped to that login environment, not an exhaustive filesystem inventory.

### Keep the build toolkit separate

In a disposable SSH shell, the assistant loaded `cuda/13.2.1`, saved the absolute `nsys` path, then loaded `cuda/12.8.1`. The selected absolute profiler still reported `2025.6.3.541`, while `nvcc --version` reported CUDA `12.8`, `V12.8.93`. No persistent environment helper was changed.

This demonstrates tool selection and CLI startup, not successful CUDA/GL tracing against an allocated GPU. Loading the newer module for discovery does not require building or running the probe with CUDA 13.2.

The newer profiler's actual `profile --help` recognizes `--gpuctxsw`, `--opengl-gpu-workload`, and GPU metric selection. Its GPU context-switch option states driver R435.17+ and system scope, without the root wording in the older 2024.7 help. This does not itself establish collection permission, redaction behavior, or availability. Do not transfer old-version permission claims to the new version without review. No hardware metric/device query or context-switch collection was attempted on a login node.

NVIDIA's [2025.5.1 announcement](https://forums.developer.nvidia.com/t/nsight-systems-2025-5-1-desktop-server-x86-64-arm-sbsa-release-announcement/342135) lists “CUDA 13.0 support”; its [2025.6.1 announcement](https://forums.developer.nvidia.com/t/nsight-systems-2025-6-1-desktop-server-x86-64-arm-sbsa-release-announcement/352230) lists a CUDA version-compatibility update and changed tracing-backend defaults. These justify testing the installed newer candidate. They do not certify every 2025.6.3/driver/architecture combination or establish site collection permission.

## 2. Fresh export and full event-content comparison

The source report was exported with the matching historical exporter `2024.7.1.84` to a previously unused output file. The command explicitly set `--force-overwrite=false`, refused an existing destination, and exited `0`. Source report digests before and after export matched the local preserved report.

| Artifact | SHA256 |
| --- | --- |
| Preserved `.nsys-rep`, local and remote input | `2aa596a196a85a1a5c16a1ec4691031c41557b84af5286789051532e0a5915a5` |
| Original preserved SQLite | `b2c0256233ff12ba08f417fbe8d75825246afe48685b3cc026269cd107f5997b` |
| Fresh export, remote and retrieved local copy | `b2805f46393d469a2404cf32575f27da38843c59fe9792b8ef760dde3a7ab50b` |

Both SQLite files were opened using `mode=ro`. Comparison checked exact table schema strings and unordered row multisets, including duplicate multiplicities and every column value—not just row counts.

| Check | Observed result |
| --- | --- |
| Table-name sets | Same 91 tables |
| Table schema strings | All 91 equal |
| Every table except `META_DATA_EXPORT` | All 90 row multisets equal |
| CUDA kernel rows | 341 in each database |
| GL workload rows | 666 in each database |
| NVTX rows | 6 in each database |
| `PRAGMA integrity_check` | `ok` for both databases |
| Local original input digests | Unchanged |

`META_DATA_EXPORT` alone differs in eight fields: export host, append setting, output filename/two output paths, export duration, and UTC/local export time. Both versions of this table have 29 rows. Private comparison captures preserve the exact differences.

**Conclusion:** the newly observed export from the hash-matched report reproduces all non-export-metadata contents of the preserved SQLite file. This resolves the earlier content-correspondence uncertainty for those artifacts. It does not authenticate every historical actor/command or recover the original failed export's execution history. The failed `File already exists` command remains recorded, not rewritten as a success.

A fresh export adds no missing activity. GPU context-switch collection was disabled in EXP-003; re-exporting cannot supply those events. Gate 4 remains inconclusive on active spatial sharing and TSG root cause.

## 3. Authorized GPU preflight

The first immediate allocation request, job `8389737`, hit its 60-second availability limit: `Unable to allocate resources: Connection timed out`. A subsequent scheduler record confirmed `CANCELLED`, with no allocation granted.

The finite batch preflight, job `8389776`, requested one GPU, four CPU cores, 16 GiB, five minutes, and `--gres=nsight` under the user-confirmed research project. It completed in seven seconds with exit `0`. All recorded preflight commands returned `0`.

| Allocated preflight observation | Result |
| --- | --- |
| Accessible GPU | One NVIDIA A100-PCIE-40GB |
| Driver | `580.95.05` |
| CPU/memory grant | Five CPUs, 16 GiB |
| Selected profiler/toolkit | Nsight `2025.6.3.541`; CUDA `12.8`, `V12.8.93` |
| Root privilege | Disabled |
| Profiler environment check | Process-tree and system-wide CPU profiling checks `OK` |
| Metric-device discovery | A100 identified; `cuda-visible` selection recognized |
| Metric-set discovery | `ga100` alias listed |

Raw evidence retains the allocation/node/GPU UUID/PCI identity. The script checked the compute hostname against the allocated node list and refused missing allocation/visibility. Metric help is discovery, not actual counter collection or CUDA tracing. Although requested GRES included `nsight=1`, the allocation record reports `gres/nsight=0`; neither value alone proves DCGM state or counter availability. No service was changed.

## 4. Remaining runtime prerequisites

- Use the user-confirmed research project for future allocations. Recheck each actual grant rather than reusing the completed preflight's identity.
- Validate the newer profiler's documented tracing support and the site's supported context-switch/metric path, privacy scope, and allocation requirements. CLI recognition is not runtime proof.
- Re-inventory each new allocation; this preflight does not fix future GPU/driver identity.
- Apply the [Gate 4 protocol](../../research/GATE4_CAPTURE_PROTOCOL.md), including output verification, reduced/original compute controls, repeated unprofiled runs, and separate profiled evidence. The existing probe still lacks parts of the corrected measurement harness.
- Keep [Gate 5 observer/mutation prerequisites](../../research/GATE5_OBSERVER_REVIEW.md) separate. No permission or implementation was supplied by this check.

## Sources and verification boundary

The principal evidence is the captured live CLI output, fresh export, and read-only full-content comparison, preserved privately with a new experiment manifest. Profiler option/version discovery was exercised; new GPU tracing was not. Historical [EXP-003](exp003-profile-results.md) and [EXP-005 metadata audit](exp005-trace-metadata-audit.json) remain unchanged raw-input references.
