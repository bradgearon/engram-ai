# v0.7.1 validation

**Superseded loader coverage:** the v0.7.1 synthetic fixture exported all symbols from each library, so it did not catch the Windows device-name lookup bug. v0.7.2 uses physically separate export sets, reproduces the reported failure and fixes the lookup. See [VALIDATION-v0.7.2.md](VALIDATION-v0.7.2.md). The results below are retained as the historical record of what ran.

Results are retained in `examples/verification-v071/`.

| Check | Result |
|---|---|
| Debug runtime-pack build on Linux | 18/18 CTest tests passed |
| Release source llama.cpp / GGML CPU build | 18/18 CTest tests passed |
| React production bundle | Built as 0.7.1; matching source fingerprint included |
| Runtime-pack CMake cache | CUDA compiler absent; source llama and GGML trainer disabled |
| Browser / JavaScript UI tests | Not run, as requested |

The runtime-pack suite uses **synthetic shared libraries with the exact b9870 C declarations**. It verifies dynamic loading, struct/batch handling, context reset, scoped cache, representation provenance, explicit input limits and retained partial generated output. It rejects the wrong commit, null version, missing library/symbol, requested GPU without a device, failed decode and missing representation output. ABI/GPU failures are checked before a model-loading marker can be written. A real WDS child process is also launched against an incompatible synthetic pack to verify that the Python supervisor surfaces the native error and retains it in the log.

The setup test switches an existing configuration to pack mode while preserving its model, data, run, package and port settings. It checks missing backend diagnostics, a relocated pack path and actual launcher CMake argument lists: pack mode disables source/CUDA builds; source CUDA mode requests `native` architectures.

The source-build suite includes the existing tiny random-weight Qwen3 GGUF test. It executes the current pinned native graph to check the shared adapter refactor. It does **not** execute b9870 or test pretrained Qwen3 4B knowledge. The two suites also cover the existing engram, Wind ABI/activation, journal restart, training and WebSocket behaviors. The interface WebSocket test is a backend-relay test, not a UI test.

No Windows execution environment, real b9870 binary pack, NVIDIA GPU or pretrained 4B GGUF was available in this workspace. Windows DLL loading, driver compatibility, model offload, CUDA throughput and actual Qwen3 4B accuracy remain unverified locally. The adapter checks the detected runtime at startup and reports failures; synthetic fixtures are not evidence of a successful real-model GPU run.

Reproduce the pack suite without CUDA or model downloads:

```sh
cmake -S . -B build-pack -DWDS_RUNTIME_PACK=ON -DWDS_LLAMA=OFF -DWDS_GGML_TRAIN=OFF -DGGML_CUDA=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-pack --config Debug -j 3
python -m pip install -r tools/requirements.txt
ctest --test-dir build-pack -C Debug --output-on-failure
```

For real model loading, use the separate [runtime-pack startup command](RUNTIME_PACKS.md). Test fixture libraries must never be substituted for an inference runtime.
