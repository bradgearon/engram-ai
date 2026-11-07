#pragma once
#include "wds/ai/evaluation.hpp"

namespace wds::ai {
namespace mesh {
json topology(const relationships::HeartScope &);
class MemoryIndex {
  public:
    virtual ~MemoryIndex() = default;
    virtual std::vector<json> retrieve(const std::string &, const Embedding &) const = 0;
    virtual std::vector<json> fetch(const std::string &, const std::vector<json> &refs) const {
        return refs;
    }
};
// Both roles read the same Arc. Neither owns a second memory store or a writer.
class IGFBP2Indexer {
    const chain::Store &arc_;
  public:
    explicit IGFBP2Indexer(const chain::Store &arc) : arc_(arc) {}
    std::vector<json> locate(const std::string &, const Embedding &) const;
};
class IGF2BP2Handler {
    const chain::Store &arc_;
  public:
    explicit IGF2BP2Handler(const chain::Store &arc) : arc_(arc) {}
    std::vector<json> fetch(const std::string &, const std::vector<json> &) const;
    json prepare_rna(const json &packet, const std::string &trace) const;
};
class LocalScanIndex final : public MemoryIndex {
    IGFBP2Indexer indexer_;
    IGF2BP2Handler handler_;

  public:
    explicit LocalScanIndex(const chain::Store &s) : indexer_(s), handler_(s) {}
    std::vector<json> retrieve(const std::string &, const Embedding &) const override;
    std::vector<json> fetch(const std::string &, const std::vector<json> &) const override;
};
struct VoiceBudget {
    explicit VoiceBudget(size_t limit) : remaining(limit) {
        require(limit <= 8, "voice budget exceeds 8");
    }
    size_t remaining, used = 0;
};
// Voices have input values only: no garden, writer, mesh, or recursive evaluator capability.
struct VoiceContext {
    std::string proposal, seed;
    json rubric;
    relationships::SoulView soul;
};
class Voice {
  public:
    virtual ~Voice() = default;
    virtual int version() const {
        return 1;
    }
    virtual heart::VoiceContribution contribute(const VoiceContext &) const = 0;
};
using Voices = std::map<std::string, std::unique_ptr<Voice>>;
Voices builtin_voices();
class MeshNode {
  public:
    virtual ~MeshNode() = default;
    virtual bool has_voice(const std::string &) const = 0;
    virtual std::vector<json> retrieve(const std::string &, const Embedding &) const = 0;
    virtual std::vector<json> fetch(const std::string &, const std::vector<json> &refs) const {
        return refs;
    }
    virtual heart::VoiceContribution consult(const std::string &voice_id, const std::string &soul_id,
                                             const std::string &proposal, const heart::EvaluationContext &,
                                             VoiceBudget &) const = 0;
};
class SingleNodeMesh final : public MeshNode {
    std::unique_ptr<MemoryIndex> index_;
    Voices voices_;

  public:
    explicit SingleNodeMesh(std::unique_ptr<MemoryIndex>, Voices voices = builtin_voices());
    bool has_voice(const std::string &) const override;
    std::vector<json> retrieve(const std::string &, const Embedding &) const override;
    std::vector<json> fetch(const std::string &, const std::vector<json> &) const override;
    heart::VoiceContribution consult(const std::string &, const std::string &, const std::string &,
                                     const heart::EvaluationContext &, VoiceBudget &) const override;
};
// Lexical eligibility is an explicit baseline, not a semantic or truth score.
double relevance(const std::string &query, const std::string &purpose);
} // namespace mesh
} // namespace wds::ai
