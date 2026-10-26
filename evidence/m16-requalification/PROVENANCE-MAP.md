# M16 historical checksum-path map (2026-10-26)

This is an **additive path interpretation**, not a new qualification, source
rebuild, or correction to the retained receipt. `SHA256SUMS` and `build.log`
remain byte-for-byte historical evidence. Its first eight absolute paths
refer to the old `m17-physics` checkout. The implementation source was pinned
at `a2c0d7fbc03f7ce91fcb6389312a0ee8630322cd`; the receipt first appeared
in the later evidence commit `b64753d`. The mapping below was checked by
hashing `git show <pin>:<relative-path>` for the first seven rows and the
retained receipt bytes for the eighth. No original checkout is required.

| Original `SHA256SUMS` suffix | Repository-relative path | Pin | Recorded SHA-256 |
| --- | --- | --- | --- |
| `m16/m16_native.h` | `m16/m16_native.h` | `a2c0d7f` | `2840151393f7292e2d1b74c4510f813ca5754877e5df40d1d7afca15987f42b8` |
| `m16/m16_native.c` | `m16/m16_native.c` | `a2c0d7f` | `1b00fd6f6a50bce45bcfb7a8f7b621b699f930557b92b6acb6019b0a4fa518d5` |
| `m16/m16_requalify.c` | `m16/m16_requalify.c` | `a2c0d7f` | `af50f8225930565edbce9d1dd668fab235a59f4b779610d63d1e1767051b5320` |
| `m16/m16_concurrent.c` | `m16/m16_concurrent.c` | `a2c0d7f` | `27c532db479abdf372d1400c049a3fb07bd7ae46e213645e781a3c736ead04b8` |
| `nvrm/nvrm.h` | `nvrm/nvrm.h` | `a2c0d7f` | `1885d5daa862e18b76ca68a5613d8c4b115b6915f4bb2d1d4f98cea49e5287fd` |
| `nvrm/nvrm.c` | `nvrm/nvrm.c` | `a2c0d7f` | `6976b1ead456daf421e7fd0dd6fbb344a91248b5a86207ca023e25ec9a2435a3` |
| `tests/run_m16_requalification.sh` | `tests/run_m16_requalification.sh` | `a2c0d7f` | `4684d4886f398f6f05f93eeb206b521d27f2fcecf44ee234c778c7fd760a0a16` |
| `evidence/m16-blackwell-native-path-requalification-receipt.json` | same relative path | `b64753d` | `1a32cc2e04793d9fa86849bb93a51145f5b62fea4a8d70484c6a3fccdcbc3b56` |

To check one source row, from any clone of this repository:

```sh
git show a2c0d7fbc03f7ce91fcb6389312a0ee8630322cd:m16/m16_native.h | sha256sum
```

To check the receipt row, substitute
`b64753d:evidence/m16-blackwell-native-path-requalification-receipt.json`.
The remaining entries in `SHA256SUMS` name files relative to
`evidence/m16-requalification/` and are not remapped. The retained
`build.log` has SHA-256
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`:
it is empty, not a transcript of compiler invocation/version/output. The
historical script contains GCC command lines at
`tests/run_m16_requalification.sh:42-60`, and the receipt records a GCC
version, but neither substitutes for an observed build transcript. This map
establishes portability of the eight checksum references only; it does not
re-exercise hardware or qualify the current source revision.
