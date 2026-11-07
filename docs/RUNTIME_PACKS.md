# Use the existing b9870 native runtime

v0.7.1 adds a native library adapter for the pack shown at:

```text
U:\six-stars-solution-2\runtime-packs\win-x64\llama-cpp-dynamic-win-x64
```

The screenshot contains `llama-b9870-bin-win-cuda-12.4-x64.zip`, `llama.dll`, `ggml.dll`, `ggml-base.dll`, `ggml-cuda.dll`, CPU backend variants and CUDA 12 dependency DLLs. The actual loaded libraries are checked at startup; the archive filename alone is not treated as proof of compatibility.

v0.7.2 corrects the device-name lookup to use `ggml-base.dll`. If v0.7.1 reports a missing `ggml_backend_dev_name`, update the application files; keep your current config and native pack.

## Update your existing installation

Stop the earlier launcher. Extract the new archive's `wds-ai` contents into your existing `U:\wds-ai` application folder. Keep `configs/sixstars.local.json`, your data and runs; the release does not contain those local files. Start in Developer PowerShell for Visual Studio 2022:

```powershell
cd U:\wds-ai
python tools/workshop.py --config configs/sixstars.local.json start --runtime-pack "U:\six-stars-solution-2\runtime-packs\win-x64\llama-cpp-dynamic-win-x64" --ui
```

The launcher preserves the selected GGUF, data, runs, package directories, initial package, ports and GPU layer count. It saves the runtime pack path, selects a separate `build-native-pack` directory, disables source llama/GGML/CUDA builds and selects the small C++ scalar Wind optimizer. It leaves `build-sixstars` in place. The first launch compiles WDS and its template helper with your ordinary C++20 compiler. It does not fetch llama.cpp, invoke `nvcc`, launch `llama-server.exe` or load an additional Python inference framework. It serves the included React bundle when its source fingerprint matches.

Open **http://127.0.0.1:8787**. Later:

```powershell
python tools/workshop.py --config configs/sixstars.local.json start --ui
```

Add `--web` for bounded web evidence or `--train music,architecture` to start the supplied curricula. Append `--resume` for interrupted curriculum checkpoints. Training writes candidates with the existing manifest and checksummed `policy.wind`; activation is still an explicit episode-boundary command or UI action.

This switch retains existing data. Foundation identities now include the pack and GGUF fingerprints. Old in-flight model episodes cannot be resumed under a different identity; finish/cancel them under their original backend or start a new episode. Historical records remain attributed to the backend that produced them.

## Compatibility boundary

| Part | Required behavior |
|---|---|
| Native API | Exact llama.cpp b9870 public ABI; commit `2d973636e292ee6f75fadcf08d29cb33511f509f`, GGML 0.15.3 |
| Windows process | x64 application and x64 DLLs from one complete matching pack |
| Model | Existing GGUF supported by that pack; neuronal representations currently support dense Qwen3 |
| Dependencies | Keep llama/GGML, CPU backend, CUDA backend and CUDA runtime dependency DLLs together |
| GPU offload | `gpu_layers > 0`; startup rejects a pack that registers no GPU device |
| CPU choice | Explicit `gpu_layers: 0`; CPU backend still required in the pack |
| Wind optimizer | Small C++ scalar SGD; it does not use the external pack's optimizer ABI |
| Package activation | Separate explicit action, unchanged |

In the WDS config, `cuda: false` means **do not build CUDA**. It does not disable GPU inference in pack mode. The separate `gpu_layers` setting is retained from your original config (normally 99 from your CUDA setup). To change it on an existing installation, edit that field explicitly. New setup with `--runtime-pack` defaults to 99, or accepts `--gpu-layers 0` for CPU.

The loader checks the exported GGML commit/version and all required symbols before passing model/context parameter structures across the native API. It rejects mismatched or unidentifiable versions instead of guessing. Keep `llama.dll` and GGML from the same release: GGML's version cannot independently authenticate an unrelated replacement `llama.dll`. Do not mix individual DLLs from different packs. Another build needs a matching adapter or the source-build path.

On Windows the adapter adds the selected directory to DLL dependency search and registers GGML backend plugins from that directory. It uses the pack's compiled CUDA kernels and the installed NVIDIA driver. A separately installed CUDA 13.3 compiler is not involved. Successful loading reports `Native runtime pack:` and, when requested, `Native GPU backend:` in the runtime log.

Neuronal semantic requests still execute compatible Qwen model computations; this change is how the native libraries are supplied. It does not implement direct retrieval of facts from isolated weights. Native engram expression remains the default chat path; the existing AI host mode separately permits generated text.

## Diagnose startup

Startup failures are printed in the launcher and retained in `<runs>/logs/runtime.log` (for the standard Six Stars config: `sixstars-wds-runs/logs/runtime.log`).

- **ABI mismatch:** verify the whole folder comes from the b9870 release. The detected commit/version is included in the error. There is no automatic source-build fallback.
- **Missing required symbol:** restore a complete matching pack; a newer or older DLL may have a different API.
- **Cannot load runtime library:** keep all dependent DLLs together and use x64 Developer PowerShell. The Windows error number is retained; error 126 can refer to a dependent DLL, while error 193 can indicate the wrong architecture.
- **No GPU device:** inspect the same log for GGML/CUDA initialization errors. Check the existing pack and driver. WDS does not silently select CPU when positive GPU layers were requested.
- **Missing GGUF:** correct `model` in the existing config. The runtime directory contains executable code, not your model weights.
- **Still invoking nvcc:** verify the extracted files show v0.7.2, you used the new `--runtime-pack` option and the config now selects `build-native-pack`.
- **Long model loading:** startup has an explicit 180-second timeout and retains the model log. Model identity hashing also reads the GGUF once at startup.

## Source-build alternative

You can still compile the existing pinned source backend. Use a separate config/build directory and remove the `runtime_pack` field from that config. Set `llama`, `ggml` and `cuda` true, with `gpu_layers: 99`. The launcher now supplies `CMAKE_CUDA_ARCHITECTURES=native` by default for source CUDA builds. CMake 3.24+ is required for `native`; a `cuda_architectures` config value can explicitly select compatible targets.

For manual CMake use:

```powershell
cmake -S . -B build-source-gpu -DWDS_LLAMA=ON -DWDS_GGML_TRAIN=ON -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=native
cmake --build build-source-gpu --config Release --target wds-ai wds-train -j 4
```

Your original error passed `compute_50` to CUDA 13.3. That obsolete target is incompatible with CUDA 13's removed pre-Turing compilation support. Pack mode bypasses that compiler entirely. References: [NVIDIA CUDA 13 release notes](https://docs.nvidia.com/cuda/archive/13.0.0/cuda-toolkit-release-notes/index.html), [CMake CUDA architectures](https://cmake.org/cmake/help/latest/prop_tgt/CUDA_ARCHITECTURES.html).

See [VALIDATION-v0.7.2.md](VALIDATION-v0.7.2.md) for the tested scope. The Windows DLL/GPU path needs confirmation on your machine; Linux synthetic-library tests are not a GPU benchmark.
