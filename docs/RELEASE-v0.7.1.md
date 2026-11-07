# v0.7.1 — reuse the Six Stars native runtime pack

The previous launcher always built its own llama/GGML backend. On the reported CUDA 13.3 installation, compiler probing failed on `compute_50` before WDS could start. This update adds direct use of the existing b9870 native runtime pack.

- `--runtime-pack DIR` on `workshop.py ... start` retains the configured GGUF and application data, saves the selection and compiles the app into `build-native-pack` without enabling CUDA or fetching llama.cpp.
- The native loader checks b9870's GGML version/commit and required symbols before loading model parameters. Positive GPU-layer requests require a registered GPU; mismatches and missing dependencies produce explicit errors.
- The existing Qwen representation, context reset, scope cache, chat template and explicit generation-limit behavior are adapted to b9870's batch API. Current source builds remain available.
- The pack supplies GPU model computation. Wind optimization uses the existing small native C++ scalar implementation, retaining the same package ABI and activation rules.
- Source CUDA builds now explicitly select `CMAKE_CUDA_ARCHITECTURES=native` unless configured otherwise. This prevents the launcher's default path from retaining an obsolete target list.
- Startup diagnostics are surfaced in the terminal as well as logs. The launcher builds only the application and trainer targets. The web UI version and bundled assets identify this update as 0.7.1.

This is a runtime integration fix on top of v0.7, not a new model or neuronal architecture milestone. Existing projects, chats, journals and candidates stay in their configured paths. See [RUNTIME_PACKS.md](RUNTIME_PACKS.md) for the exact startup command and [VALIDATION-v0.7.1.md](VALIDATION-v0.7.1.md) for test evidence and limitations.
