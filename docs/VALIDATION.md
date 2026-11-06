# Validation record — v0.5

Validated November 6, 2026 on Linux x86-64. This release adds the React workspace and its journaled C++ protocol to v0.4. Earlier evidence remains in [VALIDATION-v0.4.md](VALIDATION-v0.4.md).

## Executed checks

- **11/11 scalar CTest suites passed.** Includes the new durable workspace and actual HTTP/WebSocket interface tests.
- **12/12 full native llama.cpp/ggml CTest suites passed.** Includes Gemma template rendering, existing optimizer/checkpoint/ABI tests, and the new workspace tests.
- **3/3 JavaScript protocol tests passed.** Correlated acknowledgments, explicit rejection/disconnection without mutation replay, and visible timeout behavior.
- **Vite production build passed.** React 19.3.1 and Vite 8.3.6, JavaScript/JSX components with default exports. Main JavaScript is approximately 285 KB, 87 KB gzipped; CSS approximately 26 KB. Fonts are local assets. The source fingerprint matches the supplied bundle.
- **Full supervisor passed** with `start --ui --upkeep --train music,architecture --exit-after-training`. All 256 benchmark episodes finished. Neither candidate was activated. Repeating with `--resume` added zero benchmark episodes and exited cleanly.
- **Browser checks passed** against the real local C++ demo runtime: create chat, persist reply, save attributed reflection, create bird-form soul with presence/interests, inspect memory graph, train a music candidate, run structured practice against it without activation, inspect live activity, and receive its scheduled quiet contribution in the original chat. The practice repaired the fixture's pitch range and passed all its declared constraints.

Logs and a machine-readable summary are in `examples/verification-v05/`. UI tests are in `ui/src/lib/client.test.js`; integration tests are in `tests/workspace_test.cpp` and `tests/interface_test.py`.

## New behavioral coverage

Workspace integration verifies durable projects/chats/replies, identical-send deduplication, conflicting request rejection, explicit guest scope, original speaker/project attribution, exact response memory references, offline review delivery, the 30-minute quiet gate, chat moves, restart recovery, paginated original provenance records, journal hashes, and no automatic activation.

The interface integration starts the real runtime and loopback gateway, checks same-origin WebSockets, rejects a foreign Origin and path traversal, disconnects a browser client during work, and recovers the durable result from history.

The existing native and upkeep suites continue to verify ABI bounds/checksums, explicit activation boundaries, read-only scope, coordinator writes, output exhaustion with retained partial text, independent optimizer checkpoints, interest isolation and shared upkeep limits.

## Limits of this validation

No large GGUF was supplied. Real conversation with the user's exact model, GPU inference and Windows/macOS execution remain untested. Browser checks used the visibly labeled deterministic foundation; they establish integration, not conversational quality. The music/architecture curriculum scores measure small symbolic constraints, not general expertise.

This is a trusted local single-operator interface. Account/project scopes organize data; they do not provide authenticated remote accounts. Its real records can be inspected and changed through the coordinator, but the graph is a bounded view, not a biological simulation or exhaustive causal inference. See [INTERFACE.md](INTERFACE.md) for behavior and remaining scope limits.

## Reproduce

```sh
python -m pip install -r tools/requirements.txt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
ctest --test-dir build -C Release --output-on-failure
cmake -S . -B build-native -DWDS_LLAMA=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --config Release -j 4
ctest --test-dir build-native -C Release --output-on-failure
npm --prefix ui ci
npm --prefix ui test
npm --prefix ui run build
python tools/workshop.py --config configs/offline.json start --ui --upkeep --train music,architecture --resume --exit-after-training
```

Use a fresh, isolated data/runs configuration for acceptance testing. The native build fetches the pinned llama.cpp source unless an explicit checkout is configured. No model is downloaded automatically.
