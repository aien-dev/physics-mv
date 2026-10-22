# A realization request and its refusal

I separated machine realization from semantic authority so a backend can propose machine code without declaring it safe. The name FORGE and this boundary are recorded in [ADR 0014](https://github.com/aien-dev/aien-architecture-mv/blob/main/docs/adr/0014-rename-machine-physics-to-forge.md); this repository still has its historical `physics` name. The host seam here is implemented, but the sample lowering is deliberately simulated, not a GB10 instruction generator.

The [host fixture](tests/test_forge_aegis_host.c) builds a `ForgeRealizationRequest` with program digest, opcode count, IR bytes and a target [machine descriptor](forge/forge_descriptor.h). [`forge_realize`](forge/forge_realize.c) checks the descriptor digest, pads a copy of IR to an aligned code buffer and computes request, code and realization digests. [`aegis_verify_realization`](forge/forge_realize.c) checks code size, machine limits, descriptor and request binding, then marks the result verified. A result for program A is refused when replayed for program B or a different opcode count. Mutated code, a stale identity and another target descriptor are also refused. The token in this seam uses a public constant and is an integrity marker, not authentication against a caller able to recompute it. The submit-side fixture verifies refusals before NVRM access. This particular seam fixture does not select a physical provider; [the form realization layer](forge/form/forge_form.c) holds that separate choice, and [the analog simulation fixture](tests/test_forge_analog_sim.c) checks its simulated provider against an oracle. The [V2 descriptor specification](docs/FORGE_SUBSTRATE_V2_SPEC.md) explains the serialized machine boundary.

## Replay

Use a fresh checkout at `14273c5ce7d13fbc6e410c4fed9aa36837918177` on a Linux host with gcc and bash. This host test needs no GPU, model, sibling repository or QEMU.

```sh
git clone https://github.com/aien-dev/physics-mv.git physics-lesson
cd physics-lesson
git checkout --detach 14273c5ce7d13fbc6e410c4fed9aa36837918177
tests/gates/run.sh forge-aegis
```

Expected exit is 0 with `REALIZE_OK`, `VERIFY_OWN_REQUEST_ACCEPTED` and `NEG_RESULT_REPLAYED_TO_OTHER_PROGRAM_REFUSED` reported as PASS. See my [raw replay](TEACHING-REPLAY.txt) for the observed output and environment. This is a host seam test. The analog provider is simulation; the older assembly boot receipts are QEMU evidence; neither demonstrates a current physical GB10 execution. Hardware routes remain separate in [README.md](README.md) and [the gate manifests](tests/gates).


### Parent fresh-checkout replay

A separate coordination replay is retained in [the parent receipt](docs/parent-replay-2026-10-22.txt). Its scope and observed exit are explicit; source review and final desk qualification remain separate. A recorded absence or expected failing baseline is not a passing implementation.
