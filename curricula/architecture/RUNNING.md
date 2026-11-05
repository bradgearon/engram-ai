# Run the architecture learning package

This package is included in the wds.ai v0.3 project under `curricula/architecture`. Use the complete project to run it; this archive contains curriculum data and original lesson sources, not a second runtime.

```sh
python tools/workshop.py start --train architecture
```

The launcher imports lessons, obtains native graded trajectories, trains with C++ ggml (or the small C++ fallback), verifies the existing Wind ABI, and compares candidate/parent/fixed-order policies on validation and locked tests. Checkpoints support the same command with `--resume` after a clean stop. Read the full project's `docs/WORKSHOP.md` for configuration and interruption recovery.

Inspect the reported candidate path and `runs/architecture/RUN_ID/report.json`. Activation is always a separate operator command at an episode boundary:

```sh
python tools/workshop.py activate --domain architecture --package packages/candidates/architecture/RUN_ID
```

All original Markdown files remain under `sources/`. `sources/PROVENANCE.json` records the supplied archive. `manifest.json` pins lesson and split hashes; `EVALUATION.md` defines the deliberately limited claims. The locked test must not be used to select optimizer parameters or update training labels.
