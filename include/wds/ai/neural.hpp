#pragma once
#include "wds/ai/types.hpp"

namespace wds::ai {
namespace winds { class Package; }
namespace neural {
// Application neurons are bounded decision units, not individual Qwen weight rows.
// Both feature and action schemas are versioned independently of policy.wind ABI 1.
inline constexpr const char *feature_schema = "neural.features/1";
std::vector<std::string> actions();
json default_settings();
void validate_settings(const json &);
void validate_examples(const json &);
State features(const json &pulse, const json &node, const std::vector<json> &memories);
struct Decision {
    std::string action, reason, method;
    float strength = 0, margin = 0;
    bool uncertain = false;
    State features{};
    json evidence = json::array();
    json record() const;
};
Decision decide(const State &, const winds::Package &, const json &node);
// A feature extractor returns model-bound representations. These are not factual evidence.
struct Representation {
    std::vector<float> values;
    std::string identity;
    int input_tokens = 0;
    bool cached = false;
};
float cosine(const std::vector<float> &, const std::vector<float> &);
Decision classify(const Decision &, const Representation &, const std::vector<Representation> &,
                  const json &examples, float minimum, float margin);
} // namespace neural
} // namespace wds::ai
