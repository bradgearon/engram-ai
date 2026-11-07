# v0.7.2 — correct native DLL symbol ownership

The Windows build completed, but startup failed with `runtime pack is missing required b9870 symbol: ggml_backend_dev_name`. The loader requested this device accessor from `ggml.dll`; b9870 exports it from `ggml-base.dll`. The registry functions remain in `ggml.dll`.

The loader now resolves each function from its correct library. This does not change the selected GGUF, pack, GPU offload settings, Wind ABI, active packages or existing data. The C4324 padding warning is caused by the intentionally 64-byte-aligned Wind policy; it is unrelated to the startup failure.

The earlier synthetic fixture incorrectly exported every function from every library, masking the Windows error. The regression now builds independent base, registry and model fixtures. That test reproduced the exact reported error before the fix. It also exercises device-name lookup with a synthetic device after the fix, while retaining the explicit missing-GPU and ABI-failure checks. No physical GPU is involved in this test.

## Apply

Extract the updated `wds-ai` application files over the existing installation. Keep your local config and data. Run:

```powershell
cd U:\wds-ai
python tools/workshop.py --config configs/sixstars.local.json start --ui
```

The launcher detects the changed source and rebuilds the application. No DLL replacement, CUDA compilation or build-directory deletion is required. The included web bundle and server identify this fix as 0.7.2.

Exact upstream ownership was checked against llama.cpp b9870:

- [GGML CMake targets](https://github.com/ggml-org/llama.cpp/blob/b9870/ggml/src/CMakeLists.txt): `ggml-backend.cpp` belongs to `ggml-base`; `ggml-backend-reg.cpp` belongs to `ggml`.
- [Device accessors](https://github.com/ggml-org/llama.cpp/blob/b9870/ggml/src/ggml-backend.cpp): definition of `ggml_backend_dev_name`.
- [Backend registry](https://github.com/ggml-org/llama.cpp/blob/b9870/ggml/src/ggml-backend-reg.cpp): device selection and backend loading.

See [VALIDATION-v0.7.2.md](VALIDATION-v0.7.2.md) for reproduced failure and verification scope.
