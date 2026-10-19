---
name: physics-basics
description: How to build and test physics (the FORGE realization layer in C, the nvrm GB10 client and the AArch64 boot artifacts), the gates the Spark judge runs, the invariants that must not move, where receipts live, and what agents must not touch. Read before editing anything in this repository.
---

# physics basics

Facts from this repository's `README.md`, `Makefile`, `tests/gates/` and `docs/PIPELINE.md`.
Status is owned by aien-architecture (`CURRENT_EXECUTION_PLAN.md`, `doctrine/ROADMAP.md`).

## Build

- Needs host `gcc` and `make`, the AArch64 binutils and compiler (`aarch64-linux-gnu-gcc`,
  `-ld`, `-objcopy`; override with `CC`, `LD`, `OBJCOPY`, or `CROSS` for the M3 build),
  `qemu-system-aarch64`, `socat` and `jq`.
- The `Makefile` holds only the host test targets for the m16 GPU wait primitive
  (`make host-tests-m16`: host test plus 3 mutants that must be caught). Other builds are driven
  by the gate scripts (`build.sh`, `m2_build.sh`, `m3/`).

## Full host test target and gates

- `tests/gates/run.sh <step>` is the one entry point; each `tests/gates/*.gate` manifest names
  this script and one step word. The host steps are `forge-v2`, `forge-aegis`, `analog-sim`,
  `form0`, `gate34-writer`, `gb10-lock`, `zero-disk-secrets`, `m2`, `m3`, plus
  `make host-tests-m16`. Run all of them before a pull request.
- `analog-sim`, `form0` and `gate34-writer` need a sibling `../omega` checkout; `m3` and
  `crumb-verify` need `../aien-architecture`. They exit 3 with a message when it is missing.
- `.aien/always` lists the gates that run on every commit: CRUMB-VERIFY,
  PHYSICS-ZERO-DISK-SECRETS.
- The DGX Spark is the only test judge for pull requests (`docs/PIPELINE.md`). Its verdict is the
  commit status "AIEN Verification (Spark)".
- GB10 gates (`forge-gate34`, `m15`, `m16-requal`, `nvrm-lifecycle`, `submission-visibility`) run
  on the DGX Spark one at a time and are never killed mid-run. They take `/tmp/aien-gb10.lock`
  through `tests/gb10_lock.sh`. Do not take that lock yourself.
- `./run_m2_gates.sh` leaves the committed receipt untouched unless `--write-receipt` is passed;
  that flag is only for a recorded requalification decision.

## Invariants that must not move

- FORGE is realization and lowering, not a security gatekeeper: AEGIS verifies, AIENOS owns the
  trusted substrate.
- Language rule (aien-architecture ADR 0024): Rust is scaffolding, Omega is the destination; C
  only where hardware justifies it (FORGE, `nvrm/`), assembly only where measured (the boot
  artifacts). No Python. No CUDA toolkit or CUDA library dependence: the GB10 is driven through
  its open kernel interface. No systemd. Offline builds.
- An emulator (QEMU) pass is never reported as native DGX Spark qualification.
- `third_party/nvidia-open-580.173.02/` keeps NVIDIA's own license terms and its `SHA256SUMS`.
- omega pins this repository by commit (`physics.lock` in omega).

## Receipts and evidence

- `evidence/` holds receipts and raw logs. Add only: old receipts are never edited, and
  committed receipts or binaries are not changed without matching rebuilt evidence.
- `evidence/m16-blackwell-native-path-receipt.json` is superseded; do not cite it as a
  qualification (see README "Evidence notes").
- Signed Spark results are published to aien-dev/aien-receipts at `physics/<sha>/bundle.json`.

## Do not touch

- `.crumb`, `crumb.lock` (Crumb Protocol files; do not run `crumb compile`).
- `.aien/`, `tests/gates/`, `.github/workflows/`, `.forgejo/`, `tools/aien-test/`: a head that
  changes these is BLOCKED_OPERATOR and only the operator runs it (`docs/PIPELINE.md`).
- Existing files under `evidence/`, and `qualification_receipt.json`.
