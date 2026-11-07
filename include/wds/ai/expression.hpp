#pragma once
#include "wds/ai/types.hpp"

namespace wds::ai {
namespace winds { class Package; }
namespace expression {
inline constexpr const char *feature_schema = "expression.features/1";
std::vector<std::string> actions();
std::string intent(const std::string &);
State features(const json &assembly);
// Pure planning/realization over immutable, attributed contributions. No writer or model handle.
json compose(const json &assembly, const winds::Package &package);
std::vector<json> evidence(const std::vector<json> &, const std::string &query, bool direct);
} // namespace expression
} // namespace wds::ai
