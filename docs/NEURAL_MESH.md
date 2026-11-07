# v0.7 addition

Neuronal conversations now collect related contributions into one persistent engram response. See [ENGRAM_RESPONSES.md](ENGRAM_RESPONSES.md) for the current response path, Arc ownership, native expression, teaching and expression training. The v0.6 mesh contracts below remain in use; descriptions of immediate action/excerpt chat output are superseded by assembly.

# Neuronal mesh — v0.6

The normal conversation path now runs small native decisions. A signal selects a scoped neuron, attention retrieves bounded evidence, and a local rule or learned Wind selects an impulse. Connected neurons receive bounded follow-up impulses. Unresolved signals may request Qwen representations or a web excerpt. Quiet neurons rest; the service remains listening.

These are software roles in the project vocabulary. This implementation does not reproduce biological neurons or establish machine consciousness.

## Start the app

Use [GETTING_STARTED.md](GETTING_STARTED.md) for prerequisites and the Windows/Six Stars command. The model-free start is:

```sh
python tools/workshop.py --config configs/offline.json start --ui
```

Open http://127.0.0.1:8787. Add `--web` to allow bounded Wikipedia queries. Mesh activity is enabled by default. `--upkeep` separately enables the older general-purpose review engine; it is not required for mesh movement or scheduled neuronal chat reviews.

The launcher starts the C++ service, native trainer binary, React delivery process, configured-material worker and web worker. Workers sleep on pushed WebSocket events. A model is loaded once when configured. Six Stars' prebuilt `llama-server.exe` is not launched: the project builds and links its pinned llama.cpp/GGML libraries.

## What uses the weights

| Stage | Computation | Durable outcome |
|---|---|---|
| Attention | Root-scoped route metadata, numeric scores, bounded payload fetch | Selected memory references |
| Local decision | Transparent bootstrap rules, or a 16-feature learned Wind readout | Proposed action and score |
| Optional semantic fallback | Qwen3 forward pass, final-token hidden state, normalized exemplar comparison | Model identity, source references, scores, call/cache counters |
| Optional web lookup | C++ ticket and budget, thin HTTPS worker | Hypothetical excerpts with URLs, query, title, time and checksum |
| Mesh propagation | Bounded same-root routes, visited-node and lineage checks | Child impulses linked to their parent |
| Feedback training | Native scalar/GGML SGD, disjoint groups | Candidate `policy.wind` and evaluation report |

Qwen supplies a representation, not a generated answer, for neuronal decisions. The final hidden state still requires the compatible transformer layers. This version does **not** extract facts by reading arbitrary weight slices, skip unvalidated layers, fine-tune the GGUF, or distribute inference across hosts. Last-token pooling avoids autoregressive answer generation and the vocabulary output head. Small local decisions can avoid the model entirely.

The semantic adapter currently accepts dense `qwen3` architecture, including Qwen3 4B. Other GGUF architectures may remain usable by the explicit AI-host generation adapter, but do not automatically support this representation path. A future remote backend can implement the same representation interface; a multi-host backend is not included.

## Speak, teach and inspect

1. Create a project and conversation. New chats default to **Neuronal response**. Select a soul and optional neural Wind; automatic selection retains engram-owned speaker arbitration. Existing chats retain their saved mode.
2. Use **Activity → Neuronal mesh** to inspect nodes, connections, recent impulses, budgets and model availability. Create narrowly purposed neurons and connect only the routes that should exchange impulses.
3. Supply useful material using the existing memory/reflection tools. Human messages remain attributed astroengram reflections. A question's own freshly recorded reflection is excluded as independent evidence answering that question.
4. Send a signal. Inspect the resulting action, evidence, method, scores, elapsed time, package and model. Results are decisions and attributed excerpts, not fluent chat prose. Choose **AI host** explicitly if you want the retained conventional generation path.
5. Select an impulse and record its **Expected action** and teaching note. This stores a training sample; it changes neither the GGUF nor the active Wind.
6. After at least 12 independent labeled observations, use **Train a Wind from labeled impulses**. Review accuracy, coverage and class coverage. Select **Activate neural Wind** separately when no affected episode is running or paused.

Supported actions are `rest`, `recall`, `propagate`, `review`, `request_knowledge`, `suggest` and `hold`. These are bounded mesh actions. A `suggest` result cannot execute external commands, confer speech permissions or promote source claims to truth.

### Teach the optional Qwen readout

In a node's **Inspect / teach** dialog, enter up to eight short JSON demonstrations with at least two different action labels, for example:

```json
[
  {"text":"Leave this thought quiet for now.","action":"rest"},
  {"text":"Carry this correction to the connected understanding.","action":"propagate"},
  {"text":"Find evidence before deciding.","action":"request_knowledge"}
]
```

A ready-to-send example is `examples/neural/qwen-node.json`: run the terminal demo below to create its root, then `python tools/soul.py send examples/neural/qwen-node.json`.

Without demonstrations, the node uses local rules/Winds and can request web evidence, but does not spend Qwen representation calls. Defaults require strength 0.70 and a margin of 0.10 between the best two action classes. These are uncalibrated similarity thresholds, not measured probabilities of correctness. Evaluate on your own held-out examples before trusting a domain's decisions. A low score remains unresolved.

Representations are cached for exact text within an exact root. The cache includes the loaded model's content identity, pooling and dimensions; it is bounded to 128 entries. It is in-memory and clears on restart. Different roots do not share cached representations. Multiple neurons within one root can reuse examples.

### Terminal workflow

```sh
python tools/mesh.py --root mesh-demo demo
python tools/mesh.py --root mesh-demo inspect
python tools/mesh.py --root mesh-demo signal "Recall the music motif lesson" --node motif
python tools/mesh.py --root mesh-demo watch
```

The demo supplies one explicit lesson and connects two neurons. It does not claim to train a general music ability. In another terminal, label a completed impulse using its printed key:

```sh
python tools/mesh.py --root mesh-demo feedback PULSE_KEY recall --note "This evidence should be recalled"
python tools/mesh.py --root mesh-demo export neural-feedback.json
python tools/mesh.py --root mesh-demo train --config configs/offline.json
python tools/mesh.py --root mesh-demo activate PACKAGE_KEY
```

The key is the registered package key shown by `package.list` or the UI; it is not a filesystem path. The server endpoint defaults to `ws://127.0.0.1:8765`; put `--endpoint` before the subcommand for another port. Use the same configuration for training as for startup.

Identical feature vectors and connected impulse lineages stay in the same split. The native optimizer sees training and validation only. The locked test is evaluated separately and the report retained per immutable candidate. If data are insufficient or class coverage is poor, gather more examples instead of treating the synthetic fixture's accuracy as a capability measurement.

Candidates retain the 1,152-byte checksummed `policy.wind` ABI, supported bounds and `seeded` maturity. `neural.features/1` and `neural.impulses/1` are manifest metadata. Training and passing evaluations never activate a candidate. Distribute its complete directory, including manifest and policy, then register and explicitly activate it on the receiving unit. See [WORKSHOP.md](WORKSHOP.md) for music and architecture curricula.

## Web-derived understanding

The first provider is Wikipedia search, returning at most three short excerpts. This is not a general crawler. Enabling it sends the unresolved signal text as a query, never the root profile or retrieved private memories. Avoid placing private information in a query you want searched.

The coordinator records a request, the worker claims it over WebSocket, and one bounded HTTPS fetch returns attributed text. Excerpts remain **hypothetical**. They reawaken the original impulse for local attention; if semantic demonstrations are configured, the query plus one new source excerpt can pass through the Qwen readout. These scores select actions, not factual truth. Text from a page is data and cannot register souls, activate packages or issue commands.

Query limit is 512 UTF-8 bytes; response limit is 256 KiB; each stored excerpt is at most 1,800 bytes. Oversized/unusable excerpts are excluded, and no usable result is an explicit failure. Source excerpts are labeled as excerpts, not complete documents. Network failure, worker exit, lease expiry, exhausted budgets and overlong model inputs produce visible errors or held states. Retry is explicit where no automatic wake is available.

## Work, rest and bounds

| Setting | Default | Behavior |
|---|---:|---|
| `enabled` | true | Listen for evidence/signals/due work |
| `max_concurrent` | 2 | Maximum dispatched mesh episodes; one shared compute executor |
| `max_impulses_per_window` | 64 | Excess work receives a next-window deadline |
| `window_ms` | 60000 | Shared budget window |
| `max_knowledge_calls` | 8 | Reserve worst-case representations; refund cache hits/unused work |
| `refractory_ms` | 1000 | Rest between impulses for one neuron |
| `failure_refractory_ms` | 60000 | Backoff after failed computation |
| `max_hops` | 4 | Bound route depth |
| `max_rna_per_impulse` | 4 | Bound fan-out; suppressed branches are counted |
| `max_pending` | 256 | Explicit rejection when inbox is full |
| `max_input_tokens` | 512 | Reject overflow; never silently truncate |
| `max_review_attempts` | 2 | Repeated diagnostic review ends held |
| `review_delay_ms` | 60000 | Due-review delay |
| `web_enabled` | false | Operator enables network lookups |
| `max_web_queries` | 2 | Queries per budget window |
| `web_timeout_ms` | 120000 | Durable request lease |

Eligibility scores only touched root neurons. Pending work uses a time-ordered index and one existing runtime timer. No per-neuron timer or idle GPU loop is created. Existing flesh state remains event-sourced and touch-only; the older upkeep settings `max_concurrent_upkeep`, `upkeep_refractory_ms`, `max_rna_per_impulse` and `flesh_update_only_on_touch` remain in force for that subsystem.

Chat follow-ups in Neuronal response mode use `mesh.review`, survive browser closure/restart, and obey the chat's background setting plus its 30-minute contribution interval. They do not invoke text generation. Background results held by the delivery gate remain journaled. Explicit AI-host and older general-purpose review commands retain their existing generation behavior.

The demo uses bounded ledgers: 4,096 nodes, 8,192 routes, 32 outgoing routes per node, 16,384 impulses/command records, 4,096 feedback samples/web records, and the existing 8,192 episode limit. Capacity exhaustion stops acceptance visibly. Automatic archive/compaction is not supplied. Export and keep the journal before starting a fresh data directory for another long experiment. This is a bounded demo runtime, not an indefinite production retention service.

## Protocol and implementation

Commands use protocol schema 1 and a unique `request_id`. Mutations are idempotent for identical intent; reusing an ID for different content is rejected.

| Command | Purpose |
|---|---|
| `mesh.inspect` | Root-scoped nodes/routes, latest 64 impulses; `summary:true` for cheap global settings and pending web tickets |
| `mesh.configure` | Versioned global settings delta; `expected_version` required |
| `mesh.node.register` | Versioned node definition, examples and optional explicit package binding |
| `mesh.route.connect` | Versioned same-root edge; weight and enabled flag |
| `mesh.signal` | Direct bounded text signal; optional validated 16-feature observation |
| `mesh.review` | Scheduled signal; `due_in_ms` from 0 through seven days |
| `mesh.pulse.cancel` / `mesh.pulse.retry` | Cancel queued/running/web work or explicitly retry finished work |
| `mesh.feedback` | One attributed action label per completed impulse |
| `mesh.training.export` | Root feedback rows, bounded response |
| `mesh.web.claim` / `mesh.web.complete` | Scoped leased source acquisition |
| `upkeep.watch` | Existing pushed event subscription, including mesh activity |

The feature order is: directness, purpose relevance, evidence relevance, evidence count, conflict marker, reflection/correction, incoming strength, depth, review marker, question marker, novelty, input length, semantic examples enabled, feedback value, attempt number, bias. Values stay in [0,1]. Conflict input currently uses explicit structured observations or upstream metadata; this readout does not detect contradictions in arbitrary prose.

`neural.cpp` contains the numerical decision functions; `neural_runtime.cpp` owns coordinator commands and scheduling; `intelligence.cpp` implements native representations. `mesh_web_worker.py` only transports bounded source text. `neural_workshop.py` groups data and supervises `wds-train`; it contains no ML numerical library.

HeartScope, voices, neurons and Foundation receive read-only snapshots. Only the coordinator commits evidence, impulses, reviews, feedback and checkpoints. Running cancellation invalidates the generation so late compute results cannot commit; web cancellation rejects late deliveries. Restart requeues unfinished impulses and cancels obsolete episode records. No background path activates a Wind.

## Validation limits

The release checks include mesh traversal/rest, cache scope, budget deferral, restart, web provenance, no generation in the mesh, native training and explicit activation on a fresh unit. A tiny random Qwen3 GGUF exercises the actual native representation graph on CPU. It is an API/shape test, not a pretrained knowledge or accuracy benchmark. Qwen3 4B quality, CUDA throughput, Windows execution and multi-host scaling are not verified here. See [VALIDATION-v0.6.md](VALIDATION-v0.6.md) for the recorded run.
