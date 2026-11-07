# v0.9.0 — language learning lifecycle

Added an experimental native sequence Language Wind and an explicitly selectable local Qwen streaming fallback. The assisted/strict language path bypasses natural-language grammar/keyword routing. Strict mode is the default; select assisted mode to enable the teacher. Legacy mode remains an explicit compatibility option for v0.8 commands.

- 1,440 synthetic conversations, 5,904 turns, split before training by wording family and entity set.
- Native C++ next-byte cross entropy, embedding/recurrent/output matrices, 32-step truncated backpropagation, clipped SGD and epoch checkpoints.
- A separately checksummed `language.tensor`, preserving the existing `policy.wind` ABI and explicit episode-boundary activation.
- Local Qwen teacher streams through the existing WebSocket token channel. Its context is bounded and limited to the selected chat plus scoped retrieved knowledge. The existing model allocation is reused; generation context is reset between requests.
- Teacher calls reserve a global per-window budget. Failures retain partial output and release the chat queue in both host and neural modes. A new interactive chat message can retry without waiting for the background failure cooldown; shared budgets and concurrency limits still apply. Strict mode forbids teacher generation.
- Teacher lessons are journaled as unreviewed. Explicit operator review is journaled separately; training can include approved examples from one root.
- Learning-time weight/gradient traces, complete local tensor checkpoints and post-training activation examples preserve source IDs. No claims of semantic Qwen weight addressing.
- Old reflection snippets no longer qualify as answer evidence in native expression.
- English training and explicit activation are available in the JSX interface. Generated teacher responses have visible attribution.

Limits: the small 64-unit recurrent student is a learning experiment, not Qwen's transformer architecture or general English competence. High symbol probability is not calibrated answer confidence. Original Qwen corpus reconstruction, Qwen logit distillation, base-model fine-tuning, GPU student kernels and universal semantic understanding are not implemented. The current mesh retrieval index is unchanged; internet acquisition remains the existing bounded web/source mechanism.
