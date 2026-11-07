// Synthetic C ABI fixture. This does not evaluate weights or implement a model.
#include "llama.h"
#include "ggml-backend.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <algorithm>

static bool mode(const char *value) {
    const char *selected=std::getenv("WDS_PACK_TEST_CASE");
    return selected && std::strcmp(selected,value)==0;
}
// Compile independent libraries with the upstream b9870 export ownership. Do
// not link them together: dlsym must not find a wrongly selected owner's export
// through a dependency, which would hide a Windows GetProcAddress failure.
#if defined(WDS_FIXTURE_BASE)
const char *ggml_commit() { return mode("bad-abi") ? "deadbeef" : "2d973636"; }
const char *ggml_version() { return mode("null-version") ? nullptr : "0.15.3"; }
const char *ggml_backend_dev_name(ggml_backend_dev_t) { return "synthetic-device-fixture"; }
#elif defined(WDS_FIXTURE_REGISTRY)
void ggml_backend_load_all_from_path(const char *) {}
ggml_backend_dev_t ggml_backend_dev_by_type(enum ggml_backend_dev_type type) {
    static int device;
    return type==GGML_BACKEND_DEVICE_TYPE_GPU && !mode("gpu-fixture")
        ? nullptr : reinterpret_cast<ggml_backend_dev_t>(&device);
}
#elif defined(WDS_FIXTURE_MISSING)
const char *ggml_commit() { return "fixture-without-llama-symbols"; }
#else
struct llama_model {};
struct llama_vocab {};
struct llama_context { int next=0; bool embedding=false; float values[4]={1,2,3,4}; };
static llama_vocab vocab;
void llama_backend_init() {}
void llama_backend_free() {}
llama_model_params llama_model_default_params() { return {}; }
llama_context_params llama_context_default_params() { return {}; }
llama_model *llama_model_load_from_file(const char *,llama_model_params) {
    if(const auto *marker=std::getenv("WDS_PACK_TEST_MODEL_MARKER")) {
        if(auto *f=std::fopen(marker,"w")) { std::fputs("loaded",f); std::fclose(f); }
    }
    return new llama_model;
}
void llama_model_free(llama_model *model) { delete model; }
int32_t llama_model_meta_val_str(const llama_model *,const char *,char *out,size_t size) {
    if(size>=6)std::memcpy(out,"qwen3",6); return 5;
}
const char *llama_model_chat_template(const llama_model *,const char *) {
    return "{% for message in messages %}{{message['role']}}:{{message['content']}}\n{% endfor %}assistant:";
}
bool llama_model_has_encoder(const llama_model *) { return false; }
const llama_vocab *llama_model_get_vocab(const llama_model *) { return &vocab; }
int32_t llama_model_n_embd_out(const llama_model *) { return 4; }
int32_t llama_tokenize(const llama_vocab *,const char *text,int32_t size,llama_token *out,int32_t cap,bool special,bool) {
    const int n=size+(special?1:0);
    if(cap<n)return -n;
    int i=0;if(special)out[i++]=1;
    for(int j=0;j<size;++j)out[i++]=static_cast<unsigned char>(text[j])+3;
    return n;
}
llama_context *llama_init_from_model(llama_model *,llama_context_params cp) {
    if(cp.embeddings && (cp.pooling_type!=LLAMA_POOLING_TYPE_LAST || cp.n_batch!=2048 || cp.n_ubatch!=2048))return nullptr;
    auto *ctx=new llama_context;ctx->embedding=cp.embeddings;return ctx;
}
void llama_free(llama_context *ctx) { delete ctx; }
llama_memory_t llama_get_memory(const llama_context *ctx) { return reinterpret_cast<llama_memory_t>(const_cast<llama_context *>(ctx)); }
void llama_memory_clear(llama_memory_t mem,bool) { reinterpret_cast<llama_context *>(mem)->next=0; }
float *llama_get_embeddings_seq(llama_context *ctx,llama_seq_id id) {
    return ctx->embedding && id==0 && !mode("missing-output") ? ctx->values : nullptr;
}
const char *llama_vocab_get_text(const llama_vocab *,llama_token) { return ""; }
llama_token llama_vocab_bos(const llama_vocab *) { return 1; }
llama_token llama_vocab_eos(const llama_vocab *) { return 2; }
bool llama_vocab_is_eog(const llama_vocab *,llama_token id) { return id==2; }
llama_sampler *llama_sampler_init_greedy() { return new llama_sampler{}; }
void llama_sampler_free(llama_sampler *s) { delete s; }
llama_token llama_sampler_sample(llama_sampler *,llama_context *,int32_t) { return mode("limit")?3:2; }
int32_t llama_token_to_piece(const llama_vocab *,llama_token,char *out,int32_t size,int32_t,bool) {
    if(size<1)return -1;out[0]='x';return 1;
}
llama_batch llama_batch_init(int32_t capacity,int32_t,int32_t) {
    llama_batch b{};
    b.token=new llama_token[capacity];b.pos=new llama_pos[capacity];b.n_seq_id=new int32_t[capacity];
    b.seq_id=new llama_seq_id*[capacity+1]{};b.logits=new int8_t[capacity];
    for(int i=0;i<capacity;++i)b.seq_id[i]=new llama_seq_id[1];
    return b;
}
void llama_batch_free(llama_batch b) {
    for(int i=0;b.seq_id && b.seq_id[i];++i)delete[] b.seq_id[i];
    delete[] b.seq_id;delete[] b.token;delete[] b.pos;delete[] b.n_seq_id;delete[] b.logits;
}
int32_t llama_decode(llama_context *ctx,llama_batch b) {
    if(mode("decode-fails"))return -1;
    if(b.n_tokens<1)return -1;
    for(int i=0;i<b.n_tokens;++i) {
        if(b.pos[i]!=ctx->next+i || b.n_seq_id[i]!=1 || b.seq_id[i][0]!=0 || b.logits[i]!=(i==b.n_tokens-1))return -1;
    }
    ctx->next+=b.n_tokens;return 0;
}
#endif
