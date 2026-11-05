# Validation record — v0.3

Delivery: 2026-11-05. Linux x86-64, GCC 14.2, CMake 4.4.4, Python 3.12, WebSocket client 16.0. Source/model backend pin: llama.cpp `b1c902f026d32535b5e8eae55e5e47b379e57672`.

## Executed checks

All six CTest suites passed in three C++20 configurations: offline scalar, ggml-only trainer, and full llama.cpp adapter. Six CTest suites cover the core, composite heart, real WebSocket transport, coroutine controls, native training and explicit activation. The scalar and ggml optimizer paths are exercised; the ggml optimizer uses CPU SGD.

| Suite | Evidence |
|---|---|
| `core` | ABI/checksum, duplicate memory rejection, scoped retrieval, attention, propose/refine, pause/resume, policy/route separation, journal replay and torn-tail rejection |
| `heart` | Atomic declarations, expected versions, idempotency, cross-root isolation, immutable snapshots, disagreement, consultation budgets, pending/paused voices, injected voice failures, proposed-only interests and restart persistence |
| `heart_websocket` | Declare, restart the OS process, retry declaration, inspect scope, consult the attributed voice, exhaust its budget, exclude another root |
| `websocket` | Streamed episodes, versioned memory replacement, malformed messages, source import/teacher fixture, URL guards, aligned RNA export and replay |
| `workshop_core` | Native repair actions, invalid artifacts, scoped neuron impulse, candidate registration without activation, boundary/CAS/idempotency checks, pinned episodes, and cancellation while inference blocks; stale results cannot overwrite cancellation |
| `native_trainer` | Native SGD, saved-epoch resume with identical final bytes, immutable retry, ABI/bounds, invalid input rejection, ggml/scalar parameter agreement within 0.002, task-evaluator attribution, explicit activation and replay |

The full launcher was run for both curricula, then restarted with `--resume`. Completed tasks and training epochs were reused; no new benchmark episodes were created by the resume. Python clients compile successfully. The launcher CLI start/status/explicit-activate/run/stop/restart path also passed in an isolated store. Generated candidates were loaded by the **original v0.2 executable**, not merely by the modified loader.

## Curriculum results

Each domain has 48 training tasks, 12 validation tasks and 20 locked tests, split by disjoint authored defect combinations. Each parent/candidate/fixed-order episode has the same maximum four-action budget. An unchanged-input reference is reported separately. The demo foundation was explicitly identified; no real LLM baseline was claimed.

| Domain / split | Candidate full passes | Parent full passes | Fixed-order full passes | Candidate mean constraint score |
|---|---:|---:|---:|---:|
| Music validation | 12/12 | 0/12 | 8/12 | 1.000 |
| Music locked test | 18/20 | 0/20 | 4/20 | 0.975 |
| Architecture validation | 12/12 | 0/12 | 6/12 | 1.000 |
| Architecture locked test | 20/20 | 0/20 | 4/20 | 1.000 |

Both policies passed the declared improvement gates. Neither was activated by training. Music's two exhausted locked cases (`music-test-17`, `music-test-18`) still failed motif repetition after the action budget. Their feedback was not used to retune the model or task definitions. The original scalar run and the ggml run produced the same aggregate pass counts.

These are small symbolic fixtures authored alongside the action/evaluator implementation. Their holdout measures new combinations within that grammar, not independent real-world generalization. The fixed-order baseline is deliberately simple. The same check definitions supply observations and grades, so the experiment demonstrates learned repair selection under known constraints, not discovery of domain semantics. Per-task outcomes, implementation hashes and split pins are included in `examples/verification/`.

## Limits

- No real GGUF inference, teacher quality or GPU offload was tested. Building a llama.cpp adapter does not establish model quality or throughput.
- No GPU policy training, tensor-core speedup, distributed learning, new language-model neurons, or biological mechanisms are demonstrated.
- Music checks do not establish emotion, artistic quality, audio output or Resone compatibility; the referenced original Resone source material was not supplied.
- Architecture graph checks do not establish production latency, resilience or universally good design. They operate on declared flags and symbolic artifacts.
- Mid-inference control is logical cancellation: compute may finish, but generation checks discard its result. There is one serialized compute executor, not parallel model contexts.
- Windows/macOS CI configurations are included but were not executed here. Disk crash recovery, signed packages, remote authorization and multi-tenant isolation remain outside this local prototype.
- FNV1a64 detects accidental corruption, not adversarial tampering. Candidate checkpoints support ordinary interruption; arbitrary power-loss atomicity is not promised.

## Reproduce

```sh
python -m pip install -r tools/requirements.txt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
ctest --test-dir build -C Release --output-on-failure
cmake -S . -B build-ggml -DWDS_GGML_TRAIN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-ggml --config Release -j 4
ctest --test-dir build-ggml -C Release --output-on-failure
python tools/workshop.py start --train music,architecture --exit-after-training
python tools/workshop.py start --train music,architecture --resume --exit-after-training
```

Supply `-DWDS_LLAMA_SOURCE=/path/to/pinned/llama.cpp` to reuse source. The default launcher enables ggml; use a copied config with `ggml:false` for a fully local scalar build. The launcher has no automatic activation option. See `WORKSHOP.md` for the separate activation command.
