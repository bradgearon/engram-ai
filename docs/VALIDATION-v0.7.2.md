# v0.7.2 validation

The native loader regression is documented in `examples/verification-v072/`.

| Check | Result |
|---|---|
| Correctly split fixture libraries with the v0.7.1 loader | Reproduced the reported `ggml_backend_dev_name` failure |
| Runtime-pack build after the fix | 18/18 CTest tests passed on Linux |
| React production bundle | Built as 0.7.2 |
| Browser / JavaScript UI tests | Not run, as requested |
| Actual Windows DLLs / NVIDIA GPU | Not available in this workspace |

`regression-before-fix.txt` retains the expected failing test. `pack-ctest.txt` records the passing suite after the fix. Base, registry and llama fixtures now have disjoint exports matching the used b9870 API functions. They are independent libraries so Linux dependency lookup cannot hide an incorrect Windows `GetProcAddress` module selection. CMake exports only the functions each fixture actually implements on Windows.

The test checks device naming through a synthetic GPU device as well as explicit rejection when no GPU is registered. It retains tests for the wrong commit, null version, missing libraries/symbols, failed decoding, missing representation output, context reset, scoped caching and limits. A real WDS child startup failure is checked through the supervisor and its log. These fixtures do not perform real model inference.

The rest of the runtime-pack suite covers native training, checksummed Wind artifacts and explicit activation, engram assembly, persistence, scope, upkeep, setup and backend WebSocket behavior. The source-built model backend was not changed by this DLL lookup fix; its last full validation is recorded under v0.7.1.

The supplied user log confirms that v0.7.1 compiled with MSVC and reached library symbol resolution. It does not establish successful GPU or model startup. The v0.7.2 Windows execution path still needs a local run on the user's machine.

Reproduce:

```sh
cmake -S . -B build-pack -DWDS_RUNTIME_PACK=ON -DWDS_LLAMA=OFF -DWDS_GGML_TRAIN=OFF -DGGML_CUDA=OFF -DCMAKE_BUILD_TYPE=Debug
cmake --build build-pack --config Debug -j 3
ctest --test-dir build-pack -C Debug --output-on-failure
```
