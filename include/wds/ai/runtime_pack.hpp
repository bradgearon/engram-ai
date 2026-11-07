#pragma once
#include "llama.h"
#include <string>
namespace wds::ai::runtime_pack {
struct Api {
    decltype(&::llama_backend_init) llama_backend_init = nullptr;
    decltype(&::llama_backend_free) llama_backend_free = nullptr;
    decltype(&::llama_model_default_params) llama_model_default_params = nullptr;
    decltype(&::llama_model_load_from_file) llama_model_load_from_file = nullptr;
    decltype(&::llama_model_free) llama_model_free = nullptr;
    decltype(&::llama_model_meta_val_str) llama_model_meta_val_str = nullptr;
    decltype(&::llama_model_chat_template) llama_model_chat_template = nullptr;
    decltype(&::llama_model_has_encoder) llama_model_has_encoder = nullptr;
    decltype(&::llama_model_get_vocab) llama_model_get_vocab = nullptr;
    decltype(&::llama_tokenize) llama_tokenize = nullptr;
    decltype(&::llama_context_default_params) llama_context_default_params = nullptr;
    decltype(&::llama_init_from_model) llama_init_from_model = nullptr;
    decltype(&::llama_free) llama_free = nullptr;
    decltype(&::llama_memory_clear) llama_memory_clear = nullptr;
    decltype(&::llama_get_memory) llama_get_memory = nullptr;
    decltype(&::llama_get_embeddings_seq) llama_get_embeddings_seq = nullptr;
    decltype(&::llama_model_n_embd_out) llama_model_n_embd_out = nullptr;
    decltype(&::llama_vocab_get_text) llama_vocab_get_text = nullptr;
    decltype(&::llama_vocab_bos) llama_vocab_bos = nullptr;
    decltype(&::llama_vocab_eos) llama_vocab_eos = nullptr;
    decltype(&::llama_vocab_is_eog) llama_vocab_is_eog = nullptr;
    decltype(&::llama_sampler_init_greedy) llama_sampler_init_greedy = nullptr;
    decltype(&::llama_sampler_free) llama_sampler_free = nullptr;
    decltype(&::llama_sampler_sample) llama_sampler_sample = nullptr;
    decltype(&::llama_token_to_piece) llama_token_to_piece = nullptr;
    decltype(&::llama_batch_init) llama_batch_init = nullptr;
    decltype(&::llama_batch_free) llama_batch_free = nullptr;
    decltype(&::llama_decode) llama_decode = nullptr;
    decltype(&::ggml_backend_load_all_from_path) ggml_backend_load_all_from_path = nullptr;
    decltype(&::ggml_backend_dev_by_type) ggml_backend_dev_by_type = nullptr;
    decltype(&::ggml_backend_dev_name) ggml_backend_dev_name = nullptr;
    decltype(&::ggml_commit) ggml_commit = nullptr;
    decltype(&::ggml_version) ggml_version = nullptr;
};
Api &api();
void load(const std::string &directory, bool require_gpu);
std::string identity();
} // namespace wds::ai::runtime_pack

#ifdef WDS_RUNTIME_PACK_REMAP
#define llama_backend_init wds::ai::runtime_pack::api().llama_backend_init
#define llama_backend_free wds::ai::runtime_pack::api().llama_backend_free
#define llama_model_default_params wds::ai::runtime_pack::api().llama_model_default_params
#define llama_model_load_from_file wds::ai::runtime_pack::api().llama_model_load_from_file
#define llama_model_free wds::ai::runtime_pack::api().llama_model_free
#define llama_model_meta_val_str wds::ai::runtime_pack::api().llama_model_meta_val_str
#define llama_model_chat_template wds::ai::runtime_pack::api().llama_model_chat_template
#define llama_model_has_encoder wds::ai::runtime_pack::api().llama_model_has_encoder
#define llama_model_get_vocab wds::ai::runtime_pack::api().llama_model_get_vocab
#define llama_tokenize wds::ai::runtime_pack::api().llama_tokenize
#define llama_context_default_params wds::ai::runtime_pack::api().llama_context_default_params
#define llama_init_from_model wds::ai::runtime_pack::api().llama_init_from_model
#define llama_free wds::ai::runtime_pack::api().llama_free
#define llama_memory_clear wds::ai::runtime_pack::api().llama_memory_clear
#define llama_get_memory wds::ai::runtime_pack::api().llama_get_memory
#define llama_get_embeddings_seq wds::ai::runtime_pack::api().llama_get_embeddings_seq
#define llama_model_n_embd_out wds::ai::runtime_pack::api().llama_model_n_embd_out
#define llama_vocab_get_text wds::ai::runtime_pack::api().llama_vocab_get_text
#define llama_vocab_bos wds::ai::runtime_pack::api().llama_vocab_bos
#define llama_vocab_eos wds::ai::runtime_pack::api().llama_vocab_eos
#define llama_vocab_is_eog wds::ai::runtime_pack::api().llama_vocab_is_eog
#define llama_sampler_init_greedy wds::ai::runtime_pack::api().llama_sampler_init_greedy
#define llama_sampler_free wds::ai::runtime_pack::api().llama_sampler_free
#define llama_sampler_sample wds::ai::runtime_pack::api().llama_sampler_sample
#define llama_token_to_piece wds::ai::runtime_pack::api().llama_token_to_piece
#define llama_batch_init wds::ai::runtime_pack::api().llama_batch_init
#define llama_batch_free wds::ai::runtime_pack::api().llama_batch_free
#define llama_decode wds::ai::runtime_pack::api().llama_decode
#endif
