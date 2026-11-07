# Engram responses and Arc — v0.7

The normal conversation path now assembles one response from related neuronal contributions. It does not send the conversation to a generative LLM. The native renderer makes sentences from explicitly taught relationships, quotes other source material, handles greetings, and reports missing or conflicting evidence. A trained expression Wind can select a reply plan. This is a working, bounded grounding/realization path, not a general learned language generator.

## Ownership

| Component | Responsibility | Durable state |
|---|---|---|
| Arc | Authoritative history for both engram and astroengram datasets | The existing checksummed `events.jsonl` journal |
| Engram | Holds taught understanding, assembles contributions and arbitrates the final speaker | Versioned memory records and response assemblies in Arc |
| Astroengram | Retains attributed experience, reflection and links between memories and behavior | Reflection records and expression traces in the same Arc |
| IGFBP2 indexer | Returns scoped locations, versions, numeric attention metadata and payload references | Rebuildable indices over Arc; no separate content bank |
| IGF2BP2 handler | Fetches selected payloads and validates incoming RNA into proposed records | No independent store or write authority |
| Astrocyte | Proposes a small impulse when touched evidence matters | The proposal records its source engram, importance and flesh priority |
| Neuron and Wind | Apply attention and the selected movement policy within shared budgets | Decisions, source versions and package identity in Arc |
| Coordinator | Validates and commits accepted updates, schedules work and delivers replies | Sole writer |

`mesh::IGFBP2Indexer` and `mesh::IGF2BP2Handler` have read-only references to the same `chain::Store` (Arc). The indexer does not return text, semantic statements, attribution or learning-history payloads. The handler fetches at most the selected top-k and checks root and version. The current local index ranks a bounded root slice, not a production ANN index. `arc_dataset` distinguishes engrams and astroengrams without duplicating records. `arc_sequence` identifies the event establishing the current record; older versions remain in the journal.

These names are software roles in this project. They do not establish corresponding biological mechanisms.

## From a signal to words

1. A chat signal opens a persistent assembly and pins its expression package. A direct `mesh.signal` can request this with `response: true`.
2. Attention retrieves scoped locations; the handler loads selected Arc payloads. Each neuron returns a small decision and an `engram.payload/1` containing numeric features, evidence snapshots and model/policy bindings.
3. Connected impulses contribute to the same lineage. A web lookup, if enabled and budgeted, contributes an attributed, unverified excerpt. The response waits for related work to settle.
4. The engram deduplicates sources by identity/version, checks simple declared-exclusive conflicts, and uses the current speaker arbitration. Root read grants are checked again at completion.
5. Native expression renders the selected plan. The coordinator records the result, an astroengram learning trace, and the chat message. Restart repairs a crash between assembly completion and delivery without a duplicate message.

The response trace is not later treated as independent factual evidence for its own answer. A newly recorded question cannot answer itself. Correcting a taught relationship changes the next response; completed responses retain their original evidence versions.

## Start and teach

From `wds-ai`, install the single Python dependency and start the services:

```sh
python -m pip install -r tools/requirements.txt
python tools/workshop.py --config configs/offline.json start --ui
```

Open http://127.0.0.1:8787. In another terminal:

```sh
python tools/engram.py --root demo demo
python tools/engram.py --root demo ask "What is The Last Cat?"
python tools/engram.py --root demo list
python tools/engram.py --root demo inspect ASSEMBLY_KEY
```

The demo creates a project/chat, explicitly teaches a relationship and asks about it. Running the same demo command reuses its idempotent teaching record. A fresh demo lesson is version 1. Correct it explicitly:

```sh
python tools/engram.py --root demo teach --id cat-definition --version 1 --subject "The Last Cat" --predicate "is" --object "a game about rescuing stars" --source "My revised project notes" --exclusive
python tools/engram.py --root demo ask "What is The Last Cat?"
```

`--version` is the current memory version, not the new version. A stale version is rejected. Unverified teaching defaults to `hypothetical`; it is attributed to the local operator. Free-form chat feedback remains a reflection. It does not silently overwrite an unrelated structured fact; use Teach to correct that fact.

In the UI, use **Activity → Engram responses → Teach a relationship**. Inspect a response to see its contributions, source chain, selected speaker and expected-expression feedback form. Soul status continues to expose the underlying engrams and astroengrams.

## Importance and weight connections

Importance is a bounded numeric attention value stored with a versioned memory. For example, after the correction above:

```sh
python tools/engram.py --root demo attune --id cat-definition --version 2 --importance 0.8
```

This creates version 3. It preserves the claim, authorship and content. It can add at most 0.25 to a relevant attention score. On a memory touch, importance and the root's flesh priority contribute at most 0.25 to the proposed impulse strength. Eligibility, fan-out, refractory intervals, budgets and the active Wind still control execution. No background scan recomputes importance. v0.7 importance is explicitly taught, not autonomously optimized.

An assembly's `weight_bindings` identifies the exact checksummed `policy.wind`, feature schema, seven readout rows, sixteen columns and seven biases used by a neural or expression policy. Its astroengram `learning_link` joins those bindings to source memory versions and the response checksum. Bootstrap rules have no learned readout weights, so no fictitious binding is recorded. Optional Qwen representation identity remains in each neuronal contribution's `model_binding`.

These are usable connections between memory, attention and application behavior. They are not addresses of individual facts in Qwen's pretrained tensors. Base GGUF weights remain frozen. Learned adapters, weight-slice discovery, arbitrary architecture support and multi-host execution are not implemented.

## Include a Wind creation

A completed Wind episode can contribute its artifact and evaluation to a new assembly:

```sh
python tools/engram.py --root demo ask "Review this Wind creation" --wind-instance INSTANCE_ID
```

The episode must be completed, within the root's read scope, and have an artifact of 1–2,048 bytes. Oversized artifacts are rejected explicitly. The artifact is quoted with its episode/package provenance; its evaluator result does not establish factual truth. The wire equivalent is `chat.send` or `mesh.signal` with `wind_instance_id`. This is explicit attachment, not automatic inclusion of every past Wind output.

## Train expression independently

```sh
python tools/engram.py --root demo feedback ASSEMBLY_KEY answer --note "Use the cited relationship"
python tools/engram.py --root demo export expression-feedback.json
python tools/engram.py --root demo train --config configs/offline.json
python tools/engram.py --root demo activate PACKAGE_KEY
```

Record distinct examples before training. At least 12 independent feature/lineage groups are required. Repeating identical conversations does not satisfy this requirement. Connected and identical observations remain in one split. Native scalar SGD or pinned GGML learns plan choices; Python only exports, splits, launches and transports. Training uses train/validation data, then evaluates the immutable candidate against its locked test. Reports measure plan classification, not prose quality or factual correctness. The UI offers the same training and separate activation controls.

The seven plans are `answer`, `compare`, `acknowledge`, `clarify`, `defer`, `reflect`, `conflict`. The source contract always rejects a learned plan that would answer without evidence or suppress a detected conflict. Insufficient strength/margin falls back to native rules with a recorded reason. Unverified source material stays labeled.

`expression.features/1` has sixteen float lanes: evidence present; typed statement present; question; correction; comparison; greeting; preference; background; unresolved work; exclusive conflict; observed source present; contribution count/16; maximum lexical relevance; source count/8; statement; bias. Counts are bounded to [0,1]. It is metadata over the existing 1,152-byte Wind ABI, not a new artifact layout. Passing a gate does not activate it. Activation is explicit and requires all assemblies and affected episodes to reach a boundary.

## Bounds and visible outcomes

| Setting or limit | Default / bound |
|---|---|
| `response_deadline_ms` | 30,000; configurable 1,000–300,000 |
| `max_response_contributions` | 16; configurable 1–32 |
| `max_response_sources` | 8; configurable 1–8 |
| Native reply size | 16,384 bytes; explicit failure on overflow |
| Sources per neuron / quoted in reply | At most 3 / 3; retained extras and omissions are recorded |
| Assemblies / expression feedback rows | 8,192 / 4,096 |
| Existing memory capacity | 4,096 shared engram/astroengram records |

The same one-shot scheduler handles assembly deadlines, even while mesh dispatch is paused. Reaching a deadline or contribution budget completes an `incomplete` response with the reason, cancels remaining related work and rejects stale worker results. Explicit cancellation is `cancelled`; declared-exclusive conflicts are `held`; composition failures are `failed`. Missing evidence is stated in the text even if the assembly successfully completed its processing. A full memory bank records `trace_error` on the assembly rather than losing the response.

A proposed contribution count limit stops further related work once reached; siblings already completed in one coordinator event may appear in that final event. Overall fan-out and lineage bounds still apply. Limits do not imply successful understanding. Journal/ledger archival remains manual in this demo.

## Future binary RNA / peptide instruction set

The intended extension is a versioned binary instruction set whose concepts describe RNA/peptide operations. Its compatibility must be defined through byte order, layouts, bounded operands, opcodes, feature/action schemas and capability declarations in Wind manifests. Unknown versions/opcodes must fail explicitly. A peptide structure would be data interpreted by defined operations, not arbitrary executable code or an inferred biological function.

This instruction language is deliberately deferred. v0.7 retains the existing typed RNA mutation contract, 256-byte aligned numeric export tiles and unchanged Wind ABI. Neither an RNA packet nor a future instruction gains implicit package activation or bypasses coordinator writes.
