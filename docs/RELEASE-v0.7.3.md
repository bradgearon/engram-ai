# v0.7.3 — open conversations, journal recovery and diagnostics

The supplied Windows journal contains singleton objects in fields that require arrays of records. Applying those records produced `cannot use at() with boolean`. Model generation had succeeded, but chat completion failed; later queued work retained a stale busy neuron reservation.

This release explicitly constructs record arrays, checks collection shapes before durable append, and accepts the known legacy singleton representation during replay without changing original history. Completed host replies are delivered on recovery, and stale terminal reservations are released. Unexpected coordinator errors fail/reconcile the episode visibly and let following work progress; journal failure still stops recovery visibly.

Both chat modes now accept bounded, durable follow-up queues. The composer stays open during a response, queued messages have visible status, and switching focus waits for accepted work. Completion, failure and cancellation release the next message. Paused work retains explicit resume behavior. Queue bounds fail explicitly. Draft text typed while awaiting acknowledgement is preserved.

Local diagnostics capture native commands/events, neural activity, model tokens, browser exceptions, connection failures, supervisor output and training activity. `start --ui --trace` enables full recording while retaining existing model/runtime-pack/data paths. `tools/diagnostics.py ... bundle` exports a hashed ZIP without model/runtime binaries. Details and exact commands: [DIAGNOSTICS.md](DIAGNOSTICS.md).

The production React bundle is included and reports v0.7.3. No new ML framework, Boost, Python dependency or TypeScript is introduced. Policy ABI, existing neural execution budgets, checksums, and explicit episode-boundary activation are unchanged.

See [VALIDATION-v0.7.3.md](VALIDATION-v0.7.3.md) for verification and platform limits.
