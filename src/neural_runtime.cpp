#include "wds/ai/core.hpp"
#include <algorithm>
#include <cmath>

namespace wds::ai::runtime {
namespace {
bool final_pulse(const json &p) {
    const auto s = p.at("status");
    return s != "queued" && s != "running" && s != "waiting_web";
}
json mesh_delta(const std::string &type, const json &row) { return {{type, json::array({row})}}; }
void threshold(const json &m, const char *name) {
    require(m.at(name).is_number(), std::string(name) + " must be a number");
    const double value = m.at(name);
    require(std::isfinite(value) && value >= 0 && value <= 1, std::string(name) + " must be in [0,1]");
}
}
json Engine::neural_node(const std::string &root, const std::string &id, const std::string &soul) const {
    auto key = root + "/" + id;
    if (store_.mesh_neurons.count(key)) return store_.mesh_neurons.at(key);
    auto view = relationships::snapshot(store_, root);
    const auto *s = view.find(soul);
    require(s, "neuron soul is outside its root");
    return {{"key", key}, {"id", id}, {"root_id", root}, {"soul_id", soul}, {"purpose", s->purpose},
            {"version", 0}, {"enabled", true}, {"knowledge", true}, {"web", true},
            {"examples", json::array()}, {"minimum_strength", 0.7}, {"minimum_margin", 0.1},
            {"refractory_until_ms", 0}, {"active", ""}, {"package_key", ""},
            {"impulses", 0}, {"knowledge_calls", 0}, {"cache_hits", 0}, {"last_action", "rest"}};
}
json Engine::neural_pulse(const json &node, const std::string &text, const std::string &source,
                         uint64_t sequence, const std::string &kind) const {
    const auto root = node.at("root_id").get<std::string>();
    auto key = "pulse-" + checksum(node.at("key").get<std::string>() + "/" + source + "/" + std::to_string(sequence));
    return {{"key", key}, {"root_id", root}, {"node_key", node.at("key")}, {"text", text},
            {"source", source}, {"source_sequence", sequence}, {"kind", kind}, {"direct", false},
            {"lineage", key}, {"depth", 0}, {"visited", {node.at("key")}}, {"attempt", 0},
            {"strength", 0.5}, {"status", "queued"}, {"due_ms", now_ms()}, {"instance_id", ""},
            {"reason", "waiting for shared compute budget"}};
}
void Engine::neural_recover(Emit emit) {
    auto running = store_.mesh_running;
    for (const auto &key : running) {
        auto p = store_.mesh_pulses.at(key), n = store_.mesh_neurons.at(p.at("node_key"));
        p["status"] = "queued"; p["due_ms"] = now_ms(); p["reason"] = "recovered unfinished impulse after restart";
        n["active"] = "";
        json data = {{"mesh", {{"pulses", json::array({p})}, {"nodes", json::array({n})}}}};
        auto id = p.value("instance_id", std::string{});
        if (store_.instances.count(id)) {
            auto x = store_.instances.at(id); x["status"] = "cancelled";
            x["generation"] = x.value("generation", 0) + 1;
            x["error"] = "interrupted by restart; durable impulse requeued";
            data["maintenance"] = {{"instances", json::array({x})}};
        }
        event("mesh.recovered", p.at("root_id"), "startup", data, emit);
    }
}
void Engine::neural_touch(const std::string &root, const std::string &memory_id, const std::string &text,
                          uint64_t sequence, Emit emit) {
    auto index = store_.mesh_nodes_by_root.find(root);
    if (index == store_.mesh_nodes_by_root.end()) return;
    std::vector<std::pair<double, std::string>> eligible;
    for (const auto &key : index->second) {
        const auto &n = store_.mesh_neurons.at(key);
        if (n.at("enabled") != true) continue;
        const double score = mesh::relevance(text, n.at("purpose"));
        if (score > 0 || n.at("soul_id") == root) eligible.emplace_back(score, key);
    }
    std::sort(eligible.rbegin(), eligible.rend());
    json pulses = json::array();
    size_t selected = 0;
    for (const auto &[score, key] : eligible) {
        if (selected >= store_.mesh_settings.at("max_rna_per_impulse").get<size_t>() ||
            store_.mesh_due.size() + pulses.size() >= store_.mesh_settings.at("max_pending").get<size_t>() ||
            store_.mesh_pulses.size() + pulses.size() >= 16384) break;
        auto p = neural_pulse(store_.mesh_neurons.at(key), text, "memory:" + memory_id, sequence);
        if (store_.mesh_pulses.count(p.at("key"))) continue;
        const auto importance = store_.engrams.count(memory_id) ?
            store_.engrams.at(memory_id).value("attunement", json::object()).value("importance", 0.) : 0.;
        const auto flesh = upkeep::Flesh::read(store_.flesh.count(root) ? store_.flesh.at(root) : json());
        p["strength"] = std::min(1., std::max(0.25, score) + .25 * importance * flesh.priority());
        p["astrocyte_proposal"] = {{"source_engram", memory_id}, {"importance", importance},
                                  {"flesh_priority", flesh.priority()}, {"grants_execution", false}};
        if (store_.engrams.count(memory_id) && store_.engrams.at(memory_id).contains("attribution"))
            p["kind"] = "reflection";
        pulses.push_back(p); ++selected;
    }
    if (!pulses.empty() || selected < eligible.size())
        event("mesh.evidence_queued", root, "memory-touch",
              {{"source_engram", memory_id}, {"eligible", eligible.size()}, {"selected", selected},
               {"held", eligible.size() - selected},
               {"reason", selected < eligible.size() ? "fan-out bounded; source retained for explicit signal or later evidence" : "attention eligibility scored in one batch"},
               {"mesh", {{"pulses", pulses}}}}, emit);
}
json Engine::neural_snapshot(const std::string &root, bool summary) const {
    json nodes = json::array(), routes = json::array(), pulses = json::array(), web = json::array();
    if (!summary) for (const auto &[key, row] : store_.mesh_neurons) if (root == "*" || row.at("root_id") == root) nodes.push_back(row);
    if (!summary) for (const auto &[key, row] : store_.mesh_routes) if (root == "*" || row.at("root_id") == root) routes.push_back(row);
    std::vector<json> recent;
    if (!summary) for (const auto &[key, row] : store_.mesh_pulses) if (root == "*" || row.at("root_id") == root) recent.push_back(row);
    std::sort(recent.begin(), recent.end(), [](const json &a, const json &b) {
        return a.at("source_sequence").get<uint64_t>() > b.at("source_sequence").get<uint64_t>();
    });
    for (const auto &row : recent) { if (pulses.size() == 64) break; auto p = row; p.erase("text"); pulses.push_back(p); }
    for (const auto &[key, row] : store_.mesh_web) if (root == "*" || row.at("root_id") == root)
        if (row.at("status") == "pending" || row.at("status") == "claimed") web.push_back(row);
    json result = {{"nodes", nodes}, {"routes", routes}, {"pulses", pulses}, {"pulse_total", recent.size()},
                  {"pending_web", web}, {"settings", store_.mesh_settings}, {"usage", store_.mesh_usage},
                  {"pending", store_.mesh_due.size()}, {"running", store_.mesh_running.size()},
                  {"representations_available", foundation_.supports_representations()},
                  {"knowledge_model", foundation_.name()}, {"actions", neural::actions()},
                  {"feature_schema", neural::feature_schema}, {"summary", summary}};
    require(result.dump().size() <= 200000, "mesh snapshot too large; inspect one root");
    return result;
}
bool Engine::neural_command(const json &m, Emit emit) {
    const auto op = m.at("type").get<std::string>();
    if (op.rfind("mesh.", 0) != 0) return false;
    const auto trace = m.at("request_id").get<std::string>(), root = m.value("root_id", std::string("*"));
    if (op == "mesh.inspect") {
        require(root == "*" || store_.roots.count(root), "unknown mesh root");
        emit({{"schema", 1}, {"type", "mesh.snapshot"}, {"root_id", root}, {"trace_id", trace}, {"data", neural_snapshot(root, m.value("summary", false))}});
        return true;
    }
    if (op == "mesh.training.export") {
        require(store_.roots.count(root), "choose one root for training export");
        json samples = json::array();
        for (const auto &[key, row] : store_.mesh_samples) if (row.at("root_id") == root) samples.push_back(row);
        require(samples.dump().size() <= 180000, "training export exceeds response bound; archive samples first");
        emit({{"schema", 1}, {"type", "mesh.training.rows"}, {"root_id", root}, {"trace_id", trace},
              {"data", {{"feature_schema", neural::feature_schema}, {"samples", samples}}}}); return true;
    }
    const auto request_key = root + "/" + trace;
    auto intent = m; intent.erase("request_id"); const auto digest = checksum(intent.dump());
    if (store_.mesh_requests.count(request_key)) {
        const auto &old = store_.mesh_requests.at(request_key);
        require(old.at("data").at("mesh").at("intent") == digest, "mesh request_id conflict");
        emit(old); return true;
    }
    require(store_.mesh_requests.size() < 16384, "mesh command ledger full; archive before accepting more");
    auto commit = [&](const std::string &type, json changes, json extra = json::object()) {
        changes["request_key"] = request_key; changes["intent"] = digest; extra["mesh"] = changes;
        return event(type, root, trace, extra, emit);
    };
    if (op == "mesh.configure") {
        require(root == "*", "mesh budget is global");
        auto config = store_.mesh_settings;
        require(m.at("expected_version") == config.at("version"), "mesh settings version conflict");
        const auto &changes = m.at("settings");
        require(changes.is_object() && !changes.contains("version"), "configuration version is coordinator owned");
        config.update(changes); config["version"] = config.at("version").get<int>() + 1;
        neural::validate_settings(config);
        commit("mesh.configured", {{"settings", config}}, {{"settings", config}}); return true;
    }
    require(store_.roots.count(root), "an existing root_id is required");
    if (op == "mesh.node.register") {
        auto id = field(m, "node_id", 80); identifier(id);
        auto soul = m.value("soul_id", root); identifier(soul);
        require(relationships::snapshot(store_, root).find(soul), "node soul outside root");
        auto n = neural_node(root, id, soul);
        require(n.at("version") == m.at("expected_version"), "node version conflict");
        require(n.at("active") == "", "node changes require an impulse boundary");
        require(store_.mesh_neurons.count(n.at("key")) || store_.mesh_neurons.size() < 4096, "node capacity reached");
        n["soul_id"] = soul;
        if (m.contains("purpose")) n["purpose"] = field(m, "purpose", 1024);
        for (auto name : {"enabled", "knowledge", "web"}) if (m.contains(name)) {
            require(m.at(name).is_boolean(), "node flag must be boolean"); n[name] = m.at(name);
        }
        for (auto name : {"minimum_strength", "minimum_margin"}) if (m.contains(name)) { threshold(m, name); n[name] = m.at(name); }
        if (m.contains("examples")) {
            neural::validate_examples(m.at("examples"));
            std::set<std::string> labels;
            for (const auto &ex : m.at("examples")) labels.insert(ex.at("action"));
            require(labels.empty() || labels.size() >= 2, "semantic demonstrations need two distinct labels");
            n["examples"] = m.at("examples");
        }
        if (m.contains("package_key")) {
            auto package = m.at("package_key").get<std::string>();
            require(package.empty() || (packages_.count(package) && packages_.at(package)->manifest.value("feature_schema", std::string{}) == neural::feature_schema), "choose a neural Wind package");
            n["package_key"] = package;
        }
        n["version"] = n.at("version").get<int>() + 1;
        commit("mesh.node_registered", mesh_delta("nodes", n)); return true;
    }
    if (op == "mesh.route.connect") {
        auto from = root + "/" + field(m, "from", 80), to = root + "/" + field(m, "to", 80);
        require(from != to && store_.mesh_neurons.count(from) && store_.mesh_neurons.count(to), "route needs two distinct nodes in this root");
        auto key = from + ">" + to;
        auto old = store_.mesh_routes.find(key);
        require(m.at("expected_version") == (old == store_.mesh_routes.end() ? 0 : old->second.at("version").get<int>()), "route version conflict");
        require(old != store_.mesh_routes.end() || store_.mesh_routes.size() < 8192, "route capacity reached");
        require(old != store_.mesh_routes.end() || store_.mesh_edges_by_node[from].size() < 32, "at most 32 outgoing routes per neuron");
        json route = {{"key", key}, {"root_id", root}, {"from", from}, {"to", to},
                      {"version", old == store_.mesh_routes.end() ? 1 : old->second.at("version").get<int>() + 1},
                      {"enabled", m.value("enabled", true)}, {"weight", m.value("weight", 0.8)}};
        require(route.at("enabled").is_boolean(), "route enabled must be boolean"); threshold(route, "weight");
        commit("mesh.route_connected", mesh_delta("routes", route)); return true;
    }
    if (op == "mesh.signal" || op == "mesh.review") {
        const auto node_key = root + "/" + m.value("node_id", root);
        require(store_.mesh_neurons.count(node_key), "register the target neuron first");
        require(store_.mesh_due.size() < store_.mesh_settings.at("max_pending").get<size_t>() && store_.mesh_pulses.size() < 16384, "mesh inbox full; signal was not accepted");
        auto p = neural_pulse(store_.mesh_neurons.at(node_key), field(m, "text", 4096), "request:" + trace, store_.sequence() + 1, "request");
        p["direct"] = true;
        if(m.contains("lookup")){require(m.at("lookup").is_boolean(),"lookup must be boolean");p["lookup"]=m.at("lookup");}
        if (op == "mesh.review") {
            const auto delay = m.value("due_in_ms", 60000);
            require(delay >= 0 && delay <= 604800000, "review delay must be 0 through 7 days");
            p["direct"] = false; p["kind"] = "review"; p["background"] = true;
            p["due_ms"] = now_ms() + delay; p["reason"] = "waiting for scheduled mesh review";
        }
        if (m.contains("chat_id")) {
            auto chat = field(m, "chat_id", 80); require(store_.chats.count(chat), "unknown source chat");
            p["chat_id"] = chat;
        }
        if (m.contains("wind_instance_id")) {
            auto id = field(m, "wind_instance_id", 80);
            require(store_.instances.count(id), "unknown Wind contribution");
            const auto &x = store_.instances.at(id); auto grants = read_roots(root);
            require(std::find(grants.begin(), grants.end(), x.at("root_id").get<std::string>()) != grants.end(), "Wind contribution is outside read scope");
            require(x.at("status") == "completed" && x.contains("artifact"), "Wind contribution must be a completed episode");
            const auto artifact = x.at("artifact").is_string() ? x.at("artifact").get<std::string>() : x.at("artifact").dump();
            require(!artifact.empty() && artifact.size() <= 2048, "Wind contribution must contain 1 through 2048 bytes; explicitly select a smaller result");
            p["wind_contribution"] = {{"id", "wind:" + id}, {"version", 1}, {"root_id", x.at("root_id")},
                {"text", artifact}, {"source", "Wind episode " + id}, {"claim_status", "hypothetical"}, {"relevance", 1.0},
                {"memory_kind", "wind_contribution"}, {"package_key", x.at("package_key")},
                {"evaluation", x.value("evaluation", json::object())}};
        }
        if (m.contains("package_key")) {
            auto key = field(m, "package_key", 160);
            require(packages_.count(key) && packages_.at(key)->manifest.value("feature_schema", std::string{}) == neural::feature_schema,
                    "signal needs a neural readout package");
            p["package_key"] = key;
        }
        if (m.contains("features")) { p["features"] = m.at("features"); (void)neural::features(p, store_.mesh_neurons.at(node_key), {}); }
        json pulses = json::array();
        // A direct request absorbs identical queued background evidence at the same neuron.
        for (const auto &[due, key] : store_.mesh_due) {
            const auto &old = store_.mesh_pulses.at(key);
            if (p.at("direct") == true && old.at("node_key") == node_key && old.at("text") == p.at("text") &&
                old.at("direct") == false && !p.contains("features")) {
                auto merged = old; merged["status"] = "coalesced";
                merged["reason"] = "identical evidence absorbed by direct impulse " + p.at("key").get<std::string>();
                pulses.push_back(merged);
                if (!p.contains("coalesced_sources")) p["coalesced_sources"] = json::array();
                p["coalesced_sources"].push_back(old.at("key"));
            }
        }
        pulses.push_back(p);
        json changes = {{"pulses", pulses}}, extra = {{"pulse_key", p.at("key")}};
        if (m.value("response", false) || p.contains("chat_id")) {
            require(store_.assemblies.size() < 8192, "response assembly capacity reached; export before continuing");
            auto a = assembly_start(p); changes["assemblies"] = json::array({a}); extra["assembly_key"] = a.at("key");
        }
        commit("mesh.signal_queued", changes, extra); return true;
    }
    if (op == "mesh.pulse.retry" || op == "mesh.pulse.cancel") {
        auto key = field(m, "pulse_key", 80);
        require(store_.mesh_pulses.count(key) && store_.mesh_pulses.at(key).at("root_id") == root, "unknown scoped impulse");
        auto p = store_.mesh_pulses.at(key);
        if (p.at("status") == "running") {
            require(op == "mesh.pulse.cancel", "running impulse cannot retry");
            auto n = store_.mesh_neurons.at(p.at("node_key")); n["active"] = "";
            auto x = store_.instances.at(p.at("instance_id")); x["status"] = "cancelled";
            x["generation"] = x.value("generation", 0) + 1;
            p["status"] = "cancelled"; p["reason"] = "operator cancelled; any in-flight result will be discarded";
            commit("mesh.pulse_changed", {{"pulses", json::array({p})}, {"nodes", json::array({n})}}, {{"maintenance", {{"instances", json::array({x})}}}});
            return true;
        }
        if (p.at("status") == "waiting_web") {
            require(op == "mesh.pulse.cancel", "wait for web completion or lease expiration before retry");
            json web = json::array();
            for (const auto &[id, item] : store_.mesh_web) if (item.at("pulse_key") == key &&
                (item.at("status") == "pending" || item.at("status") == "claimed")) {
                auto cancelled = item; cancelled["status"] = "cancelled"; web.push_back(cancelled);
            }
            p["status"] = "cancelled"; p["reason"] = "web lookup cancelled; late results will be rejected";
            commit("mesh.pulse_changed", {{"pulses", json::array({p})}, {"web", web}}); return true;
        }
        if (op == "mesh.pulse.retry") {
            require(final_pulse(p), "only finished impulses can retry");
            require(store_.mesh_due.size() < store_.mesh_settings.at("max_pending").get<size_t>(), "mesh queue full");
            p["status"] = "queued"; p["due_ms"] = now_ms(); p["attempt"] = 0;
            p.erase("web_attempt"); p.erase("features");
        } else { p["status"] = "cancelled"; }
        p["reason"] = "explicit operator " + op;
        commit("mesh.pulse_changed", mesh_delta("pulses", p)); return true;
    }
    if (op == "mesh.feedback") {
        auto key = field(m, "pulse_key", 80), action = field(m, "action", 32);
        auto names = neural::actions(); auto label = std::find(names.begin(), names.end(), action);
        require(label != names.end(), "unknown feedback action");
        require(store_.mesh_pulses.count(key) && store_.mesh_pulses.at(key).at("root_id") == root, "unknown scoped impulse");
        const auto &p = store_.mesh_pulses.at(key);
        require(p.contains("decision") && final_pulse(p), "feedback needs a finished decision");
        auto sample_key = root + "/" + key;
        require(!store_.mesh_samples.count(sample_key), "an impulse already has a training label");
        require(store_.mesh_samples.size() < 4096, "training sample capacity reached");
        json sample = {{"key", sample_key}, {"root_id", root}, {"pulse_key", key}, {"lineage", p.at("lineage")},
                       {"features", p.at("decision").at("features")}, {"target", label - names.begin()},
                       {"action", action}, {"source_sequence", store_.sequence() + 1},
                       {"feature_schema", neural::feature_schema}, {"node_key", p.at("node_key")},
                       {"actor", "operator"}, {"note", m.value("note", std::string{})}};
        require(sample.at("note").get<std::string>().size() <= 1024, "feedback note too long");
        commit("mesh.feedback_recorded", mesh_delta("samples", sample), {{"active_policy_changed", false}}); return true;
    }
    if (op == "mesh.web.claim" || op == "mesh.web.complete") {
        auto key = field(m, "web_key", 80);
        require(store_.mesh_web.count(key) && store_.mesh_web.at(key).at("root_id") == root, "unknown web request");
        auto item = store_.mesh_web.at(key);
        require(item.at("expires_ms").get<int64_t>() > now_ms(), "web request expired");
        if (op == "mesh.web.claim") {
            require(store_.mesh_settings.at("enabled") == true && store_.mesh_settings.at("web_enabled") == true, "web work is paused");
            require(item.at("status") == "pending", "web request already claimed");
            item["status"] = "claimed"; item["claim_id"] = trace;
            commit("mesh.web_assigned", mesh_delta("web", item), {{"request", item}}); return true;
        }
        require(item.at("status") == "claimed" && item.at("claim_id") == field(m, "claim_id", 80), "web claim mismatch");
        auto p = store_.mesh_pulses.at(item.at("pulse_key"));
        require(p.at("status") == "waiting_web", "impulse no longer awaits web material");
        json memories = json::array();
        if (m.contains("error")) {
            item["status"] = "failed"; item["reason"] = field(m, "error", 1024);
            p["status"] = "held"; p["reason"] = item.at("reason");
        } else {
            const auto &results = m.at("results");
            require(results.is_array() && !results.empty() && results.size() <= 3, "web response needs 1..3 attributed excerpts");
            size_t i = 0;
            for (const auto &r : results) {
                auto url = field(r, "url", 256), text = field(r, "text", 1800), title = field(r, "title", 256);
                require(url.rfind("https://", 0) == 0, "web sources must be HTTPS");
                auto engram = key + "-" + std::to_string(i++);
                json packet = {{"schema", 1}, {"intent", "memory.upsert"}, {"root_id", root},
                               {"engram_id", engram}, {"packet_id", engram}, {"expected_version", 0},
                               {"hops", 0}, {"expires_ms", now_ms() + 60000}, {"text", text},
                               {"source", url}, {"claim_status", "hypothetical"}};
                auto memory = mesh::prepare_memory(store_, packet, trace);
                memory["web_provenance"] = {{"query", item.at("query")}, {"provider", item.at("provider")},
                                            {"title", title}, {"url", url}, {"retrieved_ms", now_ms()},
                                            {"excerpt", true}, {"content_checksum", checksum(text)}};
                memories.push_back(memory);
            }
            item["status"] = "completed"; item["result_count"] = memories.size();
            p["status"] = "queued"; p["due_ms"] = now_ms(); p["reason"] = "new web evidence available; re-evaluate locally";
            p.erase("features");
        }
        auto changes = json{{"web", json::array({item})}, {"pulses", json::array({p})}};
        auto extra = json{{"status", item.at("status")}, {"maintenance", {{"memories", memories}}}};
        commit("mesh.web_received", changes, extra);
        return true;
    }
    throw std::runtime_error("unknown mesh command");
}

void Engine::neural_dispatch(int64_t time, std::vector<std::string> &ready, Emit emit) {
    const auto settings = store_.mesh_settings;
    // Lease expiration is housekeeping, including while activity is paused.
    for (size_t count = 0; count < 32 && !store_.mesh_web_due.empty() && store_.mesh_web_due.begin()->first <= time; ++count) {
        auto web = store_.mesh_web.at(store_.mesh_web_due.begin()->second);
        auto p = store_.mesh_pulses.at(web.at("pulse_key"));
        web["status"] = "failed"; web["reason"] = "web worker lease expired; explicit retry available";
        p["status"] = "held"; p["reason"] = web.at("reason");
        event("mesh.web_expired", p.at("root_id"), "web-expired", {{"mesh", {{"web", json::array({web})}, {"pulses", json::array({p})}}}}, emit);
    }
    if (settings.at("enabled") != true) return;
    for (size_t count = 0; count < 32 && !store_.mesh_due.empty() && store_.mesh_due.begin()->first <= time; ++count) {
        if (store_.mesh_running.size() >= settings.at("max_concurrent").get<size_t>() ||
            store_.active_instances.size() >= 16 || ready.size() >= 8) break;
        auto p = store_.mesh_pulses.at(store_.mesh_due.begin()->second);
        auto n = store_.mesh_neurons.at(p.at("node_key"));
        const auto root = p.at("root_id").get<std::string>();
        auto usage = store_.mesh_usage;
        if (time >= usage.at("window_start_ms").get<int64_t>() + settings.at("window_ms").get<int64_t>())
            usage = {{"window_start_ms", time}, {"impulses", 0}, {"knowledge_calls", 0}, {"web_queries", 0}};
        auto defer = [&](const std::string &reason, int64_t due = 0) {
            p["reason"] = reason;
            if (due) p["due_ms"] = due; else p["status"] = "held";
            event("mesh.deferred", root, p.at("key"), {{"mesh", mesh_delta("pulses", p)}}, emit);
        };
        if (n.at("enabled") != true) { defer("neuron paused; enable and explicitly retry"); continue; }
        if (n.at("active") != "") {
            // Avoid timer spin: completion will move this deadline earlier.
            defer("waiting for this neuron's running impulse", time + settings.at("failure_refractory_ms").get<int64_t>()); continue;
        }
        // A new chat message is explicit attention. Failure cooldown still holds
        // background work, while concurrency and shared budgets apply to both.
        const bool conversation = language_mode_ != "legacy" && p.value("direct",false) && !p.value("chat_id",std::string{}).empty();
        if (!conversation && n.at("refractory_until_ms").get<int64_t>() > time) {
            defer("neuron refractory interval", n.at("refractory_until_ms")); continue;
        }
        if (usage.at("impulses").get<int>() >= settings.at("max_impulses_per_window").get<int>()) {
            defer("shared mesh impulse budget exhausted", usage.at("window_start_ms").get<int64_t>() + settings.at("window_ms").get<int64_t>()); continue;
        }
        if (store_.instances.size() >= 8192) { defer("episode ledger capacity reached; archive before retry"); continue; }
        auto key = p.value("package_key", n.at("package_key").get<std::string>()); if (key.empty()) key = active_key("neural");
        require(packages_.count(key), "node package unavailable"); const auto &package = *packages_.at(key);
        auto id = "mesh-wind-" + std::to_string(store_.sequence() + 1);
        // Reserve worst-case representation evaluations. Cached work is refunded at completion.
        const bool language_work=p.value("direct",false) && ((language_mode_=="legacy" && packages_.at(active_key("language"))->manifest.value("feature_schema",std::string{})==cognition::language_schema) || p.at("text").get<std::string>().starts_with("/cognition "));
        const int need = language_work ? 1 : int(n.at("examples").size()) + 1;
        const bool allow = n.at("knowledge") == true && (language_work || !n.at("examples").empty()) &&
                           foundation_.supports_representations() &&
                           usage.at("knowledge_calls").get<int>() + need <= settings.at("max_knowledge_calls").get<int>();
        if(language_work && !allow && n.at("knowledge")==true && foundation_.supports_representations() &&
           settings.at("max_knowledge_calls").get<int>()>0) {
            defer("shared knowledge budget exhausted; awaiting next window",usage.at("window_start_ms").get<int64_t>()+settings.at("window_ms").get<int64_t>());continue;
        }
        json x = {{"id", id}, {"root_id", root}, {"trace_id", p.at("key")}, {"seed", p.at("text")},
                  {"artifact", ""}, {"feedback", ""}, {"heart_score", 0.0}, {"iteration", 0}, {"max_steps", 1},
                  {"status", "running"}, {"state", State{}}, {"rubric", json::object()},
                  {"evaluator", default_evaluator_}, {"voice_budget", 0}, {"ablations", json::object()},
                  {"package_key", key}, {"domain", "neural"}, {"mode", "neural"}, {"generation", 0},
                  {"allow_upkeep", false}, {"work_class", p.at("direct") == true ? "interactive" : "upkeep"},
                  {"requested_soul", n.at("soul_id")}, {"max_output_tokens", 1},
                  {"package_id", package.manifest.at("id")}, {"policy_version", package.manifest.at("policy_version")},
                  {"policy_checksum", package.digest}, {"foundation", foundation_.name()},
                  {"mesh_node", n}, {"mesh_pulse", p}, {"knowledge_reserved", allow ? need : 0},
                  {"budget_window", usage.at("window_start_ms")}, {"input_token_budget", settings.at("max_input_tokens")}};
        p["status"] = "running"; p["instance_id"] = id; n["active"] = p.at("key");
        usage["impulses"] = usage.at("impulses").get<int>() + 1;
        usage["knowledge_calls"] = usage.at("knowledge_calls").get<int>() + (allow ? need : 0);
        event("mesh.dispatched", root, p.at("key"),
              {{"instance_id", id}, {"pulse_key", p.at("key")}, {"mesh", {{"pulses", json::array({p})}, {"nodes", json::array({n})}, {"usage", usage}}},
               {"maintenance", {{"instances", json::array({x})}}}}, emit);
        ready.push_back(id);
    }
}
Engine::Result Engine::neural_compute(const Work &w) const {
    if (w.instance.at("mesh_pulse").value("direct",false)) {
        try {
            if (cognition::parse(w.request.seed).at("op")!="unknown" || w.language->manifest.value("feature_schema",std::string{})==cognition::language_schema) return cognitive_compute(w);
        } catch(const std::exception &) { return cognitive_compute(w); }
    }
    Result result;
    const auto start = std::chrono::steady_clock::now();
    const auto &x = w.instance, &node = x.at("mesh_node"), &pulse = x.at("mesh_pulse");
    int calls = 0, cache_hits = 0;
    json knowledge = json::object();
    try {
        // Recording a question is provenance, not independent evidence answering that question.
        std::vector<json> memories;
        for (const auto &memory : w.request.memories)
            if (!(memory.value("memory_kind", std::string{}) == "reflection" && memory.at("text") == pulse.at("text")) &&
                memory.value("memory_kind", std::string{}) != "expression_trace") memories.push_back(memory);
        auto decision = neural::decide(neural::features(pulse, node, memories), *w.package, node);
        if(pulse.value("lookup",false) && decision.features[2]<=0){decision.action="request_knowledge";decision.uncertain=true;decision.reason="explicit knowledge need awaits scoped archive or attributed web evidence";}
        const auto intent = expression::intent(pulse.at("text"));
        if (pulse.at("direct") == true && (intent == "greeting" || intent == "preference" || intent == "correction")) {
            decision.action = "rest"; decision.uncertain = false;
            decision.reason = "direct conversational contribution recorded; no external lookup required";
            decision.method = "conversation-input-gate/1";
        }
        for (const auto &m : memories) if (mesh::relevance(pulse.at("text"), m.at("text")) > 0)
            decision.evidence.push_back({{"id", m.at("id")}, {"version", m.at("version")},
                                         {"source", m.at("source")}, {"claim_status", m.at("claim_status")}});
        const bool interpret_new_source = pulse.value("web_attempt", false) && decision.method == "bootstrap-rules/1";
        if ((decision.action == "request_knowledge" || interpret_new_source) && x.at("knowledge_reserved").get<int>() > 0) {
            auto get = [&](const std::string &text) {
                auto r = foundation_.represent(x.at("root_id"), text, x.at("input_token_budget"));
                if (r.cached) ++cache_hits; else ++calls;
                return r;
            };
            std::string input_text = pulse.at("text");
            json source_ids = json::array();
            if (interpret_new_source) for (const auto &memory : memories) {
                if (!memory.contains("web_provenance") || source_ids.size() >= 1) continue;
                input_text += "\nSource excerpt (unverified): " + memory.at("text").get<std::string>();
                source_ids.push_back({{"id", memory.at("id")}, {"version", memory.at("version")}});
            }
            const auto input = get(input_text);
            std::vector<neural::Representation> examples;
            for (const auto &ex : node.at("examples")) examples.push_back(get(ex.at("text")));
            decision = neural::classify(decision, input, examples, node.at("examples"),
                                        node.at("minimum_strength"), node.at("minimum_margin"));
            knowledge = {{"representation_id", input.identity}, {"input_tokens", input.input_tokens},
                         {"dimensions", input.values.size()}, {"generated_tokens", 0}, {"scope", x.at("root_id")},
                         {"source_excerpts", source_ids}};
        } else if (decision.action == "request_knowledge") {
            knowledge["reason"] = !foundation_.supports_representations() ? "Qwen3 representation backend unavailable" :
                                  node.at("examples").empty() ? "semantic decision needs labeled demonstrations" :
                                  node.at("knowledge") != true ? "knowledge disabled for this neuron" : "knowledge budget exhausted";
        }
        // A classifier cannot authorize speech, a source as true, or a durable write.
        result.neural = decision.record();
        result.neural["knowledge"] = knowledge;
        result.neural["payload"] = {{"schema", "engram.payload/1"}, {"numeric", decision.features},
                                    {"evidence", expression::evidence(memories, pulse.at("text"), pulse.at("direct"))},
                                    {"model_binding", knowledge}, {"policy_binding", x.at("package_key")}};
        result.state = decision.features;
        result.proposal = decision.action + ": " + decision.reason;
        if (decision.action == "recall") {
            size_t shown = 0;
            for (const auto &memory : memories) if (mesh::relevance(pulse.at("text"), memory.at("text")) > 0 && shown++ < 2)
                result.proposal += "\n\nSource excerpt [" + memory.at("id").get<std::string>() + ", v" +
                    std::to_string(memory.at("version").get<int>()) + "; " + memory.at("claim_status").get<std::string>() +
                    "]: " + memory.at("text").get<std::string>() + "\n" + memory.at("source").get<std::string>();
        }
        result.evaluation.status = decision.uncertain ? heart::Status::inconclusive : heart::Status::pass;
        result.evaluation.evaluator_id = "wds.ai.heart.neural/1";
        result.evaluation.score = std::clamp(decision.strength, 0.f, 1.f);
        result.evaluation.feedback = decision.reason;
        result.evaluation.scope = x.at("root_id");
        result.evaluation.snapshot_sequence = w.context.heart.sequence();
    } catch (const std::exception &e) { result.error = e.what(); }
    if (!result.neural.is_object()) result.neural = json::object();
    result.neural["knowledge_calls"] = calls; result.neural["cache_hits"] = cache_hits;
    result.elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    return result;
}

bool Engine::neural_finish(const Work &w, Result result, Emit emit) {
    auto x = store_.instances.at(w.instance.at("id"));
    auto p = store_.mesh_pulses.at(w.instance.at("mesh_pulse").at("key"));
    auto n = store_.mesh_neurons.at(p.at("node_key"));
    const auto root = p.at("root_id").get<std::string>();
    auto usage = store_.mesh_usage;
    const int actual = result.neural.value("knowledge_calls", 0), hits = result.neural.value("cache_hits", 0);
    if (usage.at("window_start_ms") == w.instance.at("budget_window") && result.error.empty())
        usage["knowledge_calls"] = std::max(0, usage.at("knowledge_calls").get<int>() - w.instance.at("knowledge_reserved").get<int>() + actual);
    n["active"] = ""; n["impulses"] = n.at("impulses").get<int>() + 1;
    n["knowledge_calls"] = n.at("knowledge_calls").get<int>() + actual;
    n["cache_hits"] = n.at("cache_hits").get<int>() + hits;
    n["refractory_until_ms"] = now_ms() + store_.mesh_settings.at(result.error.empty() ? "refractory_ms" : "failure_refractory_ms").get<int64_t>();
    p["decision"] = result.neural; p["elapsed_ms"] = result.elapsed_ms;
    p["package_key"] = x.at("package_key"); p["model"] = foundation_.name();
    x["speaker"] = w.instance.at("speaker"); x["memory_sources"] = w.instance.at("memory_sources");
    if(!result.cognitive.is_null())x["language_trace"]=result.cognitive.at("trace");
    else if(result.neural.value("generation_calls",0)>0)x["language_trace"]={{"teacher",{{"model",foundation_.name()}}},{"complete",false}};
    x["artifact"] = result.proposal; x["state"] = result.state; x["iteration"] = 1;
    x["feedback"] = result.error.empty() ? result.evaluation.feedback : result.error;
    x["heart_score"] = result.evaluation.score; x["evaluation"] = result.evaluation.record();
    x["neural_decision"] = result.neural;
    json children = json::array(), web = json::array();
    const auto action = result.neural.value("action", std::string("hold")); n["last_action"] = action;
    p["status"] = result.error.empty() ? (result.neural.value("uncertain", false) ? "held" : "completed") : "failed";
    p["reason"] = result.error.empty() ? result.neural.value("reason", std::string{}) : result.error;
    if (!result.error.empty()) x["error"] = result.error;
    if (result.error.empty() && action == "request_knowledge" && !p.value("web_attempt", false) &&
        store_.mesh_settings.at("enabled") == true && store_.mesh_settings.at("web_enabled") == true && n.at("web") == true) {
        if (usage.at("web_queries").get<int>() < store_.mesh_settings.at("max_web_queries").get<int>() && store_.mesh_web.size() < 4096) {
            // Only the bounded signal is sent. Root profiles and private retrieved memories are excluded.
            const auto query = p.at("text").get<std::string>();
            if (query.size() <= 512) {
                auto key = "web-" + checksum(p.at("key").get<std::string>() + "/" + std::to_string(store_.sequence()));
                web.push_back({{"key", key}, {"root_id", root}, {"pulse_key", p.at("key")},
                               {"provider", "wikipedia"}, {"query", query}, {"status", "pending"},
                               {"expires_ms", now_ms() + store_.mesh_settings.at("web_timeout_ms").get<int64_t>()}});
                p["status"] = "waiting_web"; p["web_attempt"] = true; p["reason"] = "awaiting attributed web evidence";
                usage["web_queries"] = usage.at("web_queries").get<int>() + 1;
            } else p["reason"] = "web query exceeds 512 bytes; submit a shorter explicit signal";
        } else p["reason"] = "web budget or ledger exhausted; explicit retry after next budget window";
    }
    size_t suppressed = 0;
    if (result.error.empty() && !result.neural.value("uncertain", false) &&
        (action == "propagate" || action == "recall" || action == "suggest" || action == "review") &&
        store_.mesh_settings.at("enabled") == true) {
        auto routes = store_.mesh_edges_by_node.find(n.at("key"));
        if (routes != store_.mesh_edges_by_node.end()) for (const auto &key : routes->second) {
            const auto &route = store_.mesh_routes.at(key); const auto &target = store_.mesh_neurons.at(route.at("to"));
            if (route.at("enabled") != true || target.at("enabled") != true) continue;
            if (std::find(p["visited"].begin(), p["visited"].end(), target.at("key")) != p["visited"].end()) { ++suppressed; continue; }
            if (p.at("depth").get<int>() >= store_.mesh_settings.at("max_hops").get<int>() ||
                children.size() >= store_.mesh_settings.at("max_rna_per_impulse").get<size_t>() ||
                store_.mesh_due.size() + children.size() >= store_.mesh_settings.at("max_pending").get<size_t>() ||
                store_.mesh_pulses.size() + children.size() >= 16384) { ++suppressed; continue; }
            const auto child_key = "pulse-" + checksum(p.at("lineage").get<std::string>() + "/" + target.at("key").get<std::string>());
            if (store_.mesh_pulses.count(child_key)) { ++suppressed; continue; }
            auto child = p;
            child["key"] = child_key; child["node_key"] = target.at("key"); child["source"] = p.at("key");
            child["source_sequence"] = store_.sequence() + 1; child["direct"] = false;
            child["depth"] = p.at("depth").get<int>() + 1; child["visited"].push_back(target.at("key"));
            child["strength"] = std::clamp(result.neural.at("strength").get<float>() * route.at("weight").get<float>(), 0.f, 1.f);
            child["status"] = "queued"; child["due_ms"] = now_ms(); child["instance_id"] = "";
            child["reason"] = "RNA impulse from connected neuron"; child.erase("decision"); child.erase("features");
            child["attempt"] = 0; children.push_back(child);
        }
    }
    if (result.error.empty() && action == "review" && store_.mesh_settings.at("enabled") == true &&
        p.at("attempt").get<int>() < store_.mesh_settings.at("max_review_attempts").get<int>()) {
        p["status"] = "queued"; p["kind"] = "review"; p["direct"] = false;
        p["attempt"] = p.at("attempt").get<int>() + 1;
        p["due_ms"] = now_ms() + store_.mesh_settings.at("review_delay_ms").get<int64_t>();
        p["reason"] = "bounded diagnostic review scheduled";
    }
    if (action == "review" && p.at("status") == "completed") {
        p["status"] = "held"; p["reason"] = "review attempts exhausted; new evidence or explicit retry required";
    }
    x["status"] = result.error.empty() ? "completed" : "failed";
    children.push_back(p);
    // Wake same-neuron work after completion without a periodic scan of all neurons.
    for (const auto &[due, key] : store_.mesh_due) {
        const auto &waiting = store_.mesh_pulses.at(key);
        if (waiting.at("node_key") == n.at("key") && due > n.at("refractory_until_ms").get<int64_t>()) {
            auto ready = waiting;
            ready["due_ms"] = language_mode_ != "legacy" && waiting.value("direct",false) && !waiting.value("chat_id",std::string{}).empty() ? json(now_ms()) : n.at("refractory_until_ms");
            children.push_back(ready);
        }
    }
    event("mesh.impulse", root, p.at("key"),
          {{"instance_id", x.at("id")}, {"pulse_key", p.at("key")}, {"node_key", n.at("key")},
           {"decision", result.neural}, {"status", p.at("status")}, {"reason", p.at("reason")},
           {"routes_suppressed", suppressed}, {"elapsed_ms", result.elapsed_ms},
           {"mesh", {{"nodes", json::array({n})}, {"pulses", children}, {"usage", usage}, {"web", web}}},
           {"maintenance", {{"instances", json::array({x})}}}}, emit);
    return false;
}
} // namespace wds::ai::runtime
