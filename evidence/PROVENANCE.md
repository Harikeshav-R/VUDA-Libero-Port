# Evidence provenance and publication boundaries

## Public artifacts

| Artifact | Origin and interpretation |
| --- | --- |
| [Sanitized Ascend summary](public/ascend-inventory.md) | Derived from user-supplied OSC inventory for job 8374257 and later pasted toolchain output. The assistant did not execute remote commands. Private identifiers and unrelated host details are withheld. Documents temporary testbed while awaiting EPFL host lab GPU access. |
| [Sanitized EXP-002 probe results](public/exp002-probe-results.md) | Sanitized empirical benchmark and correctness results from native probe execution under Slurm job 8374921 on OSC Ascend (A100-PCIE-40GB). |
| [Sanitized EXP-003 profile results](public/exp003-profile-results.md) | Nsight timeline data from job 8375678 on temporary OSC Ascend. Gate 5 review corrects earlier TSG/preemption claims; active execution is inconclusive and final export failed on an existing file. |
| [Technical plan derivative](../research/reference/VUDA_OPENGL_LIBERO_PLAN.md) | Public derivative of the original historical source-based analysis, retaining citations S1–S23. Not an executed port. |
| [Session notes derivative](../research/reference/VUDA_SESSION_NOTES.md) | Public derivative preserving mentor requirements, interpretation corrections, and historical verification limits. Private screenshots are omitted. |
| [Export plan derivative](../research/reference/SAVE_RESEARCH_ARTIFACTS_PLAN.md) | Public derivative of the historical export procedure; workstation/staging paths are redacted. Its completion claims and original checksums refer to the private original bundle. |
| [Gate 5 assessment](../research/GATE5_ASSESSMENT.md) and [driver source map](../research/GATE5_DRIVER_ASSESSMENT.md) | New source-based review plus read-only analysis of existing local trace artifacts. No new GPU run or site approval. Exact driver release and primary citations are recorded in the reports. |
| [EXP-005 metadata audit](public/exp005-trace-metadata-audit.json), [observer review](../research/GATE5_OBSERVER_REVIEW.md), and [capture protocol](../research/GATE4_CAPTURE_PROTOCOL.md) | November 7 offline follow-up: read-only queries of preserved EXP-003 metadata and exact-version primary-source review. No remote connection, GPU run, interception, or support message. Public audit omits host/user/path/device identifiers. |
| [EXP-006 live findings](public/exp006-live-prerequisites.md) | Assistant-executed live login/module/scheduler checks, a new non-overwriting export from the hash-matched report, full read-only event-content comparison, and completed authorized GPU preflight. No application workload or trace in EXP-006. Private project/node/device/path identifiers remain in ignored raw captures. |
| [EXP-007 native capture results](public/exp007-profile-results.md) and [sanitized audit](public/exp007-profile-audit.json) | Assistant-executed authorized native build/run and new Nsight captures. Preserves initial pre-build guard failure, private UUID-guard repair, actual successful single-inventory allocation, ten unprofiled runs, four profiles/fresh exports, and read-only audits. Switch/metric records are not active-spatial-sharing or RM TSG proof. |
| [EXP-008 correctness and latency record](public/exp008-correctness-latency.md) | Sanitized derivative of original/candidate source snapshots, test-only API fault scenarios, allocation/launcher records, and runtime outcomes as recorded in report. Static output checks do not prove iteration freshness or spatial sharing. |
| [EXP-009 controlled scheduling plan](../research/EXP009_CONTROLLED_SCHEDULING_PLAN.md) | Documentation-only synthesis of established EXP-008 limits and proposed next controls. Defines an implementation/execution contract, not an executed experiment, new allocation, measured headroom, or active-spatial-sharing finding. |
| [Active collector](../scripts/pitzer-diagnostics.sh) | Existing read-only diagnostic script moved into scripts/. It prints to stdout; callers capture logs explicitly. |
| [Public checksum manifest](PUBLIC_SHA256SUMS) | Repository-root-relative hashes of public research references, inventory/experiment derivatives, Gate 5 reports, controlled-experiment plan, and active collector. Excludes itself and living index/status documents. |

The user explicitly chose a sanitized public workspace. The public summary is not a raw capture; public checksums prove file integrity, not independently authenticated execution. The historical references were not re-fetched or re-validated against live upstream sources during publication.

## Local-only originals

These paths are retained in the original working directory but ignored by Git:

- `research/archive/2026-11-06/`: original research/export Markdown, original checksum manifest, collector snapshot, three original screenshots, and original export ZIP.
- `evidence/osc/ascend/8374257/osc-system-info-ascend-8374257.txt`: original remote inventory, moved without changing bytes.
- `evidence/osc/ascend/8374257/toolchain-followup.txt`: labeled transcription of later user-pasted commands/output, with module separators and blank lines normalized.
- `evidence/osc/ascend/8374921/vuda_exp002_8374921.log`: raw execution log of native probe EXP-002; private node/account identifiers remain only in originals.
- `evidence/osc/ascend/8375678/`: raw Nsight report, SQLite export, summaries, execution log and module warnings. The execution log records export exit code 1. EXP-006 now reproduces all non-export-metadata contents through a fresh derivation from the hash-matched report; this establishes content correspondence, not historical execution authenticity.
- `evidence/reference/module6_assignment_cnn_intro.ipynb`: unchanged coursework reference; no coursework/grading cells were executed.
- `evidence/SHA256SUMS`: original preservation manifest. `evidence/osc/ascend/gate5-inputs.sha256` separately records later EXP-002/EXP-003 inputs reviewed for Gate 5; existing manifests are retained.
- `evidence/osc/ascend/exp005-offline/`: raw metadata-query snapshot and primary-source snapshots for the November 7 offline follow-up; separate preservation manifest retained alongside them.
- `evidence/osc/ascend/exp006-live-prerequisites/`: exact remote commands/output, user-approved allocation records, preflight logs, original-version fresh SQLite export, complete comparison results, raw source captures/provenance, and a separate experiment manifest.
- `evidence/osc/ascend/exp007-profiler-smoke/`: immutable first-attempt input manifest and failed logs/inventory; separately hashed repaired launcher/identity helpers and source snapshots; seven CPU boundary-case receipts; raw identity specifications/provenance; successful allocation logs, executed binaries, four report/SQLite pairs, ten unprofiled logs, terminal scheduler states, remote/local hash receipts, and accepted read-only analyses. The rejected interpretation draft is retained and explicitly not accepted as evidence. A new experiment preservation manifest supplements, rather than rewrites, historical manifests.
- `evidence/osc/ascend/hardware-trace-review-20261108/`: exact-version Nsight 2025.6 raw guide, URL/time/hash provenance, documented architecture quote, and saved installed-help excerpt supporting the later HES explanation. This is source research, not a new allocation, profiler run, or successful Blackwell test. Its separate manifest does not rewrite earlier experiment manifests.
- `evidence/osc/ascend/exp008-correctness-latency/`: immutable failed submission capture and separate successful retry; original/candidate sources, effective source/binary digests, scheduler and physical-identity records, controlled API fault inputs, raw logs, aggregates, and separate preservation manifest. Job `8391277` completed the bounded correctness/latency experiment; this does not establish spatial sharing.
- `evidence/osc/ascend/exp009-planning/`: documentation provenance and local verification receipts only. Not a GPU allocation or EXP-009 runtime capture.

A fresh clone intentionally lacks those paths. The original screenshots and ZIP were recovered from the staging location recorded in the private export plan. They are now preserved locally, so future work in that original workspace does not depend on the staging location. The ZIP is the received historical export, not an export of the current public layout.

Original archive payloads were checked against all seven hashes in their original manifest; ZIP integrity passed. The supplied notebook copy matched its local source checksum. Publication leaves these originals unchanged. Living documents describe the public/private split rather than linking publicly to unavailable private files.

## Capture timing and missing inputs

The raw inventory reports 2026-11-07 00:41 UTC, corresponding to November 6 local time. The later module/toolchain output has no independent timestamp or repeated job identity; its association with job 8374257 comes from conversation context.

The user reports emailing the VUDA authors; no response/source is recorded. EXP-004/EXP-005 reviewed existing files offline. EXP-006 subsequently connected live to OSC, performed a lightweight historical-report export, and ran an authorized GPU identity/tool-capability preflight without application work. EXP-007 then ran the native probe and new traces under the same user-confirmed research project; its successful application log spans UTC `2026-11-08 00:14:39–00:15:27`. EXP-008 separately ran original/updated probes and rejection/correctness controls, with successful application log UTC `2026-11-08 01:19:15–01:19:53`. Those captures correspond to November 7 locally. EXP-009 currently has documentation only, with no new remote connection, allocation, or runtime result in this planning step. No deployed LIBERO workload, policy checkpoint, or site approval for internal driver mutation is present. OSC remains temporary while awaiting EPFL host lab provisioning.
## Future evidence handling

1. Preserve original captures privately with stable experiment identifiers and explicit execution provenance.
2. Create separate public derivatives; remove usernames, project codes, node/login names, device UUIDs/serials, private paths, network endpoints, and unrelated records.
3. Keep private screenshots, coursework, and bundled archives outside public staging unless separately reviewed and explicitly approved.
4. Link public findings to the research ledger and update the public checksum manifest for changed covered files.
5. Validate private manifests only where originals exist. Use the public manifest for fresh-clone checks.

The recorded course account demonstrated scheduler access, not authorization to charge unrelated research. No verified OSC blanket prohibition on user-space scheduler research was established. Seeking site guidance before undocumented device-control operations remains an operational precaution, not a claimed policy citation.
