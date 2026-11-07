# v0.8.0 validation — 2026-11-06

## Completed

- Linux runtime-pack adapter build: 23/23 CTest tests passed, including split-library b9870 ABI fixtures. No CUDA compilation was requested.
- Linux native llama.cpp source/CPU build: 23/23 CTest tests passed, including the synthetic Qwen representation fixture and template checks.
- New native cognition tests: zero generative calls, held-out entity/property transfer, newly discovered prerequisite, contradictory feedback, explicit missing-state uncertainty, simulation without self-reinforcement, bounded recurrence/eligibility, cross-root isolation, restart and crash exactly between cognitive commit and mesh completion.
- Shared-budget test: a second language sample stays deferred after one reserved encoding, then resumes at the next budget deadline. No polling or unrestricted parallel encoding.
- Native Language Wind training: unchanged 1152-byte ABI, bounded native SGD, three-class held-out synthetic feature evaluation, model identity rejection, and unchanged candidate bytes after evaluation/failed training. Synthetic test accuracy and coverage were 1.0; these values are only fixture checks, not measurements of Qwen's language quality.
- WebSocket workflow: repeated host and neuronal messages, learning/prediction for a held-out entity, one background general-knowledge fetch, attributed source arrival in a later chat contribution, no normal prose generation, unchanged activation and restart persistence.
- Existing tests retain coverage for queue recovery, failed and cancelled work, partial diagnostic generation, single-writer/heart behavior, scope, RNA/expression, training, configuration and diagnostics. Legacy generative benchmark tests now request `foundation_once` explicitly; normal chat tests exercise native behavior.
- React production bundle built successfully with the existing JavaScript/JSX stack. No browser/UI interaction tests were performed, as requested.
- Python orchestration files compile; no ML framework was added.

## Not established by these checks

- The user's actual Qwen3-4B-Q4_K_M model and Windows CUDA DLLs were not available for a live GPU run here. Existing pack loading behavior is preserved; DLL/driver installation and real model throughput still need that machine.
- Web acquisition tests inject a deterministic source response. They verify scheduling, provenance, persistence and delivery, not live Wikipedia uptime or correctness.
- The Language Wind's shipped curriculum has not been scored on the user's real GGUF. Training writes its actual held-out report on that machine and never activates automatically.
- The operation learner covers finite scalar-state, conjunctive tasks. It does not demonstrate open-ended reasoning, broad natural-language understanding, arbitrary weight-address knowledge retrieval, biological equivalence or superiority to an LLM.
- Predicate activation is inspectable and learned locally; typed preconditions/conflicts still gate action execution. The tests do not claim that recurrence alone produced the transfer result.
- Recognized instructions bypass generic memory scanning. General archive retrieval retains the existing scoped index implementation; it is not a newly benchmarked approximate-nearest-neighbor index.

This release is a runnable, bounded research milestone with explicit limitations. Reproduce backend checks with `ctest --test-dir BUILD_DIR --output-on-failure`. See [QUICKSTART-v0.8.md](QUICKSTART-v0.8.md) for real-model language training and the teaching demonstration.
