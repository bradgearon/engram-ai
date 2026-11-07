#include "wds/ai/core.hpp"
#include <algorithm>
#include <fstream>

namespace wds::ai::chain {
json Store::provenance(const std::string &root, uint64_t before, size_t limit) const {
    require(limit > 0 && limit <= 40, "history limit must be 1..40");
    json rows = json::array();
    auto found = event_offsets_.find(root);
    if (found == event_offsets_.end())
        return {{"events", rows}, {"next_before", nullptr}};
    const auto &index = found->second;
    auto it = std::lower_bound(index.begin(), index.end(), std::pair<uint64_t, uint64_t>{before, 0});
    std::ifstream file(directory_ / "events.jsonl", std::ios::binary);
    size_t bytes = 0;
    while (it != index.begin() && rows.size() < limit) {
        --it;
        file.seekg(it->second);
        std::string line;
        require(bool(std::getline(file, line)), "cannot read indexed event");
        if (!rows.empty() && bytes + line.size() > 180000) {
            ++it;
            break;
        }
        rows.push_back(json::parse(line));
        bytes += line.size();
    }
    return {{"events", rows},
            {"next_before", it != index.begin() && !rows.empty() ? rows.back().at("seq") : json()}};
}
} // namespace wds::ai::chain

namespace wds::ai::runtime {
bool Engine::workspace_command(const json &m, Emit emit) {
    auto op = m.at("type").get<std::string>(), trace = m.at("request_id").get<std::string>();
    if (op.rfind("workspace.", 0) != 0 && op.rfind("chat.", 0) != 0 && op.rfind("project.", 0) != 0 &&
        op.rfind("soul.inspect", 0) != 0 && op != "provenance.page")
        return false;
    auto reply = [&](const std::string &type, const json &data) {
        emit({{"schema", 1}, {"type", type}, {"trace_id", trace}, {"data", data}});
    };
    if (op == "workspace.snapshot") {
        json projects = json::array(), chats = json::array();
        for (const auto &[id, p] : store_.projects)
            projects.push_back(p);
        for (const auto &[id, c] : store_.chats) {
            auto row = c;
            auto pending = c.at("pending_instance").get<std::string>();
            row["pending_status"] =
                store_.instances.count(pending) ? store_.instances.at(pending).at("status") : json("");
            chats.push_back(row);
        }
        reply("workspace.snapshot", {{"projects", projects},
                                     {"chats", chats},
                                     {"sequence", store_.sequence()},
                                     {"foundation", foundation_.name()}});
        return true;
    }
    if (op == "provenance.page") {
        auto root = field(m, "root_id", 80);
        require(store_.roots.count(root), "unknown soul scope");
        auto before = m.contains("before") ? upkeep::integer(m, "before", 1, INT64_MAX)
                                           : int64_t(store_.sequence() + 1);
        auto limit = m.contains("limit") ? upkeep::integer(m, "limit", 1, 40) : 20;
        reply("provenance.page", store_.provenance(root, before, limit));
        return true;
    }
    if (op == "soul.inspect") {
        auto root = field(m, "root_id", 80);
        require(store_.roots.count(root), "unknown soul scope");
        auto offset = m.contains("offset") ? upkeep::integer(m, "offset", 0, 100000) : 0;
        json memories = json::array(), neurons = json::array(), ideas = json::array(),
             outputs = json::array();
        size_t total = 0;
        for (const auto &[id, e] : store_.engrams)
            if (e.at("root_id") == root) {
                if (total++ >= size_t(offset) && memories.size() < 32)
                    memories.push_back(e);
            }
        for (const auto &[id, n] : store_.neurons)
            if (n.at("root_id") == root)
                neurons.push_back(n);
        for (const auto &[id, i] : store_.ideas)
            if (i.at("root_id") == root)
                ideas.push_back(i);
        auto learning = store_.learning_index.find(root);
        if (learning != store_.learning_index.end())
            for (auto it = learning->second.rbegin(); it != learning->second.rend() && outputs.size() < 8;
                 ++it) {
                const auto &x = store_.instances.at(it->second);
                outputs.push_back({{"id", x.at("id")},
                                   {"artifact", x.at("artifact")},
                                   {"status", x.at("status")},
                                   {"speaker", x.value("speaker", json::object())},
                                   {"memory_sources", x.value("memory_sources", json::array())},
                                   {"package_key", x.at("package_key")}});
            }
        json weights = json::array();
        for (const auto &[key, a] : store_.soul_atoms)
            if (key.rfind(root + "/", 0) == 0 && (a.flags & 1))
                weights.push_back(
                    {{"key", key},
                     {"weight", a.weight},
                     {"version",
                      store_.soul_weights.count(key) ? store_.soul_weights.at(key).at("version") : json(0)}});
        auto scope = relationships::snapshot(store_, root);
        auto scope_record = scope.record();
        json profile_versions = json::object();
        for (const auto &s : scope_record.at("souls")) {
            auto soul = s.at("soul_id").get<std::string>();
            auto key = root + "/" + soul;
            profile_versions[soul] =
                store_.profiles.count(key) ? store_.profiles.at(key).at("version") : json(0);
        }
        reply("soul.snapshot",
              {{"root", store_.roots.at(root)},
               {"scope", scope_record},
               {"mesh", mesh::topology(scope)},
               {"weights", weights},
               {"profile_versions", profile_versions},
               {"outputs", outputs},
               {"memories", memories},
               {"neurons", neurons},
               {"ideas", ideas},
               {"memory_total", total},
               {"next_offset",
                size_t(offset) + memories.size() < total ? json(size_t(offset) + memories.size()) : json()},
               {"flesh", store_.flesh.count(root) ? store_.flesh.at(root) : upkeep::Flesh{}.record()}});
        return true;
    }
    if (op == "chat.history") {
        auto id = field(m, "chat_id", 80);
        require(store_.chats.count(id), "unknown chat");
        const auto &ids = store_.messages_by_chat[id];
        auto before = m.contains("before") ? upkeep::integer(m, "before", 0, INT64_MAX) : int64_t(ids.size());
        size_t end = std::min(size_t(before), ids.size()), start = end, bytes = 0;
        while (start > 0 && end - start < 60) {
            auto size = store_.chat_messages.at(ids[start - 1]).dump().size();
            if (start < end && bytes + size > 160000)
                break;
            bytes += size;
            --start;
        }
        json messages = json::array();
        for (size_t i = start; i < end; ++i)
            messages.push_back(store_.chat_messages.at(ids[i]));
        reply("chat.history", {{"chat", store_.chats.at(id)},
                               {"messages", messages},
                               {"next_before", start ? json(start) : json()}});
        return true;
    }
    auto intent = m;
    intent.erase("request_id");
    auto intent_hash = checksum(intent.dump());
    if (store_.workspace_requests.count(trace)) {
        const auto &saved = store_.workspace_requests.at(trace);
        require(saved.at("data").at("workspace").at("intent") == intent_hash,
                "workspace request_id conflict");
        emit(saved);
        return true;
    }
    require(store_.workspace_requests.size() < 16384, "workspace request capacity reached");
    auto commit = [&](const std::string &type, const std::string &root, json w, json extra = json::object()) {
        w["request_key"] = trace;
        w["intent"] = intent_hash;
        extra["workspace"] = w;
        return event(type, root, trace, extra, emit);
    };
    if (op == "project.create") {
        auto id = field(m, "project_id", 80), name = field(m, "name", 80), purpose = field(m, "purpose", 512);
        identifier(id);
        require(!store_.projects.count(id), "project already exists");
        require(store_.projects.size() < 128, "project capacity reached");
        auto kind = m.value("kind", std::string("project"));
        require(kind == "project" || kind == "account", "invalid project kind");
        auto root = roots::select(store_, id, purpose, trace);
        root["soul_name"] = name;
        event("root.updated", id, trace, root, emit);
        json project = {
            {"id", id}, {"name", name}, {"purpose", purpose}, {"kind", kind}, {"created_ms", now_ms()}};
        commit("project.saved", id, {{"project", project}});
        return true;
    }
    if (op == "chat.create") {
        auto id = field(m, "chat_id", 80), root = field(m, "project_id", 80);
        identifier(id);
        require(store_.projects.count(root), "choose an existing project");
        require(!store_.chats.count(id) && store_.chats.size() < 512, "chat exists or capacity reached");
        json chat = {{"id", id},
                     {"project_id", root},
                     {"title", field(m, "title", 120)},
                     {"version", 1},
                     {"updated_ms", now_ms()},
                     {"soul_id", ""},
                     {"mode", "wind"},
                     {"wind_key", ""},
                     {"background", true},
                     {"guests", json::array()},
                     {"last_background_ms", 0},
                     {"pending_signal", ""},
                     {"pending_instance", ""},
                     {"last_instance", ""}};
        commit("chat.saved", root, {{"chat", chat}});
        return true;
    }
    auto id = field(m, "chat_id", 80);
    require(store_.chats.count(id), "unknown chat");
    auto chat = store_.chats.at(id);
    auto root = chat.at("project_id").get<std::string>();
    if (op == "chat.update") {
        require(chat.at("version") == m.at("expected_version"), "chat changed; refresh before editing");
        require(chat.at("pending_signal") == "" && chat.value("queued_messages", json::array()).empty(), "wait for responses and queued messages before changing focus");
        if (m.contains("title"))
            chat["title"] = field(m, "title", 120);
        if (m.contains("project_id")) {
            root = field(m, "project_id", 80);
            require(store_.projects.count(root), "unknown project");
            chat["project_id"] = root;
            chat["soul_id"] = "";
            chat["last_instance"] = "";
            chat["guests"] = json::array();
        }
        if (m.contains("soul_id")) {
            auto s = m.at("soul_id").get<std::string>();
            require(s.empty() || relationships::snapshot(store_, root).find(s), "soul outside project");
            chat["soul_id"] = s;
        }
        if (m.contains("mode")) {
            require(m["mode"] == "host" || m["mode"] == "wind", "invalid conversation mode");
            chat["mode"] = m["mode"];
        }
        if (m.contains("wind_key")) {
            auto k = m.at("wind_key").get<std::string>();
            require(k.empty() || packages_.count(k), "unknown Wind");
            chat["wind_key"] = k;
        }
        if (m.contains("background")) {
            require(m["background"].is_boolean(), "background must be boolean");
            chat["background"] = m["background"];
        }
        if (m.contains("guests")) {
            require(m["guests"].is_array() && m["guests"].size() <= 8, "at most eight invited projects");
            for (const auto &g : m["guests"])
                require(g.is_string() && store_.projects.count(g.get<std::string>()) && g != root,
                        "unknown guest project");
            chat["guests"] = m["guests"];
        }
        chat["version"] = chat.at("version").get<int>() + 1;
        commit("chat.saved", root, {{"chat", chat}});
        return true;
    }
    require(op == "chat.send" || op == "chat.reflect" || op == "chat.review", "unknown workspace command");
    auto text = field(m, "text", 2048);
    require(store_.chat_messages.size() < 32768, "chat message capacity reached; export before continuing");
    if (op == "chat.review") {
        require(chat.at("background") == true, "background contributions are paused for this chat");
        if (chat.at("mode") == "wind") {
            const auto selected = chat.at("soul_id") == "" ? root : chat.at("soul_id").get<std::string>();
            if (!store_.mesh_neurons.count(root + "/" + selected)) {
                auto n = neural_node(root, selected, selected); n["version"] = 1;
                event("mesh.node_registered", root, trace, {{"mesh", {{"nodes", json::array({n})}}}}, emit);
            }
            json request = {{"schema", 1}, {"type", "mesh.review"}, {"request_id", "review-" + checksum(trace)},
                            {"root_id", root}, {"node_id", selected}, {"text", text}, {"chat_id", id},
                            {"due_in_ms", m.value("due_in_ms", 60000)}};
            if (chat.at("wind_key") != "") request["package_key"] = chat.at("wind_key");
            command(request, emit);
            commit("chat.reviewed", root, json::object());
            return true;
        }
        json request = {{"schema", 1},
                        {"type", "review.suggest"},
                        {"request_id", "review-" + checksum(trace)},
                        {"root_id", root},
                        {"soul_id", chat.at("soul_id") == "" ? json(root) : chat.at("soul_id")},
                        {"text", text},
                        {"chat_id", id},
                        {"reason", "Conversation follow-up requested; bounded by shared upkeep budget"},
                        {"due_in_ms", m.value("due_in_ms", 60000)}};
        command(request, emit);
        commit("chat.reviewed", root, json::object());
        return true;
    }
    auto source = m.value("source_root", root);
    require(source == root ||
                std::find(chat["guests"].begin(), chat["guests"].end(), source) != chat["guests"].end(),
            "invite the source project before asking its soul to speak");
    require(store_.projects.count(source), "source project unavailable");
    auto queue = chat.value("queued_messages", json::array());
    const auto queued_id = m.value("queued_message_id", std::string{});
    if (!queued_id.empty()) {
        require(op == "chat.send" && chat.at("pending_signal") == "" && !queue.empty() &&
                    queue.front().at("message_id") == queued_id, "queued message is not ready");
        auto original = m; original.erase("queued_message_id");
        original["request_id"] = queue.front().at("command").at("request_id");
        require(original == queue.front().at("command"), "queued message command changed");
    } else if (op == "chat.send" && (chat.at("pending_signal") != "" || !queue.empty())) {
        require(queue.size() < 16, "conversation queue is full (16 waiting messages); wait for a response before sending more");
        const auto message_id = "message-" + checksum(trace);
        queue.push_back({{"message_id",message_id},{"command",m}});
        require(queue.dump().size() <= 65536, "conversation queue byte budget reached; wait for a response");
        chat["queued_messages"] = queue; chat["updated_ms"] = now_ms();
        chat["version"] = chat.at("version").get<int>() + 1;
        json message = {{"id",message_id},{"chat_id",id},{"role","user"},{"text",text},{"time_ms",now_ms()},
                        {"origin_project",root},{"source_root",source},{"kind","message"},{"status","queued"}};
        commit("chat.message",root,{{"chat",chat},{"message",message}});
        if (chat.at("pending_signal") == "") ready_chats_.insert(id);
        return true;
    }
    require(chat.at("pending_signal") == "", "wait for the response before adding a reflection");
    auto user = std::string("workspace-user");
    if (!relationships::snapshot(store_, source).find(user))
        command({{"schema", 1},
                 {"type", "relationship.declare"},
                 {"request_id", "user-" + checksum(source)},
                 {"root_id", source},
                 {"soul_id", user},
                 {"soul_name", "You"},
                 {"purpose", "Human goals and feedback"},
                 {"relationship_to", source},
                 {"relationship", "conversation partner"},
                 {"voice_id", "purpose"},
                 {"expected_version", 0},
                 {"paused", true}},
                emit);
    json reflection = {{"schema", 1},
                       {"type", "reflection.record"},
                       {"request_id", "reflect-" + checksum(trace)},
                       {"root_id", source},
                       {"actor_soul_id", user},
                       {"actor_kind", "human"},
                       {"text", text},
                       {"chat_id", id}};
    if (source == root && chat.at("soul_id") != "")
        reflection["soul_id"] = chat.at("soul_id");
    if (m.contains("instance_id"))
        reflection["instance_id"] = m.at("instance_id");
    command(reflection, emit);
    json message = {{"id", "message-" + checksum(trace)},
                    {"chat_id", id},
                    {"role", "user"},
                    {"text", text},
                    {"time_ms", now_ms()},
                    {"origin_project", root},
                    {"source_root", source},
                    {"kind", op == "chat.reflect" ? "reflection" : "message"},
                    {"status", "saved"}};
    if (!queued_id.empty()) {
        message["id"] = queued_id; message["time_ms"] = store_.chat_messages.at(queued_id).at("time_ms");
        queue.erase(queue.begin()); chat["queued_messages"] = queue;
    }
    if (op == "chat.send" && chat.at("mode") == "wind") {
        const auto selected = source == root && chat.at("soul_id") != "" ?
            chat.at("soul_id").get<std::string>() : source;
        auto node_key = source + "/" + selected;
        if (!store_.mesh_neurons.count(node_key)) {
            auto n = neural_node(source, selected, selected); n["version"] = 1;
            event("mesh.node_registered", source, trace, {{"mesh", {{"nodes", json::array({n})}}}}, emit);
        }
        auto key = chat.at("wind_key").get<std::string>();
        require(key.empty() || (packages_.count(key) && packages_.at(key)->manifest.value("feature_schema", std::string{}) == neural::feature_schema),
                "select a neural Wind for mesh conversation; structured action Winds belong in Training");
        json accepted;
        json signal = {{"schema", 1}, {"type", "mesh.signal"}, {"request_id", "chat-" + checksum(trace)},
                       {"root_id", source}, {"node_id", selected}, {"text", text}, {"chat_id", id}};
        if (!key.empty()) signal["package_key"] = key;
        if (m.contains("wind_instance_id")) signal["wind_instance_id"] = m.at("wind_instance_id");
        command(signal, [&](const json &e) { emit(e); if(e.at("type")=="mesh.signal_queued")accepted=e.at("data"); });
        require(accepted.contains("pulse_key"), "mesh signal was not accepted");
        chat["pending_signal"] = accepted.at("pulse_key");
        chat["pending_source_root"] = source;
    } else if (op == "chat.send") {
        json signal = {{"schema", 1},
                       {"type", "neuron.signal"},
                       {"request_id", "chat-" + checksum(trace)},
                       {"root_id", source},
                       {"text", text},
                       {"max_steps", 1},
                       {"max_output_tokens", m.value("max_output_tokens", 1024)},
                       {"force", true},
                       {"mode", "wind"}};
        if (source == root && chat.at("soul_id") != "")
            signal["soul_id"] = chat.at("soul_id");
        if (chat.at("mode") == "host")
            signal["soul_id"] = source;
        auto wk = chat.at("wind_key").get<std::string>();
        if (chat.at("mode") == "wind" && !wk.empty()) {
            require(packages_.count(wk) && !packages_.at(wk)->manifest.contains("action_domain"),
                    "this action Wind trains structured tasks; choose it in Training, not text chat");
            signal["package_key"] = wk;
        }
        auto prior = chat.at("last_instance").get<std::string>();
        if (!prior.empty() && store_.instances.count(prior) &&
            store_.instances.at(prior).at("root_id") == source)
            signal["reply_to"] = prior;
        json decision;
        command(signal, [&](const json &e) {
            emit(e);
            if (e.at("type") == "neuron.decision")
                decision = e.at("data");
        });
        require(decision.contains("signal_key"), "signal was not accepted");
        chat["pending_signal"] = decision.at("signal_key");
    }
    if (chat.at("title") == "New conversation") {
        size_t end = std::min(size_t(70), text.size());
        while (end < text.size() && end > 0 && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80)
            --end;
        chat["title"] = text.substr(0, end);
    }
    chat["updated_ms"] = now_ms();
    chat["version"] = chat.at("version").get<int>() + 1;
    commit("chat.message", root, {{"chat", chat}, {"message", message}});
    if (op == "chat.reflect" && chat.at("background") == true &&
        store_.upkeep_settings.at("enabled") == true) {
        bool waiting = false;
        for (const auto &[key, r] : store_.reviews)
            if (r.value("chat_id", std::string{}) == id &&
                (r.at("status") == "scheduled" || r.at("status") == "running" ||
                 r.at("status") == "waiting_neuron"))
                waiting = true;
        if (!waiting && now_ms() - chat.at("last_background_ms").get<int64_t>() >= 1800000) {
            try {
                command({{"schema", 1},
                         {"type", "review.suggest"},
                         {"request_id", "reflect-review-" + checksum(trace)},
                         {"root_id", root},
                         {"chat_id", id},
                         {"text", text},
                         {"reason", "Attributed reflection raises a small desire to reconsider"},
                         {"engram_ids",
                          json::array({"reflection-" + checksum(source + "/reflect-" + checksum(trace))})}},
                        emit);
            } catch (const std::exception &error) {
                event("chat.contribution_held", root, trace, {{"chat_id", id}, {"reason", error.what()}},
                      emit);
            }
        }
    }
    return true;
}

void Engine::workspace_event(const json &e, Emit emit) {
    auto type = e.at("type").get<std::string>();
    const bool assembled = type == "assembly.completed";
    if (type != "neuron.dispatched" && type != "neuron.completed" && type != "neuron.failed" &&
        type != "mesh.dispatched" && type != "mesh.impulse" && type != "mesh.web_expired" && type != "mesh.web_received" &&
        type != "mesh.deferred" && type != "mesh.pulse_changed" && !assembled)
        return;
    auto d = e.at("data");
    if (!assembled && type != "mesh.dispatched" && d.contains("mesh") && d.at("mesh").contains("pulses")) {
        bool belongs = false;
        for (const auto &p : d.at("mesh").at("pulses")) if (store_.assembly_by_lineage.count(p.at("lineage"))) belongs = true;
        if (belongs) return; // One assembled reply owns delivery for this lineage.
    }
    if (assembled) {
        type = "neuron.completed";
        d["instance_id"] = store_.mesh_pulses.at(d.at("signal_key")).value("instance_id", std::string{});
    }
    if (type == "mesh.dispatched") { type = "neuron.dispatched"; d["signal_key"] = d.at("pulse_key"); }
    if (type == "mesh.impulse") {
        if (d.at("status") == "waiting_web" || d.at("status") == "queued") return;
        type = "neuron.completed"; d["signal_key"] = d.at("pulse_key");
        const auto &x = store_.instances.at(d.at("instance_id"));
        d["artifact"] = x.at("artifact");
    }
    if (type == "mesh.web_expired" || type == "mesh.web_received") {
        const auto &p = d.at("mesh").at("pulses").front();
        if(p.at("status")!="held")return;
        d["signal_key"]=p.at("key");d["instance_id"]=p.at("instance_id");d["status"]="held";
        d["artifact"]=p.at("reason");d["reason"]=p.at("reason");type="neuron.completed";
    }
    if (type == "mesh.deferred" || type == "mesh.pulse_changed") {
        const auto &p = d.at("mesh").at("pulses").front();
        if (p.at("status") == "queued" || p.at("status") == "running") return;
        d["signal_key"]=p.at("key");d["instance_id"]=p.at("instance_id");d["status"]=p.at("status");
        d["artifact"]=p.at("reason");d["reason"]=p.at("reason");type="neuron.completed";
    }
    if (!d.contains("signal_key"))
        return;
    auto signal = d.at("signal_key").get<std::string>();
    std::string target;
    bool background = false;
    for (const auto &[id, c] : store_.chats)
        if (c.at("pending_signal") == signal) {
            target = id;
            break;
        }
    auto instance = d.value("instance_id", std::string{});
    if (target.empty() && type == "neuron.completed" && store_.mesh_pulses.count(signal)) {
        const auto &pulse = store_.mesh_pulses.at(signal);
        if (pulse.value("background", false)) {
            target = pulse.value("chat_id", std::string{}); background = true;
        }
    }
    if (target.empty() && type == "neuron.completed" && !instance.empty() &&
        store_.instances.count(instance)) {
        const auto &x = store_.instances.at(instance);
        if (x.contains("review_key") && store_.reviews.count(x.at("review_key"))) {
            const auto &r = store_.reviews.at(x.at("review_key"));
            target = r.value("chat_id", std::string{});
            background = true;
        }
    }
    if (target.empty() || !store_.chats.count(target))
        return;
    auto chat = store_.chats.at(target);
    auto root = chat.at("project_id").get<std::string>();
    if (type == "neuron.dispatched") {
        chat["pending_instance"] = instance;
        emit(store_.append("chat.saved", root, e.at("trace_id"), {{"workspace", {{"chat", chat}}}}));
        return;
    }
    if (background && (chat.at("background") != true || chat.at("pending_signal") != "" ||
                       store_.chat_messages.size() >= 32768 ||
                       now_ms() - chat.at("last_background_ms").get<int64_t>() < 1800000 ||
                       d.value("status", std::string{}) != "completed")) {
        json held = {{"chat_id", target}, {"instance_id", instance},
                     {"reason", "quiet contribution gate: paused, busy, incomplete, capacity or 30-minute interval"}};
        if (assembled) { auto a = store_.assemblies.at(d.at("assembly_key")); a["delivery"] = "held";
            held["mesh"] = {{"assemblies", json::array({a})}}; }
        emit(store_.append("chat.contribution_held", root, e.at("trace_id"), held));
        return;
    }
    auto source = e.at("root_id").get<std::string>();
    if (source != root &&
        std::find(chat["guests"].begin(), chat["guests"].end(), source) == chat["guests"].end())
        return;
    auto x = store_.instances.count(instance) ? store_.instances.at(instance) : json::object();
    auto id = "reply-" + checksum(target + "/" + signal);
    if (store_.chat_messages.count(id))
        return;
    json message = {{"id", id},
                    {"chat_id", target},
                    {"role", "assistant"},
                    {"text", assembled ? d.at("artifact") : json(x.value("partial_artifact", d.value("artifact", std::string{})))},
                    {"time_ms", now_ms()},
                    {"origin_project", source},
                    {"origin_chat", target},
                    {"instance_id", instance},
                    {"kind", background ? "contribution" : "message"},
                    {"status", d.value("status", std::string("failed"))},
                    {"speaker", assembled ? d.at("speaker") : x.value("speaker", json::object())},
                    {"wind_key", x.value("package_key", std::string{})},
                    {"error", (d.value("status", std::string{}) == "completed" || d.value("status", std::string{}) == "reuse") ? std::string{} : x.value("error", d.value("reason", std::string{}))},
                    {"evaluation", x.value("evaluation", json::object())},
                    {"evidence_sequence", e.at("seq")},
                    {"memory_sources", x.value("memory_sources", json::array())}};
    if(x.contains("language_trace"))message["language_trace"]=x.at("language_trace");
    if (assembled) {
        const auto &a = store_.assemblies.at(d.at("assembly_key"));
        message["assembly_key"] = a.at("key"); message["expression_package"] = a.at("expression_package");
        message["expression"] = a.at("expression"); message["memory_sources"] = json::array();
        for (const auto &source : a.at("expression").at("sources"))
            message["memory_sources"].push_back({{"engram_id", source.at("id")}, {"root_id", source.at("root_id")},
                {"version", source.at("version")}, {"chat_id", source.value("attribution", json::object()).value("chat_id", std::string{})}});
        message["evaluation"] = {{"status", a.at("status")}, {"evaluator_id", "native-expression-contract/1"},
                                  {"scope", "provenance and expression contract; no independent truth validation"}};
        message["error"] = a.value("error", std::string{});
    }
    chat["last_instance"] = instance;
    chat["updated_ms"] = now_ms();
    chat["version"] = chat.at("version").get<int>() + 1;
    if (background)
        chat["last_background_ms"] = now_ms();
    else {
        chat["pending_signal"] = "";
        chat["pending_source_root"] = "";
        chat["pending_instance"] = "";
    }
    json changes = {{"workspace", {{"chat", chat}, {"message", message}}}};
    if (assembled) { auto a = store_.assemblies.at(d.at("assembly_key")); a["delivery"] = "delivered";
        changes["mesh"] = {{"assemblies", json::array({a})}}; }
    emit(store_.append("chat.message", root, e.at("trace_id"), changes));
    if (!background && !chat.value("queued_messages", json::array()).empty()) ready_chats_.insert(target);
}

void Engine::workspace_dequeue(Emit emit) {
    auto ready = std::move(ready_chats_); ready_chats_.clear();
    for (const auto &id : ready) {
        auto chat = store_.chats.at(id);
        auto queue = chat.value("queued_messages", json::array());
        if (chat.at("pending_signal") != "" || queue.empty()) continue;
        auto m = queue.front().at("command");
        auto message_id = queue.front().at("message_id").get<std::string>();
        m["queued_message_id"] = message_id;
        m["request_id"] = "dequeue-" + checksum(m.at("request_id"));
        try { command(m, emit); }
        catch (const std::exception &error) {
            chat = store_.chats.at(id); queue = chat.value("queued_messages",json::array());
            if (queue.empty() || queue.front().at("message_id") != message_id) throw;
            queue.erase(queue.begin()); chat["queued_messages"] = queue;
            chat["version"] = chat.at("version").get<int>() + 1;
            auto message = store_.chat_messages.at(message_id); message["status"] = "failed"; message["error"] = error.what();
            emit(store_.append("chat.message",chat.at("project_id"),m.at("request_id"),{{"workspace",{{"chat",chat},{"message",message}}}}));
            if (!queue.empty()) ready_chats_.insert(id);
        }
    }
}

void Engine::workspace_recover(Emit emit) {
    // Reconcile completion persisted before chat delivery (including old MSVC
    // singleton-record failures). All repairs append events; history is untouched.
    for (const auto &[key, signal] : store_.signals) {
        const auto id = signal.value("instance_id",std::string{});
        if (signal.at("status") == "running" && store_.instances.count(id)) {
            const auto x = store_.instances.at(id);
            if (x.at("status") != "running" && x.at("status") != "paused") settle_signal(x,emit);
        }
    }
    for (const auto &[key, saved] : store_.neurons) {
        auto active = saved.value("active", std::string{});
        if (active.empty()) continue;
        if (store_.instances.count(active) &&
            (store_.instances.at(active).at("status") == "running" || store_.instances.at(active).at("status") == "paused")) continue;
        auto n = saved; n["active"] = "";
        event("neuron.recovered", saved.at("root_id"), "startup-recovery",
              {{"reason","released stale activity reservation"},{"instance_id",active},
               {"maintenance",{{"neurons",json::array({n})}}}},emit);
    }
    for (const auto &[id, saved] : store_.chats) {
        const auto signal_key = saved.at("pending_signal").get<std::string>();
        if (store_.signals.count(signal_key)) {
            auto signal = store_.signals.at(signal_key);
            const auto instance = signal.value("instance_id", std::string{});
            if (store_.instances.count(instance)) {
                auto x = store_.instances.at(instance);
                if (x.at("status") != "running" && x.at("status") != "paused") {
                    settle_signal(x, emit);
                    workspace_event({{"type","neuron.completed"},{"root_id",x.at("root_id")},
                        {"trace_id","chat-recovery-"+checksum(id+signal_key)},{"seq",store_.instance_sequences.at(instance)},
                        {"data",{{"signal_key",signal_key},{"instance_id",instance},{"status",x.at("status")},
                                  {"artifact",x.at("artifact")},{"reason",x.value("error",std::string{})}}}},emit);
                }
            }
        }
        const auto &chat = store_.chats.at(id);
        if (chat.at("pending_signal") == "" && !chat.value("queued_messages",json::array()).empty()) ready_chats_.insert(id);
    }
}
} // namespace wds::ai::runtime
