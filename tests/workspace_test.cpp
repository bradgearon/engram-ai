#include "wds/ai/core.hpp"
#include <iostream>
using namespace wds::ai;
struct Foundation final : intelligence::Foundation {
    std::string name() const override {
        return "workspace-test";
    }
    std::string propose(const intelligence::Request &r, intelligence::Token) override {
        return "A thoughtful music composition response to " + r.seed;
    }
};
int main(int argc, char **argv) {
    auto dir = fs::temp_directory_path() / ("wds-workspace-" + std::to_string(now_ms()));
    try {
        require(argc == 2, "package path required");
        winds::Package package(argv[1]);
        Foundation f;
        uint64_t final_seq = 0;
        {
            chain::Store store(dir);
            runtime::Engine engine(store, package, f, {}, {}, "wds.ai.heart.rules/1");
            int seq = 0;
            std::vector<json> events;
            auto emit = [&](const json &e) { events.push_back(e); };
            auto cmd = [&](json m) {
                m["schema"] = 1;
                if (!m.contains("request_id"))
                    m["request_id"] = "request-" + std::to_string(++seq);
                return engine.command(m, emit);
            };
            auto drain = [&] {
                for (auto id : engine.dispatch_ready(now_ms() + 100, emit))
                    while (engine.step(id, emit)) {
                    };
            };
            auto rejected = [&](json m) {
                bool bad = false;
                try {
                    cmd(m);
                } catch (const std::exception &) {
                    bad = true;
                }
                require(bad, "invalid request was accepted");
            };
            for (auto root : {"home", "other"})
                cmd({{"type", "project.create"},
                     {"project_id", root},
                     {"name", root},
                     {"purpose", "music composition"}});
            cmd({{"type", "chat.create"},
                 {"chat_id", "chat"},
                 {"project_id", "home"},
                 {"title", "New conversation"}});
            json send = {{"type", "chat.send"},
                         {"request_id", "send-once"},
                         {"chat_id", "chat"},
                         {"text", "music melody"}};
            cmd(send);
            auto sequence = store.sequence();
            cmd(send);
            require(store.sequence() == sequence, "send retry duplicated durable work");
            send["text"] = "different";
            rejected(send);
            drain();
            require(store.messages_by_chat.at("chat").size() == 2, "missing durable reply");
            require(store.chats.at("chat").at("pending_signal") == "", "completed chat stayed busy");
            const auto &reply = store.chat_messages.at(store.messages_by_chat.at("chat").back());
            require(reply.at("speaker").at("selected_soul") == "home" && reply.at("origin_project") == "home",
                    "speaker provenance missing");
            require(!reply.at("memory_sources").empty(), "response memory provenance missing");
            auto pages = store.provenance("home", store.sequence() + 1, 2);
            require(pages.at("events").size() == 2 && !pages.at("next_before").is_null(),
                    "provenance pagination failed");
            auto next = store.provenance("home", pages.at("next_before"), 2);
            require(next.at("events").front().at("seq") < pages.at("events").back().at("seq"),
                    "provenance pages overlap");
            rejected({{"type", "chat.send"},
                      {"chat_id", "chat"},
                      {"text", "music guest"},
                      {"source_root", "other"}});
            cmd({{"type", "chat.update"},
                 {"chat_id", "chat"},
                 {"expected_version", store.chats.at("chat").at("version")},
                 {"guests", json::array({"other"})}});
            cmd({{"type", "chat.send"},
                 {"chat_id", "chat"},
                 {"text", "music guest"},
                 {"source_root", "other"}});
            drain();
            require(store.chat_messages.at(store.messages_by_chat.at("chat").back()).at("origin_project") ==
                        "other",
                    "guest attribution lost");
            cmd({{"type", "upkeep.configure"},
                 {"expected_version", 0},
                 {"settings",
                  {{"enabled", true}, {"max_steps", 1}, {"max_attempts", 1}, {"review_delay_ms", 10}}}});
            cmd({{"type", "chat.review"},
                 {"chat_id", "chat"},
                 {"text", "music: reflect on the melody"},
                 {"due_in_ms", 0}});
            drain();
            require(store.chat_messages.at(store.messages_by_chat.at("chat").back()).at("kind") ==
                        "contribution",
                    "offline follow-up was not retained");
            auto size = store.messages_by_chat.at("chat").size();
            cmd({{"type", "chat.review"},
                 {"chat_id", "chat"},
                 {"text", "music: another thought"},
                 {"due_in_ms", 0}});
            drain();
            require(store.messages_by_chat.at("chat").size() == size,
                    "quiet contribution interval was ignored");
            require(store.active_packages.empty(), "conversation activated a package");
            cmd({{"type", "chat.update"},
                 {"chat_id", "chat"},
                 {"expected_version", store.chats.at("chat").at("version")},
                 {"project_id", "other"}});
            require(store.chats.at("chat").at("last_instance") == "", "move retained foreign continuation");
            require(store.chat_messages.at(store.messages_by_chat.at("chat").front()).at("origin_project") ==
                        "home",
                    "move rewrote message origin");
            final_seq = store.sequence();
        }
        {
            chain::Store store(dir);
            require(store.sequence() == final_seq, "restart changed journal");
            require(store.projects.size() == 2 && store.chats.size() == 1 && store.chat_messages.size() >= 5,
                    "restart lost workspace");
            auto page = store.provenance("home", store.sequence() + 1, 20);
            require(!page.at("events").empty(), "restart lost provenance index");
            for (auto event : page.at("events")) {
                auto hash = event.at("hash").get<std::string>();
                event.erase("hash");
                require(checksum(event.dump()) == hash, "provenance not original journal bytes");
            }
        }
        fs::remove_all(dir);
        std::cout << "workspace chat, idempotency, scopes, offline contributions, quiet gate, provenance and "
                     "restart passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        fs::remove_all(dir);
        return 1;
    }
}
