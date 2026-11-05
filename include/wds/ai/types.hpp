#pragma once
#include <array>
#include <atomic>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <vector>
namespace wds::ai {
using json = nlohmann::json;
namespace fs = std::filesystem;
constexpr size_t dim = 32, state_dim = 16, max_candidates = 64, top_k = 8;
using Embedding = std::array<float, dim>;
using State = std::array<float, state_dim>;
uint64_t hash64(const std::string &s);
std::string checksum(const std::string &s);
int64_t now_ms();
void require(bool b, const std::string &message);
std::string field(const json &j, const char *key, size_t max = 4096);
void identifier(const std::string &s);
Embedding embed(const std::string &text);
} // namespace wds::ai
