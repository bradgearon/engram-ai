#pragma once
#include <memory>
#include <string>
#include <vector>
namespace wds::ai::intelligence {
struct PromptPart {
    std::string text;
    bool input = false;
};
// Native llama.cpp Jinja subset, parsed once per model. Input markers survive rendering.
class ChatTemplate {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit ChatTemplate(std::string source);
    ~ChatTemplate();
    std::vector<PromptPart> render(const std::string &bos, const std::string &eos, const std::string &system,
                                   const std::string &user) const;
};
} // namespace wds::ai::intelligence
