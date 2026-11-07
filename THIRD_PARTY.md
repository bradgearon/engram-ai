# Dependencies

Vendored headers retain upstream notices; corresponding licenses are in `third_party/`.

| Dependency | Version / commit | License | Use |
|---|---|---|---|
| Asio | 1.30.2 / 12e0ce9e0500bf0f247dbd1ae894272656456079 | Boost Software License 1.0 | portable event-driven I/O |
| WebSocket++ | 0.8.2 / 56123c87598f8b1dd471be83ca841ceae07f95ba | BSD-style, see COPYING | RFC 6455 framing/transport |
| nlohmann/json | 3.11.3 / 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03 | MIT | bounded JSON protocol and journal |
| llama.cpp | b1c902f026d32535b5e8eae55e5e47b379e57672 | MIT | optional frozen model inference / ggml backends |
| Python websockets | 16.0 | BSD | example/learning clients and integration tests |

Pinned llama.cpp source (or its ggml subtree) is fetched at configuration only when either model inference or ggml training is enabled, or supplied as a local checkout. GGUF weights are separately supplied by the operator and have their own licensing requirements. The ChristEye reference archive is not redistributed in this project.

Project-specific code is authored for We Develop Software LLC. No third-party license is applied to that code by this delivery; the owner can choose a project license before publication.

Standalone Asio uses the Boost Software License; it does not require or link the Boost libraries. The runtime uses C++20 coroutines and standard-library executors. No Python ML framework is included.

The optional v0.4 model adapter also builds the pinned llama.cpp native Jinja engine (`common/jinja`), its Unicode helpers and JSON wrapper. These source files are obtained from the same optional llama.cpp checkout under its MIT license; the project does not vendor a second Python template engine or the full common HTTP/CLI stack. The pin and build target are in CMakeLists.txt.

## v0.7.1 runtime-pack adapter

`third_party/llama-b9870/` contains the exact public C headers and MIT license from llama.cpp **b9870**, commit `2d973636e292ee6f75fadcf08d29cb33511f509f` (GGML 0.15.3). Only declarations are compiled into WDS; the operator supplies the native shared libraries. The pack must remain a complete matching release. See its `PROVENANCE.json`.

`third_party/llama-template/` vendors the native Jinja/Unicode/JSON helper subset from the existing source pin `b1c902f026d32535b5e8eae55e5e47b379e57672`, with its MIT license. The JSON wrapper assertion is adapted to throw a C++ exception, avoiding a dependency on a second compiled GGML library. `tests/fixtures/templates/google-gemma-4-31B-it.jinja` comes from that same pin's `models/templates/` directory and is used only for template regression. No model weights, CUDA binaries or upstream HTTP server are redistributed.
