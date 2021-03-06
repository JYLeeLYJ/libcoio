#ifndef COIO_AWAITABLE_HPP
#define COIO_AWAITABLE_HPP

#include <concepts>
#include <coroutine>
#include <type_traits>
#include <utility>

namespace coio {

namespace concepts {

namespace details {

template <class T> inline constexpr bool is_coroutine_handle_v = true;

template <class Promise>
inline constexpr bool is_coroutine_handle_v<std::coroutine_handle<Promise>> =
    false;

template <class T>
concept suspend_result =
    std::same_as<bool, T> || std::same_as<void, T> || is_coroutine_handle_v<T>;

} // namespace details

template <class T>
concept awaiter = requires(T t) {
  { t.await_ready() } -> std::convertible_to<bool>;
  { t.await_suspend(std::coroutine_handle<>{}) } -> details::suspend_result;
  t.await_resume();
};

template <class T>
concept awaitable = awaiter<T> || requires(T t) {
  { operator co_await(t) } -> awaiter;
} || requires(T t) {
  { t.operator co_await() } -> awaiter;
};

template <class Fn, class... Args>
concept coroutine = requires {
  typename std::invoke_result_t<Fn, Args...>::promise_type;
};

template <class A, class T>
concept awaiter_of = awaiter<A> && requires(A a) {
  { a.await_resume() } -> std::same_as<T>;
};
} // namespace concepts

template <concepts::awaiter T> decltype(auto) get_awaiter(T &&t) {
  return std::forward<T>(t);
}

template <concepts::awaitable T>
  requires requires(T t) { t.operator co_await(); }
decltype(auto) get_awaiter(T &&t) {
  return static_cast<T &&>(t).operator co_await();
}

template <concepts::awaitable T> decltype(auto) get_awaiter(T &&t) {
  return operator co_await(static_cast<T &&>(t));
}

template <concepts::awaitable T> struct awaitable_traits {
  using awaiter_t = decltype(get_awaiter(std::declval<T>()));
  // T , T& or T&&
  using await_resume_t = decltype(std::declval<awaiter_t>().await_resume());
  // T
  using await_result_t = std::remove_reference_t<await_resume_t>;
};

// namespace concepts{
//     template<class A , class T>
//     concept async_result =
//         awaitable<A> &&
//         std::same_as<T , typename awaitable_traits<A>::await_result_t>;
// }
} // namespace coio

#endif