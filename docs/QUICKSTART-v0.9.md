# v0.9 — streamed teacher assistance and experimental sequence learning

This release adds a working training/inspection loop. Its small native sequence student is experimental. It is not a replacement for Qwen's broad English ability. The optional teacher is normal local Qwen generation, clearly labeled in chat. No cloud model service is required.

## Start on your Windows installation

Stop the previous workshop, preserve your local configuration and data, then extract the new source over `U:\wds-ai`.

Use the working GGUF and runtime-pack configuration you already have:

```bat
python tools/workshop.py --config configs/sixstars.local.json start --ui --trace --language-mode assisted
```

Open http://127.0.0.1:8787 and refresh the page. The footer should show v0.9.1. AI host and Neural response both keep the conversation open. A Qwen fallback streams with a **Qwen teacher** label; partial failures preserve the output and show an error.

To train while the app runs:

```bat
python tools/workshop.py --config configs/sixstars.local.json start --ui --trace --language-mode assisted --train-english --english-epochs 8
```

Training uses a separate native process, emits progress, checkpoints after each epoch and never activates its result. The trainer is CPU-based in this release; teacher inference uses your configured GGML/GPU pack. No Boost or ML framework was added.

For a fresh configuration:

```bat
python tools/workshop.py start --six-stars "U:\six-stars-solution-2" --model "U:\six-stars-solution-2\models\llm\Qwen3-4B-Q4_K_M.gguf" --runtime-pack "U:\six-stars-solution-2\runtime-packs\win-x64\llama-cpp-dynamic-win-x64" --gpu-layers 99 --ui --trace --language-mode assisted
```

The startup language flag is a session override. To persist it, add this object to the existing WDS JSON configuration, retaining its other paths:

```json
"language": {
  "mode": "assisted",
  "teacher_calls_per_window": 8,
  "teacher_max_tokens": 1024,
  "student_threshold": 0.9
}
```

The window is the existing mesh budget window (normally one minute). Exhaustion is visible and requires a later retry. A reservation can be consumed even if the student eventually supplies the response. `teacher_max_tokens` is an explicit maximum: reaching it is a failure with preserved partial output, never silent completion. `--offline` disables web/source acquisition; local Qwen assistance remains available.

Modes:
- `assisted`: an explicitly activated student may respond when complete and above the selected-symbol threshold; otherwise local Qwen streams assistance. That score is not factual confidence.
- `strict` (default): no teacher calls; an absent or unaccepted student produces an explicit held explanation.
- `legacy`: v0.8 structured grammar and narrow readout compatibility. Select this explicitly only when running the old compatibility workflow.

## Train from the included conversations

The corpus contains 1,440 synthetic conversations / 5,904 turns, covering reference, word-order reversals, negation, correction, uncertainty, conditions, projects, preferences, scope, missing information, comparison, hypothetical statements, clarification, social language, fiction, agency and recovery. They describe invented worlds; they are not imported into your personal memories.

In the app choose **Winds & learning → English → Train / resume Wind**. The selected project's explicitly approved teacher lessons are included. Or, with the runtime running:

```bat
python tools/workshop.py --config configs/sixstars.local.json english-train --epochs 8
```

Read the reported `report.json`, including generated samples and the conversation completion results. It reports measured sequence prediction, not a claim of general understanding. CPU elapsed time depends on the corpus and machine. Stop/retry the identical command to resume the last complete epoch; changing the job creates a different candidate.

When you decide to try the candidate, finish or cancel all active episodes and use the separate activation command:

```bat
python tools/workshop.py --config configs/sixstars.local.json activate --domain language --package PATH_PRINTED_BY_TRAINING
```

The UI also offers explicit activation. Passing an evaluation, training completion, or teacher feedback cannot activate a package.

## Teach from your conversations

Teacher replies are saved as unreviewed demonstrations in the Arc. Click **Approve language example** only for a reply you want the student to imitate. This approves the language example, not all factual claims in the response. Training remains a separate action.

Then train from that project using its root/project ID:

```bat
python tools/workshop.py --config configs/sixstars.local.json english-train --root home --epochs 8
```

To inspect or edit lessons outside the app:

```bat
python tools/workshop.py --config configs/sixstars.local.json english-export --root home --output sixstars-wds-runs/language-lessons.json
python tools/english_review.py --lessons sixstars-wds-runs/language-lessons.json --approve SOURCE_INSTANCE_ID --output curricula/english-personal.local.json
python tools/workshop.py --config configs/sixstars.local.json english-train --curriculum curricula/english-personal.local.json --epochs 8
```

Use the real project ID shown in the app/trace if it is not `home`. Review/export stays inside that root. Personal examples and checkpoint directories are excluded from normal source distribution.

## Inspect the package

- `policy.wind`: existing checksummed 1,152-byte ABI, unchanged bounds/maturity contract.
- `language.tensor`: 64-byte-aligned file layout of the local recurrent model's trained FP32 matrices.
- `manifest.json`: architecture, tensor checksum, dataset fingerprint and validation result.
- `weight-updates.jsonl`: sampled learning-time hidden activity and actual local weight updates.
- `learning-traces.jsonl`: bounded post-training output contributions linked to source examples.
- Adjacent `.checkpoint` directory: per-epoch tensors and resumable optimizer position.

The tensor layout is suitable for a future GPU kernel. Student execution/training presently uses native CPU loops; GGUF/Qwen execution still uses your existing native GPU backend. Do not treat the small student as Qwen's knowledge copied into a new package.

See [Qwen training research](QWEN-LANGUAGE-TRAINING.md), [release changes](RELEASE-v0.9.md), and [diagnostic export](DIAGNOSTICS.md).
