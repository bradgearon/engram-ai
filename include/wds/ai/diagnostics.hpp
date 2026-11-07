#pragma once
#include "wds/ai/core.hpp"
#include <fstream>
#include <iostream>
#include <mutex>

namespace wds::ai::diagnostics {
// Separate from the authoritative chain. Bounded, thread-safe, opt-in telemetry.
class Trace {
    fs::path path_;
    std::ofstream file_;
    std::mutex mutex_;
    uint64_t sequence_ = 0, bytes_ = 0, limit_ = 16 * 1024 * 1024;
    int backups_ = 8;
    bool content_ = true, enabled_ = false, errors_only_ = false;
    std::string session_ = std::to_string(now_ms());
    void line(const json &value) {
        auto text = value.dump(-1, ' ', false, json::error_handler_t::replace) + "\n";
        file_ << text; file_.flush(); bytes_ += text.size();
        if (!file_) throw std::runtime_error("trace write failed: " + path_.string());
    }
public:
    void open(fs::path path, bool content, bool errors_only, int mb, int backups) {
        path_ = std::move(path); content_ = content; errors_only_ = errors_only;
        limit_ = uint64_t(mb) * 1024 * 1024; backups_ = backups;
        fs::create_directories(path_.parent_path());
        bytes_ = fs::exists(path_) ? fs::file_size(path_) : 0;
        file_.open(path_, std::ios::app | std::ios::binary);
        require(bool(file_), "cannot open trace: " + path_.string()); enabled_ = true;
    }
    void record(const std::string &kind, const json &payload, bool error = false) noexcept {
        std::lock_guard lock(mutex_);
        if (!enabled_ || (errors_only_ && !error)) return;
        try {
            if (bytes_ >= limit_) {
                file_.close();
                fs::path oldest = path_.string()+"."+std::to_string(backups_);
                bool expired = fs::exists(oldest); if (expired) fs::remove(oldest);
                for (int i = backups_-1; i >= 1; --i) {
                    fs::path old = path_.string()+"."+std::to_string(i);
                    if (fs::exists(old)) fs::rename(old, path_.string()+"."+std::to_string(i+1));
                }
                fs::rename(path_, path_.string()+".1");
                file_.open(path_, std::ios::binary | std::ios::trunc); bytes_ = 0;
                line({{"schema",1},{"time_ms",now_ms()},{"session_id",session_},{"kind","trace.rotation"},
                      {"retention_expired",expired},{"keep_files",backups_+1}});
            }
            json data = payload;
            if (!content_) {
                data = json::object();
                for (auto key : {"type","request_id","trace_id","root_id","instance_id","seq","status","duration_ms"})
                    if (payload.is_object() && payload.contains(key)) data[key] = payload.at(key);
                data["payload_bytes"] = payload.dump(-1,' ',false,json::error_handler_t::replace).size();
                data["content_omitted"] = true;
            }
            line({{"schema",1},{"time_ms",now_ms()},{"session_id",session_},{"sequence",++sequence_},
                  {"source","runtime"},{"kind",kind},{"payload",data}});
        } catch (const std::exception &e) {
            enabled_ = false;
            std::cerr << "DIAGNOSTICS FAILURE: " << e.what() << "; tracing disabled, runtime journal remains authoritative\n";
        }
    }
};
}
