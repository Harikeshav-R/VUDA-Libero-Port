# Gate 5: NVIDIA 580.95.05 driver-source assessment

## Verdict and evidence boundary

**Candidate object mechanisms exist; a compatible OpenGL port is not established.** The exact source release contains channel-group copying, context-share/VAS bindings, and GSP RPC paths for duplication. That evidence rules out the simplistic argument that GSP offers no duplication path. It does not prove that the recorded OpenGL/CUDA stack accepts VUDA's allocation sequence or supplies correct control mappings.

Review date: November 6, 2026 local. Target evidence: OSC Ascend A100-PCIE-40GB, open kernel module/GSP `580.95.05`, CUDA toolkit `12.8.1`. OSC is a temporary preliminary testbed; EPFL is the permanent host lab. Neither environment currently has approval recorded for the proposed undocumented operations.

This assessment reads published source; it does not load a driver, intercept calls, issue an ioctl, or inspect proprietary user-mode binaries. The [source tree response](https://api.github.com/repos/NVIDIA/open-gpu-kernel-modules/git/trees/580.95.05?recursive=1) resolves release tag `580.95.05` to `2b436058a616676ec888ef3814d1db6b2220f2eb`. References below pin that release rather than `main`.

NVIDIA's [release README](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/README.md) requires corresponding-version GSP firmware and user-space components. Its [open-module guide](https://us.download.nvidia.com/XFree86/Linux-x86_64/580.95.05/README/kernel_open.html) says the open modules depend on GSP and that user-space components are identical between kernel-module flavors. Open kernel modules do not imply an open OpenGL user-mode implementation.

## 1. VUDA version-specific module changes

| Component | V1 | V2 | OpenGL adaptation requirement |
| --- | --- | --- | --- |
| Submission integration | Forwarding-channel design | User-space `LD_PRELOAD`/allocation-ioctl shim | Observe actual native GL allocation lifecycle; do not infer it from Vulkan API explicitness |
| Scheduling | Channel redirection described with earlier implementation | Duplicate graphics TSG into CUDA client; redirect selected CUDA channel/subcontext parents | Identify initialized GL TSG, engine, selected CUDA channels, and initialization ordering |
| Data address translation | Earlier page-table work | Preserve CUDA VAS; distinct subcontext/PDB selection | Preserve actual CUDA data mappings and consistent TSG subcontext state |
| Shared control state | Kernel-module page-table changes | §4.2 describes graphics PDE grafting; §6 claims no driver patch | Obtain actual mapping procedure, address ranges, permissions, coherency, and teardown details |
| Resource control | Do not treat earlier design as v2 | Dedicated shadow queue/channel issues undocumented SCG mode/TPC-quota commands | Independently establish hardware/driver commands and graphics-locality constraints |

Source: [VUDA v2 §§4–6](https://arxiv.org/html/2605.01352v2); v1 distinction retained from the [historical review](reference/VUDA_OPENGL_LIBERO_PLAN.md#what-vuda-establishes). The v1 implementation was not freshly re-reviewed here.

V2 evaluates Linux native Vulkan/CUDA on RTX 4090 (`610.43.02`), A800-SXM4 (`610.57.04`), and Orin (`540.4.0`), not OpenGL/A100 `580.95.05`. Section 7.1 states redirect-only uses default SCG mode, inter-SM on GA100. That is a paper finding, not a runtime observation of this A100. It also does not establish which firmware mode the authors used.

## 2. Exact-release candidate interfaces

These are source-defined candidates, not an observed allocation transcript or instructions for device mutation. Escape numbers are not complete Linux ioctl request encodings.

| Mechanism | Verified source definition | What it establishes / what it does not |
| --- | --- | --- |
| RM allocation and duplication escapes | [`nv_escape.h`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/src/nvidia/arch/nvalloc/unix/include/nv_escape.h): `NV_ESC_RM_ALLOC = 0x2B`, `NV_ESC_RM_DUP_OBJECT = 0x34`, `NV_ESC_RM_CONTROL = 0x2A`, `NV_ESC_RM_FREE = 0x29` | These internal entry points exist. `0x41` is `NV_ESC_RM_IDLE_CHANNELS`, not RM allocation. Do not confuse escape numbers with `NV04_*` function IDs. |
| Allocation wrapper | [`nvos.h`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/src/common/sdk/nvidia/inc/nvos.h): `NVOS21_PARAMETERS`, `NVOS64_PARAMETERS` | Wrappers carry parent/new-object/class/parameter fields; NVOS64 additionally carries requested rights and serialization flags. The actual runtime wrapper must be observed. |
| Object duplication | Same `nvos.h`: `NVOS55_PARAMETERS` | Source/destination clients and objects, destination parent, flags, and returned RM status are explicit. Merely knowing handles does not grant access. |
| Channel-group class | [`cla06c.h`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/src/common/sdk/nvidia/inc/class/cla06c.h): `KEPLER_CHANNEL_GROUP_A = 0xA06C`; `nvos.h`: `NV_CHANNEL_GROUP_ALLOCATION_PARAMETERS` | Group parameters include `hVASpace` and `engineType`. Class existence does not prove GL uses it in the expected sequence. |
| Context-share class | [`cl9067.h`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/src/common/sdk/nvidia/inc/class/cl9067.h): `FERMI_CONTEXT_SHARE_A = 0x9067`; `nvos.h`: `NV_CTXSHARE_ALLOCATION_PARAMETERS` | Fields are `hVASpace`, `flags`, `subctxId`. `subctxId` is input/output with documented verification-specific uses; do not invent a differently named field. |
| Ampere GPFIFO class | [`clc56f.h`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/src/common/sdk/nvidia/inc/class/clc56f.h): `AMPERE_CHANNEL_GPFIFO_A = 0xC56F` | Candidate channel class; runtime use unobserved. |
| Channel parameters | [`alloc/alloc_channel.h`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/src/common/sdk/nvidia/inc/alloc/alloc_channel.h): `NV_CHANNEL_ALLOC_PARAMS` | Contains `hContextShare`, `hVASpace`, USERD handles/offsets and instance/RAMFC/method-buffer descriptors. It does not contain a generic `subcontextID` field to rewrite. Context-share association matters. |
| Runqueue behavior | Same header: `NVOS04_FLAGS_GROUP_CHANNEL_RUNQUEUE` | Source comment says channels on different runqueues within a TSG *may* feed methods simultaneously. Not a guarantee of active GL/CUDA overlap. |

No handwritten struct, guessed offset, or executable ioctl example is supplied. Any later observer must decode the actual request size/version and preserve both syscall and RM statuses.

## 3. Validation, GSP, and lifetime evidence

### Resource duplication is validated

[`src/nvidia/src/libraries/resserv/src/rs_server.c`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/src/nvidia/src/libraries/resserv/src/rs_server.c), `serverCopyResource()`, locks both clients, checks active state, calls `clientValidate()` on the destination, resolves the source resource and destination parent, rejects invalidated/noncopyable resources, then calls `clientCopyResource()`.

This is a validation boundary, not an unrestricted handle alias primitive. This bounded review does not trace every access-control check inside `clientCopyResource()` and class policy. Same-process and cross-process success remain untested; non-root permission must not be asserted either way.

### TSG duplication explicitly includes GSP

[`kernel_channel_group_api.c`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/src/nvidia/src/kernel/gpu/fifo/kernel_channel_group_api.c), `kchangrpapiCopyConstruct_IMPL()`:

- Shares the underlying `KernelChannelGroup` through `serverRefShare()`.
- Adds a dependency when matching destination VAS state is found.
- Duplicates a kernel graphics context and legacy context-share handles when present.
- Calls `NV_RM_RPC_DUP_OBJECT` for virtual GPUs **or `IS_GSP_CLIENT(pGpu)`**, propagates failure, and requests automatic RPC free.
- Contains failure cleanup for duplicated child handles and the shared reference.

Its ordinary constructor also validates supported class, device/parent, engine/runlist and VAS state, with a separate GSP allocation RPC path. Therefore, GSP duplication exists in this release's host code. Firmware acceptance and end-to-end sharing correctness still require runtime evidence.

### Context shares preserve VAS and group relationships

[`kernel_ctxshare.c`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/src/nvidia/src/kernel/gpu/fifo/kernel_ctxshare.c), `kctxshareapiConstruct_IMPL()`, requires an existing channel-group parent. Nonlegacy construction resolves the requested VAS and calls `kctxshareInitCommon()` with flags and `subctxId`; it adds VAS/graphics dependencies. Legacy mode instead uses the parent group's VAS.

`kctxshareapiCopyConstruct_IMPL()` also has a GSP duplication RPC path for nonlegacy context shares. `kctxshareapiDestruct_IMPL()` handles shared references and calls destruction on the last reference. Legacy/nonlegacy behavior must be observed, not silently interchanged.

### Channel privilege is not an arbitrary user flag

[`kernel_channel.c`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/580.95.05/src/nvidia/src/kernel/gpu/fifo/kernel_channel.c), `kchannelConstruct_IMPL()`, clears client-supplied internal fields and derives privilege through driver paths. Its comments explain how GSP-RM receives privilege information. The allocation header limits the privileged-channel flag's role; it is not a general permission switch for page-table edits.

These code paths justify lifecycle/validation work. They do not prove a particular failed operation causes a named Xid, firmware crash, orphan, or lock leak. No such failure was observed in this assessment. Site-managed fault monitoring/recovery remains a prerequisite because malformed control state could affect the allocated device or shared host.

## 4. Remaining native OpenGL and control-plane gaps

1. **Actual object graph:** No trace identifies GL's TSGs, channel classes, engine/runqueue selections, context shares, or allocation timing. Vulkan's public queue API does not expose NVIDIA RM TSG handles or pushbuffer layouts either; both need internal evidence.
2. **Shared control mappings:** V2 §4.2 explains why redirecting channels without graphics control-buffer access can fault. Generic CPU memory mapping, CUDA/GL image registration, or external-memory APIs are not proof of PDE grafting. The source reviewed here does not supply a verified user-space grafting recipe for this stack.
3. **Address-space safety:** CUDA-high/graphics-low virtual-address separation is the paper's observation, not a guaranteed GL allocation convention. Identify regions and collisions before any manipulation.
4. **Initialization and lifetime:** Determine when the graphics TSG is ready, which CUDA channels to reserve, stream/channel associations, quiescence, handle dependencies, and failure/exit cleanup. Existing constructors/destructors do not automatically validate a shim's sequencing.
5. **SCG commands:** The paper describes undocumented mode/quota commands without supplying an executable architecture-specific command sequence in the reviewed text. The bounded source review did not establish one. Do not infer one from `tpcConfigID` or other similarly named fields.

Keep redirection, image handoff, and partitioning separate. A fixed TPC quota is a sensible first partition experiment only after verified commands and site approval. Do not substitute a GPC split or assume a fixed quota eliminates every transient. Preserve graphics capacity across active GPCs as required by the paper.

## 5. Information needed and next decision

Request the author artifact/commit, supported driver/firmware configurations, actual allocation matching rules, control-buffer identification/grafting implementation and permissions, stream/channel reservation rules, SCG class/method sequence, and initialization/teardown/error tests. Ask explicitly about native EGL/OpenGL and GA100 `580.95.05`; no successful author OpenGL experiment is recorded.

Without that artifact, this is independent reverse engineering. A reviewed, **non-mutating own-process runtime observer** is the next credible object-discovery step after site review, not an offline or zero-risk operation. `strace` alone is not guaranteed to decode nested RM argument buffers, so no unverified tracing command is presented as a complete solution.

**November 7 offline follow-up:** The [observer readiness review](GATE5_OBSERVER_REVIEW.md) now traces transfer-envelope routing, allocation-size dispatch, lifecycle payloads, and separate syscall/RM outcome channels in the same pinned release. It is a capture contract, not a compiled ABI check or implemented observer. No GL object graph/control mapping was captured, and no site approval was added.

See the [Gate 5 decision and unsent support inquiry](GATE5_ASSESSMENT.md) for operational stages. OSC may approve, constrain, or reject the experiment; the reviewed policy does not establish a blanket ban. EPFL also needs its own permission/isolation/recovery review. No root access, driver change, kernel reload, GPU reset, or renderer replacement is prescribed.
