# v0.7 validation

Recorded logs and results are in `examples/verification-v07/`.

| Check | Result |
|---|---|
| Debug scalar backend | 16 CTest tests passed |
| Release native llama.cpp / GGML CPU backend | 18 CTest tests passed |
| Production React bundle | Built successfully |
| Browser and JavaScript UI tests | Not run, as requested |

`engram_assembly` verifies that connected neurons contribute before speaking; scope isolation; evidence deduplication; actual native sentences; corrected knowledge on the next reply; unchanged historical evidence; explicit exclusive conflicts; deadline/cancellation; feedback idempotency; restart recovery; bounded importance in numeric attention; and Arc dataset ownership with payload-free indexing and lazy handler fetch. Its test foundation throws on any attempted prose generation.

`expression_workflow` builds a native expression candidate from synthetic labels, evaluates held-out rows, checks the unchanged 1,152-byte policy ABI, confirms registration does not activate it, rejects activation during a collecting assembly and then activates explicitly. It runs the shipped CLI against the real WebSocket server to teach, ask, attach a completed music Wind result, correct a relationship, set importance and inspect exact weight references. It simulates a crash after assembly completion but before chat delivery by replaying a valid journal prefix; two restarts each show exactly one delivered reply. Feedback and export retain the sixteen feature lanes.

The synthetic three-class plan classification result is only an optimizer/ABI regression, not a language-understanding benchmark. Source contract behavior is tested separately. The existing neural feedback test now exercises `trainer_backend: auto`, including real export, grouped splits, candidate reuse and no automatic activation.

Existing tests also cover native training, task repair, bounded upkeep, scope and speaker provenance, offline contributions, WebSocket lifecycle, Six Stars setup and event-driven web-worker integration. The interface WebSocket check tests the backend relay, not a browser.

The native Qwen fixture uses a tiny **random-weight** Qwen3 GGUF and executes the pinned model graph. It checks representation normalization, caching, scope, reset and overflow rejection with zero answer tokens. It does not measure Qwen3 4B knowledge. No pretrained 4B model or GPU was available. Windows/macOS, CUDA throughput, general natural-language understanding and distributed inference were not validated.

Reproduce the scalar suite:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug -j 3
python -m pip install -r tools/requirements.txt
ctest --test-dir build -C Debug --output-on-failure
```

For native integration add `-DWDS_LLAMA=ON -DWDS_LLAMA_SOURCE=/path/to/pinned/source` with commit `b1c902f026d32535b5e8eae55e5e47b379e57672`. NumPy and upstream gguf-py are only test-fixture construction dependencies, not application or optimizer dependencies. Without NumPy the tiny Qwen test is omitted.
