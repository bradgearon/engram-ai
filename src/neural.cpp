#include "wds/ai/core.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace wds::ai::neural {
std::vector<std::string> actions() {
    return {"rest", "recall", "propagate", "review", "request_knowledge", "suggest", "hold"};
}
json default_settings() {
    return {{"version", 0}, {"enabled", true}, {"auto_register", true},
            {"max_concurrent", 2}, {"max_impulses_per_window", 64},
            {"max_knowledge_calls", 8}, {"window_ms", 60000},
            {"refractory_ms", 1000}, {"failure_refractory_ms", 60000},
            {"max_hops", 4}, {"max_rna_per_impulse", 4}, {"max_pending", 256},
            {"max_input_tokens", 512}, {"max_review_attempts", 2}, {"review_delay_ms", 60000},
            {"web_enabled", false}, {"max_web_queries", 2}, {"web_timeout_ms", 120000},
            {"response_deadline_ms", 30000}, {"max_response_contributions", 16}, {"max_response_sources", 8}};
}
void validate_settings(const json &v) {
    const auto defaults = default_settings();
    require(v.is_object(), "mesh settings must be an object");
    for (auto it = v.begin(); it != v.end(); ++it)
        require(defaults.contains(it.key()), "unknown mesh setting: " + it.key());
    for (auto key : {"enabled", "auto_register", "web_enabled"}) require(v.at(key).is_boolean(), "mesh flag must be boolean");
    upkeep::integer(v, "version", 0, 1000000);
    upkeep::integer(v, "max_concurrent", 1, 4);
    upkeep::integer(v, "max_impulses_per_window", 1, 1024);
    upkeep::integer(v, "max_knowledge_calls", 0, 128);
    upkeep::integer(v, "window_ms", 1000, 86400000);
    upkeep::integer(v, "refractory_ms", 10, 604800000);
    upkeep::integer(v, "failure_refractory_ms", 10, 604800000);
    upkeep::integer(v, "max_hops", 1, 8);
    upkeep::integer(v, "max_rna_per_impulse", 1, 8);
    upkeep::integer(v, "max_pending", 1, 1024);
    upkeep::integer(v, "max_input_tokens", 8, 2048);
    upkeep::integer(v, "max_review_attempts", 0, 4);
    upkeep::integer(v, "review_delay_ms", 10, 604800000);
    upkeep::integer(v, "max_web_queries", 0, 16);
    upkeep::integer(v, "web_timeout_ms", 1000, 300000);
    upkeep::integer(v, "response_deadline_ms", 1000, 300000);
    upkeep::integer(v, "max_response_contributions", 1, 32);
    upkeep::integer(v, "max_response_sources", 1, 8);
}
void validate_examples(const json &examples) {
    require(examples.is_array() && examples.size() <= 8, "at most eight semantic demonstrations");
    auto names = actions();
    std::set<std::string> texts;
    for (const auto &e : examples) {
        auto text = field(e, "text", 512), action = field(e, "action", 32);
        require(std::find(names.begin(), names.end(), action) != names.end(), "unknown semantic action");
        require(texts.insert(text).second, "duplicate semantic demonstration");
    }
}
State features(const json &p, const json &n, const std::vector<json> &memories) {
    if (p.contains("features")) {
        require(p.at("features").is_array() && p.at("features").size() == 16, "expected 16 neural features");
        auto x = p.at("features").get<State>();
        for (float v : x) require(std::isfinite(v) && v >= 0 && v <= 1, "neural features must be in [0,1]");
        return x;
    }
    State x{};
    const auto text = p.at("text").get<std::string>();
    float relevant = 0, conflict = 0, correction = 0;
    for (const auto &m : memories) {
        const float score = float(mesh::relevance(text, m.at("text")));
        relevant = std::max(relevant, score);
        if (score > 0) {
            conflict = std::max(conflict, m.value("claim_status", std::string{}) == "disputed" ? 1.f : 0.f);
            correction = std::max(correction, m.value("attribution", json::object()).value("kind", std::string{}) == "reflection" ? 1.f : 0.f);
        }
    }
    x[0] = p.value("direct", false) ? 1.f : 0.f;
    x[1] = float(mesh::relevance(text, n.at("purpose")));
    x[2] = relevant;
    x[3] = std::min(1.f, float(memories.size()) / 8.f);
    x[4] = conflict;
    x[5] = p.value("kind", std::string{}) == "reflection" ? 1.f : correction;
    x[6] = std::clamp(p.value("strength", 0.5f), 0.f, 1.f);
    x[7] = std::min(1.f, float(p.value("depth", 0)) / 8.f);
    x[8] = p.value("kind", std::string{}) == "review" ? 1.f : 0.f;
    x[9] = text.find('?') != std::string::npos ? 1.f : 0.f;
    x[10] = p.value("novel", relevant == 0) ? 1.f : 0.f;
    x[11] = std::min(1.f, float(text.size()) / 4096.f);
    x[12] = n.value("knowledge", true) && !n.at("examples").empty() ? 1.f : 0.f;
    x[13] = std::clamp(p.value("feedback", 0.5f), 0.f, 1.f);
    x[14] = std::min(1.f, float(p.value("attempt", 0)) / 4.f);
    x[15] = 1;
    return x;
}
json Decision::record() const {
    return {{"action", action}, {"reason", reason}, {"method", method}, {"strength", strength},
            {"margin", margin}, {"uncertain", uncertain}, {"features", features},
            {"feature_schema", feature_schema}, {"evidence", evidence}, {"confidence_calibrated", false}};
}
Decision decide(const State &x, const winds::Package &package, const json &node) {
    Decision d;
    d.features = x;
    d.method = "bootstrap-rules/1";
    d.strength = x[6];
    if (package.manifest.value("feature_schema", std::string{}) == feature_schema) {
        auto names = actions();
        std::array<float, 7> logits{};
        for (size_t o = 0; o < logits.size(); ++o) {
            logits[o] = package.policy.bias[o];
            for (size_t i = 0; i < 16; ++i) logits[o] += package.policy.weight[o * 16 + i] * x[i];
        }
        auto best = size_t(std::max_element(logits.begin(), logits.end()) - logits.begin());
        float max = logits[best], sum = 0, next = 0;
        for (auto &v : logits) { v = std::exp(v - max); sum += v; }
        for (size_t i = 0; i < logits.size(); ++i) if (i != best) next = std::max(next, logits[i] / sum);
        d.strength = logits[best] / sum;
        d.margin = d.strength - next;
        d.action = names[best];
        d.method = "policy.wind/neural.features/1";
        d.uncertain = d.strength < node.at("minimum_strength").get<float>() ||
                      d.margin < node.at("minimum_margin").get<float>();
        d.reason = d.uncertain ? "local readout below configured threshold" : "local learned readout accepted";
        if (d.uncertain) d.action = "request_knowledge";
        return d;
    }
    // Transparent bootstrap behavior until an operator activates a trained readout.
    if (x[4] > 0) { d.action = "review"; d.reason = "conflicting evidence needs a diagnostic review"; }
    else if (x[5] > 0) { d.action = "propagate"; d.reason = "recorded correction should reach connected understanding"; }
    else if (x[2] > 0) { d.action = "recall"; d.reason = "relevant local evidence is already available"; }
    else if (x[0] == 0 && x[1] == 0) { d.action = "rest"; d.reason = "outside declared interest"; }
    else { d.action = "request_knowledge"; d.uncertain = true; d.reason = "local evidence does not support a decision"; }
    return d;
}
float cosine(const std::vector<float> &a, const std::vector<float> &b) {
    require(!a.empty() && a.size() == b.size(), "incompatible representation dimensions");
    double dot = 0, aa = 0, bb = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        require(std::isfinite(a[i]) && std::isfinite(b[i]), "non-finite representation");
        dot += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i];
    }
    require(aa > 0 && bb > 0, "zero representation");
    return float(std::clamp(dot / std::sqrt(aa * bb), -1., 1.));
}
Decision classify(const Decision &local, const Representation &input,
                  const std::vector<Representation> &examples, const json &labels,
                  float minimum, float margin) {
    require(examples.size() == labels.size() && !examples.empty(), "missing semantic demonstrations");
    std::map<std::string, float> scores;
    for (size_t i = 0; i < examples.size(); ++i) {
        require(examples[i].identity == input.identity, "representation identity mismatch");
        auto action = labels[i].at("action").get<std::string>();
        float score = cosine(input.values, examples[i].values);
        auto it = scores.find(action);
        if (it == scores.end() || score > it->second) scores[action] = score;
    }
    require(scores.size() >= 2, "semantic readout needs at least two distinct action labels");
    std::vector<std::pair<float, std::string>> order;
    for (const auto &[action, score] : scores) order.emplace_back(score, action);
    std::sort(order.rbegin(), order.rend());
    auto d = local;
    d.method = "qwen-representation/exemplar-readout/1";
    d.strength = order[0].first;
    d.margin = order[0].first - order[1].first;
    d.uncertain = d.strength < minimum || d.margin < margin;
    d.action = d.uncertain ? "hold" : order[0].second;
    d.reason = d.uncertain ? "semantic readout ambiguous; explicit feedback or new evidence required" :
                            "semantic demonstration matched; score is similarity, not factual confidence";
    return d;
}
} // namespace wds::ai::neural
