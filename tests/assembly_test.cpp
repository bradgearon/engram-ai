#include "wds/ai/core.hpp"
#include <iostream>
using namespace wds::ai;
struct QuietFoundation final : intelligence::Foundation {
    int generations = 0;
    std::string name() const override { return "assembly-test-no-generation"; }
    std::string propose(const intelligence::Request &, intelligence::Token) override {
        ++generations; throw std::runtime_error("engram expression must not request prose generation");
    }
};
int main(int argc, char **argv) {
    auto dir = fs::temp_directory_path() / ("wds-assembly-" + std::to_string(now_ms()));
    try {
        require(argc == 2, "package required"); winds::Package package(argv[1]); QuietFoundation foundation;
        std::string unfinished;
        {
            chain::Store store(dir); runtime::Engine engine(store, package, foundation);engine.configure_language("legacy");
            int seq = 0; std::vector<json> events;
            auto emit = [&](const json &e) { events.push_back(e); };
            auto cmd = [&](json m) { m["schema"] = 1; if (!m.contains("request_id")) m["request_id"] = "a-" + std::to_string(++seq); engine.command(m, emit); };
            auto config = [&](json settings) { cmd({{"type", "mesh.configure"}, {"expected_version", store.mesh_settings.at("version")}, {"settings", settings}}); };
            auto drain = [&] { for (int i = 0; i < 32; ++i) { auto ids = engine.dispatch_ready(now_ms() + 1000, emit); if (ids.empty()) break; for (const auto &id : ids) engine.step(id, emit); } };
            auto teach = [&](const std::string &id, int version, const std::string &value, const std::string &root = "home") {
                cmd({{"type", "assembly.teach"}, {"root_id", root}, {"engram_id", id}, {"expected_version", version},
                     {"subject", "The Last Cat"}, {"predicate", "is"}, {"object", value}, {"source", "Operator project notes"}, {"exclusive", true}});
            };
            auto ask = [&](std::string text) {
                const auto before = events.size();
                cmd({{"type", "mesh.signal"}, {"root_id", "home"}, {"node_id", "home"}, {"text", text}, {"response", true}});
                for (size_t i = before; i < events.size(); ++i) if (events[i].at("type") == "mesh.signal_queued") return events[i].at("data").at("assembly_key").get<std::string>();
                throw std::runtime_error("missing assembly acknowledgment");
            };
            for (auto root : {"home", "foreign"}) cmd({{"type", "root.select"}, {"root_id", root}, {"purpose", "The Last Cat project"}});
            config({{"refractory_ms", 10}, {"max_impulses_per_window", 1024}, {"max_knowledge_calls", 0}});
            teach("definition", 0, "an exploration game about lost stars");
            teach("secret", 0, "FOREIGN_PRIVATE_VALUE", "foreign"); drain();
            cmd({{"type", "mesh.node.register"}, {"root_id", "home"}, {"node_id", "listener"}, {"expected_version", 0}, {"purpose", "The Last Cat understanding"}});
            cmd({{"type", "mesh.route.connect"}, {"root_id", "home"}, {"from", "home"}, {"to", "listener"}, {"expected_version", 0}});
            auto first = ask("What is The Last Cat?");
            auto ids = engine.dispatch_ready(now_ms() + 1000, emit); require(!ids.empty(), "response not dispatched");
            for (const auto &id : ids) engine.step(id, emit);
            require(store.assemblies.at(first).at("status") == "collecting", "first neuron spoke before connected contribution");
            drain(); const auto &a = store.assemblies.at(first);
            require(a.at("status") == "completed" && a.at("contributions").size() == 2, "assembly did not collect both neurons");
            auto reply = a.at("expression").at("text").get<std::string>();
            require(reply.find("an exploration game about lost stars") != std::string::npos, "meaning absent from actual reply");
            require(reply.find("FOREIGN_PRIVATE_VALUE") == std::string::npos, "foreign evidence entered response");
            require(a.at("expression").at("sources").size() == 1, "duplicate neuronal evidence was not collapsed");
            require(a.at("speaker").at("selected_soul") == "home", "engram lost speaker ownership");
            require(foundation.generations == 0, "native response generated through foundation");
            teach("definition", 1, "a game about returning light"); drain();
            auto corrected = ask("What is The Last Cat?"); drain();
            const auto &changed = store.assemblies.at(corrected).at("expression");
            require(changed.at("text").get<std::string>().find("returning light") != std::string::npos && changed.at("sources")[0].at("version") == 2, "new reply ignored corrected understanding");
            require(store.assemblies.at(first).at("expression").at("sources")[0].at("version") == 1, "correction rewrote historical evidence");
            cmd({{"type", "assembly.attune"}, {"root_id", "home"}, {"engram_id", "definition"}, {"expected_version", 2}, {"importance", 0.8}});
            drain();
            const auto &memory = store.engrams.at("home/definition");
            require(memory.at("version") == 3 && memory.at("attunement").at("importance") == 0.8, "attention importance not versioned in Arc");
            require(memory.at("claim_status") == "hypothetical", "importance improperly changed truth status");
            auto locations = mesh::IGFBP2Indexer(store).locate("home", embed("The Last Cat"));
            for (const auto &location : locations) {
                require(!location.contains("text") && !location.contains("semantic") && !location.contains("learning_link"), "indexer returned payload data");
                auto fetched = mesh::IGF2BP2Handler(store).fetch("home", {location});
                require(fetched.size() == 1 && fetched[0].contains("text"), "handler did not fetch Arc payload");
            }
            require(store.engrams.at("home/" + first).at("arc_dataset") == "astroengrams" && memory.at("arc_dataset") == "engrams", "Arc dataset ownership lost");
            // Numeric importance changes attention, without modifying weights or activating a package.
            auto low = memory, high = memory; low["id"] = "low"; high["id"] = "high";
            low["attunement"]["importance"] = 0.; high["attunement"]["importance"] = 1.;
            auto focus = understanding::attend({low, high}, memory.at("key").get<Embedding>(), false, false);
            require(focus.candidates[0].at("id") == "high", "importance did not affect numeric attention");
            auto greeting = ask("hello"); drain();
            require(store.assemblies.at(greeting).at("expression").at("action") == "acknowledge", "greeting did not produce native words");
            auto missing = ask("What causes orbital resonance?"); drain();
            require(store.assemblies.at(missing).at("expression").at("text").get<std::string>().find("don't have enough") != std::string::npos, "missing evidence pretended to answer");
            json feedback = {{"type", "assembly.feedback"}, {"request_id", "feedback-replay"}, {"root_id", "home"}, {"assembly_key", first}, {"action", "answer"}, {"note", "Answer from the cited relationship"}};
            cmd(feedback); auto count = store.sequence(); cmd(feedback); require(store.sequence() == count, "feedback replay changed chain");
            require(store.active_packages.empty(), "feedback automatically activated a Wind");
            teach("different", 0, "a wholly different exclusive definition"); drain();
            auto disputed = ask("What is The Last Cat?"); drain();
            require(store.assemblies.at(disputed).at("status") == "held" && store.assemblies.at(disputed).at("conflict") == true, "conflict was silently collapsed");
            config({{"enabled", false}, {"response_deadline_ms", 1000}});
            auto expired = ask("What is The Last Cat?"); engine.dispatch_ready(now_ms() + 2000, emit);
            require(store.assemblies.at(expired).at("status") == "incomplete", "response deadline failed silently");
            require(store.assemblies.at(expired).at("pending") == 0, "expired assembly still has work");
            auto cancelled = ask("Please stop this response");
            cmd({{"type", "mesh.pulse.cancel"}, {"root_id", "home"}, {"pulse_key", store.assemblies.at(cancelled).at("origin_pulse")}});
            require(store.assemblies.at(cancelled).at("status") == "cancelled", "cancellation did not reach assembly");
            config({{"enabled", true}, {"response_deadline_ms", 30000}});
            unfinished = ask("What is The Last Cat?"); engine.dispatch_ready(now_ms() + 1000, emit);
        }
        {
            chain::Store store(dir); runtime::Engine engine(store, package, foundation);engine.configure_language("legacy"); auto emit = [](const json &) {};
            for (int i = 0; i < 16; ++i) { auto ids = engine.dispatch_ready(now_ms() + 1000, emit); if (ids.empty()) break; for (const auto &id : ids) engine.step(id, emit); }
            require(store.assemblies.at(unfinished).at("status") != "collecting", "assembly did not resume after restart");
            require(store.expression_samples.size() == 1 && store.active_packages.empty(), "restart changed labels or activation");
            require(foundation.generations == 0, "restart introduced model prose generation");
        }
        fs::remove_all(dir);
        std::cout << "Multi-neuron assembly, native replies, corrected knowledge, scope, source deduplication, conflicts, deadline, feedback and restart passed.\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << " (fixture " << dir << ")\n"; return 1; }
}
