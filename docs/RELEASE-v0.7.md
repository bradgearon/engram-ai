# v0.7.0 — assembled responses and shared Arc memory

Neuronal chat now waits for related contributions and produces one native sourced reply. v0.6 delivered an individual neuronal result. v0.7 adds a persistent assembly, explicit teaching/correction, expression-plan training, bounded memory importance and inspectable connections to the exact learned readouts used.

## Delivered

- Engram assemblies retain lineage, contributing neurons, source versions, model/policy bindings, speaker choice, unresolved work and outcome. Deadline/cancellation are visible; obsolete work cannot overwrite the result.
- Native expression realizes taught subject/relationship/object statements, quotes sources, handles greetings and reports missing/conflicting evidence. A completed Wind artifact can be attached explicitly.
- Direct teaching and correction create versioned engrams. Bounded importance influences relevant attention and event-triggered impulse strength without granting truth or activation.
- Arc remains the authoritative journal for both engrams and astroengrams. IGFBP2 supplies scoped locations; IGF2BP2 fetches selected payloads and prepares RNA with no write handle. Payload fields are excluded from the location index.
- Astroengram response traces join memory versions to actual Wind readout rows, feature schema and package checksum. They are not recursively reused as factual evidence.
- Expression feedback/export, native scalar/GGML candidate training, held-out evaluation and explicit episode-boundary activation preserve the existing Wind ABI.
- The React JSX Activity panel adds response inspection, teaching, optional importance, feedback and separate expression training/activation. Soul status identifies both Arc datasets. Built assets are included.
- `tools/engram.py` provides a runnable demo plus ask, teach, attune, inspect, feedback, export, train and activate commands. The readout supervisor now resolves the configured `auto` backend before native training.
- Recovery covers interruption during an assembly and between durable completion and chat delivery. Delivery is deduplicated on repeated restart.

## Start and upgrade

Back up existing data first. Extract the archive into a fresh code directory and retain your intended WDS config/data/package paths. Old journals replay; new settings merge with persisted settings. Existing Wind packages retain their checksummed ABI and are not activated by the upgrade.

```sh
python tools/workshop.py --config configs/offline.json start --ui
```

Open http://127.0.0.1:8787, then try `python tools/engram.py --root demo demo` in another terminal. Full setup, Six Stars/Qwen selection and training instructions are in [GETTING_STARTED.md](GETTING_STARTED.md) and [ENGRAM_RESPONSES.md](ENGRAM_RESPONSES.md).

`RELEASE_MANIFEST.json` lists file hashes and changes against the delivered v0.6 ZIP, so this archive can be distinguished by content rather than approximate size.

## Limits

Native expression is a bounded renderer and learned plan selector, not a general learned language generator. Semantic teaching is explicit. Conflict detection compares explicitly exclusive structured relationships; it is not an unrestricted contradiction detector. Memory importance is explicitly taught. No adapter optimizer, arbitrary fact-to-GGUF-weight lookup, base-model mutation, multi-host backend or tensor-core acceleration of the small REA kernels is claimed.

The binary RNA/peptide instruction-language idea is recorded as a future extension; no new executable instruction format is introduced. Activation stays explicit and the coordinator remains the only writer.

This is a local bounded demonstration with finite ledgers and manual archival. Authentication/remote production deployment and audio output remain outside this release. Linux CPU builds were checked; actual Qwen3 4B quality, CUDA performance and Windows/macOS execution were not. Browser/UI tests were skipped at the user's request; the production bundle was built.
