# Start speaking and training — v0.7.3

For the stuck-chat fix, automatic journal recovery, and a shareable full trace bundle, see [Diagnostics and recovery](DIAGNOSTICS.md). Start your existing configuration with `start --ui --trace`.

This release adds persistent response assembly, direct teaching/correction and native grounded expression to the neuronal mesh and React workspace. Arc stores both engram and astroengram records, with separate location indexing and payload handling. Qwen3 4B is optional for semantic representations. In v0.8, AI host also uses native knowledge operations; see [the current quickstart](QUICKSTART-v0.8.md).

## 1. Extract and prepare

Extract `wds_ai_rea_v0_7_2.zip` and open a terminal in `wds-ai`.

On Windows use Developer PowerShell for Visual Studio 2022 with the Desktop development with C++ workload. Install CMake 3.20+, Git, Python 3.11+ and Node.js 22.12+ (a recent LTS version is suitable). Linux needs a C++20 compiler; macOS needs its corresponding compiler tools. Reusing the native runtime pack does not need the CUDA development toolkit or Git. Building CUDA from source needs CMake 3.24+ and a CUDA toolkit compatible with your compiler and GPU driver.

```sh
python -m pip install -r tools/requirements.txt
```

The only Python application dependency is `websockets`. Numerical learning and representations run in C++. No Boost or full ML framework is introduced. The launcher installs the locked frontend dependencies and builds the UI. The archive also contains the built UI assets.

## 2. Start without a model first, if desired

```sh
python tools/workshop.py --config configs/offline.json start --ui
```

Open **http://127.0.0.1:8787**. This starts the service and all supporting workers. The mesh listens immediately and rests when no relevant work is pending. It uses explicit bootstrap rules until you train/activate a neural Wind. No GGUF is needed for this local path.

For optional web evidence:

```sh
python tools/workshop.py --config configs/offline.json start --ui --web
```

Stop the earlier process before starting another on the same ports. Ctrl+C shuts down the supervised processes. Alternatively, `python tools/workshop.py --config configs/offline.json stop` requests graceful shutdown.

## 3. Use your Qwen3 4B GGUF and Six Stars layout

The Six Stars files are `config/models.json` and `config/runtime-manifest.json`. They are inputs to setup; the WDS settings file is `configs/sixstars.local.json`.

**Existing installation: keep your selected GGUF and data.** After extracting the update over the application files, run this from `U:\wds-ai` in Developer PowerShell:

```powershell
python tools/workshop.py --config configs/sixstars.local.json start --runtime-pack "U:\six-stars-solution-2\runtime-packs\win-x64\llama-cpp-dynamic-win-x64" --ui
```

This saves the pack selection in your existing config and uses a fresh `build-native-pack` directory. It retains your model, data, run, package, GPU-layer and port settings. It does not touch the failed `build-sixstars` directory. Your prior `--cuda` setup used 99 GPU layers; that setting is retained. Later starts need only:

```powershell
python tools/workshop.py --config configs/sixstars.local.json start --ui
```

Open **http://127.0.0.1:8787**. `--web` adds bounded web evidence; `--train music,architecture` starts the curriculum process. Activation remains a separate action.

**New setup:** explicitly select your actual Qwen3 4B file if the Six Stars default manifest selects a different model:

```powershell
python tools/workshop.py start --six-stars "U:\six-stars-solution-2" --model "U:\path-to-your-models\Qwen3-4B-Q4_K_M.gguf" --runtime-pack "U:\six-stars-solution-2\runtime-packs\win-x64\llama-cpp-dynamic-win-x64" --gpu-layers 99 --ui
```

The adapter targets the **b9870** archive shown in your screenshot. It loads `llama.dll`, `ggml-base.dll`, `ggml.dll` and the matching backend DLLs directly. CUDA kernels come from the existing pack; `nvcc`, a new CUDA build and `llama-server.exe` are not used. Keep the complete pack, including CUDA dependency DLLs, together. The GGUF is read in place. A mismatched ABI, absent GPU or missing library produces an explicit startup error; the launcher does not fall back to compiling CUDA. See [RUNTIME_PACKS.md](RUNTIME_PACKS.md) for compatibility and source-build alternatives.

Use `--gpu-layers 0` during new setup for explicit CPU operation. With an existing config, edit its `gpu_layers` value deliberately. In pack mode `cuda: false` means **do not compile CUDA**; offloading is still controlled by `gpu_layers` and the loaded GPU backend.

Inspect **Activity → Knowledge backend**. Qwen ready means the architecture adapter is available, not that task accuracy has been calibrated. Avoid a second model instance in Six Stars if you need its RAM/VRAM for WDS.

The optional representation path performs a compatible Qwen forward pass and compares hidden states with examples. It produces no autoregressive answer tokens. It does not recover arbitrary facts by directly indexing weights. CUDA speed and your actual 4B model must be evaluated on your machine.

## 4. Start a conversation

Choose a project, start **New conversation**, and leave **Neuronal response** selected. The current soul and Wind appear in the conversation controls. A newly selected or created root gets a small neuron; existing roots from older releases get one when used in a neuronal chat. Declaring a non-paused soul creates its scoped neuron atomically.

User messages become attributed reflections. Related neurons contribute to an engram assembly before one sourced response is delivered. Taught relationships become sentences; other sources are quoted. Use **Activity → Engram responses** to teach a relationship, correct its version, set bounded attention importance and inspect the response chain. A free-form correction remains a reflection; use the Teach form to revise a structured relationship. An unresolved question remains held or requests web evidence if enabled. AI host and Neuronal response both avoid model prose generation in v0.8. Type `help` to see native language operations.

Use **Soul status** for engrams, astroengrams, speaker profiles and provenance. Forms and expression can represent people, creatures, objects or organizations. Cross-project speech requires an invited source and keeps its origin visible. Project/account scopes and the existing engram speaker arbitration remain active.

For an immediately runnable teaching example:

```sh
python tools/engram.py --root demo demo
python tools/engram.py --root demo ask "What is The Last Cat?"
```

See [ENGRAM_RESPONSES.md](ENGRAM_RESPONSES.md) for correction, importance, Wind creation attachments and separate expression training/activation. The default renderer is bounded native realization, not general language generation.

## 5. Teach neurons and build Winds

Open **Activity → Neuronal mesh**. Create a neuron with a focused purpose and connect it to relevant peers. Send a signal and inspect the action/provenance. Use **Inspect / teach** to enter semantic demonstrations if you want Qwen to help classify unresolved signals. There must be at least two different action labels, at most eight examples. See the ready-to-copy JSON in [NEURAL_MESH.md](NEURAL_MESH.md).

Select completed impulses and record expected actions with notes. At least 12 independent observations are required before training; repeated or connected observations are grouped to avoid train/test leakage. Use **Train a Wind from labeled impulses**, inspect the held-out report, and then choose **Activate neural Wind** as a separate action. Nothing in training or evaluation changes the active policy.

For a repeatable terminal demonstration:

```sh
python tools/mesh.py --root mesh-demo demo
python tools/mesh.py --root mesh-demo inspect
python tools/mesh.py --root mesh-demo signal "Recall the music motif lesson" --node motif
```

To label, train and explicitly activate:

```sh
python tools/mesh.py --root mesh-demo feedback PULSE_KEY recall --note "Recall this lesson"
python tools/mesh.py --root mesh-demo train --config configs/sixstars.local.json
python tools/mesh.py --root mesh-demo activate PACKAGE_KEY
```

Use `configs/offline.json` in the train command when that is how you started the server. Get the package key from the registration result/UI. Distribute the complete candidate directory including `manifest.json` and `policy.wind`. The on-disk Wind ABI remains compatible.

The included music and architecture curricula still train their separate structured task abilities:

```sh
python tools/workshop.py --config configs/offline.json start --ui --train music,architecture
```

Add `--resume` after an interrupted curriculum run. Those packages belong in **Winds & learning**, not the neural conversation Wind selector. Their gates and explicit activation workflow are unchanged.

## 6. Leave work for later

Enable background contributions for a chat and schedule a review. In Neuronal response mode, that becomes a durable timed mesh impulse. It survives browser closure and process restart. Completed follow-ups are subject to the quiet contribution gate and remain auditable if held. The mesh does not run an idle inference loop.

`--upkeep` is optional compatibility support for the older general-purpose flesh/review engine. It now uses native decisions for ordinary review episodes. Leave it off when you want only neuronal mesh decisions; mesh reviews do not require it. Existing persisted upkeep settings are retained unless changed in Activity or through the CLI.

## 7. Upgrade and troubleshooting

Back up your data/run directories before changing versions. Keep the same WDS configuration paths to reuse journals, projects and installed packages. Existing policy files remain loadable; neural candidates add manifest schemas. No package is activated by this upgrade. To compare with a clean demonstration, use a separate configuration/data directory.

- **Runtime pack:** missing DLLs, ABI mismatch and GPU registration failures appear in the terminal and `runs/logs/runtime.log` under your configured run directory. See [RUNTIME_PACKS.md](RUNTIME_PACKS.md).
- **Wrong config:** use `--six-stars` for the Six Stars folder; `--config` expects a WDS config.
- **Local only:** no supported Qwen3 representation backend is loaded. Local Winds and web evidence still work.
- **No Qwen calls:** add labeled semantic demonstrations to the target neuron. Local decisions intentionally avoid Qwen when already sufficient.
- **Held or failed:** inspect the impulse reason. It may need evidence, labels, a shorter input, an available budget, an enabled node or explicit retry.
- **Training rejected:** gather independent examples rather than relabeling the same impulse. All splits need usable observations.
- **Activation rejected:** wait for or explicitly finish/cancel affected episodes; activation cannot replace an in-flight policy.
- **Web failure:** the error and request lease remain visible. Wikipedia is the first provider; there is no silent search-engine fallback.
- **Capacity reached:** export/retain the journal and begin a fresh demo data directory. Automatic archival is not implemented.

Full runtime details, limits and commands: [NEURAL_MESH.md](NEURAL_MESH.md). Prior generic LLM setup: [GETTING_STARTED-v0.5.md](GETTING_STARTED-v0.5.md). Validation: [VALIDATION-v0.7.3.md](VALIDATION-v0.7.3.md), with prior milestone coverage in [VALIDATION-v0.7.md](VALIDATION-v0.7.md).
