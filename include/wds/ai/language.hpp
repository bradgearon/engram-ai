#pragma once
#include "wds/ai/types.hpp"
namespace wds::ai::language {
// Experimental sequence learner. Bytes are symbols, not hand-selected words.
inline constexpr const char *schema = "language.sequence/1";
inline constexpr size_t hidden = 64, vocab = 256;
struct alignas(64) Weights {
    std::array<float, vocab * hidden> embedding{};
    std::array<float, hidden * hidden> recurrent{};
    std::array<float, vocab * hidden> output{};
    std::array<float, hidden> hidden_bias{};
    std::array<float, vocab> output_bias{};
};
using Hidden = std::array<float, hidden>;
struct Model { Weights weights; std::string digest; json metadata; };
std::shared_ptr<const Model> load(const fs::path &, const json &);
std::string encode_context(const json &turns, const std::string &input);
json generate(const Model &, const std::string &context, size_t max_bytes);
int train(const json &job);
json evaluate(const json &job);
}
