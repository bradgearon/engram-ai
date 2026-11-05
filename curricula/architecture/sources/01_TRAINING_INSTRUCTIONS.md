# Training Instructions — Architecture & Design Wind

## Goal

Produce a small, inspectable policy or scoring package that improves held-out architecture and design tasks (boundary decisions, pattern selection, trade-off articulation) after practice on the materials in this package.

## Step 0 — Lock held-out tasks first

Examples:
- “Given these requirements (latency budget, consistency need, team boundaries), choose and justify sync vs event-driven vs actor; name the patterns and the risks.”
- “Refactor this description of a distributed monolith into clearer bounded contexts; list the integration events.”
- “Identify the missing idempotency, retry, or supervision boundary in this event flow.”
- “Apply ports-and-adapters: what is the core domain, what are the adapters, what must not leak?”

Score on:
- Correct identification of constraints
- Appropriate pattern choice with explicit trade-off
- Clean boundary language
- Absence of common anti-patterns

Do not train on the held-out set.

## Step 1 — Practice trajectories from the distilled files

Turn each extracted principle into short scenarios:
- input constraints + partial design
- critique (what is weak)
- improved design note that names the pattern and the boundary

## Step 2 — External enrichment

Feed selected chapters or articles from the URL list only after the held-out set exists. Ask for additional graded exercises and counter-examples, not free-form redesigns that ignore the constraints.

## Step 3 — Acceptance

The package is useful only if, on the pre-registered held-out tasks, the wind-assisted path produces better boundary decisions or clearer trade-off statements than one-shot or simple iterative refinement under matched effort.

## Package suggestion

```
packages/architecture-candidate-N/
  manifest.json
  policy.wind          # or a structured scoring/artifact format you define
  LEARNING_REPORT.md
```
