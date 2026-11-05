#pragma once
#include <array>
#include <condition_variable>
#include <coroutine>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>
namespace wds::ai::async {
// Lazy, single-consumer, move-only task. Awaited frames own their children.
template <class T> class Task {
  public:
    struct promise_type {
        std::optional<T> result;
        std::exception_ptr error;
        std::coroutine_handle<> continuation = std::noop_coroutine();
        Task get_return_object() {
            return Task{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept {
            return {};
        }
        struct Final {
            bool await_ready() noexcept {
                return false;
            }
            std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                return h.promise().continuation;
            }
            void await_resume() noexcept {}
        };
        Final final_suspend() noexcept {
            return {};
        }
        void unhandled_exception() noexcept {
            error = std::current_exception();
        }
        void return_value(T value) {
            result.emplace(std::move(value));
        }
    };
    using Handle = std::coroutine_handle<promise_type>;
    explicit Task(Handle h) : handle_(h) {}
    Task(Task &&other) noexcept : handle_(std::exchange(other.handle_, {})) {}
    Task(const Task &) = delete;
    ~Task() {
        if (handle_)
            handle_.destroy();
    }
    struct Awaiter {
        Handle h;
        explicit Awaiter(Handle value) : h(value) {}
        Awaiter(const Awaiter &) = delete;
        Awaiter(Awaiter &&other) noexcept : h(std::exchange(other.h, {})) {}
        ~Awaiter() {
            if (h)
                h.destroy();
        }
        bool await_ready() const noexcept {
            return false;
        }
        Handle await_suspend(std::coroutine_handle<> caller) noexcept {
            h.promise().continuation = caller;
            return h;
        }
        T await_resume() {
            if (h.promise().error)
                std::rethrow_exception(h.promise().error);
            return std::move(*h.promise().result);
        }
    };
    Awaiter operator co_await() && {
        return Awaiter{std::exchange(handle_, {})};
    }

  private:
    Handle handle_;
};
// Unit is the value for operations without a payload, keeping one Task implementation.
struct Unit {};
enum class Priority { interactive = 0, upkeep = 1, workshop = 2 };
class Executor {
    std::mutex mutex_;
    std::condition_variable ready_;
    std::array<std::deque<std::function<void()>>, 3> queues_;
    size_t queued_ = 0, cursor_ = 0;
    bool stopping_ = false;
    size_t capacity_;
    std::jthread thread_;

  public:
    explicit Executor(size_t capacity = 128)
        : capacity_(capacity), thread_([this] {
              for (;;) {
                  std::function<void()> fn;
                  {
                      std::unique_lock lock(mutex_);
                      ready_.wait(lock, [&] { return stopping_ || queued_ > 0; });
                      if (queued_ == 0 && stopping_)
                          return;
                      constexpr std::array<size_t, 6> order{0, 0, 0, 0, 1, 2};
                      while (queues_[order[cursor_]].empty())
                          cursor_ = (cursor_ + 1) % order.size();
                      auto &queue = queues_[order[cursor_]];
                      cursor_ = (cursor_ + 1) % order.size();
                      fn = std::move(queue.front());
                      queue.pop_front();
                      --queued_;
                  }
                  fn();
              }
          }) {}
    Executor(const Executor &) = delete;
    ~Executor() {
        shutdown();
    }
    bool post(std::function<void()> fn, bool completion = false, Priority priority = Priority::interactive) {
        std::lock_guard lock(mutex_);
        if (stopping_ || queued_ >= capacity_ + (completion ? 32 : 0))
            return false;
        queues_[size_t(priority)].push_back(std::move(fn));
        ++queued_;
        ready_.notify_one();
        return true;
    }
    struct Switch {
        Executor &executor;
        bool completion;
        Priority priority;
        bool accepted = false;
        bool await_ready() const noexcept {
            return false;
        }
        bool await_suspend(std::coroutine_handle<> h) {
            // Do not touch this awaiter after publication: another thread may already resume it.
            accepted = true;
            if (executor.post([h] { h.resume(); }, completion, priority))
                return true;
            accepted = false;
            return false;
        }
        void await_resume() {
            if (!accepted)
                throw std::runtime_error("executor closed or queue full");
        }
    };
    Switch schedule(bool completion = false, Priority priority = Priority::interactive) {
        return {*this, completion, priority};
    }
    void shutdown() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        ready_.notify_all();
        if (thread_.joinable())
            thread_.join();
    }
};
struct Detached {
    struct promise_type {
        Detached get_return_object() {
            return {};
        }
        std::suspend_never initial_suspend() noexcept {
            return {};
        }
        std::suspend_never final_suspend() noexcept {
            return {};
        }
        void return_void() noexcept {}
        void unhandled_exception() noexcept {
            std::terminate();
        }
    };
};
inline Detached launch(Task<Unit> task, std::function<void(std::exception_ptr)> on_error) {
    try {
        co_await std::move(task);
    } catch (...) {
        on_error(std::current_exception());
    }
}
} // namespace wds::ai::async
