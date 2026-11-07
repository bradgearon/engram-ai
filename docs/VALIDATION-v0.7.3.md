# v0.7.3 validation

## Results

| Check | Result |
| --- | --- |
| Native runtime-pack build, Debug, Linux | 20/20 CTests passed |
| Source llama.cpp build, Release, Linux | 20/20 CTests passed |
| React production bundle | Built successfully as v0.7.3 |
| Supplied journal replay | All 83 original events retained byte-for-byte; repairs appended |
| Completed host reply | Original persisted output restored exactly, without regeneration |
| Other two blocked conversations | Both released and completed using the deterministic test foundation |
| UI browser/interaction tests | Not run, as requested |
| Windows MSVC / user's CUDA GPU | Not available in this environment; requires local confirmation |

The supplied original runtime log already demonstrates that the user's Qwen model loaded and generated on CUDA. The failure addressed here is after generation, in journal application and completion delivery. This does not constitute a new GPU benchmark or a Windows execution test of this release.

## New regression coverage

`tests/chat_recovery_test.cpp` verifies host and neural follow-up queues; completion order; cancellation; generation-limit partial output retention; explicit coordinator failure; queue overflow rejected before durable changes; record-list validation before append; replay of synthetic singleton-object records; recovery after completed work but missing chat delivery; stale reservations; queued-message restart; idempotent recovery; and unchanged history prefixes. No automatic package activation occurs.

`tests/diagnostics_test.py` checks native request/error/neural events, explicit trace rotation and retention markers, same-origin browser telemetry ingestion, credential-field redaction, metadata-only mode, excluded model/runtime files, export hashes, preserved model/pack configuration, and the actual launcher process/trace path. It does not launch or test the React UI.

Existing suites cover chain and RNA behavior, evaluator/voice contracts, upkeep budgets, WebSocket protocols, training/export/activation boundaries, response assembly, Six Stars setup, prompt templates, and the native ABI fixture. Source-mode tests include the synthetic Qwen representation fixture; runtime-pack tests include split library export ownership.

`examples/verification-v073/` contains test summaries and production-build output. Uploaded private logs are not included in this release. The replay summary contains counts only. Windows users should start with `--trace`, confirm the footer shows v0.7.3, and export diagnostics if another failure occurs.
