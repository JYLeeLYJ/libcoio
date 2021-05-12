#ifndef COIO_TYPE_CONCEPTS_HPP
#define COIO_TYPE_CONCEPTS_HPP

#include <type_traits>

namespace coio {
namespace concepts {

// type category
template <class T>
concept void_type = std::is_void_v<T>;

template <class T>
concept reference_type = std::is_reference_v<T>;

template <class T>
concept function_type = std::is_function_v<T>;

template <class T>
concept rvalue_reference = reference_type<T> && std::is_rvalue_reference_v<T>;

template <class T>
concept lvalue_reference = reference_type<T> && !rvalue_reference<T>;

template <class T>
concept value_type =
    !std::is_void_v<T> && !reference_type<T> && !function_type<T>;

} // namespace concepts

struct void_t {};

} // namespace coio

#endif