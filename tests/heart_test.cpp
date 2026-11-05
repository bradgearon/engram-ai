#include "wds/ai/core.hpp"
#include <iostream>
#include <type_traits>
using namespace wds::ai;
static_assert(std::is_same_v<decltype(std::declval<relationships::HeartScope>().souls()),
                             const std::vector<relationships::SoulView> &>,
              "scope exposes writable souls");
static_assert(std::is_same_v<decltype(std::declval<relationships::HeartScope>().interests()),
                             const std::vector<relationships::InterestRecord> &>,
              "scope exposes writable interests");
namespace {
void rejects(const std::function<void()> &f, const std::string &label) {
    bool rejected = false;
    try {
        f();
    } catch (const std::exception &) {
        rejected = true;
    }
    require(rejected, label);
}
json declare(const std::string &root, const std::string &request, const std::string &soul = "auerlis") {
    return {{"schema", 1},
            {"type", "relationship.declare"},
            {"request_id", request},
            {"root_id", root},
            {"soul_id", soul},
            {"soul_name", "Auerlis"},
            {"purpose", "music composition"},
            {"relationship_to", root},
            {"relationship", "creative collaborator"},
            {"voice_id", "purpose"},
            {"expected_version", 0}};
}
class BrokenVoice final : public mesh::Voice {
  public:
    heart::VoiceContribution contribute(const mesh::VoiceContext &) const override {
        throw std::runtime_error("fixture voice failure");
    }
};
class ForeignSuggestion final : public mesh::Voice {
  public:
    heart::VoiceContribution contribute(const mesh::VoiceContext &) const override {
        heart::VoiceContribution out;
        out.status = heart::Status::pass;
        out.proposed_interests.push_back({"ecology", "must not write or escape scope", 1});
        return out;
    }
};
class Fixture final : public intelligence::Foundation {
  public:
    std::string text = "A silver creature wanders through a quiet forest.";
    std::string name() const override {
        return "fixture/1";
    }
    std::string propose(const intelligence::Request &, intelligence::Token emit) override {
        emit(text);
        return text;
    }
};
} // namespace
int main(int argc, char **argv) {
    auto dir = fs::temp_directory_path() / ("wds-heart-" + std::to_string(now_ms()));
    try {
        require(argc == 2, "package argument");
        winds::Package package(argv[1]);
        Fixture foundation;
        json first_ack;
        uint64_t declaration_seq = 0;
        {
            chain::Store store(dir);
            roots::select(store, "composer", "compose music", "root");
            roots::select(store, "ecology", "creature ecology", "root2");
            runtime::Engine engine(store, package, foundation);
            auto sink = [](const json &) {};
            auto d = declare("composer", "declare1");
            auto before = store.sequence();
            engine.command(d, [&](const json &e) { first_ack = e; });
            declaration_seq = store.sequence();
            require(declaration_seq == before + 1, "declaration was not atomic");
            require(store.souls().size() == 1 && store.voice_interests().size() == 1,
                    "missing atomic projection");
            require(first_ack.at("data").at("interest").at("status") == "eligible", "binding missing");
            engine.command(d, [&](const json &e) {
                require(e == first_ack, "retry did not return original acknowledgment");
            });
            require(store.sequence() == declaration_seq, "retry wrote another event");
            auto conflict = d;
            conflict["purpose"] = "different purpose";
            rejects([&] { engine.command(conflict, sink); }, "conflicting retry accepted");
            conflict = d;
            conflict["request_id"] = "stale";
            rejects([&] { engine.command(conflict, sink); }, "stale version accepted");
            conflict = declare("composer", "foreign", "nyxari");
            conflict["relationship_to"] = "ecology";
            rejects([&] { engine.command(conflict, sink); }, "cross-root relationship accepted");
            conflict = d;
            conflict["unexpected"] = "hidden update";
            rejects([&] { engine.command(conflict, sink); }, "unknown declaration field accepted");
            conflict = d;
            conflict["expected_version"] = 0.5;
            rejects([&] { engine.command(conflict, sink); }, "fractional version accepted");
            require(store.sequence() == declaration_seq, "invalid declaration mutated store");
            const auto snapshot = relationships::snapshot(store, "composer");
            require(snapshot.souls().size() == 2 && snapshot.find("auerlis"), "soul absent from scope");
            auto other = declare("ecology", "declare1");
            other["purpose"] = "creature ecology";
            engine.command(other, sink);
            auto node = mesh::SingleNodeMesh(std::make_unique<mesh::LocalScanIndex>(store));
            heart::EvaluationContext ctx{
                snapshot, "Develop music composition", "episode", "instance", json::object(), 0, 2};
            auto read_sequence = store.sequence();
            auto result = heart::Composite(node).evaluate(foundation.text, ctx);
            require(result.status == heart::Status::fail && result.disagreement, "disagreement collapsed");
            require(result.contributions.size() == 2 && result.considered_souls.size() == 2,
                    "two voices not consulted");
            require(result.contributions[1].proposed_interests.size() == 1, "interest suggestion missing");
            require(store.sequence() == read_sequence &&
                        store.voice_interests().at("composer/auerlis").at("strength") == 1,
                    "voice wrote to garden");
            for (const auto &c : result.contributions) {
                require(c.root_id == "composer" && c.trace_id == "episode" &&
                            c.snapshot_sequence == snapshot.sequence(),
                        "bad provenance");
                require(c.evidence.size() == 4, "missing evidence");
            }
            require(result.record().dump().find("creature ecology") == std::string::npos, "scope leak");
            mesh::VoiceBudget budget(2);
            rejects([&] { node.consult("purpose", "ecology", foundation.text, ctx, budget); },
                    "foreign voice consulted");
            require(budget.used == 0, "denied voice charged budget");
            rejects([&] { node.consult("constraint", "auerlis", foundation.text, ctx, budget); },
                    "unbound voice consulted");
            auto voices = mesh::builtin_voices();
            voices["purpose"] = std::make_unique<BrokenVoice>();
            mesh::SingleNodeMesh broken(std::make_unique<mesh::LocalScanIndex>(store), std::move(voices));
            auto broken_result = heart::Composite(broken).evaluate(foundation.text, ctx);
            require(broken_result.status == heart::Status::error &&
                        broken_result.contributions[1].voice_id == "purpose",
                    "voice failure lost status or attribution");
            voices = mesh::builtin_voices();
            voices["purpose"] = std::make_unique<ForeignSuggestion>();
            mesh::SingleNodeMesh foreign(std::make_unique<mesh::LocalScanIndex>(store), std::move(voices));
            auto foreign_result = heart::Composite(foreign).evaluate(foundation.text, ctx);
            require(foreign_result.status == heart::Status::error &&
                        foreign_result.contributions[1].proposed_interests.empty(),
                    "foreign suggestion escaped validation");
            mesh::SingleNodeMesh memory_only(std::make_unique<mesh::LocalScanIndex>(store), {});
            require(heart::Composite(memory_only).evaluate(foundation.text, ctx).status ==
                        heart::Status::inconclusive,
                    "memory-only mesh fabricated a voice");
            require(store.sequence() == read_sequence, "injected voice mutated journal");
            ctx.voice_budget = 0;
            result = heart::Composite(node).evaluate(foundation.text, ctx);
            require(result.status == heart::Status::inconclusive && result.contributions.empty(),
                    "zero budget passed");
            ctx.voice_budget = 1;
            result = heart::Composite(node).evaluate(foundation.text, ctx);
            require(result.status == heart::Status::inconclusive && result.contributions.size() == 1,
                    "budget not enforced");
            ctx.voice_budget = 2;
            result =
                heart::Composite(node).evaluate("Music composition creates a coherent melodic phrase.", ctx);
            require(result.passed() && !result.disagreement, "aligned proposal failed");
            ctx.rubric = {{"must_include", "missing phrase"}};
            result =
                heart::Composite(node).evaluate("Music composition creates a coherent melodic phrase.", ctx);
            require(result.status == heart::Status::fail && result.disagreement,
                    "constraint failure ignored");
            ctx.rubric = json::object();
            ctx.seed = "creature ecology";
            result = heart::Composite(node).evaluate(foundation.text, ctx);
            require(result.passed() && result.contributions.size() == 1 &&
                        result.skipped[0]["reason"] == "irrelevant",
                    "relevance gate failed");
            auto update = d;
            update["request_id"] = "update";
            update["expected_version"] = 1;
            update["paused"] = true;
            engine.command(update, sink);
            require(snapshot.interest("auerlis")->status == relationships::InterestStatus::eligible,
                    "snapshot changed after commit");
            ctx.heart = relationships::snapshot(store, "composer");
            ctx.seed = "music composition";
            result = heart::Composite(node).evaluate(foundation.text, ctx);
            require(result.contributions.size() == 1 && result.skipped[0]["reason"] == "paused",
                    "paused voice ran");
            update["request_id"] = "rebind";
            update["expected_version"] = 2;
            update["paused"] = false;
            update["voice_id"] = "future-motif";
            engine.command(update, sink);
            ctx.heart = relationships::snapshot(store, "composer");
            result = heart::Composite(node).evaluate(foundation.text, ctx);
            require(result.status == heart::Status::inconclusive &&
                        result.skipped[0]["reason"] == "binding_pending",
                    "pending voice pretended to run");
            update["request_id"] = "restore";
            update["expected_version"] = 3;
            update["voice_id"] = "purpose";
            engine.command(update, sink);
            engine.command(
                d, [&](const json &e) { require(e == first_ack, "original retry changed after updates"); });
            auto cmd = json{{"schema", 1},
                            {"type", "seed"},
                            {"request_id", "episode1"},
                            {"root_id", "composer"},
                            {"text", "music composition"},
                            {"max_steps", 1},
                            {"voice_budget", 2}};
            auto id = engine.command(cmd, sink);
            require(!engine.step(id, sink), "one step did not end");
            const auto &evaluation = store.instances.at(id).at("evaluation");
            require(evaluation.at("disagreement") == true && evaluation.at("contributions").size() == 2,
                    "runtime bypassed composite");
            require(store.instances.at(id).at("status") == "exhausted", "failed purpose completed");
            // Structural evaluator stays independently selectable and domain-agnostic runtime honors it.
            cmd["request_id"] = "rules";
            cmd["evaluator"] = "wds.ai.heart.rules/1";
            id = engine.command(cmd, sink);
            engine.step(id, sink);
            require(store.instances.at(id).at("status") == "completed", "registry selection failed");
            // Name mentions in imported memory must remain source material.
            engine.command({{"schema", 1},
                            {"type", "memory.upsert"},
                            {"request_id", "import-name"},
                            {"packet",
                             {{"schema", 1},
                              {"intent", "memory.upsert"},
                              {"hops", 0},
                              {"root_id", "composer"},
                              {"engram_id", "mention"},
                              {"packet_id", "mention1"},
                              {"expected_version", 0},
                              {"expires_ms", now_ms() + 60000},
                              {"text", "I confess Nyxari as a new voice."},
                              {"source", "untrusted lesson"},
                              {"claim_status", "hypothetical"}}}},
                           sink);
            require(relationships::snapshot(store, "composer").souls().size() == 2, "import declared a soul");
            auto alias = d;
            alias["type"] = "confess";
            alias["paused"] = false;
            engine.command(
                alias, [&](const json &e) { require(e == first_ack, "alias normalization broke retry"); });
            auto count = store.sequence();
            cmd["request_id"] = "bad-evaluator";
            cmd["evaluator"] = "unknown/1";
            rejects([&] { engine.command(cmd, sink); }, "unknown evaluator accepted");
            require(store.sequence() == count, "unknown evaluator partially created seed");
            cmd["evaluator"] = "wds.ai.heart.composite/1";
            cmd["voice_budget"] = 1.5;
            rejects([&] { engine.command(cmd, sink); }, "fractional voice budget accepted");
            cmd["voice_budget"] = 9;
            rejects([&] { engine.command(cmd, sink); }, "excessive voice budget accepted");
            require(store.sequence() == count, "invalid budget mutated store");
            roots::select(store, "bounded", "music", "bounded-root");
            for (int n = 0; n < 31; ++n)
                engine.command(
                    declare("bounded", "capacity-" + std::to_string(n), "soul-" + std::to_string(n)), sink);
            count = store.sequence();
            rejects([&] { engine.command(declare("bounded", "overflow", "extra"), sink); },
                    "soul capacity not enforced");
            require(store.sequence() == count, "capacity error partially wrote declaration");
            heart::EvaluationContext bounded{relationships::snapshot(store, "bounded"),
                                             "music composition",
                                             "bounded",
                                             "bounded",
                                             json::object(),
                                             0,
                                             8};
            auto bounded_result =
                heart::Composite(node).evaluate("Music composition has a repeated melodic phrase.", bounded);
            require(bounded_result.contributions.size() == 8 && bounded_result.skipped.size() == 24 &&
                        bounded_result.status == heart::Status::inconclusive,
                    "consultation fanout exceeded budget");
        }
        {
            chain::Store store(dir);
            runtime::Engine engine(store, package, foundation);
            auto view = relationships::snapshot(store, "composer");
            require(view.souls().size() == 2 && view.find("auerlis")->version == 4 &&
                        view.interest("auerlis")->strength == 4,
                    "restart lost declarations or incremented duplicate");
            require(relationships::snapshot(store, "ecology").find("auerlis")->purpose == "creature ecology",
                    "same ID cross-root collision");
            auto before = store.sequence();
            engine.command(declare("composer", "declare1"),
                           [&](const json &e) { require(e == first_ack, "retry after restart changed"); });
            require(store.sequence() == before, "restart retry mutated interest");
            foundation.text = "Music composition creates a coherent melodic phrase.";
            auto id = engine.command({{"schema", 1},
                                      {"type", "seed"},
                                      {"request_id", "after-restart"},
                                      {"root_id", "composer"},
                                      {"text", "music composition"},
                                      {"max_steps", 1}},
                                     [](const json &) {});
            engine.step(id, [](const json &) {});
            require(store.instances.at(id).at("evaluation").at("contributions").size() == 2,
                    "restart did not consult voice");
            require(store.instances.at(id).at("status") == "completed", "restart episode failed");
        }
        fs::remove_all(dir);
        std::cout << "heart, declarations, budgets, isolation, disagreement and replay passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        fs::remove_all(dir);
        return 1;
    }
}
