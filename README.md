# VUDA–OpenGL/LIBERO Research

Research into simultaneous native EGL/OpenGL rendering and CUDA model computation on one NVIDIA GPU, using VUDA's design as a starting point for LIBERO.

**Current state:** research and machine inventory only. No VUDA port, native GPU probe, measured concurrency, or LIBERO speedup exists yet.

## Start here

- [Current status and next steps](research/STATUS.md)
- [Research ledger: sources, findings, decisions, questions, experiments](research/RESEARCH_LOG.md)
- [OSC setup and operating commands](docs/OSC_RUNBOOK.md)
- [Agent context and working instructions](AGENTS.md)
- [Sanitized Ascend inventory and CUDA toolchain evidence](evidence/public/ascend-inventory.md)
- [Evidence provenance and publication boundaries](evidence/PROVENANCE.md)

## Public layout

```text
AGENTS.md
README.md
docs/
  OSC_RUNBOOK.md
evidence/
  PROVENANCE.md
  PUBLIC_SHA256SUMS
  public/
    ascend-inventory.md
research/
  STATUS.md
  RESEARCH_LOG.md
  reference/
    VUDA_OPENGL_LIBERO_PLAN.md
    VUDA_SESSION_NOTES.md
    SAVE_RESEARCH_ARTIFACTS_PLAN.md
scripts/
  pitzer-diagnostics.sh
```

Public research references are labeled derivatives of the original research/export records. They retain source findings and citations while omitting private attachments and workstation paths. Current status and the ledger supersede historical session status.

## Latest evidence

The user collected an Ascend A100 40-GB inventory under job `8374257`, with NVIDIA driver `580.95.05` and MIG disabled. Later user-pasted output confirms `cuda/12.8.1` loads and exposes nvcc `V12.8.93`. NVIDIA graphics libraries are present; actual EGL rendering and CUDA execution remain untested.

The next proposed experiment is a standard-API native EGL/OpenGL + CUDA probe. It must verify physical device placement and output correctness before measuring solo, serial, and independently submitted work. Timing alone will not establish spatial sharing.

## Public preservation checks

Run from the repository root:

```bash
shasum -a 256 -c evidence/PUBLIC_SHA256SUMS
bash -n scripts/pitzer-diagnostics.sh
```

These checks validate published derivatives and shell syntax, not GPU functionality. No application build or GPU test suite exists yet.

## Private originals

The original workspace retains raw OSC captures, mentor screenshots, an export ZIP, and the supplied coursework notebook. They are intentionally excluded from this public repository through `.gitignore`:

- `research/archive/`
- `evidence/osc/`
- `evidence/reference/`
- `evidence/SHA256SUMS`

A fresh clone will not contain those originals. Public findings are available through the sanitized evidence summary and research references. New captures require privacy review before publication; preserve originals privately and publish reviewed derivatives only.
