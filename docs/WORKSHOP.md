# Wind workshop v0.4

This workshop teaches small action-selection policies from graded practice. The frozen language model remains unchanged. The two supplied lesson collections are preserved under `curricula/*/sources`; their executable additions contain training tasks, validation tasks, locked tests, checksums, explicit budgets and reports.

## Start everything

Requirements: Python 3.11+, CMake 3.20+, a C++20 compiler, and Git for the default pinned ggml source fetch. From the project directory:

```sh
python tools/workshop.py start --upkeep --train music,architecture
```

The launcher builds C++, installs only the WebSocket client if needed, starts the runtime, processes both curricula in order, runs the native trainer, registers immutable candidates, and produces benchmark reports. The runtime remains running afterward. Omit `--train` to start without learning. Use `--exit-after-training` for a bounded batch run. First build needs internet for pinned source; no model is downloaded.

```sh
python tools/workshop.py start
python tools/workshop.py start --train music,architecture --resume
python tools/workshop.py status
python tools/workshop.py stop
```

Run status/stop from a second terminal. These are explicit one-shot WebSocket commands. Progress, readiness, shutdown and episode completion use events; there is no status polling. Ctrl+C shuts down supervised processes. The runtime drains in-flight compute; forced termination after the supervisor timeout can leave a writer lock requiring the documented recovery procedure.

`--resume` requires the same curriculum, parent, implementation source, trainer binary and run settings. It resumes collected trajectories, native epoch checkpoints and completed benchmark episodes. Rebuilding/changing inputs creates a new fingerprint and a separate run. Changing the active policy never changes an existing episode's pinned package.

## Explicit activation

Training never activates a candidate. A passing gate only records evidence. A rejected candidate stays available for inspection; it is not recommended by the gate. After reviewing `runs/<domain>/<run-id>/report.json`, use the exact package path printed by `workshop.finished`:

```sh
python tools/workshop.py activate --domain music --package packages/candidates/music/RUN_ID
python tools/workshop.py run --task examples/music-task.json --root music
```

Activation is a separate compare-and-swap journal event. It requires every episode in that domain to be completed, exhausted, failed or cancelled, and no work still in flight. Paused episodes block activation: resume them to completion or explicitly cancel them through the protocol. The CLI has no `--auto-activate` or activation-on-pass setting. Rollback uses the same explicit command with a previously registered package, including `packages/breeze`.

## What happens during learning

1. Verify SHA256 pins of lesson sources and splits; reject overlapping defect-combination families. Import the extracted lessons as hypothetical source-linked memory, with stable IDs.
2. Ask the C++ evaluator to score each possible repair on training and validation trajectories. Its greedy one-step improvement selects the imitation target. No language-model judgment defines numeric labels.
3. Train the 16×16 policy plus 16 biases with native ggml SGD/cross-entropy. A small C++ scalar SGD fallback is available. Parameters remain finite and bounded to ±4. Validation loss selects an epoch; locked-test feedback never enters optimization.
4. Emit a checksummed `policy.wind` and manifest in a new immutable candidate directory. Load them through the production loader before publishing. Registering a candidate does not make it active.
5. Run candidate, parent, fixed-order refinement and unchanged-input references over validation and locked tests. Parent/candidate/fixed-order each have at most four actions. Record per-task traces and final constraint scores. A gate requires candidate improvement over both parent and fixed-order, with zero execution errors, on both splits.
6. Save the report and journal its evidence. Stop learning at the configured time/call/task budget. Activation remains a distinct operator step.

This is intentionally a small, inspectable controller. The same C++ check definitions supply training observations and evaluation scores; the holdout changes defect combinations and artifact parameters, not the evaluator. These results demonstrate bounded repair selection, not independent proof of musical artistry or software expertise. The greedy teacher can miss repairs that require an initially neutral or harmful intermediate action.

## Curriculum contents

| Domain | Actions | Split sizes |
|---|---|---|
| Music | Pitch range, duration, home return, positional interval, divergence, motif repetition, stop | 48 train / 12 validation / 20 locked test |
| Architecture | Ports, write ownership, event idempotency, call bounds, async boundary, supervision, stop | 48 train / 12 validation / 20 locked test |

Music tasks are small note/duration objects, not rendered audio. Architecture tasks are declared component graphs, not executed distributed systems. Original Resone source files and its resonator contract were referenced in the supplied lessons but were not supplied. No claims about those missing originals or universal interval/emotion mappings are inferred. The source material is retained with provenance.

Each policy gets a domain, action schema and feature schema in manifest metadata. The disk ABI stays unchanged: schema 1, 1152 bytes, existing 64-byte alignment, 16×16 f32 matrix plus 16 biases, FNV1a64 checksum, existing maturity enum. Candidate IDs include the immutable training-job fingerprint, so separate runs cannot collide merely because their final numeric weights match. Exported candidates remain `seeded`; gate results do not mutate package identity. `growing` remains a recognized runtime value. No unsupported maturity is introduced.

## Configuration and optional models

For an offline scalar build, run `python tools/workshop.py --config configs/offline.json start --train music,architecture`.

Edit `configs/local.json`, or copy it and pass `--config PATH` before the command. Default: ggml trainer, demo foundation, no teacher, one runtime process plus bounded native trainer jobs. A configured local teacher model adds a second runtime with a separate data directory. Python only supervises processes, files and WebSocket exchanges; all numerical training is C++.

- `ggml: false` selects the small C++ numerical fallback and avoids fetching ggml. Plain CMake also defaults to this offline build.
- `llama_source` may point to a checkout of pinned revision `b1c902f026d32535b5e8eae55e5e47b379e57672`. `build_jobs` bounds compilation concurrency.
- `model` supplies a real local GGUF; `teacher_model` optionally supplies a separate GGUF and `teacher_port`. Models are never downloaded automatically. `teacher_ws` alternatively targets an already running compatible teacher.
- `max_teacher_calls` bounds optional critique requests. Teacher output is marked unverified and excluded from numerical labels; held-out inputs are never sent to it.
- `max_foundation_calls` optionally runs a real model as a separate one-shot artifact reference. Demo output is never labeled as an LLM benchmark. This reference is not a matched-compute gate.
- `run_timeout_seconds`, curriculum epochs, task counts and step limits bound work. Nothing schedules indefinite background retraining.
- `parents` can map a domain to an existing package path for a later learning generation; changing it changes the run identity.

The default ggml trainer uses a CPU backend for this tiny policy. This milestone does not establish GPU training, fine-tune GGUF models, or implement an expanding neural language model. The runtime uses C++20 `Task<T>` coroutines and bounded standard-library executors; standalone Asio handles sockets. There is no Boost dependency.

## Recovery and integrity

Native epoch checkpoints resume the same immutable job. Candidate publishing uses staging plus rename. The runtime chain remains authoritative for episodes, relationships and activation. Keep registered candidate directories with the data directory: startup rejects missing or changed registered packages. Do not move or overwrite a registered package; publish a new version.

Application checkpoints protect normal interruption, not arbitrary disk failure. Runtime journal flush/sync protections and stale-lock/torn-tail recovery are described in `DESIGN.md`. FNV1a64 is accidental-corruption detection, not a cryptographic signature; the curriculum additionally pins source/split SHA256 hashes. Loopback WebSocket access remains trusted-local only.


## Six Stars models and interactive chat

See [GETTING_STARTED.md](GETTING_STARTED.md). `tools/configure_sixstars.py` reads the supplied Six Stars `config/models.json`, resolves the actual local LLM GGUF, checks that it exists, and creates a separate wds.ai configuration. It reuses model bytes, not `llama-server.exe` or its DLL ABI. `cuda:true` builds the pinned native adapter with CUDA and requires the CUDA development toolkit; CPU setup needs no CUDA. The trainer remains a small CPU ggml loop.

`python tools/chat.py --root last-cat --name Brad` opens interactive text conversation on the local WebSocket. It records attributed user turns and follows the preceding exchange without importing other project conversations. Use `--tokens 2048` for a larger explicit generation allowance. `/reflect` saves feedback; `/soul ID` changes speaker preference; `/soul auto` restores automatic routing. No microphone, TTS or Six Stars UI bridge is included in this milestone.

`workshop.py upkeep pause` stops future upkeep scheduling while current work completes; `upkeep resume` enables it. `upkeep status` is a one-shot snapshot. `cancel --instance ID` and `resume --instance ID` handle paused episodes explicitly. Use the same `--config` for all supervisor commands.
