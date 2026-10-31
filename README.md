# PHYSICS

Two things live here: the **FORGE** machine realization layer (C), and the historical **Atlas / PHYSICS boot artifacts** (AArch64 assembly) with their evidence. FORGE turns a verified Omega meaning into a concrete realization on the machine at hand (today the CPU and the NVIDIA GB10 GPU; a simulated analog provider exists for testing). It is realization and lowering, not a security gatekeeper: AEGIS verifies, and AIENOS owns the trusted substrate. This repository also holds `nvrm/`, the small C client that drives the GB10 directly through the open NVIDIA kernel interface (no CUDA).

Part of [Sovereign Intelligence (SI)](https://github.com/aien-dev/sovereign-intelligence). SI's repository index lists it under "CUDA++ runtime" ([ADR 0045](https://github.com/aien-dev/aien-architecture-mv/blob/f13c6238e585b284a87e7f5b227f7f8a1bc6bef7/docs/adr/0045-aienix-retired-sovereign-intelligence-center.md)).

**Status 2026-10-19: research-grade, pre-alpha.** FORGE's realize-and-verify seam and its descriptor contract pass their gates. The boot artifacts are qualified in an emulator (QEMU) only and have not booted natively on the DGX Spark. The native memory design for the Spark is a design document, not code.

```text
ATLAS AWAKENS. AIEN PROPOSES. OMEGA DEFINES. FORGE REALIZES. AEGIS VERIFIES. HARDWARE ACTS. EVIDENCE TEACHES.
```

Historical `PHYSICS_*` milestone names (for example `PHYSICS_BOOT`) are unchanged in evidence. The subsystem role was renamed FORGE by ADR 0014 in aien-architecture. The repository itself is still named `physics`: the planned rename to `forge` (aien-architecture `CURRENT_EXECUTION_PLAN.md`, step A2) has not been done.

## Current state

Research-grade and pre-alpha.

- **FORGE:** the realize/verify seam (Gates 3 and 4) and the substrate-neutral descriptor contract (V1 and V2) pass their gates. The AEGIS check refuses a result that was realized for a different request (physics#54, merged 2026-10-13). The analog provider is a simulation checked against a digital oracle; no analog hardware is involved. Status and receipts: [CURRENT_EXECUTION_PLAN.md](https://github.com/aien-dev/aien-architecture-mv/blob/main/CURRENT_EXECUTION_PLAN.md) and [doctrine/ROADMAP.md](https://github.com/aien-dev/aien-architecture-mv/blob/main/doctrine/ROADMAP.md) in aien-architecture.
- **Native GB10 path (`nvrm/`, `m16/`):** since physics#64 (merged 2026-10-18) the client retries the compute-object allocation when the driver briefly reports `NV_ERR_NO_MEMORY`, a bounded number of times (`NVRM_CHANNEL_NO_MEMORY_RETRIES` and `NVRM_CHANNEL_NO_MEMORY_RETRY_MS` in `nvrm/nvrm.h`). This was the cause of the channel-create failures in omega#523. omega does not use it yet: omega's `physics.lock` still pins `6d7cf0d` (2026-10-03), 45 commits behind this main (read 2026-10-19).
- **Atlas boot, PHYSICS boot and PHYSICS effects (Milestones 1 to 3):** qualified in QEMU only (AArch64 virt). Native DGX Spark qualification of PHYSICS (`PHYSICS_BOOT_NATIVE_PASS`) has not been done and is never inferred from an emulator pass. The committed `qualification_receipt.json` is historical evidence from a retired Python harness and is kept unchanged ([TRANSITION_PLAN_M2.md](TRANSITION_PLAN_M2.md)).
- **Native memory design for the Spark:** [docs/native-frame-authority.md](docs/native-frame-authority.md) is a design only, re-checked against receipts on 2026-10-16 (physics#63). Its issue (physics#2) is closed because the design was delivered; nothing is implemented, and `PHYSICS_BOOT_NATIVE_PASS` is still pending.
- **Physics Zero** (a clean-room scientific discovery program, distinct from this repository's PHYSICS) is specified here as documents: the overview and benchmark specs are in `docs/` (`PHYSICS0_DISCOVERY_ENGINE.md`, `PD0_HIDDEN_EQUATION_BENCHMARK.md`, `PD0B_SEALED_BENCHMARK.md`, `PD1_MACHINE_LAW_BENCHMARK.md`) and the sealed-benchmark receipts are in `evidence/PD0B/` (the seal-1 verdict there is FAIL). No code here implements it; the learner and harness behind those receipts are in omega. **DIRAC-0** is documents in aien-architecture only.

## What is where

| Path | What it is |
|---|---|
| `forge/` | FORGE in C: descriptor, realize, the V2 substrate serializer (`forge/v2/`), the realization forms (`forge/form/`) and the simulated analog provider (`forge/analog-sim/`) |
| `nvrm/` | Native GB10 client over the open NVIDIA kernel interface, plus its lifecycle and submission-visibility gates |
| `m15/`, `m16/` | Milestone 15 and 16 code for the native accelerator path (GB10 sysfs access, native submission, the GPU wait primitive) |
| `*.s`, `physics.bin`, `atlas*.bin`, `build.sh`, `m2_build.sh`, `run_m2_gates.sh` | Milestone 2 boot artifacts (AArch64 assembly) and their build and QEMU gates |
| `m3/` | Milestone 3 (PHYSICS_EFFECTS): effect broker, receipt ledger, its build and QEMU gates |
| `third_party/nvidia-open-580.173.02/` | The NVIDIA open driver headers used by `nvrm/`, with their own `COPYING` and `SHA256SUMS` |
| `contracts/` | Draft machine contract for the native DGX Spark (not canonical) |
| `docs/` | Specs and designs (FORGE V2, native frame authority, Physics Zero benchmarks, the test pipeline) |
| `evidence/` | Receipts and raw logs. Add only: old receipts are never edited |
| `tests/` | Gate scripts; `tests/gates/` holds the gate manifests the Spark judge runs |
| `tools/` | Host tools in C and shell (`m2tool`, the QEMU inspector) |

## How this fits with the other repositories

- [omega](https://github.com/aien-dev/omega-mv) pins this repository by commit (`physics.lock`) and uses `nvrm/` and `m16/` for the native GB10 path.
- [aienos](https://github.com/aien-dev/aienux) is the operating system.
- [aien-architecture](https://github.com/aien-dev/aien-architecture-mv) owns status, the ADRs and the crumb tool.
- [aien-sovereign-core](https://github.com/aien-dev/aien-sovereign-core-mv) holds the test runner (`aien-test`) used by the Spark judge.
- [aien-receipts](https://github.com/aien-dev/aien-receipts) stores the signed test results the judge publishes.

`nvrm_channel_n` (2026-10-20, omega L136) opens up to three extra command queues (GPFIFO channels) in the same channel group and address space as the first; `nvrm_channel` is `nvrm_channel_n(rm, 0)`, unchanged. Measured on the GB10: two queues side by side give no speed gain for 27B decode (omega `docs/numeric/GPU_CHANNELS.md`).

## Standing rules

Per ADR 0024 (aien-architecture, accepted 2026-10-01): Rust is scaffolding, Omega is the destination, and C is used only where hardware justifies it, with assembly only where measured. In this repository that is the C of FORGE and `nvrm/`, and the AArch64 boot artifacts. No Python (the old Python harness was removed; tools are C or shell). No CUDA toolkit or CUDA library dependence: the GB10 is driven natively through its open kernel interface. No systemd. Offline builds. Never overwrite old receipts.

## Build and verify

Needs host `gcc` and `make`, the AArch64 binutils and compiler (`aarch64-linux-gnu-gcc`, `-ld` and `-objcopy` by default; override with `CC`, `LD`, `OBJCOPY`, or `CROSS` for the M3 build), QEMU for AArch64 (`qemu-system-aarch64`), `socat` (the QEMU inspector uses QMP) and `jq`.

`tests/gates/run.sh <step>` is the one entry point. It runs the same script as the gate manifest of that name. Some steps need sibling checkouts next to this one: `../omega` for `analog-sim`, `form0` and `gate34-writer`, and `../aien-architecture` for `m3` and `crumb-verify`. They exit 3 with a message if the sibling is missing.

```bash
git clone https://github.com/aien-dev/physics-mv physics
git clone https://github.com/aien-dev/omega-mv omega
git clone https://github.com/aien-dev/aien-architecture-mv aien-architecture
cd physics
tests/gates/run.sh forge-v2           # FORGE V2 known-answer tests
tests/gates/run.sh forge-aegis        # AEGIS verifier binds a result to its request; submit refusals before any GPU call
tests/gates/run.sh analog-sim         # simulated analog provider vs digital oracle
tests/gates/run.sh form0              # FORM0 realization-form gates (simulated results only)
tests/gates/run.sh gate34-writer      # Gate 3/4 receipt writer, no GPU
tests/gates/run.sh gb10-lock          # GPU lock helper, scratch lock file, no GPU
tests/gates/run.sh zero-disk-secrets  # no plaintext .env files in the tree
tests/gates/run.sh m2                 # M2 PHYSICS_BOOT in QEMU, rebuilt from source
tests/gates/run.sh m3                 # M3 PHYSICS_EFFECTS in QEMU, in a temporary copy
make host-tests-m16                   # m16 GPU wait primitive: host test plus 3 mutants that must be caught
```

Last run of these on main `aecb9d3`, on the DGX Spark, 2026-10-19: every one exited 0. The counts each script printed: forge-v2 6 passed, forge-aegis 13 of 13, analog-sim 14, form0 16, gate34-writer 47, gb10-lock 9, m2 15 gates, m3 19 of 19, and `make host-tests-m16` killed 3 of 3 mutants. These runs were not recorded as receipts; the Spark judge's signed results are in aien-receipts.

`./run_m2_gates.sh` leaves the committed receipt untouched unless `--write-receipt` is passed, which is only for a recorded requalification decision.

Gates that need the real GB10 run on the DGX Spark one at a time and are never killed mid-run: `forge-gate34` (`tests/run_forge_gates.sh`), `m15`, `m16-requal`, `nvrm-lifecycle` and `submission-visibility`. They take the GPU lock (`/tmp/aien-gb10.lock`) through `tests/gb10_lock.sh`, which skips it when the caller already holds it (the runner sets `AIEN_GB10_LOCK_HELD=1`, or a parent process holds the lock). A gate that locked again would wait on its own parent forever (physics#70, merged 2026-10-18). `tests/test_gb10_lock.sh` (gate `PHYSICS-GB10-LOCK`, no GPU) checks this.

## How a pull request is tested

Since 2026-10-16 the DGX Spark is the only test judge for pull requests (aien-architecture ADR 0033, phase 3). Its gates are the manifests in `tests/gates/`. On 2026-10-18 the hosted test workflows were removed (physics#69). The one workflow left, `.github/workflows/aien-verification.yml`, does not build or test this code: it checks the crumbs, fetches the Spark's signed result from aien-receipts and verifies it. The verdict of record is the commit status "AIEN Verification (Spark)". [docs/PIPELINE.md](docs/PIPELINE.md) explains how a pull request is tested and how the `ready-to-merge` label leads to a merge.

## Evidence notes

- `evidence/m16-blackwell-native-path-receipt.json` still says `QUALIFIED / PASS`. It is the original M16 receipt (commit `f72e297`) and it is superseded: it failed an independent runtime audit (it linked `libcuda`, reused stale session state, and cited raw evidence that was never committed). Do not cite it as a qualification. Its own requalification record is `evidence/m16-blackwell-native-path-requalification-receipt.json` with the audit in `evidence/m16-requalification/M16_REQUALIFICATION_AUDIT.md` (implementation `a2c0d7f`). Evidence files are add-only, so the original receipt is left unchanged on purpose.
- Even the requalification binds the code of its own time. It is historical evidence, not a claim about the current candidate. Source: `reports/L2-EVID.md` of the 2026-10-05 candidate audit (CAND-0 physics pin `6d7cf0d`), as cited in aien-architecture `doctrine/ROADMAP.md`.

## Contributing

Open a pull request with the command you ran and its output. Do not edit committed receipts or binaries without the matching rebuilt evidence. A pull request merges only after the Spark judge passes it and a maintainer adds the `ready-to-merge` label. Who built what is in [CONTRIBUTORS.md](CONTRIBUTORS.md). Contact: aien@aienos.com.

## Crumbs

This repo uses the Crumb Protocol (RFC-0001). Each directory carries a `.crumb` file (what the directory is for, its rules, and a backfilled history). Live coordination between agents goes in `.crumb.local`, which is never committed.
Before editing a directory, read its `.crumb`. Rules and the `crumb` tool: https://github.com/aien-dev/aien-architecture-mv/blob/main/docs/CRUMB_PROTOCOL.md
Backfilled history (marked as such, not live whispers) is in [docs/crumbs/BACKFILL.md](docs/crumbs/BACKFILL.md).

## License

Apache License 2.0 with LLVM Exceptions, see [LICENSE](LICENSE). The NVIDIA headers in `third_party/nvidia-open-580.173.02/` keep NVIDIA's own license terms (MIT except where that file notes otherwise), see [COPYING](third_party/nvidia-open-580.173.02/COPYING).

## Human-Directed Agentic Engineering

Development follows Human-Directed Agentic Engineering (HDAE): humans define goals,
architecture, constraints, acceptance criteria, and stopping points; agents implement,
execute checks, diagnose failures, and revise within those boundaries. Human review
retains responsibility for claims, acceptance, and release decisions. This describes
current development, not a retrospective claim about historical experiments or receipts.

For the documented method and a systems-software case study, see Gregory M. Kurtzer,
*Ginux: Building an Independent Kernel That Runs Linux Userspace through Human-Directed
Agentic Engineering* (2026), https://doi.org/10.5281/zenodo.23682548.
