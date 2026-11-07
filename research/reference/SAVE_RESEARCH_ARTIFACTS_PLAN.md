> Public derivative of the original research/export record. Private screenshots, raw logs, coursework, ZIP, and local paths are omitted or redacted. Original files remain local in `research/archive/2026-11-06/`. Historical completion claims and checksums describe that original bundle, not this derivative. Current status: [research/STATUS.md](../STATUS.md).

# Save Research Information and Artifacts

## Context

The user requests saving all substantive research information and all generated artifacts into `PROJECT_ROOT`. The existing `VUDA_OPENGL_LIBERO_PLAN.md` already matches the session-local canonical research plan byte-for-byte. Save the missing diagnostic script, consolidated session notes, three supplied reference images, this export plan, and a checksum manifest without changing the existing project or running any OSC jobs. This is a file-export task, not implementation of a CUDA/OpenGL prototype.

## Source inventory and evidence

All staging files live in this physically observed source directory, which remains usable after conversation compaction:

```text
PRIVATE_STAGING_DIRECTORY
```

The original staging inventory contained exactly the canonical research plan, the diagnostic script, and three PNGs. The comprehensive session notes and this export plan were then created for this request. `RESEARCH_ARTIFACTS.sha256` is staged after this plan is written; it records every payload's final SHA-256 except its own digest.

| Staged source basename | Repository basename | Purpose | SHA-256 established during preparation |
| --- | --- | --- | --- |
| `vuda-opengl-feasibility-plan.md` | `VUDA_OPENGL_LIBERO_PLAN.md` | Complete evidence-backed technical plan, 23 primary-source groups, OSC hardware/defaults/commands | `64e412aeaf3478924e0fa5d50630fd48e9f39da623e26912dc951e00456e724a` |
| `VUDA_SESSION_NOTES.md` | `VUDA_SESSION_NOTES.md` | Original assignment, mentor clarification, complete corrected findings, cluster information, commands, artifact search and verification limits | `2f032877d1ef3b5cee78b61d6c1558cdc7ce1b85cbb570522ac27835deefa850` |
| `pitzer-diagnostics.sh` | `pitzer-diagnostics.sh` | Read-only inventory collector for Ascend/Cardinal/Pitzer | `1350f3e53c6f0f9faab8f6474c4646ff913e7d96cde00806148440460f9b3e26` |
| `pasted-image-55c5202555566d6e.png` | same | User's resource-storage diagram | `4878306097504edaedb05268b0bd195600b47dd4d2a9ce19ebd390ad8de95281` |
| `pasted-image-ed38f8ea94fcefa3.png` | same | Mentor's research brief | `3b9b910ad53e013a340c701fa493da6ba1996385bd4e77b5b7a302453fad5b58` |
| `pasted-image-402d85a512a4f2fa.png` | same | Screenshot of the user's scope-clarification messages | `001973dc2e9429838cc725783a397be61df3f94cce3bd0c66ce29c92801bbe23` |
| `save-research-artifacts-plan.md` | `SAVE_RESEARCH_ARTIFACTS_PLAN.md` | This execution specification | Use staged checksum manifest created after this plan |
| `RESEARCH_ARTIFACTS.sha256` | same | Portable verification manifest with repository-relative filenames | Self-digest intentionally excluded |

Observed destination files before execution are `README.md`, `LICENSE`, `.gitignore`, `.git/`, and `VUDA_OPENGL_LIBERO_PLAN.md`. Preserve all existing content. No standalone `vuda-paper-findings.md`, `libero-render-findings.md`, or `opengl-cuda-findings.md` was present despite initial subagent claims. Do not invent or export those missing files; the consolidated notes document this limitation. User-supplied images are included to preserve context even though they are not assistant-generated.

## Approach

### 1. Resolve sources and preserve destination changes

Use the physical source directory above, not an assumed current session's `local://` mapping. Re-read the eight source artifacts before copying and check their hashes against `RESEARCH_ARTIFACTS.sha256`, using the basename mapping above. Do not fetch new sources, install dependencies, modify the research claims, or regenerate historical images.

Choose the repository root as the export base if every planned destination is absent or already identical to its staged source. An identical existing `VUDA_OPENGL_LIBERO_PLAN.md` requires no write. If any planned destination has different content, preserve it and export the entire eight-file bundle into `research-export-64e412aeaf34/` under the repository instead. If that directory conflicts, use the smallest positive integer suffix `research-export-64e412aeaf34-1/`, `-2/`, etc. Select an absent directory or one whose complete intended destinations are identical. Keep the bundle together so relative links remain valid. Never overwrite an unexpected file or ask to delete it.

### 2. Copy the complete bundle without alteration

Copy all table entries to the selected export base, preserving text bytes and image bytes. Prefer a no-clobber file copy; binary copying with `cp -n` is appropriate because the text write tool is not a binary image writer. Treat `cp -n` skipping a file as insufficient proof: verify content afterward. Do not rewrite the existing detailed plan just to replace historical `local://` references. `VUDA_SESSION_NOTES.md` supplies the portable source-to-repository link mapping, and its research links use the repository basenames.

Preserve the diagnostic script's content and executable mode from the staged source when feasible; the documented invocation uses `bash pitzer-diagnostics.sh` and does not require executable permission. Leave `README.md`, `.gitignore`, `LICENSE`, and Git metadata untouched. Do not commit or push.

### 3. Verify the export before reporting it saved

Run the checks in Verification from the export base. Report every saved filename, whether the detailed plan was already identical, and the exact export base if conflict preservation selected a subdirectory. State that the files contain research and a diagnostic collector, not a working VUDA prototype or measured OSC results.

## Verification

The portable checksum manifest must verify all seven non-manifest payloads:

```bash
shasum -a 256 -c RESEARCH_ARTIFACTS.sha256
bash -n pitzer-diagnostics.sh
```

Both commands must exit 0. Each manifest entry must report OK. If any destination hash differs, inspect the specific source/destination and re-copy without overwriting unrelated files; do not edit the manifest to match a bad copy.

Exercise the diagnostic safety path from the export base, without issuing a real cluster diagnostic or contacting OSC:

```python
from pathlib import Path
import os
import subprocess

script = Path("pitzer-diagnostics.sh").resolve()
environment = dict(os.environ)
environment.pop("SLURM_JOB_ID", None)
result = subprocess.run(
    ["bash", str(script)],
    env=environment,
    text=True,
    capture_output=True,
    check=False,
)
assert result.returncode == 2, result
assert "No SLURM_JOB_ID is set" in result.stderr, result
print("PASS: collector refuses execution outside a Slurm allocation")
```

Expected: exit 2 from the collector and the explicit missing-allocation message, before any inventory commands run. This proves the copied artifact's relevant safety behavior without fabricating GPU availability.

Verify the five relative research/image links at the top of `VUDA_SESSION_NOTES.md` all resolve to files within the export base. The three PNGs must have their source-identical hashes; that establishes binary preservation without re-encoding them. Verify the source digest of the original existing detailed plan remains unchanged if that file was preserved in the repository root.

Local preparation already established source hashes, identical existing/canonical plan content, shell/Python syntax, and the collector's safety guard. Those checks do not prove the repository export has happened: run the destination checks after copying.

## Assumptions and contingencies

- Save substantive user-facing research information and real artifacts, including supplied reference images. Do not include unsupported subagent drafts or internal tool transport logs as authoritative research deliverables.
- All staged source files were observed on disk. If a source becomes unavailable, preserve and save all reachable files, identify the exact missing source, and do not claim a complete export. Never fabricate a replacement image, source artifact, benchmark, or raw research output.
- No OSC system access is needed for this save operation. Do not allocate cluster GPUs, create EGL contexts, run inference/training, or change drivers while executing it.
