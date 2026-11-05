#include "wds/ai/core.hpp"
#include <algorithm>
#include <cmath>

namespace wds::ai::upkeep {
int64_t integer(const json &j, const char *key, int64_t lo, int64_t hi) {
    require(j.contains(key) && j.at(key).is_number_integer(), std::string(key) + " must be an integer");
    auto value = j.at(key).get<int64_t>();
    require(value >= lo && value <= hi, std::string(key) + " out of bounds");
    return value;
}
Flesh Flesh::read(const json &j) {
    Flesh f;
    if (j.is_null())
        return f;
    f.hunger = j.at("hunger");
    f.thirst = j.at("thirst");
    f.candle = j.at("candle");
    f.kindling = j.at("kindling");
    f.fatigue = j.at("fatigue");
    for (double v : {f.hunger, f.thirst, f.candle, f.kindling, f.fatigue})
        require(std::isfinite(v) && v >= 0 && v <= 1, "invalid flesh state");
    return f;
}
json Flesh::record() const {
    return {{"hunger", hunger},
            {"thirst", thirst},
            {"candle", candle},
            {"kindling", kindling},
            {"fatigue", fatigue}};
}
void Flesh::need(bool knowledge, double deficit) {
    auto &target = knowledge ? hunger : thirst;
    target = std::max(target, std::clamp(deficit, 0., 1.));
    kindling = std::min(1., kindling + 0.15);
}
void Flesh::outcome(double before, double after, bool passed) {
    if (passed) {
        hunger *= 0.5;
        thirst *= 0.25;
        kindling *= 0.25;
        fatigue *= 0.5;
    } else if (after > before + 1e-6) {
        thirst = std::max(0., thirst - (after - before));
        kindling *= 0.75;
        fatigue *= 0.75;
    } else {
        fatigue = std::min(1., fatigue + 0.25);
        kindling *= 0.5;
    }
}
double Flesh::priority() const {
    return std::clamp(candle + 0.3 * hunger + 0.5 * thirst + 0.2 * kindling - 0.2 * fatigue, 0., 1.);
}
json default_settings() {
    return {{"version", 0},
            {"enabled", false},
            {"max_concurrent_upkeep", 2},
            {"upkeep_refractory_ms", 60000},
            {"max_rna_per_impulse", 4},
            {"flesh_update_only_on_touch", true},
            {"max_episodes", 4},
            {"max_foundation_calls", 2},
            {"max_fetches", 2},
            {"window_ms", 3600000},
            {"review_delay_ms", 60000},
            {"max_attempts", 2},
            {"max_steps", 2},
            {"max_output_tokens", 512},
            {"sources", json::array()}};
}
void validate_settings(const json &j) {
    require(j.is_object(), "upkeep settings must be an object");
    auto defaults = default_settings();
    for (auto it = j.begin(); it != j.end(); ++it)
        require(defaults.contains(it.key()), "unknown upkeep setting: " + it.key());
    require(j.at("enabled").is_boolean() && j.at("flesh_update_only_on_touch") == true,
            "upkeep is event sourced; flesh_update_only_on_touch must be true");
    integer(j, "version", 0, 1000000);
    integer(j, "max_concurrent_upkeep", 1, 4);
    integer(j, "upkeep_refractory_ms", 10, 604800000);
    integer(j, "max_rna_per_impulse", 1, 8);
    integer(j, "max_episodes", 1, 64);
    integer(j, "max_foundation_calls", 0, 64);
    integer(j, "max_fetches", 0, 16);
    integer(j, "window_ms", 1000, 86400000);
    integer(j, "review_delay_ms", 10, 604800000);
    integer(j, "max_attempts", 1, 4);
    integer(j, "max_steps", 1, 8);
    integer(j, "max_output_tokens", 1, 4096);
    require(j.at("sources").is_array() && j.at("sources").size() <= 8, "at most eight configured sources");
    for (const auto &source : j.at("sources")) {
        auto url = field(source, "url", 256);
        require(url.rfind("https://", 0) == 0, "configured sources must be HTTPS");
        field(source, "topic", 128);
    }
}
} // namespace wds::ai::upkeep
namespace wds::ai::understanding {
upkeep::Decision Neuron::decide(const upkeep::GateInput &g) const {
    if (g.duplicate)
        return {"reuse", "same input, evidence and pinned policy; return prior outcome"};
    if (!g.direct && !g.relevant)
        return {"hold", "signal is outside this neuron's declared purpose"};
    if (!g.direct && g.drive < 0.1)
        return {"hold", "no unresolved need above the candle baseline"};
    return {g.local ? "respond_local" : "request_assistance",
            g.direct ? "explicit request" : "relevant changed evidence or due review"};
}
} // namespace wds::ai::understanding
namespace wds::ai::runtime {
namespace {
bool terminal(const json &x) {
    auto s = x.at("status");
    return s != "running" && s != "paused";
}
json delta(std::string name, const json &record) {
    return {{std::move(name), json::array({record})}};
}
void flag(const json &j, const char *key) {
    if (j.contains(key))
        require(j.at(key).is_boolean(), std::string(key) + " must be boolean");
}
std::string domain_of(const json &s) {
    return s.contains("task") ? s.at("task").at("domain").get<std::string>() : "general";
}
} // namespace
json Engine::neuron(const std::string &root, const std::string &soul) const {
    auto scope = relationships::snapshot(store_, root);
    auto view = scope.find(soul);
    require(view, "unknown scoped neuron/soul");
    auto key = root + "/" + soul;
    auto it = store_.neurons.find(key);
    if (it != store_.neurons.end()) {
        auto n = it->second;
        n["purpose"] = view->purpose;
        n["soul_version"] = view->version;
        return n;
    }
    require(store_.neurons.size() < 4096, "neuron capacity reached");
    return {{"key", key},
            {"root_id", root},
            {"soul_id", soul},
            {"purpose", view->purpose},
            {"soul_version", view->version},
            {"active", ""},
            {"last_signature", ""},
            {"last_instance", ""},
            {"refractory_until_ms", 0}};
}
std::string Engine::signal_signature(const json &s) const {
    auto root = s.at("root_id").get<std::string>();
    auto n = neuron(root, s.at("soul_id"));
    json versions = json::object();
    for (const auto &scope : read_roots(root))
        versions[scope] = store_.memory_generations.count(scope) ? store_.memory_generations.at(scope) : 0;
    auto speaker = choose_speaker(root, s.at("text"), s.at("soul_id"));
    auto scope = relationships::snapshot(store_, root);
    auto represented = scope.find(speaker.at("selected_soul"));
    require(represented, "selected speaker missing from scope");
    speaker["identity"] = {{"name", represented->name},
                           {"purpose", represented->purpose},
                           {"relationship", represented->relationship},
                           {"relationship_to", represented->relationship_to}};
    return checksum(json{{"text", s.at("text")},
                         {"task", s.value("task", json())},
                         {"memory_generations", versions},
                         {"speaker", speaker},
                         {"purpose", n.at("purpose")},
                         {"policy", active_key(domain_of(s))},
                         {"foundation", foundation_.name()},
                         {"gate_version", 1},
                         {"max_steps", s.at("max_steps")},
                         {"max_output_tokens", s.value("max_output_tokens", 256)},
                         {"reply_to", s.value("reply_to", std::string{})}}
                        .dump());
}
json Engine::history(const std::string &root, const std::string &query) const {
    json found = json::array();
    auto index = store_.learning_index.find(root);
    if (index == store_.learning_index.end())
        return found;
    size_t examined = 0;
    for (auto it = index->second.rbegin(); it != index->second.rend() && examined++ < 64 && found.size() < 4;
         ++it) {
        const auto &x = store_.instances.at(it->second);
        if (!terminal(x))
            continue;
        if (mesh::relevance(query, x.at("seed")) == 0 && x.at("seed") != query)
            continue;
        found.push_back({{"instance_id", x.at("id")},
                         {"sequence", it->first},
                         {"status", x.at("status")},
                         {"policy_key", x.value("package_key", std::string{})},
                         {"score", x.value("heart_score", 0.)},
                         {"feedback", x.value("feedback", std::string{}).substr(0, 512)},
                         {"artifact", x.value("artifact", std::string{}).substr(0, 512)},
                         {"artifact_excerpt", x.value("artifact", std::string{}).size() > 512}});
    }
    return found;
}
json Engine::upkeep_snapshot(const std::string &root) const {
    json ns = json::array(), rs = json::array(), ms = json::array(), ss = json::array(), fs = json::object();
    // Snapshot is explicit and bounded; no periodic scan runs while idle.
    for (const auto &kv : store_.neurons)
        if (root == "*" || kv.second.at("root_id") == root)
            ns.push_back(kv.second);
    for (const auto &kv : store_.reviews)
        if (root == "*" || kv.second.at("root_id") == root) {
            auto r = kv.second;
            r.erase("task");
            r.erase("text");
            rs.push_back(r);
        }
    for (const auto &kv : store_.materials)
        if (root == "*" || kv.second.at("root_id") == root)
            ms.push_back(kv.second);
    for (const auto &kv : store_.signals)
        if ((root == "*" || kv.second.at("root_id") == root) &&
            (kv.second.at("status") == "queued" || kv.second.at("status") == "running")) {
            auto s = kv.second;
            s.erase("task");
            s.erase("text");
            ss.push_back(s);
        }
    for (const auto &kv : store_.flesh)
        if (root == "*" || kv.first == root)
            fs[kv.first] = kv.second;
    return {{"settings", store_.upkeep_settings},
            {"usage", store_.upkeep_usage},
            {"flesh", fs},
            {"neurons", ns},
            {"reviews", rs},
            {"materials", ms},
            {"pending_signals", ss},
            {"index",
             {{"scheduled_reviews", store_.review_due.size()},
              {"queued_signals", store_.pending_signals.size()}}}};
}
bool Engine::upkeep_command(const json &m, Emit emit) {
    auto op = m.at("type").get<std::string>(), trace = m.at("request_id").get<std::string>();
    if (op.rfind("upkeep.", 0) != 0 && op.rfind("neuron.", 0) != 0 && op.rfind("review.", 0) != 0 &&
        op.rfind("material.", 0) != 0 && op != "learning.recall")
        return false;
    auto root = m.value("root_id", std::string("*"));
    if (op == "upkeep.inspect" || op == "upkeep.watch") {
        if (root != "*")
            require(store_.roots.count(root), "unknown root");
        auto data = upkeep_snapshot(root);
        data["watch"] = op == "upkeep.watch";
        require(data.dump().size() <= 200000, "snapshot exceeds response budget; request a specific root");
        emit({{"schema", 1},
              {"type", "upkeep.snapshot"},
              {"root_id", root},
              {"trace_id", trace},
              {"data", data}});
        return true;
    }
    if (op == "learning.recall") {
        require(store_.roots.count(root), "unknown root");
        auto query = field(m, "text", 4096);
        emit({{"schema", 1},
              {"type", "learning.history"},
              {"root_id", root},
              {"trace_id", trace},
              {"data", history(root, query)}});
        return true;
    }
    auto request_key = root + "/" + trace;
    auto intent = m;
    intent.erase("request_id");
    auto intent_hash = checksum(intent.dump());
    if (auto it = store_.upkeep_requests.find(request_key); it != store_.upkeep_requests.end()) {
        require(it->second.at("data").at("maintenance").at("intent") == intent_hash,
                "upkeep request_id conflict");
        emit(it->second);
        return true;
    }
    require(store_.upkeep_requests.size() < 16384,
            "upkeep request ledger full; archive the store before accepting more");
    auto commit = [&](const std::string &type, json changes, json extra = json::object()) {
        changes["request_key"] = request_key;
        changes["intent"] = intent_hash;
        extra["maintenance"] = changes;
        return event(type, root, trace, extra, emit);
    };
    if (op == "upkeep.configure") {
        require(root == "*", "upkeep budget is global; omit root_id");
        auto settings = store_.upkeep_settings;
        require(m.at("expected_version") == settings.at("version"), "upkeep configuration version conflict");
        const auto &changes = m.at("settings");
        require(changes.is_object() && !changes.contains("version"), "version is coordinator owned");
        for (auto it = changes.begin(); it != changes.end(); ++it)
            settings[it.key()] = it.value();
        settings["version"] = settings.at("version").get<int>() + 1;
        upkeep::validate_settings(settings);
        commit("upkeep.configured", {{"settings", settings}}, {{"settings", settings}});
        return true;
    }
    require(root != "*" && store_.roots.count(root), "an existing root_id is required");
    if (op == "neuron.signal") {
        require(store_.signals.size() < 8192, "signal ledger full; archive before accepting more");
        auto text = field(m, "text", 4096);
        auto soul =
            resolve_soul(root, text, m.value("soul_id", std::string{})).at("soul_id").get<std::string>();
        auto n = neuron(root, soul);
        auto kind = m.value("kind", std::string("request"));
        require(kind == "request" || kind == "evidence", "signal kind must be request or evidence");
        flag(m, "force");
        auto steps = m.value("max_steps", 3);
        require(steps >= 1 && steps <= 8, "max_steps must be 1..8");
        auto tokens = m.value("max_output_tokens", 1024);
        require(tokens >= 1 && tokens <= 4096, "max_output_tokens must be 1..4096");
        auto key = n.at("key").get<std::string>() + "/" + trace;
        json signal = {
            {"key", key},      {"root_id", root},    {"soul_id", soul},    {"text", text},
            {"kind", kind},    {"max_steps", steps}, {"status", "queued"}, {"order", store_.sequence() + 1},
            {"review_key", ""}};
        signal["max_output_tokens"] = tokens;
        if (m.contains("reply_to")) {
            auto prior = field(m, "reply_to", 80);
            require(store_.instances.count(prior) && store_.instances.at(prior).at("root_id") == root &&
                        terminal(store_.instances.at(prior)),
                    "reply_to must be a finished attempt in this root");
            signal["reply_to"] = prior;
        }
        if (m.contains("task")) {
            tasks::validate(m.at("task"));
            signal["task"] = m.at("task");
        }
        auto signature = signal_signature(signal);
        signal["signature"] = signature;
        signal["force"] = m.value("force", false);
        auto cached = n.at("last_instance").get<std::string>();
        bool duplicate = !m.value("force", false) && n.at("last_signature") == signature && !cached.empty() &&
                         store_.instances.count(cached) &&
                         store_.instances.at(cached).at("status") == "completed";
        auto decision = understanding::Neuron().decide(
            {kind == "request", duplicate, mesh::relevance(text, n.at("purpose")) > 0,
             signal.contains("task"), kind == "request" ? 1. : 0.3});
        if (kind == "evidence" && n.at("refractory_until_ms").get<int64_t>() > now_ms() &&
            decision.action != "reuse")
            decision = {"hold", "neuron refractory interval; evidence retained, explicit retry after " +
                                    std::to_string(n.at("refractory_until_ms").get<int64_t>())};
        if (decision.action == "hold" || decision.action == "reuse")
            signal["status"] = decision.action;
        else {
            require(store_.pending_signals.size() < 256,
                    "neuron inbox capacity; request rejected, no work was accepted");
            size_t waiting = 0;
            for (const auto &entry : store_.pending_signals)
                if (store_.signals.at(entry.second).at("root_id") == root &&
                    store_.signals.at(entry.second).at("soul_id") == soul)
                    ++waiting;
            require(waiting < 16, "neuron inbox full; request rejected, retry after completion");
        }
        signal["decision"] = decision.action;
        signal["reason"] = decision.reason;
        json extra = {{"signal_key", key},
                      {"decision", decision.action},
                      {"status", signal.at("status")},
                      {"reason", decision.reason}};
        if (duplicate) {
            signal["instance_id"] = cached;
            extra["instance_id"] = cached;
            extra["artifact"] = store_.instances.at(cached).at("artifact");
        }
        commit("neuron.decision", {{"signals", {signal}}, {"neurons", {n}}}, extra);
        return true;
    }
    if (op == "review.suggest") {
        require(store_.reviews.size() < 4096, "review capacity reached");
        auto soul = m.value("soul_id", root);
        auto n = neuron(root, soul);
        auto text = field(m, "text", 4096);
        flag(m, "requires_material");
        auto due = m.value("due_in_ms", store_.upkeep_settings.at("review_delay_ms").get<int64_t>());
        require(due >= 0 && due <= 604800000, "due_in_ms must be 0..604800000");
        auto key = "review-" + checksum(root + "/" + trace);
        json refs = m.value("engram_ids", json::array());
        require(refs.is_array() && refs.size() <= 8, "at most eight engram references");
        for (const auto &id : refs) {
            require(id.is_string(), "engram reference must be a local ID");
            identifier(id.get<std::string>());
            require(store_.engrams.count(root + "/" + id.get<std::string>()),
                    "engram reference outside root or unknown");
        }
        json review = {{"key", key},
                       {"root_id", root},
                       {"soul_id", soul},
                       {"text", text},
                       {"engram_ids", refs},
                       {"status", "scheduled"},
                       {"due_ms", now_ms() + due},
                       {"attempts", 0},
                       {"reason", m.value("reason", std::string("explicit upkeep suggestion"))},
                       {"requires_material", m.value("requires_material", false)},
                       {"version", 1},
                       {"before_score", 0.0}};
        require(review.at("reason").is_string() && review.at("reason").get<std::string>().size() <= 512,
                "review reason exceeds limit");
        if (m.contains("task")) {
            tasks::validate(m.at("task"));
            review["task"] = m.at("task");
            auto checks = tasks::checks(m.at("task"));
            double score = 0;
            for (auto &c : checks)
                score += c.second;
            review["before_score"] = score / checks.size();
        }
        auto f = upkeep::Flesh::read(store_.flesh.count(root) ? store_.flesh.at(root) : json());
        f.need(review.at("requires_material"), 0.4);
        commit("review.suggested", {{"reviews", {review}}, {"neurons", {n}}, {"flesh", {{root, f.record()}}}},
               {{"review_key", key}, {"due_ms", review.at("due_ms")}});
        return true;
    }
    if (op == "review.cancel" || op == "review.retry") {
        auto key = field(m, "review_key", 80);
        require(store_.reviews.count(key), "unknown review");
        auto r = store_.reviews.at(key);
        require(r.at("root_id") == root, "review outside root");
        require(m.at("expected_version") == r.at("version"), "review version conflict");
        require(r.at("status") != "running", "finish, pause or cancel the associated episode first");
        r["version"] = r.at("version").get<int>() + 1;
        r["status"] = op == "review.cancel" ? "cancelled" : "scheduled";
        r["due_ms"] = now_ms() + store_.upkeep_settings.at("upkeep_refractory_ms").get<int64_t>();
        if (op == "review.retry")
            r["attempts"] = 0;
        commit("review.changed", delta("reviews", r),
               {{"review_key", key}, {"status", r.at("status")}, {"due_ms", r.at("due_ms")}});
        return true;
    }
    if (op == "material.claim" || op == "material.complete") {
        auto key = field(m, "material_key", 80);
        require(store_.materials.count(key), "unknown material request");
        auto item = store_.materials.at(key);
        require(item.at("root_id") == root, "material outside root");
        if (op == "material.claim") {
            require(store_.upkeep_settings.at("enabled") == true, "upkeep paused");
            require(item.at("status") == "pending", "material request already claimed or finished");
            item["status"] = "claimed";
            item["claim_id"] = trace;
            item["expires_ms"] = now_ms() + 120000;
            commit("material.assigned", delta("materials", item), {{"request", item}});
            return true;
        }
        require(item.at("status") == "claimed" && item.at("claim_id") == field(m, "claim_id", 80),
                "material claim mismatch");
        require(item.at("expires_ms").get<int64_t>() > now_ms(),
                "material claim expired; result rejected explicitly");
        json changes;
        auto review = store_.reviews.at(item.at("review_key").get<std::string>());
        require(review.at("status") == "waiting_material", "review no longer awaits this material");
        if (m.contains("error")) {
            item["status"] = "failed";
            item["error"] = field(m, "error", 1024);
            review["status"] = "resting";
            review["reason"] = "material fetch failed: " + item.at("error").get<std::string>();
        } else {
            const auto &chunks = m.at("chunks");
            auto max = store_.upkeep_settings.at("max_rna_per_impulse").get<size_t>();
            require(chunks.is_array() && !chunks.empty() && chunks.size() <= max,
                    "material chunk budget exceeded; no chunks accepted");
            require(store_.engrams.size() + chunks.size() <= 4096 &&
                        store_.packet_ids.size() + chunks.size() <= 100000,
                    "memory capacity exceeded; no chunks accepted");
            changes["memories"] = json::array();
            int n = 0;
            for (const auto &chunk : chunks) {
                require(chunk.is_string(), "material chunk must be text");
                auto id = key + "-" + std::to_string(n++);
                json packet = {{"schema", 1},
                               {"intent", "memory.upsert"},
                               {"hops", 0},
                               {"root_id", root},
                               {"engram_id", id},
                               {"packet_id", id},
                               {"expected_version", 0},
                               {"expires_ms", now_ms() + 60000},
                               {"text", chunk},
                               {"source", item.at("url")},
                               {"claim_status", "hypothetical"}};
                changes["memories"].push_back(mesh::prepare_memory(store_, packet, trace));
                review["engram_ids"].push_back(id);
            }
            item["status"] = "completed";
            item["content_sha256"] = field(m, "content_sha256", 64);
            auto hash = item.at("content_sha256").get<std::string>();
            require(hash.size() == 64 &&
                        hash.find_first_not_of("0123456789abcdefABCDEF") == std::string::npos,
                    "content hash must be 64 hexadecimal characters");
            if (m.contains("resolved_url")) {
                auto url = field(m, "resolved_url", 2048);
                require(url.rfind("https://", 0) == 0, "resolved source must be HTTPS");
                item["resolved_url"] = url;
            }
            item["extraction_limited"] = m.value("extraction_limited", false);
            flag(m, "extraction_limited");
            review["requires_material"] = false;
            review["status"] = "scheduled";
            review["due_ms"] = now_ms();
            review["reason"] = "new source material is hypothetical; review required";
        }
        review["version"] = review.at("version").get<int>() + 1;
        changes["materials"] = {item};
        changes["reviews"] = {review};
        commit(
            "material.received", changes,
            {{"material_key", key}, {"status", item.at("status")}, {"review_status", review.at("status")}});
        return true;
    }
    throw std::runtime_error("unknown upkeep command");
}
void Engine::memory_changed(const std::string &root, const std::string &memory_id, Emit emit) {
    if (store_.upkeep_settings.at("enabled") != true)
        return;
    auto index = store_.reviews_by_memory.find(memory_id);
    if (index == store_.reviews_by_memory.end())
        return;
    json updates = json::array();
    size_t limit = store_.upkeep_settings.at("max_rna_per_impulse");
    for (const auto &key : index->second) {
        const auto &old = store_.reviews.at(key);
        if (old.at("status") == "cancelled" || old.at("status") == "running" ||
            old.at("status") == "waiting_material")
            continue;
        if (updates.size() == limit)
            break;
        auto r = old;
        r["status"] = "scheduled";
        r["due_ms"] = now_ms() + store_.upkeep_settings.at("review_delay_ms").get<int64_t>();
        r["attempts"] = 0;
        r["version"] = r.at("version").get<int>() + 1;
        r["reason"] = "referenced engram changed";
        updates.push_back(r);
    }
    if (!updates.empty()) {
        auto f = upkeep::Flesh::read(store_.flesh.count(root) ? store_.flesh.at(root) : json());
        f.need(true, 0.3);
        event("review.evidence_changed", root, "memory-change",
              {{"engram_id", memory_id},
               {"unexamined_review_count", index->second.size() - updates.size()},
               {"overflow_reason",
                "additional linked reviews retain their prior status; inspect and retry explicitly"},
               {"maintenance", {{"reviews", updates}, {"flesh", {{root, f.record()}}}}}},
              emit);
    }
}
void Engine::suggest_from_failure(const json &x, Emit emit) {
    if (store_.upkeep_settings.at("enabled") != true || !x.value("allow_upkeep", true) ||
        !x.contains("task") || x.at("status") != "exhausted")
        return;
    auto root = x.at("root_id").get<std::string>(), key = "review-" + checksum(x.at("id"));
    if (store_.reviews.count(key))
        return;
    if (store_.reviews.size() >= 4096) {
        event("upkeep.limit", root, x.at("trace_id"),
              {{"reason", "review ledger full; failed attempt remains in learning history"},
               {"instance_id", x.at("id")}},
              emit);
        return;
    }
    auto soul = x.value("neuron_soul_id", root);
    auto n = neuron(root, soul);
    json refs = json::array();
    auto mi = store_.memory_index.find(root);
    if (mi != store_.memory_index.end())
        for (const auto &id : mi->second) {
            if (refs.size() >= 4)
                break;
            const auto &e = store_.engrams.at(id);
            if (mesh::relevance(x.at("seed"), e.at("text")) > 0)
                refs.push_back(e.at("local_id"));
        }
    auto f = upkeep::Flesh::read(store_.flesh.count(root) ? store_.flesh.at(root) : json());
    f.need(false, std::max(0.2, 1. - x.value("heart_score", 0.)));
    json r = {{"key", key},
              {"root_id", root},
              {"soul_id", soul},
              {"text", x.at("seed")},
              {"task", x.at("task")},
              {"engram_ids", refs},
              {"status", "scheduled"},
              {"due_ms", now_ms() + store_.upkeep_settings.at("review_delay_ms").get<int64_t>()},
              {"attempts", 0},
              {"version", 1},
              {"requires_material", false},
              {"reason", "failed practice left an unresolved understanding need"},
              {"source_instance", x.at("id")},
              {"before_score", x.value("heart_score", 0.)}};
    event("review.suggested", root, x.at("trace_id"),
          {{"review_key", key},
           {"maintenance", {{"reviews", {r}}, {"neurons", {n}}, {"flesh", {{root, f.record()}}}}}},
          emit);
}
void Engine::settle_signal(const json &x, Emit emit) {
    auto root = x.at("root_id").get<std::string>();
    json changes;
    if (x.at("status") == "completed" && x.value("work_class", std::string("interactive")) == "interactive" &&
        x.contains("speaker")) {
        auto selected = x.at("speaker").at("selected_soul").get<std::string>();
        auto n = neuron(root, selected);
        double credit = n.value("attention_credit", 0.);
        if (credit > 0) {
            auto key = root + (selected == root ? "/root/" : "/soul/") + selected;
            const auto &atom = store_.soul_atoms.at(key);
            int version =
                store_.soul_weights.count(key) ? store_.soul_weights.at(key).at("version").get<int>() : 0;
            json weight = {
                {"key", key},
                {"weight", std::min(1., atom.weight + credit * (1. - atom.weight))},
                {"version", version + 1},
                {"source_engram", n.at("attention_source")},
                {"response_instance", x.at("id")},
                {"meaning", "response attention priority; no claim of truth or automatic wind activation"}};
            n["attention_credit"] = 0.;
            event("neuron.reinforced", root, x.at("trace_id"),
                  {{"weight", weight}, {"maintenance", {{"weights", {weight}}, {"neurons", {n}}}}}, emit);
        }
    }
    // A speaker proposal is retained if its attempted expression was cancelled or failed.
    if (x.contains("speaker") && !x.at("speaker").at("idea_key").get<std::string>().empty()) {
        auto idea = store_.ideas.at(x.at("speaker").at("idea_key").get<std::string>());
        if (idea.value("instance_id", std::string{}) == x.at("id").get<std::string>()) {
            idea["status"] =
                (x.at("status") == "completed" || x.at("status") == "exhausted") ? "delivered" : "pending";
            idea["version"] = idea.at("version").get<int>() + 1;
            changes["ideas"] = {idea};
        }
    }
    if (!x.contains("neuron_signal_key")) {
        if (!changes.empty())
            event("engram.idea_outcome", root, x.at("trace_id"), {{"maintenance", changes}}, emit);
        return;
    }
    auto key = x.at("neuron_signal_key").get<std::string>();
    if (!store_.signals.count(key))
        return;
    auto signal = store_.signals.at(key);
    if (signal.at("status") != "running")
        return;
    signal["status"] = x.at("status");
    signal["reason"] = x.value("error", x.value("feedback", std::string{}));
    auto n = neuron(root, signal.at("soul_id"));
    n["active"] = "";
    if (x.at("status") == "completed") {
        n["last_signature"] = signal.at("signature");
        n["last_instance"] = x.at("id");
    }
    if (x.at("status") != "completed")
        n["refractory_until_ms"] =
            now_ms() + store_.upkeep_settings.at("upkeep_refractory_ms").get<int64_t>();
    changes["signals"] = {signal};
    changes["neurons"] = {n};
    auto review_key = signal.value("review_key", std::string{});
    if (!review_key.empty()) {
        auto r = store_.reviews.at(review_key);
        double before = r.at("before_score"), after = x.value("heart_score", 0.);
        bool passed = x.at("status") == "completed";
        auto f = upkeep::Flesh::read(store_.flesh.count(root) ? store_.flesh.at(root) : json());
        f.outcome(before, after, passed);
        changes["flesh"] = {{root, f.record()}};
        r["version"] = r.at("version").get<int>() + 1;
        r["last_instance"] = x.at("id");
        r["reason"] = signal.at("reason");
        r["before_score"] = after;
        if (x.contains("task"))
            r["task"] = x.at("task");
        if (passed)
            r["status"] = "satisfied";
        else if (x.at("status") == "cancelled")
            r["status"] = "cancelled";
        else if (x.at("status") == "exhausted" &&
                 r.at("attempts").get<int>() < store_.upkeep_settings.at("max_attempts").get<int>()) {
            r["status"] = "scheduled";
            r["due_ms"] = now_ms() + store_.upkeep_settings.at("upkeep_refractory_ms").get<int64_t>() *
                                         r.at("attempts").get<int>();
        } else {
            r["status"] = "resting";
            r["reason"] = "review stopped: " + signal.at("reason").get<std::string>();
        }
        changes["reviews"] = {r};
    }
    auto index = store_.reviews_by_neuron.find(n.at("key"));
    if (index != store_.reviews_by_neuron.end())
        for (const auto &review_id : index->second) {
            auto r = store_.reviews.at(review_id);
            if (r.at("status") != "waiting_neuron")
                continue;
            r["status"] = "scheduled";
            r["due_ms"] = std::max(now_ms(), n.at("refractory_until_ms").get<int64_t>());
            r["version"] = r.at("version").get<int>() + 1;
            if (!changes.contains("reviews"))
                changes["reviews"] = json::array();
            changes["reviews"].push_back(r);
        }
    event("neuron.completed", root, x.at("trace_id"),
          {{"signal_key", key},
           {"instance_id", x.at("id")},
           {"status", x.at("status")},
           {"artifact", x.at("artifact")},
           {"evaluation", x.value("evaluation", json())},
           {"reason", signal.at("reason")},
           {"maintenance", changes}},
          emit);
}
std::optional<int64_t> Engine::next_wake(int64_t time_ms) const {
    if (store_.upkeep_settings.at("enabled") != true)
        return {};
    std::optional<int64_t> earliest;
    if (!store_.review_due.empty())
        earliest = store_.review_due.begin()->first;
    if (!store_.material_due.empty() && (!earliest || store_.material_due.begin()->first < *earliest))
        earliest = store_.material_due.begin()->first;
    (void)time_ms;
    return earliest;
}
std::vector<std::string> Engine::dispatch_ready(int64_t time, Emit emit) {
    std::vector<std::string> ready;
    const auto settings = store_.upkeep_settings;
    auto usage = store_.upkeep_usage;
    if (time >= usage.at("window_start_ms").get<int64_t>() + settings.at("window_ms").get<int64_t>())
        usage = {{"window_start_ms", time}, {"episodes", 0}, {"foundation_calls", 0}, {"fetches", 0}};
    size_t active = store_.active_instances.size(), background = store_.running_reviews.size();
    if (settings.at("enabled") == true) {
        while (!store_.material_due.empty() && store_.material_due.begin()->first <= time) {
            auto item = store_.materials.at(store_.material_due.begin()->second);
            item["status"] = "failed";
            item["error"] = "fetch lease expired; no automatic retry";
            auto r = store_.reviews.at(item.at("review_key").get<std::string>());
            if (r.at("status") == "waiting_material") {
                r["status"] = "resting";
                r["reason"] = item.at("error");
                r["version"] = r.at("version").get<int>() + 1;
            }
            event("material.expired", item.at("root_id"), "fetch-timeout",
                  {{"material_key", item.at("key")},
                   {"reason", item.at("error")},
                   {"maintenance", {{"materials", {item}}, {"reviews", {r}}}}},
                  emit);
        }
        // Each event handles a bounded number of due records; remaining due work gets a one-shot wake.
        for (size_t count = 0;
             count < 32 && !store_.review_due.empty() && store_.review_due.begin()->first <= time; ++count) {
            auto r = store_.reviews.at(store_.review_due.begin()->second);
            auto root = r.at("root_id").get<std::string>();
            auto n = neuron(root, r.at("soul_id"));
            auto commit = [&](std::string type, json extra = json::object(), json changes = json::object()) {
                r["version"] = r.at("version").get<int>() + 1;
                changes["reviews"] = {r};
                extra["review_key"] = r.at("key");
                extra["status"] = r.at("status");
                extra["reason"] = r.at("reason");
                extra["maintenance"] = changes;
                event(type, root, "upkeep", extra, emit);
            };
            if (!n.at("active").get<std::string>().empty() ||
                background >= settings.at("max_concurrent_upkeep").get<size_t>() || active >= 16) {
                r["status"] = "waiting_neuron";
                r["reason"] = "waiting for a shared execution slot; completion event will wake this review";
                commit("review.deferred");
                continue;
            }
            if (n.at("refractory_until_ms").get<int64_t>() > time) {
                r["due_ms"] = n.at("refractory_until_ms");
                r["reason"] = "neuron refractory interval";
                commit("review.deferred", {{"wake_ms", r.at("due_ms")}});
                continue;
            }
            if (r.at("requires_material") == true) {
                json selected;
                for (const auto &source : settings.at("sources"))
                    if (mesh::relevance(r.at("text"), source.at("topic")) > 0) {
                        selected = source;
                        break;
                    }
                if (selected.is_null()) {
                    r["status"] = "awaiting_source";
                    r["reason"] =
                        "no relevant configured source; add a source or supply local memory, then retry";
                    commit("review.deferred");
                    continue;
                }
                if (usage.at("fetches").get<int>() >= settings.at("max_fetches").get<int>()) {
                    r["due_ms"] =
                        usage.at("window_start_ms").get<int64_t>() + settings.at("window_ms").get<int64_t>();
                    r["reason"] = "shared material-fetch budget exhausted";
                    if (settings.at("max_fetches") == 0) {
                        r["status"] = "awaiting_budget";
                        r["reason"] = "fetch budget is zero; configure budget and retry";
                    }
                    commit("review.deferred", {{"wake_ms", r.at("due_ms")}});
                    continue;
                }
                if (store_.materials.size() >= 2048) {
                    r["status"] = "resting";
                    r["reason"] = "material ledger capacity reached";
                    commit("upkeep.limit");
                    continue;
                }
                auto key = "material-" + checksum(r.at("key").get<std::string>() + "/" +
                                                  std::to_string(r.at("version").get<int>()));
                json item = {{"key", key},
                             {"root_id", root},
                             {"review_key", r.at("key")},
                             {"url", selected.at("url")},
                             {"status", "pending"},
                             {"max_chunks", settings.at("max_rna_per_impulse")},
                             {"topic", r.at("text")},
                             {"reason", "review requires additional source material"}};
                usage["fetches"] = usage.at("fetches").get<int>() + 1;
                r["status"] = "waiting_material";
                r["reason"] = "waiting for configured-source worker";
                commit("material.requested", {{"request", item}}, {{"materials", {item}}, {"usage", usage}});
                continue;
            }
            int steps = settings.at("max_steps"), calls = r.contains("task") ? 0 : steps;
            if (usage.at("episodes").get<int>() >= settings.at("max_episodes").get<int>() ||
                usage.at("foundation_calls").get<int>() + calls >
                    settings.at("max_foundation_calls").get<int>()) {
                if (calls > settings.at("max_foundation_calls").get<int>()) {
                    r["status"] = "awaiting_budget";
                    r["reason"] = "configured foundation-call budget cannot fit one review; change "
                                  "configuration and retry";
                } else {
                    r["due_ms"] =
                        usage.at("window_start_ms").get<int64_t>() + settings.at("window_ms").get<int64_t>();
                    r["reason"] = "shared upkeep budget exhausted";
                }
                commit("review.deferred", {{"wake_ms", r.at("due_ms")}});
                continue;
            }
            auto f = upkeep::Flesh::read(store_.flesh.count(root) ? store_.flesh.at(root) : json());
            auto decision =
                understanding::Neuron().decide({false, false, true, r.contains("task"), f.priority()});
            if (decision.action == "hold") {
                r["status"] = "resting";
                r["reason"] = decision.reason;
                commit("review.deferred");
                continue;
            }
            if (store_.signals.size() >= 8192 || store_.pending_signals.size() >= 256) {
                r["status"] = "resting";
                r["reason"] = "signal capacity exhausted; explicit retry required after archival";
                commit("upkeep.limit");
                continue;
            }
            auto key = "upkeep-signal-" + checksum(r.at("key").get<std::string>() + "/" +
                                                   std::to_string(r.at("version").get<int>()));
            json signal = {{"key", key},
                           {"root_id", root},
                           {"soul_id", r.at("soul_id")},
                           {"text", r.at("text")},
                           {"kind", "upkeep"},
                           {"max_steps", steps},
                           {"max_output_tokens", settings.at("max_output_tokens")},
                           {"status", "queued"},
                           {"order", store_.sequence() + 1},
                           {"review_key", r.at("key")},
                           {"decision", decision.action},
                           {"reason", decision.reason}};
            if (r.contains("task"))
                signal["task"] = r.at("task");
            signal["signature"] = signal_signature(signal);
            r["attempts"] = r.at("attempts").get<int>() + 1;
            r["status"] = "running";
            r["signal_key"] = key;
            r["reason"] = "shared budget reserved; neuron work queued";
            usage["episodes"] = usage.at("episodes").get<int>() + 1;
            usage["foundation_calls"] = usage.at("foundation_calls").get<int>() + calls;
            commit("review.started",
                   {{"signal_key", key},
                    {"decision", decision.action},
                    {"learning_history", history(root, r.at("text"))}},
                   {{"signals", {signal}}, {"neurons", {n}}, {"usage", usage}});
            ++background;
            ++active;
        }
    }
    std::vector<std::string> pending;
    for (const auto &entry : store_.pending_signals)
        pending.push_back(entry.second);
    for (const auto &key : pending) {
        auto signal = store_.signals.at(key);
        auto root = signal.at("root_id").get<std::string>();
        auto n = neuron(root, signal.at("soul_id"));
        if (!n.at("active").get<std::string>().empty())
            continue;
        if (signal.at("kind") == "upkeep" && settings.at("enabled") != true)
            continue;
        if (ready.size() >= 8 || in_flight_.size() + ready.size() >= 16)
            break;
        auto fresh = signal_signature(signal);
        auto cached = n.at("last_instance").get<std::string>();
        if (signal.at("kind") != "upkeep" && !signal.value("force", false) &&
            n.at("last_signature") == fresh && !cached.empty() &&
            store_.instances.at(cached).at("status") == "completed") {
            signal["status"] = "reuse";
            signal["instance_id"] = cached;
            signal["reason"] = "queued duplicate reused after prior completion";
            event("neuron.completed", root, key,
                  {{"signal_key", key},
                   {"instance_id", cached},
                   {"status", "reuse"},
                   {"artifact", store_.instances.at(cached).at("artifact")},
                   {"maintenance", {{"signals", {signal}}}}},
                  emit);
            continue;
        }
        signal["signature"] = fresh;
        auto request = "signal-" + checksum(key);
        std::string id;
        if (store_.episode_by_trace.count(request))
            id = store_.episode_by_trace.at(request);
        if (id.empty()) {
            try {
                json message = {{"schema", 1},
                                {"type", signal.contains("task") ? "task.run" : "seed"},
                                {"request_id", request},
                                {"root_id", root},
                                {"soul_id", signal.at("soul_id")},
                                {"text", signal.at("text")},
                                {"max_steps", signal.at("max_steps")},
                                {"max_output_tokens", signal.value("max_output_tokens", 256)},
                                {"work_class", signal.at("kind") == "upkeep" ? "upkeep" : "interactive"},
                                {"allow_upkeep", signal.at("kind") != "upkeep"}};
                if (signal.contains("task"))
                    message["task"] = signal.at("task");
                id = command(message, emit);
            } catch (const std::exception &error) {
                signal["status"] = "failed";
                signal["reason"] = error.what();
                json changes = {{"signals", {signal}}};
                if (!signal.at("review_key").get<std::string>().empty()) {
                    auto r = store_.reviews.at(signal.at("review_key"));
                    r["status"] = "resting";
                    r["reason"] = error.what();
                    r["version"] = r.at("version").get<int>() + 1;
                    changes["reviews"] = {r};
                }
                event("neuron.failed", root, request,
                      {{"signal_key", key}, {"reason", error.what()}, {"maintenance", changes}}, emit);
                continue;
            }
        }
        auto x = store_.instances.at(id);
        if (signal.contains("reply_to"))
            x["reply_to"] = signal.at("reply_to");
        x["neuron_signal_key"] = key;
        x["neuron_soul_id"] = signal.at("soul_id");
        if (!signal.at("review_key").get<std::string>().empty()) {
            x["review_key"] = signal.at("review_key");
            x["learning_history"] = history(root, signal.at("text"));
        }
        signal["status"] = "running";
        signal["instance_id"] = id;
        n["active"] = id;
        event("neuron.dispatched", root, request,
              {{"signal_key", key},
               {"instance_id", id},
               {"decision", signal.at("decision")},
               {"maintenance", {{"signals", {signal}}, {"neurons", {n}}, {"instances", {x}}}}},
              emit);
        if (terminal(x))
            settle_signal(x, emit);
        else if (x.at("status") == "running")
            ready.push_back(id);
    }
    // Completions release global slots for any waiting neuron, including another root.
    if (background < settings.at("max_concurrent_upkeep").get<size_t>() && active < 16 && ready.empty()) {
        size_t woke = 0;
        auto waiting = store_.waiting_reviews;
        for (const auto &key : waiting)
            if (woke++ < 4) {
                auto r = store_.reviews.at(key);
                auto n = neuron(r.at("root_id"), r.at("soul_id"));
                if (!n.at("active").get<std::string>().empty())
                    continue;
                r["status"] = "scheduled";
                r["due_ms"] = std::max(time, n.at("refractory_until_ms").get<int64_t>());
                r["version"] = r.at("version").get<int>() + 1;
                event("review.ready", r.at("root_id"), "slot-available",
                      {{"review_key", r.at("key")}, {"maintenance", {{"reviews", {r}}}}}, emit);
            }
    }
    return ready;
}
} // namespace wds::ai::runtime
