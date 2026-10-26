# Security policy

## Reporting a vulnerability

Report privately. Do not open a public issue, pull request or discussion about it.

1. GitHub: open this repository's Security tab and choose "Report a vulnerability". This
   creates a private advisory that only the maintainers can see. If that option is not shown,
   private vulnerability reporting has not been enabled for this repository yet; use email.
2. Email: drake@aienos.com with the subject `[physics-mv] security report`.

Include the commit or release you tested, the component (path), steps to reproduce, and what an
attacker gains. If the report involves a secret, say where it was exposed and do not send the
secret itself. Encrypted mail is not required; do not post proof-of-concept payloads publicly.

## Response targets

This project has one maintainer; the targets below are best effort and stated honestly.

- Acknowledgement: within 3 working days.
- Triage and severity: within 7 days of acknowledgement.
- Fix or mitigation for confirmed issues: aim for 30 days for high and critical, 90 days
  otherwise, with a status update at least every 14 days until closed.
- Disclosure: coordinated. An advisory is published with credit (unless you ask otherwise)
  once the fix is on the default branch. Default embargo 90 days, negotiable.

## Scope

In scope:

- `nvrm/`: the native GB10 client over the open NVIDIA kernel interface (ioctl argument
  handling, memory mapping, channel and object lifetime, retry bounds).
- `m15/`, `m16/`: GB10 sysfs access, native submission and the GPU wait primitive.
- `forge/`: descriptor parsing, the V2 substrate serializer, and the AEGIS check that binds a
  realized result to its request.
- `m3/` effect broker and receipt ledger; the boot artifacts (`*.s`, `physics.bin`, `atlas*.bin`)
  as they run in QEMU.
- `tests/gates/run.sh`, `tests/gb10_lock.sh`, `.aien/`, `.github/workflows/`.

Out of scope: the NVIDIA kernel driver and the vendored headers in
`third_party/nvidia-open-580.173.02/` (report upstream); the Spark test judge `aien-test` (report
to aien-sovereign-core-mv). FORGE is realization, not a security gatekeeper (README).

## Supported versions

Only the default branch `main`; there is no release. Fixes land on the default branch only.

## Status

Research grade and pre-alpha; boot artifacts are qualified in QEMU only, never natively (README).
There is no certification and no bug bounty. See AUDIT_SCOPE.md for what to review and how to
build and test it.
