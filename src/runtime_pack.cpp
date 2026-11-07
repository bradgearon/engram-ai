#include "wds/ai/core.hpp"
#include "wds/ai/runtime_pack.hpp"
#include <fstream>
#include <iostream>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace wds::ai::runtime_pack {
namespace {
Api functions;
std::string loaded_path, loaded_identity;
// The modules stay loaded until process exit; GGML owns backend registrations.
std::vector<void *> modules;
void *module(const fs::path &path) {
#ifdef _WIN32
    auto h = LoadLibraryExW(path.wstring().c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    require(h != nullptr, "cannot load runtime library " + path.string() + " (Windows error " + std::to_string(GetLastError()) + "); keep the complete pack and its dependent DLLs together");
    auto p = reinterpret_cast<void *>(h);
#else
    auto p = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
    if (!p) { const char *error = dlerror(); throw std::runtime_error("cannot load runtime library " + path.string() + ": " + (error ? error : "unknown loader error")); }
#endif
    modules.push_back(p); return p;
}
template<class T> void symbol(void *handle, T &target, const char *name) {
#ifdef _WIN32
    target = reinterpret_cast<T>(GetProcAddress(static_cast<HMODULE>(handle), name));
#else
    target = reinterpret_cast<T>(dlsym(handle, name));
#endif
    require(target != nullptr, "runtime pack is missing required b9870 symbol: " + std::string(name));
}
std::string digest(const fs::path &path) {
    std::ifstream in(path, std::ios::binary); require(bool(in), "cannot read runtime identity");
    uint64_t h = 14695981039346656037ull; char buffer[65536];
    while (in) { in.read(buffer, sizeof(buffer)); for (std::streamsize i=0; i<in.gcount(); ++i) { h ^= uint8_t(buffer[i]); h *= 1099511628211ull; } }
    require(in.eof(), "runtime identity read failed"); return std::to_string(h);
}
}
Api &api() { return functions; }
std::string identity() { return loaded_identity; }
void load(const std::string &directory, bool require_gpu) {
    require(!directory.empty(), "this executable needs --runtime-pack DIR containing the b9870 native libraries");
    const auto path = fs::canonical(directory);
    require(loaded_path.empty() || loaded_path == path.string(), "cannot mix native runtime packs in one process");
    if (loaded_path.empty()) {
#ifdef _WIN32
        require(SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS) != 0, "cannot configure native DLL search");
        require(AddDllDirectory(path.wstring().c_str()) != nullptr, "cannot add native runtime directory");
        auto base_path = path / "ggml-base.dll", ggml_path = path / "ggml.dll", llama_path = path / "llama.dll";
#elif defined(__APPLE__)
        auto base_path = path / "libggml-base.dylib", ggml_path = path / "libggml.dylib", llama_path = path / "libllama.dylib";
#else
        auto base_path = path / "libggml-base.so", ggml_path = path / "libggml.so", llama_path = path / "libllama.so";
#endif
        auto base = module(base_path);
        symbol(base, functions.ggml_commit, "ggml_commit"); symbol(base, functions.ggml_version, "ggml_version");
        const auto *commit_text = functions.ggml_commit(), *version_text = functions.ggml_version();
        const std::string commit = commit_text ? commit_text : "", version = version_text ? version_text : "";
        const std::string expected = "2d973636e292ee6f75fadcf08d29cb33511f509f";
        require(commit.size() >= 7 && expected.rfind(commit, 0) == 0 && version == "0.15.3",
                "runtime ABI mismatch: this adapter requires clean llama.cpp b9870 (2d973636, GGML 0.15.3); found GGML " + version + " commit " + commit + ". Keep the matching pack together; no source build fallback was started");
        auto ggml = module(ggml_path), llama = module(llama_path);
#define BIND(name) symbol(llama, functions.name, #name)
        BIND(llama_backend_init); BIND(llama_backend_free); BIND(llama_model_default_params);
        BIND(llama_model_load_from_file); BIND(llama_model_free); BIND(llama_model_meta_val_str);
        BIND(llama_model_chat_template); BIND(llama_model_has_encoder); BIND(llama_model_get_vocab);
        BIND(llama_tokenize); BIND(llama_context_default_params); BIND(llama_init_from_model);
        BIND(llama_free); BIND(llama_memory_clear); BIND(llama_get_memory); BIND(llama_get_embeddings_seq);
        BIND(llama_model_n_embd_out); BIND(llama_vocab_get_text); BIND(llama_vocab_bos); BIND(llama_vocab_eos);
        BIND(llama_vocab_is_eog); BIND(llama_sampler_init_greedy); BIND(llama_sampler_free);
        BIND(llama_sampler_sample); BIND(llama_token_to_piece); BIND(llama_batch_init);
        BIND(llama_batch_free); BIND(llama_decode);
#undef BIND
#define BIND(name) symbol(ggml, functions.name, #name)
        BIND(ggml_backend_load_all_from_path); BIND(ggml_backend_dev_by_type);
#undef BIND
        // Device accessors live in ggml-base; the registry lives in ggml.
        // Windows GetProcAddress does not search the module's dependencies.
        symbol(base, functions.ggml_backend_dev_name, "ggml_backend_dev_name");
        functions.ggml_backend_load_all_from_path(path.string().c_str());
        loaded_identity = "llama.cpp/b9870:lib-fnv64:" + digest(llama_path) + ":ggml-fnv64:" + digest(base_path);
        loaded_path = path.string();
        std::cerr << "Native runtime pack: " << loaded_path << " (b9870; no CUDA compilation)\n";
    }
    if (require_gpu) {
        auto device = functions.ggml_backend_dev_by_type(GGML_BACKEND_DEVICE_TYPE_GPU);
        require(device != nullptr, "GPU offload requested but the prebuilt pack registered no GPU device; inspect its CUDA DLL dependencies and driver. CPU fallback was not selected");
        std::cerr << "Native GPU backend: " << functions.ggml_backend_dev_name(device) << '\n';
    }
}
} // namespace wds::ai::runtime_pack
