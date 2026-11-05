#pragma once
#include "wds/ai/types.hpp"

namespace wds::ai {
namespace chain {
class Store;
}
namespace relationships {
constexpr size_t max_souls_per_root = 32, max_souls = 4096, max_declarations = 100000;
enum class InterestStatus { pending, eligible, paused };
std::string name(InterestStatus);

struct RelationshipDeclaration {
    std::string root_id, soul_id, soul_name, purpose, relationship_to, relationship, voice_id;
    int expected_version = 0;
    bool paused = false;
    static RelationshipDeclaration parse(const json &);
    json record() const;
};
struct SoulView {
    std::string id, name, purpose, relationship_to, relationship;
    int version = 0;
    uint64_t declaration_sequence = 0;
    json expression = {{"gender_type", "unspecified"},
                       {"form", "unspecified"},
                       {"presence", json::array()},
                       {"interests", json::array()}};
};
struct InterestRecord {
    std::string soul_id, voice_id;
    InterestStatus status = InterestStatus::pending;
    int strength = 0, version = 0;
    uint64_t declaration_sequence = 0;
};

// An immutable value snapshot, never a second garden or a handle to the writer.
class HeartScope {
    std::string root_id_;
    uint64_t sequence_;
    std::vector<SoulView> souls_;
    std::vector<InterestRecord> interests_;
    HeartScope(std::string, uint64_t, std::vector<SoulView>, std::vector<InterestRecord>);
    friend HeartScope snapshot(const chain::Store &, const std::string &);

  public:
    const std::string &root_id() const {
        return root_id_;
    }
    uint64_t sequence() const {
        return sequence_;
    }
    const std::vector<SoulView> &souls() const {
        return souls_;
    }
    const std::vector<InterestRecord> &interests() const {
        return interests_;
    }
    const SoulView *find(const std::string &) const;
    const InterestRecord *interest(const std::string &) const;
    json record() const;
};
HeartScope snapshot(const chain::Store &, const std::string &root);
// Validate and prepare one atomic journal payload. Only the coordinator appends it.
json prepare(const chain::Store &, const RelationshipDeclaration &, const std::string &trace,
             bool voice_available);
} // namespace relationships
} // namespace wds::ai
