# v0.5.1 — Six Stars setup clarification

This patch keeps the v0.5 web UI and runtime behavior and repairs the setup handoff.

- Start with `python tools/workshop.py start --six-stars "U:\six-stars-solution-2" --ui --upkeep`; no pre-existing WDS config is required.
- Accept the install folder, its `config` folder, or the model/runtime manifest path.
- Distinguish Six Stars' manifests from WDS's generated configuration, with actionable missing/wrong-file errors.
- Read model paths from the supplied `models.json`; follow Windows models.lnk targets when necessary, or accept an explicit model directory/file.
- Report the runtime manifest's executable location and the runtime-packs alternative. These remain diagnostics: WDS uses its own source-built llama.cpp, not those prebuilt server binaries.
- Reuse a matching WDS configuration without rewriting existing data paths or GPU settings. Require explicit replacement for mismatched model/backend choices.
- Add supplied-manifest fixtures and setup tests; explain generation timing in LLM_FLOW.md.

No GGUF weights, Wind ABI, activation behavior, or existing conversation data are changed. Actual Windows COM shortcut resolution and inference on your GPU require validation on that machine; path joining and launch configuration are tested with fixtures here.
