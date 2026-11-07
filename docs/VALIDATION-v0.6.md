# v0.6 validation

Recorded results are in `examples/verification-v06/RESULTS.json`; complete CTest logs and summaries are beside it.

| Check | Result |
|---|---|
| Debug scalar backend CTest | 14/14 passed |
| Release llama.cpp/GGML CPU CTest | 16/16 passed |
| Production React bundle | Built successfully |
| Existing JavaScript transport unit tests, before the user's request to skip UI testing | 3/3 passed |
| Browser layout/interaction checks | Skipped at the user's request |
| Live Wikipedia REST lookup | Three attributed excerpts received |

The mesh tests verify local decisions with no model generation, bounded connected traversal, cycle suppression, root isolation, same-root representation reuse, zero-knowledge-budget holds, explicit next wake, restart recovery, feedback replay and web provenance. The WebSocket fixture verifies a real server timer/worker flow and that recording a chat question cannot answer that same question. Queued chat cancellation releases pending state. Existing workspace tests cover guest attribution, retained offline follow-ups and the quiet contribution gate; new neuronal follow-ups use the mesh path.

The native representation test builds a tiny **random-weight Qwen3 GGUF** and executes the real pinned model graph, with final-token pooled hidden state, normalized output, root-scoped caching, cleared context and explicit token-overflow rejection. It samples no answer tokens. This validates native integration, not pretrained knowledge or task accuracy.

The feedback integration test records 90 explicit synthetic labels in the runtime, exports them through WebSocket, groups identical/connected observations, trains through the shipped native supervisor, evaluates held-out rows, registers the candidate and confirms no active package changed. The numerical fixture has 54/18/18 train/validation/test rows across three simple classes. Its 18/18 held-out accuracy and acceptance are only a controlled regression result. They are not a claim of general ability. Repeating the same data reuses the immutable candidate and report. A fresh runtime accepts the 1,152-byte Wind and executes it after a separate explicit activation command.

The live provider check used `orbital resonance`. Source URLs are recorded in RESULTS.json. End-to-end deterministic HTTP fixtures keep automated tests independent of changing web content.

No real Qwen3 4B GGUF was available for a quality benchmark. CUDA, Windows and macOS execution were not run here. Multi-host inference and early-exit transformer computation are not implemented. The README and startup guide distinguish the neuronal decision path from explicitly selected conventional AI-host generation.

Reproduce:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
python -m pip install -r tools/requirements.txt
ctest --test-dir build -C Release --output-on-failure
```

For native integration, configure with `-DWDS_LLAMA=ON` and optionally `-DWDS_LLAMA_SOURCE=/path/to/pinned/source`. The tiny model fixture test requires NumPy and upstream gguf-py **for test data construction only**; neither is an application/trainer dependency. Without NumPy, CMake omits that fixture test and the reported test count will be lower.
