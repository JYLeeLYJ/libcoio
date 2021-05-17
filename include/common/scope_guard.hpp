#ifndef COIO_SCOPEGUARD_HPP
#define COIO_SCOPEGUARD_HPP

#include "common/non_copyable.hpp"
#include <type_traits>
#include <utility>

namespace coio {

template <class F>
  requires std::is_nothrow_invocable_r_v<void, F>
struct scope_guard : non_copyable {
  F f;
  scope_guard(F &&f_) noexcept : f(std::move(f_)) {}
  scope_guard(F &f_) noexcept : f(f_) {}
  ~scope_guard() { f(); }
};

} // namespace coio

#endif