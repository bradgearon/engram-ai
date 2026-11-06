#include "wds/ai/core.hpp"
#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <sstream>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
namespace wds::ai {
uint64_t hash64(const std::string &s) {
    uint64_t h = 14695981039346656037ull;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ull;
    }
    return h;
}
std::string checksum(const std::string &s) {
    std::ostringstream o;
    o << std::hex << std::setw(16) << std::setfill('0') << hash64(s);
    return o.str();
}
int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}
void require(bool b, const std::string &m) {
    if (!b)
        throw std::runtime_error(m);
}
std::string field(const json &j, const char *k, size_t max) {
    require(j.contains(k) && j[k].is_string(), std::string("missing string: ") + k);
    auto s = j[k].get<std::string>();
    require(!s.empty() && s.size() <= max, std::string("invalid length: ") + k);
    return s;
}
void identifier(const std::string &s) {
    require(!s.empty() && s.size() <= 80, "invalid identifier length");
    for (unsigned char c : s)
        require(std::isalnum(c) || c == '_' || c == '-' || c == '.', "invalid identifier");
}
Embedding embed(const std::string &text) {
    Embedding e{};
    std::string word;
    auto flush = [&] {
        if (!word.empty()) {
            auto h = hash64(word);
            e[h % dim] += 1;
            word.clear();
        }
    };
    for (unsigned char c : text) {
        if (std::isalnum(c))
            word += char(std::tolower(c));
        else
            flush();
    }
    flush();
    float n = 0;
    for (float f : e)
        n += f * f;
    if (n > 0)
        for (float &f : e)
            f /= std::sqrt(n);
    return e;
}
namespace chain {
void Store::apply(const json &e) {
    event_offsets_[e.at("root_id")].push_back({e.at("seq"), journal_bytes_});
    if (e.at("data").contains("workspace")) {
        const auto &w = e.at("data").at("workspace");
        if (w.contains("project")) projects[w.at("project").at("id")] = w.at("project");
        if (w.contains("chat")) chats[w.at("chat").at("id")] = w.at("chat");
        if (w.contains("message")) {
            const auto &m = w.at("message");
            if (!chat_messages.count(m.at("id"))) messages_by_chat[m.at("chat_id")].push_back(m.at("id"));
            chat_messages[m.at("id")] = m;
        }
        if (w.contains("request_key")) workspace_requests[w.at("request_key")] = e;
    }
    auto d = e.at("data");
    auto t = e.at("type");
    auto atom = [&](const std::string &root, const std::string &kind, const std::string &id, float weight) {
        auto key = root + "/" + kind + "/" + id;
        auto &a = soul_atoms[key];
        a.identity = hash64(key);
        a.root = hash64(root);
        a.source_sequence = e.at("seq");
        a.weight = soul_weights.count(key) ? soul_weights.at(key).at("weight").get<float>() : weight;
        a.flags = kind == "soul" || kind == "root" ? 1u : 0u;
    };
    auto memory = [&](const json &row) {
        auto root = row.at("root_id").get<std::string>();
        engrams[row.at("id")] = row;
        packet_ids.insert(row.at("packet_id"));
        ++memory_generations[root];
        memory_index[root].insert(row.at("id"));
        atom(root, "engram", row.at("local_id"), 0.1f);
    };
    auto instance = [&](const json &row) {
        auto id = row.at("id").get<std::string>(), root = row.at("root_id").get<std::string>();
        if (instance_sequences.count(id))
            learning_index[root].erase({instance_sequences.at(id), id});
        instance_sequences[id] = e.at("seq");
        learning_index[root].insert({e.at("seq"), id});
        instances[id] = row;
        episode_by_trace[row.at("trace_id")] = id;
        active_instances.erase(id);
        if (row.at("status") == "running" || row.at("status") == "paused")
            active_instances.insert(id);
        if (!row.value("artifact", std::string{}).empty())
            atom(root, "artifact", id, 0.1f);
    };
    if (d.is_object() && d.contains("maintenance")) {
        const auto &delta = d.at("maintenance");
        for (auto pair : {std::pair{"neurons", &neurons},
                          {"signals", &signals},
                          {"reviews", &reviews},
                          {"materials", &materials},
                          {"scopes", &scopes},
                          {"ideas", &ideas},
                          {"profiles", &profiles},
                          {"preferences", &preferences}})
            if (delta.contains(pair.first))
                for (const auto &row : delta.at(pair.first)) {
                    const auto key = row.at("key").get<std::string>();
                    if (std::string(pair.first) == "signals") {
                        auto old = signals.find(key);
                        if (old != signals.end())
                            pending_signals.erase({old->second.at("order"), key});
                        if (row.at("status") == "queued")
                            pending_signals.insert({row.at("order"), key});
                    }
                    if (std::string(pair.first) == "materials") {
                        auto old = materials.find(key);
                        if (old != materials.end() && old->second.contains("expires_ms"))
                            material_due.erase({old->second.at("expires_ms"), key});
                        if (row.at("status") == "claimed")
                            material_due.insert({row.at("expires_ms"), key});
                    }
                    if (std::string(pair.first) == "reviews") {
                        running_reviews.erase(key);
                        if (row.at("status") == "running")
                            running_reviews.insert(key);
                        waiting_reviews.erase(key);
                        if (row.at("status") == "waiting_neuron")
                            waiting_reviews.insert(key);
                        auto old = reviews.find(key);
                        if (old != reviews.end()) {
                            review_due.erase({old->second.at("due_ms"), key});
                            for (const auto &id : old->second.at("engram_ids"))
                                reviews_by_memory[old->second.at("root_id").get<std::string>() + "/" +
                                                  id.get<std::string>()]
                                    .erase(key);
                        }
                        if (row.at("status") == "scheduled")
                            review_due.insert({row.at("due_ms"), key});
                        auto root = row.at("root_id").get<std::string>();
                        reviews_by_neuron[root + "/" + row.at("soul_id").get<std::string>()].insert(key);
                        for (const auto &id : row.at("engram_ids"))
                            reviews_by_memory[root + "/" + id.get<std::string>()].insert(key);
                    }
                    if (std::string(pair.first) == "ideas")
                        ideas_by_root[row.at("root_id")].insert(key);
                    (*pair.second)[key] = row;
                }
        if (delta.contains("weights"))
            for (const auto &row : delta.at("weights")) {
                soul_weights[row.at("key")] = row;
                soul_atoms.at(row.at("key").get<std::string>()).weight = row.at("weight");
            }
        if (delta.contains("flesh"))
            for (auto it = delta.at("flesh").begin(); it != delta.at("flesh").end(); ++it)
                flesh[it.key()] = it.value();
        if (delta.contains("settings"))
            upkeep_settings = delta.at("settings");
        if (delta.contains("usage"))
            upkeep_usage = delta.at("usage");
        if (delta.contains("instances"))
            for (const auto &row : delta.at("instances"))
                instance(row);
        if (delta.contains("memories"))
            for (const auto &row : delta.at("memories"))
                memory(row);
        if (delta.contains("request_key"))
            upkeep_requests[delta.at("request_key")] = e;
    }
    if (t == "root.created" || t == "root.updated") {
        roots[d.at("id")] = d;
        atom(d.at("id"), "root", d.at("id"), 0.5f);
    }
    if (t == "memory.accepted")
        memory(d);
    if (t == "wind.checkpoint")
        instance(d);
    if (t == "package.registered")
        packages[d.at("key")] = d;
    if (t == "wind.activated") {
        active_packages[d.at("domain")] = d;
        activation_requests[e.at("trace_id")] = e;
    }
    if (t == "seed.accepted")
        request_ids.insert(d.at("request_id"));
    if (t == "relationship.declared") {
        const auto root = e.at("root_id").get<std::string>();
        const auto key = root + "/" + d.at("soul").at("soul_id").get<std::string>();
        souls_[key] = d.at("soul");
        atom(root, "soul", d.at("soul").at("soul_id"), 0.5f);
        voice_interests_[key] = d.at("interest");
        declarations_[root + "/" + d.at("request_id").get<std::string>()] = e;
    }
}
Store::Store(fs::path dir) : directory_(std::move(dir)) {
    fs::create_directories(directory_);
    require(fs::create_directory(directory_ / "writer.lock"),
            "store locked; only one process may write (see recovery guide)");
    try {
        std::ifstream in(directory_ / "events.jsonl", std::ios::binary);
        std::string line;
        while (std::getline(in, line)) {
            require(!in.eof(), "torn event tail; restore or repair before opening");
            require(line.size() <= 256 * 1024, "event too large");
            auto e = json::parse(line);
            auto digest = e.at("hash").get<std::string>();
            e.erase("hash");
            require(e.at("schema") == 1 && e.at("seq") == seq_ + 1 && e.at("prev") == tail_ &&
                        checksum(e.dump()) == digest,
                    "event chain integrity failure");
            e["hash"] = digest; // Preserve the original acknowledgment for idempotent declaration replay.
            apply(e);
            journal_bytes_ += line.size() + 1;
            tail_ = digest;
            seq_++;
        }
        require(souls_.size() <= relationships::max_souls &&
                    declarations_.size() <= relationships::max_declarations,
                "relationship store capacity exceeded");
        require(neurons.size() <= 4096 && signals.size() <= 8192 && reviews.size() <= 4096 &&
                    materials.size() <= 2048 && upkeep_requests.size() <= 16384,
                "upkeep capacity exceeded");
        upkeep::validate_settings(upkeep_settings);
        require(roots.size() <= 1024 && engrams.size() <= 4096 && instances.size() <= 8192,
                "store capacity exceeded");
    } catch (...) {
        fs::remove(directory_ / "writer.lock");
        throw;
    }
}
Store::~Store() {
    std::error_code ec;
    fs::remove(directory_ / "writer.lock", ec);
}
json Store::append(const std::string &type, const std::string &root, const std::string &trace,
                   const json &data) {
    require(!failed_, "store unavailable after write failure");
    json e = {{"schema", 1},  {"seq", seq_ + 1}, {"prev", tail_},     {"time_ms", now_ms()},
              {"type", type}, {"root_id", root}, {"trace_id", trace}, {"actor", "wds.ai.runtime"},
              {"data", data}};
    auto digest = checksum(e.dump());
    e["hash"] = digest;
    auto bytes = e.dump() + "\n";
    require(bytes.size() <= 256 * 1024, "event exceeds byte budget");
    auto f = std::fopen((directory_ / "events.jsonl").string().c_str(), "ab");
    require(f != nullptr, "cannot open event chain");
    bool ok = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
    if (std::fflush(f) != 0)
        ok = false;
#ifdef _WIN32
    if (_commit(_fileno(f)) != 0)
        ok = false;
#else
    if (fsync(fileno(f)) != 0)
        ok = false;
#endif
    if (std::fclose(f) != 0)
        ok = false;
    if (!ok) {
        failed_ = true;
        throw std::runtime_error("durable append failed; restart after recovery");
    }
    tail_ = digest;
    ++seq_;
    apply(e);
    journal_bytes_ += bytes.size();
    return e;
}
} // namespace chain
namespace roots {
json select(chain::Store &s, const std::string &id, const std::string &purpose, const std::string &trace) {
    identifier(id);
    auto it = s.roots.find(id);
    if (it != s.roots.end())
        return it->second;
    require(s.roots.size() < 1024, "root capacity reached");
    json r = {{"id", id},
              {"version", 1},
              {"purpose", purpose},
              {"source", trace},
              {"soul_name", "Lumen of " + id},
              {"relationship_level", 1},
              {"level_basis", "shared completed practice, not intelligence or spiritual status"},
              {"confession", purpose},
              {"heart_commitment", "declared purpose; outcomes evaluated separately"},
              {"shared_episodes", 0},
              {"interests", json::object()}};
    s.append("root.created", id, trace, r);
    return r;
}
} // namespace roots
namespace mesh {
json prepare_memory(const chain::Store &s, const json &p, const std::string &trace) {
    require(p.dump().size() <= 8192, "RNA packet exceeds 8 KiB");
    require(p.at("schema") == 1 && p.at("intent") == "memory.upsert" && p.at("hops") == 0,
            "unsupported packet contract");
    auto root = field(p, "root_id", 80), id = field(p, "engram_id", 80), pid = field(p, "packet_id", 80);
    identifier(root);
    identifier(id);
    identifier(pid);
    require(s.roots.count(root), "unknown root");
    require(!s.packet_ids.count(pid), "duplicate packet");
    require(s.packet_ids.size() < 100000, "packet ledger capacity reached");
    auto expiry = p.at("expires_ms").get<int64_t>();
    require(expiry > now_ms() && expiry <= now_ms() + 300000, "expired or excessive packet TTL");
    auto text = field(p, "text", 2048), source = field(p, "source", 256);
    auto key = root + "/" + id;
    auto old = s.engrams.find(key);
    int version = old == s.engrams.end() ? 0 : old->second.at("version").get<int>();
    require(p.at("expected_version") == version, "memory version conflict");
    require(version < 1000000, "memory version limit");
    require(old != s.engrams.end() || s.engrams.size() < 4096, "engram capacity reached");
    auto claim = field(p, "claim_status", 32);
    require(claim == "observed" || claim == "hypothetical" || claim == "retired", "invalid claim status");
    RnaTile tile;
    tile.key = embed(text);
    tile.features[0] = claim == "observed" ? 0.8f : 0.3f;
    json d = {{"id", key},
              {"local_id", id},
              {"root_id", root},
              {"version", version + 1},
              {"text", text},
              {"source", source},
              {"claim_status", claim},
              {"key", tile.key},
              {"reliability", tile.features[0]},
              {"uncertainty", claim == "hypothetical" ? 0.7 : 0.2},
              {"cost", 0.1},
              {"route_score", 0.0},
              {"packet_id", pid},
              {"trace_id", trace},
              {"tile_schema", 1}};
    return d;
}
json ingest(chain::Store &s, const json &p, const std::string &trace) {
    auto data = prepare_memory(s, p, trace);
    return s.append("memory.accepted", data.at("root_id"), trace, data);
}
std::vector<json> retrieve(const chain::Store &s, const std::string &root, const Embedding &q,
                           bool locations_only) {
    // Bounded store scan for v0.1; keep best 64 before dense gate. Replace with an inverted index later.
    std::vector<std::pair<float, const json *>> ranked;
    auto index = s.memory_index.find(root);
    if (index == s.memory_index.end())
        return {};
    ranked.reserve(index->second.size());
    for (const auto &id : index->second) {
        const auto &e = s.engrams.at(id);
        if (e.at("root_id") != root || e.at("claim_status") == "retired")
            continue;
        auto k = e.at("key").get<Embedding>();
        float dot = 0;
        for (size_t i = 0; i < dim; i++)
            dot += q[i] * k[i];
        ranked.push_back({dot, &e});
    }
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const auto &a, const auto &b) { return a.first > b.first; });
    std::vector<json> out;
    for (size_t i = 0; i < std::min(max_candidates, ranked.size()); ++i) {
        json value = json::object();
        if (locations_only)
            for (auto it = ranked[i].second->begin(); it != ranked[i].second->end(); ++it) {
                if (it.key() != "text" && it.key() != "attribution" && it.key() != "learning_link")
                    value[it.key()] = it.value();
            }
        else
            value = *ranked[i].second;
        out.push_back(std::move(value));
    }
    return out;
}
} // namespace mesh
namespace understanding {
Focus attend(std::vector<json> candidates, const Embedding &q, bool uniform, bool routes) {
    require(candidates.size() <= max_candidates, "attention candidate budget");
    AttentionBatch batch;
    batch.count = candidates.size();
    for (size_t row = 0; row < batch.count; row++) {
        auto key = candidates[row].at("key").get<Embedding>();
        std::copy(key.begin(), key.end(), batch.keys.begin() + row * dim);
        auto &e = candidates[row];
        batch.biases[row] = 0.5f + 0.5f * e.at("reliability").get<float>() -
                            0.5f * e.at("uncertainty").get<float>() - 0.1f * e.at("cost").get<float>() +
                            (routes ? e.at("route_score").get<float>() : 0.f);
    }
    std::array<float, max_candidates> scores{};
    // Dense path: fixed numeric arrays only. Backend replacement needs no JSON/graph traversal.
    for (size_t row = 0; row < batch.count; row++) {
        float dot = 0;
        for (size_t i = 0; i < dim; i++)
            dot += q[i] * batch.keys[row * dim + i];
        scores[row] = 2 * dot + batch.biases[row];
    }
    std::vector<std::pair<float, json>> ranked;
    for (size_t row = 0; row < batch.count; row++) {
        auto &e = candidates[row];
        // Context C=1: retrieval already enforces exact root scope. Coefficients fixed/versioned in v0.1.
        float score = scores[row];
        ranked.push_back({uniform ? 0.f : score, std::move(e)});
    }
    std::stable_sort(ranked.begin(), ranked.end(),
                     [](const auto &a, const auto &b) { return a.first > b.first; });
    Focus f;
    for (size_t i = 0; i < std::min(top_k, ranked.size()); i++) {
        f.scores.push_back(ranked[i].first);
        f.candidates.push_back(std::move(ranked[i].second));
    }
    float total = 0;
    for (float s : f.scores) {
        float a = std::exp((s - f.scores.front()) / 0.7f);
        f.attention.push_back(a);
        total += a;
    }
    for (size_t n = 0; n < f.attention.size(); n++) {
        f.attention[n] /= total;
        auto k = f.candidates[n].at("key").get<Embedding>();
        for (size_t i = 0; i < dim; i++)
            f.pooled[i] += f.attention[n] * k[i];
    }
    return f;
}
Impulse Neuron::receive(const Signal &signal, const winds::Package &package) const {
    identifier(signal.root_id);
    require(signal.engrams.size() <= max_candidates, "neuron signal exceeds attention budget");
    for (const auto &engram : signal.engrams)
        require(engram.at("root_id").get<std::string>() == signal.root_id ||
                    std::find(signal.read_roots.begin(), signal.read_roots.end(),
                              engram.at("root_id").get<std::string>()) != signal.read_roots.end(),
                "neuron signal outside granted scope");
    auto focus = attend(signal.engrams, signal.query, signal.uniform_attention, signal.use_routes);
    auto state = package.step(signal.previous, focus.pooled, signal.feedback, signal.freeze_motion);
    return {std::move(focus), state};
}
} // namespace understanding
namespace runtime {
Engine::Engine(chain::Store &s, winds::Package &p, intelligence::Foundation &f,
               std::unique_ptr<mesh::MeshNode> node, heart::Registry registry, std::string default_evaluator,
               fs::path package_root)
    : store_(s), package_(p), package_root_(fs::weakly_canonical(package_root)), foundation_(f),
      mesh_(node ? std::move(node)
                 : std::make_unique<mesh::SingleNodeMesh>(std::make_unique<mesh::LocalScanIndex>(s))),
      evaluators_(registry.empty() ? heart::defaults(*mesh_) : std::move(registry)),
      default_evaluator_(std::move(default_evaluator)) {
    require(evaluators_.contains(default_evaluator_), "default evaluator not registered");
    initial_key_ = p.manifest.at("id").get<std::string>() + "@" +
                   std::to_string(p.manifest.at("policy_version").get<int>()) + ":" + p.digest;
    packages_[initial_key_] = std::make_shared<const winds::Package>(p);
    for (const auto &entry : store_.packages) {
        const auto path = fs::weakly_canonical(package_root_ / entry.second.at("path").get<std::string>());
        const auto rel = path.lexically_relative(package_root_);
        require(!rel.empty() && *rel.begin() != "..", "registered package escaped package root");
        auto loaded = std::make_shared<const winds::Package>(path);
        require(loaded->digest == entry.second.at("checksum").get<std::string>() &&
                    loaded->manifest == entry.second.at("manifest"),
                "registered package changed on disk");
        packages_[entry.first] = std::move(loaded);
    }
    for (const auto &entry : store_.active_packages)
        require(packages_.count(entry.second.at("key")), "active package unavailable");
}
std::string Engine::active_key(const std::string &domain) const {
    auto found = store_.active_packages.find(domain);
    return found == store_.active_packages.end() ? initial_key_ : found->second.at("key").get<std::string>();
}
std::shared_ptr<const winds::Package> Engine::pinned(const json &x) const {
    auto key = x.value("package_key", initial_key_);
    require(packages_.count(key), "pinned package unavailable");
    auto p = packages_.at(key);
    require(p->digest == x.at("policy_checksum").get<std::string>() &&
                p->manifest.at("policy_version") == x.at("policy_version"),
            "pinned package mismatch");
    return p;
}
json Engine::event(const std::string &t, const std::string &r, const std::string &trace, const json &d,
                   Emit emit) {
    auto e = store_.append(t, r, trace, d);
    emit(e);
    workspace_event(e, emit);
    return e;
}
std::string Engine::command(const json &m, Emit emit) {
    require(m.is_object() && m.dump().size() <= 16384, "invalid command size");
    require(m.at("schema") == 1, "unsupported schema");
    auto op = field(m, "type", 32), trace = field(m, "request_id", 80);
    identifier(trace);
    if (workspace_command(m, emit) || scope_command(m, emit) || upkeep_command(m, emit))
        return {};
    if (op == "package.register") {
        auto relative = field(m, "path", 512);
        fs::path requested(relative);
        require(!requested.is_absolute(), "package path must be relative to package root");
        auto path = fs::weakly_canonical(package_root_ / requested),
             rel = path.lexically_relative(package_root_);
        require(!rel.empty() && *rel.begin() != "..", "package escaped package root");
        auto loaded = std::make_shared<const winds::Package>(path);
        require(loaded->digest == field(m, "checksum", 16), "requested package checksum mismatch");
        auto key = loaded->manifest.at("id").get<std::string>() + "@" +
                   std::to_string(loaded->manifest.at("policy_version").get<int>()) + ":" + loaded->digest;
        if (packages_.count(key)) {
            require(packages_.at(key)->manifest == loaded->manifest,
                    "package identity reused with changed manifest");
            emit({{"schema", 1},
                  {"type", "package.available"},
                  {"trace_id", trace},
                  {"data", {{"key", key}, {"checksum", loaded->digest}}}});
            return {};
        }
        require(packages_.size() < 256, "package registry capacity");
        event("package.registered", "system", trace,
              {{"key", key},
               {"path", rel.generic_string()},
               {"checksum", loaded->digest},
               {"manifest", loaded->manifest}},
              emit);
        packages_[key] = std::move(loaded);
        emit({{"schema", 1},
              {"type", "package.available"},
              {"trace_id", trace},
              {"data", {{"key", key}, {"checksum", packages_.at(key)->digest}}}});
        return {};
    }
    if (op == "package.list") {
        json all = json::array(), active = json::object();
        for (const auto &entry : packages_)
            all.push_back({{"key", entry.first}, {"manifest", entry.second->manifest}});
        for (auto domain : {"general", "music", "architecture"})
            active[domain] = active_key(domain);
        emit({{"schema", 1},
              {"type", "package.snapshot"},
              {"trace_id", trace},
              {"data", {{"packages", all}, {"active", active}}}});
        return {};
    }
    if (op == "wind.activate") {
        auto domain = field(m, "domain", 32), key = field(m, "package_key", 160),
             expected = field(m, "expected_active_key", 160);
        require(domain == "general" || domain == "music" || domain == "architecture",
                "invalid activation domain");
        json intent = {{"domain", domain}, {"key", key}, {"expected_active_key", expected}};
        if (store_.activation_requests.count(trace)) {
            const auto &old = store_.activation_requests.at(trace);
            require(old.at("data").at("intent") == intent, "activation request_id conflict");
            emit(old);
            return {};
        }
        require(packages_.count(key), "unknown registered package");
        require(active_key(domain) == expected, "active package version conflict");
        const auto &manifest = packages_.at(key)->manifest;
        require(!manifest.contains("action_domain") ||
                    manifest.at("action_domain").get<std::string>() == domain,
                "package action domain mismatch");
        for (const auto &entry : store_.instances) {
            auto state = entry.second.at("status");
            if (entry.second.value("domain", std::string("general")) == domain)
                require(state != "running" && state != "paused" && !in_flight_.count(entry.first),
                        "activation requires an episode boundary; finish or cancel active episodes");
        }
        require(store_.activation_requests.size() < 100000, "activation ledger capacity");
        event("wind.activated", "system", trace,
              {{"domain", domain},
               {"key", key},
               {"previous_key", expected},
               {"intent", intent},
               {"reason", "explicit operator command at episode boundary"}},
              emit);
        return {};
    }
    if (op == "episode.attach") {
        auto wanted = field(m, "episode_request_id", 80);
        for (const auto &entry : store_.instances)
            if (entry.second.at("trace_id").get<std::string>() == wanted) {
                emit({{"schema", 1},
                      {"type", "episode.attached"},
                      {"trace_id", trace},
                      {"instance_id", entry.first},
                      {"data", entry.second}});
                return entry.second.at("status") == "running" ? entry.first : "";
            }
        emit({{"schema", 1}, {"type", "episode.missing"}, {"trace_id", trace}});
        return {};
    }
    if (op == "practice.assess") {
        auto root = field(m, "root_id", 80);
        require(store_.roots.count(root), "unknown root");
        auto assessed = tasks::assess(m.at("task"));
        event("practice.assessed", root, trace,
              {{"task_id", m.at("task").at("id")},
               {"task_hash", checksum(m.at("task").dump())},
               {"score", assessed.at("score")},
               {"target_action", assessed.at("target_action")}},
              emit);
        emit({{"schema", 1},
              {"type", "practice.options"},
              {"trace_id", trace},
              {"root_id", root},
              {"data", assessed}});
        return {};
    }
    if (op == "root.select") {
        auto id = field(m, "root_id", 80);
        auto r = roots::select(store_, id, field(m, "purpose", 1024), trace);
        event("root.selected", id, trace, r, emit);
        return {};
    }
    if (op == "relationship.declare" || op == "confess") {
        const auto d = relationships::RelationshipDeclaration::parse(m);
        const auto key = d.root_id + "/" + trace;
        const auto old = store_.declarations().find(key);
        if (old != store_.declarations().end()) {
            require(old->second.at("data").at("declaration") == d.record(),
                    "declaration request_id conflict");
            emit(old->second); // Exact original durable acknowledgment; no second interest increment.
            return {};
        }
        const auto payload = relationships::prepare(store_, d, trace, mesh_->has_voice(d.voice_id));
        event("relationship.declared", d.root_id, trace, payload, emit);
        return {};
    }
    if (op == "heart.inspect") {
        auto root = field(m, "root_id", 80);
        auto view = relationships::snapshot(store_, root);
        auto data = view.record();
        data["mesh"] = mesh::topology(view);
        emit({{"schema", 1},
              {"type", "heart.snapshot"},
              {"root_id", root},
              {"trace_id", trace},
              {"data", data}});
        return {};
    }
    if (op == "memory.upsert") {
        auto accepted = mesh::ingest(store_, m.at("packet"), trace);
        emit(accepted);
        memory_changed(accepted.at("root_id"), accepted.at("data").at("id"), emit);
        return {};
    }
    if (op == "learning.record") {
        auto root = field(m, "root_id", 80);
        require(store_.roots.count(root), "unknown root");
        auto report = m.at("report");
        require(report.is_object() && report.dump().size() < 12000, "report too large");
        event("learning.candidate", root, trace,
              {{"report", report}, {"status", "candidate only; active policy unchanged"}}, emit);
        return {};
    }
    if (op == "seed" || op == "task.run") {
        require(!store_.request_ids.count(trace), "duplicate seed request_id");
        require(store_.request_ids.size() < 100000, "request ledger capacity");
        require(store_.instances.size() < 8192, "instance capacity reached");
        const bool task_run = op == "task.run";
        if (task_run)
            tasks::validate(m.at("task"));
        auto domain = task_run ? m.at("task").at("domain").get<std::string>() : "general";
        auto key = m.value("package_key", active_key(domain));
        require(packages_.count(key), "unknown package for episode");
        auto selected = packages_.at(key);
        require(!selected->manifest.contains("action_domain") ||
                    selected->manifest.at("action_domain").get<std::string>() == domain,
                "package action domain mismatch");
        auto mode = m.value("mode", std::string("wind"));
        require(mode == "wind" || mode == "fixed_refine" || mode == "input_once" || mode == "foundation_once",
                "invalid task mode");
        auto root = field(m, "root_id", 80),
             seed = task_run ? m.value("text", std::string("Solve structured ") + domain + " task")
                             : field(m, "text", 4096);
        require(!seed.empty() && seed.size() <= 4096, "invalid seed length");
        identifier(root);
        if (m.contains("max_steps"))
            require(m.at("max_steps").is_number_integer(), "max_steps must be integer");
        int steps = m.value("max_steps", 3);
        require(steps >= 1 && steps <= 8, "max_steps must be 1..8");
        json rubric = m.value("rubric", json::object());
        require(rubric.is_object() && rubric.dump().size() <= 1024, "invalid rubric");
        auto task_evaluator = "wds.ai.heart." + domain + "/1";
        auto evaluator = m.value("evaluator", task_run ? task_evaluator : default_evaluator_);
        require(!task_run || evaluator == task_evaluator, "task evaluator must match its domain");
        require(evaluators_.contains(evaluator), "unknown evaluator: " + evaluator);
        if (m.contains("voice_budget"))
            require(m.at("voice_budget").is_number_integer() && m.at("voice_budget") >= 0 &&
                        m.at("voice_budget") <= 8,
                    "voice_budget must be 0..8");
        const int voice_budget = m.value("voice_budget", 4);
        if (rubric.contains("must_include"))
            field(rubric, "must_include", 128);
        auto ab = m.value("ablations", json::object());
        require(ab.is_object(), "invalid ablations");
        for (auto k : {"uniform_attention", "no_routes", "freeze_motion", "no_memory"})
            if (ab.contains(k))
                require(ab[k].is_boolean(), "ablation must be boolean");
        if (mode == "input_once" || mode == "foundation_once")
            steps = 1;
        auto relationship = roots::select(store_, root, seed, trace);
        auto requested =
            resolve_soul(root, seed, m.value("soul_id", std::string{})).at("soul_id").get<std::string>();
        auto work_class = m.value("work_class", std::string("interactive"));
        require(work_class == "interactive" || work_class == "workshop" || work_class == "upkeep",
                "unknown work class");
        if (m.contains("max_output_tokens"))
            upkeep::integer(m, "max_output_tokens", 1, 4096);
        if (m.contains("allow_upkeep"))
            require(m.at("allow_upkeep").is_boolean(), "allow_upkeep must be boolean");
        // Explicit goal becomes bounded interest evidence; interest never grants autonomous permissions.
        auto interests = relationship.at("interests");
        std::istringstream words(seed);
        std::string word;
        int count = 0;
        while (words >> word && count < 6) {
            std::string key;
            for (unsigned char c : word)
                if (std::isalnum(c))
                    key += char(std::tolower(c));
            if (key.size() < 4 || key.size() > 32)
                continue;
            if (interests.contains(key) || interests.size() < 16) {
                interests[key] = std::min(100, interests.value(key, 0) + 1);
                ++count;
            }
        }
        relationship["interests"] = interests;
        relationship["version"] = relationship.at("version").get<int>() + 1;
        event("root.updated", root, trace, relationship, emit);
        auto id = "wind-" + std::to_string(store_.sequence() + 1);
        json instance = {{"id", id},
                         {"root_id", root},
                         {"trace_id", trace},
                         {"seed", seed},
                         {"artifact", ""},
                         {"feedback", ""},
                         {"heart_score", 0.0},
                         {"iteration", 0},
                         {"max_steps", steps},
                         {"status", "running"},
                         {"state", State{}},
                         {"rubric", rubric},
                         {"evaluator", evaluator},
                         {"voice_budget", voice_budget},
                         {"ablations", ab},
                         {"package_key", key},
                         {"domain", domain},
                         {"mode", mode},
                         {"generation", 0},
                         {"allow_upkeep", m.value("allow_upkeep", true)},
                         {"work_class", m.value("work_class", std::string("interactive"))},
                         {"requested_soul", requested},
                         {"max_output_tokens", m.value("max_output_tokens", 256)},
                         {"package_id", selected->manifest.at("id")},
                         {"policy_version", selected->manifest.at("policy_version")},
                         {"policy_checksum", selected->digest},
                         {"foundation", foundation_.name()}};
        if (task_run)
            instance["task"] = m.at("task");
        event("seed.accepted", root, trace, {{"request_id", trace}, {"instance_id", id}, {"seed", seed}},
              emit);
        event("wind.checkpoint", root, trace, instance, emit);
        return id;
    }
    if (op == "pause" || op == "resume" || op == "inspect" || op == "cancel") {
        auto id = field(m, "instance_id", 80);
        require(store_.instances.count(id), "unknown instance");
        auto x = store_.instances.at(id);
        auto root = x.at("root_id").get<std::string>();
        if (op == "inspect") {
            emit({{"schema", 1}, {"type", "wind.snapshot"}, {"trace_id", trace}, {"data", x}});
            return {};
        }
        if (op == "resume") {
            require(x.at("status") == "paused", "only paused instances can resume");
            (void)pinned(x);
            require(x.at("foundation").get<std::string>() == foundation_.name(),
                    "pinned foundation mismatch");
            require(evaluators_.contains(x.value("evaluator", std::string("wds.ai.heart.rules/1"))),
                    "pinned evaluator not registered");
            x["status"] = "running";
        } else if (op == "cancel") {
            require(x.at("status") == "running" || x.at("status") == "paused",
                    "only active episodes can cancel");
            x["status"] = "cancelled";
        } else {
            require(x.at("status") == "running", "only running instances can pause");
            x["status"] = "paused";
        }
        x["generation"] = x.value("generation", 0) + 1;
        if (op == "pause")
            x["pause_reason"] = "operator";
        event("wind.checkpoint", root, trace, x, emit);
        if (op == "cancel")
            settle_signal(x, emit);
        return op == "resume" ? id : "";
    }
    throw std::runtime_error("unknown command type");
}
std::optional<Engine::Work> Engine::begin_step(const std::string &id, Emit emit) {
    auto x = store_.instances.at(id);
    if (x.at("status") != "running" || in_flight_.count(id))
        return {};
    if (in_flight_.size() >= 16) {
        x["status"] = "failed";
        x["error"] = "inference queue capacity";
        event("wind.checkpoint", x.at("root_id"), x.at("trace_id"), x, emit);
        settle_signal(x, emit);
        return {};
    }
    auto root = x.at("root_id").get<std::string>(), trace = x.at("trace_id").get<std::string>();
    auto package = pinned(x);
    heart::EvaluationContext context{
        relationships::snapshot(store_, root), x.at("seed"), trace, id, x.at("rubric"), x.at("iteration"),
        x.value("voice_budget", size_t(4)),    nullptr};
    auto ab = x.at("ablations");
    auto q = embed(x.at("seed").get<std::string>() + " " + x.at("feedback").get<std::string>());
    std::vector<json> locations;
    auto grants = read_roots(root);
    if (!ab.value("no_memory", false))
        for (const auto &scope : grants) {
            auto rows = mesh_->retrieve(scope, q);
            size_t limit = grants.size() == 1 ? 64 : (scope == root ? 48 : 16);
            for (size_t i = 0; i < std::min(limit, rows.size()); ++i)
                locations.push_back(std::move(rows[i]));
        }
    understanding::Signal signal{root,
                                 q,
                                 std::move(locations),
                                 x.at("state").get<State>(),
                                 x.at("heart_score").get<float>(),
                                 ab.value("uniform_attention", false),
                                 !ab.value("no_routes", false),
                                 ab.value("freeze_motion", false) || x.value("mode",std::string{})=="foundation_once",
                                 grants};
    auto impulse = understanding::Neuron().receive(signal, *package);
    auto &focus = impulse.focus;
    auto after = impulse.state;
    json refs = json::array();
    for (const auto &c : focus.candidates)
        refs.push_back({{"id", c.at("id")}, {"version", c.at("version")}, {"source", c.at("source")}});
    x["memory_sources"]=json::array();
    for(const auto &ref:refs) {
        const auto &memory=store_.engrams.at(ref.at("id").get<std::string>());
        x["memory_sources"].push_back({{"engram_id",memory.at("id")},{"root_id",memory.at("root_id")},
            {"chat_id",memory.value("attribution",json::object()).value("chat_id",std::string{})}});
    }
    event("attention.selected", root, trace,
          {{"instance_id", id},
           {"query", q},
           {"candidates", refs},
           {"scores", focus.scores},
           {"attention", focus.attention},
           {"scorer_version", 1},
           {"owner", "neuron"}},
          emit);
    std::vector<json> memories;
    for (const auto &scope : grants) {
        std::vector<json> selected;
        for (const auto &ref : focus.candidates)
            if (ref.at("root_id") == scope)
                selected.push_back(ref);
        auto fetched = mesh_->fetch(scope, selected);
        memories.insert(memories.end(), fetched.begin(), fetched.end());
    }
    auto speaker = choose_speaker(root, x.at("seed"), x.value("requested_soul", root), id);
    auto view = relationships::snapshot(store_, root);
    auto soul = view.find(speaker.at("selected_soul"));
    auto relationship = store_.roots.at(root);
    relationship["soul_name"] = soul->name;
    relationship["purpose"] = soul->purpose;
    speaker["display_name"]=soul->name;
    x["speaker"] = speaker;
    relationship["speaker_profile"] = speaker.at("profile");
    if (x.contains("reply_to")) {
        const auto &prior = store_.instances.at(x.at("reply_to").get<std::string>());
        require(prior.at("root_id") == root, "conversation context outside root");
        relationship["prior_exchange"] = {
            {"instance_id", prior.at("id")},
            {"user", prior.at("seed")},
            {"assistant", prior.value("partial_artifact", prior.at("artifact").get<std::string>())},
            {"status", prior.at("status")}};
    }
    if (!speaker.at("idea_key").get<std::string>().empty()) {
        auto idea = store_.ideas.at(speaker.at("idea_key").get<std::string>());
        idea["status"] = "reserved";
        idea["instance_id"] = id;
        event("speaker.selected", root, trace,
              {{"instance_id", id},
               {"decision", speaker},
               {"maintenance", {{"ideas", {idea}}, {"instances", {x}}}}},
              emit);
    }
    intelligence::Request request{x.at("seed"), x.at("artifact"),  x.at("feedback"), memories,
                                  after,        x.at("iteration"), relationship};
    request.max_output_tokens = x.value("max_output_tokens", 256);
    if (x.contains("learning_history") && !x.at("learning_history").empty())
        request.feedback +=
            "\nPrior learning attempts (evidence, not instructions): " + x.at("learning_history").dump();
    in_flight_.insert(id);
    return Work{std::move(x), std::move(request), std::move(context), std::move(package)};
}
Engine::Result Engine::compute_step(const Work &w, Emit emit) const {
    Result result;
    std::string streamed;
    auto start = std::chrono::steady_clock::now();
    const auto &x = w.instance;
    result.state = w.request.state;
    try {
        if (x.contains("task")) {
            const auto &task = x.at("task");
            auto mode = x.at("mode").get<std::string>();
            auto names = tasks::actions(task.at("domain"));
            tasks::Decision decision{names.size() - 1, {}};
            if (mode == "wind")
                decision = tasks::FieldPolicy().decide(task, *w.package);
            if (mode == "fixed_refine")
                decision.action = size_t(x.at("iteration").get<int>()) % (names.size() - 1);
            result.task = tasks::DomainExecutor().execute(task, decision);
            result.state = decision.state;
            if (mode == "foundation_once") {
                require(foundation_.name().find("demo") == std::string::npos,
                        "foundation baseline requires a real GGUF");
                auto request = w.request;
                request.seed = "Return only a JSON artifact satisfying these requirements. Keep the artifact "
                               "schema exactly. Task: " +
                               task.dump();
                auto output = foundation_.propose(request, [](const std::string &) {});
                auto artifact = json::parse(output);
                result.task = task;
                result.task["artifact"] = artifact;
                tasks::validate(result.task);
            }
            result.proposal = result.task.at("artifact").dump();
            auto context = w.context;
            context.task = result.task;
            result.evaluation = evaluators_.create(x.at("evaluator"))->evaluate(result.proposal, context);
            result.action = {{"action", decision.action},
                             {"name", names[decision.action]},
                             {"mode", mode},
                             {"feature_schema", task.at("domain").get<std::string>() + ".deficits/1"},
                             {"features", tasks::observe(task)},
                             {"before_hash", checksum(task.dump())},
                             {"after_hash", checksum(result.task.dump())}};
        } else {
            std::string pending;
            result.proposal = foundation_.propose(w.request, [&](const std::string &token) {
                streamed += token;
                if (streamed.size() > 8192)
                    throw intelligence::GenerationLimit("stream exceeded 8192-byte artifact budget",
                                                        streamed.substr(0, 8192));
                pending += token;
                try {
                    (void)json(pending).dump();
                } catch (const json::type_error &) {
                    return;
                }
                emit({{"schema", 1},
                      {"type", "token"},
                      {"trace_id", x.at("trace_id")},
                      {"root_id", x.at("root_id")},
                      {"instance_id", x.at("id")},
                      {"iteration", x.at("iteration")},
                      {"generation", x.value("generation", 0)},
                      {"speaker", x.at("speaker")},
                      {"text", pending}});
                pending.clear();
            });
            require(result.proposal.size() <= 8192, "proposal exceeds artifact budget");
            result.evaluation = evaluators_.create(x.value("evaluator", std::string("wds.ai.heart.rules/1")))
                                    ->evaluate(result.proposal, w.context);
        }
    } catch (const intelligence::GenerationLimit &e) {
        result.error = e.what();
        result.proposal = json::parse(json(e.partial).dump(-1, ' ', false, json::error_handler_t::replace))
                              .get<std::string>();
    } catch (const std::exception &e) {
        result.error = e.what();
        if (result.proposal.empty())
            result.proposal = json::parse(json(streamed).dump(-1, ' ', false, json::error_handler_t::replace))
                                  .get<std::string>();
    }
    result.elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start)
            .count();
    return result;
}
bool Engine::finish_step(const Work &w, Result result, Emit emit) {
    auto id = w.instance.at("id").get<std::string>();
    in_flight_.erase(id);
    auto x = store_.instances.at(id);
    auto root = x.at("root_id").get<std::string>(), trace = x.at("trace_id").get<std::string>();
    if (x.at("status") != "running" || x.value("generation", 0) != w.instance.value("generation", 0)) {
        event("work.discarded", root, trace,
              {{"instance_id", id},
               {"generation", w.instance.value("generation", 0)},
               {"reason", "episode paused, cancelled, or superseded"}},
              emit);
        return x.at("status") == "running";
    }
    x["speaker"] = w.instance.at("speaker");
    x["memory_sources"] = w.instance.value("memory_sources",json::array());
    if (!result.error.empty()) {
        x["status"] = "failed";
        x["error"] = result.error;
        x["partial_artifact"] = result.proposal;
        x["output_complete"] = false;
        event("wind.checkpoint", root, trace, x, emit);
        settle_signal(x, emit);
        return false;
    }
    x["speaker"] = w.instance.at("speaker");
    x["memory_sources"] = w.instance.value("memory_sources",json::array());
    auto record = result.evaluation.record();
    record["speaker"] = x.at("speaker");
    if (!result.action.is_null())
        event("action.executed", root, trace,
              {{"instance_id", id}, {"decision", result.action}, {"package_key", x.at("package_key")}}, emit);
    event("wind.moved", root, trace,
          {{"instance_id", id},
           {"before", x.at("state")},
           {"after", result.state},
           {"policy_version", x.at("policy_version")},
           {"policy_checksum", w.package->digest}},
          emit);
    event("heart.evaluated", root, trace,
          {{"instance_id", id},
           {"proposal", result.proposal},
           {"proposal_hash", checksum(result.proposal)},
           {"evaluation", record},
           {"elapsed_ms", result.elapsed_ms}},
          emit);
    x["artifact"] = result.proposal;
    x["state"] = result.state;
    x["feedback"] = result.evaluation.feedback;
    x["heart_score"] = result.evaluation.score;
    x["evaluation"] = record;
    if (!result.task.is_null())
        x["task"] = result.task;
    int iteration = x.at("iteration").get<int>() + 1;
    x["iteration"] = iteration;
    bool done = iteration >= x.at("max_steps").get<int>() ||
                (result.evaluation.passed() && (x.contains("task") || iteration >= 2));
    if (done)
        x["status"] = result.evaluation.passed() ? "completed" : "exhausted";
    event("wind.checkpoint", root, trace, x, emit);
    if (done) {
        auto relationship = store_.roots.at(root);
        int episodes = relationship.at("shared_episodes").get<int>() + 1;
        relationship["shared_episodes"] = episodes;
        relationship["relationship_level"] = std::min(5, 1 + int(std::log2(1. + episodes)));
        relationship["version"] = relationship.at("version").get<int>() + 1;
        event("root.updated", root, trace, relationship, emit);
        event("episode.finished", root, trace,
              {{"instance_id", id},
               {"status", x.at("status")},
               {"artifact", result.proposal},
               {"evaluation", record},
               {"speaker", x.at("speaker")}},
              emit);
        settle_signal(x, emit);
        if (!x.contains("review_key"))
            suggest_from_failure(x, emit);
    }
    return !done;
}
bool Engine::step(const std::string &id, Emit emit) {
    auto work = begin_step(id, emit);
    if (!work)
        return false;
    return finish_step(*work, compute_step(*work, emit), emit);
}
async::Task<bool> Engine::advance(std::string id, Emit emit, async::Executor &compute,
                                  async::Executor &coordinator) {
    auto work = begin_step(id, emit);
    if (!work)
        co_return false;
    Result result;
    try {
        co_await compute.schedule(false,
                                  work->instance.value("work_class", std::string("interactive")) == "workshop"
                                      ? async::Priority::workshop
                                  : work->instance.value("work_class", std::string("interactive")) == "upkeep"
                                      ? async::Priority::upkeep
                                      : async::Priority::interactive);
        result = compute_step(*work, emit);
    } catch (const std::exception &e) {
        result.error = e.what();
    }
    co_await coordinator.schedule(true);
    co_return finish_step(*work, std::move(result), emit);
}
void Engine::pause_all() {
    for (auto &kv : store_.instances) {
        auto x = kv.second;
        if (x.at("status") == "running") {
            x["status"] = "paused";
            store_.append("wind.checkpoint", x.at("root_id"), x.at("trace_id"), x);
        }
    }
}
} // namespace runtime
} // namespace wds::ai
