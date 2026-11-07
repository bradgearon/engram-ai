# Legacy generation path (retained explicitly in v0.6)

The description below documents the v0.5 generation adapter. **New v0.6 Neuronal response chats use [NEURAL_MESH.md](NEURAL_MESH.md) instead.** AI host and older explicit generation/review APIs still use this path. General-Wind chat generation below is historical behavior, not the new default.

# When WDS calls the LLM

The GGUF contains Gemma's trained model weights and metadata. The linked llama.cpp library loads that model once when the native WDS process starts and runs inference when a language proposal is requested. This release does not send those requests to Six Stars' separate llama-server.exe or a hosted service. The web UI talks to WDS over WebSockets; model invocation is an internal C++ call.

## A normal message

1. `chat.send` journals attributed human input and queues an interactive neuron request. The current chat UI explicitly requests a fresh response, rather than reusing a cached answer.
2. Before generation, the runtime retrieves candidate memory locations only from granted roots. The neuron selects attention; the mesh fetches selected payloads. The engram layer selects the actual speaker and preserves the selection reason.
3. The foundation request includes the user goal, selected speaker/profile and root context, selected memory text and provenance, the preceding finished exchange when present, Wind movement state, and any applicable previous candidate/evaluator feedback. It does not load the entire event chain or all chats into the prompt.
4. llama.cpp renders the GGUF's chat template, tokenizes the prompt, runs Gemma inference, and streams generated text through WebSocket events. The model stays loaded; this implementation creates a bounded generation context per call rather than retaining a separate long-lived KV cache for every chat.
5. The heart evaluates the proposal with the registered rules or task evaluator. The coordinator saves the response, evaluation, speaker, policy and memory references. Current heart/mesh voices are deterministic evaluators; each voice does not secretly launch another language model.

The current chat UI requests one generation step per send. Other runtime episode APIs can explicitly request bounded multi-step refinement. Exhaustion or errors preserve partial output and report an incomplete/failed result; they are not counted as successful completion.

## Which actions need inference?

| Action | Real LLM called? |
| --- | --- |
| AI-host text chat | Yes, one generation; Wind movement is frozen |
| General-Wind text chat | Yes, one generation guided by selected memory, speaker and Wind state |
| Save reflection, edit profile, update memory, inspect activity | No immediate call for that operation |
| Eligible background text review | Yes, only after attention and shared review/foundation-call budgets allow it |
| Idle system, irrelevant/held evidence or reusable background result | No new call |
| Standard music/architecture structured practice and native policy optimization | No; small policies select repair actions and native task evaluators score them |
| Explicit foundation comparison or configured teacher | Yes, within its separate configured budget |
| Demo foundation | No; deterministic fixture output is visibly labeled |

The current model context budget is 8192 tokens including the requested output; chat requests default to at most 1024 output tokens. Six Stars’ server context setting is not inherited. An oversized prompt is rejected explicitly, and exhausted generation preserves partial output.

A reflection can create a bounded later review when upkeep is enabled. Saving the reflection itself does not run generation. A completed eligible review may add a quiet contribution to its chat, subject to the delivery interval and chat settings.

## What learning changes

Reflections and RNA updates change external scoped memory, evidence, saved ideas and response priority. Native Wind training changes the small candidate policy artifact. Neither path rewrites Gemma's GGUF. Candidate activation remains a separate explicit operation at an episode boundary.

Code: `src/workspace.cpp` (chat request), `src/upkeep.cpp` (attention/queue/budget gates), `src/core.cpp` (`begin_step`, `compute_step`, `finish_step`), and `src/intelligence.cpp` (`Llama::propose`).
