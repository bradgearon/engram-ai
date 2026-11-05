# Training Instructions — Musician / Producer Wind

## Goal

Produce a small, checksummed, GPU-aligned wind package (compatible with the existing 16×16 or modestly extended `policy.wind` ABI) that improves held-out music tasks after practice.

## Step 0 — Lock the held-out set first

Before generating any training data, write 12–20 tasks such as:

- “Continue this 2-bar motif for 4 bars using primarily minor-6th Continuation relationships; keep the original home key; end with a clear return.”
- “Apply AHD: establish C major, open a dissonant space on F#, develop a short melody there, return so the return feels like payoff.”
- “Transform the given riff by fragmentation + register expansion; do not introduce a new unrelated motif.”
- Genre-constrained: “8-bar post-rock build from a quiet pedal; climax must recombine earlier material at larger scale.”

Score them with a simple heart:
- constraint satisfaction (interval usage, AHD return, no new unrelated motif, resonator legality if applicable)
- optional preference model or consistent human rubric

Save the held-out set and the scoring function. Do not train on these examples.

## Step 1 — Turn local guides into practice trajectories

From the extracted files in this package:

- Interval guide → generate (source interval, Continuation target, example phrase) triples
- AHD → generate (home, dissonant pitch, development, return) sequences
- Genre guide → generate (seed motif, section role, required transformation) examples
- Statement/Answer/Counter → generate positional-anchor response pairs

Format each as:
```
seed: ...
constraints: ...
intermediate_states: ...
critique: ...
final: ...
```

## Step 2 — External documents

You can give an LLM the three strongest local files plus 1–2 chapters from Open Music Theory or Belkin. Ask it only to:

- extract additional graded exercises
- propose counter-examples
- stay inside the resonator or a simple pitch/rhythm representation you define

Do not let it freely rewrite the core emotional or AHD rules.

## Step 3 — Train the small policy

Target the existing wind format:
- 16×16 weight matrix + bias (or a clearly versioned extension)
- checksummed
- bounds ±4
- maturity “seeded” or “growing”

Use a surrogate that includes:
- reconstruction or policy imitation on the practice trajectories
- a probe on a small validation slice of the same distribution

Reject the candidate unless the held-out music tasks improve relative to:
- one-shot foundation
- iterative refine without the wind

## Step 4 — Package layout

```
packages/musician-candidate-N/
  manifest.json
  policy.wind
  weights.json          # optional human-readable
  LEARNING_REPORT.md    # sources, probe numbers, rejection/acceptance
```

## Step 5 — Integration

Load with the existing runtime:
```
./wds-ai --package packages/musician-candidate-N --data exp-N
```

Use RelationshipDeclaration / InterestRecord (v0.2) if you want the music voice to become eligible only after confession of a “composer” or “producer” soul.

## Acceptance

The wind is useful only if, on the pre-registered held-out set, the full path (wind + heart + memory) beats the simpler baselines under matched compute. Otherwise keep the package as a candidate and iterate on the trajectories or the heart.
