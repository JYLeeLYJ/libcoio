#ifndef COIO_REF_HPP
#define COIO_REF_HPP

#include "common/type_concepts.hpp"
#include <memory>
namespace coio {

template <class T>
  requires(!concepts::void_type<T>)
class ref {
public:
  using type = T;

  constexpr ref(T &t) noexcept : m_ptr(std::addressof(t)) {}

  ref(const ref &) noexcept = default;
  ref &operator=(const ref &x) noexcept = default;

  constexpr operator T &() const noexcept { return *m_ptr; }
  constexpr T &get() const noexcept { return *m_ptr; }

private:
  T *m_ptr;
};

// template<class T>
// ref(T&) -> ref<T>;
} // namespace coio

#endif