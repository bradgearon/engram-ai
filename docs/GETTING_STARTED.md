# Start talking and training — wds.ai v0.5

For the new React interface, follow [INTERFACE.md](INTERFACE.md). After configuring your GGUF, `python tools/workshop.py --config configs/sixstars.local.json start --ui --upkeep` opens the local workspace at http://127.0.0.1:8787. The terminal client remains available below.

This is the complete local setup guide. The release provides a React browser workspace, an interactive **text chat** terminal, a C++ runtime, native Wind training, and optional bounded background upkeep. The Six Stars microphone, VAD, Whisper and Qwen-TTS playback/UI are not connected to wds.ai in this version. Text conversation is ready through `chat.py`; do not expect `launch.cmd` from Six Stars to route its conversations into this new runtime.

## 1. Unzip and install build prerequisites

Extract `wds_ai_rea_v0_5.zip` and open a terminal in its `wds-ai` directory.

On Windows, use a **Developer PowerShell for Visual Studio 2022** with the Desktop development with C++ workload. Install CMake 3.20+, Git, and Python 3.11+ and ensure their commands are on PATH. On Linux, use a C++20 compiler, CMake, Git and Python 3.11+. macOS uses the corresponding compiler tools. Linux was tested here; the Windows instructions and CI build are supplied, but Windows was not executed in this environment.

```sh
python --version
cmake --version
git --version
python -m pip install -r tools/requirements.txt
```

The sole Python package is `websockets`. Training math, inference, attention and the coordinator are native C++. The launcher also bootstraps a local WebSocket-only environment when needed. Installing the requirement explicitly makes the separate chat commands use the same Python environment.

## 2. Point it at your Six Stars model

The provided Six Stars source declares these paths relative to its repository/install folder:

| Role | Declared location | Used by this release |
|---|---|---|
| Chat LLM | `models/llm/gemma-4-E4B-it-Q6_K.gguf` | Yes: existing GGUF loaded directly |
| Vision projector | `models/llm/mmproj-gemma-4-E4B-it-Q8_0.gguf` | No vision adapter in this release |
| Whisper model | `models/asr/ggml-large-v3-turbo.bin` | Not connected yet |
| TTS Talker | `models/tts/qwen-talker-0.6b-base-Q8_0.gguf` | Not connected yet |
| TTS codec | `models/tts/qwen-tokenizer-12hz-Q8_0.gguf` | Not connected yet |

The authoritative file is Six Stars' `config/models.json`. The setup helper reads it rather than assuming the filename will never change. Replace the example folder below with the **actual folder on your computer** containing that `config/models.json`:

```powershell
python tools/configure_sixstars.py --six-stars "C:\Projects\the-six-stars"
```

This verifies the local GGUF and writes `configs/sixstars.local.json`. It defaults to a CPU build to work without CUDA. The GGUF is read in place, not copied or modified. If you moved models, use `--models-root "D:\AI\models"` or the existing `SIX_STARS_MODELS_ROOT` environment variable. `--model "D:\AI\exact-model.gguf"` explicitly selects a different existing GGUF. `--replace` permits replacing a previously generated configuration; otherwise the helper preserves it.

For NVIDIA GPU inference, install the CUDA development toolkit compatible with your compiler and driver, then generate the CUDA configuration:

```powershell
python tools/configure_sixstars.py --six-stars "C:\Projects\the-six-stars" --cuda --gpu-layers 99
```

Add `--replace` if you already ran the CPU command. Source compilation requires the CUDA toolkit, even though Six Stars' prebuilt CUDA distribution may only need a driver. This runtime builds its own pinned llama.cpp library; it does not load Six Stars' `llama-server.exe` or swap its DLLs. The small Wind optimizer remains CPU ggml in this milestone.

If the GGUF is missing, the helper stops explicitly. Run `launch.cmd --download-models-only` from Six Stars, or supply another existing GGUF. This wds.ai helper does not download weights or silently fall back to demo output.

The pinned llama.cpp includes Gemma 4 architecture support. The adapter also uses its native Jinja engine, with thinking disabled and input-token markers preserved. The actual large GGUF was not included in the supplied source archive, so loading/inference with your exact file and GPU must be checked on your machine. Template rendering and native adapter compilation were tested here.

## 3. Start the runtime

```sh
python tools/workshop.py --config configs/sixstars.local.json start --upkeep
```

First launch builds the pinned native components. Unless `llama_source` points to an existing pinned checkout, it fetches llama.cpp source from GitHub. It downloads no GGUF. Later launches reuse the build when its fingerprint matches. Keep the terminal open.

Readiness prints `workshop.ready` and `ws://127.0.0.1:8765`. The printed foundation must identify `llama.cpp/...`, rather than `wds.ai.intelligence.demo/1`, for real model conversation. Startup errors name the log folder. For this generated configuration, logs and training runs are under `sixstars-wds-runs/`; journal data is under `sixstars-wds-data/runtime/`.

`--upkeep` enables persisted background reviews with shared budgets. Omit it to preserve the existing setting; a new store starts with upkeep disabled. No idle review means no timer work. Configured-source fetching runs only when an enabled review requests a declared source.

If Six Stars is already running its own LLM, close it first if you need the memory for this separate model instance. Both applications can point to the same read-only GGUF, but simultaneous processes use separate RAM/VRAM. The model file is not a shared inference session.

## 4. Give the root its expression and start text chat

Open a second terminal in `wds-ai`, using the same Python environment:

```sh
python tools/soul.py send examples/expression/root.json
python tools/soul.py send examples/expression/profile.json
python tools/soul.py send examples/expression/preferences.json
python tools/chat.py --root last-cat --user brad --name Brad
```

The example root speaks as **The Last Cat organization**, with warm, sisterly, curious presence and interests in cats, music and stories. Gender is unspecified. Edit the JSON examples to choose a bird, a quiet rock, a fatherly presence, another organization, or other expressive details. Profiles live at the root and optional individual-soul level. Change `request_id` and use the current `expected_version` for a new edit; replaying the unchanged example is idempotent.

The chat command declares `brad` as a local human conversation identity if needed, with its own speaking interest paused. It records your user turns as attributed astroengram memories. It includes the preceding finished exchange in the next prompt and retrieves relevant scoped memory. This is a bounded conversational context, not an unlimited transcript loaded into every prompt. Original authorship is retained even if the engram layer selects another responder.

Useful commands inside chat:

```text
/profile
/soul auto
/soul auerlis
/reflect The ending felt rushed; leave more space before the final note.
/quit
```

`/soul ID` requires a declared soul in this root; `/soul auto` returns to automatic matching. `/reflect` saves feedback without starting a response. Ordinary text starts a response. Explicit speaker preference guides selection; a sufficiently relevant saved engram idea can still override it, with the reason recorded. Weak cross-soul associations stay saved.

To add a separate bird persona first:

```sh
python tools/relationship.py --root last-cat --soul bird --name Bird --purpose "melody singing" --voice purpose --request-id declare-bird-1
```

Then send a profile JSON file with `type:"speaker.profile"`, `root_id:"last-cat"`, `soul_id:"bird"`, `expected_version:0`, `form:"bird"`, and your chosen `presence`/`interests`. In chat, use `/soul bird`. The deterministic purpose evaluator currently checks lexical overlap; extra relevant voices may make an episode inconclusive or fail their narrow check. Profiles guide the model's expression; they are not separately trained characters yet.

For longer replies:

```sh
python tools/chat.py --root last-cat --user brad --name Brad --tokens 2048
```

Default chat output allowance is 1024 tokens; valid range is 1..4096, within the runtime's 8192-token total context and 8192-byte artifact limits. Your Six Stars 16384 context setting is not imported automatically. A limit produces an explicit failed checkpoint with preserved partial output, never a passing truncated answer. User turns are bounded to 2048 UTF-8 bytes for attributed memory; split longer input into separate turns. If you change roots, the chat and memory scope changes too.

## 5. Start optional Wind training

To start runtime, chat support, upkeep and both learning packages together:

```sh
python tools/workshop.py --config configs/sixstars.local.json start --upkeep --train music,architecture
```

Use this instead of starting a second supervisor on the same port. Stop the existing supervisor first. Chat can run in another terminal during learning. Training uses lower-priority episode work and native numerical optimization; benchmark outcomes do not create upkeep needs or add interactive reinforcement.

For a deterministic demo without any LLM, run:

```sh
python tools/workshop.py --config configs/offline.json start --upkeep --train music,architecture
python tools/chat.py --root last-cat
```

The demo is explicitly labeled and exercises the architecture; it is not a real conversational model. The offline configuration uses the small C++ scalar optimizer. `configs/local.json` instead builds the native ggml optimizer while still using the demo foundation until you configure a GGUF.

The curricula train two small action-selection Winds:

- Music: symbolic note range, duration, cadence, intervals and motif repair.
- Architecture: symbolic component/port/write ownership, idempotency and bounded async design repairs.

They preserve supplied lessons, verify split/source checksums, gather graded trajectories, optimize the existing 16×16 policy, evaluate validation and locked test splits, and write immutable candidates and reports. They do not fine-tune the GGUF. Conversation/reflections update memory and response priority; they do not automatically rewrite active policy weights.

## 6. Review and explicitly activate a candidate

The supervisor prints `workshop.finished`, its report path and candidate path. Open the report. A passing gate never activates anything.

Pause future upkeep and let active work finish before activation:

```sh
python tools/workshop.py --config configs/sixstars.local.json upkeep pause
python tools/workshop.py --config configs/sixstars.local.json upkeep status
python tools/workshop.py --config configs/sixstars.local.json status
python tools/workshop.py --config configs/sixstars.local.json activate --domain music --package packages/candidates/music/RUN_ID
python tools/workshop.py --config configs/sixstars.local.json run --task examples/music-task.json --root music
python tools/workshop.py --config configs/sixstars.local.json upkeep resume
```

Replace `RUN_ID` with the actual printed directory, not a report checksum or a guessed ID. Activate architecture separately with its domain and candidate path. Activation succeeds only at an episode boundary for that domain. Paused episodes still block it: explicitly resume them or cancel them by instance ID:

```sh
python tools/workshop.py --config configs/sixstars.local.json resume --instance wind-ID
python tools/workshop.py --config configs/sixstars.local.json cancel --instance wind-ID
```

Use an actual instance ID from the snapshot. Rollback uses the same explicit activation command with a previous package, including `packages/breeze`. Keep registered package directories unchanged and available with their journal.

## 7. Inspect and guide upkeep

```sh
python tools/soul.py inspect --root last-cat
python tools/soul.py watch --root last-cat
python tools/soul.py reflect --root last-cat --actor brad --text "Revisit the melody's ending next time."
```

Inspect is a one-time request. Watch receives events; do not repeatedly poll inspect. Default limits are two concurrent upkeep reservations, four episodes and two foundation calls per hour, four RNA chunks per material import, and two attempts before rest. The defaults favor quiet, targeted work.

Use `review.suggest` JSON to request an explicit review. Structured failed practice can create a review automatically when upkeep is enabled. Add actual public HTTPS lesson URLs under `upkeep.sources` in the supervisor config if you want the worker to fetch more material. An empty source list means no external fetching. See `UPKEEP.md` and `PROTOCOL.md` for exact fields and deferral/retry behavior.

## 8. Stop, resume and troubleshoot

```sh
python tools/workshop.py --config configs/sixstars.local.json stop
python tools/workshop.py --config configs/sixstars.local.json start --upkeep --train music,architecture --resume
```

`/quit` closes only text chat. `stop` or Ctrl+C in the supervisor stops its processes. `--resume` reuses an interrupted training job only when its input/source/binary fingerprint matches. A changed implementation or curriculum creates a new run. In-flight runtime episodes are paused after restart and need explicit resume/cancel; scheduled reviews remain persisted.

| Symptom | Action |
|---|---|
| GGUF not found | Verify Six Stars folder or pass `--models-root`/`--model`; download through Six Stars if necessary |
| Compiler/CMake/Git absent | Install prerequisites; on Windows use Developer PowerShell |
| CUDA configuration fails | Install the compatible CUDA toolkit, or regenerate the CPU config explicitly |
| `demo/1` foundation | Use `configs/sixstars.local.json`; the default demo is intentional |
| Token/context/artifact budget reached | Read the saved partial result; use a larger permitted `--tokens` allowance or shorter context |
| Review resting/awaiting source/budget | Inspect its reason, change the relevant setting/material, then issue a versioned retry |
| Activation blocked | Finish or explicitly cancel active/paused episodes in that domain |
| Training timeout | Read run checkpoints and rerun with `--resume`; failure is reported explicitly |
| Port in use | Stop the other supervisor, or use another config port and matching chat `--ws` |
| Writer lock after forced exit | Confirm no writer is running; follow `DESIGN.md` recovery instructions before removing a stale lock |
| Missing/changed registered package | Restore the exact candidate directory; do not overwrite its weights in place |

No credentials or model bytes from Six Stars are copied into this project. The adapter uses its own pinned C++ source and the model you explicitly configure. The current service is loopback-only for trusted local clients; the project/account scopes are not remote-user authentication.
