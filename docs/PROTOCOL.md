# WebSocket protocol v1 — implementation 0.4

Endpoint: `ws://127.0.0.1:8765`. Only JSON text frames, maximum 16 KiB, nesting ≤32. Browser origins are rejected in this local CLI milestone. Identifiers are 1–80 ASCII alphanumeric / `_-.` characters. Every command has `schema:1`, typed `type`, and caller `request_id`. Choose unique request IDs; duplicate seeds and memory packet IDs are rejected. Relationship declaration retries return the original acknowledgment if their normalized content is identical. Reconnect/inspect uses explicit commands, not polling.

## Relationship root

```json
{"schema":1,"type":"root.select","request_id":"root-1","root_id":"composer","purpose":"Learn composition together"}
```

Creates or selects a durable root. Selecting an existing root does not replace its purpose. `root.selected.data` includes `soul_name`, level, commitment, interests and version. A seed implicitly creates its root if necessary.

## Relationship declaration (alias: confess)

```json
{"schema":1,"type":"relationship.declare","request_id":"declare-auerlis-1","root_id":"composer","soul_id":"auerlis","soul_name":"Auerlis","purpose":"music composition","relationship_to":"composer","relationship":"creative collaborator","voice_id":"purpose","expected_version":0,"paused":false}
```

Root must already exist. `relationship_to` must name that root or another declared soul inside it. This creates a scoped soul identity, not a separate top-level root. The primary root ID cannot be declared again; self relationships are rejected. All fields except `paused` are required; unknown fields are rejected. Names/purposes in imported text or seed strings have no declaration effect.

The one durable `relationship.declared` event contains `data.declaration` (normalized request), `data.soul`, and `data.interest`. Each projection includes origin/trace/version and the declaration sequence. Interest starts at strength 1. Bindings `purpose` and `constraint` are available; other IDs stay `pending`. `paused:true` overrides binding eligibility. Each distinct accepted declaration revision raises strength by one, capped at 100. No voice or teacher runs merely because interest was declared.

Retry the **same root + request ID + normalized declaration** to obtain the exact original event without an additional write or increment, including after restart. Changing the payload with that ID returns `declaration request_id conflict`. To revise a relationship, use a new request ID and the current soul version as `expected_version`; a stale value is rejected. A new version can rename, change purpose/relationship, rebind or pause the soul. Pending bindings require explicit redeclaration after a compatible implementation becomes available. Versioning is per `(root_id,soul_id)`, not the root's general practice version.

Bounds: IDs 80 bytes, name/relationship 128, purpose 1024; max 31 declared souls plus the primary root per heart; 4096 declared souls and 100000 request-ledger entries per store. Heart scoping is a local trusted-client boundary, not authentication.

```json
{"schema":1,"type":"heart.inspect","request_id":"heart-inspect-1","root_id":"composer"}
```

Returns `heart.snapshot` with `data.root_id`, `snapshot_sequence`, `souls`, and `interests`. This explicit request reads state once, has no durable side effect, and is not needed to wait for progress. Its current snapshot may be newer than a retried declaration acknowledgment.

## Seed

```json
{"schema":1,"type":"seed","request_id":"seed-1","root_id":"composer","text":"Develop a recurring motif","max_steps":3,"evaluator":"wds.ai.heart.composite/1","voice_budget":4,"rubric":{"must_include":"motif"},"ablations":{"uniform_attention":false,"no_routes":false,"freeze_motion":false,"no_memory":false}}
```

`text`: 1–4,096 bytes; `max_steps`: 1–8; optional required phrase: 1–128 bytes; rubric at most 1024 bytes. `evaluator` selects `wds.ai.heart.composite/1` (default) or `wds.ai.heart.rules/1`; unknown IDs are rejected before creating a seed. `voice_budget` is an integer 0..8 per evaluation (default 4), including the mandatory primary constraint voice. These values are pinned in the checkpoint. The server streams `root.updated`, `seed.accepted`, `wind.checkpoint`, `attention.selected`, `wind.moved`, `token`, `heart.evaluated`, and `episode.finished`. Multiple episodes may interleave; correlate using `trace_id` and instance IDs. Events carry journal sequence where durably recorded.

Example transient token:

```json
{"schema":1,"type":"token","trace_id":"seed-1","root_id":"composer","instance_id":"wind-4","iteration":0,"text":"A recurring motif"}
```

Token chunks are buffered to valid UTF-8 boundaries. The final artifact is authoritative. A heart result includes evaluator ID/version, aggregation policy, status, score, scope, metrics, evidence, represented/considered souls, voice contributions, skipped reasons, and disagreement. `completed` means all selected narrow checks passed; `exhausted` means the step budget ended without passing, including an inconclusive result. Each eligible/relevant consultation must fit the budget; pending or budget-omitted relevant voices make the result inconclusive unless a check already failed. `error` takes precedence over `fail`, then `inconclusive`, then `pass`. Scores never override status. No status establishes factual verification. A model failure produces a `wind.checkpoint` with status `failed` and error text; clients must terminate their wait on that status as well as on `episode.finished`.

Contributions are journaled inside `heart.evaluated.data.evaluation.contributions`, not emitted as separate partial writes. Each carries node and voice ID/version, root, trace, instance, iteration, snapshot sequence, represented souls, status, scope, feedback, metrics and evidence. `proposed_interests` are data with `status:proposed_only`; they never automatically change interest or start a nested evaluation. All views come from one authoritative garden snapshot per step.

The purpose voice checks token overlap with declared purpose. The constraint voice checks byte length and the required substring. `disagreement:true` preserves a pass and fail together, with both contributions. Voice selection records `selection_priority`; skipped reasons are `paused`, `irrelevant`, `binding_pending` or `budget_exhausted`. These are deterministic baseline checks, not verified creative quality or independent expert opinions.

## Living memory / local mesh packet

```json
{
  "schema":1,"type":"memory.upsert","request_id":"memory-1",
  "packet":{
    "schema":1,"intent":"memory.upsert","hops":0,
    "root_id":"composer","engram_id":"motif-technique","packet_id":"rna-1",
    "expected_version":0,"expires_ms":2000000000000,
    "text":"A motif can recur with a changed rhythm.",
    "source":"lesson URL or evidence identifier","claim_status":"hypothetical"
  }
}
```

Replace the illustrative expiry with current Unix milliseconds + 60,000. Allowed TTL ≤5 minutes. Packet ≤8 KiB, memory text ≤2,048 bytes, source ≤256 bytes. Source labels are caller-supplied provenance, not authenticated endorsements. Known root required. Expected version 0 creates; subsequent updates require exact current version. Allowed claim statuses: `observed`, `hypothetical`, `retired`. All web/teacher imports use `hypothetical`. The only supported destination is the local scoped engram node, with `hops=0`.

`memory.accepted` acknowledges durable storage. Duplicate packet IDs, stale versions, expired TTLs, absent sources, wrong schema/intent/hops, invalid status and oversized payloads fail. Accepted memories are available on the next retrieval. Policy theta and foundation weights do not change.

## Pause, resume, inspect

```json
{"schema":1,"type":"pause","request_id":"pause-1","instance_id":"wind-4"}
{"schema":1,"type":"resume","request_id":"resume-1","instance_id":"wind-4"}
{"schema":1,"type":"inspect","request_id":"inspect-1","instance_id":"wind-4"}
```

Pause takes effect at the next proposal boundary; it does not interrupt a running decode. Resume requires paused status and matching package/model identities plus an available pinned evaluator. Old 0.1 checkpoints without an evaluator field resume with rules/1. Inspect returns `wind.snapshot`, a read response with complete checkpoint state; it is not a new journal event. No continuous inspect loop is required: progress is pushed.

## Learning provenance

```json
{"schema":1,"type":"learning.record","request_id":"lesson-1","root_id":"composer","report":{"source":"lesson","objective":"explicit task/proxy name","accepted":false,"reason":"insufficient held-out data"}}
```

Report ≤12 KiB. Emits `learning.candidate`. This records a client-produced report; it is not a trusted runtime approval, does not install the package, and cannot promote maturity. The local learning tools use it after emitting memory updates and evaluating a candidate.

## Error and connection behavior

```json
{"schema":1,"type":"error","request_id":"memory-1","message":"memory version conflict"}
```

Validation errors are returned without dispatching the requested operation; rejection metadata is logged where possible. Some multi-event commands may have durable partial progress on disk/I/O failure; see recovery design. Queue-full errors may lack request ID because parsing is intentionally deferred to the worker. Transport rejects oversized/binary frames and may close slow clients. The server does not retry commands automatically or guarantee exactly-once responses after disconnect. Use unique IDs and inspect a known instance before retrying.

## Teacher adapter contract

A teacher endpoint emits a `hello` with `foundation` identity and accepts the same `seed` envelope. It streams progress and finishes with `episode.finished.data.artifact`. The bridge sends one bounded lesson request, waits for pushed completion, then disconnects. A normal OpenAI-compatible HTTP server is not automatically compatible: provide a WebSocket adapter or run a second wds-ai process backed by llama.cpp. No hidden-weight access is assumed.


## v0.3 workshop commands

All commands retain `schema: 1` and a bounded `request_id`. Responses echo it as `trace_id` except error responses (`request_id`). No command polls or starts an indefinite learning loop.

| Command | Fields beyond envelope | Result |
|---|---|---|
| `practice.assess` | `root_id`, structured `task` | `practice.options`: features, one-step action scores, target action and next task |
| `task.run` | `root_id`, `task`, optional `text`, `max_steps` 1..8, `package_key`, `mode` | Checkpoints, action/heart events, then `episode.finished` or a failed checkpoint |
| `episode.attach` | `episode_request_id` | `episode.attached` snapshot and live subscription, or `episode.missing` |
| `package.register` | `path` relative to configured package root, `checksum` | `package.available`; never activation |
| `package.list` | none | `package.snapshot` with manifests and active keys by domain |
| `wind.activate` | `domain`, `package_key`, `expected_active_key` | Durable `wind.activated`; exact request retry is idempotent |
| `cancel` | `instance_id` | Cancelled checkpoint; late compute is discarded |
| `runtime.shutdown` | none | Graceful local process shutdown; connection closes |

Task modes: `wind` (default), `fixed_refine`, `input_once`, `foundation_once`. The last requires an actual configured model. A task's action domain must match a specialized package's manifest; the initial generic Breeze package supplies the parent baseline. Task observations and actions are defined in `src/tasks.cpp`. Maximum task size is 10KB within the 16KiB command budget.

A candidate can be explicitly selected for a benchmark episode without becoming active. Activation checks the expected current key and requires no running/paused/in-flight episode in that domain. All existing checkpoints keep their original package key. The training runner has no activation path, and rejected gates do not delete evidence or activate a fallback. Changing a previously registered manifest or binary causes restart to fail validation.

`heart.inspect` additionally returns `data.mesh`: read-only neuron nodes, axon routes, journal/index references and crest/stem proposal-role metadata. This projection comes from committed relationships. `attention.selected.data.owner` identifies the neuron endpoint as attention owner. Neither the projection nor the endpoint has a garden writer.


## v0.4 scopes, expression and selective response

All commands retain `schema:1` and `request_id`. Mutating commands below share a durable request ledger: an identical retry returns its original acknowledgment, while changed content with the same root/request ID is rejected. Root IDs must already exist except for the original `root.select`/seed paths. Version checks are required where listed.

| Command | Additional fields | Response |
|---|---|---|
| `scope.bind` | root_id, kind (`project`/`account`), expected_version, optional account_root | scope.bound |
| `speaker.profile` | root_id, soul_id, expected_version; optional gender_type, pronouns, form, description, presence[], interests[] | speaker.profiled |
| `persona.preferences` | root_id, expected_version, preferences object | persona.configured |
| `route.resolve` | root_id, text; optional soul_id preference | route.selected |
| `reflection.record` | root_id, actor_soul_id, actor_kind (`human`/`voice`), text; optional actor_root, soul_id, instance_id | reflection.recorded |
| `engram.suggest` | root_id, local engram_id, speaker_soul_id, topic, reason; optional priority 0..100, due_in_ms | engram.idea_saved |
| `engram.defer` | root_id, idea_key, expected_version, due_in_ms | engram.idea_saved |
| `soul.inventory` | root_id; optional offset and limit 1..256 | soul.inventory |
| `soul.weight` | root_id, item_key, expected_version, finite weight 0..1 | soul.weighted |
| `neuron.signal` | root_id, text; optional soul_id, kind (`request`/`evidence`), task, force, max_steps, max_output_tokens, reply_to | neuron.decision, then pushed dispatch/completion |
| `learning.recall` | root_id, text | learning.history |
| `upkeep.inspect` / `upkeep.watch` | optional root_id (default `*`) | upkeep.snapshot |
| `upkeep.configure` | expected_version, settings; root omitted or `*` | upkeep.configured |
| `review.suggest` | root_id, text; optional soul_id, task, engram_ids[], due_in_ms, requires_material, reason | review.suggested |
| `review.retry` / `review.cancel` | root_id, review_key, expected_version | review.changed |
| `material.claim` | root_id, material_key | material.assigned |
| `material.complete` | root_id, material_key, claim_id; either error or chunks[], content_sha256, extraction_limited, optional resolved_url | material.received |

A `reply_to` must reference a finished attempt in the same root. The next prompt includes that prior exchange; it cannot import another project's conversation. `chat.py` sets it automatically. The same tool records user turns as attributed astroengram memory. Feedback-only `/reflect` does not start a response.

`upkeep.watch` sends one snapshot and subscribes the connection to future matching root events, including background responses. Call it before `neuron.signal` when waiting for its eventual completion. Signals are correlated by `signal_key`; episodes by `instance_id`. A queued signal is accepted work, while `hold`/`reuse` complete immediately and state why. Paused or failed episodes remain inspectable. Settings changes affect future scheduling; already in-flight compute reaches its normal boundary.

Profiles emit stable expression metadata with `gender_type:unspecified` when no gender was declared. Form, presence and interests are not restricted to human characters. Account memory access requires an explicit one-way grant, with no transitive project access. All local clients are trusted: these scopes are data boundaries, not authentication.

The primary heart result, token events and final checkpoint identify the selected outward speaker. Consulted voices include their own `expression`. Requested speaker, engram override and original memory author remain separate. Idle item identities never grant speaking permission.

Use [UPKEEP.md](UPKEEP.md) for the complete scheduling policy, capacities, defaults, expression inheritance and reinforcement rules. `max_output_tokens` is 1..4096: direct seed default 256, neuron signal default 1024, upkeep default 512. Reaching it returns an explicit failed checkpoint with a partial artifact rather than a successful truncated response.

## v0.7 engram response API

All commands below use schema 1, a unique `request_id` and `root_id`. Reads do not append journal events. Mutation request IDs are idempotent only for identical content. Maximum string lengths are UTF-8 byte limits.

| Command | Additional fields | Result/event |
|---|---|---|
| `assembly.teach` | `engram_id`, `expected_version`, `subject` (256), `predicate` (80), `object` (1024), `source` (256); optional `exclusive`, `claim_status` | `assembly.taught` |
| `assembly.attune` | `engram_id`, `expected_version`, numeric `importance` in [0,1] | `assembly.attuned` |
| `assembly.list` | optional `before` sequence | `assembly.listed` with 20 summaries and `next_before` |
| `assembly.inspect` | `assembly_key` | `assembly.snapshot` |
| `assembly.feedback` | `assembly_key`, `action`, optional `note` (1024) | `assembly.feedback_recorded` |
| `assembly.training.export` | none | `assembly.training.rows` |

`mesh.signal` and `mesh.review` accept `response: true` to open an assembly; a `chat_id` also requests one. Their acknowledgment includes `assembly_key`. An optional `wind_instance_id` attaches a completed, scoped artifact up to 2,048 bytes. `chat.send` forwards the same attachment field. Subscribe with `upkeep.watch` for pushed `assembly.updated`, `assembly.completed` and `chat.message` events. Use explicit inspect after reconnect; do not poll.

`wind.activate` additionally accepts domain `expression` with an expression-feature package and the current `expected_active_key`. Collecting assemblies must finish or be cancelled first. Feedback, passing tests and package registration never activate it. See [ENGRAM_RESPONSES.md](ENGRAM_RESPONSES.md) for the complete contract and limits.
