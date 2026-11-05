#include "wds/ai/core.hpp"
#include <algorithm>

namespace wds::ai::heart {
std::string name(Status s) {
    switch (s) {
    case Status::pass:
        return "pass";
    case Status::fail:
        return "fail";
    case Status::error:
        return "error";
    default:
        return "inconclusive";
    }
}
json Evidence::record() const {
    return {{"kind", kind}, {"id", id}, {"digest", digest}, {"version", version}, {"sequence", sequence}};
}
json InterestProposal::record() const {
    return {{"soul_id", soul_id},
            {"strength_delta", strength_delta},
            {"reason", reason},
            {"status", "proposed_only"}};
}
json VoiceContribution::record() const {
    json refs = json::array(), proposals = json::array();
    for (const auto &e : evidence)
        refs.push_back(e.record());
    for (const auto &p : proposed_interests)
        proposals.push_back(p.record());
    return {{"expression", expression},
            {"node_id", node_id},
            {"voice_id", voice_id},
            {"voice_version", voice_version},
            {"root_id", root_id},
            {"trace_id", trace_id},
            {"instance_id", instance_id},
            {"iteration", iteration},
            {"snapshot_sequence", snapshot_sequence},
            {"represented_souls", represented_souls},
            {"status", name(status)},
            {"scope", scope},
            {"metrics", metrics},
            {"evidence", refs},
            {"feedback", feedback},
            {"proposed_interests", proposals}};
}
json Evaluation::record() const {
    json refs = json::array(), voices = json::array();
    for (const auto &e : evidence)
        refs.push_back(e.record());
    for (const auto &c : contributions)
        voices.push_back(c.record());
    return {{"passed", passed()},
            {"score", score},
            {"status", name(status)},
            {"feedback", feedback},
            {"evaluator", evaluator_id + "/" + std::to_string(evaluator_version)},
            {"evaluator_id", evaluator_id},
            {"evaluator_version", evaluator_version},
            {"scope", scope},
            {"metrics", metrics},
            {"evidence", refs},
            {"snapshot_sequence", snapshot_sequence},
            {"represented_souls", represented_souls},
            {"considered_souls", considered_souls},
            {"contributions", voices},
            {"skipped", skipped},
            {"disagreement", disagreement},
            {"aggregation_policy", aggregation_policy}};
}
Evaluation Rules::evaluate(const std::string &text, const EvaluationContext &ctx) const {
    const auto term = ctx.rubric.value("must_include", std::string{});
    bool length = text.size() >= 24 && text.size() <= 8192;
    bool phrase = term.empty() || text.find(term) != std::string::npos;
    Evaluation e;
    e.status = length && phrase ? Status::pass : Status::fail;
    e.score = e.passed() ? 1.f : 0.f;
    e.evaluator_id = "wds.ai.heart.rules";
    e.aggregation_policy = "all_structural_checks/1";
    e.scope = "length and required substring only; no factual verification";
    e.feedback = e.passed() ? "Structural checks passed."
                            : "Revise to 24..8192 bytes and include required phrase: " + term;
    e.snapshot_sequence = ctx.heart.sequence();
    e.represented_souls = {ctx.heart.root_id()};
    e.considered_souls = e.represented_souls;
    e.metrics = {{"length_pass", length ? 1. : 0.}, {"phrase_pass", phrase ? 1. : 0.}};
    e.evidence = {{"proposal", ctx.instance_id, checksum(text), 0, ctx.heart.sequence()},
                  {"rubric", ctx.instance_id, checksum(ctx.rubric.dump()), 1, ctx.heart.sequence()}};
    return e;
}
void Registry::add(const std::string &id, std::function<std::unique_ptr<Evaluator>()> factory) {
    require(!id.empty() && id.size() <= 80 && bool(factory) && factories_.size() < 32,
            "invalid evaluator registration");
    require(factories_.emplace(id, std::move(factory)).second, "duplicate evaluator registration");
}
std::unique_ptr<Evaluator> Registry::create(const std::string &id) const {
    auto found = factories_.find(id);
    require(found != factories_.end(), "unknown evaluator: " + id);
    auto evaluator = found->second();
    require(bool(evaluator), "evaluator factory returned null");
    return evaluator;
}
Registry defaults(const mesh::MeshNode &mesh) {
    Registry registry;
    registry.add("wds.ai.heart.rules/1", [] { return std::make_unique<Rules>(); });
    registry.add("wds.ai.heart.composite/1", [&mesh] { return std::make_unique<Composite>(mesh); });
    registry.add("wds.ai.heart.music/1", [] { return std::make_unique<tasks::DomainEvaluator>("music"); });
    registry.add("wds.ai.heart.architecture/1",
                 [] { return std::make_unique<tasks::DomainEvaluator>("architecture"); });
    return registry;
}
Evaluation Composite::evaluate(const std::string &text, const EvaluationContext &ctx) const {
    Evaluation e;
    e.evaluator_id = "wds.ai.heart.composite";
    e.aggregation_policy = "all_selected_checks/1";
    e.scope =
        "structural constraints plus selected lexical-purpose checks; no semantic or factual verification";
    e.snapshot_sequence = ctx.heart.sequence();
    for (const auto &s : ctx.heart.souls())
        e.represented_souls.push_back(s.id);
    mesh::VoiceBudget budget(ctx.voice_budget);
    bool failed = false, passed = false, incomplete = false, error = false;
    auto skip = [&](const std::string &soul, const std::string &voice, const std::string &why,
                    double priority) {
        e.skipped.push_back(
            {{"soul_id", soul}, {"voice_id", voice}, {"reason", why}, {"priority", priority}});
    };
    auto consult = [&](const std::string &voice, const std::string &soul, double priority) {
        if (!mesh_.has_voice(voice)) {
            incomplete = true;
            skip(soul, voice, "binding_pending", priority);
            return;
        }
        if (!budget.remaining) {
            incomplete = true;
            skip(soul, voice, "budget_exhausted", priority);
            return;
        }
        auto c = mesh_.consult(voice, soul, text, ctx, budget);
        c.metrics["selection_priority"] = priority;
        failed |= c.status == Status::fail;
        passed |= c.status == Status::pass;
        incomplete |= c.status == Status::inconclusive;
        error |= c.status == Status::error;
        e.considered_souls.push_back(soul);
        if (!e.feedback.empty())
            e.feedback += " ";
        e.feedback += soul + ": " + c.feedback;
        e.contributions.push_back(std::move(c));
    };
    consult("constraint", ctx.heart.root_id(), 1.);
    struct Candidate {
        const relationships::InterestRecord *interest;
        double priority;
    };
    std::vector<Candidate> selected;
    for (const auto &i : ctx.heart.interests()) {
        auto s = ctx.heart.find(i.soul_id);
        require(s != nullptr, "interest without scoped soul");
        double relevance = mesh::relevance(ctx.seed, s->purpose);
        double priority = relevance * (1. + double(i.strength) / 100.);
        if (i.status == relationships::InterestStatus::paused) {
            skip(i.soul_id, i.voice_id, "paused", priority);
        } else if (relevance == 0) {
            skip(i.soul_id, i.voice_id, "irrelevant", priority);
        } else if (i.status == relationships::InterestStatus::pending || !mesh_.has_voice(i.voice_id)) {
            skip(i.soul_id, i.voice_id, "binding_pending", priority);
            incomplete = true;
        } else
            selected.push_back({&i, priority});
    }
    std::stable_sort(selected.begin(), selected.end(),
                     [](const Candidate &a, const Candidate &b) { return a.priority > b.priority; });
    for (const auto &c : selected)
        consult(c.interest->voice_id, c.interest->soul_id, c.priority);
    e.disagreement = passed && failed;
    // A failed concrete check remains a failure even when another consultation was omitted.
    e.status = error        ? Status::error
               : failed     ? Status::fail
               : incomplete ? Status::inconclusive
                            : Status::pass;
    size_t passes = 0;
    for (const auto &c : e.contributions)
        if (c.status == Status::pass)
            ++passes;
    e.score = e.contributions.empty() ? 0.f : float(passes) / float(e.contributions.size());
    e.metrics = {{"voice_budget", double(ctx.voice_budget)},
                 {"voices_used", double(budget.used)},
                 {"voices_remaining", double(budget.remaining)},
                 {"passing_contributions", double(passes)}};
    e.evidence = {
        {"proposal", ctx.instance_id, checksum(text), 0, ctx.heart.sequence()},
        {"seed", ctx.trace_id, checksum(ctx.seed), 0, ctx.heart.sequence()},
        {"heart_scope", ctx.heart.root_id(), checksum(ctx.heart.record().dump()), 1, ctx.heart.sequence()}};
    if (incomplete)
        e.feedback += " Eligible perspectives were unavailable or exceeded the consultation budget.";
    return e;
}
} // namespace wds::ai::heart
