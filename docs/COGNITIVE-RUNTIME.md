# Cognitive runtime v1 — v0.8 implementation boundary

## Execution

The versioned built-in Language Wind (`wds.ai.winds.language.grammar/1`) maps explicit language to typed operations. Unrecognized input can use an explicitly activated, model-bound Language Wind readout for three read-only intents. Ordinary chat has no generative fallback. Existing memory-based expression remains available for cited knowledge.

`cognition.run` accepts `operation` and `root_id` through schema-1 WebSockets. It schedules one normal bounded native episode; `cognition.inspect` returns the exact root snapshot. Operations are `entity`, `teach`, `predict`, `simulate`, `inspect`, `help`, `greeting`, and `language.sample`. The chat grammar is listed in the quickstart. A structured call uses, for example:

```json
{"schema":1,"type":"cognition.run","request_id":"example-1","root_id":"home","operation":{"op":"predict","action":"unlock","entity":"cave-gate"}}
```

The compute worker sees a copied immutable snapshot. It returns a proposed state, a structured claim/prediction and a native speech plan. The coordinator rechecks the state version, commits one journal event, and releases the reply. A changed version produces an explicit failure rather than overwriting new knowledge. A cancelled/paused episode discards its pending result. Stable signal/pulse identities prevent duplicate learning after a crash between knowledge commit and reply delivery.

Recognized grammar instructions skip the generic memory retrieval scan. They address the root's typed entity/action maps and inspect only the selected action's bounded demonstrations/edges. The current implementation still copies the bounded root snapshot; it does not claim constant work for arbitrarily large roots.

The heart's existing evaluation remains attached to grounded free-text responses. Typed operations use `wds.ai.heart.cognition/1` contract validation; a successful contract is not a truth judgment or an empirical success of the predicted action. Engram speaker selection remains in the existing coordinator/assembly path. Only an origin's direct user instruction can propose a mutation; propagated mesh payloads and retrieved web excerpts cannot execute grammar instructions.

## Typed knowledge and learning

An entity has an identifier, kind, version, scalar properties, source and claim status. An action is parameterized by target kind, not an entity name. Demonstrations store before/after properties, observed success and source. Successful examples supply candidate preconditions and effects. A property whose observed value varies across successful examples is not a fixed precondition; an unreported property is unknown. Shared changed fields become predicted effects. This is a bounded conjunctive learner, not causal identification: sparse demonstrations can suggest incidental preconditions and fail on complex/disjunctive tasks.

Negative demonstrations veto matching known failed contexts. Later positive examples can introduce a newly observed prerequisite. Identical inputs with incompatible outcomes remain conflicting. No destructive correction of history occurs. Simulation applies the predicted effects to a versioned local entity and marks it simulated; it does not train on its own output. Operator-reported outcomes remain attributed reports, not independently verified reality.

For demonstrated predicate edges the native update is:

```
e_next = min(2, 0.8 * e + 1)
w_next = clamp(w + 0.2 * outcome * e_next - 0.01 * w, -2, 2)
outcome = +1 for reported success, -1 for reported failure
```

This is an event-step eligibility rule, not a measured biological time constant. Only edges touched by the demonstration update. During prediction matching predicate edges supply `drive`; four bounded recurrent steps calculate:

```
u_next = 0.5 * u + 0.5 * tanh(drive + 0.2 * u)
```

The activation and exact touched edges are emitted for inspection. In this milestone activation measures response strength; the explicit learned preconditions and conflicts gate execution. It does not establish factual truth, prove convergence or make a self-sustaining worker. Changing the numeric activation alone cannot bypass a precondition. Native scalar work is appropriate for these tiny networks; Qwen representation computation continues to use the configured GGML backend/GPU.

## Qwen and Language Wind artifacts

Qwen's causal-last representation is computed through its supported native architecture. No chat prompt, text-token sampling or answer generation is used for language training/readout. Reading arbitrary weight rows is not treated as knowledge retrieval. The existing b9870 pack ABI remains unchanged; no intermediate-layer hooks or unsupported new DLL symbols were added.

`language.projection/1` hashes each embedding dimension into one of 16 signed lanes using FNV-1a over `language.projection/1:<index>`, then L2-normalizes. The projection is fixed. A native learned linear readout uses the existing Wind matrix/bias. Only its first three rows are decoded for greeting/help/inspect; acceptance requires strength .75 and margin .25. The manifest pins the full model/pooling/dimension identity. Train and validation texts must be distinct and model identities equal. The locked test is evaluated after optimization. Train/validation/test labels remain explicit in Arc.

Candidates remain the existing 1152-byte checksummed `policy.wind` ABI, 64-byte alignment, bounded weights (±4), and seeded maturity. No expanded on-disk matrix or renamed ABI is hidden in a new manifest. Registering, training or evaluating does not activate a candidate. Explicit language activation is blocked while any episode is running/paused or an assembly is collecting.

The built-in grammar is a native capability, not a newly pretrained broad-language neural package. The optional learned package expands only three read-only intent routes. General semantic parsing, multi-step planning, open-ended language realization, automatic corpus extraction and distributed GGUF execution remain future work.

## Arc, RNA and bounded work

`cognition.committed` stores the root state and result in the authoritative Arc journal. An RNA-prepared `expression_trace` memory links the operation/claim/source and is indexed as an astroengram. These traces are not reused as factual evidence and do not invalidate ordinary-memory cache generations. Changes to cognitive state and Language Wind selection do invalidate signal cache keys. IGFBP2/IGF2BP2 retain their existing index/payload responsibilities; the new typed root map is a replay projection, not a second writable store.

| Bound | Value / behavior |
|---|---|
| Root cognitive state | 96 KiB, explicit rejection before commit |
| Entities / operations per root | 256 / 64 |
| Scalar properties per entity or observation | 16 |
| Demonstrations per operation | 32 |
| Learned predicate edges per operation | 128 |
| Language examples per root | 128 |
| Recurrent prediction steps | 4, only matching edges |
| Language encoder request | One per classification/sample, 512 input tokens |
| Global episodes / queue / mesh limits | Existing coordinator and mesh budgets still apply |

The language workshop is an external WebSocket client and native trainer process. It does not get a journal writer. Learning calls reserve the normal scheduler budget; mesh calls never encode without a reserved knowledge slot. Grammar and learned structured operations can run without Qwen. Unknown natural language may not.

Language samples use `mesh.signal` and the stable pulse key, including when submitted through `cognition.run`; their acknowledgments are `mesh.signal_queued`, followed by `cognition.committed`/`mesh.impulse`. They do not create response assemblies with a chat deadline. A depleted positive encoder budget defers them to the next window via the existing ordered queue/timer. Ordinary host classification also reserves from the shared encoder budget and falls back to native grounded expression when no slot is available.

## Wider mesh and rolling context

An unanswered general host question records a typed `knowledge_need` and schedules one idempotent `mesh.review` with `lookup=true`. It first traverses scoped archived evidence, then can request a bounded web query. Results enter Arc with URL, provider, query, retrieval time, content checksum and hypothetical claim status. They can inform later attention and an attributed background chat contribution, subject to the existing quiet-contribution, concurrency, source and byte budgets. Neither a source excerpt nor a high attention weight activates a Wind or validates an action's predicted outcome.

Missing entity properties instead record an observation need. The runtime does not search the web to invent whether a private physical/simulated object is locked or jammed. Knowledge requests are typed by what evidence could actually resolve them.

The workshop starts the existing WebSocket-driven archive/source and web workers. Network acquisition is on by default (`network_enabled`), with `--offline` taking precedence. The supported search provider is Wikipedia; this milestone does not add general browser automation or unlimited internet crawling. Configured HTTPS source fetches and local archive reads remain distinct. Worker failure terminates the supervised launch visibly; it cannot silently leave a supposedly live acquisition service behind. No periodic polling was added.

These terms are software roles inspired by the project's framework. The implementation does not demonstrate that astrocytes, IGFBP2, IGF2BP2 or Arc perform these exact computational roles in a biological brain.
