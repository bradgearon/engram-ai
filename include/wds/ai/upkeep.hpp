#pragma once
#include "wds/ai/types.hpp"
namespace wds::ai::upkeep {
struct Flesh {
    double hunger = 0, thirst = 0, candle = 0.05, kindling = 0, fatigue = 0;
    static Flesh read(const json &);
    json record() const;
    void need(bool knowledge, double deficit);
    void outcome(double before, double after, bool passed);
    double priority() const;
};
struct GateInput {
    bool direct = false, duplicate = false, relevant = false, local = false;
    double drive = 0;
};
struct Decision {
    std::string action, reason;
};
json default_settings();
void validate_settings(const json &);
int64_t integer(const json &, const char *, int64_t, int64_t);
} // namespace wds::ai::upkeep
