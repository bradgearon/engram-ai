#include "wds/ai/core.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>

namespace wds::ai::expression {
namespace {
std::string lower(std::string text) {
    for (auto &c : text) c = char(std::tolower(static_cast<unsigned char>(c)));
    return text;
}
bool begins(const std::string &s, const std::string &prefix) { return s.rfind(prefix, 0) == 0; }
bool has(const std::string &s, const std::string &part) { return s.find(part) != std::string::npos; }
std::string sentence(std::string s) {
    if (!s.empty() && s.back() != '.' && s.back() != '?' && s.back() != '!') s += '.';
    return s;
}
}
std::vector<std::string> actions() { return {"answer", "compare", "acknowledge", "clarify", "defer", "reflect", "conflict"}; }
std::string intent(const std::string &input) {
    auto s = lower(input);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.pop_back();
    if (s == "hi" || s == "hello" || s == "hey" || s == "hello!" || s == "hi!" || s == "thanks" || s == "thank you") return "greeting";
    if (begins(s, "i prefer ") || begins(s, "please remember ") || begins(s, "remember that ")) return "preference";
    if (begins(s, "correction:") || begins(s, "actually,") || begins(s, "please change ")) return "correction";
    if (has(s, "compare") || has(s, "difference between")) return "compare";
    if (has(s, "?") || begins(s, "what ") || begins(s, "how ") || begins(s, "why ") || begins(s, "when ") ||
        begins(s, "where ") || begins(s, "who ") || begins(s, "tell me ") || begins(s, "explain ") || begins(s, "recall ")) return "question";
    return "statement";
}
std::vector<json> evidence(const std::vector<json> &memories, const std::string &query, bool direct) {
    (void)direct;
    std::vector<std::pair<float, json>> ranked;
    for (const auto &m : memories) {
        if (m.at("claim_status") == "retired" || m.value("memory_kind", std::string{}) == "expression_trace") continue;
        if (m.value("memory_kind", std::string{}) == "reflection") continue;
        auto relevance = float(mesh::relevance(query, m.at("text")));
        if (m.contains("semantic") && has(lower(query), lower(m.at("semantic").at("subject")))) relevance = std::max(relevance, 1.f);
        if (relevance <= 0) continue;
        auto score = relevance + .25f * m.value("attunement", json::object()).value("importance", 0.f);
        json item = {{"id", m.at("id")}, {"root_id", m.at("root_id")}, {"version", m.at("version")},
                     {"text", m.at("text")}, {"source", m.at("source")}, {"claim_status", m.at("claim_status")},
                     {"relevance", relevance}, {"attention_score", score}, {"memory_kind", m.value("memory_kind", std::string("source"))}};
        for (auto field : {"semantic", "web_provenance", "attribution", "attunement"}) if (m.contains(field)) item[field] = m.at(field);
        ranked.emplace_back(score, std::move(item));
    }
    std::stable_sort(ranked.begin(), ranked.end(), [](const auto &a, const auto &b) { return a.first > b.first; });
    std::vector<json> out;
    for (const auto &row : ranked) { if (out.size() == 3) break; out.push_back(row.second); }
    return out;
}
State features(const json &a) {
    State x{};
    auto kind = intent(a.at("input"));
    x[0] = !a.at("evidence").empty();
    for (const auto &e : a.at("evidence")) {
        x[1] = std::max(x[1], e.contains("semantic") ? 1.f : 0.f);
        x[10] = std::max(x[10], e.at("claim_status") == "observed" ? 1.f : 0.f);
        x[12] = std::max(x[12], e.value("relevance", 0.f));
    }
    x[2] = kind == "question"; x[3] = kind == "correction"; x[4] = kind == "compare";
    x[5] = kind == "greeting"; x[6] = kind == "preference"; x[7] = a.value("background", false);
    x[8] = !a.at("unresolved").empty(); x[9] = a.value("conflict", false);
    x[11] = std::min(1.f, float(a.at("contributions").size()) / 16);
    x[13] = std::min(1.f, float(a.at("evidence").size()) / 8);
    x[14] = kind == "statement"; x[15] = 1;
    return x;
}
json compose(const json &a, const winds::Package &package) {
    auto x = features(a); const auto kind = intent(a.at("input"));
    std::string action = x[9] ? "conflict" : x[0] ? (x[4] ? "compare" : x[7] ? "reflect" : "answer") :
                         (x[5] || x[6] || x[3] || (x[14] && !x[8])) ? "acknowledge" : x[8] ? "defer" : "clarify";
    std::string method = "native-expression-rules/1", proposed = action, gate;
    float strength = 0, margin = 0;
    if (package.manifest.value("feature_schema", std::string{}) == feature_schema) {
        auto names = actions(); std::array<float, 7> values{};
        for (size_t o = 0; o < 7; ++o) {
            values[o] = package.policy.bias[o];
            for (size_t i = 0; i < 16; ++i) values[o] += package.policy.weight[o * 16 + i] * x[i];
        }
        const auto best = std::max_element(values.begin(), values.end()) - values.begin();
        float maximum = values[best], sum = 0, next = 0;
        for (auto &v : values) { v = std::exp(v - maximum); sum += v; }
        for (size_t i = 0; i < 7; ++i) if (int(i) != best) next = std::max(next, values[i] / sum);
        strength = values[best] / sum; margin = strength - next; proposed = names[best];
        method = "policy.wind/expression.features/1";
        // A learned style cannot invent supporting evidence or suppress a known conflict.
        const bool permitted = x[9] ? proposed == "conflict" : x[0] ?
            (proposed == "answer" || proposed == "compare" || proposed == "reflect") :
            (proposed == "clarify" || proposed == "defer" || (proposed == "acknowledge" && (x[5] || x[6] || x[3] || x[14])));
        if (strength >= .7f && margin >= .1f && permitted) action = proposed;
        else gate = permitted ? "readout below strength/margin threshold; native realization selected" : "expression contract rejected unsupported style";
    }
    std::string text;
    json cited = json::array(), clauses = json::array();
    if (a.value("cancelled", false)) text = "I stopped this response. The work already recorded is still available for inspection.";
    else if (action == "acknowledge") {
        if (kind == "greeting") text = has(lower(a.at("input")), "thank") ? "You're welcome. What would you like to work on next?" : "Hello. What would you like to work on?";
        else if (kind == "preference") text = a.value("chat_id", std::string{}).empty() ? "Understood. You can save that as a preference for this project." : "I've recorded that preference in this project's memory.";
        else if (kind == "correction") text = "I've recorded your correction. It will be available when we revisit this understanding.";
        else text = "I've recorded that. What would you like to build or understand from it?";
    } else if (!a.at("evidence").empty()) {
        if (action == "conflict") text = "I found conflicting recorded statements, so I can't choose one as settled:\n\n";
        else if (action == "compare") text = "Here are the relevant statements to compare:\n\n";
        else if (action == "reflect") text = "I returned to this understanding and found:\n\n";
        for (const auto &e : a.at("evidence")) {
            if (cited.size() == 3) break;
            auto number = std::to_string(cited.size() + 1);
            std::string clause;
            if (e.contains("semantic")) {
                const auto &s = e.at("semantic");
                clause = sentence(s.at("subject").get<std::string>() + " " + s.at("predicate").get<std::string>() + " " + s.at("object").get<std::string>());
                if (e.at("claim_status") != "observed") clause = "The recorded statement is: " + clause;
            } else if (e.value("memory_kind", std::string{}) == "reflection") clause = "Previously recorded feedback: “" + e.at("text").get<std::string>() + "”";
            else clause = "The source says: “" + e.at("text").get<std::string>() + "”";
            text += clause + " [" + number + "]\n\n";
            clauses.push_back({{"text", clause}, {"source_id", e.at("id")}, {"source_version", e.at("version")}});
            cited.push_back(e);
        }
        if (a.at("evidence").size() > cited.size()) text += "Additional relevant sources are retained in the response record.\n\n";
        bool unverified = false; for (const auto &e : cited) if (e.at("claim_status") != "observed") unverified = true;
        if (unverified) text += "These include unverified material; I haven't independently established it as fact.\n\n";
        if (!a.at("unresolved").empty()) text += "Some requested work remains unresolved; this is a partial response.\n\n";
        text += "Sources:";
        for (size_t i = 0; i < cited.size(); ++i)
            text += "\n[" + std::to_string(i + 1) + "] " + cited[i].at("source").get<std::string>() + " (" +
                    cited[i].at("id").get<std::string>() + ", version " + std::to_string(cited[i].at("version").get<int>()) + ")";
    } else {
        text = "I don't have enough relevant evidence to answer that yet.";
        if (!a.at("unresolved").empty()) text += " " + a.at("unresolved").front().at("reason").get<std::string>() + ".";
        text += " Share a source or teach the relevant relationship, and I can use it when we try again.";
    }
    require(text.size() <= 16384, "assembled response exceeds output bound; no partial text committed");
    return {{"text", text}, {"action", action}, {"proposed_action", proposed}, {"method", method}, {"features", x},
            {"strength", strength}, {"margin", margin}, {"gate", gate}, {"clauses", clauses}, {"sources", cited},
            {"confidence_calibrated", false}, {"renderer", "native-grounded-realization/1"}, {"generated_tokens", 0}};
}
} // namespace wds::ai::expression
