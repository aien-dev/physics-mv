# Audit scope

Written for an outside, independent security audit. Revision described:
`8e92aa629c945d9574952b49ac4a2b04ef52ab06` on `main`, 2026-10-26. Everything below is taken from
the repository's own files at that revision; items marked NOT_RUN were not executed while writing
this document.

## What this repository is

Two things live here. FORGE (C, `forge/`) turns a verified Omega meaning into a realization on the
CPU, the NVIDIA GB10 GPU, or a simulated analog provider. `nvrm/` is a small C client that drives
the GB10 directly through the open NVIDIA kernel interface, with no CUDA. The root `*.s` files,
`physics.bin`, `atlas*.bin` and `m3/` are the historical Atlas and PHYSICS boot artifacts
(AArch64 assembly), qualified in QEMU only. Status (README, 2026-10-19): research grade,
pre-alpha. FORGE is realization and lowering, not a security gatekeeper; AEGIS verifies.

## What to review, in priority order

1. `nvrm/nvrm.c`, `nvrm/nvrm.h`: ioctl construction and result handling, memory mapping,
   channel creation (`nvrm_channel_n`), the bounded allocation retry
   (`NVRM_CHANNEL_NO_MEMORY_RETRIES`, `NVRM_CHANNEL_NO_MEMORY_RETRY_MS`). Runs with access to the
   GPU device nodes.
2. `m15/`, `m16/`: sysfs access, native submission, the GPU wait primitive.
3. `forge/` and `forge/v2/`: descriptor parsing and serialization; the AEGIS check that refuses a
   result realized for a different request (`tests/run_forge_aegis_host_gates.sh`).
4. `m3/`: effect broker and receipt ledger.
5. Boot artifacts: `physics.s`, `vector_table.s`, `capability.s`, `memory_alloc.s`, `atlas_m2.s`,
   `physics.ld`, `build.sh`, `m2_build.sh`, and the committed `physics.bin` and `atlas.bin`
   against `physics.sha256` and `atlas.sha256`.
6. `tests/gates/run.sh`, `tests/gb10_lock.sh`, `run_m2_gates.sh` (`--write-receipt` rewrites the
   committed receipt), `tools/`.
7. `.github/workflows/aien-verification.yml`: pull request and push, `contents: read`, advisory;
   on `alpha/*` tags verifies the Spark's signed bundle. Builds nothing from this repository.
8. `.aien/always`, `.aien/unmapped-ok`, `.aien/verify.pub`.

AGENTS.md: a head that changes `.aien/`, `tests/gates/`, `.github/workflows/`, `.forgejo/` or
`tools/aien-test/` is `BLOCKED_OPERATOR` and needs an operator run on the Spark.

## Trust boundaries

- Untrusted: pull request heads (run by the Spark judge in its sandbox); descriptors and requests
  passed into FORGE.
- Trusted: the NVIDIA kernel driver and its device interface; the vendored headers (with their
  `SHA256SUMS`); sibling checkouts of omega and aien-architecture; `.aien/verify.pub` on `main`.
- The DGX Spark is the only test judge; nothing on GitHub gates a merge (AGENTS.md,
  `docs/PIPELINE.md`).

## Threat model summary

- Assets: GPU and host memory reachable through the driver interface; correctness of realized
  results; receipt integrity.
- Attackers considered: a malicious pull request; a mismatched or substituted realization result;
  malformed descriptors.
- Defended: AEGIS binding of result to request; submit refusals before any GPU call; GPU lock
  serializing hardware gates; `nvrm/` and `m16/` frozen by the unchanged-since-base check in
  `tests/test_forge_seam.c`; zero-disk-secrets gate.
- Not defended / explicitly out of scope: native boot on the DGX Spark
  (`PHYSICS_BOOT_NATIVE_PASS` not earned); the native frame authority design is not implemented;
  the analog provider is a simulation.

## Build from source at a pin

Host `gcc` and `make`, `aarch64-linux-gnu-gcc`, `-ld`, `-objcopy`, `qemu-system-aarch64`,
`socat`, `jq`. Sibling checkouts: `../omega` for `analog-sim`, `form0`, `gate34-writer`;
`../aien-architecture` for `m3` and `crumb-verify` (crumb tool at `crumb.lock`
`12ad0141b41ec67ef5aa51cb4e1815894c1083bb`).

```sh
git clone https://github.com/aien-dev/physics-mv physics
git clone https://github.com/aien-dev/omega-mv omega
git clone https://github.com/aien-dev/aien-architecture-mv aien-architecture
cd physics
git checkout 8e92aa629c945d9574952b49ac4a2b04ef52ab06
```

## Run every test

```sh
tests/gates/run.sh forge-v2
tests/gates/run.sh forge-aegis
tests/gates/run.sh analog-sim
tests/gates/run.sh form0
tests/gates/run.sh gate34-writer
tests/gates/run.sh gb10-lock
tests/gates/run.sh zero-disk-secrets
tests/gates/run.sh m2
tests/gates/run.sh m3
tests/gates/run.sh crumb-verify
make host-tests-m16
```

- README, main `aecb9d3`, DGX Spark, 2026-10-19 (not recorded as receipts): forge-v2 6 passed,
  forge-aegis 13 of 13, analog-sim 14, form0 16, gate34-writer 47, gb10-lock 9, m2 15 gates, m3 19
  of 19, `make host-tests-m16` killed 3 of 3 mutants.
- GB10 gates `forge-gate34`, `m15`, `m16-requal`, `nvrm-lifecycle`, `submission-visibility` need
  the real GB10 on the DGX Spark: NOT_RUN. All commands above: NOT_RUN while writing this document.

## Known limitations and NOT_RUN

- Boot artifacts qualified in QEMU only; native boot NOT_RUN.
- `qualification_receipt.json` is historical evidence from a retired Python test driver, kept
  unchanged.
- `evidence/m16-blackwell-native-path-receipt.json` is superseded and must not be cited as a
  qualification (AGENTS.md).
- `aien-verification.yml` sets `REPO_SLUG: aien-dev/physics` while this repository is
  `aien-dev/physics-mv`. GitHub redirects the old name (HTTP 301 to `aien-dev/physics-mv`,
  observed 2026-10-31), but the slug is not fetched: it is passed as
  `aien-test verify-bundle --repo github.com/aien-dev/physics`, so a tagged release verifies only
  if the Spark judge signs its bundle under that name. Not exercised: no `alpha/*` tag was run.
- omega pins an older commit of this repository in its `physics.lock` (README).

## Dependencies and provenance

- NVIDIA open driver headers 580.173.02, vendored with `COPYING` and `SHA256SUMS`.
- Toolchain: gcc, make, AArch64 cross binutils, QEMU, socat, jq; versions not pinned in the
  repository. No Python, no CUDA.
- Committed binaries: `physics.bin`, `atlas.bin`, `atlas_m2.bin`, `nvrm/lifecycle_gates`.
- Licence: Apache-2.0 (`LICENSE`, `NOTICE`).

## Evidence

- `evidence/`: receipts and raw logs, additions only (`AR2`, `FORM0`, `M19R`, `PD0B`,
  `m15-boundary`, `m16-requalification` and JSON receipts).
- Signed Spark bundles: `aien-dev/aien-receipts` at `physics/<sha>/bundle.json` (workflow path).
  Receipts are append-only and never edited.
