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
| [Active collector](../scripts/pitzer-diagnostics.sh) | Existing read-only diagnostic script moved into scripts/. It prints to stdout; callers capture logs explicitly. |
| [Public checksum manifest](PUBLIC_SHA256SUMS) | Repository-root-relative hashes of public research references, inventory/experiment derivatives, Gate 5 reports, and active collector. Excludes itself and living index/status documents. |

The user explicitly chose a sanitized public workspace. The public summary is not a raw capture; public checksums prove file integrity, not independently authenticated execution. The historical references were not re-fetched or re-validated against live upstream sources during publication.

## Local-only originals

These paths are retained in the original working directory but ignored by Git:

- `research/archive/2026-11-06/`: original research/export Markdown, original checksum manifest, collector snapshot, three original screenshots, and original export ZIP.
- `evidence/osc/ascend/8374257/osc-system-info-ascend-8374257.txt`: original remote inventory, moved without changing bytes.
- `evidence/osc/ascend/8374257/toolchain-followup.txt`: labeled transcription of later user-pasted commands/output, with module separators and blank lines normalized.
- `evidence/osc/ascend/8374921/vuda_exp002_8374921.log`: raw execution log of native probe EXP-002; private node/account identifiers remain only in originals.
- `evidence/osc/ascend/8375678/`: raw Nsight report, SQLite export, summaries, execution log and module warnings. The execution log records export exit code 1; the report/export byte pairing is not independently established.
- `evidence/reference/module6_assignment_cnn_intro.ipynb`: unchanged coursework reference; no coursework/grading cells were executed.
- `evidence/SHA256SUMS`: original preservation manifest. `evidence/osc/ascend/gate5-inputs.sha256` separately records later EXP-002/EXP-003 inputs reviewed for Gate 5; existing manifests are retained.

A fresh clone intentionally lacks those paths. The original screenshots and ZIP were recovered from the staging location recorded in the private export plan. They are now preserved locally, so future work in that original workspace does not depend on the staging location. The ZIP is the received historical export, not an export of the current public layout.

Original archive payloads were checked against all seven hashes in their original manifest; ZIP integrity passed. The supplied notebook copy matched its local source checksum. Publication leaves these originals unchanged. Living documents describe the public/private split rather than linking publicly to unavailable private files.

## Capture timing and missing inputs

The raw inventory reports 2026-11-07 00:41 UTC, corresponding to November 6 local time. The later module/toolchain output has no independent timestamp or repeated job identity; its association with job 8374257 comes from conversation context.

The user reports emailing the VUDA authors; no response/source is recorded. Later EXP-002/EXP-003 artifacts now supply native GPU workload logs and a profiler capture. The current Gate 5 review analyzes those existing files offline and performs no remote login or GPU execution. No deployed LIBERO workload, policy checkpoint, or site approval for internal driver mutation is present. OSC remains temporary while awaiting EPFL host lab provisioning.
## Future evidence handling

1. Preserve original captures privately with stable experiment identifiers and explicit execution provenance.
2. Create separate public derivatives; remove usernames, project codes, node/login names, device UUIDs/serials, private paths, network endpoints, and unrelated records.
3. Keep private screenshots, coursework, and bundled archives outside public staging unless separately reviewed and explicitly approved.
4. Link public findings to the research ledger and update the public checksum manifest for changed covered files.
5. Validate private manifests only where originals exist. Use the public manifest for fresh-clone checks.

The recorded course account demonstrated scheduler access, not authorization to charge unrelated research. No verified OSC blanket prohibition on user-space scheduler research was established. Seeking site guidance before undocumented device-control operations remains an operational precaution, not a claimed policy citation.
