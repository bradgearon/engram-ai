# What Qwen's published training actually tells us

Qwen learned its language behavior through optimization over sequences. Its training system predicts tokens and adjusts parameters to reduce prediction error. It did not acquire that behavior through a programmer's list of words that trigger canned replies.

The Qwen3 report describes approximately 36 trillion pretraining tokens across 119 languages and dialects. Its stages cover general language/knowledge, a stronger mixture of reasoning/STEM/code, and longer contexts. Smaller models, including 4B, then use teacher-generated responses and teacher prediction distributions during post-training. The report describes both off-policy response distillation and on-policy logit matching using KL divergence.

The dense model uses a transformer with grouped-query attention, SwiGLU, rotary positions and normalization. Qwen tokenization uses byte-level BPE. Those details matter: the published weights were trained to work with that computation. Reading a few selected weight rows does not reproduce the resulting language ability.

Primary sources:
- [Qwen3 technical report, sections 2–4](https://arxiv.org/html/2505.09388v1)
- [Qwen team's release and training overview](https://qwenlm.github.io/blog/qwen3/)
- [Qwen's tokenizer and model-type documentation](https://github.com/QwenLM/Qwen3/blob/main/docs/source/getting_started/concepts.md)

## The learning operation, step by step

1. **Represent the sequence.** Byte-level BPE turns text into token IDs. Learned embedding rows turn those IDs into vectors. Tokenization determines the symbols; it does not assign a fixed meaning to a sentence.
2. **Compute context.** Causal attention combines information from earlier tokens. Feed-forward layers transform the result. Position information lets the computation distinguish different word orders.
3. **Predict.** The output produces a probability distribution for the next token. The training text supplies the target, so large amounts of ordinary text can provide their own prediction labels.
4. **Adjust parameters.** Backpropagation computes how prediction loss changes with each participating parameter. An optimizer updates those parameters over many examples. This is the general mechanism underlying causal language-model training, not a recovered copy of Qwen's private optimizer configuration.
5. **Refine behavior.** Post-training adds demonstrations and other feedback. Qwen's small-model distillation includes learning from teacher responses and matching teacher predictions on student-generated sequences.

For example, the difference between “the cat chased the dog” and “the dog chased the cat” must come from the sequence computation and training examples. A rule such as “contains cat, dog and chased” cannot establish who acted on whom. WDS evaluation includes such role reversals and unseen wording; it must improve on those tests before we call the student capable.

The intended connection to an engram is therefore an input/context representation, a reproducible model computation, and its resulting state/update trace. We cannot replace that computation with a fact lookup into isolated Qwen weight addresses. Capturing active units can help inspect a run, but activity alone does not establish that those units are a complete, reusable implementation of a meaning.

Foundational references: [Attention Is All You Need](https://arxiv.org/abs/1706.03762) and [Distilling the Knowledge in a Neural Network](https://arxiv.org/abs/1503.02531). These describe the broader methods; they do not supply Qwen's private training run.

## What is known, and what is unavailable

The public material supplies architecture, broad data composition, staged training, several objectives and evaluation results. It does not provide the complete original corpus, exact per-example sampling history or a complete reproducible pretraining run. We cannot honestly claim to know every detail of how this checkpoint was trained. A few thousand conversations cannot reproduce that training scale.

For ordinary sequence prediction the general objective is:

    loss = -sum_t log p_theta(symbol_t | symbols_before_t)

One possible teacher-distribution objective is forward KL:

    loss_distill = sum_t KL(p_teacher(. | context_t) || p_student(. | context_t))

The Qwen3 report says KL alignment but does not specify every implementation detail or the KL direction in that section. The displayed direction is illustrative, not a claim about its undisclosed implementation. These are different objectives. WDS v0.9 implements the first on conversation examples. Reviewed teacher responses can supply those examples. It does NOT implement Qwen vocabulary-logit KL distillation, Qwen fine-tuning, or Qwen-scale pretraining.

## The next WDS milestone

The optional Qwen teacher supplies useful streamed language now. An experimental native recurrent student learns sequence prediction from full dialogues. It uses an embedding matrix, recurrent matrix, tanh hidden state, and output softmax; native C++ truncated backpropagation trains all these matrices. It is a small research baseline, not the same architecture as Qwen and not a claim of general English understanding.

The new `assisted` and `strict` language paths contain no natural-language keyword classifier, hash-of-words classifier, regex fact parser, or three-intent lookup. The explicitly selected `legacy` mode retains the v0.8 grammar for compatibility. `/cognition {...}` remains an explicit machine command, not a language understanding rule. Existing mesh location retrieval still has its older numerical/lexical index; this release does not claim to have replaced that index with semantic retrieval.

A generative sequence student is itself a language model. The broader WDS system can still own memory, identity, decision-making and execution independently. Renaming the language component a Wind does not change its mathematical nature.

## What the learning record means

`language.tensor` stores the complete trained local parameter set. `weight-updates.jsonl` records the first 32-symbol learning chunk of every training conversation in every epoch, with its hidden state and the eight largest target-output-row gradients, before/after weights and source example ID. `learning-traces.jsonl` records bounded post-training activations and output-weight contributions. Epoch tensors remain in the local checkpoint directory.

This is a bounded, reproducible account of local parameter updates, not a complete recording of every intermediate activation. It does not label coordinates as particular facts. No Qwen intermediate-layer capture or arbitrary copying of Qwen weights is implied. Qwen's model identity and generated demonstration are recorded separately.

## How to judge progress

Report validation and locked-test cross entropy/bits per byte separately from generated conversation answers. Exact-match dialogue checks are intentionally strict and can reject valid paraphrases. Conversely, good prediction loss or high selected-symbol probability does not establish correct reasoning, memory, factual accuracy or useful conversation. Inspect the generated samples, use unseen human conversations and expand the evaluation set before promoting a student to routine use.
