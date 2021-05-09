#ifndef COIO_AWAIT_COUNTER_HPP
#define COIO_AWAIT_COUNTER_HPP

#ifndef __cpp_lib_atomic_wait
// disable content_t usage to keep this lib header only
#define __NO_TABLE
// https://github.com/ogiroux/atomic_wait
#include "common/atomic_wait.hpp"
#endif

#include <coroutine>

namespace coio {

struct awaitable_counter {

  auto operator co_await() noexcept {
    struct awaiter {
      bool await_ready() const noexcept {
        return m_counter.cnt.load(std::memory_order_acquire) == 0;
      }

      void await_suspend(std::coroutine_handle<> continuation) {
        m_counter.continuation = continuation;
      }

      void await_resume() noexcept {}

      awaitable_counter &m_counter;
    };
    return awaiter{*this};
  }

  void notify_complete_one() {
    if (cnt.fetch_sub(1, std::memory_order_acq_rel) == 1)
      if (continuation)
        continuation.resume();
  }

  std::coroutine_handle<> continuation{nullptr};
  std::atomic<uint64_t> cnt{0};
};

} // namespace coio

#endif