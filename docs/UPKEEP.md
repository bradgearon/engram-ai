# Selective response, upkeep and root expression — v0.4

Run the processes with `python tools/workshop.py start --upkeep`. Add `--train music,architecture` to build candidate Winds in the same session. The runtime is C++20 coroutines and a small standard-library executor; standalone Asio handles WebSockets and one review timer. Python launches processes, handles files and optionally fetches configured HTTPS documents. There is no numerical Python training and no Boost dependency.

`--upkeep` explicitly enables the persisted setting. Omitting the flag preserves the existing setting; a fresh store defaults to disabled. Ctrl+C or `workshop.py stop` shuts down the processes. Unfinished episodes become paused on restart and need explicit resume/cancel. Due reviews survive restart. The material worker reconnects on process restart through a single snapshot and then waits for pushed tickets. A lost connection exits visibly; it does not poll or silently retry.

## Small movement

`neuron.signal` addresses a declared soul in an existing root. An explicit request is considered even with low lexical relevance. Evidence signals can be held when irrelevant or refractory. The neuron owns the decision. Cheap eligibility precedes dense attention and inference.

The coordinator keeps an inbox of at most 16 queued signals per neuron and 256 globally. It serializes responses from the same neuron. A successful result can be reused when the input, evidence generations, selected expression, policy and step allowance match. The dispatch stage checks again, so duplicates queued during a response can reuse its outcome. `force:true` explicitly requests fresh work. A held or reused signal is a journaled decision, not an invisible discard.

The memory index returns route/embedding metadata within the project and an explicitly connected account scope. Attention chooses at most four payloads; only then does the Arc payload fetch occur. The current index still scores each memory key in an allowed root; it is not an ANN database or a global garden scan. Learning recall examines at most the 64 most recently updated attempts and returns up to four relevant excerpts, including unsuccessful attempts and their evaluator/package attribution. Full artifacts remain in their episode checkpoints.

## Flesh and reviews

Flesh is a small event-sourced vector per root: hunger (missing knowledge), thirst (practice need), candle (small standing preference), kindling (a concrete unresolved need), and fatigue. Only touched roots change; there is no per-token or idle recomputation. It guides priority and never changes policy weights.

A failed interactive structured task can create a review with its current artifact and score. Benchmark episodes set `allow_upkeep:false` and run in the workshop class. Explicit `review.suggest` can request local review, practice or configured-source material. Success reduces the need; no progress raises fatigue and a refractory delay. Repeated failure reaches a visible `resting` state. A new evidence event may reschedule an affected review. There is no suppress-all inflammation flag in this version; conflict memories remain available for diagnostic review.

| Bound | Default | Behavior at the bound |
|---|---:|---|
| `max_concurrent_upkeep` | 2 | Reservations, running and paused reviews occupy slots; further reviews wait for completion |
| `upkeep_refractory_ms` | 60000 | Repeated unsuccessful reviews wait before another attempt |
| `max_rna_per_impulse` | 4 | Oversize source batches are rejected atomically; extraction limits are reported |
| `flesh_update_only_on_touch` | true | Mandatory; no polling recomputation |
| `max_episodes` / window | 4 | Due review receives a reason and next window deadline |
| `max_foundation_calls` / window | 2 | Reserve the full possible review cost before starting |
| `max_fetches` / window | 2 | Reserve one configured-source request; no implicit search |
| `window_ms` | 3600000 | Shared across roots; restart preserves usage |
| `max_steps`, `max_attempts` | 2, 2 | At most two bounded review attempts before rest |
| `review_delay_ms` | 60000 | Failed practice creates a later review |

A time-ordered index provides O(log n) review updates. One Asio timer wakes at the earliest deadline and is rearmed early only when an earlier need appears. Removing an early deadline may cause one harmless early wake. There is no timer when there is no work. Each wake handles at most 32 due review records. The compute queue gives interactive/upkeep/workshop classes weighted service (4:1:1) without preempting an in-progress model call. Upkeep concurrency is a reservation bound; the first implementation still has one compute worker.

Ledger limits reject new work explicitly. A memory change touches at most `max_rna_per_impulse` linked review records in one event and reports the count left unchanged; those retain their prior schedule/status for inspection or explicit retry. Broad fan-out is not silently expanded. A budget that cannot fit even one review produces `awaiting_budget`, requiring a configuration change and explicit retry. An unconfigured source produces `awaiting_source`.

## Root expression and flexible soul binding

A root can represent a person, creature, object, project or organization. `speaker.profile` supplies a free-text `form`, an optional description, and lists of `presence` and `interests`. Examples include motherly, brotherly, fatherly, sisterly, birdlike curiosity, a child's age-appropriate curiosity, a quiet rock, or The Last Cat as an organization. These are presentation declarations, not claims that the implementation creates living beings.

Gender is an optional independent attribute (`man`, `woman`, `nonbinary`, `neutral`, `unspecified`), with optional pronouns. It is never inferred from a name. Root form, presence and interests supply defaults to its speakers; explicit speaker profiles override them. Gender and pronouns belong to each declared identity and are not inferred from the root. Account preferences supply defaults to an explicitly connected project, whose settings take precedence. Preference fields are tone, familiarity, preferred gender types, and an optional preferred soul ID. Intimate personalization means explicit familiarity, warmth, interests and presentation, adjustable at these roots.

The runtime selects a known relevant soul within the current root; an explicit `soul_id` is a speaker preference. Ambiguity falls back to the primary or configured preferred soul. The engram layer has final speaker selection: an evidence-backed saved idea may override that preference. Weak topic matches stay deferred. `engram.defer` saves an idea for later, with a deadline. The journal records requested/selected soul, reason, evidence and expression. Cancellation/failure returns a reserved idea to pending. Successful expression marks it delivered. This selects one outward speaker; evaluator perspectives remain attributed internal contributions. Keep cross-soul noise minimal and follow the user's current direction.

`reflection.record` captures human or declared-voice feedback as an astroengram. It preserves the original actor, target, source root and optional episode/artifact/policy reference. Selection of a new speaker never rewrites that author. Next responses can retrieve the reflection, and its memory generation invalidates cached responses. Human and voice identities are locally declared attribution; they are not cryptographically authenticated.

Attributed attention grants up to 0.2 pending reinforcement credit to the target neuron. A completed interactive response from that target consumes the credit once and updates its bounded attention weight: `new = old + credit * (1 - old)`. The journal links the source reflection and response. The routing bias is small (up to 10% of an already relevant lexical score); irrelevant souls gain no route solely through weight. Weight measures response attention priority, not truth, human worth, or permission. Benchmark and upkeep output cannot earn this interactive credit. A passing response still means the configured narrow evaluator passed.

The AI-owned item registry derives a 32-byte aligned numeric identity/weight lane for each root, declared soul, accepted engram and produced episode artifact. Map keys and provenance metadata have additional storage overhead. These dormant records do not create tasks, voices or speaking permission. `soul.inventory` is paginated; `soul.weight` changes a known item's weight with version checks.

## Source worker and visible limits

Supply an `upkeep` object in the workshop configuration, for example:

```json
{"enabled":true,"sources":[{"url":"https://example.org/your-actual-lesson","topic":"music composition"}]}
```

Use actual public HTML/plain-text lessons. A matching review emits `material.requested`; the thin worker claims it, fetches once, and submits bounded hypothetical memories. It validates public HTTPS addresses and redirects, limits a document to 1 MiB and uses a 120-second claim lease. It stores content hash and resolved URL; extraction truncation is explicit. Failures become a visible resting review. No unconfigured browsing or arbitrary code execution occurs. Material additions alone do not train a Wind.

Model output has an explicit token allowance (`max_output_tokens`, direct-seed default 256, chat/signal default 1024, upkeep default 512; valid 1..4096) and an 8192-byte artifact limit. Exhaustion fails the episode with `output_complete:false`, the reason and a UTF-8-safe `partial_artifact`. Partial output never passes as a complete response. Reconnect to an episode checkpoint to inspect it. The launcher reports training timeout and checkpoint/resume instructions.

All durable writes remain coordinator-owned. HeartScope and voice inputs are immutable snapshots. Candidate generation and upkeep never activate packages; `wind.activate` remains a separate explicit operation at an episode boundary. The schema-1 checksummed `policy.wind` layout, numerical bounds and maturity values are unchanged.

## Commands and examples

After starting, create and configure the example root:

```sh
python tools/soul.py send examples/expression/root.json
python tools/soul.py send examples/expression/profile.json
python tools/soul.py send examples/expression/preferences.json
python tools/soul.py send examples/expression/signal.json
python tools/soul.py reflect --root last-cat --actor last-cat --text "The music ending felt rushed; leave room to breathe."
python tools/soul.py inspect --root last-cat
python tools/soul.py watch --root last-cat
```

The example profile uses the primary organization identity. Use `relationship.declare` to add a bird or rock as a distinct known speaker, then configure its profile; merely mentioning one creates no new speaker. Files use fixed request IDs for idempotent examples; edit the ID and expected version for a new change. Profile changes do not silently reset other fields. See the protocol guide for the full command table. Watch once for pushed events; do not loop inspect commands.
