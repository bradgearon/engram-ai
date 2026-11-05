#pragma once
#include "wds/ai/mesh.hpp"

namespace wds::ai {
namespace heart {
class Evaluator {
  public:
    virtual ~Evaluator() = default;
    virtual Evaluation evaluate(const std::string &, const EvaluationContext &) const = 0;
};
class Rules final : public Evaluator {
  public:
    Evaluation evaluate(const std::string &, const EvaluationContext &) const override;
};
class Registry {
    std::map<std::string, std::function<std::unique_ptr<Evaluator>()>> factories_;

  public:
    void add(const std::string &id, std::function<std::unique_ptr<Evaluator>()> factory);
    bool contains(const std::string &id) const {
        return factories_.count(id) != 0;
    }
    bool empty() const {
        return factories_.empty();
    }
    std::unique_ptr<Evaluator> create(const std::string &) const;
};
class Composite final : public Evaluator {
    const mesh::MeshNode &mesh_;

  public:
    explicit Composite(const mesh::MeshNode &m) : mesh_(m) {}
    Evaluation evaluate(const std::string &, const EvaluationContext &) const override;
};
Registry defaults(const mesh::MeshNode &);
} // namespace heart
} // namespace wds::ai
