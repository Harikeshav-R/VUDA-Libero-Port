# Evidence provenance and publication boundaries

## Public artifacts

| Artifact | Origin and interpretation |
| --- | --- |
| [Sanitized Ascend summary](public/ascend-inventory.md) | Derived from user-supplied OSC inventory for job 8374257 and later pasted toolchain output. The assistant did not execute remote commands. Private identifiers and unrelated host details are withheld. |
| [Technical plan derivative](../research/reference/VUDA_OPENGL_LIBERO_PLAN.md) | Public derivative of the original historical source-based analysis, retaining citations S1–S23. Not an executed port. |
| [Session notes derivative](../research/reference/VUDA_SESSION_NOTES.md) | Public derivative preserving mentor requirements, interpretation corrections, and historical verification limits. Private screenshots are omitted. |
| [Export plan derivative](../research/reference/SAVE_RESEARCH_ARTIFACTS_PLAN.md) | Public derivative of the historical export procedure; workstation/staging paths are redacted. Its completion claims and original checksums refer to the private original bundle. |
| [Active collector](../scripts/pitzer-diagnostics.sh) | Existing read-only diagnostic script moved into scripts/. It prints to stdout; callers capture logs explicitly. |
| [Public checksum manifest](PUBLIC_SHA256SUMS) | Repository-root-relative hashes of sanitized research references, the public inventory summary, and active collector. Excludes itself and living documentation. |

The user explicitly chose a sanitized public workspace. The public summary is not a raw capture; public checksums prove file integrity, not independently authenticated execution. The historical references were not re-fetched or re-validated against live upstream sources during publication.

## Local-only originals

These paths are retained in the original working directory but ignored by Git:

- `research/archive/2026-11-06/`: original research/export Markdown, original checksum manifest, collector snapshot, three original screenshots, and original export ZIP.
- `evidence/osc/ascend/8374257/osc-system-info-ascend-8374257.txt`: original remote inventory, moved without changing bytes.
- `evidence/osc/ascend/8374257/toolchain-followup.txt`: labeled transcription of later user-pasted commands/output, with module separators and blank lines normalized.
- `evidence/reference/module6_assignment_cnn_intro.ipynb`: unchanged copy of the supplied coursework notebook, retained only as setup reference. No coursework or grading cells were executed.
- `evidence/SHA256SUMS`: original local preservation manifest covering thirteen private/original artifacts and the active collector.

A fresh clone intentionally lacks those paths. The original screenshots and ZIP were recovered from the staging location recorded in the private export plan. They are now preserved locally, so future work in that original workspace does not depend on the staging location. The ZIP is the received historical export, not an export of the current public layout.

Original archive payloads were checked against all seven hashes in their original manifest; ZIP integrity passed. The supplied notebook copy matched its local source checksum. Publication leaves these originals unchanged. Living documents describe the public/private split rather than linking publicly to unavailable private files.

## Capture timing and missing inputs

The raw inventory reports 2026-11-07 00:41 UTC, corresponding to November 6 local time. The later module/toolchain output has no independent timestamp or repeated job identity; its association with job 8374257 comes from conversation context.

The user reports emailing the VUDA authors; no response or source was supplied. No OSC credentials, active remote connection, deployed LIBERO workload, actual policy checkpoint, profiler capture, or GPU workload execution output was supplied. The coursework notebook informed the Slurm request pattern; it is not the research model or workload.

## Future evidence handling

1. Preserve original captures privately with stable experiment identifiers and explicit execution provenance.
2. Create separate public derivatives; remove usernames, project codes, node/login names, device UUIDs/serials, private paths, network endpoints, and unrelated records.
3. Keep private screenshots, coursework, and bundled archives outside public staging unless separately reviewed and explicitly approved.
4. Link public findings to the research ledger and update the public checksum manifest for changed covered files.
5. Validate private manifests only where originals exist. Use the public manifest for fresh-clone checks.

The recorded course account demonstrated scheduler access, not authorization to charge unrelated research. No verified OSC blanket prohibition on user-space scheduler research was established. Seeking site guidance before undocumented device-control operations remains an operational precaution, not a claimed policy citation.
