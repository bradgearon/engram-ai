#include "wds/ai/core.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
namespace wds::ai::winds {
Package::Package(const fs::path &dir) {
    std::ifstream m(dir / "manifest.json");
    require(bool(m), "missing wind manifest");
    require(fs::file_size(dir / "manifest.json") < 16384, "manifest too large");
    m >> manifest;
    require(manifest.at("schema") == 1 && manifest.at("state_dim") == 16 && manifest.at("alignment") == 64,
            "incompatible wind schema");
    require(manifest.at("policy_version").is_number_unsigned() &&
                manifest.at("policy_version").get<unsigned>() > 0,
            "invalid policy version");
    identifier(field(manifest, "id", 80));
    require(manifest.at("maturity") == "seeded" || manifest.at("maturity") == "growing",
            "unsupported maturity; promotion requires independent evaluation");
    std::ifstream b(dir / "policy.wind", std::ios::binary);
    require(bool(b), "missing wind weights");
    require(fs::file_size(dir / "policy.wind") == 1152, "wrong policy byte length");
    std::string bytes((std::istreambuf_iterator<char>(b)), {});
    digest = checksum(bytes);
    require(digest == manifest.at("checksum_fnv1a64").get<std::string>(), "wind checksum mismatch");
    require(bytes.substr(0, 8) == std::string("WDSWND1\0", 8), "bad wind magic");
    auto u32 = [&](size_t n) {
        uint32_t v = 0;
        for (int k = 0; k < 4; k++)
            v |= uint32_t(uint8_t(bytes[n + k])) << (8 * k);
        return v;
    };
    require(u32(8) == 1 && u32(12) == 16 && u32(16) == 64 && u32(20) == 272, "bad wind header");
    auto f32 = [&](size_t off) {
        uint32_t v = u32(off);
        float f;
        std::memcpy(&f, &v, 4);
        require(std::isfinite(f) && std::abs(f) <= 4, "invalid policy weight");
        return f;
    };
    for (size_t i = 0; i < 256; i++)
        policy.weight[i] = f32(64 + 4 * i);
    for (size_t i = 0; i < 16; i++)
        policy.bias[i] = f32(1088 + 4 * i);
}
State Package::step(const State &z, const Embedding &active, float feedback, bool frozen) const {
    if (frozen)
        return z;
    State input{}, out{};
    for (size_t i = 0; i < 16; i++)
        input[i] = z[i] + active[i];
    input[15] += feedback;
    for (size_t o = 0; o < 16; o++) {
        float v = policy.bias[o];
        for (size_t i = 0; i < 16; i++)
            v += policy.weight[o * 16 + i] * input[i];
        out[o] = std::clamp(z[o] + 0.25f * std::tanh(v), -1.f, 1.f);
    }
    return out;
}
} // namespace wds::ai::winds
