# Local diagnostics and recovery — v0.7.3

The normal launcher already writes native model output to `<runs>/logs/runtime.log` (for this installation, normally `sixstars-wds-runs/logs/runtime.log`). The authoritative event journal is `<data>/runtime/events.jsonl`. The journal records accepted changes, but cannot explain every rejected command or browser error. v0.7.3 adds correlated diagnostic traces for those cases.

## Start with full tracing

Stop the old workshop with Ctrl+C. Extract the updated project over your source folder, preserving your local config, data, models and runtime pack. From `U:\wds-ai`:

```powershell
python tools/workshop.py --config configs/sixstars.local.json start --ui --trace
```

`--trace` saves the diagnostics settings below in your existing WDS config, preserving model, runtime-pack and data paths. The launcher rebuilds changed native code and serves the bundled matching React UI. Refresh the browser at `http://127.0.0.1:8787`. The version shown in the sidebar should be **v0.7.3**. Add `--upkeep` or `--web` only if you want those existing options enabled.

If your generated config has a different filename, substitute that path in every command. You do not need to regenerate the Six Stars config or compile CUDA. Use the same working runtime pack.

```json
"diagnostics": {
  "enabled": true,
  "level": "trace",
  "include_content": true,
  "max_file_mb": 16,
  "keep_files": 8
}
```

This is one property inside the existing config object; retain all other properties and the comma separating it from the preceding property. Accepted levels are `trace` and `errors`. File size is 1–256 MiB; retained backups are 1–32 per trace stream, in addition to the active file. Full tracing is opt-in. Changes take effect on restart.

Alternatively enable/disable without starting:

```powershell
python tools/diagnostics.py --config configs/sixstars.local.json enable
python tools/diagnostics.py --config configs/sixstars.local.json disable
```

`enable --metadata-only` omits content in newly recorded traces. Existing logs may still contain content captured earlier. It also excludes the authoritative journal from the export. `start --trace` explicitly turns full content back on.

## Send a diagnostic bundle

Reproduce the issue, then stop the workshop with Ctrl+C to get a complete, quiescent snapshot:

```powershell
python tools/diagnostics.py --config configs/sixstars.local.json bundle --output wds-diagnostics.zip
```

Attach `U:\wds-ai\wds-diagnostics.zip`. Export also works while the app is running: it copies complete journal lines up to each file's recorded cutoff, reporting omitted tail bytes. Concurrent rotation can produce a missing-file warning in the manifest. Stopping first avoids that race.

The bundle includes:

- Native model startup/output logs and rotated diagnostic traces.
- Raw incoming commands, parsed commands and request IDs, failures, command timing, and runtime events even when the browser is closed.
- Neural eligibility/attention events emitted by the engine, Wind decisions, impulses, assemblies, memory updates, RNA/provenance, budget outcomes, completion and recovery events. The original journal is included when `include_content` is true.
- Browser JavaScript errors, rejected promises, React error-boundary stacks, console warnings/errors, request and connection metadata, and training supervisor events.
- Current config, available application release metadata, package manifests, and a per-file SHA-256 inventory.

GGUF files, model weights, DLLs, executables, build outputs, and environment variables are excluded. Known credential fields in config/structured Python diagnostics are redacted. Full conversation content, local paths, and anything pasted into a message or console can still be present; inspect the ZIP before sharing publicly. The bundle is intended for debugging your local installation.

This is application/neural-event tracing, not a GPU kernel profiler or a dump of every tensor operation. The native model's own stderr remains in `runtime.log`. That existing log and the authoritative journal are not rotated by the trace settings; only diagnostic JSONL streams are.

## Trace files and bounds

Under `<runs>/logs/diagnostics/`:

| File | Records |
| --- | --- |
| `runtime.jsonl` | Native commands, emitted events/tokens, exceptions, startup and shutdown |
| `teacher.jsonl` | Same native telemetry for an optional teacher runtime |
| `workshop.jsonl` | Build output, launcher lifecycle and command-line training progress |
| `supervisor.jsonl` | Runtime process arguments, output, readiness, exit and launcher failure |
| `teacher-supervisor.jsonl` | Optional teacher process lifecycle |
| `interface.jsonl` | Browser telemetry, relay errors and training activity |

Every trace record includes schema, time, source, session and event kind; runtime records also carry a per-session sequence. Request IDs, event sequence numbers, root IDs and instance IDs remain in payloads so they can be joined to the journal. Trace rotation records say when older files expire. A single record can take a file slightly over its configured size before the next rotation.

Browser telemetry is bounded separately: 200 buffered records, 12,000 characters per record, 64 KiB socket backlog, and 500 records/second at ingestion. Overflow/oversize events explicitly report omissions. Native events and the journal retain the canonical accepted state. Trace I/O failures are printed to supervisor stderr. Closing the page can lose its last unsent browser records; native activity continues to be logged. The diagnostics socket reconnects on browser focus/online events, with no polling.

## Chat behavior and recovery

Typing and sending stay available in both AI host and Neuronal response modes while a reply is pending. Follow-ups display **Queued** and run in acceptance order, one pending response per conversation. The queue allows 16 waiting messages and 64 KiB of stored commands; a full queue produces an explicit error and leaves the draft intact. Shared mesh, model and upkeep budgets remain in force. Focus changes wait until the conversation drains so accepted messages keep their intended scope.

On startup, old singleton record collections are normalized only in the in-memory projection. Original journal bytes/hashes are retained. Recovery appends delivery events for completed host work and releases stale neuron reservations, allowing waiting conversations to continue. Already-completed output is delivered without asking the model to generate it again. Work genuinely paused at shutdown still shows **Resume response**; queued messages wait behind it.

Never delete `events.jsonl` to fix a stuck chat. A corrupt checksum is still an error; this compatibility repair does not bypass chain validation. None of these repairs or tracing options activates a Wind package.
