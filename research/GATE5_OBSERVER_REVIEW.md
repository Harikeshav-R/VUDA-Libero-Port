# Gate 5: Own-process allocation-observer readiness review

## Decision and boundary

**The exact-release source supplies a decoding foundation, not a deployable observer.** Continue offline observer design and transparency validation. Runtime capture still requires site review, an authorized allocation, and an implemented/reviewed observer. Channel redirection additionally requires a complete shared-control-mapping design; none was established here.

EPFL is the permanent host lab. OSC Ascend is a temporary testbed. This review targets NVIDIA open-kernel release `580.95.05`, commit `2b436058a616676ec888ef3814d1db6b2220f2eb`, matching the recorded A100/GSP evidence. Source behavior is not firmware acceptance or an observed proprietary OpenGL allocation sequence. No remote connection, GPU run, interception, or device-control call occurred.

## 1. Source-backed findings

Links below pin that exact commit. Raw source bytes and URL/SHA256 mappings are retained privately under `evidence/osc/ascend/exp005-offline/observer/PROVENANCE.json`.

| Boundary | Exact source | Finding and decoding consequence |
| --- | --- | --- |
| Command namespace | [`nv-ioctl-numbers.h:29–37`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/kernel-open/common/inc/nv-ioctl-numbers.h#L29-L37), [`nv_escape.h`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/src/nvidia/arch/nvalloc/unix/include/nv_escape.h) | NVIDIA defines ioctl magic `'F'`, frontend base `200`, and transfer escape `NV_IOCTL_BASE + 11`. RM escapes are a separate set: allocation `0x2B`, free `0x29`, control `0x2A`, duplication `0x34`. Do not add base `200` to these RM numbers or confuse them with `NV04_*` API function IDs. |
| Outer request | [`nv.c:2405–2451`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/kernel-open/nvidia/nv.c#L2405-L2451) | The frontend extracts `_IOC_NR(cmd)` and `_IOC_SIZE(cmd)`. The full request word, device identity, and architecture are needed; a command byte alone is not a complete request encoding. |
| Transfer envelope | [`nv-ioctl.h:40–52`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/kernel-open/common/inc/nv-ioctl.h#L40-L52), [`nv.c:2408–2435`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/kernel-open/nvidia/nv.c#L2408-L2435), [`nv.h:70`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/kernel-open/common/inc/nv.h#L70) | `nv_ioctl_xfer_t` carries `cmd`, `size`, and aligned `ptr`. The kernel checks the outer size against `sizeof(nv_ioctl_xfer_t)`, copies the envelope, unwraps the inner command/size/pointer, and rejects inner sizes above `NV_ABSOLUTE_MAX_IOCTL_SIZE` (`16384`). A transfer request needs two distinct capture layers. |
| Allocation variants | [`escape.c:287–291,381–429`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/src/nvidia/arch/nvalloc/unix/src/escape.c#L287-L429), [`nvos.h:462–488`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/src/common/sdk/nvidia/inc/nvos.h#L462-L488) | `NV_ESC_RM_ALLOC` accepts either `sizeof(NVOS21_PARAMETERS)` or `sizeof(NVOS64_PARAMETERS)` and chooses the access variant by size. Shared leading-field offsets have compile-time assertions. Both carry `pAllocParms`, `paramsSize`, and `status`; NVOS64 also carries requested-rights pointer and serialization flags. |
| Lifecycle variants | [`escape.c:432–452,656–669,716–777`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/src/nvidia/arch/nvalloc/unix/src/escape.c#L432-L777), [`nvos.h:2229–2273`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/src/common/sdk/nvidia/inc/nvos.h#L2229-L2273) | Free, duplication, and control each require their own exact payload size. Duplication distinguishes source and destination clients/objects; free identifies root, parent, and old object; control adds command, flags, and nested parameter buffer. One generic handle tuple is insufficient. |
| Return channels | [`nv.c:2759–2785`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/kernel-open/nvidia/nv.c#L2759-L2785), [`escape.c:936–939`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/src/nvidia/arch/nvalloc/unix/src/escape.c#L936-L939) | The outer RM dispatch return becomes syscall success or `-EINVAL`; earlier copy/size failures have separate paths. A dispatched operation can report its outcome through the embedded payload `status` while the dispatcher returns `NV_OK`. Capture syscall result/errno and valid returned RM status separately. |

No handwritten ABI structs, guessed offsets, or executable ioctl requests are supplied. Before implementation, derive `sizeof`/`offsetof` using the complete pinned headers and the target Linux x86_64 toolchain. This session did not compile an ABI-layout check. Do not use an illustrative byte count as a portable contract.

### Candidate nested allocation payloads

These classes are source-defined candidates, not an observed GL object graph:

- `KEPLER_CHANNEL_GROUP_A` (`0xA06C`): `NV_CHANNEL_GROUP_ALLOCATION_PARAMETERS`, including `hVASpace` and `engineType`. See [`nvos.h:2900–2911`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/src/common/sdk/nvidia/inc/nvos.h#L2900-L2911).
- `FERMI_CONTEXT_SHARE_A` (`0x9067`): `NV_CTXSHARE_ALLOCATION_PARAMETERS`, including `hVASpace`, `flags`, and input/output `subctxId`. See [`nvos.h:3198–3248`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/src/common/sdk/nvidia/inc/nvos.h#L3198-L3248).
- `AMPERE_CHANNEL_GPFIFO_A` (`0xC56F`): `NV_CHANNEL_ALLOC_PARAMS`, including `hContextShare`, `hVASpace`, USERD and other control descriptors. See [`alloc_channel.h:296–342`](https://github.com/NVIDIA/open-gpu-kernel-modules/blob/2b436058a616676ec888ef3814d1db6b2220f2eb/src/common/sdk/nvidia/inc/alloc/alloc_channel.h#L296-L342).

Class constants are preserved in the matching `cla06c.h`, `cl9067.h`, and `clc56f.h` snapshots; the [existing driver assessment](GATE5_DRIVER_ASSESSMENT.md#2-exact-release-candidate-interfaces) links their release definitions. Unknown classes and serialized payloads must remain undecoded until their exact formats are reviewed. Allocation records alone do not establish all graphics control mappings or active spatial execution.

## 2. Proposed capture contract — not implemented

The following requirements are engineering decisions inferred from the source and the project's evidence/safety constraints. They are not claims that an interposer already meets them.

### Transparent forwarding and bounded observation

- Forward each original application call once, with its original fd, full request, argument bits, and memory contents. Never synthesize allocation, duplication, free, mapping, or control requests.
- Preserve application-visible return values and errno, including errno around pre-call inspection and post-call logging. Observation failure must not become an application ioctl failure or trigger a retry.
- Establish actual device identity. Magic `'F'` alone is not proof of an NVIDIA fd; account for fd close/reuse and aliases. Leave non-target devices uninterpreted.
- Resolve forwarding safely and guard recursion/reentrancy. Do not assume a constructor, `dlsym`, heap allocation, or a logging function is automatically safe inside a driver call path.
- Resolve the variadic ABI before choosing `LD_PRELOAD`: not every intercepted ioctl has a pointer-valued third argument, or any third argument. Unconditionally reading a pointer with `va_arg` is not a transparent forwarding design. Direct syscalls, other symbol bindings, and helper processes may bypass interposition; record that coverage limit.
- Use bounded records/storage and explicit truncation/drop/error counts. Logging can block, allocate, perturb timing, fail partially, or change errno; direct `write()` alone is not a guarantee of non-blocking or transparent behavior.
- Prove thread ordering, initialization, shutdown, fork/exec, and log-fd lifetime behavior. A PID check or `pthread_atfork` call alone does not prove correctness. Capture is an observation experiment, not a performance baseline.

### Safe decoding and status validity

- Capture outer request metadata first. Only inspect recognized exact-size wrappers on confirmed target fds. For transfer envelopes retain outer and inner fields separately; do not mistake an inner command for a full ioctl request word.
- Implement and validate a fault-contained user-memory read mechanism before dereferencing application pointers. Canonical-address/range checks alone do not establish readability. Invalid or concurrently changed buffers must produce an explicit unreadable/incomplete record, not a process crash or a guessed decode.
- Bound every read by the supplied length, reviewed struct size, and a documented capture budget. Record original length and truncation. The kernel's `16384` outer-transfer ceiling is not a universal safe bound for nested `pAllocParms` buffers.
- Treat NVOS64 serialization flags as a decoding boundary; never cast serialized bytes as native class structs. Unknown flags, classes, wrappers, rights masks, and nested pointers remain opaque metadata.
- Capture relevant inputs before the real call and output fields/nested input-output parameters afterward. Record whether each snapshot was readable and complete. A failed syscall/copy-out can leave caller memory stale; an observed `status` value is not automatically a valid operation result.
- Retain syscall outcome even if RM status is unavailable. Do not invent numeric status meanings; use the matching complete status definitions when adding symbolic decoding.

### Object graph and provenance

- Record process/exec generation, thread, fd generation/device identity, request sequence, monotonic timestamps bracketing the real call, raw class/command, wrapper variant, lengths, and capture validity. Keep private addresses, handles, PIDs, UUIDs, and paths out of public derivatives.
- Record allocation root/parent/requested handle/class and returned handle/status. Record duplication source/destination relationships and successful frees. Keep generations so reused handles do not merge unrelated lifetimes.
- Preserve failed operations as failures; do not add successful graph edges merely because the syscall returned zero. Root/parent destruction and process/fd exit may invalidate more than one object. Mark incomplete lifetimes rather than manufacturing teardown order.
- Associate API phases with EGL initialization, current-context binding, first rendered work, CUDA initialization/work, and teardown. Thread identity alone does not prove whether a driver worker call belongs to GL or CUDA.
- An allocation-only observer cannot certify GPU VA ranges, control-buffer accessibility, or mapping correctness. A later mapping/control observer needs its own reviewed scope; do not blindly log arbitrary control buffers or unrelated process data.

## 3. Validation and smallest later runtime capture

Before target deployment, require Linux-side tests/smokes covering supported wrappers, unreadable/short buffers, unknown requests, both return channels, recursion, concurrent calls, logging failure, and fork/exec. Validate layouts from pinned headers rather than retyping them. Compare the real program with and without observation, including output correctness, operation outcomes, teardown, and capture completeness. These tests were not created or run here.

After implementation, review, and **permission for the chosen site**, use the existing finite native EGL/CUDA probe, not a new PyTorch environment, for the smallest capture. Stage solo GL, solo CUDA, both initialization orders, independent work, and teardown. Observe only its own approved device/object operations. No parent rewrites, extra duplication calls, PTE changes, SCG commands, service changes, or GPU reset.

Success is a bounded, attributable lifecycle transcript with declared coverage, preserved application behavior, and no unexplained failures. It may identify GL/CUDA groups and context shares; it must not assume that every expected class appears. Unreadable payloads, unexplained classes/statuses, drops, or device mismatch stop graph conclusions. Site staff define fault monitoring/recovery.

Approval at OSC does not authorize EPFL experiments, or vice versa. Runtime capture remains unperformed.

## 4. Remaining blockers and next decision

- Actual GL wrapper/class choice, serialized-buffer use, TSG creation timing, worker/API attribution, and initialization/teardown remain unobserved.
- No compiled target layout check, transparent forwarding mechanism, safe memory reader, observer binary, or runtime regression evidence exists yet.
- The author artifact/control-buffer mapping procedure and architecture-specific SCG commands remain unavailable. This observer specification is not a replacement for them.
- Static decode feasibility is stronger than before: the transfer envelope, allocation-size dispatch, lifecycle variants, and independent return channels are source-backed. That justifies further offline implementation review, not device mutation.

The [Gate 4 protocol](GATE4_CAPTURE_PROTOCOL.md) remains a separate standard-API measurement path. Prefer supported observation before internals; neither shared handles nor overlapping elapsed ranges prove the mentor's spatial-sharing goal.
