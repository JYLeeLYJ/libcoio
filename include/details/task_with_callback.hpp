#pragma once

#include <cassert>
#include <coroutine>
#include <exception>
#include <functional>

#include "awaitable.hpp"
#include "common/non_copyable.hpp"

namespace coio {

namespace details {

struct void_t {};

template <class T> class task_with_callback : non_copyable {
public:
  using value_type = std::remove_reference_t<T>;

  struct promise_type {

    struct final_awaiter : std::suspend_always {
      void await_suspend(std::coroutine_handle<>) {
        if (!promise.m_callback)
          return;
        std::function cb = std::move(promise.m_callback);
        cb();
      }
      promise_type &promise;
    };

    constexpr auto initial_suspend() noexcept { return std::suspend_always{}; }

    constexpr auto final_suspend() noexcept {
      return final_awaiter{.promise = *this};
    }

    constexpr void return_void() noexcept {}

    auto get_return_object() {
      return std::coroutine_handle<promise_type>::from_promise(*this);
    }

    void unhandled_exception() noexcept {
      m_exception = std::current_exception();
    }
    template <class U>
      requires std::same_as<std::remove_reference_t<U>, value_type>
    auto yield_value(U &&val) {
      m_pointer = std::addressof(val);
      return final_suspend();
    }

    decltype(auto) result() {
      if (m_exception)
        std::rethrow_exception(m_exception);
      if constexpr (std::is_same_v<value_type, void>)
        return;
      else
        return static_cast<T &&>(*m_pointer);
    }

    value_type *m_pointer{};
    std::exception_ptr m_exception{};
    std::function<void()> m_callback{};
  };

  task_with_callback(std::coroutine_handle<promise_type> handle)
      : m_handle(handle) {}

  task_with_callback(task_with_callback &&othr) noexcept
      : m_handle(othr.m_handle) {
    othr.m_handle = nullptr;
  }

  task_with_callback &operator=(task_with_callback &&othr) {
    if (m_handle)
      this->~task_with_callback();
    std::swap(othr.m_handle, m_handle);
    return *this;
  }

  ~task_with_callback() {
    if (m_handle)
      m_handle.destroy();
    m_handle = nullptr;
  }

  void start() noexcept {
    if (m_handle)
      m_handle.resume();
  }

  template <std::invocable<> F> void set_callback(F &&f) {
    if (m_handle)
      m_handle.promise().m_callback = std::forward<F>(f);
  }

  decltype(auto) get() {
    assert(m_handle);
    return m_handle.promise().result();
  }

  decltype(auto) get_non_void() {
    if constexpr (std::is_void_v<T>)
      return void_t{};
    else
      return get();
  }

  std::coroutine_handle<> handle() { return m_handle; }

  std::coroutine_handle<promise_type> m_handle;
};

} // namespace details

} // namespace coio