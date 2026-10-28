# Contributors

This project is built by AI models, directed by Drake Stapleton, who wrote none of the code.
Current development follows [Human-Directed Agentic Engineering (HDAE)](README.md#human-directed-agentic-engineering):
the human defines architecture, constraints, acceptance criteria and stopping points, while agents
implement, execute checks, diagnose failures and revise within those boundaries. Human review
retains responsibility for claims, acceptance and release decisions. The README cites Gregory M.
Kurtzer's documented method and case study; it does not reattribute historical work.
Every model below did real work. A model is listed only for work it did. The orchestrator checks all output before it merges.

| Model | What it did |
|---|---|
| Claude Opus 5.5 | M2 requalification (ingress can never widen frame authority), native DGX Spark frame authority design, Milestone 3 effects ledger, effect broker and execution receipts, `nvrm_free` and the uncached allocation, FORGE SUBSTRATE V2 reference serializer, tests and specification, the machine-made Gate 3/4 receipt, and the fixes from review |
| Codex | Code review of the Gate 3/4 receipt writer: one finding (binaries were hashed after the GPU lock was released), fixed |
| Grok | Code review of the Gate 3/4 receipt writer: three findings (empty binary hash on PASS, blobs symlink, existing blob mode), fixed |
| Gemini | Code review of the Gate 3/4 receipt writer (no findings) |
| OpenAI GPT-6.1 Sol | Current HDAE development wording and Gregory M. Kurtzer citation in README and contributor guidance (2026-10-28); no implementation or historical receipt changes |

This table is built from the model credit lines in this repository's commit history and the review notes in its pull requests. Commits without such a line are not attributed here; add rows only for work that can be traced to a model.

Update this table in the same commit as the work it credits.
