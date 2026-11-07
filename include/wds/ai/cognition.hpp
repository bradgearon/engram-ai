#pragma once
#include "wds/ai/neural.hpp"
namespace wds::ai::winds { class Package; }
namespace wds::ai::cognition {
inline constexpr const char *language_schema = "language.projection/1";
inline constexpr const char *grammar = "wds.ai.winds.language.grammar/1";
std::vector<std::string> language_actions();
json empty_state(const std::string &root);
json parse(const std::string &text);
State project(const neural::Representation &);
// Pure, bounded proposal over a coordinator-owned snapshot. No writer/scheduler access.
using Encode = std::function<neural::Representation(const std::string &)>;
json process(const json &snapshot, const json &operation, const std::string &source,
             const Encode &encode = {}, const winds::Package *language = nullptr);
}
