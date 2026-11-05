#include "wds/ai/core.hpp"
#include <algorithm>

namespace wds::ai::relationships {
std::string name(InterestStatus s) {
    switch (s) {
    case InterestStatus::eligible:
        return "eligible";
    case InterestStatus::paused:
        return "paused";
    default:
        return "pending";
    }
}
RelationshipDeclaration RelationshipDeclaration::parse(const json &j) {
    require(j.is_object(), "invalid declaration");
    const std::set<std::string> fields = {"schema",       "type",      "request_id",       "root_id",
                                          "soul_id",      "soul_name", "purpose",          "relationship_to",
                                          "relationship", "voice_id",  "expected_version", "paused"};
    for (auto it = j.begin(); it != j.end(); ++it)
        require(fields.count(it.key()), "unknown declaration field: " + it.key());
    RelationshipDeclaration d;
    d.root_id = field(j, "root_id", 80);
    d.soul_id = field(j, "soul_id", 80);
    d.soul_name = field(j, "soul_name", 128);
    d.purpose = field(j, "purpose", 1024);
    d.relationship_to = field(j, "relationship_to", 80);
    d.relationship = field(j, "relationship", 128);
    d.voice_id = field(j, "voice_id", 80);
    for (const auto &id : {d.root_id, d.soul_id, d.relationship_to, d.voice_id})
        identifier(id);
    require(j.contains("expected_version") && j.at("expected_version").is_number_integer() &&
                j.at("expected_version") >= 0 && j.at("expected_version") < 1000000,
            "invalid expected_version");
    d.expected_version = j.at("expected_version");
    if (j.contains("paused"))
        require(j.at("paused").is_boolean(), "paused must be boolean");
    d.paused = j.value("paused", false);
    return d;
}
json RelationshipDeclaration::record() const {
    return {{"root_id", root_id},
            {"soul_id", soul_id},
            {"soul_name", soul_name},
            {"purpose", purpose},
            {"relationship_to", relationship_to},
            {"relationship", relationship},
            {"voice_id", voice_id},
            {"expected_version", expected_version},
            {"paused", paused}};
}
HeartScope::HeartScope(std::string root, uint64_t seq, std::vector<SoulView> souls,
                       std::vector<InterestRecord> interests)
    : root_id_(std::move(root)), sequence_(seq), souls_(std::move(souls)), interests_(std::move(interests)) {}
const SoulView *HeartScope::find(const std::string &id) const {
    for (const auto &s : souls_)
        if (s.id == id)
            return &s;
    return nullptr;
}
const InterestRecord *HeartScope::interest(const std::string &id) const {
    for (const auto &i : interests_)
        if (i.soul_id == id)
            return &i;
    return nullptr;
}
json HeartScope::record() const {
    json souls = json::array(), interests = json::array();
    for (const auto &s : souls_)
        souls.push_back({{"soul_id", s.id},
                         {"name", s.name},
                         {"purpose", s.purpose},
                         {"version", s.version},
                         {"relationship_to", s.relationship_to},
                         {"relationship", s.relationship},
                         {"declaration_sequence", s.declaration_sequence},
                         {"expression", s.expression}});
    for (const auto &i : interests_)
        interests.push_back({{"soul_id", i.soul_id},
                             {"voice_id", i.voice_id},
                             {"strength", i.strength},
                             {"version", i.version},
                             {"status", name(i.status)},
                             {"declaration_sequence", i.declaration_sequence}});
    return {
        {"root_id", root_id_}, {"snapshot_sequence", sequence_}, {"souls", souls}, {"interests", interests}};
}
HeartScope snapshot(const chain::Store &store, const std::string &root) {
    require(store.roots.count(root), "unknown root");
    const auto &r = store.roots.at(root);
    std::vector<SoulView> souls{
        {root, r.at("soul_name"), r.at("purpose"), "", "primary", r.at("version"), 0}};
    std::vector<InterestRecord> interests;
    for (const auto &kv : store.souls()) {
        const auto &s = kv.second;
        if (s.at("root_id") != root)
            continue;
        souls.push_back({s.at("soul_id"), s.at("soul_name"), s.at("purpose"), s.at("relationship_to"),
                         s.at("relationship"), s.at("version"), s.at("declaration_sequence")});
        const auto &i = store.voice_interests().at(kv.first);
        auto status = i.at("status") == "eligible" ? InterestStatus::eligible
                      : i.at("status") == "paused" ? InterestStatus::paused
                                                   : InterestStatus::pending;
        interests.push_back({i.at("soul_id"), i.at("voice_id"), status, i.at("strength"), i.at("version"),
                             i.at("declaration_sequence")});
    }
    for (auto &s : souls)
        for (const auto &key : std::vector<std::string>{root + "/" + root, root + "/" + s.id})
            if (store.profiles.count(key))
                for (auto it = store.profiles.at(key).begin(); it != store.profiles.at(key).end(); ++it)
                    if (it.key() != "soul_id" && it.key() != "key" &&
                        (key == root + "/" + s.id || (it.key() != "gender_type" && it.key() != "pronouns")))
                        s.expression[it.key()] = it.value();
    return HeartScope(root, store.sequence(), std::move(souls), std::move(interests));
}
json prepare(const chain::Store &store, const RelationshipDeclaration &d, const std::string &trace,
             bool voice_available) {
    require(store.roots.count(d.root_id), "unknown root");
    require(d.soul_id != d.root_id, "primary root cannot be redeclared as a soul");
    require(d.relationship_to != d.soul_id, "self relationship is not allowed");
    auto view = snapshot(store, d.root_id);
    require(view.find(d.relationship_to), "relationship target is outside heart scope");
    auto old = view.find(d.soul_id);
    require(d.expected_version == (old ? old->version : 0), "relationship version conflict");
    require(old || (view.souls().size() < max_souls_per_root && store.souls().size() < max_souls),
            "soul capacity reached");
    require(store.declarations().size() < max_declarations, "declaration ledger capacity reached");
    const auto seq = store.sequence() + 1;
    const int version = d.expected_version + 1;
    auto prior_interest = view.interest(d.soul_id);
    json soul = {{"root_id", d.root_id},
                 {"soul_id", d.soul_id},
                 {"soul_name", d.soul_name},
                 {"purpose", d.purpose},
                 {"relationship_to", d.relationship_to},
                 {"relationship", d.relationship},
                 {"version", version},
                 {"originating_root", d.root_id},
                 {"trace_id", trace},
                 {"declaration_sequence", seq}};
    json interest = {{"root_id", d.root_id},
                     {"soul_id", d.soul_id},
                     {"voice_id", d.voice_id},
                     {"strength", std::min(100, prior_interest ? prior_interest->strength + 1 : 1)},
                     {"version", version},
                     {"status", name(d.paused          ? InterestStatus::paused
                                     : voice_available ? InterestStatus::eligible
                                                       : InterestStatus::pending)},
                     {"originating_root", d.root_id},
                     {"trace_id", trace},
                     {"declaration_sequence", seq}};
    return {{"request_id", trace}, {"declaration", d.record()}, {"soul", soul}, {"interest", interest}};
}
} // namespace wds::ai::relationships
