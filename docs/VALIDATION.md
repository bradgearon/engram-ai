# Validation record — v0.4

Delivery: November 5, 2026. Linux x86-64, GCC 14.2, CMake 4.4.4, Python 3.12 and WebSocket client 16.0. Native source pin: llama.cpp `b1c902f026d32535b5e8eae55e5e47b379e57672`.

## Executed checks

All **9 scalar-build suites** and **10 full native llama.cpp/ggml-build suites** passed. The full native build uses static libraries and the selected native Jinja files, not the complete common HTTP/CLI stack. Logs and reports are in `examples/verification-v04/`. The earlier `examples/verification/` and `VALIDATION-v0.3.md` remain historical evidence.

| Suite | Checked behavior |
|---|---|
| core, heart, heart_websocket, websocket | Existing journal integrity, versioned memory, ABI, scope, declaration replay, consultation provenance, disagreement and budgets |
| workshop_core | Explicit activation at episode boundaries; cancellation during blocked inference; late output cannot overwrite cancellation |
| native_trainer | Native scalar/ggml optimization, deterministic checkpoint resume, numeric bounds, immutable output and package loading |
| upkeep_core | Duplicate/queued response reuse, irrelevant evidence hold, scoped account reads, original feedback authorship, context invalidation after feedback/identity changes, profile/preferences, engram override/defer, bounded reinforcement, partial-output failure, concurrency/budget reservation, atomic RNA rejection and restart |
| upkeep_websocket | Real timer across process restart; no idle journal writes; configured-source worker with a local fetch fixture; disabled upkeep; two-turn text chat and feedback; prior-exchange linkage; reflection/material RNA export |
| sixstars_setup | Authoritative manifest path resolution, relocated model root, CPU/CUDA config selection and explicit missing-file failure; fixture headers are not real models |
| gemma4_template (native build) | Render the pinned Gemma 4 template, produce the non-thinking generation suffix, preserve input markers against special-token reinterpretation |

The whole supervisor ran both curricula with upkeep enabled, using the CPU ggml trainer. All 256 benchmark episodes completed with **zero automatically suggested reviews, zero flesh mutations and zero activation events**. Restarting with `--resume` added **zero benchmark episodes**. The resulting candidate artifacts were each loaded by the actual original v0.2 executable: both remain **1152 bytes**, with the existing manifest, checksum, numerical bounds and maturity enum.

## Observable upkeep acceptance

A separately isolated fixture explicitly activated the candidate before practice. The initial music exercise exhausted its one-step allowance at a declared-constraint score of **1/3**. One later scheduled review completed its remaining repairs, scored **1.0**, and reached `satisfied`; thirst/kindling decreased. The review used zero foundation calls. Only the explicit test operator activated a package.

For ten repeated equivalent requests with unchanged evidence/context, the selective path performed **one** episode evaluation and reused exactly the same artifact **nine** times. Ten direct seeds performed ten episode evaluations. This is a bounded synthetic count comparison, not an LLM latency, energy or broad quality result. Changed attributed feedback and renamed speaker context were separately tested to invalidate reuse.

## Curriculum results

| Domain | Validation full passes | Locked-test full passes | Locked-test mean constraint score |
|---|---:|---:|---:|
| Music | 12/12 | 18/20 | 0.975 |
| Architecture | 12/12 | 20/20 | 1.000 |

Both declared gates passed. These are small symbolic repair tasks, not independent demonstrations of musical quality or production architecture expertise. The two music failures remain in the report and were not used to tune the candidate or change the locked split. The supplied curricula's source/split hashes remained unchanged.

## Practical limits

- Your Six Stars source declares the Gemma 4 GGUF location, but its actual large GGUF was not present in the archive. No real-model inference, GPU loading, audio generation or microphone path was tested here. Native compilation and template rendering do not establish model quality or throughput.
- Windows/macOS CI configuration is included; only Linux was executed. CUDA is a source-build option, not a tested binary included in this ZIP. Building CUDA requires its development toolkit.
- This release includes text chat. Six Stars' Whisper/Qwen-TTS/UI and vision projector are not connected. Model file reuse does not reuse an already-running model process.
- One compute worker serializes inference. Upkeep concurrency limits reservations; weighted queue service does not preempt a long decode.
- The memory index scans keys within granted roots and fetches only selected payloads. It is not a distributed or approximate-nearest-neighbor index. Learning-history recall is bounded; exact full artifacts remain in checkpoints.
- Ledger and message capacities reject or defer work explicitly. Broad evidence fan-out reports linked reviews left unchanged. No background compaction/archival daemon is implemented.
- The software roles do not simulate biological mechanisms or expand the GGUF's neurons. Reflection changes scoped memory and response priority. Only the separate native trainer emits candidate Wind weights, and only explicit activation changes the active package.
- Model output exhaustion preserves partial text and fails visibly. The test covers the typed limit path and malformed trailing UTF-8 through a fake foundation; real GGUF limits were not exercised without model bytes.
- FNV1a64 is corruption detection, not a signature. Root scoping is a trusted-local data boundary, not authenticated multi-user access. Existing crash-recovery limits remain documented in DESIGN.md.

## Reproduce

```sh
python -m pip install -r tools/requirements.txt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j 4
ctest --test-dir build -C Release --output-on-failure
cmake -S . -B build-native -DWDS_LLAMA=ON -DBUILD_SHARED_LIBS=OFF -DCMAKE_BUILD_TYPE=Release
cmake --build build-native --config Release -j 4
ctest --test-dir build-native -C Release --output-on-failure
python tools/workshop.py start --upkeep --train music,architecture --exit-after-training
python tools/verify_upkeep.py --binary build/wds-ai
```

The optional native build downloads the pinned source unless `WDS_LLAMA_SOURCE` points to that checkout. The included sample candidates remain unactivated. See GETTING_STARTED.md for actual GGUF setup and the separate activation command.
