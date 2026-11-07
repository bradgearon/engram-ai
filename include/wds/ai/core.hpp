#pragma once
#include "wds/ai/heart.hpp"
#include "wds/ai/relationships.hpp"
#include "wds/ai/task.hpp"
#include "wds/ai/tasks.hpp"
#include "wds/ai/types.hpp"
#include "wds/ai/upkeep.hpp"
#include "wds/ai/neural.hpp"
#include "wds/ai/expression.hpp"
#include "wds/ai/cognition.hpp"
#include "wds/ai/language.hpp"
namespace wds::ai {
namespace soul {
struct alignas(32) Atom {
    uint64_t identity = 0, root = 0, source_sequence = 0;
    float weight = 0.1f;
    uint32_t flags = 0;
};
static_assert(sizeof(Atom) == 32, "small soul identity lane");
} // namespace soul
namespace chain {
class Store {
    fs::path directory_;
    std::string tail_ = "0000000000000000";
    uint64_t seq_ = 0;
    bool failed_ = false;
    uint64_t journal_bytes_ = 0;
    std::map<std::string, std::vector<std::pair<uint64_t, uint64_t>>> event_offsets_;
    void apply(const json &e);
    std::map<std::string, json> souls_, voice_interests_, declarations_;

  public:
    std::map<std::string, json> cognitive_states, cognitive_results, language_reviews;
    std::map<std::string, json> projects, chats, chat_messages, workspace_requests;
    std::map<std::string, json> mesh_neurons, mesh_routes, mesh_pulses, mesh_samples, mesh_requests, mesh_web;
    std::map<std::string, json> assemblies, expression_samples, assembly_requests;
    std::map<std::string, std::string> assembly_by_lineage;
    std::map<std::string, std::set<std::string>> pulses_by_lineage, assemblies_by_root;
    std::set<std::pair<int64_t, std::string>> assembly_due;
    std::map<std::string, std::set<std::string>> mesh_nodes_by_root, mesh_edges_by_node;
    std::set<std::pair<int64_t, std::string>> mesh_due, mesh_web_due;
    std::set<std::string> mesh_running;
    json mesh_settings = neural::default_settings();
    json mesh_usage = {{"window_start_ms", 0}, {"impulses", 0}, {"knowledge_calls", 0}, {"web_queries", 0}};
    std::map<std::string, std::vector<std::string>> messages_by_chat;
    json provenance(const std::string &, uint64_t before, size_t limit) const;
    std::map<std::string, json> roots, engrams, instances, packages, active_packages, activation_requests;
    std::set<std::string> packet_ids, request_ids;
    std::map<std::string, json> neurons, signals, reviews, materials, upkeep_requests, flesh, scopes, ideas,
        profiles, preferences;
    std::map<std::string, soul::Atom> soul_atoms;
    std::map<std::string, json> soul_weights;
    std::map<std::string, std::set<std::string>> memory_index, reviews_by_memory, reviews_by_neuron,
        ideas_by_root;
    std::map<std::string, std::set<std::pair<uint64_t, std::string>>> learning_index;
    std::map<std::string, uint64_t> instance_sequences;
    std::set<std::pair<int64_t, std::string>> review_due, material_due;
    std::set<std::string> active_instances, waiting_reviews, running_reviews;
    std::map<std::string, std::string> episode_by_trace;
    std::set<std::pair<uint64_t, std::string>> pending_signals;
    std::map<std::string, uint64_t> memory_generations;
    json upkeep_settings = upkeep::default_settings();
    json upkeep_usage = {{"window_start_ms", 0}, {"episodes", 0}, {"foundation_calls", 0}, {"fetches", 0}};
    explicit Store(fs::path directory);
    ~Store();
    Store(const Store &) = delete;
    Store &operator=(const Store &) = delete;
    json append(const std::string &type, const std::string &root, const std::string &trace, const json &data);
    const auto &souls() const {
        return souls_;
    }
    const auto &voice_interests() const {
        return voice_interests_;
    }
    const auto &declarations() const {
        return declarations_;
    }
    uint64_t sequence() const {
        return seq_;
    }
};
} // namespace chain
namespace roots {
json select(chain::Store &store, const std::string &id, const std::string &purpose, const std::string &trace);
}
namespace mesh {
// Fixed, 64-byte aligned numeric payload. Metadata/provenance live outside GPU lanes.
struct alignas(64) RnaTile {
    Embedding key{};
    std::array<float, 16> features{};
    std::array<float, 16> reserved{};
};
static_assert(sizeof(RnaTile) == 256, "RNA tile ABI");
json prepare_memory(const chain::Store &, const json &packet, const std::string &trace);
json ingest(chain::Store &, const json &packet, const std::string &trace);
std::vector<json> retrieve(const chain::Store &, const std::string &root, const Embedding &query,
                           bool locations_only = false);
} // namespace mesh
namespace understanding {
struct alignas(64) AttentionBatch {
    std::array<float, max_candidates * dim> keys{};
    std::array<float, max_candidates> biases{};
    size_t count = 0;
};
struct Focus {
    std::vector<json> candidates;
    std::vector<float> scores, attention;
    Embedding pooled{};
};
Focus attend(std::vector<json> candidates, const Embedding &query, bool uniform = false, bool routes = true);
} // namespace understanding
namespace winds {
struct alignas(64) Policy {
    std::array<float, 256> weight{};
    State bias{};
};
class Package {
  public:
    json manifest;
    Policy policy;
    std::shared_ptr<const language::Model> sequence;
    std::string digest;
    explicit Package(const fs::path &directory);
    State step(const State &z, const Embedding &active, float feedback, bool frozen = false) const;
};
} // namespace winds
namespace understanding {
// A neuron owns attention and forms an impulse from an immutable scoped signal.
// It has no Store, writer callback, scheduler or activation capability.
struct Signal {
    std::string root_id;
    Embedding query{};
    std::vector<json> engrams;
    State previous{};
    float feedback = 0;
    bool uniform_attention = false, use_routes = true, freeze_motion = false;
    std::vector<std::string> read_roots;
};
struct Impulse {
    Focus focus;
    State state{};
};
class Neuron {
  public:
    Impulse receive(const Signal &, const winds::Package &) const;
    upkeep::Decision decide(const upkeep::GateInput &) const;
};
} // namespace understanding
namespace intelligence {
using Token = std::function<void(const std::string &)>;
struct Request {
    std::string seed, previous, feedback;
    std::vector<json> memories;
    State state{};
    int iteration = 0;
    json relationship;
    int max_output_tokens = 256;
    bool language_teacher = false;
};
class GenerationLimit : public std::runtime_error {
  public:
    std::string partial;
    GenerationLimit(std::string reason, std::string output)
        : std::runtime_error(std::move(reason)), partial(std::move(output)) {}
};
class Foundation {
  public:
    virtual ~Foundation() = default;
    virtual std::string name() const = 0;
    virtual std::string propose(const Request &, Token) = 0;
    virtual bool supports_representations() const { return false; }
    virtual neural::Representation represent(const std::string &, const std::string &, int) {
        throw std::runtime_error("knowledge representations unavailable; configure a Qwen3 GGUF backend");
    }
};
std::unique_ptr<Foundation> create(const std::string &model, int gpu_layers, const std::string &runtime_pack = {});
} // namespace intelligence
namespace runtime {
using Emit = std::function<void(const json &)>;
class Engine {
    chain::Store &store_;
    winds::Package &package_;
    fs::path package_root_;
    std::map<std::string, std::shared_ptr<const winds::Package>> packages_;
    std::set<std::string> in_flight_;
    std::string initial_key_;
    std::string active_key(const std::string &) const;
    std::shared_ptr<const winds::Package> pinned(const json &) const;
    struct Work {
        json instance;
        intelligence::Request request;
        heart::EvaluationContext context;
        std::shared_ptr<const winds::Package> package, language;
        json cognition, prior_cognition;
    };
    struct Result {
        std::string proposal, error;
        heart::Evaluation evaluation;
        State state{};
        json task, action;
        json neural, cognitive;
        int64_t elapsed_ms = 0;
    };
    std::optional<Work> begin_step(const std::string &, Emit);
    Result compute_step(const Work &, Emit) const;
    Result cognitive_compute(const Work &, Emit = {}) const;
    std::string language_mode_ = "strict";
    int language_teacher_limit_ = 8;
    int language_teacher_tokens_ = 1024;
    float language_student_threshold_ = .9f;
    void cognitive_commit(const Work &, Result &, Emit);
    bool finish_step(const Work &, Result, Emit);
    intelligence::Foundation &foundation_;
    std::unique_ptr<mesh::MeshNode> mesh_;
    heart::Registry evaluators_;
    std::string default_evaluator_;
    json neuron(const std::string &, const std::string &) const;
    bool scope_command(const json &, Emit);
    bool workspace_command(const json &, Emit);
    void workspace_recover(Emit);
    void workspace_dequeue(Emit);
    std::set<std::string> ready_chats_;
    bool neural_command(const json &, Emit);
    bool assembly_command(const json &, Emit);
    json assembly_start(const json &) const;
    void assembly_event(const json &, Emit);
    void assembly_finish(json, Emit, const std::string &reason = "");
    void assembly_expire(int64_t, Emit);
    void assembly_recover(Emit);
    void neural_touch(const std::string &, const std::string &, const std::string &, uint64_t, Emit);
    void neural_dispatch(int64_t, std::vector<std::string> &, Emit);
    json neural_snapshot(const std::string &, bool summary = false) const;
    json neural_node(const std::string &, const std::string &, const std::string &) const;
    json neural_pulse(const json &, const std::string &, const std::string &, uint64_t,
                      const std::string &kind = "evidence") const;
    Result neural_compute(const Work &) const;
    bool neural_finish(const Work &, Result, Emit);
    void neural_recover(Emit);
    void workspace_event(const json &, Emit);
    json resolve_soul(const std::string &, const std::string &, const std::string &explicit_soul = "") const;
    std::vector<std::string> read_roots(const std::string &) const;
    json speaker_profile(const std::string &, const std::string &) const;
    json choose_speaker(const std::string &, const std::string &, const std::string &,
                        const std::string &instance = "") const;
    std::string signal_signature(const json &) const;
    bool upkeep_command(const json &, Emit);
    void settle_signal(const json &, Emit);
    void suggest_from_failure(const json &, Emit);
    void memory_changed(const std::string &, const std::string &, Emit);
    json history(const std::string &, const std::string &) const;
    json event(const std::string &type, const std::string &root, const std::string &trace, const json &data,
               Emit emit);

  public:
    Engine(chain::Store &, winds::Package &, intelligence::Foundation &,
           std::unique_ptr<mesh::MeshNode> node = {}, heart::Registry evaluators = {},
           std::string default_evaluator = "wds.ai.heart.composite/1", fs::path package_root = "packages");
    std::string command(const json &, Emit); // Returns instance ID needing one scheduled step, or empty.
    bool step(const std::string &id, Emit);  // One bounded proposal; true schedules the next step.
    async::Task<bool> advance(std::string id, Emit emit, async::Executor &compute,
                              async::Executor &coordinator);
    void fail(const std::string &id, const std::string &error, Emit);
    void pause_all();
    void configure_language(std::string mode, int calls = 8, int tokens = 1024, float threshold = .9f);
    std::vector<std::string> dispatch_ready(int64_t time_ms, Emit);
    std::optional<int64_t> next_wake(int64_t time_ms) const;
    json upkeep_snapshot(const std::string &root = "*") const;
};
} // namespace runtime
} // namespace wds::ai
