# v0.4 execution evidence

These reports were generated from the v0.4 implementation with the native CPU ggml trainer and the explicit demo foundation. `verification.json` records successful scalar/native suites, original-v0.2 candidate loading, restart/resume counts and workshop/upkeep isolation. `upkeep.json` records the repeated-request and later-practice fixtures. The full CTest logs are included.

Both current candidate directories referenced by the reports are included in the ZIP; none is automatically active. The practice fixture explicitly selected a music candidate in its temporary store. The two older sample packages and `examples/verification/` remain historical v0.3 evidence. Run IDs depend on source/binary/configuration hashes; builds on another computer will generate new IDs.

No real GGUF, GPU performance, microphone/TTS or Windows execution claim follows from these tests. Native Gemma 4 template rendering was checked with the pinned upstream template, without loading model weights.
