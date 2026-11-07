#include "wds/ai/core.hpp"
#include <iostream>
using namespace wds::ai;
namespace {
class Counting final : public intelligence::Foundation {
  public:
    int calls = 0;
    bool limited = false;
    intelligence::Request last;
    std::string name() const override {
        return "counted-test/1";
    }
    std::string propose(const intelligence::Request &r, intelligence::Token) override {
        ++calls;
        last = r;
        if (limited)
            throw intelligence::GenerationLimit("token budget reached; incomplete",
                                                std::string("partial ") + char(0xe2));
        std::string out = "A concrete music composition proposal for " + r.seed;
        for (const auto &m : r.memories)
            out += " " + m.at("text").get<std::string>();
        return out;
    }
};
void rejects(std::function<void()> fn, const std::string &label) {
    bool bad = false;
    try {
        fn();
    } catch (const std::exception &) {
        bad = true;
    }
    require(bad, label);
}
} // namespace
int main(int argc, char **argv) {
    auto dir = fs::temp_directory_path() / ("wds-upkeep-" + std::to_string(now_ms()));
    try {
        require(argc == 2, "package argument");
        winds::Package package(argv[1]);
        std::string review_key, idea_key;
        uint64_t sequence = 0;
        {
            chain::Store store(dir);
            Counting f;
            runtime::Engine e(store, package, f, {}, {}, "wds.ai.heart.rules/1");
            int number = 0;
            json last;
            std::vector<json> events;
            auto sink = [&](const json &j) {
                last = j;
                events.push_back(j);
            };
            auto cmd = [&](json j) {
                j["schema"] = 1;
                if (!j.contains("request_id"))
                    j["request_id"] = "test-" + std::to_string(++number);
                return e.command(j, sink);
            };
            auto drain = [&] {
                auto ids = e.dispatch_ready(now_ms(), sink);
                for (const auto &id : ids)
                    while (e.step(id, sink)) {
                    };
                std::erase_if(ids, [&](const auto &id) { return store.instances.at(id).value("mode", std::string{}) == "neural"; });
                return ids;
            };
            for (auto root : {"music", "account", "private"})
                cmd({{"type", "root.select"}, {"root_id", root}, {"purpose", "music composition"}});
            cmd({{"type", "scope.bind"},
                 {"root_id", "account"},
                 {"kind", "account"},
                 {"expected_version", 0}});
            cmd({{"type", "scope.bind"},
                 {"root_id", "music"},
                 {"kind", "project"},
                 {"account_root", "account"},
                 {"expected_version", 0}});
            auto memory = [&](std::string root, std::string id, std::string text) {
                cmd({{"type", "memory.upsert"},
                     {"packet",
                      {{"schema", 1},
                       {"intent", "memory.upsert"},
                       {"root_id", root},
                       {"engram_id", id},
                       {"packet_id", root + "-" + id},
                       {"expected_version", 0},
                       {"hops", 0},
                       {"expires_ms", now_ms() + 60000},
                       {"text", text},
                       {"source", "test"},
                       {"claim_status", "observed"}}}});
            };
            memory("account", "shared", "music shared account memory");
            memory("private", "secret", "music PRIVATE_SECRET");
            cmd({{"type", "relationship.declare"},
                 {"root_id", "music"},
                 {"soul_id", "bird"},
                 {"soul_name", "Bird"},
                 {"purpose", "melody singing"},
                 {"relationship_to", "music"},
                 {"relationship", "companion"},
                 {"voice_id", "purpose"},
                 {"expected_version", 0}});
            cmd({{"type", "speaker.profile"},
                 {"root_id", "music"},
                 {"soul_id", "music"},
                 {"expected_version", 0},
                 {"form", "organization"},
                 {"presence", {"warm", "motherly"}},
                 {"interests", {"music", "stories"}}});
            cmd({{"type", "speaker.profile"},
                 {"root_id", "music"},
                 {"soul_id", "bird"},
                 {"expected_version", 0},
                 {"form", "bird"},
                 {"gender_type", "neutral"},
                 {"presence", {"curious", "sisterly"}}});
            cmd({{"type", "persona.preferences"},
                 {"root_id", "account"},
                 {"expected_version", 0},
                 {"preferences", {{"tone", "formal"}, {"familiarity", "close"}}}});
            cmd({{"type", "persona.preferences"},
                 {"root_id", "music"},
                 {"expected_version", 0},
                 {"preferences", {{"tone", "playful"}}}});
            cmd({{"type", "route.resolve"}, {"root_id", "music"}, {"text", "melody singing"}});
            require(last.at("data").at("selected_soul") == "bird", "automatic soul routing failed");
            require(last["data"]["profile"]["preferences"]["tone"] == "playful" &&
                        last["data"]["profile"]["preferences"]["familiarity"] == "close",
                    "project/account preference precedence");
            json sig = {{"type", "neuron.signal"},
                        {"root_id", "music"},
                        {"soul_id", "music"},
                        {"text", "music composition"},
                        {"max_steps", 1}};
            cmd(sig);
            auto first = drain();
            require(first.size() == 1 && f.calls == 0, "first signal must run exactly once");
            require(store.instances.at(first[0]).at("memory_sources").size() == 1 && store.instances.at(first[0]).at("memory_sources")[0]["engram_id"] == "account/shared",
                    "shared grant or isolation failed");
            cmd(sig);
            require(last["data"]["decision"] == "reuse", "completed duplicate failed cache");
            require(drain().empty() && f.calls == 0, "duplicate caused compute");
            auto irrelevant = sig;
            irrelevant["text"] = "unrelated telescope galaxies";
            irrelevant["kind"] = "evidence";
            cmd(irrelevant);
            require(last["data"]["decision"] == "hold", "irrelevant evidence woke neuron");
            // Forced requests queue; unforced duplicates are collapsed again at dispatch after completion.
            sig["text"] = "music composition changes";
            cmd(sig);
            cmd(sig);
            auto a = drain();
            require(a.size() == 1, "per-neuron concurrency exceeded");
            require(drain().empty() && f.calls == 0, "queued duplicate ran");
            json reflection = {{"type", "reflection.record"},
                               {"request_id", "human-feedback"},
                               {"root_id", "music"},
                               {"actor_soul_id", "music"},
                               {"actor_kind", "human"},
                               {"soul_id", "music"},
                               {"text", "music composition needs warmer endings"},
                               {"instance_id", first[0]}};
            cmd(reflection);
            auto ack = *std::find_if(events.rbegin(), events.rend(), [](const json &e) { return e.at("type") == "reflection.recorded"; });
            auto seq = store.sequence();
            cmd(reflection);
            require(last == ack && store.sequence() == seq, "reflection idempotence failed");
            auto ref = ack["data"]["engram_id"].get<std::string>();
            require(store.engrams.at(ref)["attribution"]["actor_kind"] == "human", "author attribution lost");
            cmd(sig);
            require(drain().size() == 1 && f.calls == 0, "reflection did not invalidate response cache");
            require(store.engrams.count(ref)==1, "reflection not retained");
            require(store.soul_atoms.at("music/root/music").weight > 0.5f,
                    "attributed attention did not reinforce response");
            memory("music", "idea", "melody singing ideas");
            cmd({{"type", "engram.suggest"},
                 {"root_id", "music"},
                 {"engram_id", "idea"},
                 {"speaker_soul_id", "bird"},
                 {"topic", "melody singing"},
                 {"reason", "bird perspective fits the melody"}});
            idea_key = last["data"]["idea"]["key"];
            cmd({{"type", "route.resolve"},
                 {"root_id", "music"},
                 {"soul_id", "music"},
                 {"text", "melody singing"}});
            require(last["data"]["selected_soul"] == "bird", "engram cannot override speaker");
            cmd({{"type", "engram.defer"},
                 {"root_id", "music"},
                 {"idea_key", idea_key},
                 {"expected_version", 1},
                 {"due_in_ms", 10000}});
            cmd({{"type", "route.resolve"},
                 {"root_id", "music"},
                 {"soul_id", "music"},
                 {"text", "melody singing"}});
            require(last["data"]["selected_soul"] == "music", "deferred idea interrupted");
            rejects(
                [&] {
                    cmd({{"type", "route.resolve"},
                         {"root_id", "private"},
                         {"soul_id", "bird"},
                         {"text", "melody"}});
                },
                "foreign speaker crossed project scope");
            rejects(
                [&] {
                    cmd({{"type", "reflection.record"},
                         {"root_id", "music"},
                         {"actor_root", "private"},
                         {"actor_soul_id", "private"},
                         {"actor_kind", "human"},
                         {"text", "secret"}});
                },
                "foreign author leak");
            // A renamed declared speaker invalidates cached presentation even when purpose is unchanged.
            json bird_signal = {{"type", "neuron.signal"},
                                {"root_id", "music"},
                                {"soul_id", "bird"},
                                {"text", "melody singing"},
                                {"max_steps", 1}};
            cmd(bird_signal);
            require(drain().size() == 1, "bird did not respond");
            cmd(bird_signal);
            require(last["data"]["decision"] == "reuse", "bird cache missing");
            cmd({{"type", "relationship.declare"},
                 {"root_id", "music"},
                 {"soul_id", "bird"},
                 {"soul_name", "Songbird"},
                 {"purpose", "melody singing"},
                 {"relationship_to", "music"},
                 {"relationship", "companion"},
                 {"voice_id", "purpose"},
                 {"expected_version", 1}});
            cmd(bird_signal);
            require(drain().size() == 1, "renamed speaker reused stale context");
            cmd({{"type", "speaker.profile"},
                 {"root_id", "music"},
                 {"soul_id", "music"},
                 {"expected_version", 1},
                 {"gender_type", "woman"}});
            cmd({{"type", "relationship.declare"},
                 {"root_id", "music"},
                 {"soul_id", "stone"},
                 {"soul_name", "Stone"},
                 {"purpose", "quiet geology"},
                 {"relationship_to", "music"},
                 {"relationship", "companion"},
                 {"voice_id", "purpose"},
                 {"expected_version", 0}});
            auto scope = relationships::snapshot(store, "music");
            require(scope.find("stone")->expression.at("gender_type") == "unspecified",
                    "gender inferred from root defaults");
            // Token exhaustion preserves valid UTF-8 partial text and fails visibly.
            f.limited = true;
            auto failed = cmd(
                {{"type", "seed"}, {"mode", "foundation_once"}, {"root_id", "music"}, {"text", "music partial output"}, {"max_steps", 1}});
            e.step(failed, sink);
            f.limited = false;
            require(store.instances.at(failed)["status"] == "failed" &&
                        store.instances.at(failed)["output_complete"] == false,
                    "limit silently passed");
            require(store.instances.at(failed)["partial_artifact"].get<std::string>().find("partial") == 0,
                    "partial output lost");
            cmd({{"type", "upkeep.configure"},
                 {"expected_version", 0},
                 {"settings",
                  {{"enabled", true},
                   {"max_concurrent_upkeep", 2},
                   {"max_episodes", 2},
                   {"max_steps", 1},
                   {"max_foundation_calls", 2},
                   {"review_delay_ms", 10},
                   {"upkeep_refractory_ms", 10}}}});
            for (auto root : {"music", "account", "private"})
                cmd({{"type", "review.suggest"},
                     {"root_id", root},
                     {"text", "music composition review"},
                     {"due_in_ms", 0}});
            auto ready = e.dispatch_ready(now_ms() + 1, sink);
            require(ready.size() == 2, "shared concurrent upkeep bound failed");
            require(store.upkeep_usage["episodes"] == 2 && store.upkeep_usage["foundation_calls"] == 2,
                    "upfront reservation failed");
            require(e.dispatch_ready(now_ms() + 1, sink).empty(), "running reviews multiplied work");
            for (const auto &id : ready)
                while (e.step(id, sink)) {
                }
            e.dispatch_ready(now_ms() + 1, sink);
            require(e.dispatch_ready(now_ms() + 1, sink).empty(), "budget exhausted but work ran");
            bool deferred = false;
            for (const auto &kv : store.reviews)
                if (kv.second["reason"] == "shared upkeep budget exhausted")
                    deferred = true;
            require(deferred, "budget has no observable deferral reason");
            // Local import budget rejection is atomic; worker failures remain inspectable.
            cmd({{"type", "upkeep.configure"},
                 {"expected_version", 1},
                 {"settings",
                  {{"sources", {{{"url", "https://example.org/material"}, {"topic", "music"}}}}}}});
            cmd({{"type", "review.suggest"},
                 {"root_id", "music"},
                 {"text", "music source review"},
                 {"requires_material", true},
                 {"due_in_ms", 0}});
            review_key = last["data"]["review_key"];
            e.dispatch_ready(now_ms() + 2, sink);
            require(store.materials.size() == 1, "material ticket not created");
            auto material = store.materials.begin()->first;
            cmd({{"type", "material.claim"},
                 {"root_id", "music"},
                 {"material_key", material},
                 {"request_id", "claim"}});
            json many = json::array();
            for (int i = 0; i < 5; ++i)
                many.push_back("music chunk");
            auto memory_count = store.engrams.size();
            rejects(
                [&] {
                    cmd({{"type", "material.complete"},
                         {"root_id", "music"},
                         {"material_key", material},
                         {"claim_id", "claim"},
                         {"chunks", many},
                         {"content_sha256", std::string(64, 'a')}});
                },
                "RNA cap ignored");
            require(store.engrams.size() == memory_count, "partial RNA batch committed");
            cmd({{"type", "material.complete"},
                 {"root_id", "music"},
                 {"material_key", material},
                 {"claim_id", "claim"},
                 {"error", "fixture fetch unavailable"}});
            require(store.reviews.at(review_key)["status"] == "resting", "failed fetch spun");
            require(store.active_packages.empty(), "upkeep activated a package");
            require(store.soul_atoms.count("music/artifact/" + first[0]) == 1 && sizeof(soul::Atom) == 32,
                    "AI item identity missing");
            cmd({{"type", "learning.recall"}, {"root_id", "music"}, {"text", "music"}});
            require(!last["data"].empty(), "history recall empty");
            sequence = store.sequence();
        }
        {
            chain::Store store(dir);
            require(store.sequence() == sequence, "restart sequence changed");
            require(store.reviews.at(review_key)["status"] == "resting" &&
                        store.ideas.at(idea_key)["status"] == "pending",
                    "restart lost deferred work");
            require(store.profiles.at("music/bird")["form"] == "bird" &&
                        store.soul_atoms.at("music/root/music").weight > 0.5f,
                    "restart lost expression/reinforcement");
            require(store.upkeep_usage["episodes"] == 2 && store.active_packages.empty(),
                    "restart reset budget or activated policy");
        }
        fs::remove_all(dir);
        std::cout
            << "upkeep, cache, scope, expression, reflection, limits, reinforcement and restart passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        fs::remove_all(dir);
        return 1;
    }
}
