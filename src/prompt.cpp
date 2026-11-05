#include "wds/ai/prompt.hpp"
#include "jinja/lexer.h"
#include "jinja/parser.h"
#include "jinja/runtime.h"
#include "json.h"
#include <stdexcept>
namespace wds::ai::intelligence {
struct ChatTemplate::Impl {
    std::string source;
    jinja::program ast;
    explicit Impl(std::string text) : source(std::move(text)) {
        if (source.empty() || source.size() > 262144)
            throw std::runtime_error("missing or oversized model chat template");
        jinja::lexer lexer;
        ast = jinja::parse_from_tokens(lexer.tokenize(source));
    }
};
ChatTemplate::ChatTemplate(std::string source) : impl_(std::make_unique<Impl>(std::move(source))) {}
ChatTemplate::~ChatTemplate() = default;
std::vector<PromptPart> ChatTemplate::render(const std::string &bos, const std::string &eos,
                                             const std::string &system, const std::string &user) const {
    jinja::context context(impl_->source);
    jinja::global_from_json(context,
                            common_json{{"bos_token", bos},
                                        {"eos_token", eos},
                                        {"add_generation_prompt", true},
                                        {"enable_thinking", false},
                                        {"tools", common_json::array()}},
                            false);
    common_json messages = common_json::array();
    messages.push_back({{"role", "system"}, {"content", system}});
    messages.push_back({{"role", "user"}, {"content", user}});
    jinja::global_from_json(context, common_json{{"messages", messages}}, true);
    jinja::runtime runtime(context);
    auto rendered = runtime.gather_string_parts(runtime.execute(impl_->ast))->as_string();
    std::vector<PromptPart> out;
    size_t bytes = 0;
    for (const auto &part : rendered.parts) {
        bytes += part.val.size();
        if (bytes > 65536)
            throw std::runtime_error("rendered prompt exceeds 65536-byte context budget");
        if (!part.val.empty())
            out.push_back({part.val, part.is_input});
    }
    return out;
}
} // namespace wds::ai::intelligence
