#include "wds/ai/core.hpp"
#include <cctype>
#include <cmath>

namespace wds::ai::mesh {
json topology(const relationships::HeartScope &scope) {
    json nodes = json::array(), routes = json::array();
    for (const auto &s : scope.souls()) {
        nodes.push_back({{"id", scope.root_id() + "/" + s.id},
                         {"kind", "neuron"},
                         {"soul_id", s.id},
                         {"version", s.version},
                         {"arc_sequence", s.declaration_sequence},
                         {"igfbp2_index_key", scope.root_id() + "/" + s.id},
                         {"igf2bp2_payload_ref", {{"journal_sequence", s.declaration_sequence}}}});
    }
    for (const auto &i : scope.interests())
        routes.push_back({{"from", scope.root_id() + "/" + i.soul_id},
                          {"to", "voice/" + i.voice_id},
                          {"kind", "axon"},
                          {"status", relationships::name(i.status)},
                          {"strength", i.strength},
                          {"version", i.version},
                          {"provenance_sequence", i.declaration_sequence}});
    return {{"schema", 1},
            {"root_id", scope.root_id()},
            {"snapshot_sequence", scope.sequence()},
            {"nodes", nodes},
            {"routes", routes},
            {"update_role", "astrocyte: propose typed RNA; coordinator validates and commits"},
            {"growth_roles",
             {{"crest_cell",
               {{"purpose", "propose new neuron endpoints and axon routes"},
                {"commit_path", "relationship.declare"},
                {"writes_garden", false}}},
              {"stem_cell",
               {{"purpose", "propose growth of an existing endpoint's understanding"},
                {"commit_path", "memory.upsert"},
                {"writes_garden", false}}}}},
            {"storage_role", "Arc: authoritative append-only journal for engrams and astroengrams"},
            {"index_role", "IGFBP2: scoped locations, versions and numeric attention metadata"},
            {"payload_role", "IGF2BP2: bounded lazy payload fetch and validated RNA preparation"},
            {"growth", "explicit relationship declarations within capacity"},
            {"interpretation",
             "software topology and proposed biological analogy; no biological mechanism simulated"}};
}

namespace {
std::set<std::string> words(const std::string &text) {
    static const std::set<std::string> stop = {"about", "also", "from",  "have",  "help", "into", "that",
                                               "their", "then", "there", "these", "this", "with", "your"};
    std::set<std::string> out;
    std::string word;
    auto flush = [&] {
        if (word.size() >= 4 && !stop.count(word))
            out.insert(word);
        word.clear();
    };
    for (unsigned char c : text) {
        if (std::isalnum(c))
            word += char(std::tolower(c));
        else
            flush();
    }
    flush();
    return out;
}
class PurposeVoice final : public Voice {
    heart::VoiceContribution contribute(const VoiceContext &c) const override {
        heart::VoiceContribution out;
        const double overlap = relevance(c.proposal, c.soul.purpose);
        out.status = overlap > 0 ? heart::Status::pass : heart::Status::fail;
        out.scope = "ASCII purpose-token overlap only; not semantic alignment or truth";
        out.metrics = {{"purpose_token_overlap", overlap}};
        out.feedback = overlap > 0 ? "Proposal contains a declared-purpose token."
                                   : "Revise to address the declared purpose: " + c.soul.purpose;
        if (overlap == 0)
            out.proposed_interests.push_back({c.soul.id, "Purpose alignment needs review.", 1});
        return out;
    }
};
class ConstraintVoice final : public Voice {
    heart::VoiceContribution contribute(const VoiceContext &c) const override {
        heart::VoiceContribution out;
        auto term = c.rubric.value("must_include", std::string{});
        bool length = c.proposal.size() >= 24 && c.proposal.size() <= 8192;
        bool phrase = term.empty() || c.proposal.find(term) != std::string::npos;
        out.status = length && phrase ? heart::Status::pass : heart::Status::fail;
        out.scope = "24..8192-byte length and required substring; no factual verification";
        out.metrics = {{"length_pass", length ? 1. : 0.}, {"phrase_pass", phrase ? 1. : 0.}};
        out.feedback = length && phrase ? "Structural checks passed."
                                        : "Revise to 24..8192 bytes and include required phrase: " + term;
        return out;
    }
};
} // namespace
double relevance(const std::string &query, const std::string &purpose) {
    auto q = words(query), p = words(purpose);
    if (p.empty())
        return 0;
    size_t shared = 0;
    for (const auto &w : p)
        if (q.count(w))
            ++shared;
    return double(shared) / double(p.size());
}
std::vector<json> LocalScanIndex::retrieve(const std::string &root, const Embedding &q) const {
    return indexer_.locate(root, q);
}
std::vector<json> LocalScanIndex::fetch(const std::string &root, const std::vector<json> &refs) const {
    return handler_.fetch(root, refs);
}
std::vector<json> IGFBP2Indexer::locate(const std::string &root, const Embedding &q) const {
    return mesh::retrieve(arc_, root, q, true);
}
json IGF2BP2Handler::prepare_rna(const json &packet, const std::string &trace) const {
    return mesh::prepare_memory(arc_, packet, trace);
}
std::vector<json> IGF2BP2Handler::fetch(const std::string &root, const std::vector<json> &refs) const {
    require(refs.size() <= top_k, "payload fetch exceeds top-k budget");
    std::vector<json> out;
    for (const auto &ref : refs) {
        const auto &row = arc_.engrams.at(ref.at("id").get<std::string>());
        require(row.at("root_id") == root && row.at("version") == ref.at("version"),
                "payload scope/version mismatch");
        out.push_back(row);
    }
    return out;
}
std::vector<json> SingleNodeMesh::fetch(const std::string &root, const std::vector<json> &refs) const {
    return index_->fetch(root, refs);
}
Voices builtin_voices() {
    Voices voices;
    voices["purpose"] = std::make_unique<PurposeVoice>();
    voices["constraint"] = std::make_unique<ConstraintVoice>();
    return voices;
}
SingleNodeMesh::SingleNodeMesh(std::unique_ptr<MemoryIndex> index, Voices voices)
    : index_(std::move(index)), voices_(std::move(voices)) {
    require(bool(index_), "mesh requires an index");
    require(voices_.size() <= 16, "voice registry capacity");
    for (const auto &entry : voices_) {
        identifier(entry.first);
        require(bool(entry.second) && entry.second->version() > 0 && entry.second->version() <= 1000000,
                "invalid voice registration");
    }
}
bool SingleNodeMesh::has_voice(const std::string &id) const {
    return voices_.count(id) != 0;
}
std::vector<json> SingleNodeMesh::retrieve(const std::string &root, const Embedding &q) const {
    return index_->retrieve(root, q);
}
heart::VoiceContribution SingleNodeMesh::consult(const std::string &voice, const std::string &soul,
                                                 const std::string &proposal,
                                                 const heart::EvaluationContext &ctx,
                                                 VoiceBudget &budget) const {
    const auto *s = ctx.heart.find(soul);
    require(s != nullptr, "consultation outside heart scope");
    if (soul == ctx.heart.root_id()) {
        require(voice == "constraint", "primary root only binds the constraint voice");
    } else {
        const auto *i = ctx.heart.interest(soul);
        require(i && i->voice_id == voice && i->status == relationships::InterestStatus::eligible,
                "voice not eligible for scoped soul");
        require(relevance(ctx.seed, s->purpose) > 0, "voice not relevant to seed");
    }
    require(has_voice(voice), "unknown voice");
    require(budget.remaining > 0, "voice budget exhausted");
    require(proposal.size() <= 8192 && ctx.seed.size() <= 4096 && ctx.rubric.dump().size() <= 1024,
            "voice input exceeds budget");
    --budget.remaining;
    ++budget.used;
    heart::VoiceContribution out;
    try {
        out = voices_.at(voice)->contribute({proposal, ctx.seed, ctx.rubric, *s});
        require(out.feedback.size() <= 2048 && out.scope.size() <= 256 && out.metrics.size() < 16 &&
                    out.proposed_interests.size() <= 4,
                "voice output exceeds budget");
        for (const auto &metric : out.metrics)
            require(!metric.first.empty() && metric.first.size() <= 64 && std::isfinite(metric.second),
                    "invalid voice metric");
        require(out.status == heart::Status::pass || out.status == heart::Status::fail ||
                    out.status == heart::Status::inconclusive || out.status == heart::Status::error,
                "invalid voice status");
        for (const auto &p : out.proposed_interests)
            require(p.soul_id == soul && p.strength_delta >= 0 && p.strength_delta <= 1 &&
                        p.reason.size() <= 256,
                    "invalid interest proposal");
    } catch (const std::exception &error) {
        out = {};
        out.status = heart::Status::error;
        out.scope = "voice execution failure";
        out.feedback = std::string(error.what()).substr(0, 2048);
    }
    // The mesh stamps authoritative attribution, rather than trusting voice-supplied identity.
    out.node_id = "wds.ai.mesh.local";
    out.voice_id = voice;
    out.voice_version = voices_.at(voice)->version();
    out.root_id = ctx.heart.root_id();
    out.trace_id = ctx.trace_id;
    out.instance_id = ctx.instance_id;
    out.iteration = ctx.iteration;
    out.snapshot_sequence = ctx.heart.sequence();
    out.represented_souls = {soul};
    out.expression = s->expression;
    out.evidence = {{"proposal", ctx.instance_id, checksum(proposal), 0, ctx.heart.sequence()},
                    {"seed", ctx.trace_id, checksum(ctx.seed), 0, ctx.heart.sequence()},
                    {"rubric", ctx.instance_id, checksum(ctx.rubric.dump()), 1, ctx.heart.sequence()},
                    {"soul", ctx.heart.root_id() + "/" + soul, checksum(s->purpose), s->version,
                     s->declaration_sequence}};
    // Suggestions are returned as data. Nothing here commits, grants permission, or schedules work.
    return out;
}
} // namespace wds::ai::mesh
