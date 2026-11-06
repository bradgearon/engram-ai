# wds.ai — Rooted Understanding

**Project milestone 0.5.0.** A small C++20 implementation of the REA v0.2 hybrid loop for We Develop Software LLC.

Start the React workspace with `python tools/workshop.py --config configs/sixstars.local.json start --ui --upkeep`. See [the v0.5 interface guide](docs/INTERFACE.md) for setup, conversations, soul inspection, training and live activity. The source uses JavaScript/JSX, not TypeScript. [Release changes and upgrade notes](docs/RELEASE-v0.5.md).

**Seed → relationship root → attend → move → propose → heart → refine → checkpoint.**

The foundation is a frozen llama.cpp GGUF model. External memory changes independently through bounded, source-linked RNA packets. Small Wind policies are separate aligned artifacts. The default build uses an explicitly labeled deterministic demo so the core works without downloading model weights.

Read [the complete talking and training setup guide](docs/GETTING_STARTED.md), including reuse of your Six Stars GGUF. Interactive text chat is included; microphone/TTS integration is not yet connected.

## Start the Wind workshop

```sh
python tools/workshop.py start --upkeep --train music,architecture
```

This builds the C++ runtime and native ggml trainer, starts the WebSocket service and configured-source worker, enables bounded upkeep, runs both learning packages, and writes candidate policies and reports. Omit `--train` to start without policy training; omit `--upkeep` to preserve the current upkeep setting (disabled in a fresh store); add `--resume` to continue the same checkpointed run. The only Python package is the WebSocket client; Python handles orchestration, not numerical training. The default launcher fetches pinned ggml source on first build but downloads no model. Supply CMake 3.20+, a C++20 compiler, Git and Python 3.11+.

**Training never changes the active package.** Review the printed report, then separately activate at an episode boundary:

```sh
python tools/workshop.py activate --domain music --package packages/candidates/music/RUN_ID
python tools/workshop.py run --task examples/music-task.json --root music
python tools/workshop.py stop
```

The two curricula teach bounded symbolic repair choices: music note constraints and architecture graph constraints. They do not establish general expertise. Candidates retain the existing checksummed `policy.wind` ABI and maturity values. See [the workshop guide](docs/WORKSHOP.md) for configuration, gates, checkpoints, teacher models and explicit activation.

## Root expression and selective upkeep

Roots carry intimate presentation details: form, presence, interests and optional gender/pronouns. A root or declared speaker may represent a person, bird, rock or organization such as The Last Cat. Engram evidence has final speaker selection; other useful ideas can stay saved for later. Project/account scoping keeps memories and preferences aligned, with minimal cross-soul noise.

Attributed feedback becomes an astroengram available to the next attempt. A completed response can consume its bounded attention credit. Repeated equivalent requests reuse their result; due reviews share a budget and reach explicit rest or deferral states. See [the v0.4 guide](docs/UPKEEP.md) for commands, examples, bounds and the meaning of each software role.

```sh
python tools/workshop.py start --upkeep
python tools/soul.py inspect
```

## Build the offline demo directly

Requirements: CMake 3.20+, C++20 compiler, Python 3.11+ for clients/tests. Networking/JSON headers are included. No downloads during the default C++ build.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
python -m pip install -r tools/requirements.txt
ctest --test-dir build -C Release --output-on-failure
./build/wds-ai --package packages/breeze --data data
```

Windows with Visual Studio uses `build\Release\wds-ai.exe`; the other arguments are the same. Linux is the verified platform in this delivery. Windows/macOS builds are provided in CI but have not been run here.

In another terminal:

```sh
python tools/client.py --root music "Plan a short composition using recurring motifs."
```

Only WebSocket client messages are accepted. There are no status polling endpoints, reconnect loops, heartbeat polling, or periodic background inference. Enabled upkeep uses a single one-shot timer for concrete due reviews. URL ingestion makes one explicitly requested HTTPS document fetch; all interaction with the runtime is WebSocket.

The first seed creates `Lumen of music`, a level-1 relationship root with the declared purpose. Completed episodes advance the displayed shared-practice level. This is an experience counter, not an intelligence, consciousness, scientific-validity, or spiritual-status measurement. Interests accumulate from explicit seed words and remain bounded. See the relationship contract in `docs/DESIGN.md`.

## Declare a relationship and consult its voice

After creating the `music` root with the seed above:

```sh
python tools/relationship.py --root music --soul auerlis --name Auerlis --purpose "music composition" --voice purpose --request-id declare-auerlis-1
python tools/relationship.py --root music --inspect
```

Stop the server gracefully, restart it with the same `--data data`, then submit:

```sh
python tools/client.py --root music --voice-budget 2 "Develop music composition with a recurring motif."
```

The final `evaluation.contributions` includes a structural constraint check and Auerlis's attributed purpose check. Both record identity, version, scope, evidence, and feedback. `represented_souls` lists the heart's identities; `considered_souls` lists the ones actually consulted. Exact declaration retries are idempotent across restart; changed declarations use a new request ID and the current `--expected-version`.

The default composite heart requires all selected checks to pass. A budget of one omits a relevant declared voice and returns `inconclusive`; disagreement remains visible. Purpose alignment currently means **lexical overlap only**, so this is a measurable architecture baseline rather than a learned character or music-quality evaluator. To compare the original structural baseline, use `--evaluator wds.ai.heart.rules/1`.

HeartScope is read-only; voices propose changes and Engine owns durable writes. Declaration starts bounded interest, while URL imports and ordinary name mentions stay source material. No background training is started by a declaration. See [protocol](docs/PROTOCOL.md) and [component design](docs/DESIGN.md) for updating, pausing and pending bindings.

## Use a real frozen foundation

```sh
cmake -S . -B build-llama -DWDS_LLAMA=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-llama --config Release -j 4
./build-llama/wds-ai --package packages/breeze --data model-data --model /path/to/model.gguf --gpu-layers 0
```

This downloads pinned llama.cpp source, not a model. Supply a licensed decoder-only GGUF with a llama.cpp-supported chat template. Unknown templates and oversized contexts fail explicitly. Use separate data directories when switching foundations: resume checks the model content identity.

For a local checkout, add `-DWDS_LLAMA_SOURCE=/path/to/llama.cpp`. The tested revision is `b1c902f026d32535b5e8eae55e5e47b379e57672`; APIs are pinned deliberately. CUDA: add `-DGGML_CUDA=ON` and choose `--gpu-layers 99`; Metal/Vulkan use the corresponding llama.cpp CMake options and installed SDKs. The compact REA attention/policy kernels currently run on CPU; alignment prepares them for a later backend without implying measured GPU acceleration.

## Teach from a URL

Keep the server running, then:

```sh
python tools/learn.py --root music --url https://example.com/lesson --base packages/breeze --output packages/music-candidate-2
```

Use an actual HTML/plain-text lesson URL. Or use `--text lessons.txt` for a local lesson. The client fetches one bounded public document, imports source-linked hypothetical memories, and records provenance over WebSocket. It does not produce policy weights. Domain policy learning now uses the native workshop and executable practice tasks above; the former Python reconstruction surrogate is retired.

## Learn from another live LLM

Start a second wds-ai server backed by a real GGUF, on a different port and data directory:

```sh
./build-llama/wds-ai --model /path/to/teacher.gguf --port 8766 --data teacher-data --package packages/breeze
python tools/teacher_bridge.py --teacher ws://127.0.0.1:8766 --student ws://127.0.0.1:8765 --root music --topic "motif development with counterexamples" --base packages/breeze --output packages/teacher-candidate-2
```

One invocation starts one bounded lesson in the background relative to the student. Omit `--topic` to select the root's highest-count learning interest via a single WebSocket request. Teacher output carries the model identity and trace ID and enters as hypothetical memory. Lessons update memory only; use the native workshop for policy learning. A third-party LLM needs a WebSocket adapter implementing the documented teacher contract. This bridge uses model outputs; it cannot inspect another model's hidden weights or establish that its claims are true.

Enabled upkeep can revisit failed practice or explicitly scheduled needs within a shared budget. Configured-source requests use the same WebSocket chain. No passing gate or learning process activates a candidate.

## Aligned Wind and RNA artifacts

```sh
python tools/pack_wind.py --input lightweight-weights.json --output packages/custom-v2 --version 2
python tools/export_rna.py data/events.jsonl memory.rna
```

`policy.wind` has a 64-byte header followed by contiguous little-endian float32 matrix and bias tensors, each at 64-byte boundaries. `memory.rna` has a 64-byte header and fixed 256-byte memory tiles plus a provenance index. The runtime also uses `alignas(64)` policy/tile types. Alignment does not itself make these GPU allocations or tensor-core kernels. See `docs/FORMATS.md`.

## Project map

| Path | Purpose |
|---|---|
| `include/wds/ai/` | Types, relationships, evaluation, mesh, heart and runtime contracts |
| `src/core.cpp` | Roots, memory ingestion, attention, chain and episode coordinator |
| `src/relationships.cpp` | Read-only heart snapshots and atomic declaration preparation |
| `src/mesh.cpp` | Memory index adapter, voice dispatch and bounded deterministic voices |
| `src/heart.cpp` | Evaluator registry, structural baseline and composite aggregation |
| `src/winds.cpp` | Versioned package loader and movement kernel |
| `src/intelligence.cpp` | Demo and frozen llama.cpp adapters |
| `src/server.cpp` | Standalone Asio transport and C++20 coroutine scheduling |
| `include/wds/ai/task.hpp` | Move-only Task and bounded standard-library executors |
| `src/upkeep.cpp` | Selective signals, persistent reviews, flesh and shared budget |
| `src/scopes.cpp` | Root scopes, expression profiles, attributed reflections and deferred speaker ideas |
| `src/tasks.cpp` | Native music/architecture actions, observations and checks |
| `src/trainer.cpp` | ggml SGD or small C++ scalar optimizer; existing Wind ABI |
| `curricula/` | Original lessons, executable splits, hashes and evaluation scope |
| `tools/workshop.py` | One-command launch, optional learning and separate activation |
| `tools/` | Client, URL learning, teacher bridge, package and RNA conversion |
| `packages/breeze/` | Executable seeded policy, editable weight source, manifest |
| `docs/DESIGN.md` | Full component design, limits and future interfaces |
| `docs/PROTOCOL.md` | Wire examples, events and error behavior |
| `docs/FORMATS.md` | Artifact layouts and parameter separation |
| `docs/VALIDATION.md` | Actual checks and unresolved limits |

This prototype binds to loopback and rejects browser Origin headers. It is intended for local, trusted clients. Add authentication, WSS, quotas, and per-root authorization before remote or multi-user deployment. Root scoping currently prevents accidental mixing; it is not user authorization.
