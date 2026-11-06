#include "wds/ai/core.hpp"
#include <algorithm>
#include <cmath>
namespace wds::ai::runtime {
std::vector<std::string> Engine::read_roots(const std::string &root) const {
    std::vector<std::string> roots{root};
    auto it = store_.scopes.find(root);
    if (it != store_.scopes.end() && !it->second.at("account_root").get<std::string>().empty())
        roots.push_back(it->second.at("account_root"));
    return roots;
}
json Engine::speaker_profile(const std::string &root, const std::string &soul) const {
    auto key = root + "/" + soul;
    json profile = {{"key", key},     {"root_id", root}, {"soul_id", soul}, {"gender_type", "unspecified"},
                    {"pronouns", ""}, {"version", 0}};
    // Root expression supplies defaults; a declared speaker may override them.
    for (const auto &id : std::vector<std::string>{root + "/" + root, key})
        if (store_.profiles.count(id))
            for (auto it = store_.profiles.at(id).begin(); it != store_.profiles.at(id).end(); ++it)
                if (it.key() != "key" && it.key() != "root_id" && it.key() != "soul_id" &&
                    it.key() != "version" &&
                    (id == key || (it.key() != "gender_type" && it.key() != "pronouns")))
                    profile[it.key()] = it.value();
    profile["root_profile_version"] = store_.profiles.count(root + "/" + root)
                                          ? store_.profiles.at(root + "/" + root).at("version")
                                          : json(0);
    profile["version"] = store_.profiles.count(key) ? store_.profiles.at(key).at("version") : json(0);
    if (!profile.contains("form"))
        profile["form"] = "unspecified";
    if (!profile.contains("presence"))
        profile["presence"] = json::array();
    if (!profile.contains("interests"))
        profile["interests"] = json::array();
    json prefs = {{"tone", "warm"},
                  {"familiarity", "standard"},
                  {"preferred_gender_types", json::array()},
                  {"cross_soul_noise", "minimal"},
                  {"conversational_direction", "follow_user"}};
    auto grants = read_roots(root);
    for (auto it = grants.rbegin(); it != grants.rend(); ++it)
        if (store_.preferences.count(*it)) {
            const auto &saved = store_.preferences.at(*it);
            for (auto field : {"tone", "familiarity", "preferred_gender_types", "preferred_soul_id"})
                if (saved.contains(field))
                    prefs[field] = saved.at(field);
        }
    profile["preferences"] = prefs;
    profile["source"] = store_.profiles.count(key)                 ? "declared profile"
                        : store_.profiles.count(root + "/" + root) ? "inherited root expression"
                                                                   : "unconfigured; no gender inferred";
    return profile;
}
json Engine::resolve_soul(const std::string &root, const std::string &text,
                          const std::string &explicit_soul) const {
    auto view = relationships::snapshot(store_, root);
    if (!explicit_soul.empty()) {
        require(view.find(explicit_soul), "requested soul is outside this scope");
        return {{"soul_id", explicit_soul}, {"reason", "requested speaker preference"}};
    }
    auto prefs = speaker_profile(root, root).at("preferences");
    auto bound = prefs.value("preferred_soul_id", std::string{});
    std::string selected = root;
    double best = 0, second = 0;
    for (const auto &s : view.souls())
        if (s.id != root) {
            auto interest = view.interest(s.id);
            if (!interest || interest->status != relationships::InterestStatus::eligible)
                continue;
            double score = mesh::relevance(text, s.purpose);
            auto weight_key = root + "/soul/" + s.id;
            if (score > 0 && store_.soul_atoms.count(weight_key))
                score *= 1. + 0.1 * store_.soul_atoms.at(weight_key).weight;
            auto profile = speaker_profile(root, s.id);
            const auto &preferred = profile.at("preferences").at("preferred_gender_types");
            if (score > 0 &&
                std::find(preferred.begin(), preferred.end(), profile.at("gender_type")) != preferred.end())
                score += 0.05;
            if (score > best) {
                second = best;
                best = score;
                selected = s.id;
            } else
                second = std::max(second, score);
        }
    if ((best == 0 || best - second < 0.15) && !bound.empty() && view.find(bound))
        return {{"soul_id", bound}, {"reason", "root-level preferred soul binding"}};
    if (best == 0 || best - second < 0.15)
        return {{"soul_id", root}, {"reason", "ambiguous or absent match; primary scope heart"}};
    return {{"soul_id", selected}, {"reason", "unambiguous match within the current scope"}};
}
json Engine::choose_speaker(const std::string &root, const std::string &text, const std::string &requested,
                            const std::string &instance) const {
    auto preferred = resolve_soul(root, text, requested);
    json result = {{"requested_soul", preferred.at("soul_id")},
                   {"selected_soul", preferred.at("soul_id")},
                   {"reason", preferred.at("reason")},
                   {"idea_key", ""},
                   {"scope", root}};
    auto index = store_.ideas_by_root.find(root);
    if (index == store_.ideas_by_root.end()) {
        result["profile"] = speaker_profile(root, result.at("selected_soul"));
        return result;
    }
    double best = -1;
    auto view = relationships::snapshot(store_, root);
    for (const auto &key : index->second) {
        const auto &idea = store_.ideas.at(key);
        auto status = idea.at("status");
        if (status != "pending" &&
            !(status == "reserved" && idea.value("instance_id", std::string{}) == instance))
            continue;
        if (idea.at("eligible_ms").get<int64_t>() > now_ms())
            continue;
        auto target = idea.at("speaker_soul_id").get<std::string>();
        if (!view.find(target))
            continue;
        double relevance = mesh::relevance(text, idea.at("topic"));
        if (relevance < 0.5)
            continue; // Weak associations stay saved instead of interrupting the chosen speaker.
        double score = relevance * (1. + idea.at("priority").get<double>() / 100.);
        if (score > best) {
            best = score;
            result["selected_soul"] = target;
            result["idea_key"] = key;
            result["reason"] = "engram alignment proposal: " + idea.at("reason").get<std::string>();
            result["evidence_id"] = idea.at("engram_id");
        }
    }
    result["profile"] = speaker_profile(root, result.at("selected_soul"));
    return result;
}
bool Engine::scope_command(const json &m, Emit emit) {
    auto op = m.at("type").get<std::string>();
    if (op != "scope.bind" && op != "route.resolve" && op != "reflection.record" && op != "engram.suggest" &&
        op != "engram.defer" && op != "soul.inventory" && op != "soul.weight" && op != "speaker.profile" &&
        op != "persona.preferences")
        return false;
    auto root = field(m, "root_id", 80), trace = field(m, "request_id", 80);
    require(store_.roots.count(root), "unknown root scope");
    if (op == "soul.inventory") {
        if (m.contains("offset"))
            upkeep::integer(m, "offset", 0, 1000000);
        if (m.contains("limit"))
            upkeep::integer(m, "limit", 1, 256);
        size_t offset = m.value("offset", size_t(0)), limit = m.value("limit", size_t(64));
        require(limit > 0 && limit <= 256, "inventory limit must be 1..256");
        json rows = json::array();
        size_t n = 0, total = 0;
        for (const auto &entry : store_.soul_atoms)
            if (entry.first.rfind(root + "/", 0) == 0) {
                ++total;
                if (n++ < offset || rows.size() >= limit)
                    continue;
                const auto &a = entry.second;
                rows.push_back({{"key", entry.first},
                                {"identity", a.identity},
                                {"weight", a.weight},
                                {"source_sequence", a.source_sequence},
                                {"declared_identity", (a.flags & 1) != 0},
                                {"version", store_.soul_weights.count(entry.first)
                                                ? store_.soul_weights.at(entry.first).at("version")
                                                : json(0)}});
            }
        emit({{"schema", 1},
              {"type", "soul.inventory"},
              {"root_id", root},
              {"trace_id", trace},
              {"data",
               {{"owner_ai_soul", "wds.ai"},
                {"numeric_bytes_per_item", sizeof(soul::Atom)},
                {"weight_meaning", "attention priority; not intrinsic worth or speaking permission"},
                {"items", rows},
                {"total", total},
                {"next_offset", offset + rows.size() < total ? json(offset + rows.size()) : json()}}}});
        return true;
    }
    if (op == "route.resolve") {
        auto text = field(m, "text", 4096);
        auto requested = resolve_soul(root, text, m.value("soul_id", std::string{}));
        emit({{"schema", 1},
              {"type", "route.selected"},
              {"root_id", root},
              {"trace_id", trace},
              {"data", choose_speaker(root, text, requested.at("soul_id"))}});
        return true;
    }
    auto key = root + "/" + trace;
    auto intent = m;
    intent.erase("request_id");
    auto digest = checksum(intent.dump());
    if (store_.upkeep_requests.count(key)) {
        auto old = store_.upkeep_requests.at(key);
        require(old.at("data").at("maintenance").at("intent") == digest, "request_id conflict");
        emit(old);
        return true;
    }
    require(store_.upkeep_requests.size() < 16384, "request ledger capacity reached");
    auto commit = [&](std::string type, json changes, json data) {
        changes["request_key"] = key;
        changes["intent"] = digest;
        data["maintenance"] = changes;
        return event(type, root, trace, data, emit);
    };
    auto gender_valid = [](const json &v) {
        return v == "man" || v == "woman" || v == "nonbinary" || v == "neutral" || v == "unspecified";
    };
    if (op == "speaker.profile") {
        auto soul = field(m, "soul_id", 80);
        require(relationships::snapshot(store_, root).find(soul), "profile soul outside scope");
        auto id = root + "/" + soul;
        auto version = store_.profiles.count(id) ? store_.profiles.at(id).at("version").get<int>() : 0;
        require(m.at("expected_version") == version, "profile version conflict");
        if (m.contains("gender_type"))
            require(gender_valid(m.at("gender_type")), "unsupported declared gender_type");
        auto existing = store_.profiles.count(id) ? store_.profiles.at(id) : json::object();
        auto pronouns = m.value("pronouns", existing.value("pronouns", std::string{}));
        require(pronouns.size() <= 80, "pronouns too long");
        json profile = {{"key", id},
                        {"root_id", root},
                        {"soul_id", soul},
                        {"gender_type",
                         m.value("gender_type", existing.value("gender_type", std::string("unspecified")))},
                        {"pronouns", pronouns},
                        {"version", version + 1}};
        for (auto field : {"form", "description", "presence", "interests"}) {
            if (existing.contains(field))
                profile[field] = existing.at(field);
            if (!m.contains(field))
                continue;
            if (std::string(field) == "presence" || std::string(field) == "interests") {
                require(m.at(field).is_array() && m.at(field).size() <= 16,
                        "at most sixteen expression tags per field");
                for (const auto &v : m.at(field))
                    require(v.is_string() && !v.get<std::string>().empty() &&
                                v.get<std::string>().size() <= 80,
                            "invalid expression tag");
            } else
                require(m.at(field).is_string() && m.at(field).get<std::string>().size() <=
                                                       (std::string(field) == "form" ? 80 : 512),
                        "expression text too long");
            profile[field] = m.at(field);
        }
        commit("speaker.profiled", {{"profiles", {profile}}}, {{"profile", profile}});
        return true;
    }
    if (op == "persona.preferences") {
        int version =
            store_.preferences.count(root) ? store_.preferences.at(root).at("version").get<int>() : 0;
        require(m.at("expected_version") == version, "preference version conflict");
        auto values = m.at("preferences");
        require(values.is_object(), "preferences must be an object");
        for (auto it = values.begin(); it != values.end(); ++it)
            require(it.key() == "tone" || it.key() == "familiarity" || it.key() == "preferred_gender_types" ||
                        it.key() == "preferred_soul_id",
                    "unknown preference");
        if (values.contains("preferred_gender_types")) {
            auto types = values.at("preferred_gender_types");
            require(types.is_array() && types.size() <= 5, "invalid preferred gender list");
            for (const auto &v : types)
                require(gender_valid(v), "unknown gender preference");
        }
        if (values.contains("preferred_soul_id"))
            require(values.at("preferred_soul_id").is_string() &&
                        (values.at("preferred_soul_id") == "" ||
                         relationships::snapshot(store_, root).find(values.at("preferred_soul_id"))),
                    "preferred soul must belong to this scope");
        if (values.contains("tone"))
            require(values.at("tone") == "warm" || values.at("tone") == "direct" ||
                        values.at("tone") == "formal" || values.at("tone") == "playful",
                    "unknown tone");
        if (values.contains("familiarity"))
            require(values.at("familiarity") == "standard" || values.at("familiarity") == "close" ||
                        values.at("familiarity") == "reserved",
                    "unknown familiarity");
        if (store_.preferences.count(root))
            for (auto it = store_.preferences.at(root).begin(); it != store_.preferences.at(root).end(); ++it)
                if (!values.contains(it.key()))
                    values[it.key()] = it.value();
        values["key"] = root;
        values["version"] = version + 1;
        commit("persona.configured", {{"preferences", {values}}}, {{"preferences", values}});
        return true;
    }
    if (op == "soul.weight") {
        auto id = field(m, "item_key", 256);
        require(id.rfind(root + "/", 0) == 0 && store_.soul_atoms.count(id), "item outside AI soul scope");
        int version = store_.soul_weights.count(id) ? store_.soul_weights.at(id).at("version").get<int>() : 0;
        require(m.at("expected_version") == version, "weight version conflict");
        auto weight = m.at("weight").get<double>();
        require(std::isfinite(weight) && weight >= 0 && weight <= 1, "weight must be finite in 0..1");
        json row = {{"key", id}, {"weight", weight}, {"version", version + 1}};
        commit("soul.weighted", {{"weights", {row}}}, {{"item", row}});
        return true;
    }
    if (op == "scope.bind") {
        int version = store_.scopes.count(root) ? store_.scopes.at(root).at("version").get<int>() : 0;
        require(m.at("expected_version") == version, "scope version conflict");
        auto kind = field(m, "kind", 16), account = m.value("account_root", std::string{});
        require(kind == "project" || kind == "account", "scope kind must be project or account");
        if (!account.empty())
            require(kind == "project" && account != root && store_.scopes.count(account) &&
                        store_.scopes.at(account).at("kind") == "account",
                    "account read grant requires a separate declared account scope");
        if (kind != "account")
            for (const auto &entry : store_.scopes)
                require(entry.second.at("account_root") != root,
                        "scope is still used as shared account memory");
        json scope = {{"key", root}, {"kind", kind}, {"account_root", account}, {"version", version + 1}};
        commit("scope.bound", {{"scopes", {scope}}}, {{"scope", scope}});
        return true;
    }
    if (op == "reflection.record") {
        auto actor_root = m.value("actor_root", root);
        auto grants = read_roots(root);
        require(std::find(grants.begin(), grants.end(), actor_root) != grants.end(),
                "actor scope is not connected to this project");
        auto actor = field(m, "actor_soul_id", 80), kind = field(m, "actor_kind", 16);
        require(kind == "human" || kind == "voice", "actor_kind must be human or voice");
        require(relationships::snapshot(store_, actor_root).find(actor), "unknown attributed speaker");
        auto text = field(m, "text", 2048);
        auto target = resolve_soul(root, text, m.value("soul_id", std::string{}));
        auto episode = m.value("instance_id", std::string{});
        json link = json::object();
        if (!episode.empty()) {
            require(store_.instances.count(episode) && store_.instances.at(episode).at("root_id") == root,
                    "attempt is outside this scope");
            const auto &x = store_.instances.at(episode);
            link = {{"instance_id", episode},
                    {"policy_key", x.value("package_key", std::string{})},
                    {"artifact_hash", checksum(x.at("artifact"))},
                    {"iteration", x.at("iteration")}};
        }
        auto id = "reflection-" + checksum(key);
        json packet = {{"schema", 1},
                       {"intent", "memory.upsert"},
                       {"hops", 0},
                       {"root_id", root},
                       {"engram_id", id},
                       {"packet_id", id},
                       {"expected_version", 0},
                       {"expires_ms", now_ms() + 60000},
                       {"text", text},
                       {"source", "attributed " + kind + " feedback: " + actor_root + "/" + actor},
                       {"claim_status", "hypothetical"}};
        auto memory = mesh::prepare_memory(store_, packet, trace);
        memory["memory_kind"] = "reflection";
        memory["learning_link"] = link;
        memory["attribution"] = {{"actor_root", actor_root},
                                 {"actor_soul_id", actor},
                                 {"actor_kind", kind},
                                 {"target_soul_id", target.at("soul_id")},
                                 {"identity_basis", "declared local identity"}};
        if(m.contains("chat_id")) {
            auto chat=field(m,"chat_id",80); require(store_.chats.count(chat),"unknown source chat");
            memory["attribution"]["chat_id"]=chat;
        }
        auto n = neuron(root, target.at("soul_id"));
        n["attention_credit"] = std::min(0.2, n.value("attention_credit", 0.) + 0.05);
        n["attention_source"] = memory.at("id");
        commit("reflection.recorded", {{"memories", {memory}}, {"neurons", {n}}},
               {{"engram_id", memory.at("id")}, {"target", target}, {"learning_link", link}});
        memory_changed(root, memory.at("id"), emit);
        return true;
    }
    if (op == "engram.suggest") {
        require(store_.ideas.size() < 4096 && store_.ideas_by_root[root].size() < 32,
                "saved idea capacity reached");
        auto local = field(m, "engram_id", 80), speaker = field(m, "speaker_soul_id", 80);
        identifier(local);
        require(store_.engrams.count(root + "/" + local), "idea evidence outside this scope");
        require(relationships::snapshot(store_, root).find(speaker), "idea speaker outside this scope");
        auto priority = m.value("priority", 50);
        auto delay = m.value("due_in_ms", int64_t(0));
        require(priority >= 0 && priority <= 100 && delay >= 0 && delay <= 604800000,
                "invalid idea priority/delay");
        json idea = {{"key", "idea-" + checksum(key)},
                     {"root_id", root},
                     {"speaker_soul_id", speaker},
                     {"engram_id", root + "/" + local},
                     {"topic", field(m, "topic", 512)},
                     {"reason", field(m, "reason", 512)},
                     {"priority", priority},
                     {"eligible_ms", now_ms() + delay},
                     {"status", "pending"},
                     {"version", 1}};
        commit("engram.idea_saved", {{"ideas", {idea}}}, {{"idea", idea}});
        return true;
    }
    auto id = field(m, "idea_key", 80);
    require(store_.ideas.count(id) && store_.ideas.at(id).at("root_id") == root, "idea outside scope");
    auto idea = store_.ideas.at(id);
    require(m.at("expected_version") == idea.at("version"), "idea version conflict");
    require(idea.at("status") != "reserved", "idea is in flight; cancel the episode before deferring it");
    auto delay = upkeep::integer(m, "due_in_ms", 0, 604800000);
    idea["eligible_ms"] = now_ms() + delay;
    idea["status"] = "pending";
    idea["version"] = idea.at("version").get<int>() + 1;
    commit("engram.idea_saved", {{"ideas", {idea}}}, {{"idea", idea}});
    return true;
}
} // namespace wds::ai::runtime
