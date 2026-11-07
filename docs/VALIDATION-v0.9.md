# v0.9 validation

The source-backend and existing-runtime-pack configurations build on Linux. The runtime-pack suite uses synthetic b9870-compatible shared libraries. The native Qwen graph test uses a synthetic miniature Qwen fixture. Neither is a validation of the user's actual Windows CUDA DLLs or real Qwen3-4B quality/latency.

The production JSX bundle builds. No browser interaction/UI tests were run, as requested.

## Native English training run

- 960 training, 240 validation and 240 locked-test conversations.
- Eight epochs, deterministic shuffling, 64 recurrent hidden units, 256 byte symbols, 32-step truncated backpropagation, native CPU SGD.
- Best epoch selected only by validation loss.
- Initial validation bits/byte: 8.0002857.
- Best validation bits/byte: 2.7576155.
- Locked-test bits/byte: 2.9880986; next-byte accuracy: 55.04%.
- Held-out generated final-turn exact match: **0/240**.
- Responses accepted at the default student probability threshold: **0/240**.

The generated examples are visibly inadequate for ordinary conversation. The included checkpoint is labeled experimental. This is evidence that parameter optimization and the learning trace work; it is not evidence of broad language understanding. Teacher assistance is the currently useful conversational route.

## Regression coverage

The new native sequence test checks loss reduction, complete tensor loading, corrupted-tensor rejection, policy ABI preservation, immutable checkpoint retry, numerical-only decoding, explicit generation bounds, streamed teacher attribution, prior-turn context, failure/partial-output recovery, teacher budgets, strict mode, neural-channel delivery, lesson replay, explicit review and root isolation.

The English workflow test launches a real WebSocket runtime, trains through the native executable, registers a candidate, verifies the active package is unchanged, explicitly activates it, and checks reviewed-example import and held-out input rejection.

The existing tests cover the chain, heart, scoped memory, mesh scheduling, assembly, chat recovery, diagnostics, task training, configuration, native representation and runtime-pack ABI. Final checks on this source:

- Native llama/CPU configuration: **25/25 CTest tests passed**.
- Existing-runtime-pack configuration: **25/25 CTest tests passed** (synthetic compatible libraries).
- React production bundle: built successfully; no browser interaction tests.
- The compatibility regression fixtures explicitly select legacy mode. The new workflow verifies that the server defaults to strict mode. The sequence regression exercises streamed host/neural conversations and continuation after partial failures.

Commands used: `cmake --build build-llama -j 4`, `ctest --test-dir build-llama --output-on-failure -j 4`, the equivalent `build-pack` commands, and `npm run build` in `ui`.
