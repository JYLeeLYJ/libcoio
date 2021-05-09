#include "awaitable.hpp"
#include "common/ref.hpp"
#include "future.hpp"
#include <coroutine>

using namespace coio;
using namespace coio::concepts;

template <awaitable T> constexpr bool is_awaitable() { return true; }

template <class T> constexpr bool is_awaitable() { return false; }

template <awaiter T> constexpr bool is_awaiter() { return true; }
template <class T> constexpr bool is_awaiter() { return false; }

template <class Fn, class... Args>
  requires coroutine<Fn, Args...>
constexpr bool is_coroutine() { return true; }

template <class Fn, class... Args> constexpr bool is_coroutine() {
  return false;
}

static_assert(is_awaiter<std::suspend_always>());
static_assert(is_awaitable<std::suspend_always>());

auto f1 = []() -> future<void> { co_return; };
auto f2 = [](int) -> future<int> { co_return 1; };

static_assert(std::is_invocable_v<decltype(f1)>);
static_assert(std::is_same_v<future<void>, std::invoke_result_t<decltype(f1)>>);
static_assert(std::is_same_v<future<void>::promise_type,
                             std::invoke_result_t<decltype(f1)>::promise_type>);
static_assert(is_coroutine<decltype(f1)>());
static_assert(is_coroutine<decltype(f2)>() == false);
static_assert(is_coroutine<decltype(f2), int>());

static_assert(
    std::is_void_v<awaitable_traits<std::suspend_always>::await_result_t>);

static_assert(is_awaitable<future<void>>());
static_assert(is_awaitable<future<int>>());
static_assert(is_awaitable<future<coio::ref<int>>>());

static_assert(
    std::is_same_v<future<int>,
                   awaitable_traits<future<future<int>>>::await_result_t>);
static_assert(
    std::is_same_v<int, awaitable_traits<future<int>>::await_result_t>);
static_assert(
    std::is_same_v<coio::ref<int>,
                   awaitable_traits<future<coio::ref<int>>>::await_result_t>);

static_assert(
    std::is_same_v<int, awaitable_traits<future<int>>::await_resume_t>);
static_assert(
    std::is_same_v<int &, awaitable_traits<future<int> &>::await_resume_t>);

struct foo {};

static_assert(
    std::is_same_v<foo &&, awaitable_traits<future<foo>>::await_resume_t>);
static_assert(
    std::is_same_v<foo &, awaitable_traits<future<foo> &>::await_resume_t>);