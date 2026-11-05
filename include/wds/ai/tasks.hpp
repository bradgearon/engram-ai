#pragma once
#include "wds/ai/heart.hpp"
namespace wds::ai {
namespace winds {
class Package;
}
namespace tasks {
// Artifact and requirement schemas are validated before any indexing or arithmetic.
void validate(const json &task);
std::vector<std::string> actions(const std::string &domain);
std::map<std::string, double> checks(const json &task);
Embedding observe(const json &task);
json apply(const json &task, size_t action);
json assess(const json &task); // bounded oracle trajectories, never a garden write
heart::Evaluation evaluate(const json &task, const heart::EvaluationContext &context);
class DomainEvaluator final : public heart::Evaluator {
    std::string domain_;

  public:
    explicit DomainEvaluator(std::string domain) : domain_(std::move(domain)) {}
    heart::Evaluation evaluate(const std::string &, const heart::EvaluationContext &) const override;
};
struct Decision {
    size_t action;
    State state;
};
class WindPolicy {
  public:
    virtual ~WindPolicy() = default;
    virtual Decision decide(const json &, const winds::Package &) const = 0;
};
class FieldPolicy final : public WindPolicy {
  public:
    Decision decide(const json &, const winds::Package &) const override;
};
class ActionExecutor {
  public:
    virtual ~ActionExecutor() = default;
    virtual json execute(const json &, const Decision &) const = 0;
};
class DomainExecutor final : public ActionExecutor {
  public:
    json execute(const json &task, const Decision &d) const override {
        return apply(task, d.action);
    }
};
} // namespace tasks
} // namespace wds::ai
