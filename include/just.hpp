#ifndef COIO_JUST_HPP
#define COIO_JUST_HPP

#include <coroutine>
#include <type_traits>

namespace coio {

template <class T> struct just {
  T value;

  just(T &&t) noexcept(std::is_nothrow_move_constructible_v<T>)
      : value(std::forward<T>(t)) {}

  constexpr bool await_ready() noexcept { return true; }
  constexpr bool await_suspend(std::coroutine_handle<>) noexcept {
    return false;
  }
  constexpr T &await_resume() &noexcept { return value; }
  constexpr T await_resume() &&noexcept { return std::move(value); }
};

template <> struct just<void> {
  constexpr bool await_ready() noexcept { return true; }
  constexpr bool await_suspend(std::coroutine_handle<>) noexcept {
    return false;
  }
  constexpr void await_resume() noexcept {}
};

} // namespace coio

#endif