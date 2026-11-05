#include "wds/ai/core.hpp"
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <thread>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
using namespace wds::ai;
using Server = websocketpp::server<websocketpp::config::asio>;
using Handle = websocketpp::connection_hdl;
// Coroutine orchestration; blocking inference runs on one bounded standard-library executor.
async::Task<async::Unit> drive(runtime::Engine &engine, std::string id, runtime::Emit emit,
                               async::Executor &compute, async::Executor &coordinator,
                               std::function<void(std::string)> next, const std::atomic<bool> &stopping,
                               std::function<void()> settled) {
    bool again = co_await engine.advance(id, emit, compute, coordinator);
    if (!stopping.load()) {
        if (again)
            next(id);
        settled();
    }
    co_return async::Unit{};
}
int main(int argc, char **argv) {
    try {
        fs::path data = "data", package = "packages/breeze", package_root = "packages";
        std::string model;
        int port = 8765, layers = 0;
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--help") {
                std::cout
                    << "wds-ai [--data DIR] [--package DIR] [--model GGUF] [--gpu-layers N] [--port N]\n";
                return 0;
            }
            require(i + 1 < argc, "missing flag value");
            std::string v = argv[++i];
            if (arg == "--data")
                data = v;
            else if (arg == "--package")
                package = v;
            else if (arg == "--packages")
                package_root = v;
            else if (arg == "--model")
                model = v;
            else if (arg == "--port")
                port = std::stoi(v);
            else if (arg == "--gpu-layers")
                layers = std::stoi(v);
            else
                throw std::runtime_error("unknown flag");
        }
        require(port > 0 && port < 65536 && layers >= 0, "invalid port or gpu layers");
        chain::Store store(data);
        winds::Package wind(package);
        auto foundation = intelligence::create(model, layers);
        runtime::Engine engine(store, wind, *foundation, {}, {}, "wds.ai.heart.composite/1", package_root);
        engine.pause_all();
        store.append("runtime.started", "system", "startup",
                     {{"foundation", foundation->name()}, {"wind_manifest", wind.manifest}});
        Server server;
        server.clear_access_channels(websocketpp::log::alevel::all);
        server.clear_error_channels(websocketpp::log::elevel::all);
        server.init_asio();
        server.set_reuse_addr(true);
        server.set_max_message_size(16384);
        std::set<Handle, std::owner_less<Handle>> clients;
        std::map<std::string, std::set<Handle, std::owner_less<Handle>>> subscriptions;
        std::map<Handle, std::set<std::string>, std::owner_less<Handle>> watchers;
        std::atomic<size_t> pending{0};
        auto send = [&](Handle h, const json &value) {
            std::string instance;
            if (value.contains("instance_id"))
                instance = value.at("instance_id");
            else if (value.contains("data") && value.at("data").is_object()) {
                const auto &d = value.at("data");
                if (d.contains("instance_id"))
                    instance = d.at("instance_id");
                else if (value.value("type", std::string{}) == "wind.checkpoint")
                    instance = d.at("id");
            }
            auto root = value.value("root_id", std::string{});
            bool watch = value.value("type", std::string{}) == "upkeep.snapshot" &&
                         value.at("data").value("watch", false);
            bool subscribe = value.value("type", std::string{}) == "episode.attached";
            auto body = value.dump(-1, ' ', false, json::error_handler_t::replace);
            if (pending.fetch_add(body.size()) > 2 * 1024 * 1024) {
                pending.fetch_sub(body.size());
                server.get_io_service().post([&, h] {
                    websocketpp::lib::error_code ec;
                    server.close(h, 1013, "output backlog", ec);
                });
                return;
            }
            server.get_io_service().post([&, h, body = std::move(body), instance, subscribe, root, watch] {
                pending.fetch_sub(body.size());
                if (watch && clients.count(h))
                    watchers[h].insert(root);
                if (subscribe && clients.count(h))
                    subscriptions[instance].insert(h);
                auto recipients =
                    instance.empty() ? std::set<Handle, std::owner_less<Handle>>{} : subscriptions[instance];
                recipients.insert(h);
                for (const auto &entry : watchers)
                    if (!root.empty() && (entry.second.count("*") || entry.second.count(root)))
                        recipients.insert(entry.first);
                for (const auto &recipient : recipients) {
                    if (!clients.count(recipient))
                        continue;
                    websocketpp::lib::error_code ec;
                    auto c = server.get_con_from_hdl(recipient, ec);
                    if (ec)
                        continue;
                    if (c->get_buffered_amount() > 512 * 1024) {
                        server.close(recipient, 1013, "slow consumer", ec);
                        continue;
                    }
                    server.send(recipient, body, websocketpp::frame::opcode::text, ec);
                }
            });
        };
        // Worker is destroyed (joined) before server/engine. All engine access is confined to it.
        {
            std::set<std::string> scheduled;
            std::function<void(std::string, Handle)> schedule;
            std::function<void()> pump;
            async::Executor worker(128), compute(16);
            std::atomic<bool> stopping{false};
            asio::steady_timer upkeep_timer(server.get_io_service());
            std::optional<int64_t> armed;
            pump = [&] {
                if (stopping.load())
                    return;
                try {
                    for (const auto &id :
                         engine.dispatch_ready(now_ms(), [&](const json &e) { send(Handle{}, e); }))
                        schedule(id, Handle{});
                    auto due = engine.next_wake(now_ms());
                    server.get_io_service().post([&, due] {
                        if (stopping.load())
                            return;
                        if (!due) {
                            armed.reset();
                            upkeep_timer.cancel();
                            return;
                        }
                        // A later deadline may cause one harmless early wake; never reset for every event.
                        if (armed && *armed <= *due)
                            return;
                        armed = due;
                        upkeep_timer.expires_after(
                            std::chrono::milliseconds(std::max<int64_t>(1, *due - now_ms())));
                        upkeep_timer.async_wait([&](const asio::error_code &error) {
                            if (error || stopping.load())
                                return;
                            armed.reset();
                            if (!worker.post(pump, true)) {
                                std::cerr << "upkeep wake rejected: coordinator queue full\n";
                                server.stop();
                            }
                        });
                    });
                } catch (const std::exception &e) {
                    // Do not spin on a broken durable record. Exit visibly; replay preserves work.
                    std::cerr << "upkeep scheduler failed: " << e.what() << '\n';
                    send(Handle{},
                         {{"schema", 1}, {"type", "error"}, {"root_id", "*"}, {"message", e.what()}});
                    stopping = true;
                    server.get_io_service().post([&] { server.stop(); });
                }
            };
            schedule = [&](std::string id, Handle h) {
                if (stopping.load())
                    return;
                if (!scheduled.insert(id).second)
                    return;
                if (!worker.post([&, id, h] {
                        scheduled.erase(id);
                        auto emit = [&, h](const json &e) { send(h, e); };
                        async::launch(drive(
                                          engine, id, emit, compute, worker,
                                          [&, h](std::string next) { schedule(next, h); }, stopping, pump),
                                      [&, h, id](std::exception_ptr error) {
                                          try {
                                              std::rethrow_exception(error);
                                          } catch (const std::exception &e) {
                                              send(h, {{"schema", 1},
                                                       {"type", "error"},
                                                       {"instance_id", id},
                                                       {"message", e.what()}});
                                          }
                                      });
                    })) {
                    scheduled.erase(id);
                    engine.command({{"schema", 1},
                                    {"type", "pause"},
                                    {"request_id", "backpressure"},
                                    {"instance_id", id}},
                                   [&, h](const json &e) { send(h, e); });
                }
            };
            server.set_validate_handler([&](Handle h) {
                if (clients.size() >= 32)
                    return false;
                auto c = server.get_con_from_hdl(h);
                auto origin = c->get_request_header("Origin");
                return origin.empty();
            });
            server.set_open_handler([&](Handle h) {
                clients.insert(h);
                send(h, {{"schema", 1},
                         {"type", "hello"},
                         {"service", "wds.ai.understanding"},
                         {"foundation", foundation->name()},
                         {"protocol", 1},
                         {"implementation", "0.4.0"},
                         {"capabilities",
                          {"relationship.declare", "heart.inspect", "mesh.voices", "task.run",
                           "practice.assess", "episode.attach", "package.register", "wind.activate",
                           "upkeep.watch", "neuron.signal", "reflection.record", "speaker.profile",
                           "scope.bind", "engram.suggest"}}});
            });
            server.set_close_handler([&](Handle h) {
                clients.erase(h);
                watchers.erase(h);
                for (auto &entry : subscriptions)
                    entry.second.erase(h);
            });
            server.set_message_handler([&](Handle h, Server::message_ptr msg) {
                if (msg->get_opcode() != websocketpp::frame::opcode::text) {
                    send(h, {{"schema", 1}, {"type", "error"}, {"message", "text frames only"}});
                    return;
                }
                auto body = msg->get_payload();
                if (!worker.post([&, h, body] {
                        std::string trace = "invalid";
                        auto emit = [&, h](const json &e) { send(h, e); };
                        try {
                            auto m = json::parse(body, [](int depth, json::parse_event_t, const json &) {
                                require(depth <= 32, "JSON nesting limit");
                                return true;
                            });
                            if (m.contains("request_id") && m["request_id"].is_string())
                                trace = m["request_id"].get<std::string>().substr(0, 80);
                            if (m.value("type", std::string{}) == "runtime.shutdown") {
                                stopping = true;
                                server.get_io_service().post([&] { server.stop(); });
                                return;
                            }
                            auto id = engine.command(m, emit);
                            if (!id.empty())
                                schedule(id, h);
                            pump();
                        } catch (const std::exception &e) {
                            send(h, {{"schema", 1},
                                     {"type", "error"},
                                     {"request_id", trace},
                                     {"message", e.what()}});
                            try {
                                store.append("command.rejected", "system", trace, {{"reason", e.what()}});
                            } catch (...) {
                            }
                        }
                    }))
                    send(h, {{"schema", 1}, {"type", "error"}, {"message", "command queue full"}});
            });
            asio::signal_set signals(server.get_io_service(), SIGINT, SIGTERM);
            signals.async_wait([&](const auto &, int) { server.stop(); });
            server.listen(asio::ip::tcp::endpoint(asio::ip::make_address("127.0.0.1"), uint16_t(port)));
            server.start_accept();
            worker.post(pump);
            std::cout << "READY ws://127.0.0.1:" << port << " backend=" << foundation->name() << std::endl;
            server.run();
            stopping = true;
            compute.shutdown();
            worker.shutdown();
        }
        engine.pause_all();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return 1;
    }
}
