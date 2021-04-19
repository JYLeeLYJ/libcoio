#ifndef COIO_WHEN_ALL_HPP
#define COIO_WHEN_ALL_HPP

#include <vector>
#include <tuple>

#include "future.hpp"
#include "details/await_counter.hpp"
#include "details/awaitable_wrapper.hpp"
#include "details/callback_awaiter.hpp"

namespace coio{

namespace details{

    template<class R>
    using non_void_result_impl = std::conditional_t<std::is_void_v<R> , void_value , R>;

    template<class T>
    using non_void_result_t = non_void_result_impl<typename awaitable_traits<T>::await_result_t>;

    template<concepts::awaitable A>
    requires concepts::void_type<typename awaitable_traits<A>::await_resume_t>
    auto make_when_all_wait_task(A && a , awaitable_counter & counter )
    -> details::awaitable_wrapper<void> {
        co_await details::attach_callback(std::forward<A>(a) ,  [&]()noexcept{counter.notify_complete_one();});
    }

    template<concepts::awaitable A>
    auto make_when_all_wait_task(A && a , awaitable_counter & counter)
    -> details::awaitable_wrapper<typename awaitable_traits<A>::await_resume_t>{
        co_yield co_await details::attach_callback(std::forward<A>(a) ,  [&]()noexcept{counter.notify_complete_one();});
    }

    //is a coroutine
    template<class ...A>
    auto when_all_impl(A ...awaitable) -> future<std::tuple<details::non_void_result_t<A>...>>{
        constexpr auto N = sizeof...(awaitable);
        awaitable_counter counter{.cnt = N };
        std::tuple tasks {details::make_when_all_wait_task(std::forward<A>(awaitable) , counter)...} ;

        auto run_task   = [](auto & ...task) {(task.start() , ...);};
        auto get_result = [](auto & ...task) {return std::tuple{task.get_non_void()...};};

        std::apply(run_task , tasks);
        co_await counter;
        co_return std::apply(get_result , tasks);
    }

}

//map result type of A :
//  void    => coio::void_value
//  else    => A
template<concepts::awaitable ...A>
auto when_all(A && ...awaitable)-> future<std::tuple<details::non_void_result_t<A>...>>{
    return details::when_all_impl<A...>(std::forward<A>(awaitable)...);
}


template<concepts::awaitable A , class R = awaitable_traits<A>::await_result_t > 
requires concepts::void_type<R>
future<void> when_all(std::vector<A> awaitables){

    using task_t=  details::awaitable_wrapper<void>;
    awaitable_counter counter{.cnt = awaitables.size() };
    std::vector<task_t> v{};
    for(auto & a : awaitables){
        auto task = details::make_when_all_wait_task(a , counter);
        task.start();
        v.emplace_back(std::move(task));
    }
    
    co_await counter;
}

template<concepts::awaitable A , class R = awaitable_traits<A>::await_result_t >
future<std::vector<R>> when_all(std::vector<A> awaitables){

    awaitable_counter counter{.cnt = awaitables.size()};

    using task_t = decltype(details::make_when_all_wait_task(awaitables[0] , counter));

    std::vector<task_t> tasks{};
    for(auto & a : awaitables){
        auto task = details::make_when_all_wait_task(a , counter);
        task.start();
        tasks.emplace_back(std::move(task));
    }

    co_await counter;
    
    std::vector<R> results{};
    for(auto & t : tasks){
        results.emplace_back(std::move(t.get()));
    }
    co_return std::move(results);
}


}

#endif