# PHYSICS (FORGE machine realization, the native GB10 client and the Atlas/PHYSICS boot artifacts)

> FORGE (C, `forge/`) turns a verified Omega meaning into a concrete realization on the machine at hand: the CPU, the NVIDIA GB10 GPU, and a simulated analog provider for testing. `nvrm/` is the small C client that drives the GB10 directly through the open NVIDIA kernel interface, with no CUDA. The root `*.s` files and `m3/` are the historical Atlas and PHYSICS boot artifacts (AArch64 assembly), qualified in QEMU only. Research-grade, pre-alpha. The repository is still named `physics`; the subsystem role is FORGE.

Shared rules: read https://www.aienos.com/agents/RULES.md first. This file adds what is specific to this repository; where the two differ, this file wins here.

## Scope

- Applies to the whole repository.
- `forge/`: FORGE descriptor, realize, the V2 substrate serializer (`forge/v2/`), realization forms (`forge/form/`), the simulated analog provider (`forge/analog-sim/`).
- `nvrm/`: native GB10 client and its lifecycle and submission-visibility gates. `m15/`, `m16/`: Milestone 15 and 16 native accelerator code (sysfs access, native submission, the GPU wait primitive).
- Root `*.s`, `physics.bin`, `atlas*.bin`, `build.sh`, `m2_build.sh`, `run_m2_gates.sh`: Milestone 2 boot artifacts and their QEMU gates. `m3/`: Milestone 3 (PHYSICS_EFFECTS).
- `third_party/nvidia-open-580.173.02/`: vendored NVIDIA open driver headers with their own `COPYING` and `SHA256SUMS`. Do not edit them.
- `evidence/`: receipts and raw logs, add only. `tests/gates/`: the gate manifests the DGX Spark judge runs.
- The [omega](https://github.com/aien-dev/omega-mv) repository pins this one by commit in its `physics.lock` and uses `nvrm/` and `m16/`.

## Installation

Needs host `gcc` and `make`, the AArch64 cross binutils and compiler (`aarch64-linux-gnu-gcc`, `-ld`, `-objcopy`; override with `CC`, `LD`, `OBJCOPY`, or `CROSS` for the M3 build), `qemu-system-aarch64`, `socat` and `jq`. There is no Python and no CUDA toolkit.

Some gates need sibling checkouts next to this one, under these exact directory names:

```bash
git clone https://github.com/aien-dev/physics-mv physics
git clone https://github.com/aien-dev/omega-mv omega
git clone https://github.com/aien-dev/aien-architecture-mv aien-architecture
cd physics
```

`../omega` is needed by `analog-sim`, `form0` and `gate34-writer`; `../aien-architecture` by `m3` and `crumb-verify`. A step whose sibling is missing exits 3 with a message.

## Configuration

- `crumb.lock`: the aien-architecture commit the crumb tool is built from for `crumb-verify`.
- `OMEGA_DIR`: omega checkout used by the omega-dependent gates (default `../omega`).
- `nvrm/nvrm.h`: `NVRM_CHANNEL_NO_MEMORY_RETRIES` and `NVRM_CHANNEL_NO_MEMORY_RETRY_MS` bound the compute-object allocation retry.
- GB10 gates take the GPU lock through `tests/gb10_lock.sh`. `AIEN_GB10_LOCK_HELD=1` (set by the Spark runner) tells the helper the caller already holds it.
- `.aien/always` lists the gates every pull request must pass; `.aien/verify.pub` is the Spark judge's public key.

## Usage

`tests/gates/run.sh <step>` is the one entry point; each step runs the same script as the gate manifest of that name in `tests/gates/`.

Host-only steps (no GPU):

```bash
tests/gates/run.sh forge-v2           # FORGE V2 known-answer tests
tests/gates/run.sh forge-aegis        # AEGIS binds a result to its request; submit refused before any GPU call
tests/gates/run.sh analog-sim         # simulated analog provider vs digital oracle (needs ../omega)
tests/gates/run.sh form0              # FORM0 realization-form gates (needs ../omega)
tests/gates/run.sh gate34-writer      # Gate 3/4 receipt writer (needs ../omega)
tests/gates/run.sh gb10-lock          # GPU lock helper on a scratch lock file
tests/gates/run.sh zero-disk-secrets  # no plaintext .env files in the tree
tests/gates/run.sh m2                 # M2 PHYSICS_BOOT in QEMU, rebuilt from source
tests/gates/run.sh m3                 # M3 PHYSICS_EFFECTS in QEMU, in a temporary copy (needs ../aien-architecture)
tests/gates/run.sh crumb-verify       # crumbs match the compiler pinned in crumb.lock (needs ../aien-architecture)
make host-tests-m16                   # m16 GPU wait primitive: host test plus 3 mutants that must be caught
```

Steps that need the real GB10 on the DGX Spark: `forge-gate34` (`tests/run_forge_gates.sh`), `m15`, `m16-requal`, `nvrm-lifecycle`, `submission-visibility`. Run them only on the Spark, one at a time, and never kill one mid-run. They take the GPU lock (`/tmp/aien-gb10.lock`) through `tests/gb10_lock.sh`. Do not run them on any other machine.

## Conventions

- Language rule (ADR 0024): C only where hardware justifies it (FORGE and `nvrm/`), assembly only where measured, Omega is the destination. No Python; tools are C or shell. No CUDA toolkit or CUDA library dependence. No systemd. Offline builds.
- `.crumbignore` keeps the crumb compiler out of `nvrm/` and `m16/`; those directories are frozen by the unchanged-since-base check in `tests/test_forge_seam.c`.
- An emulator pass is never a hardware claim. `PHYSICS_BOOT_NATIVE_PASS` has not been earned; do not infer it from QEMU.
- Historical `PHYSICS_*` milestone names stay unchanged in evidence.
- Record who built what in `CONTRIBUTORS.md`, in the same commit as the work.

## Safety and approvals

- Never edit or delete a committed receipt in `evidence/`. `evidence/m16-blackwell-native-path-receipt.json` is superseded and must not be cited as a qualification; leave it unchanged.
- Do not edit committed binaries (`physics.bin`, `atlas*.bin`, tracked `nvrm/` binaries) without the matching rebuilt evidence.
- GPU gates run only on the DGX Spark, through the lock helper. A gate that took the lock again under a parent that holds it would wait forever; `tests/test_gb10_lock.sh` checks the helper skips it.
- A head that changes `.aien/`, `tests/gates/`, `.github/workflows/`, `.forgejo/` or `tools/aien-test/` is `BLOCKED_OPERATOR` and needs an operator run on the Spark. Ask before touching those paths.
- The DGX Spark is the only test judge. The one workflow, `.github/workflows/aien-verification.yml`, builds nothing: it checks crumbs and verifies the Spark's signed bundle. `main` requires the `AIEN Verification` check. A pull request merges only after the Spark judge passes it and a maintainer adds the `ready-to-merge` label (see `docs/PIPELINE.md`). Do not add that label yourself unless a maintainer asked you to.

## Further reading

- `README.md`: current state, layout and the build and verify commands.
- `docs/PIPELINE.md`: how a pull request is tested on the Spark and merged.
- `docs/FORGE_SUBSTRATE_V2_SPEC.md`, `docs/native-frame-authority.md`: FORGE V2 and the native memory design (design only).
- `TRANSITION_PLAN_M2.md`: why the historical `qualification_receipt.json` is kept unchanged.
- `CONTRIBUTORS.md`: who built what.
- Crumb Protocol: https://github.com/aien-dev/aien-architecture-mv/blob/main/docs/CRUMB_PROTOCOL.md
