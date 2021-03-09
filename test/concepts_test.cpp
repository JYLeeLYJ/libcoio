#include <coroutine>
#include "awaitable.hpp"
#include "future.hpp"

using namespace coio;
using namespace coio::concepts;

template<awaitable T>
constexpr bool is_awaitable() {return true;}

template<class T>
constexpr bool is_awaitable() {return false;}

template<awaiter T>
constexpr bool is_awaiter() {return true;}
template<class T>
constexpr bool is_awaiter() {return false;}

static_assert(is_awaiter<std::suspend_always>());
static_assert(is_awaitable<std::suspend_always>());

static_assert(std::is_void_v<awaitable_traits<std::suspend_always>::await_result_t>);

static_assert(is_awaitable<future<void>>());
static_assert(is_awaitable<future<int>>());
static_assert(is_awaitable<future<int&>>());

static_assert(std::is_same_v<int , awaitable_traits<future<int>>::await_result_t>);
static_assert(std::is_same_v<int &, awaitable_traits<future<int&>>::await_result_t>);

