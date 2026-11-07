#include "wds/ai/core.hpp"
#include <algorithm>
#include <cmath>

namespace wds::ai::runtime {
namespace {
bool pending(const json &p) { return p.at("status") == "queued" || p.at("status") == "running" || p.at("status") == "waiting_web"; }
bool conflict(const json &evidence) {
    std::map<std::string, std::string> values;
    for (const auto &e : evidence) if (e.contains("semantic") && e.at("semantic").value("exclusive", false)) {
        const auto &s = e.at("semantic");
        auto key = s.at("subject").get<std::string>() + "\n" + s.at("predicate").get<std::string>();
        auto value = s.at("object").get<std::string>();
        if (values.count(key) && values.at(key) != value) return true;
        values[key] = value;
    }
    return false;
}
}
json Engine::assembly_start(const json &p) const {
    return {{"key", "assembly-" + checksum(p.at("key"))}, {"lineage", p.at("lineage")},
            {"root_id", p.at("root_id")}, {"input", p.at("text")}, {"origin_pulse", p.at("key")},
            {"chat_id", p.value("chat_id", std::string{})}, {"background", p.value("background", false)},
            {"requested_soul", store_.mesh_neurons.at(p.at("node_key")).at("soul_id")},
            {"created_sequence", store_.sequence() + 1}, {"version", 1}, {"status", "collecting"},
            {"deadline_ms", p.at("due_ms").get<int64_t>() + store_.mesh_settings.at("response_deadline_ms").get<int64_t>()},
            {"expression_package", active_key("expression")}, {"contributions", json::array()},
            {"wind_contributions", p.contains("wind_contribution") ? json::array({p.at("wind_contribution")}) : json::array()},
            {"evidence", json::array()}, {"unresolved", json::array()}, {"pending", 1}, {"omitted_sources", 0},
            {"conflict", false}, {"expression", json::object()}};
}
bool Engine::assembly_command(const json &m, Emit emit) {
    const auto op = m.at("type").get<std::string>();
    if (op.rfind("assembly.", 0) != 0) return false;
    const auto root = field(m, "root_id", 80), trace = m.at("request_id").get<std::string>();
    require(store_.roots.count(root), "unknown assembly root");
    if (op == "assembly.inspect") {
        auto key = field(m, "assembly_key", 80);
        require(store_.assemblies.count(key) && store_.assemblies.at(key).at("root_id") == root, "unknown scoped assembly");
        emit({{"schema", 1}, {"type", "assembly.snapshot"}, {"trace_id", trace}, {"data", store_.assemblies.at(key)}}); return true;
    }
    if (op == "assembly.list") {
        json rows = json::array(); std::vector<json> ordered;
        const auto before = m.value("before", store_.sequence() + 1);
        for (const auto &key : store_.assemblies_by_root[root]) {
            const auto &a = store_.assemblies.at(key);
            if (a.at("created_sequence").get<uint64_t>() < before) ordered.push_back(a);
        }
        std::sort(ordered.begin(), ordered.end(), [](const json &a, const json &b) { return a.at("created_sequence") > b.at("created_sequence"); });
        for (const auto &a : ordered) { if (rows.size() == 20) break;
            rows.push_back({{"key", a.at("key")}, {"status", a.at("status")}, {"input", a.at("input")},
                            {"created_sequence", a.at("created_sequence")}, {"contributions", a.at("contributions").size()},
                            {"sources", a.at("evidence").size()}, {"pending", a.at("pending")}});
        }
        emit({{"schema", 1}, {"type", "assembly.listed"}, {"trace_id", trace},
              {"data", {{"assemblies", rows}, {"next_before", ordered.size() > 20 ? rows.back().at("created_sequence") : json()},
                        {"actions", expression::actions()}}}}); return true;
    }
    if (op == "assembly.training.export") {
        json rows = json::array();
        for (const auto &[key, row] : store_.expression_samples) if (row.at("root_id") == root) rows.push_back(row);
        require(rows.dump().size() <= 180000, "expression export capacity reached; retain the journal before starting another dataset");
        emit({{"schema", 1}, {"type", "assembly.training.rows"}, {"trace_id", trace},
              {"data", {{"feature_schema", expression::feature_schema}, {"samples", rows}}}}); return true;
    }
    auto intent = m; intent.erase("request_id"); const auto digest = checksum(intent.dump()), request = root + "/" + trace;
    if (store_.assembly_requests.count(request)) {
        const auto &old = store_.assembly_requests.at(request);
        require(old.at("data").at("mesh").at("assembly_intent") == digest, "assembly request_id conflict"); emit(old); return true;
    }
    require(store_.assembly_requests.size() < 16384, "assembly command ledger capacity reached");
    auto commit = [&](const std::string &type, json delta, json extra = json::object()) {
        delta["assembly_request_key"] = request; delta["assembly_intent"] = digest; extra["mesh"] = delta;
        event(type, root, trace, extra, emit);
    };
    if (op == "assembly.teach") {
        auto id = field(m, "engram_id", 80); identifier(id);
        auto subject = field(m, "subject", 256), predicate = field(m, "predicate", 80), object = field(m, "object", 1024);
        require(m.value("exclusive", json(false)).is_boolean(), "exclusive must be boolean");
        json packet = {{"schema", 1}, {"intent", "memory.upsert"}, {"root_id", root}, {"engram_id", id},
                       {"packet_id", "teaching-" + checksum(request)}, {"expected_version", m.at("expected_version")},
                       {"hops", 0}, {"expires_ms", now_ms() + 60000}, {"text", subject + " " + predicate + " " + object},
                       {"source", field(m, "source", 256)}, {"claim_status", m.value("claim_status", std::string("hypothetical"))}};
        auto memory = mesh::IGF2BP2Handler(store_).prepare_rna(packet, trace);
        memory["memory_kind"] = "taught_statement";
        memory["semantic"] = {{"subject", subject}, {"predicate", predicate}, {"object", object},
                              {"exclusive", m.value("exclusive", false)}, {"schema", "statement/1"}};
        memory["attribution"] = {{"actor_kind", "human"}, {"actor_soul_id", "operator"}, {"identity_basis", "local operator command"}};
        commit("assembly.taught", json::object(), {{"engram_id", memory.at("id")}, {"version", memory.at("version")},
               {"maintenance", {{"memories", json::array({memory})}}}});
        memory_changed(root, memory.at("id"), emit); return true;
    }
    if (op == "assembly.attune") {
        auto id = field(m, "engram_id", 80); identifier(id);
        const auto key = root + "/" + id;
        require(store_.engrams.count(key), "unknown scoped engram");
        require(m.at("importance").is_number(), "importance must be numeric");
        double importance = m.at("importance");
        require(std::isfinite(importance) && importance >= 0 && importance <= 1, "importance must be in [0,1]");
        const auto &old = store_.engrams.at(key);
        json packet = {{"schema", 1}, {"intent", "memory.upsert"}, {"root_id", root}, {"engram_id", id},
                       {"packet_id", "attune-" + checksum(request)}, {"expected_version", m.at("expected_version")},
                       {"hops", 0}, {"expires_ms", now_ms() + 60000}, {"text", old.at("text")},
                       {"source", old.at("source")}, {"claim_status", old.at("claim_status")}};
        auto memory = old;
        auto prepared = mesh::IGF2BP2Handler(store_).prepare_rna(packet, trace);
        for (auto field : {"version", "packet_id", "trace_id"}) memory[field] = prepared.at(field);
        memory["attunement"] = {{"schema", "astro.attunement/1"}, {"importance", importance},
                                {"actor", "operator"}, {"source_sequence", store_.sequence() + 1}};
        commit("assembly.attuned", json::object(), {{"engram_id", key}, {"version", memory.at("version")},
               {"active_policy_changed", false}, {"maintenance", {{"memories", json::array({memory})}}}});
        memory_changed(root, key, emit); return true;
    }
    if (op == "assembly.feedback") {
        const auto key = field(m, "assembly_key", 80), action = field(m, "action", 32);
        require(store_.assemblies.count(key) && store_.assemblies.at(key).at("root_id") == root, "unknown scoped assembly");
        const auto &a = store_.assemblies.at(key); require(a.at("status") != "collecting" && a.at("expression").contains("features"), "response has no completed expression features");
        auto names = expression::actions(); auto it = std::find(names.begin(), names.end(), action);
        require(it != names.end(), "unknown expression action");
        require(!store_.expression_samples.count(key) && store_.expression_samples.size() < 4096, "response already labeled or sample capacity reached");
        auto note = m.value("note", std::string{}); require(note.size() <= 1024, "feedback note too long");
        json sample = {{"key", key}, {"root_id", root}, {"lineage", a.at("lineage")}, {"features", a.at("expression").at("features")},
                       {"target", it - names.begin()}, {"action", action}, {"note", note}, {"actor", "operator"},
                       {"assembly_key", key}, {"feature_schema", expression::feature_schema}, {"source_sequence", store_.sequence() + 1}};
        commit("assembly.feedback_recorded", {{"expression_samples", json::array({sample})}}, {{"active_policy_changed", false}}); return true;
    }
    throw std::runtime_error("unknown assembly command");
}
void Engine::assembly_event(const json &e, Emit emit) {
    if (e.at("type").get<std::string>().rfind("mesh.", 0) != 0 || !e.at("data").contains("mesh")) return;
    const auto &delta = e.at("data").at("mesh"); if (!delta.contains("pulses")) return;
    std::set<std::string> affected;
    for (const auto &p : delta.at("pulses")) if (store_.assembly_by_lineage.count(p.at("lineage"))) affected.insert(store_.assembly_by_lineage.at(p.at("lineage")));
    for (const auto &key : affected) {
        auto a = store_.assemblies.at(key); if (a.at("status") != "collecting") continue;
        const auto &origin = store_.mesh_pulses.at(a.at("origin_pulse"));
        if (origin.at("status") == "cancelled") { a["cancelled"] = true; assembly_finish(a, emit, "operator cancelled response"); continue; }
        const auto root = a.at("root_id").get<std::string>();
        auto grants = read_roots(root);
        json contributions = json::array(), evidence = json::array(), unresolved = json::array();
        std::set<std::string> seen; size_t waiting = 0, omitted = 0;
        for (const auto &source : a.value("wind_contributions", json::array()))
            if (std::find(grants.begin(), grants.end(), source.at("root_id").get<std::string>()) != grants.end()) {
                evidence.push_back(source); seen.insert(source.at("id").get<std::string>() + "/1");
            }
        for (const auto &pulse_key : store_.pulses_by_lineage.at(a.at("lineage"))) {
            const auto &p = store_.mesh_pulses.at(pulse_key);
            if (pending(p)) ++waiting;
            if (!pending(p) && p.at("status") != "completed" && p.at("status") != "coalesced")
                unresolved.push_back({{"pulse_key", pulse_key}, {"reason", p.at("reason")}});
            if (!p.contains("decision")) continue;
            const auto &d = p.at("decision");
            contributions.push_back({{"pulse_key", pulse_key}, {"node_key", p.at("node_key")}, {"status", p.at("status")},
                {"action", d.value("action", std::string("hold"))}, {"strength", d.value("strength", 0.f)},
                {"uncertain", d.value("uncertain", true)}, {"features", d.value("features", json::array())},
                {"policy_binding", p.value("package_key", std::string{})}, {"model_binding", d.value("knowledge", json::object())},
                {"source_sequence", p.at("source_sequence")}});
            if (!d.contains("payload")) continue;
            for (const auto &source : d.at("payload").at("evidence")) {
                if (std::find(grants.begin(), grants.end(), source.at("root_id").get<std::string>()) == grants.end()) continue;
                auto identity = source.at("id").get<std::string>() + "/" + std::to_string(source.at("version").get<int>());
                if (!seen.insert(identity).second) continue;
                if (evidence.size() >= store_.mesh_settings.at("max_response_sources").get<size_t>()) { ++omitted; continue; }
                evidence.push_back(source);
            }
        }
        std::stable_sort(evidence.begin(), evidence.end(), [](const json &x, const json &y) {
            return x.value("attention_score", x.at("relevance").get<float>()) > y.value("attention_score", y.at("relevance").get<float>());
        });
        a["contributions"] = contributions; a["evidence"] = evidence; a["unresolved"] = unresolved;
        a["pending"] = waiting; a["omitted_sources"] = omitted; a["conflict"] = conflict(evidence);
        a["version"] = a.at("version").get<int>() + 1;
        if (contributions.size() >= store_.mesh_settings.at("max_response_contributions").get<size_t>() && waiting)
            assembly_finish(a, emit, "response contribution budget reached");
        else if (!waiting) assembly_finish(a, emit);
        else event("assembly.updated", root, a.at("key"), {{"assembly_key", key}, {"mesh", {{"assemblies", json::array({a})}}}}, emit);
    }
}
void Engine::assembly_finish(json a, Emit emit, const std::string &reason) {
    const auto root = a.at("root_id").get<std::string>();
    // Recheck grants even when finishing from a deadline, without another mesh event.
    const auto grants = read_roots(root);
    json authorized = json::array();
    for (const auto &source : a.at("evidence"))
        if (std::find(grants.begin(), grants.end(), source.at("root_id").get<std::string>()) != grants.end()) authorized.push_back(source);
    if (authorized.size() != a.at("evidence").size())
        a["unresolved"].push_back({{"reason", "source scope changed while response was collecting"}});
    a["evidence"] = authorized; a["conflict"] = conflict(authorized);
    json pulses = json::array(), nodes = json::array(), instances = json::array(), web = json::array();
    if (!reason.empty()) {
        a["unresolved"].push_back({{"reason", reason}});
        for (const auto &key : store_.pulses_by_lineage.at(a.at("lineage"))) {
            auto p = store_.mesh_pulses.at(key); if (!pending(p)) continue;
            if (p.at("status") == "running") {
                auto n = store_.mesh_neurons.at(p.at("node_key")); n["active"] = ""; nodes.push_back(n);
                auto x = store_.instances.at(p.at("instance_id")); x["generation"] = x.value("generation", 0) + 1;
                x["status"] = "cancelled"; x["error"] = reason; instances.push_back(x);
            }
            p["status"] = "cancelled"; p["reason"] = reason; pulses.push_back(p);
            for (const auto &[id, item] : store_.mesh_web) if (item.at("pulse_key") == key &&
                (item.at("status") == "pending" || item.at("status") == "claimed")) {
                auto cancelled = item; cancelled["status"] = "cancelled"; web.push_back(cancelled);
            }
        }
    }
    a["pending"] = 0;
    a["speaker"] = choose_speaker(root, a.at("input"), a.at("requested_soul"));
    const auto view = relationships::snapshot(store_, root);
    a["speaker"]["display_name"] = view.find(a.at("speaker").at("selected_soul"))->name;
    try {
        // Only the origin's committed cognitive plan can realize an instruction.
        const auto &origin=store_.mesh_pulses.at(a.at("origin_pulse"));
        if (reason.empty() && origin.contains("decision") && origin.at("decision").contains("cognitive_plan") && a.at("conflict")==false) {
            const auto &plan=origin.at("decision").at("cognitive_plan");
            a["expression"]=language_mode_=="legacy" ? expression::compose(a, *packages_.at(a.at("expression_package"))) : json{{"sources",json::array()}};
            a["expression"]["text"]=plan.at("text");a["expression"]["cognition"]=plan.at("trace");
            a["expression"]["renderer"]=plan.at("trace").value("renderer",std::string("native-cognition/1"));a["expression"]["generated_tokens"]=plan.at("trace").value("generated_tokens",0);
        } else if(reason.empty() && origin.contains("decision") && origin.at("decision").contains("language_failure") && a.at("conflict")==false) {
            const auto &failure=origin.at("decision").at("language_failure");
            a["expression"]={{"sources",json::array()},{"text",failure.at("text")},{"cognition",failure.at("trace")},{"renderer",failure.at("trace").at("renderer")}};
            a["error"]=failure.at("trace").at("error");
        } else if(language_mode_!="legacy" && origin.value("direct",false)) {
            const std::string detail=!reason.empty()?reason:a.at("conflict")==true?"conflicting evidence needs review":"no completed language contribution";
            a["expression"]={{"sources",json::array()},{"text","This response was held: "+detail},{"renderer","language-status/1"}};
        } else a["expression"] = expression::compose(a, *packages_.at(a.at("expression_package")));
        a["status"] = a.value("cancelled", false) ? "cancelled" : a.at("conflict") == true ? "held" :
                      !a.at("unresolved").empty() ? "incomplete" : "completed";
    } catch (const std::exception &error) {
        a["status"] = "failed"; a["error"] = error.what();
        a["expression"] = {{"text", "I couldn't complete this response: " + std::string(error.what())}, {"sources", json::array()}};
    }
    a["finished_ms"] = now_ms(); a["version"] = a.at("version").get<int>() + 1;
    a["completed_sequence"] = store_.sequence() + 1;
    // Index exact application readouts, not speculative addresses of facts in a base model.
    a["weight_bindings"] = json::array(); std::set<std::string> bound;
    auto bind = [&](const std::string &key, const std::string &role) {
        if (!bound.insert(key).second || !packages_.count(key)) return;
        const auto &manifest = packages_.at(key)->manifest;
        const auto schema = manifest.value("feature_schema", std::string{});
        if (schema != neural::feature_schema && schema != expression::feature_schema) return;
        a["weight_bindings"].push_back({{"package_key", key}, {"checksum", manifest.at("checksum_fnv1a64")},
            {"role", role}, {"feature_schema", schema}, {"artifact", "policy.wind"},
            {"rows", {0, 1, 2, 3, 4, 5, 6}}, {"columns", 16}, {"bias_count", 7},
            {"activation", "already selected for this episode; reference grants no activation"}});
    };
    bind(a.at("expression_package"), "expression");
    for (const auto &c : a.at("contributions")) bind(c.at("policy_binding"), "neural movement");
    // An outcome trace links to the assembled record; it is never reused as factual source evidence.
    const auto id = a.at("key").get<std::string>();
    json packet = {{"schema", 1}, {"intent", "memory.upsert"}, {"root_id", root}, {"engram_id", id}, {"packet_id", id},
                   {"expected_version", 0}, {"hops", 0}, {"expires_ms", now_ms() + 60000},
                   {"text", "Response assembly " + id + " retained its contribution and expression provenance."},
                   {"source", "assembly:" + id}, {"claim_status", "hypothetical"}};
    json memories = json::array();
    try {
        auto memory = mesh::IGF2BP2Handler(store_).prepare_rna(packet, id); memory["memory_kind"] = "expression_trace";
        memory["learning_link"] = {{"assembly_key", id}, {"response_checksum", checksum(a.at("expression").at("text"))},
                                    {"expression_package", a.at("expression_package")}, {"status", a.at("status")},
                                    {"weight_bindings", a.at("weight_bindings")}, {"memory_versions", json::array()}};
        for (const auto &source : a.at("evidence"))
            memory["learning_link"]["memory_versions"].push_back({{"id", source.at("id")}, {"version", source.at("version")}});
        memories.push_back(memory);
    } catch (const std::exception &error) { a["trace_error"] = error.what(); }
    event("assembly.completed", root, id,
          {{"assembly_key", id}, {"signal_key", a.at("origin_pulse")}, {"status", a.at("status")},
           {"artifact", a.at("expression").at("text")}, {"speaker", a.at("speaker")},
           {"expression_package", a.at("expression_package")},
           {"mesh", {{"assemblies", json::array({a})}, {"pulses", pulses}, {"nodes", nodes}, {"web", web}}},
           {"maintenance", {{"instances", instances}, {"memories", memories}}}}, emit);
}
void Engine::assembly_recover(Emit emit) {
    // Only startup scans the bounded assembly ledger. Normal updates use lineage/due indexes.
    std::vector<std::string> keys; for (const auto &[key, a] : store_.assemblies) keys.push_back(key);
    for (const auto &key : keys) {
        auto a = store_.assemblies.at(key);
        if (a.at("status") == "collecting") {
            json rows = json::array();
            for (const auto &p : store_.pulses_by_lineage.at(a.at("lineage"))) rows.push_back(store_.mesh_pulses.at(p));
            assembly_event({{"type", "mesh.recovered"}, {"data", {{"mesh", {{"pulses", rows}}}}}}, emit);
            continue;
        }
        if (a.contains("delivery") || a.value("chat_id", std::string{}).empty()) continue;
        workspace_event({{"type", "assembly.completed"}, {"root_id", a.at("root_id")}, {"trace_id", key},
            {"seq", a.at("completed_sequence")}, {"data", {{"assembly_key", key}, {"signal_key", a.at("origin_pulse")},
            {"status", a.at("status")}, {"artifact", a.at("expression").at("text")}, {"speaker", a.at("speaker")}}}}, emit);
    }
}
void Engine::assembly_expire(int64_t time, Emit emit) {
    for (size_t count = 0; count < 16 && !store_.assembly_due.empty() && store_.assembly_due.begin()->first <= time; ++count) {
        auto a = store_.assemblies.at(store_.assembly_due.begin()->second);
        assembly_finish(a, emit, "response deadline reached; completed contributions were retained");
    }
}
} // namespace wds::ai::runtime
