#include "wds/ai/core.hpp"
#include <fstream>
#include <iostream>
using namespace wds::ai;
int main(int argc, char **argv) {
    try {
        require(argc == 2, "package argument");
        winds::Package p(argv[1]);
        auto digest = p.digest;
        auto dir = fs::temp_directory_path() / ("wds-test-" + std::to_string(now_ms()));
        {
            chain::Store s(dir);
            roots::select(s, "test", "purpose", "t");
            json packet = {{"schema", 1},
                           {"intent", "memory.upsert"},
                           {"hops", 0},
                           {"root_id", "test"},
                           {"engram_id", "one"},
                           {"packet_id", "p1"},
                           {"expires_ms", now_ms() + 60000},
                           {"expected_version", 0},
                           {"text", "blue cat music"},
                           {"source", "unit test"},
                           {"claim_status", "observed"}};
            mesh::ingest(s, packet, "t");
            bool rejected = false;
            try {
                mesh::ingest(s, packet, "t");
            } catch (...) {
                rejected = true;
            }
            require(rejected, "duplicate accepted");
            auto q = embed("cat music");
            auto f = understanding::attend(mesh::retrieve(s, "test", q), q);
            require(f.attention.size() == 1 && f.attention[0] == 1, "softmax");
            require(mesh::retrieve(s, "other", q).empty(), "scope leak");
            auto foundation = intelligence::create("", 0);
            runtime::Engine e(s, p, *foundation);
            auto sink = [](const json &) {};
            auto id = e.command({{"schema", 1},
                                 {"type", "seed"},
                                 {"request_id", "s1"},
                                 {"root_id", "test"},
                                 {"text", "compose cat music"}},
                                sink);
            e.command({{"schema", 1}, {"type", "pause"}, {"request_id", "p"}, {"instance_id", id}}, sink);
            require(!e.step(id, sink), "paused wind ran");
            e.command({{"schema", 1}, {"type", "resume"}, {"request_id", "r"}, {"instance_id", id}}, sink);
            require(!e.step(id, sink), "did not finish");
            require(s.instances.at(id).at("status") != "failed", "bad final status");
            require(p.digest == digest, "policy mutated");
            require(s.engrams.at("test/one").at("route_score") == 0, "route mutated");
        }
        {
            chain::Store s(dir);
            require(s.roots.size() == 1 && s.engrams.size() == 2 && s.instances.size() == 1,
                    "replay mismatch");
            require(s.roots.at("test").at("relationship_level") == 2, "relationship progression lost");
        }
        // Corruption must fail closed; never silently truncate a torn event.
        {
            std::ofstream corrupt(dir / "events.jsonl", std::ios::app);
            corrupt << "{torn";
        }
        bool corrupt_rejected = false;
        try {
            chain::Store bad(dir);
        } catch (...) {
            corrupt_rejected = true;
        }
        require(corrupt_rejected, "torn chain accepted");
        fs::remove_all(dir);
        std::cout << "core tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
