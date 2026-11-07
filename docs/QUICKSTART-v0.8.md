# v0.8 — start speaking and teaching

This version changes **AI host** to the native knowledge engine. Neither chat mode asks Qwen to generate a reply. The built-in Language Wind has a deliberately small grammar. It does not yet understand arbitrary conversation as well as Qwen's generative interface.

## Update and start

1. Stop the old workshop process. Keep your existing local configuration and run/data directories. Back up the run directory before upgrading. Extract the new `wds-ai` contents over the source directory; the archive contains no personal config, GGUF, journal, or Windows DLLs.
2. In `U:\wds-ai`, reuse the configuration that already points to your working model and runtime pack:

```bat
python tools/workshop.py --config configs/sixstars.local.json start --ui --trace
```

Open **http://127.0.0.1:8787**. The launcher rebuilds the changed C++ source and serves the bundled matching web UI. Reload an existing browser tab. The footer must say **v0.8.0**.

The mesh's web/source workers are available by default. A missing general answer can trigger a background archive/web review and later add an attributed contribution to the chat. The initial web provider is Wikipedia search excerpts, not a general browser or a crawler of the whole internet. Existing configured HTTPS source reviews and local Arc memories remain available. Only bounded query text goes to the search provider, not the whole conversation or root profile.

Use `start --ui --offline` to disable runtime network acquisition, or set `"network_enabled": false` in the WDS config. Local knowledge and archived memories remain usable. The offline example config already disables it. Network failures are recorded as held/failed work; they do not erase local understanding.

If you need to create the configuration instead:

```bat
python tools/workshop.py start --six-stars "U:\six-stars-solution-2" --model "U:\six-stars-solution-2\models\llm\Qwen3-4B-Q4_K_M.gguf" --runtime-pack "U:\six-stars-solution-2\runtime-packs\win-x64\llama-cpp-dynamic-win-x64" --gpu-layers 99 --ui --trace
```

The pack route loads your existing llama/GGML/CUDA DLLs in-process. It does not compile CUDA or launch llama-server. A GGUF is optional for the native operation demo below. To run that demo without any model, use `--config configs/offline.json start --ui` on a separate data store.

## Teach and transfer an operation

Create a chat in a project. Either **AI host** or **Neuronal response** accepts these messages. Send each line separately:

```text
help
teach unlock door : locked=true,key=true,color=red => locked=false
teach unlock door : locked=true,key=true,color=blue => locked=false
teach unlock door : locked=true,key=false,color=red => failure
entity cave-gate door : locked=true,key=true,color=green
predict unlock cave-gate
try unlock cave-gate
show cave-gate
```

The successful examples disagree about color, so color drops out of the hypothesized preconditions. The new green gate was never a training example. `predict` returns the expected consequence. `try` applies it **only to the local simulation**. It never opens a real door or counts its own prediction as an observed success.

`entity` replaces the supplied entity description; include all properties you want retained. Names and kinds use identifiers such as `cave-gate`, with no spaces. Property values can be booleans, numbers, quoted strings, or simple unquoted strings. `teach` merges changed fields on the right into the full before-state; `failure` records unchanged state and a failed outcome. Text is case-sensitive for these commands.

## Add a newly discovered prerequisite

```text
entity jammed-gate door : locked=true,key=true,color=violet,jammed=true
predict unlock jammed-gate
teach unlock door : locked=true,key=true,color=violet,jammed=true => failure
teach unlock door : locked=true,key=true,color=white,jammed=false => locked=false
entity another-gate door : locked=true,key=true,color=silver,jammed=true
predict unlock another-gate
```

The later successful observation introduces `jammed=false`. The final prediction is blocked even for a new entity/color. Missing required properties produce `unknown`. Exactly contradictory observations produce `conflict` and remain visible; they are not silently overwritten.

Use **Activity console → Learned operations** to inspect predicates, learned edge weights, eligibility, demonstrations, source episode IDs and versions. `cognition.committed` events contain the prediction, matched neurons and four-step activation trajectory. The same journal contains the astroengram trace and RNA-prepared memory record. **Engram responses → Teach a relationship** remains available for factual statements and cited replies.

Restart the service and type `show cave-gate` or `show knowledge`. The native state survives independently of Qwen.

## Train a Language Wind

The built-in grammar works immediately. The optional learned readout recognizes **greeting**, **help**, and **inspect** paraphrases. These are read-only intents: the classifier cannot infer arbitrary operation arguments or authorize writes.

With Qwen3 loaded, choose **Activity console → Train language intent examples**, or run in a second terminal:

```bat
python tools/workshop.py --config configs/sixstars.local.json language-train --root language-workshop
```

The shipped `curricula/language-v1.json` contains 30 distinct phrases: 18 training, 6 validation, 6 locked test. To supply your own, copy it, retain the fields/splits, and pass `--curriculum PATH`. Use a fresh root after changing the model or relabeling existing examples.

The process encodes each phrase once through Qwen's compatible native transformer computation, applies a fixed 16-lane signed projection, and trains the existing 16×16 Wind readout using native SGD. It writes a checksummed 1152-byte `policy.wind`, manifest, checkpoint and held-out report under the configured package/run directories. No ML framework or Python numerical training is used. The locked test never reaches the optimizer.

Encoding shares the mesh budget (default eight calls per minute). The 30-example curriculum can therefore take several minutes even on a fast GPU. Waiting samples have a deadline and resume through the existing timer; progress reports `language.waiting`. Zero knowledge budget, a disabled node, or a missing Qwen encoder cannot train this readout. Paused mesh work must be resumed in Activity. Grammar/operation learning does not need encoder calls.

Inspect `accuracy`, `coverage` and `accepted_correct` in the report. A poor report is still a candidate, not proof of usable language understanding. No passing score activates it. Once all chats/episodes are finished, explicitly choose the candidate from **Activate a trained Language Wind**, or use the path printed by training:

```bat
python tools/workshop.py --config configs/sixstars.local.json activate --domain language --package PATH_FROM_REPORT
```

Model hash/pooling identity must match the candidate. Mismatch or representation failure produces a visible error, never a generative fallback. Scores below strength .75 or margin .25 defer to grounded native expression. A softmax score is not a calibrated truth probability.

## Limits and diagnostics

No silent truncation: malformed instructions, capacity, scope/version conflicts and encoder limits fail visibly. A concurrent knowledge change rejects the stale update; resubmit it against the new state. Normal chat remains open with the existing bounded message queue.

Use the existing [diagnostics instructions](DIAGNOSTICS.md) to export logs. With `--trace`, the trace includes conversation content and learning examples, so review the bundle before sharing. The server's normal chat path records `generation_calls: 0`; an encoder pass is recorded separately.

The explicit `foundation_once` benchmark API remains available for comparison testing; neither chat mode selects it. This is a narrow functional learning prototype, not a demonstrated replacement for general language understanding or a validated model of biological cognition. See [the runtime design](COGNITIVE-RUNTIME.md).
