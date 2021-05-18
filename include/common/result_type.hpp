#ifndef COIO_RESULT_HPP
#define COIO_RESULT_HPP

#include "common/type_concepts.hpp"
#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>

namespace coio {

template <class Err> struct error {
  error(Err &e) noexcept(std::is_nothrow_copy_constructible_v<Err>) : err(e) {}
  error(Err &&e) noexcept(std::is_nothrow_move_constructible_v<Err>)
      : err(std::move(e)) {}
  Err err;
};

template <class Err> error(Err e) -> error<std::decay_t<Err>>;

// this should be a <result E> monad
template <class Value, class Err>
  requires(!std::is_reference_v<Value> && !std::is_reference_v<Err>)
class result {

  static inline constexpr bool is_copyable =
      std::is_copy_constructible_v<Value> || std::is_copy_constructible_v<Err>;
  static inline constexpr bool is_nothrow_move_value =
      std::is_nothrow_move_constructible_v<Value>;
  static inline constexpr bool is_nothrow_move_error =
      std::is_nothrow_move_constructible_v<Err>;
  static inline constexpr bool is_nothrow_moveable =
      is_nothrow_move_value && is_nothrow_move_error;

public:
  // constexpr explicit result() noexcept{};

  constexpr result(Value v) noexcept(is_nothrow_move_value)
      : m_val(std::move(v)) {}

  template <std::convertible_to<Err> E>
  constexpr result(error<E> &&e) noexcept(is_nothrow_move_error)
      : m_err(std::move(e.err)), m_iserr(true) {}

  constexpr ~result() {
    if (is_error())
      std::destroy_at(error_ptr());
    else
      std::destroy_at(value_ptr());
  }

  constexpr result(result &&other) noexcept(is_nothrow_move_value) {
    m_iserr = other.m_iserr;
    if (other.is_error()) {
      std::construct_at(error_ptr(), std::move(*other.error_ptr()));
    } else
      std::construct_at(value_ptr(), std::move(*other.value_ptr()));
  }

  constexpr result(const result &other) requires is_copyable {
    m_iserr = other.m_iserr;
    if (other.is_error) {
      std::construct_at(error_ptr(), *other.error_ptr());
    } else
      std::construct_at(value_ptr(), *other.value_ptr());
  }

  constexpr result &operator=(result &&other) noexcept(is_nothrow_move_value) {
    if (std::addressof(other) == this) [[unlikely]]
      return *this;
    this->~result();
    m_iserr = other.m_iserr;
    std::construct_at(this, std::move(other));
    return *this;
  }

public:
  constexpr bool is_error() const noexcept { return m_iserr; }

  template <std::invocable<Value &> F,
            class R = std::invoke_result_t<F, Value &>>
    requires(!std::same_as<R, void>)
  constexpr auto map(F &&f) & -> result<R, Err> {
    if (is_error())
      return error<Err>(get_error());
    else
      return result<R, Err>(std::invoke(std::forward<F>(f), value()));
  }

  template <std::invocable<Value> F, class R = std::invoke_result_t<F, Value &>>
    requires(!std::same_as<R, void>)
  constexpr auto map(F &&f) && -> result<R, Err> {
    if (is_error())
      return error<Err>(std::move(get_error()));
    else
      return result<R, Err>(
          std::invoke(std::forward<F>(f), std::move(value())));
  }

  template <std::invocable<Value &> F>
  constexpr auto map(F &&f) & -> result<void_t, Err> {
    if (is_error())
      return error<Err>(get_error());
    std::invoke(std::forward<F>(f), value());
    return result<void_t, Err>{void_t{}};
  }

  template <std::invocable<Value> F>
  constexpr auto map(F &&f) && -> result<void_t, Err> {
    if (is_error())
      return error<Err>(std::move(get_error()));
    std::invoke(std::forward<F>(f), std::move(value()));
    return result<void_t, Err>{void_t{}};
  }

  constexpr Err &get_error() noexcept { return *error_ptr(); }

  constexpr void set_error(Err e) {
    this->~result();
    std::construct_at(error_ptr(), std::move(e));
  }

  constexpr void set_value(Value val) {
    this->~result();
    std::construct_at(value_ptr(), std::move(val));
  }

  constexpr Value &value() noexcept { return *value_ptr(); }

private:
  constexpr Value *value_ptr() noexcept { return std::launder(&m_val); }

  constexpr Err *error_ptr() noexcept { return std::launder(&m_err); }

private:
  union alignas(std::max(alignof(Value), alignof(Err))) {
    Value m_val;
    Err m_err;
  };
  bool m_iserr{false};
};

// map or error
// then , flat map

} // namespace coio

#endif