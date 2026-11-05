#pragma once
#include "wds/ai/relationships.hpp"

namespace wds::ai {
namespace heart {
enum class Status { pass, fail, inconclusive, error };
std::string name(Status);
struct Evidence {
    std::string kind, id, digest;
    int version = 0;
    uint64_t sequence = 0;
    json record() const;
};
struct InterestProposal {
    std::string soul_id, reason;
    int strength_delta = 1;
    json record() const;
};
struct VoiceContribution {
    json expression = {{"gender_type", "unspecified"}};
    std::string node_id, voice_id, root_id, trace_id, instance_id, scope, feedback;
    int voice_version = 1, iteration = 0;
    uint64_t snapshot_sequence = 0;
    std::vector<std::string> represented_souls;
    Status status = Status::inconclusive;
    std::map<std::string, double> metrics;
    std::vector<Evidence> evidence;
    std::vector<InterestProposal> proposed_interests;
    json record() const;
};
struct EvaluationContext {
    relationships::HeartScope heart;
    std::string seed, trace_id, instance_id;
    json rubric = json::object();
    int iteration = 0;
    size_t voice_budget = 4;
    json task; // Immutable structured task evidence, when using a domain evaluator.
};
struct Evaluation {
    Status status = Status::inconclusive;
    float score = 0;
    std::string evaluator_id, scope, feedback, aggregation_policy;
    int evaluator_version = 1;
    uint64_t snapshot_sequence = 0;
    bool disagreement = false;
    std::vector<std::string> represented_souls, considered_souls;
    std::map<std::string, double> metrics;
    std::vector<Evidence> evidence;
    std::vector<VoiceContribution> contributions;
    json skipped = json::array();
    bool passed() const {
        return status == Status::pass;
    }
    json record() const;
};
} // namespace heart
} // namespace wds::ai
