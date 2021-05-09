#ifndef COIO_WHEN_ALL_HPP
#define COIO_WHEN_ALL_HPP

#include <tuple>
#include <vector>

#include "details/await_counter.hpp"
#include "details/task_with_callback.hpp"
#include "future.hpp"

namespace coio {

namespace details {

template <class R>
using non_void_result_impl = std::conditional_t<std::is_void_v<R>, void_t, R>;

template <class T>
using non_void_result_t =
    non_void_result_impl<typename awaitable_traits<T>::await_result_t>;

template <concepts::awaitable A>
  requires concepts::void_type<typename awaitable_traits<A>::await_resume_t>
auto make_when_all_wait_task(A &&a) -> details::task_with_callback<void> {
  co_await reinterpret_cast<A &&>(a);
}

template <concepts::awaitable A>
auto make_when_all_wait_task(A &&a) -> details::task_with_callback<
    typename awaitable_traits<A>::await_resume_t> {
  co_yield co_await reinterpret_cast<A &&>(a);
}

// is a coroutine
template <class... A>
auto when_all_impl(A... awaitable)
    -> future<std::tuple<details::non_void_result_t<A>...>> {
  constexpr auto N = sizeof...(awaitable);
  awaitable_counter counter{.cnt = N};
  std::tuple tasks{
      details::make_when_all_wait_task(std::forward<A>(awaitable))...};

  auto callback = [&]() { counter.notify_complete_one(); };
  auto run_task = [](auto &...task) { (task.start(), ...); };
  auto get_result = [](auto &...task) {
    return std::tuple{task.get_non_void()...};
  };
  auto set_fin = [&](auto &...task) { (task.set_callback(callback), ...); };

  std::apply(set_fin, tasks);
  std::apply(run_task, tasks);
  co_await counter;
  co_return std::apply(get_result, tasks);
}

} // namespace details

// map result type of A :
//  void    => coio::void_value
//  else    => A
template <concepts::awaitable... A>
auto when_all(A &&...awaitable)
    -> future<std::tuple<details::non_void_result_t<A>...>> {
  return details::when_all_impl<A...>(std::forward<A>(awaitable)...);
}

template <concepts::awaitable A, class R = awaitable_traits<A>::await_result_t>
  requires concepts::void_type<R> future<void>
  when_all(std::vector<A> awaitables) {

    using task_t = details::task_with_callback<void>;
    awaitable_counter counter{.cnt = awaitables.size()};
    auto callback = [&]() { counter.notify_complete_one(); };

    std::vector<task_t> v{};
    for (auto &a : awaitables) {
      auto task = details::make_when_all_wait_task(a);
      task.set_callback(callback);
      task.start();
      v.emplace_back(std::move(task));
    }

    co_await counter;
  }

template <concepts::awaitable A, class R = awaitable_traits<A>::await_result_t>
future<std::vector<R>> when_all(std::vector<A> awaitables) {

  awaitable_counter counter{.cnt = awaitables.size()};

  using task_t = decltype(details::make_when_all_wait_task(awaitables[0]));
  auto callback = [&]() { counter.notify_complete_one(); };

  std::vector<task_t> tasks{};
  for (auto &a : awaitables) {
    auto task = details::make_when_all_wait_task(a);
    task.set_callback(callback);
    task.start();
    tasks.emplace_back(std::move(task));
  }

  co_await counter;

  std::vector<R> results{};
  for (auto &t : tasks) {
    results.emplace_back(std::move(t.get()));
  }
  co_return std::move(results);
}

} // namespace coio

#endif