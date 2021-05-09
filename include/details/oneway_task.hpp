#ifndef COIO_ONEWAY_TASK_HPP
#define COIO_ONEWAY_TASK_HPP

#include <coroutine>

namespace coio {

namespace details {

struct oneway_task {

  struct promise_type {
    constexpr auto initial_suspend() noexcept { return std::suspend_always{}; }
    constexpr auto final_suspend() noexcept { return std::suspend_never{}; }
    void return_void() {}
    void unhandled_exception() {}
    oneway_task get_return_object() noexcept {
      return std::coroutine_handle<promise_type>::from_promise(*this);
    }
  };
  void start() noexcept {
    if (!m_handle)
      return;
    std::coroutine_handle<> h = m_handle;
    m_handle = nullptr;
    h.resume();
  }

  constexpr oneway_task(std::coroutine_handle<promise_type> handle) noexcept
      : m_handle(handle) {}

  // use this handle to start the task
  std::coroutine_handle<promise_type> m_handle;
};

} // namespace details
} // namespace coio

#endif